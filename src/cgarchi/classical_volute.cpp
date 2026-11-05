#include <cgarchi/classical_volute.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// Direction de fin d'arc pour le raccord Axis : quart k (mod 4) du tour.
const std::array<VolutePoint, 4> kAxisEnd = {{ {1.0, 0.0}, {0.0, -1.0}, {-1.0, 0.0}, {0.0, 1.0} }};

double Angle (VolutePoint centre, VolutePoint p)
{
	return std::atan2 (p.v - centre.v, p.u - centre.u);
}

// Arc horaire de P a E autour de `centre`.
VoluteArc MakeArc (VolutePoint centre, double radius, VolutePoint P, VolutePoint E)
{
	VoluteArc arc;
	arc.centre = centre;
	arc.radius = radius;
	arc.a0 = Angle (centre, P);
	arc.a1 = Angle (centre, E);
	while (arc.a1 > arc.a0)
		arc.a1 -= kTwoPi;
	if (arc.a0 - arc.a1 > kTwoPi - 1e-9)
		arc.a1 += kTwoPi;
	return arc;
}

std::vector<VolutePoint> Centres (double e, double shift)
{
	std::vector<VolutePoint> out;
	for (double a : { e / 2.0, e / 3.0, e / 6.0 })
	{
		const double s = a - shift;
		out.push_back ({-s, s});
		out.push_back ({s, s});
		out.push_back ({s, -s});
		out.push_back ({-s, -s});
	}
	return out;
}

std::vector<VoluteArc> Arcs (VolutePoint start, const std::vector<VolutePoint> &c, VoluteJunction junction)
{
	std::vector<VoluteArc> arcs;
	VolutePoint P = start;
	double r = std::hypot (P.u - c[0].u, P.v - c[0].v);
	for (std::size_t k = 0; k < c.size (); ++k)
	{
		if (junction == VoluteJunction::Axis)
			r = std::hypot (P.u - c[k].u, P.v - c[k].v);
		VolutePoint E;
		double next = r;
		if (junction == VoluteJunction::CentreLine && k + 1 < c.size ())
		{
			const double du = c[k + 1].u - c[k].u, dv = c[k + 1].v - c[k].v;
			const double L = std::hypot (du, dv);
			if (!(L > 0.0))
				throw std::invalid_argument ("BuildTwelveCentreVolute : deux centres consecutifs confondus");
			E = {c[k].u + r * du / L, c[k].v + r * dv / L};
			next = r - L;
		}
		else
		{
			const VolutePoint d = kAxisEnd[k % 4];
			E = {c[k].u + r * d.u, c[k].v + r * d.v};
		}
		if (!(r > 0.0))
			throw std::invalid_argument ("BuildTwelveCentreVolute : rayon d'arc non positif");
		arcs.push_back (MakeArc (c[k], r, P, E));
		const double sweep = arcs.back ().a0 - arcs.back ().a1;
		if (!(sweep > 0.0 && sweep < kPi))
			throw std::invalid_argument ("BuildTwelveCentreVolute : arc hors du demi-tour");
		P = E;
		r = next;
	}
	return arcs;
}

// Angle de l'arc ou il coupe le cercle de centre O et de rayon e, le premier
// dans le sens de l'arc ; nullopt s'il ne le coupe pas.
std::optional<double> CircleCrossing (const VoluteArc &arc, double e)
{
	const double m = std::hypot (arc.centre.u, arc.centre.v);
	if (m == 0.0)
		return std::nullopt;
	const double K = (e * e - m * m - arc.radius * arc.radius) / (2.0 * arc.radius);
	if (std::fabs (K) > m)
		return std::nullopt;
	const double phi = std::atan2 (arc.centre.v, arc.centre.u);
	const double delta = std::acos (K / m);
	std::optional<double> best;
	for (double base : { phi + delta, phi - delta })
		for (int k = -3; k <= 3; ++k)
		{
			const double t = base + k * kTwoPi;
			if (t <= arc.a0 + 1e-12 && t >= arc.a1 - 1e-12 && (!best || t > *best))
				best = t;
		}
	return best;
}

