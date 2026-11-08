#include <cgarchi/classical_flutes.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfSqrt3 = 0.86602540378443865;

// Section a trois centres (classical_flutes.h), en unites de corde.
// Droites e-f et e-g par les points de division a 1/5 et 4/5 : x = -+(1/2 - 1/5).
constexpr double kThreeCentredDivision = 0.3;
// h au tiers de e-f cote f : h = e + (2/3) Rc u, rayon lateral Rc / 3.
constexpr double kThreeCentredLateralCentre = 2.0 / 3.0;

// |b - h|^2 - (Rc / 3)^2 pour un rayon central rc et une profondeur q : positif
// pour rc proche de q, negatif au-dela de la racine. Avec k = rc - q,
// L = |e - (X, 0)|, u = (X, -k) / L, mu = 1 - 2 lambda, la difference se
// factorise en
//   (1/2 - X rc / L)(1/2 + mu X rc / L) + (k / L)^2 (L - rc)(L + mu rc),
// et L - rc = X^2 / (L + k) - q : aucune annulation, ni carre de rc, si bien
// que q peut descendre jusqu'aux rc de l'ordre de 1e308.
double ThreeCentredMiss (double rc, double q)
{
	const double x = kThreeCentredDivision, mu = 1.0 - 2.0 * kThreeCentredLateralCentre;
	const double k = rc - q;
	const double l = std::hypot (x, k);
	const double lMinusRc = x * x / (l + k) - q;
	const double t = rc / l, kl = k / l;
	return (0.5 - x * t) * (0.5 + mu * x * t) + kl * kl * lMinusRc * (l + mu * rc);
}

// Points de la construction, en unites de corde. Les cotes sont ecrites depuis
// des differences stables (L - Rc), jamais comme difference de deux grandeurs
// de l'ordre de Rc.
struct ThreeCentredConstruction
{
	double q = 0.0, rc = 0.0, k = 0.0, l = 0.0, lMinusRc = 0.0;
	double ux = 0.0, uy = 0.0;   // direction e -> f
	double fx = 0.0, fy = 0.0;   // point de tangence
	double hx = 0.0, hy = 0.0;   // centre de l'arc lateral
	double rl = 0.0;             // rayon de l'arc lateral
	double axis = 0.0;           // distance de la corde a l'axe du fut, cos beta / w
};

ThreeCentredConstruction ConstructThreeCentred (double halfAngle, const ThreeCentredFlute &flute)
{
	const double x = kThreeCentredDivision, lambda = kThreeCentredLateralCentre;
	ThreeCentredConstruction c;
	c.q = flute.depthRatio;
	c.rc = flute.centralRadiusRatio;
	c.k = c.rc - c.q;
	c.l = std::hypot (x, c.k);
	c.lMinusRc = x * x / (c.l + c.k) - c.q;
	c.ux = x / c.l;
	c.uy = -c.k / c.l;
	// f = e + Rc u : (Rc X / L, k (L - Rc) / L).
	c.fx = c.rc * c.ux;
	c.fy = c.k / c.l * c.lMinusRc;
	// h = e + lambda Rc u : (lambda Rc X / L, k (L - lambda Rc) / L).
	c.hx = lambda * c.rc * c.ux;
	c.hy = c.k / c.l * (c.lMinusRc + (1.0 - lambda) * c.rc);
	c.rl = (1.0 - lambda) * c.rc;
	c.axis = std::cos (halfAngle) / (2.0 * std::sin (halfAngle));
	return c;
}

// Ecart angulaire, vu de h, de f a l'arete b (sens trigonometrique).
double LateralSweep (const ThreeCentredConstruction &c)
{
	const double bx = 0.5 - c.hx, by = -c.hy;
	return std::atan2 (c.ux * by - c.uy * bx, c.ux * bx + c.uy * by);
}

// Puissances C = |c - O|^2 - r^2 des deux cercles, O l'axe du fut, rapportees a
// la projection B de c - O sur la direction issue de l'axe. C est de l'ordre de
// axe x Rc, et Rc atteint 1e307 cordes pour q dans la fenetre sous-normale : C
// n'est donc jamais forme, chaque facteur de l'ordre de Rc est d'abord divise
// par B, du meme ordre.
// Arc central, centre e = (0, k) : C = (axe - q)(axe - q + 2 Rc).
double CentralPowerOverB (const ThreeCentredConstruction &c, double b)
{
	return (c.axis - c.q) * ((c.axis - c.q + 2.0 * c.rc) / b);
}

