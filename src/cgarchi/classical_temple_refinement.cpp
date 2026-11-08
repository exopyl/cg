#include <cgarchi/classical_temple_refinement.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("Raffinement : '" + field + "' " + rule);
}

void RequireNonNegative (const char *field, double value)
{
	if (!(std::isfinite (value) && value >= 0.0))
		Fail (field, "doit etre fini et >= 0");
}

// Produit ou quotient arrondi en double avant toute somme : jamais contracte en
// FMA (lecture volatile, comme StrictCrossDifference).
double Rounded (double value)
{
	const volatile double v = value;
	return v;
}

// Pas d'un axe : h sqrt (4 e / s). Infini pour s = 0 (aucune courbure le long
// de l'axe).
double Pitch (double half, double chordDeviation, double sagitta)
{
	return half * std::sqrt (4.0 * chordDeviation / sagitta);
}

// Segments d'un axe avant arrondi : len / p, par une division.
double SegmentQuotient (double halfLength, double pitch)
{
	return (2.0 * halfLength) / pitch;
}

struct AxisSpec
{
	double half = 0.0;     // demi-cote du stylobate
	double extent = 0.0;   // demi-cote du degre le plus bas
	double sagitta = 0.0;
	const char *name = "";
};

std::pair<AxisSpec, AxisSpec> AxesOf (const TempleRefinementSettings &r, const KrepisLayout &krepis)
{
	if (krepis.steps.empty ())
		throw std::invalid_argument ("Raffinement : krepis sans degre");
	const KrepisBox &stylobate = krepis.steps.front (), &lowest = krepis.steps.back ();
	// Courbure effective de chaque axe (registre RF5) : la parabole de l'axe plus
	// le terme croise f_XY = (v (s_L - s_R) + u (s_F - s_B)) / (a b), ramene a
	// l'axe par a / b ou b / a. Sans lui, une seule fleche non nulle laisse un axe
	// sans station sous un champ tordu (plan 3 section 25.38).
	const double a = stylobate.halfWidth, b = stylobate.halfDepth;
	const double sx = std::max (r.sagittaFront, r.sagittaBack) + Rounded (std::fabs (r.sagittaRight - r.sagittaLeft) * Rounded (a / b));
	const double sy = std::max (r.sagittaRight, r.sagittaLeft) + Rounded (std::fabs (r.sagittaFront - r.sagittaBack) * Rounded (b / a));
	return {{a, lowest.halfWidth, sx, "X"}, {b, lowest.halfDepth, sy, "Y"}};
}

// Plus courte distance de `c` a une coordonnee triee de `sorted`.
double NearestDistance (double c, const std::vector<double> &sorted)
{
	const auto it = std::lower_bound (sorted.begin (), sorted.end (), c);
	double d = std::numeric_limits<double>::infinity ();
	if (it != sorted.end ())
		d = std::fabs (*it - c);
	if (it != sorted.begin ())
		d = std::min (d, std::fabs (c - *(it - 1)));
	return d;
}

// Separation sure en float (plancher du deplacement, registre RF5) : 8 epsilon
// float fois la plus grande coordonnee en jeu. Deux abscisses ainsi separees
// restent a plus de 7 ulp float l'une de l'autre (ulp (x) <= epsilon |x|), et a
// plus du double du terme en ulp de DefaultWeldTolerance (4 epsilon max |x|) :
// elles ne se confondent ni a l'arrondi float, ni a la soudure par defaut.
double FloatSafeSeparation (double magnitude)
{
	return 8.0 * static_cast<double> (FLT_EPSILON) * magnitude;
}

// Blocs fermes [at - distance, at + distance] de la seconde famille,
// fusionnes, croissants.
using Blocks = std::vector<std::pair<double, double>>;

Blocks MergedBlocks (const std::vector<ClearanceCoordinate> &coordinates)
{
	Blocks raw;
	raw.reserve (coordinates.size ());
	for (const ClearanceCoordinate &c : coordinates)
		raw.push_back ({c.at - c.distance, c.at + c.distance});
	std::sort (raw.begin (), raw.end ());
	Blocks out;
	out.reserve (raw.size ());
	for (const std::pair<double, double> &b : raw)
		if (!out.empty () && b.first <= out.back ().second)
			out.back ().second = std::max (out.back ().second, b.second);
		else
			out.push_back (b);
	return out;
}

// Vrai si `c` est hors de tout bloc ferme.
bool Clear (double c, const Blocks &blocks)
{
	const auto it = std::lower_bound (blocks.begin (), blocks.end (), c,
	                                  [] (const std::pair<double, double> &b, double x) { return b.second < x; });
	return it == blocks.end () || c < it->first;
}

