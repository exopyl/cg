#include <cgarchi/classical_acanthus_detail.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace cgarchi::classical::detail
{
namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr const char *kCapitalBlock = "chapiteau";

// (x, y, h)
using Point = std::array<double, 3>;

// Polygone convexe d'au plus 3 + 4 sommets : un triangle decoupe par quatre plans.
struct Polygon
{
	std::array<Point, 8> p {};
	int n = 0;
};

// Partie de `in` ou s * q[axis] >= s * value (s = +1 ou -1).
Polygon Clip (const Polygon &in, int axis, double value, double s)
{
	Polygon out;
	for (int k = 0; k < in.n; ++k)
	{
		const Point &a = in.p[static_cast<std::size_t> (k)];
		const Point &b = in.p[static_cast<std::size_t> ((k + 1) % in.n)];
		const double da = s * (a[axis] - value), db = s * (b[axis] - value);
		if (da >= 0.0)
			out.p[static_cast<std::size_t> (out.n++)] = a;
		if ((da >= 0.0) != (db >= 0.0))
		{
			const double t = da / (da - db);
			out.p[static_cast<std::size_t> (out.n++)] = {a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2])};
		}
	}
	return out;
}

double ClippedMaxX (const Point &a, const Point &b, const Point &c, double hLo, double hHi, double yLo, double yHi)
{
	Polygon poly;
	poly.p[0] = a;
	poly.p[1] = b;
	poly.p[2] = c;
	poly.n = 3;
	poly = Clip (poly, 2, hLo, 1.0);
	poly = Clip (poly, 2, hHi, -1.0);
	poly = Clip (poly, 1, yLo, 1.0);
	poly = Clip (poly, 1, yHi, -1.0);
	double best = -std::numeric_limits<double>::infinity ();
	for (int k = 0; k < poly.n; ++k)
		best = std::max (best, poly.p[static_cast<std::size_t> (k)][0]);
	return best;
}

[[noreturn]] void Fail (const std::string &what)
{
	throw std::invalid_argument ("BuildFleuron : " + what);
}

void Require (bool condition, const char *what)
{
	if (!condition)
		Fail (std::string ("forme hors domaine (") + what + ")");
}

void CheckSpec (const FleuronSpec &s)
{
	Require (s.petals >= 4 && s.petals % 2 == 0, "petals pair et >= 4");
	Require (s.samplesPerPetal >= 2 && s.samplesPerPetal % 2 == 0, "samplesPerPetal pair et >= 2");
	Require (s.rings >= 3, "rings >= 3");
	for (const double v : {s.tipRadius, s.notchRadius, s.buttonRadius, s.coreRadius, s.front, s.petalThickness, s.minThickness})
		Require (std::isfinite (v) && v > 0.0, "rayons, face avant et epaisseurs finis et > 0");
	for (const double v : {s.petalGap, s.buttonRise, s.petalHollow, s.tipFall, s.clearance})
		Require (std::isfinite (v) && v >= 0.0, "jour entre petales, reliefs et jeu finis et >= 0");
	if (s.coreRadius >= s.buttonRadius)
		Fail ("noyau hors du bouton");
	if (s.notchRadius <= s.buttonRadius)
		Fail ("encoche sous le bouton");
}

// Contour d'un petale en cuiller, angle delta depuis son axe (|delta| <= pi / petals) : bout circulaire de rayon a
// centre a tip - a, flancs tangents issus du centre de la rosace ; entre les flancs de deux petales voisins, le
// contour descend a l'encoche. a laisse `gap` entre les bouts voisins. `lobe` : 1 sur l'axe, 0 aux flancs.
struct Petal
{
	double radius = 0.0, lobe = 0.0;
};

// Bout du petale : rayon a, centre a la distance d du centre de la rosace, demi-angle des flancs (0 si a <= 0).
struct PetalShape
{
	double a = 0.0, d = 0.0, flank = 0.0;
};

PetalShape PetalGeometry (const FleuronSpec &s)
{
	const double half = std::sin (kPi / s.petals);
	const double a = (s.tipRadius * half - 0.5 * s.petalGap) / (1.0 + half);
	const double d = s.tipRadius - a;
	return {a, d, a > 0.0 ? std::asin (a / d) : 0.0};
}

Petal PetalAt (const FleuronSpec &s, double delta)
{
	const PetalShape g = PetalGeometry (s);
	if (std::fabs (delta) >= g.flank)
		return {s.notchRadius, 0.0};
	const double sd = g.d * std::sin (delta);
	const double ratio = delta / g.flank;
	return {g.d * std::cos (delta) + std::sqrt (std::max (0.0, g.a * g.a - sd * sd)), 1.0 - ratio * ratio};
}