// Courbe coupee a son entree dans l'oeil ; `entry` recoit le point d'entree.
VoluteCurve TruncateAtEye (const VoluteCurve &curve, double e, VolutePoint &entry)
{
	VoluteCurve out;
	out.prefixStart = curve.prefixStart;
	for (const VoluteArc &arc : curve.arcs)
	{
		const VolutePoint start = arc.Start ();
		if (std::hypot (start.u, start.v) > e)
		{
			const std::optional<double> t = CircleCrossing (arc, e);
			if (t && *t < arc.a0)
			{
				VoluteArc cut = arc;
				cut.a1 = *t;
				out.arcs.push_back (cut);
				entry = cut.End ();
				return out;
			}
		}
		out.arcs.push_back (arc);
	}
	throw std::invalid_argument ("BuildVoluteSection : la courbe n'entre pas dans l'oeil");
}

int SegmentsFor (double sweep, int segmentsPerQuarter)
{
	return std::max (1, static_cast<int> (std::ceil (sweep / (0.5 * kPi) * segmentsPerQuarter - 1e-9)));
}

double Cross (VolutePoint o, VolutePoint a, VolutePoint b)
{
	return (a.u - o.u) * (b.v - o.v) - (a.v - o.v) * (b.u - o.u);
}

bool OnSegment (VolutePoint p, VolutePoint a, VolutePoint b)
{
	return std::min (a.u, b.u) <= p.u && p.u <= std::max (a.u, b.u) &&
	       std::min (a.v, b.v) <= p.v && p.v <= std::max (a.v, b.v);
}

bool SegmentsTouch (VolutePoint a, VolutePoint b, VolutePoint c, VolutePoint d)
{
	const double d1 = Cross (c, d, a), d2 = Cross (c, d, b);
	const double d3 = Cross (a, b, c), d4 = Cross (a, b, d);
	if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
		return true;
	return (d1 == 0 && OnSegment (a, c, d)) || (d2 == 0 && OnSegment (b, c, d)) ||
	       (d3 == 0 && OnSegment (c, a, b)) || (d4 == 0 && OnSegment (d, a, b));
}

void OrientCounterClockwise (const std::vector<VolutePoint> &points, std::vector<unsigned> &polygon)
{
	if (PolygonArea (points, polygon) < 0.0)
		std::reverse (polygon.begin (), polygon.end ());
}

// Angle polaire autour de l'oeil, horaire depuis le haut : atan2 (u, v).
double PolarAngle (VolutePoint p)
{
	return std::atan2 (p.u, p.v);
}

double Modulo (double psi)
{
	double m = std::fmod (psi, kTwoPi);
	if (m < 0.0)
		m += kTwoPi;
	return m;
}

// Courbe d'arcs parametree par son angle polaire deroule autour de l'oeil.
// Chaque cercle d'arc contient le centre de l'oeil, donc un rayon issu de ce
// centre le coupe une fois : le point d'angle psi est unique sur chaque arc.
struct PolarCurve
{
	explicit PolarCurve (const VoluteCurve &curve) : arcs (curve.arcs)
	{
		if (arcs.empty ())
			throw std::invalid_argument ("PolarCurve : courbe vide");
		start = arcs.front ().Start ();
		double psi = PolarAngle (start);
		psiStart = psi;
		for (const VoluteArc &arc : arcs)
		{
			if (!(arc.radius > std::hypot (arc.centre.u, arc.centre.v)))
				throw std::invalid_argument ("PolarCurve : un cercle d'arc ne contient pas le centre de l'oeil");
			const double end = Unwrap (PolarAngle (arc.End ()), psi);
			if (!(end > psi))
				throw std::invalid_argument ("PolarCurve : angle polaire non croissant");
			ranges.push_back ({psi, end});
			psi = end;
		}
		psiEnd = psi;
	}