// Point de [lo, hi] le plus eloigne des interdites (milieu d'un intervalle
// libre, ou bord), retenu s'il bat (best, bestDistance) : plus loin, puis plus
// proche de t, puis plus petit.
void FarthestIn (double t, double lo, double hi, const std::vector<double> &forbidden, double &best, double &bestDistance)
{
	constexpr double kInf = std::numeric_limits<double>::infinity ();
	auto consider = [&] (double c, double distance) {
		const double dc = std::fabs (c - t), db = std::fabs (best - t);
		if (distance > bestDistance || (distance == bestDistance && (dc < db || (dc == db && c < best))))
		{
			best = c;
			bestDistance = distance;
		}
	};
	// Intervalles (a, b) entre interdites consecutives, de celle qui precede la
	// fenetre a celle qui la suit (-inf et +inf a defaut).
	const auto begin = std::lower_bound (forbidden.begin (), forbidden.end (), lo);
	const auto end = std::upper_bound (forbidden.begin (), forbidden.end (), hi);
	double a = begin == forbidden.begin () ? -kInf : *(begin - 1);
	for (auto it = begin;; ++it)
	{
		const double b = it != end ? *it : end != forbidden.end () ? *end : kInf;
		const double from = std::max (a, lo), to = std::min (b, hi);
		if (from <= to)
		{
			double c = t;
			if (a == -kInf && b == kInf)
				c = std::min (std::max (t, from), to);
			else if (a == -kInf)
				c = from;
			else if (b == kInf)
				c = to;
			else
				c = std::min (std::max (0.5 * (a + b), from), to);
			consider (c, std::min (c - a, b - c));
		}
		if (it == end)
			break;
		a = b;
	}
}

// Repli de DisplaceOne quand aucun candidat n'est valide : FarthestIn sur les
// morceaux de la fenetre [lo, hi] hors des blocs, rentres de min (floor,
// longueur / 4) a un bout de bloc. Refus si aucun morceau, ou si le meilleur
// point n'est pas a plus de `required` (plancher float, ou
// kRefinementMaxTolerance pour les stations) des interdites.
double FarthestInWindow (double t, double lo, double hi, const std::vector<double> &forbidden, const Blocks &blocks, double clearance,
                         double floor, double required)
{
	double best = t, bestDistance = -1.0;
	bool piece = false;
	double from = lo;
	bool fromBlock = false;
	auto take = [&] (double to, bool toBlock) {
		if (from < to)
		{
			const double inset = std::min (floor, (to - from) / 4.0);
			piece = true;
			FarthestIn (t, fromBlock ? from + inset : from, toBlock ? to - inset : to, forbidden, best, bestDistance);
		}
	};
	const auto first = std::lower_bound (blocks.begin (), blocks.end (), lo,
	                                     [] (const std::pair<double, double> &b, double x) { return b.second < x; });
	for (auto it = first; it != blocks.end () && it->first <= hi; ++it)
	{
		take (std::min (it->first, hi), true);
		from = it->second;
		fromBlock = true;
	}
	if (from <= hi)
		take (hi, false);
	if (!piece)
		Fail ("refinement.chordDeviation", "ne laisse a une station aucune place a plus de " + std::to_string (clearance) +
		                                       " des sommets rencontres par ses noeuds (pied de colonne, dessus d'abaque, bout de l'egout), dans sa fenetre");
	if (!(bestDistance > required))
		Fail ("refinement.chordDeviation", "ne laisse a une station aucune place a plus " +
		                                       (required > floor ? "de kRefinementMaxTolerance (" : std::string ("du plancher float (")) +
		                                       std::to_string (required) + ") des coordonnees structurelles, dans sa fenetre");
	return best;
}

double DisplaceOne (double t, const std::vector<double> &forbidden, const Blocks &blocks, double clearance, double pitch, double floor,
                    double required)
{
	const double delta = pitch / 16.0, window = pitch / 4.0, shift = pitch / 8.0;
	bool found = false;
	double best = t;
	auto consider = [&] (double c) {
		if (!(std::fabs (c - t) <= window) || !(NearestDistance (c, forbidden) > delta) || !Clear (c, blocks))
			return;
		const double dc = std::fabs (c - t), db = std::fabs (best - t);
		if (!found || dc < db || (dc == db && c < best))
			best = c;
		found = true;
	};
	consider (t);
	// Interdites voisines de la fenetre, et celle qui la precede (milieu).
	const auto lo = std::lower_bound (forbidden.begin (), forbidden.end (), t - window - shift);
	const auto hi = std::upper_bound (forbidden.begin (), forbidden.end (), t + window + shift);
	for (auto it = lo == forbidden.begin () ? lo : lo - 1; it != hi; ++it)
	{
		consider (*it - shift);
		consider (*it + shift);
		if (it + 1 != forbidden.end ())
			consider (0.5 * (*it + *(it + 1)));
	}
	// Blocs de la seconde famille voisins de la fenetre : bouts recules de
	// `clearance`, milieu de deux blocs consecutifs.
	const auto first = std::lower_bound (blocks.begin (), blocks.end (), t - window - clearance,
	                                     [] (const std::pair<double, double> &b, double x) { return b.second < x; });
	for (auto it = first == blocks.begin () ? first : first - 1; it != blocks.end () && it->first <= t + window + clearance; ++it)
	{
		consider (it->first - clearance);
		consider (it->second + clearance);
		if (it + 1 != blocks.end ())
			consider (0.5 * (it->second + (it + 1)->first));
	}
	if (!found)
		return FarthestInWindow (t, t - window, t + window, forbidden, blocks, clearance, floor, required);
	return best;
}

} // namespace