void CheckPetals (const FleuronSpec &s)
{
	const PetalShape g = PetalGeometry (s);
	if (!(g.a > 0.0))
		Fail ("jour entre petales plus large que leur pas");
	if (g.d * std::cos (g.flank) <= s.notchRadius)
		Fail ("encoche au-dela du flanc des petales");
}

double Smootherstep (double s)
{
	return s * s * s * (s * (6.0 * s - 15.0) + 10.0);
}

void CheckStem (const FleuronSpec &s, double rosetteBottom)
{
	Require (s.stemSection >= 4 && s.stemSection % 2 == 0, "stemSection pair et >= 4");
	Require (s.stemRings >= 2, "stemRings >= 2");
	for (const double v : {s.stemWidth, s.stemDepth, s.cornetWidth})
		Require (std::isfinite (v) && v > 0.0, "largeurs et fleche de la tige finies et > 0");
	Require (std::isfinite (s.stemFoot) && std::isfinite (s.cornetBottom) && std::isfinite (s.cornetTop) && s.stemFoot < s.cornetBottom &&
	        s.cornetBottom < s.cornetTop,
	    "stemFoot < cornetBottom < cornetTop");
	if (s.cornetTop > rosetteBottom - s.clearance)
		Fail ("tige au contact de la rosace");
}

// Tige : demi-jonc a dos plat, sections horizontales de stemFoot a cornetTop (stemRings anneaux reguliers en h) ;
// largeur stemWidth, evasee par smootherstep jusqu'a cornetWidth entre cornetBottom et cornetTop ; face avant en
// demi-ellipse de fleche stemDepth (stemSection - 2 aretes), dos plat a TourDepthMax sur la boite (y, h) de l'anneau
// elargie de c, + c, avec un sommet au milieu du dos ; puis verification des facettes entre anneaux. Diagonales et
// eventails des bouts symetriques par y -> -y.
void AppendStem (const CapitalEnvelope &env, const CapitalTour &tour, const FleuronSpec &spec, FleuronPiece &piece)
{
	const int nq = spec.stemSection, nz = spec.stemRings, nf = nq - 2;
	const double c = spec.clearance;
	std::vector<double> sh (static_cast<std::size_t> (nz)), sw (sh.size ()), sb (sh.size ());
	for (std::size_t i = 0; i < sh.size (); ++i)
	{
		sh[i] = spec.stemFoot + (spec.cornetTop - spec.stemFoot) * static_cast<double> (i) / (nz - 1);
		const double t = std::clamp ((sh[i] - spec.cornetBottom) / (spec.cornetTop - spec.cornetBottom), 0.0, 1.0);
		sw[i] = spec.stemWidth + (spec.cornetWidth - spec.stemWidth) * Smootherstep (t);
	}
	const auto depth = [&] (double hLo, double hHi, double half) {
		const double x = TourDepthMax (tour, hLo - c, hHi + c, -half, half);
		if (!std::isfinite (x))
			Fail ("tour absent derriere la tige");
		return x + c;
	};
	for (std::size_t i = 0; i < sh.size (); ++i)
	{
		const std::size_t lo = i == 0 ? 0 : i - 1, hi = std::min (sh.size () - 1, i + 1);
		sb[i] = depth (sh[i], sh[i], 0.5 * std::max ({sw[lo], sw[i], sw[hi]}) + c);
	}
	// Entre deux anneaux, la corde du dos doit passer devant le tour sur chaque sous-intervalle : son minimum aux bouts
	// du sous-intervalle contre le maximum du tour sur sa boite ; un deficit releve les deux anneaux.
	constexpr int kChecks = 8;
	for (std::size_t i = 0; i + 1 < sh.size (); ++i)
	{
		const double half = 0.5 * std::max (sw[i], sw[i + 1]) + c;
		for (int k = 0; k < kChecks; ++k)
		{
			const double ta = static_cast<double> (k) / kChecks, tb = static_cast<double> (k + 1) / kChecks;
			const double ha = sh[i] + (sh[i + 1] - sh[i]) * ta, hb = sh[i] + (sh[i + 1] - sh[i]) * tb;
			const double have = std::min (sb[i] + (sb[i + 1] - sb[i]) * ta, sb[i] + (sb[i + 1] - sb[i]) * tb);
			const double deficit = depth (ha, hb, half) - have;
			if (deficit > 0.0)
			{
				sb[i] += deficit;
				sb[i + 1] += deficit;
			}
		}
	}
	const double sc = env.scale;
	const unsigned base = static_cast<unsigned> (piece.positions.size ());
	for (std::size_t i = 0; i < sh.size (); ++i)
	{
		std::vector<std::array<double, 2>> section (static_cast<std::size_t> (nq));
		for (int j = 0; 2 * j <= nf; ++j)
		{
			const double a = kPi * j / nf;
			const double x = sb[i] + spec.stemDepth * std::sin (a), y = 2 * j == nf ? 0.0 : 0.5 * sw[i] * std::cos (a);
			section[static_cast<std::size_t> (j)] = {x, y};
			section[static_cast<std::size_t> (nf - j)] = {x, -y};
		}
		section[static_cast<std::size_t> (nq - 1)] = {sb[i], 0.0};
		for (const auto &p : section)
			piece.positions.push_back ({p[0] * sc, p[1] * sc, (sh[i] + env.foot) * sc});
	}
	const auto at = [&] (std::size_t i, int p) { return base + static_cast<unsigned> (i) * static_cast<unsigned> (nq) + static_cast<unsigned> (p); };
	for (std::size_t i = 0; i + 1 < sh.size (); ++i)
		for (int p = 0; p < nq; ++p)
		{
			const int q = (p + 1) % nq;
			const bool upper = p < nf / 2 || p == nq - 1;
			const unsigned a0 = at (i, p), b0 = at (i, q), a1 = at (i + 1, p), b1 = at (i + 1, q);
			if (upper)
			{
				piece.triangles.push_back ({a0, b1, b0});
				piece.triangles.push_back ({a0, a1, b1});
			}
			else
			{
				piece.triangles.push_back ({a0, a1, b0});
				piece.triangles.push_back ({b0, a1, b1});
			}
		}
	const std::size_t top = sh.size () - 1;
	for (int p = 0; p < nf; ++p)
	{
		piece.triangles.push_back ({at (0, nq - 1), at (0, p), at (0, p + 1)});
		piece.triangles.push_back ({at (top, nq - 1), at (top, p + 1), at (top, p)});
	}
}