// Arc lateral, ecrit depuis l'arete b (sur le cercle a la precision de la
// racine) : C = (b - O).((h - O) + (h - b)) + |h - b|^2 - rl^2.
double LateralPowerOverB (const ThreeCentredConstruction &c, double b)
{
	return (0.5 * (2.0 * c.hx - 0.5) + ThreeCentredMiss (c.rc, c.q)) / b + c.axis * ((2.0 * c.hy + c.axis) / b);
}

// Distance a l'axe du point d'entree, le long d'une direction unitaire issue de
// l'axe, dans un cercle de puissance C, B etant la projection de c - O sur la
// direction : C / (B + sqrt (B^2 - C)), ecrit depuis cb = C / B, sans B^2.
double NearRoot (double b, double cb)
{
	return cb / (1.0 + std::sqrt (std::max (0.0, 1.0 - cb / b)));
}

// x dans l'arc angulaire ]start, start + sweep[, 0 < sweep < 2 pi.
bool AngleWithin (double x, double start, double sweep)
{
	double d = std::fmod (x - start, 2.0 * kPi);
	if (d < 0.0)
		d += 2.0 * kPi;
	return d > 0.0 && d < sweep;
}

// Le rayon issu de l'axe entre dans le cercle de chaque arc en tout point de
// l'arc : g = (P - O).n < 0, n normale unitaire issue du centre. Sur un arc de
// normale n(t), g = r + |c - O| cos (t - phi) n'a de maximum interieur qu'en
// t = phi, angle de c - O ; sinon son maximum est a une extremite.
bool ThreeCentredStarShaped (const ThreeCentredConstruction &c)
{
	// Fond entre l'axe et la corde, f sous la corde (l'arc central monte de d a f).
	if (!(c.axis - c.q > 0.0) || !(c.lMinusRc < 0.0))
		return false;
	// Arc central, normales de -pi/2 a l'angle de u (dans ]-pi/2, 0[) : phi = pi/2
	// n'y est pas ; extremites d, g = -(axis - q), et f.
	const double atF = c.fx * c.ux + (c.fy + c.axis) * c.uy;
	if (!(atF < 0.0))
		return false;
	// Arc lateral, normales de u a (b - h) / rl, dans le sens trigonometrique.
	const double sweep = LateralSweep (c);
	if (!(sweep > 0.0))
		return false;
	const double bx = 0.5 - c.hx, by = -c.hy;
	const double lb = std::hypot (bx, by);
	const double atB = 0.5 * (bx / lb) + c.axis * (by / lb);
	if (!(atB < 0.0))
		return false;
	const double start = std::atan2 (c.uy, c.ux);
	if (AngleWithin (std::atan2 (c.hy + c.axis, c.hx), start, sweep))
		return false;
	// Sous la corde : y = hy + rl sin t, de f (y < 0) a b (y = 0), sans passer par t = pi/2.
	return !AngleWithin (0.5 * kPi, start, sweep);
}

} // namespace

double FlutedRadius (double R, int count, double halfAngle, FluteSection section, double theta)
{
	const double pitch = 2.0 * kPi / count;
	const double offset = theta - std::round (theta / pitch) * pitch;
	// Arete : l'angle de grille d'une arete, recalcule modulo le pas, peut
	// tomber d'un ulp a l'interieur.
	if (std::fabs (offset) >= halfAngle - kFluteAngleTolerance)
		return R;
	const double s = std::sin (offset), c = std::cos (offset);
	const double chord = 2.0 * R * std::sin (halfAngle);
	double centre = 0.0, radius = 0.0;
	if (section == FluteSection::Semicircle)
	{
		centre = R * std::cos (halfAngle);
		radius = 0.5 * chord;
	}
	else
	{
		centre = R * std::cos (halfAngle) + chord * kHalfSqrt3;
		radius = chord;
	}
	// Racine proche de l'axe du rayon issu de l'axe avec le cercle de coupe.
	const double disc = radius * radius - centre * centre * s * s;
	return centre * c - std::sqrt (std::max (0.0, disc));
}