	// Representant de `psi` modulo 2 pi le plus proche de `near`.
	static double Unwrap (double psi, double near)
	{
		return psi + std::round ((near - psi) / kTwoPi) * kTwoPi;
	}

	VolutePoint At (double psi) const
	{
		std::size_t k = 0;
		while (k + 1 < ranges.size () && psi > ranges[k].second)
			++k;
		const VoluteArc &arc = arcs[k];
		const double du = std::sin (psi), dv = std::cos (psi);
		const double b = du * arc.centre.u + dv * arc.centre.v;
		const double c = arc.centre.u * arc.centre.u + arc.centre.v * arc.centre.v - arc.radius * arc.radius;
		const double t = b + std::sqrt (std::max (0.0, b * b - c));
		return {t * du, t * dv};
	}

	std::vector<VoluteArc> arcs;
	std::vector<std::pair<double, double>> ranges;
	VolutePoint start;
	double psiStart = 0.0, psiEnd = 0.0;
};

} // namespace

VolutePoint VoluteArc::At (double angle) const
{
	return {centre.u + radius * std::cos (angle), centre.v + radius * std::sin (angle)};
}

VoluteSpiral BuildTwelveCentreVolute (const IonicVoluteData &data, VoluteJunction junction,
                                      std::optional<double> innerCentreShift)
{
	const double e = data.eyeDiameter / 2.0;
	const double rTop = data.centreFromTop;
	if (!(e > 0.0) || !(rTop > e))
		throw std::invalid_argument ("BuildTwelveCentreVolute : oeil ou hauteur hors domaine");
	const double a1 = e / 2.0;

	VoluteSpiral spiral;
	spiral.eyeRadius = e;
	spiral.outer.arcs = Arcs ({-a1, rTop}, Centres (e, 0.0), junction);

	// Retour du premier tour a la cathete du premier centre : le 4e arc,
	// centre (-a1, -a1), passe a la verticale de son centre en v = -a1 + r4.
	const VoluteArc &fourth = spiral.outer.arcs[3];
	const double vReturn = fourth.centre.v + fourth.radius;
	spiral.listelWidth = (rTop - vReturn) / 4.0;

	const double shift = innerCentreShift.value_or (a1 / 12.0);
	// Les centres interieurs du dernier tour sont a +-(e/6 - shift) : au-dela
	// de e/6 ils se confondent ou s'inversent.
	if (!(shift < e / 6.0))
		throw std::invalid_argument ("BuildTwelveCentreVolute : decalage des centres interieurs >= e/6");
	const double v0 = rTop - spiral.listelWidth;
	spiral.inner.prefixStart = VolutePoint {-a1, v0};
	spiral.inner.arcs = Arcs ({-(a1 - shift), v0}, Centres (e, shift), junction);
	return spiral;
}

std::vector<VolutePoint> SampleCurve (const VoluteCurve &curve, int segmentsPerQuarter)
{
	std::vector<VolutePoint> out;
	if (curve.prefixStart)
		out.push_back (*curve.prefixStart);
	for (std::size_t k = 0; k < curve.arcs.size (); ++k)
	{
		const VoluteArc &arc = curve.arcs[k];
		if (k == 0)
			out.push_back (arc.Start ());
		const int n = SegmentsFor (arc.a0 - arc.a1, segmentsPerQuarter);
		for (int i = 1; i <= n; ++i)
			out.push_back (arc.At (i == n ? arc.a1 : arc.a0 + (arc.a1 - arc.a0) * i / n));
	}
	return out;
}