// Relief de la calotte de base `radius` et de fleche `rise` a la distance rho de l'axe.
double Button (double rho, double radius, double rise)
{
	if (rise <= 0.0)
		return 0.0;
	const double sphere = (radius * radius + rise * rise) / (2.0 * rise);
	return std::sqrt (std::max (0.0, sphere * sphere - rho * rho)) - (sphere - rise);
}

} // namespace

CapitalTour ReadCapitalTour (const ColumnProfile &profile, const std::vector<double> &angles, const CapitalEnvelope &env)
{
	if (profile.vertices.size () < 2)
		throw std::invalid_argument ("ReadCapitalTour : profil de moins de deux sommets");
	if (angles.empty ())
		throw std::invalid_argument ("ReadCapitalTour : aucun angle");
	for (std::size_t k = 0; k < angles.size (); ++k)
		if (!(angles[k] >= 0.0 && angles[k] < 2.0 * kPi) || (k > 0 && !(angles[k] > angles[k - 1])))
			throw std::invalid_argument ("ReadCapitalTour : angles non croissants ou hors de [0, 2 pi)");
	double top = -std::numeric_limits<double>::infinity ();
	for (const ProfileMember &m : profile.members)
		if (m.block == kCapitalBlock)
			top = std::max (top, m.y1);
	if (!std::isfinite (top))
		throw std::invalid_argument ("ReadCapitalTour : chapiteau absent du profil");
	const double foot = env.foot;
	CapitalTour tour;
	tour.angles = angles;
	tour.top = top - foot;
	tour.h.reserve (profile.vertices.size ());
	tour.points.reserve (profile.vertices.size ());
	for (const ProfileVertex &v : profile.vertices)
	{
		const double h = v.y - foot;
		tour.h.push_back (h);
		const bool band = v.member >= 0 && profile.members[static_cast<std::size_t> (v.member)].block == kCapitalBlock &&
		                  v.ring.shape == RingShape::Circle && h >= env.BandBottom () && h <= env.BandTop ();
		const Ring ringAt = band ? Ring::MakeCircle (env.CoreRadius (h)) : v.ring;
		std::vector<std::array<double, 2>> ring (angles.size ());
		for (std::size_t k = 0; k < angles.size (); ++k)
		{
			const double r = ringAt.RadiusAt (angles[k]);
			ring[k] = {r * std::cos (angles[k]), r * std::sin (angles[k])};
		}
		tour.points.push_back (std::move (ring));
	}
	return tour;
}