double FluteHalfAngle (const OrderData &order)
{
	if (!order.flutes)
		throw std::invalid_argument ("FluteHalfAngle : ordre sans cannelures : " + order.key);
	const FluteData &f = *order.flutes;
	double lambda = 0.0;
	if (f.listels)
		lambda = f.footListel / (kPi * order.general.lowerDiameter_p / f.count);
	return kPi / f.count * (1.0 - lambda);
}

double Segment60TowardAxisRadius (double R, double halfAngle, double offset)
{
	const double chord = 2.0 * R * std::sin (halfAngle);
	const double centre = R * std::cos (halfAngle) - chord * kHalfSqrt3;
	const double s = std::sin (offset), c = std::cos (offset);
	return centre * c + std::sqrt (std::max (0.0, chord * chord - centre * centre * s * s));
}

std::vector<double> FluteSampleOffsets (double halfAngle, FluteSection section, int samples)
{
	const int m = std::max (1, samples);
	// Geometrie a R = 1 : les decalages ne dependent que de beta.
	const double chord = 2.0 * std::sin (halfAngle);
	std::vector<double> out {0.0};
	if (section == FluteSection::Semicircle)
	{
		const double centre = std::cos (halfAngle), radius = 0.5 * chord;
		// Pied de la paroi : racine proche a l'angle beta, rayon cos 2 beta.
		const double foot = std::cos (2.0 * halfAngle);
		const double phiFoot = std::atan2 (foot * std::sin (halfAngle), centre - foot * std::cos (halfAngle));
		for (int k = 1; k < m; ++k)
		{
			const double phi = phiFoot * k / m;
			out.push_back (std::atan2 (radius * std::sin (phi), centre - radius * std::cos (phi)));
		}
		out.push_back (halfAngle * (1.0 - kFluteWallGap));
		out.push_back (halfAngle);
	}
	else
	{
		const double centre = std::cos (halfAngle) + chord * kHalfSqrt3, radius = chord;
		const double psiFoot = kPi / 6.0;   // demi-arc de 60 degres
		for (int k = 1; k < m; ++k)
		{
			const double psi = psiFoot * k / m;
			out.push_back (std::atan2 (radius * std::sin (psi), centre - radius * std::cos (psi)));
		}
		out.push_back (halfAngle);
	}
	return out;
}

std::vector<double> HeadHalfAngles (double halfAngle, FluteSection section, int samples, int rings)
{
	const std::vector<double> offsets = FluteSampleOffsets (halfAngle, section, samples);
	// Sommets de l'arc seuls (ni fond ni paroi), indices 1 .. m-1.
	const int m = std::max (1, samples);
	std::vector<double> out;
	const int n = std::max (0, std::min (rings, m - 1));
	for (int i = 1; i <= n; ++i)
		out.push_back (offsets[static_cast<std::size_t> (m - (i * m) / (n + 1))]);
	std::sort (out.begin (), out.end (), std::greater<double> ());
	out.erase (std::unique (out.begin (), out.end ()), out.end ());
	return out;
}

std::vector<double> FluteGridOffsets (double halfAngle, FluteSection section, int samples, int headRings)
{
	std::vector<double> out = FluteSampleOffsets (halfAngle, section, samples);
	if (section == FluteSection::Semicircle)
		for (double head : HeadHalfAngles (halfAngle, section, samples, headRings))
			out.push_back (head * (1.0 - kFluteWallGap));
	std::sort (out.begin (), out.end ());
	out.erase (std::unique (out.begin (), out.end (), [] (double x, double y) { return y - x < kFluteAngleTolerance; }),
	           out.end ());
	return out;
}