void ValidateTempleRefinementSettings (const TempleRefinementSettings &r, const KrepisLayout &krepis)
{
	RequireNonNegative ("refinement.sagittaFront", r.sagittaFront);
	RequireNonNegative ("refinement.sagittaRight", r.sagittaRight);
	RequireNonNegative ("refinement.sagittaBack", r.sagittaBack);
	RequireNonNegative ("refinement.sagittaLeft", r.sagittaLeft);
	RequireNonNegative ("refinement.inclination", r.inclination);
	if (!(std::isfinite (r.chordDeviation) && r.chordDeviation > 0.0))
		Fail ("refinement.chordDeviation", "doit etre fini et strictement positif");
	if (!(r.chordDeviation <= kRefinementMaxTolerance / 10.0))
		Fail ("refinement.chordDeviation", "depasse kRefinementMaxTolerance / 10 (" + std::to_string (kRefinementMaxTolerance / 10.0) +
		                                       ") : les contacts ne tiendraient plus sous la tolerance maximale de validation");
	const std::pair<AxisSpec, AxisSpec> axes = AxesOf (r, krepis);
	// Borne basse : delta = p / 16 >= kRefinementMaxTolerance sur chaque axe a pas
	// fini, sans quoi une station peut tomber a moins de tau_R d'une coordonnee
	// structurelle et la soudure a tau_R confondre leurs sommets.
	for (const AxisSpec &a : {axes.first, axes.second})
	{
		const double pitch = Pitch (a.half, r.chordDeviation, a.sagitta);
		if (std::isfinite (pitch) && !(pitch / 16.0 >= kRefinementMaxTolerance))
			Fail ("refinement.chordDeviation", std::string ("donne sur l'axe ") + a.name + " un pas dont le seizieme (" +
			                                       std::to_string (pitch / 16.0) + ") est sous kRefinementMaxTolerance");
	}
	for (const AxisSpec &a : {axes.first, axes.second})
		if (!(SegmentQuotient (a.extent, Pitch (a.half, r.chordDeviation, a.sagitta)) <=
		      static_cast<double> (kRefinementMaxSegments)))
			Fail ("refinement.chordDeviation", std::string ("donne plus de ") + std::to_string (kRefinementMaxSegments) +
			                                       " segments sur l'axe " + a.name);
}

double TempleRefinementLift (const TempleRefinementSettings &r, double a, double b, double x, double y)
{
	const double u = x / a, v = y / b;
	const double cu = 1.0 - u * u, cv = 1.0 - v * v;
	return 0.5 * ((1.0 + u) * r.sagittaRight * cv + (1.0 - u) * r.sagittaLeft * cv + (1.0 - v) * r.sagittaFront * cu +
	              (1.0 + v) * r.sagittaBack * cu);
}

RefinementStations NominalRefinementStations (const TempleRefinementSettings &r, const KrepisLayout &krepis)
{
	ValidateTempleRefinementSettings (r, krepis);
	const std::pair<AxisSpec, AxisSpec> axes = AxesOf (r, krepis);
	RefinementStations s;
	for (const bool alongX : {true, false})
	{
		const AxisSpec &a = alongX ? axes.first : axes.second;
		const double pitch = Pitch (a.half, r.chordDeviation, a.sagitta);
		const int n = std::max (1, static_cast<int> (std::ceil (SegmentQuotient (a.extent, pitch))));
		const double length = 2.0 * a.extent;
		std::vector<double> &upper = alongX ? s.x : s.y, &lower = alongX ? s.xLower : s.yLower;
		upper.reserve (static_cast<std::size_t> (n - 1));
		lower.reserve (static_cast<std::size_t> (n));
		for (int k = 1; k < n; ++k)
			upper.push_back ((length * k) / n - a.extent);
		for (int k = 1; k <= n && std::isfinite (pitch); ++k)
			lower.push_back ((length * (2 * k - 1)) / (2.0 * n) - a.extent);
		(alongX ? s.pitchX : s.pitchY) = pitch;
	}
	return s;
}