double MaxTangentBreak (const VoluteCurve &curve)
{
	auto tangent = [] (double a) { return VolutePoint {std::sin (a), -std::cos (a)}; };
	auto between = [] (VolutePoint a, VolutePoint b) {
		return std::fabs (std::atan2 (a.u * b.v - a.v * b.u, a.u * b.u + a.v * b.v));
	};
	double worst = 0.0;
	if (curve.prefixStart && !curve.arcs.empty ())
	{
		const VolutePoint s = curve.arcs.front ().Start ();
		const double L = std::hypot (s.u - curve.prefixStart->u, s.v - curve.prefixStart->v);
		if (L > 0.0)
			worst = between ({(s.u - curve.prefixStart->u) / L, (s.v - curve.prefixStart->v) / L},
			                 tangent (curve.arcs.front ().a0));
	}
	for (std::size_t k = 0; k + 1 < curve.arcs.size (); ++k)
		worst = std::max (worst, between (tangent (curve.arcs[k].a1), tangent (curve.arcs[k + 1].a0)));
	return worst;
}

VoluteBox CurveExtents (const VoluteCurve &curve, std::size_t arcCount)
{
	VoluteBox box {1e300, -1e300, 1e300, -1e300};
	auto take = [&] (VolutePoint p) {
		box.uMin = std::min (box.uMin, p.u);
		box.uMax = std::max (box.uMax, p.u);
		box.vMin = std::min (box.vMin, p.v);
		box.vMax = std::max (box.vMax, p.v);
	};
	for (std::size_t k = 0; k < std::min (arcCount, curve.arcs.size ()); ++k)
	{
		const VoluteArc &arc = curve.arcs[k];
		take (arc.Start ());
		take (arc.End ());
		for (int q = -8; q <= 8; ++q)
		{
			const double t = q * 0.5 * kPi;
			if (t <= arc.a0 && t >= arc.a1)
			{
				// Extremum d'axe exact : cos/sin valent 0 ou +-1.
				const int quadrant = ((q % 4) + 4) % 4;
				const double cu = quadrant == 0 ? 1.0 : quadrant == 2 ? -1.0 : 0.0;
				const double sv = quadrant == 1 ? 1.0 : quadrant == 3 ? -1.0 : 0.0;
				take ({arc.centre.u + arc.radius * cu, arc.centre.v + arc.radius * sv});
			}
		}
	}
	return box;
}

double PolygonArea (const std::vector<VolutePoint> &points, const std::vector<unsigned> &polygon)
{
	double twice = 0.0;
	for (std::size_t i = 0; i < polygon.size (); ++i)
	{
		const VolutePoint &a = points[polygon[i]];
		const VolutePoint &b = points[polygon[(i + 1) % polygon.size ()]];
		twice += a.u * b.v - a.v * b.u;
	}
	return 0.5 * twice;
}

bool PolygonSelfIntersects (const std::vector<VolutePoint> &points, const std::vector<unsigned> &polygon)
{
	// Balayage selon u : aretes triees par u minimal ; une arete n'est
	// comparee qu'aux suivantes dont l'intervalle en u chevauche le sien, puis
	// filtree par l'intervalle en v. O(n log n + k), k paires candidates.
	const std::size_t n = polygon.size ();
	struct Span { double u0, u1, v0, v1; std::size_t edge; };
	std::vector<Span> spans;
	spans.reserve (n);
	for (std::size_t i = 0; i < n; ++i)
	{
		const VolutePoint a = points[polygon[i]], b = points[polygon[(i + 1) % n]];
		spans.push_back ({std::min (a.u, b.u), std::max (a.u, b.u), std::min (a.v, b.v), std::max (a.v, b.v), i});
	}
	std::sort (spans.begin (), spans.end (), [] (const Span &x, const Span &y) { return x.u0 < y.u0; });
	auto neighbours = [n] (std::size_t i, std::size_t j) {
		const std::size_t d = i > j ? i - j : j - i;
		return d <= 1 || d == n - 1;
	};
	for (std::size_t x = 0; x < n; ++x)
		for (std::size_t y = x + 1; y < n && spans[y].u0 <= spans[x].u1; ++y)
		{
			const Span &p = spans[x], &q = spans[y];
			if (q.v0 > p.v1 || p.v0 > q.v1 || neighbours (p.edge, q.edge))
				continue;
			if (SegmentsTouch (points[polygon[p.edge]], points[polygon[(p.edge + 1) % n]],
			                   points[polygon[q.edge]], points[polygon[(q.edge + 1) % n]]))
				return true;
		}
	return false;
}