double TourDepthMax (const CapitalTour &tour, double hLo, double hHi, double yLo, double yHi)
{
	double best = -std::numeric_limits<double>::infinity ();
	const std::size_t na = tour.angles.size ();
	for (std::size_t i = 0; i + 1 < tour.h.size (); ++i)
	{
		const double h0 = tour.h[i], h1 = tour.h[i + 1];
		if (std::max (h0, h1) < hLo || std::min (h0, h1) > hHi)
			continue;
		for (std::size_t k = 0; k < na; ++k)
		{
			const std::size_t k1 = (k + 1) % na;
			const Point a {tour.points[i][k][0], tour.points[i][k][1], h0};
			const Point b {tour.points[i][k1][0], tour.points[i][k1][1], h0};
			const Point c {tour.points[i + 1][k1][0], tour.points[i + 1][k1][1], h1};
			const Point d {tour.points[i + 1][k][0], tour.points[i + 1][k][1], h1};
			if (std::max ({a[1], b[1], c[1], d[1]}) < yLo || std::min ({a[1], b[1], c[1], d[1]}) > yHi ||
			    std::max ({a[0], b[0], c[0], d[0]}) <= best)
				continue;
			best = std::max ({best, ClippedMaxX (a, b, c, hLo, hHi, yLo, yHi), ClippedMaxX (a, c, d, hLo, hHi, yLo, yHi)});
		}
	}
	return best;
}

FleuronSpec DefaultFleuronSpec (OrnamentDetail detail)
{
	FleuronSpec s;
	if (detail == OrnamentDetail::Heavy)
	{
		s.samplesPerPetal = 12;
		s.rings = 9;
		s.stemSection = 12;
		s.stemRings = 24;
	}
	return s;
}