ThreeCentredFlute SolveThreeCentredFlute (double depthRatio)
{
	const double q = depthRatio;
	if (!(q > 0.0 && q < 0.5))
		throw std::invalid_argument ("SolveThreeCentredFlute : profondeur / corde hors de ]0, 1/2[");
	// ThreeCentredMiss (q, q) = (1/2 - q)(1/2 + mu q) > 0 : encadrement par doublement,
	// puis bissection jusqu'a deux double voisins.
	double lo = q, hi = std::max (1.0, 2.0 * q);
	while (ThreeCentredMiss (hi, q) > 0.0)
	{
		lo = hi;
		hi *= 2.0;
		if (!std::isfinite (hi))
			throw std::invalid_argument ("SolveThreeCentredFlute : rayon central non representable en double");
	}
	for (;;)
	{
		const double mid = 0.5 * (lo + hi);
		if (!(mid > lo && mid < hi))
			break;
		if (ThreeCentredMiss (mid, q) > 0.0)
			lo = mid;
		else
			hi = mid;
	}
	return {q, hi};
}

bool IsThreeCentredFluteStarShaped (double halfAngle, double depthRatio)
{
	if (!(depthRatio > 0.0 && depthRatio < 0.5) || !(halfAngle > 0.0 && halfAngle < 0.5 * kPi))
		return false;
	ThreeCentredFlute flute;
	try
	{
		flute = SolveThreeCentredFlute (depthRatio);
	}
	catch (const std::invalid_argument &)
	{
		return false;
	}
	return ThreeCentredStarShaped (ConstructThreeCentred (halfAngle, flute));
}

double ThreeCentredFluteRadius (double R, int count, double halfAngle, const ThreeCentredFlute &flute, double theta)
{
	const double pitch = 2.0 * kPi / count;
	const double offset = std::fabs (theta - std::round (theta / pitch) * pitch);
	if (offset >= halfAngle - kFluteAngleTolerance)
		return R;
	const ThreeCentredConstruction c = ConstructThreeCentred (halfAngle, flute);
	const double s = std::sin (offset), co = std::cos (offset);
	double t = 0.0;
	if (offset <= std::atan2 (c.fx, c.axis + c.fy))
	{
		const double b = (c.k + c.axis) * co;
		t = NearRoot (b, CentralPowerOverB (c, b));
	}
	else
	{
		const double b = c.hx * s + (c.hy + c.axis) * co;
		t = NearRoot (b, LateralPowerOverB (c, b));
	}
	return 2.0 * R * std::sin (halfAngle) * t;
}

std::vector<double> ThreeCentredFluteOffsets (double halfAngle, const ThreeCentredFlute &flute, int samples)
{
	const int m = std::max (1, samples);
	const ThreeCentredConstruction c = ConstructThreeCentred (halfAngle, flute);
	const double start = std::atan2 (c.uy, c.ux);
	// Demi-arc central vu de e, de d a f : angle de (0, -1) a u.
	const double centralLength = c.rc * std::atan2 (kThreeCentredDivision, c.k);
	const double total = centralLength + c.rl * LateralSweep (c);
	auto polar = [&] (double x, double y) { return std::atan2 (x, c.axis + y); };

	std::vector<double> out {0.0};
	for (int i = 1; i < m; ++i)
	{
		const double s = total * i / m;
		if (s <= centralLength)
		{
			// d + Rc (sin phi, 1 - cos phi).
			const double phi = s / c.rc, half = std::sin (0.5 * phi);
			out.push_back (polar (c.rc * std::sin (phi), -c.q + 2.0 * c.rc * half * half));
		}
		else
		{
			// f + 2 rl sin (psi / 2) (-sin (a + psi / 2), cos (a + psi / 2)), a angle de u.
			const double psi = (s - centralLength) / c.rl, chord = 2.0 * c.rl * std::sin (0.5 * psi);
			out.push_back (polar (c.fx - chord * std::sin (start + 0.5 * psi), c.fy + chord * std::cos (start + 0.5 * psi)));
		}
	}
	out.push_back (polar (c.fx, c.fy));
	out.push_back (halfAngle);
	std::sort (out.begin (), out.end ());
	out.erase (std::unique (out.begin (), out.end (), [] (double x, double y) { return y - x < kFluteAngleTolerance; }),
	           out.end ());
	// Un sommet confondu avec l'arete a la tolerance la remplace : l'arete reste exacte.
	out.back () = halfAngle;
	return out;
}

} // namespace classical
} // namespace cgarchi