VoluteSection BuildVoluteSection (const VoluteSpiral &spiral, double closureU, int segmentsPerQuarter)
{
	const double e = spiral.eyeRadius;
	if (spiral.outer.arcs.size () < 4)
		throw std::invalid_argument ("BuildVoluteSection : moins d'un tour");

	// Point de retour Q du premier tour sur u = closureU, au-dessus du centre
	// du 4e arc.
	const VoluteArc &fourth = spiral.outer.arcs[3];
	const double cq = (closureU - fourth.centre.u) / fourth.radius;
	if (std::fabs (cq) > 1.0)
		throw std::invalid_argument ("BuildVoluteSection : le premier tour ne recoupe pas la fermeture");
	double tq = std::acos (cq);
	while (tq > fourth.a0) tq -= kTwoPi;
	while (tq < fourth.a1) tq += kTwoPi;
	if (tq > fourth.a0 || tq < fourth.a1)
		throw std::invalid_argument ("BuildVoluteSection : le premier tour ne recoupe pas la fermeture");
	const VolutePoint Q {closureU, fourth.centre.v + fourth.radius * std::sin (tq)};

	VolutePoint xo, xi;
	const VoluteCurve outerCut = TruncateAtEye (spiral.outer, e, xo);
	const VoluteCurve innerCut = TruncateAtEye (spiral.inner, e, xi);
	if (!innerCut.prefixStart)
		throw std::invalid_argument ("BuildVoluteSection : bord interieur sans prefixe");

	const PolarCurve o (outerCut);
	const PolarCurve in (innerCut);
	if (std::fabs (o.start.u - closureU) > 1e-9)
		throw std::invalid_argument ("BuildVoluteSection : le sommet de la volute n'est pas sur la fermeture");
	const double psiQ = o.Unwrap (PolarAngle (Q), o.psiStart + kPi);
	if (!(psiQ < o.psiEnd))
		throw std::invalid_argument ("BuildVoluteSection : la spirale entre dans l'oeil avant la fin du premier tour");

	// Grille commune, 2 pi-periodique : pas regulier et angles remarquables.
	// Contour, bord interieur et oeil echantillonnes aux memes angles : dans
	// chaque secteur, l'ordre radial des points est celui des courbes, et les
	// cordes ne se croisent pas, quelle que soit la resolution.
	std::vector<double> base;
	const int perTurn = 4 * std::max (1, segmentsPerQuarter);
	for (int k = 0; k < perTurn; ++k)
		base.push_back (kTwoPi * k / perTurn);
	for (double psi : { o.psiStart, psiQ, o.psiEnd, in.psiStart, in.psiEnd })
		base.push_back (Modulo (psi));
	// Raccords et extremes d'axe des arcs du contour : les cotes 9, 8, 7, 6 du
	// premier tour sont des sommets.
	for (const VoluteArc &arc : outerCut.arcs)
	{
		base.push_back (Modulo (PolarAngle (arc.Start ())));
		for (int q = -8; q <= 8; ++q)
		{
			const double t = q * 0.5 * kPi;
			if (t < arc.a0 && t > arc.a1)
				base.push_back (Modulo (PolarAngle (arc.At (t))));
		}
	}
	std::sort (base.begin (), base.end ());

	auto anglesIn = [&] (double from, double to) {
		std::vector<double> out { from };
		for (double turn = std::floor (from / kTwoPi) * kTwoPi; turn < to; turn += kTwoPi)
			for (double b : base)
			{
				const double psi = turn + b;
				if (psi > out.back () + 1e-9 && psi < to - 1e-9)
					out.push_back (psi);
			}
		out.push_back (to);
		return out;
	};

	VoluteSection section;
	std::vector<VolutePoint> &pts = section.points;

	// Contour exterieur, du sommet a son entree dans l'oeil.
	std::size_t iQ = 0;
	for (double psi : anglesIn (o.psiStart, o.psiEnd))
	{
		if (std::fabs (psi - psiQ) <= 1e-9)
		{
			iQ = pts.size ();
			pts.push_back (Q);
		}
		else
			pts.push_back (o.At (psi));
	}
	pts.front () = {closureU, o.start.v};
	pts.back () = xo;
	const unsigned nO = static_cast<unsigned> (pts.size ());
	if (iQ == 0)
		throw std::invalid_argument ("BuildVoluteSection : point de retour du premier tour non echantillonne");

	// Bord interieur : debut du prefixe sur la fermeture, puis les arcs.
	const unsigned iI = static_cast<unsigned> (pts.size ());
	pts.push_back ({closureU, innerCut.prefixStart->v});
	for (double psi : anglesIn (in.psiStart, in.psiEnd))
		pts.push_back (in.At (psi));
	pts.back () = xi;
	const unsigned nI = static_cast<unsigned> (pts.size ()) - iI;

	// Arc de l'oeil de xo a xi, dans le sens de la spirale.
	const unsigned iEye = static_cast<unsigned> (pts.size ());
	{
		const double from = Modulo (o.psiEnd);
		double to = Modulo (in.psiEnd);
		while (to <= from) to += kTwoPi;
		const std::vector<double> eye = anglesIn (from, to);
		for (std::size_t k = 1; k + 1 < eye.size (); ++k)
			pts.push_back ({e * std::sin (eye[k]), e * std::cos (eye[k])});
	}
	const unsigned nEyeInterior = static_cast<unsigned> (pts.size ()) - iEye;

	auto appendInnerReversed = [&] (std::vector<unsigned> &loop) {
		for (unsigned k = 0; k < nEyeInterior; ++k)
			loop.push_back (iEye + k);
		for (unsigned k = nI; k-- > 0;)
			loop.push_back (iI + k);
	};

	// S : premier tour, puis fermeture Q -> debut du prefixe -> sommet.
	for (unsigned k = 0; k <= iQ; ++k)
		section.body.push_back (k);
	section.body.push_back (iI);

	// R : contour exterieur jusqu'a l'oeil, arc de l'oeil, bord interieur a
	// rebours ; la fermeture debut du prefixe -> sommet referme.
	for (unsigned k = 0; k < nO; ++k)
		section.raised.push_back (k);
	appendInnerReversed (section.raised);

	// C : contour exterieur depuis Q, meme arc, meme bord a rebours ; la
	// fermeture debut du prefixe -> Q referme.
	for (unsigned k = static_cast<unsigned> (iQ); k < nO; ++k)
		section.canal.push_back (k);
	appendInnerReversed (section.canal);

	OrientCounterClockwise (pts, section.body);
	OrientCounterClockwise (pts, section.raised);
	OrientCounterClockwise (pts, section.canal);
	ValidateVoluteSection (section);
	return section;
}

void ValidateVoluteSection (const VoluteSection &section)
{
	const auto &pts = section.points;
	for (const std::vector<unsigned> *polygon : { &section.body, &section.raised, &section.canal })
	{
		if (polygon->size () < 3)
			throw std::invalid_argument ("VoluteSection : polygone de moins de trois points");
		for (unsigned index : *polygon)
			if (index >= pts.size ())
				throw std::invalid_argument ("VoluteSection : indice hors bornes");
		if (!(PolygonArea (pts, *polygon) > 0.0))
			throw std::invalid_argument ("VoluteSection : polygone d'aire non positive");
		if (PolygonSelfIntersects (pts, *polygon))
			throw std::invalid_argument ("VoluteSection : polygone non simple");
	}
	const double s = PolygonArea (pts, section.body);
	const double r = PolygonArea (pts, section.raised);
	const double c = PolygonArea (pts, section.canal);
	if (std::fabs (c + r - s) > 1e-9 * s)
		throw std::invalid_argument ("VoluteSection : C et R ne partitionnent pas S");
}

} // namespace classical
} // namespace cgarchi