double LongestClearanceBlock (const std::vector<ClearanceCoordinate> &coordinates)
{
	double longest = 0.0;
	for (const std::pair<double, double> &b : MergedBlocks (coordinates))
		longest = std::max (longest, b.second - b.first);
	return longest;
}

RefinementStations DisplaceRefinementStations (const RefinementStations &nominal, std::vector<double> forbiddenX,
                                               std::vector<double> forbiddenY, const StationClearance &clearance)
{
	static const Blocks none;
	RefinementStations s = nominal;
	auto magnitude = [] (const std::vector<double> &a, const std::vector<double> &b, const std::vector<double> &c) {
		double m = 0.0;
		for (const std::vector<double> *v : {&a, &b, &c})
			for (const double x : *v)
				m = std::max (m, std::fabs (x));
		return m;
	};
	for (const bool alongX : {true, false})
	{
		std::vector<double> &forbidden = alongX ? forbiddenX : forbiddenY;
		const double pitch = alongX ? s.pitchX : s.pitchY;
		std::vector<double> &upper = alongX ? s.x : s.y, &lower = alongX ? s.xLower : s.yLower;
		const double floor = FloatSafeSeparation (magnitude (forbidden, upper, lower));
		// Plancher dur du repli des stations (registre RF5) : kRefinementMaxTolerance.
		const double required = std::max (floor, kRefinementMaxTolerance);
		std::sort (forbidden.begin (), forbidden.end ());
		forbidden.erase (std::unique (forbidden.begin (), forbidden.end ()), forbidden.end ());
		const std::vector<ClearanceCoordinate> &family = alongX ? clearance.x : clearance.y;
		const Blocks blocks = upper.empty () ? Blocks () : MergedBlocks (family);
		double margin = 0.0;
		for (const ClearanceCoordinate &c : family)
			margin = std::max (margin, c.distance);
		for (double &t : upper)
			t = DisplaceOne (t, forbidden, blocks, margin, pitch, floor, required);
		forbidden.insert (forbidden.end (), upper.begin (), upper.end ());
		std::sort (forbidden.begin (), forbidden.end ());
		for (double &t : lower)
			t = DisplaceOne (t, forbidden, none, 0.0, pitch, floor, required);
	}
	return s;
}

std::vector<RingSectionPoint> RefineRingSectionWidth (const std::vector<RingSectionPoint> &section, double pitch,
                                                      const std::vector<double> &protectedZ,
                                                      std::vector<double> forbiddenU, double coordinateMagnitude)
{
	if (!(pitch > 0.0))
		Fail ("pitch", "doit etre strictement positif (+infini : aucune coupe)");
	const std::size_t n = section.size ();
	// Segments de chaque arete : 1 pour une arete non horizontale ou protegee.
	auto segmentsOf = [&] (std::size_t k) {
		const RingSectionPoint &a = section[k], &b = section[(k + 1) % n];
		if (!(a.z == b.z) || a.u == b.u || std::find (protectedZ.begin (), protectedZ.end (), a.z) != protectedZ.end ())
			return 1;
		const double quotient = std::fabs (b.u - a.u) / pitch;
		if (!(quotient <= static_cast<double> (kRefinementMaxSegments)))
			Fail ("pitch", "donne plus de " + std::to_string (kRefinementMaxSegments) + " segments sur une arete de la section");
		return std::max (1, static_cast<int> (std::ceil (quotient)));
	};
	std::size_t inserted = 0;
	for (std::size_t k = 0; k < n; ++k)
		inserted += static_cast<std::size_t> (segmentsOf (k) - 1);
	// Interdites : U et les bouts de toutes les aretes.
	double magnitude = std::fabs (coordinateMagnitude);
	forbiddenU.reserve (forbiddenU.size () + n);
	for (const RingSectionPoint &q : section)
		forbiddenU.push_back (q.u);
	for (const double u : forbiddenU)
		magnitude = std::max (magnitude, std::fabs (u));
	const double floor = FloatSafeSeparation (magnitude);
	std::sort (forbiddenU.begin (), forbiddenU.end ());
	forbiddenU.erase (std::unique (forbiddenU.begin (), forbiddenU.end ()), forbiddenU.end ());
	static const Blocks none;
	std::vector<RingSectionPoint> out;
	out.reserve (n + inserted);
	for (std::size_t k = 0; k < n; ++k)
	{
		const RingSectionPoint &a = section[k], &b = section[(k + 1) % n];
		out.push_back (a);
		const int segments = segmentsOf (k);
		const double du = b.u - a.u, step = std::fabs (du) / segments;
		for (int j = 1; j < segments; ++j)
			out.push_back ({DisplaceOne (a.u + (du * j) / segments, forbiddenU, none, 0.0, step, floor, floor), a.z});
	}
	return out;
}

} // namespace classical
} // namespace cgarchi