FleuronPiece BuildFleuron (const CapitalEnvelope &env, const CapitalTour &tour, const FleuronSpec &spec)
{
	CheckSpec (spec);
	CheckPetals (spec);
	const int n = spec.petals * spec.samplesPerPetal;
	const int rings = spec.rings;
	const int buttonRings = std::max (1, (rings - 1) / 4);
	const int petalRings = rings - 1 - buttonRings;
	const double c = spec.clearance;
	const double hc = tour.top - c - spec.tipRadius;
	CheckStem (spec, hc - spec.tipRadius);
	const auto at = [n] (int j, int i) { return static_cast<std::size_t> (j == 0 ? 0 : 1 + (j - 1) * n + i); };
	const std::size_t grid = static_cast<std::size_t> (1 + n * rings);

	// Avant : (y, h) et x ; i dans [0, n / 2], les autres par y -> -y. Anneau 1 : bord du noyau ; anneaux 2 a
	// 1 + buttonRings : bouton jusqu'a son bord ; puis les petales jusqu'au contour.
	std::vector<double> y (grid, 0.0), h (grid, hc), front (grid, spec.front + Button (0.0, spec.buttonRadius, spec.buttonRise));
	for (int i = 0; i <= n / 2; ++i)
	{
		const double alpha = 2.0 * kPi * i / n;
		int m = i % spec.samplesPerPetal;
		if (2 * m > spec.samplesPerPetal)
			m -= spec.samplesPerPetal;
		const Petal petal = PetalAt (spec, 2.0 * kPi * m / n);
		const double s = (i == 0 || 2 * i == n) ? 0.0 : std::sin (alpha), co = std::cos (alpha);
		for (int j = 1; j <= rings; ++j)
		{
			double rho = 0.0, relief = 0.0;
			if (j <= 1 + buttonRings)
			{
				rho = spec.coreRadius + (spec.buttonRadius - spec.coreRadius) * (j - 1) / buttonRings;
				relief = Button (rho, spec.buttonRadius, spec.buttonRise);
			}
			else
			{
				const double u = static_cast<double> (j - 1 - buttonRings) / petalRings;
				rho = spec.buttonRadius + (petal.radius - spec.buttonRadius) * u;
				relief = -petal.lobe * (spec.petalHollow * std::sin (kPi * u) + spec.tipFall * u * u);
			}
			for (const int k : {i, (n - i) % n})
			{
				const std::size_t v = at (j, k);
				y[v] = k == i ? -rho * s : rho * s;
				h[v] = hc + rho * co;
				front[v] = spec.front + relief;
			}
		}
	}

	// Diagonale des quads (j, i) - (j + 1, i + 1) pour i < n / 2, son image par y -> -y au-dela : les triangles
	// incidents a un sommet sont les images de ceux de son symetrique, ce qui permet de recopier le dos.
	std::vector<std::array<unsigned, 3>> face;
	face.reserve (static_cast<std::size_t> (n) * (2 * rings - 1));
	const auto u = [] (std::size_t v) { return static_cast<unsigned> (v); };
	for (int i = 0; i < n; ++i)
	{
		const int i1 = (i + 1) % n;
		face.push_back ({0u, u (at (1, i)), u (at (1, i1))});
		for (int j = 1; j < rings; ++j)
			if (2 * i < n)
			{
				face.push_back ({u (at (j, i)), u (at (j + 1, i)), u (at (j + 1, i1))});
				face.push_back ({u (at (j, i)), u (at (j + 1, i1)), u (at (j, i1))});
			}
			else
			{
				face.push_back ({u (at (j, i)), u (at (j + 1, i)), u (at (j, i1))});
				face.push_back ({u (at (j, i1)), u (at (j + 1, i)), u (at (j + 1, i1))});
			}
	}

	// Dos : boite (y, h) des triangles incidents a chaque sommet.
	std::vector<std::array<double, 4>> box (grid, {std::numeric_limits<double>::infinity (), -std::numeric_limits<double>::infinity (),
	                                               std::numeric_limits<double>::infinity (), -std::numeric_limits<double>::infinity ()});
	for (const auto &t : face)
		for (const unsigned v : t)
			for (const unsigned w : t)
			{
				box[v][0] = std::min (box[v][0], y[w]);
				box[v][1] = std::max (box[v][1], y[w]);
				box[v][2] = std::min (box[v][2], h[w]);
				box[v][3] = std::max (box[v][3], h[w]);
			}
	// Dos : noyau (centre et anneau 1) conforme au tour ; au-dela, tambour plan a front - petalThickness, releve au
	// tour + c la ou le tour passe devant (cone de la corbeille).
	std::vector<double> back (grid, 0.0);
	const auto depth = [&] (std::size_t v) {
		const double x = TourDepthMax (tour, box[v][2] - c, box[v][3] + c, box[v][0] - c, box[v][1] + c);
		if (!std::isfinite (x))
			Fail ("tour absent derriere la rosace");
		return x + c;
	};
	const double drum = spec.front - spec.petalThickness;
	back[0] = depth (0);
	for (int j = 1; j <= rings; ++j)
		for (int i = 0; i <= n / 2; ++i)
		{
			const double conform = std::max (depth (at (j, i)), depth (at (j, (n - i) % n)));
			const double x = j == 1 ? conform : std::max (drum, conform);
			back[at (j, i)] = x;
			back[at (j, (n - i) % n)] = x;
		}

	const double square = env.abacus.radius / std::sqrt (2.0) - c;
	for (std::size_t v = 0; v < grid; ++v)
	{
		if (front[v] - back[v] < spec.minThickness)
			Fail ("epaisseur " + std::to_string (front[v] - back[v]) + " sous minThickness a h " + std::to_string (h[v]));
		assert (h[v] <= tour.top - c + 1e-9);
		if (front[v] > square)
			Fail ("sommet hors du carre des cornes de l'abaque");
	}

	FleuronPiece piece;
	const double sc = env.scale;
	piece.positions.reserve (2 * grid);
	for (const std::vector<double> *x : {&front, &back})
		for (std::size_t v = 0; v < grid; ++v)
			piece.positions.push_back ({(*x)[v] * sc, y[v] * sc, (h[v] + env.foot) * sc});
	const unsigned b = u (grid);
	piece.triangles.reserve (static_cast<std::size_t> (4 * n * rings));
	for (const auto &t : face)
		piece.triangles.push_back (t);
	for (const auto &t : face)
		piece.triangles.push_back ({t[0] + b, t[2] + b, t[1] + b});
	for (int i = 0; i < n; ++i)
	{
		const unsigned f0 = u (at (rings, i)), f1 = u (at (rings, (i + 1) % n));
		if (2 * i < n)
		{
			piece.triangles.push_back ({f0, f0 + b, f1 + b});
			piece.triangles.push_back ({f0, f1 + b, f1});
		}
		else
		{
			piece.triangles.push_back ({f0, f0 + b, f1});
			piece.triangles.push_back ({f1, f0 + b, f1 + b});
		}
	}
	piece.rosetteVertices = static_cast<unsigned> (piece.positions.size ());
	piece.rosetteTriangles = static_cast<unsigned> (piece.triangles.size ());
	AppendStem (env, tour, spec, piece);
	return piece;
}

} // namespace cgarchi::classical::detail
