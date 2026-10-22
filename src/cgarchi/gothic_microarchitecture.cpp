#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_detail.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace cgarchi::microarch
{

namespace
{
	constexpr double kPi    = 3.14159265358979323846;
	constexpr double kTwoPi = 2.0 * kPi;

	// Operations sur Vector2d par valeur : les operateurs arithmetiques de
	// TVector2 ne sont pas tous const.
	Vector2d add (const Vector2d &a, const Vector2d &b) { return Vector2d(a.x + b.x, a.y + b.y); }
	Vector2d sub (const Vector2d &a, const Vector2d &b) { return Vector2d(a.x - b.x, a.y - b.y); }
	Vector2d mul (const Vector2d &a, double s)          { return Vector2d(a.x * s, a.y * s); }
	double   dot (const Vector2d &a, const Vector2d &b) { return a.x * b.x + a.y * b.y; }
	double   cross (const Vector2d &a, const Vector2d &b) { return a.x * b.y - a.y * b.x; }
	double   norm (const Vector2d &a)                   { return std::hypot(a.x, a.y); }
	double   distance (const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }
	bool     finite (const Vector2d &a)                 { return std::isfinite(a.x) && std::isfinite(a.y); }

	// Angle ramene dans [0, 2*pi).
	double wrapTwoPi (double a)
	{
		double w = a - kTwoPi * std::floor(a / kTwoPi);
		if (w >= kTwoPi || w < 0.0)
			w = 0.0;
		return w;
	}

	double signOf (double v) { return v < 0.0 ? -1.0 : 1.0; }

	// k ramene dans [0, n).
	int reduce (int k, int n) { return ((k % n) + n) % n; }

	std::string indexed (const char *array, std::size_t i)
	{
		return std::string(array) + "[" + std::to_string(i) + "]";
	}

	std::string number (double v)
	{
		std::ostringstream os;
		os.precision(17);
		os << v;
		return os.str();
	}

	std::string elementText (SymmetryElement g)
	{
		return "(k=" + std::to_string(g.k) + (g.reflected ? ", reflected)" : ")");
	}
}

// ===========================================================================
//  Tolerances
// ===========================================================================

ResolvedTolerances Resolve (const Tolerances &tol, double scale)
{
	ResolvedTolerances r;
	r.scale     = scale;
	r.point     = tol.point * scale;
	r.parameter = tol.parameter * scale;
	r.height    = tol.height * scale;
	r.prc_snap  = tol.prc_snap;
	return r;
}

// ===========================================================================
//  Courbes du plan
// ===========================================================================

double Length (const PlanCurve &curve)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
		return distance(seg->start, seg->end);
	const PlanArc &arc = std::get<PlanArc>(curve);
	return arc.radius * std::fabs(arc.sweep);
}

namespace
{
	double arcAngleAt (const PlanArc &arc, double s)
	{
		return arc.start_angle + signOf(arc.sweep) * s / arc.radius;
	}

	Vector2d onCircle (const Vector2d &center, double radius, double angle)
	{
		return Vector2d(center.x + radius * std::cos(angle), center.y + radius * std::sin(angle));
	}
}

Vector2d PointAt (const PlanCurve &curve, double s)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
	{
		const double L = distance(seg->start, seg->end);
		if (L == 0.0)
			return seg->start;
		return add(seg->start, mul(sub(seg->end, seg->start), s / L));
	}
	const PlanArc &arc = std::get<PlanArc>(curve);
	return onCircle(arc.center, arc.radius, arcAngleAt(arc, s));
}

Vector2d TangentAt (const PlanCurve &curve, double s)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
	{
		const Vector2d d = sub(seg->end, seg->start);
		const double L = norm(d);
		return L == 0.0 ? Vector2d(0.0, 0.0) : mul(d, 1.0 / L);
	}
	const PlanArc &arc = std::get<PlanArc>(curve);
	const double theta = arcAngleAt(arc, s);
	const double sg = signOf(arc.sweep);
	return Vector2d(-sg * std::sin(theta), sg * std::cos(theta));
}

Vector2d StartPoint (const PlanCurve &curve)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
		return seg->start;
	const PlanArc &arc = std::get<PlanArc>(curve);
	return onCircle(arc.center, arc.radius, arc.start_angle);
}

Vector2d EndPoint (const PlanCurve &curve)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
		return seg->end;
	const PlanArc &arc = std::get<PlanArc>(curve);
	return onCircle(arc.center, arc.radius, arc.start_angle + arc.sweep);
}

std::optional<double> ParameterOf (const PlanCurve &curve, const Vector2d &p, double tol)
{
	if (const auto *seg = std::get_if<PlanSegment>(&curve))
	{
		const Vector2d d = sub(seg->end, seg->start);
		const double L = norm(d);
		const Vector2d w = sub(p, seg->start);
		if (L == 0.0)
			return norm(w) <= tol ? std::optional<double>(0.0) : std::nullopt;
		const double s = dot(w, d) / L;
		if (std::fabs(cross(d, w)) / L > tol || s < -tol || s > L + tol)
			return std::nullopt;
		return std::clamp(s, 0.0, L);
	}
	const PlanArc &arc = std::get<PlanArc>(curve);
	const Vector2d v = sub(p, arc.center);
	if (std::fabs(norm(v) - arc.radius) > tol)
		return std::nullopt;
	const double span  = std::fabs(arc.sweep);
	const double slack = tol / arc.radius;
	// Ecart angulaire depuis le depart, dans le sens de parcours. Un point situe
	// juste avant le depart revient a ~2*pi : il est rabattu sur le depart.
	double delta = wrapTwoPi(signOf(arc.sweep) * (std::atan2(v.y, v.x) - arc.start_angle));
	if (delta > span + slack && kTwoPi - delta <= slack)
		delta = 0.0;
	if (delta > span + slack)
		return std::nullopt;
	return arc.radius * std::min(delta, span);
}

PlanArc MakeArc (const Vector2d &start, const Vector2d &end, const Vector2d &center, bool ccw)
{
	const Vector2d a = sub(start, center);
	const Vector2d b = sub(end, center);
	PlanArc arc;
	arc.center      = center;
	arc.radius      = norm(a);
	arc.start_angle = std::atan2(a.y, a.x);
	const double diff = std::atan2(b.y, b.x) - arc.start_angle;
	if (ccw)
	{
		arc.sweep = wrapTwoPi(diff);
		if (arc.sweep == 0.0)
			arc.sweep = kTwoPi;
	}
	else
	{
		arc.sweep = -wrapTwoPi(-diff);
		if (arc.sweep == 0.0)
			arc.sweep = -kTwoPi;
	}
	return arc;
}

// ===========================================================================
//  Symetries
// ===========================================================================

namespace
{
	int clampedOrder (const SymmetrySpec &sym)
	{
		return std::clamp(sym.rotation_order, 1, SpecLimits::max_rotation_order);
	}

	// Angle de rotation de R^k, ou angle de l'axe de R^k Q, en radians.
	double elementAngle (const SymmetrySpec &sym, SymmetryElement g)
	{
		const int n = clampedOrder(sym);
		const int k = reduce(g.k, n);
		if (!g.reflected)
			return kTwoPi * k / n;
		return sym.reflection_axis_deg.value_or(0.0) * kPi / 180.0 + kPi * k / n;
	}

	// Partie lineaire d'un element : p' = center + M (p - center). Calculee une
	// fois par element ; l'identite est exacte.
	struct ElementMap
	{
		SymmetryElement g;
		Vector2d        center;
		double          angle    = 0.0;   // elementAngle
		bool            identity = true;
		double          m00 = 1.0, m01 = 0.0, m10 = 0.0, m11 = 1.0;

		Vector2d operator() (const Vector2d &p) const
		{
			if (identity)
				return p;
			const double vx = p.x - center.x, vy = p.y - center.y;
			return Vector2d(center.x + m00 * vx + m01 * vy, center.y + m10 * vx + m11 * vy);
		}
	};

	ElementMap makeMap (const SymmetrySpec &sym, SymmetryElement g)
	{
		ElementMap m;
		m.g      = g;
		m.center = sym.center;
		m.angle  = elementAngle(sym, g);
		if (!g.reflected)
		{
			if (m.angle == 0.0)
				return m;
			const double c = std::cos(m.angle), s = std::sin(m.angle);
			m.identity = false;
			m.m00 = c; m.m01 = -s; m.m10 = s; m.m11 = c;
			return m;
		}
		// Reflexion d'axe d'angle phi : matrice (cos 2phi, sin 2phi ; sin 2phi, -cos 2phi).
		const double c = std::cos(2.0 * m.angle), s = std::sin(2.0 * m.angle);
		m.identity = false;
		m.m00 = c; m.m01 = s; m.m10 = s; m.m11 = -c;
		return m;
	}

	PlanCurve applyMap (const ElementMap &m, const PlanCurve &curve)
	{
		if (const auto *seg = std::get_if<PlanSegment>(&curve))
			return PlanSegment{m(seg->start), m(seg->end)};
		const PlanArc &arc = std::get<PlanArc>(curve);
		PlanArc out;
		out.center = m(arc.center);
		out.radius = arc.radius;
		if (!m.g.reflected)
		{
			out.start_angle = arc.start_angle + m.angle;
			out.sweep       = arc.sweep;
		}
		else
		{
			// Une reflexion renverse le sens de rotation : le balayage change de signe.
			out.start_angle = 2.0 * m.angle - arc.start_angle;
			out.sweep       = -arc.sweep;
		}
		return out;
	}
}

std::vector<SymmetryElement> Elements (const SymmetrySpec &sym)
{
	std::vector<SymmetryElement> out;
	if (sym.rotation_order < 1 || sym.rotation_order > SpecLimits::max_rotation_order)
		return out;
	const int n = sym.rotation_order;
	out.reserve(static_cast<std::size_t>(sym.reflection_axis_deg ? 2 * n : n));
	for (int k = 0; k < n; ++k)
		out.push_back(SymmetryElement{k, false});
	if (sym.reflection_axis_deg)
		for (int k = 0; k < n; ++k)
			out.push_back(SymmetryElement{k, true});
	return out;
}

Vector2d Apply (const SymmetrySpec &sym, SymmetryElement g, const Vector2d &p)
{
	return makeMap(sym, g)(p);
}

PlanCurve Apply (const SymmetrySpec &sym, SymmetryElement g, const PlanCurve &curve)
{
	return applyMap(makeMap(sym, g), curve);
}

std::vector<Axis> MirrorAxes (const SymmetrySpec &sym)
{
	std::vector<Axis> axes;
	if (!sym.reflection_axis_deg || sym.rotation_order < 1 || sym.rotation_order > SpecLimits::max_rotation_order)
		return axes;
	const int n = sym.rotation_order;
	axes.reserve(static_cast<std::size_t>(n));
	for (int k = 0; k < n; ++k)
	{
		const double a = elementAngle(sym, SymmetryElement{k, true});
		axes.push_back(Axis{sym.center, Vector2d(std::cos(a), std::sin(a))});
	}
	return axes;
}

// ===========================================================================
//  Intersections du plan
// ===========================================================================

namespace
{
	bool usableTolerance (double tol) { return std::isfinite(tol) && tol >= 0.0; }

	bool usableCurve (const PlanCurve &curve)
	{
		if (const auto *seg = std::get_if<PlanSegment>(&curve))
			return finite(seg->start) && finite(seg->end) && distance(seg->start, seg->end) > 0.0;
		const PlanArc &arc = std::get<PlanArc>(curve);
		return finite(arc.center) && std::isfinite(arc.radius) && std::isfinite(arc.start_angle)
		    && std::isfinite(arc.sweep) && arc.radius > 0.0 && arc.sweep != 0.0 && std::fabs(arc.sweep) < kTwoPi;
	}

	// Ordre total sur les donnees d'une courbe : Intersect calcule toujours dans
	// le meme ordre deux courbes de meme type, d'ou un resultat independant de
	// l'ordre des arguments.
	std::array<double, 5> curveData (const PlanCurve &curve)
	{
		if (const auto *seg = std::get_if<PlanSegment>(&curve))
			return {seg->start.x, seg->start.y, seg->end.x, seg->end.y, 0.0};
		const PlanArc &arc = std::get<PlanArc>(curve);
		return {arc.center.x, arc.center.y, arc.radius, arc.start_angle, arc.sweep};
	}

	CurveOverlap swappedOverlap (const CurveOverlap &o)
	{
		if (o.s2_begin <= o.s2_end)
			return CurveOverlap{o.s2_begin, o.s2_end, o.s1_begin, o.s1_end};
		return CurveOverlap{o.s2_end, o.s2_begin, o.s1_end, o.s1_begin};
	}

	// Points communs tries par s1 et fusionnes a tol pres (un point tangent le
	// reste) ; portions communes triees par s1_begin.
	void normalizeHits (std::vector<CurveHit> &hits, double tol)
	{
		std::sort(hits.begin(), hits.end(), [](const CurveHit &a, const CurveHit &b) { return a.s1 < b.s1; });
		std::vector<CurveHit> kept;
		for (const CurveHit &h : hits)
		{
			auto same = std::find_if(kept.begin(), kept.end(),
			                         [&](const CurveHit &k) { return distance(k.xy, h.xy) <= tol; });
			if (same == kept.end())
				kept.push_back(h);
			else
				same->tangential = same->tangential || h.tangential;
		}
		hits = std::move(kept);
	}

	void normalize (Intersection &x, double tol)
	{
		normalizeHits(x.hits, tol);
		std::sort(x.overlaps.begin(), x.overlaps.end(),
		          [](const CurveOverlap &a, const CurveOverlap &b) { return a.s1_begin < b.s1_begin; });
	}

	Intersection swapped (Intersection x, double tol)
	{
		for (CurveHit &h : x.hits)
			std::swap(h.s1, h.s2);
		for (CurveOverlap &o : x.overlaps)
			o = swappedOverlap(o);
		normalize(x, tol);
		return x;
	}

	// Extremites de chaque courbe situees sur l'autre, a tol pres. `tangential` :
	// les supports sont tangents (ou paralleles) a tol pres.
	void endpointContacts (const PlanCurve &a, const PlanCurve &b, double tol, bool tangential, std::vector<CurveHit> &hits)
	{
		const double la = Length(a), lb = Length(b);
		for (const auto &[p, s] : {std::make_pair(StartPoint(a), 0.0), std::make_pair(EndPoint(a), la)})
			if (const auto s2 = ParameterOf(b, p, tol))
				hits.push_back(CurveHit{p, s, *s2, tangential});
		for (const auto &[p, s] : {std::make_pair(StartPoint(b), 0.0), std::make_pair(EndPoint(b), lb)})
			if (const auto s1 = ParameterOf(a, p, tol))
				hits.push_back(CurveHit{p, *s1, s, tangential});
	}

	// Segments de directions paralleles a tol pres. Colineaires si les deux
	// extremites du plus court sont a tol de la droite du plus long : portion
	// commune ou contact bout a bout, par projection sur le plus long. Sinon,
	// seuls des contacts aux extremites sont possibles.
	Intersection parallelSegments (const PlanSegment &a, const PlanSegment &b, double tol)
	{
		Intersection out;
		const double la = distance(a.start, a.end), lb = distance(b.start, b.end);
		const bool aLonger = la >= lb;
		const PlanSegment &lng = aLonger ? a : b;
		const PlanSegment &sht = aLonger ? b : a;
		const double ll = aLonger ? la : lb, ls = aLonger ? lb : la;
		const Vector2d u = mul(sub(lng.end, lng.start), 1.0 / ll);
		const Vector2d ps = sub(sht.start, lng.start), pe = sub(sht.end, lng.start);
		if (std::fabs(cross(u, ps)) > tol || std::fabs(cross(u, pe)) > tol)
		{
			endpointContacts(a, b, tol, true, out.hits);
			normalize(out, tol);
			return out;
		}
		// Abscisses, sur le plus long, des extremites du plus court.
		const double x0 = dot(ps, u), x1 = dot(pe, u);
		const double lo = std::max(0.0, std::min(x0, x1));
		const double hi = std::min(ll, std::max(x0, x1));
		auto shortAt = [&](double x) { return x1 == x0 ? 0.0 : std::clamp((x - x0) / (x1 - x0) * ls, 0.0, ls); };
		auto toAB = [&](double sLong, double sShort) {
			return aLonger ? std::make_pair(sLong, sShort) : std::make_pair(sShort, sLong);
		};
		if (hi - lo > tol)
		{
			const auto [s1lo, s2lo] = toAB(lo, shortAt(lo));
			const auto [s1hi, s2hi] = toAB(hi, shortAt(hi));
			out.overlaps.push_back(s1lo <= s1hi ? CurveOverlap{s1lo, s1hi, s2lo, s2hi}
			                                    : CurveOverlap{s1hi, s1lo, s2hi, s2lo});
		}
		else if (hi - lo >= -tol)
		{
			const double x = std::clamp(0.5 * (lo + hi), 0.0, ll);
			const auto [s1, s2] = toAB(x, shortAt(x));
			out.hits.push_back(CurveHit{PointAt(PlanCurve(lng), x), s1, s2, true});
		}
		return out;
	}

	Intersection intersectSegments (const PlanSegment &a, const PlanSegment &b, double tol)
	{
		const Vector2d r = sub(a.end, a.start), w = sub(b.end, b.start);
		const double lr = norm(r), lw = norm(w);
		const double den = cross(r, w);
		if (std::fabs(den) <= tol * std::max(lr, lw))
			return parallelSegments(a, b, tol);

		Intersection out;
		const Vector2d ac = sub(b.start, a.start);
		const double t = cross(ac, w) / den;
		const double v = cross(ac, r) / den;
		const double st = tol / lr, sv = tol / lw;
		if (t >= -st && t <= 1.0 + st && v >= -sv && v <= 1.0 + sv)
		{
			const double tc = std::clamp(t, 0.0, 1.0), vc = std::clamp(v, 0.0, 1.0);
			out.hits.push_back(CurveHit{add(a.start, mul(r, tc)), tc * lr, vc * lw, false});
			return out;
		}
		// Angle rasant : une extremite a tol de l'autre segment, alors que le
		// point d'intersection des droites est au-dela de la marge.
		endpointContacts(PlanCurve(a), PlanCurve(b), tol, false, out.hits);
		normalize(out, tol);
		return out;
	}

	// Droite (origin, u unitaire) et cercle (center, radius) : abscisses sur la
	// droite des points communs, et si le contact est tangent. Calcul par le pied
	// du centre sur la droite, sans equation du second degre.
	struct LineCircle
	{
		Vector2d foot;
		double   along = 0.0;   // abscisse du pied
		double   half  = 0.0;   // demi-corde ; 0 si tangent
		bool     any   = false;
		bool     tangent = false;
	};

	LineCircle lineCircle (const Vector2d &origin, const Vector2d &u, const Vector2d &center, double radius, double tol)
	{
		LineCircle lc;
		const Vector2d oc = sub(center, origin);
		lc.along = dot(oc, u);
		lc.foot  = add(origin, mul(u, lc.along));
		const double d = std::fabs(cross(u, oc));
		if (d > radius + tol)
			return lc;
		lc.any = true;
		if (std::fabs(d - radius) <= tol)
			lc.tangent = true;
		else
			lc.half = std::sqrt(std::max(0.0, (radius - d) * (radius + d)));
		return lc;
	}

	Intersection intersectSegmentArc (const PlanSegment &seg, const PlanCurve &arcCurve, double tol)
	{
		Intersection out;
		const PlanArc &arc = std::get<PlanArc>(arcCurve);
		const Vector2d r = sub(seg.end, seg.start);
		const double lr = norm(r);
		const Vector2d u = mul(r, 1.0 / lr);
		const LineCircle lc = lineCircle(seg.start, u, arc.center, arc.radius, tol);
		if (!lc.any)
			return out;
		auto consider = [&](double s) {
			if (s < -tol || s > lr + tol)
				return;
			const Vector2d p = add(lc.foot, mul(u, s - lc.along));
			if (const auto sa = ParameterOf(arcCurve, p, tol))
				out.hits.push_back(CurveHit{p, std::clamp(s, 0.0, lr), *sa, lc.tangent});
		};
		// Supports a tol de la tangence : le pied H est a ~sqrt(2 r tol) du point
		// commun reel. Une extremite commune a tol pres le remplace.
		endpointContacts(PlanCurve(seg), arcCurve, tol, lc.tangent, out.hits);
		if (!lc.tangent)
		{
			consider(lc.along - lc.half);
			consider(lc.along + lc.half);
		}
		else if (out.hits.empty())
			consider(lc.along);
		normalize(out, tol);
		return out;
	}

	// Arc vu comme l'intervalle angulaire [lo, lo + span] parcouru dans le sens
	// trigonometrique.
	struct CcwSpan
	{
		double lo   = 0.0;
		double span = 0.0;
	};

	CcwSpan ccwSpan (const PlanArc &arc)
	{
		return CcwSpan{arc.sweep >= 0.0 ? arc.start_angle : arc.start_angle + arc.sweep, std::fabs(arc.sweep)};
	}

	// Abscisse s du point d'angle lo + phi, phi dans [0, span].
	double arcParameterCcw (const PlanArc &arc, double phi)
	{
		const double span = std::fabs(arc.sweep);
		const double p = std::clamp(phi, 0.0, span);
		return arc.radius * (arc.sweep >= 0.0 ? p : span - p);
	}

	// Arcs de meme cercle : intersection des intervalles angulaires, aux trois
	// decalages de 2*pi qui peuvent les faire se rencontrer.
	Intersection coCircularArcs (const PlanArc &a, const PlanArc &b, double tol)
	{
		Intersection out;
		const double radius = 0.5 * (a.radius + b.radius);
		const double slack = tol / radius;
		const CcwSpan sa = ccwSpan(a), sb = ccwSpan(b);
		const double delta = wrapTwoPi(sb.lo - sa.lo);
		for (int k = -1; k <= 1; ++k)
		{
			const double b0 = delta + kTwoPi * k;
			const double lo = std::max(0.0, b0);
			const double hi = std::min(sa.span, b0 + sb.span);
			if (hi - lo > slack)
			{
				const double s1lo = arcParameterCcw(a, lo), s1hi = arcParameterCcw(a, hi);
				const double s2lo = arcParameterCcw(b, lo - b0), s2hi = arcParameterCcw(b, hi - b0);
				out.overlaps.push_back(s1lo <= s1hi ? CurveOverlap{s1lo, s1hi, s2lo, s2hi}
				                                    : CurveOverlap{s1hi, s1lo, s2hi, s2lo});
			}
			else if (hi - lo >= -slack)
			{
				const double phi = std::clamp(0.5 * (lo + hi), 0.0, sa.span);
				out.hits.push_back(CurveHit{onCircle(a.center, a.radius, sa.lo + phi),
				                            arcParameterCcw(a, phi), arcParameterCcw(b, phi - b0), true});
			}
		}
		normalize(out, tol);
		return out;
	}

	Intersection intersectArcs (const PlanCurve &ca, const PlanCurve &cb, double tol)
	{
		Intersection out;
		const PlanArc &a = std::get<PlanArc>(ca);
		const PlanArc &b = std::get<PlanArc>(cb);
		const Vector2d dc = sub(b.center, a.center);
		const double d = norm(dc);
		const double r1 = a.radius, r2 = b.radius;
		// Cercles presque confondus (M18) : centres et rayons egaux a 2 tol pres, donc
		// a 4 tol l'un de l'autre partout. Toute portion angulaire commune plus longue
		// que tol est un recouvrement ; des centres a tol pres donnent aussi les
		// contacts bout a bout.
		if (d <= 2.0 * tol && std::fabs(r1 - r2) <= 2.0 * tol)
		{
			Intersection co = coCircularArcs(a, b, tol);
			if (!co.overlaps.empty() || d <= tol)
				return co;
		}
		if (d <= tol)
			return out;
		if (d > r1 + r2 + tol || d < std::fabs(r1 - r2) - tol)
			return out;
		const Vector2d e = mul(dc, 1.0 / d);
		std::vector<Vector2d> candidates;
		const bool external = std::fabs(d - (r1 + r2)) <= tol;
		const bool internal = std::fabs(d - std::fabs(r1 - r2)) <= tol;
		if (external || internal)
		{
			// Milieu des points de contact de chaque cercle sur la ligne des centres.
			const double side1 = (external || r1 >= r2) ? 1.0 : -1.0;
			const double side2 = external ? -1.0 : side1;
			const Vector2d p1 = add(a.center, mul(e, side1 * r1));
			const Vector2d p2 = add(b.center, mul(e, side2 * r2));
			candidates.push_back(mul(add(p1, p2), 0.5));
		}
		else
		{
			const double along = 0.5 * (d + (r1 - r2) * (r1 + r2) / d);
			const double half  = std::sqrt(std::max(0.0, (r1 - along) * (r1 + along)));
			const Vector2d foot = add(a.center, mul(e, along));
			const Vector2d n(-e.y, e.x);
			candidates.push_back(sub(foot, mul(n, half)));
			candidates.push_back(add(foot, mul(n, half)));
		}
		// Comme pour segment/arc : pres de la tangence, une extremite commune
		// remplace le milieu des points de contact.
		const bool tangent = external || internal;
		endpointContacts(ca, cb, tol, tangent, out.hits);
		if (tangent && !out.hits.empty())
			candidates.clear();
		for (const Vector2d &p : candidates)
		{
			const auto s1 = ParameterOf(ca, p, tol);
			const auto s2 = ParameterOf(cb, p, tol);
			if (s1 && s2)
				out.hits.push_back(CurveHit{p, *s1, *s2, tangent});
		}
		normalize(out, tol);
		return out;
	}

	// Courbes deja ordonnees : segment avant arc ; meme type dans l'ordre de curveData.
	Intersection intersectOrdered (const PlanCurve &a, const PlanCurve &b, double tol)
	{
		const auto *sa = std::get_if<PlanSegment>(&a);
		const auto *sb = std::get_if<PlanSegment>(&b);
		if (sa && sb)
			return intersectSegments(*sa, *sb, tol);
		if (sa)
			return intersectSegmentArc(*sa, b, tol);
		return intersectArcs(a, b, tol);
	}
}

namespace
{
	// Abscisse a tol d'une extremite (decision M14) : exactement 0 ou L. Pres
	// d'une tangente laterale verticale, une erreur ds sur s en donne une de
	// sqrt(2 r ds) sur z ; au bout, PointAt et HeightAt sont exacts.
	void snapToEnd (double &s, bool &atEnd, double length, double tol)
	{
		const bool atStart = s <= tol, atFinish = s >= length - tol;
		atEnd = atStart || atFinish;
		if (atStart && atFinish)
			s = s <= 0.5 * length ? 0.0 : length;
		else if (atStart)
			s = 0.0;
		else if (atFinish)
			s = length;
	}
}

Intersection Intersect (const PlanCurve &a, const PlanCurve &b, double tol_point)
{
	if (!usableTolerance(tol_point) || !usableCurve(a) || !usableCurve(b))
		return Intersection{};
	const bool aSegment = std::holds_alternative<PlanSegment>(a);
	const bool bSegment = std::holds_alternative<PlanSegment>(b);
	const bool swap = aSegment != bSegment ? !aSegment : curveData(b) < curveData(a);
	Intersection x = swap ? swapped(intersectOrdered(b, a, tol_point), tol_point) : intersectOrdered(a, b, tol_point);
	const double la = Length(a), lb = Length(b);
	for (CurveHit &h : x.hits)
	{
		snapToEnd(h.s1, h.at_end1, la, tol_point);
		snapToEnd(h.s2, h.at_end2, lb, tol_point);
	}
	return x;
}

std::vector<CurveHit> IntersectAxis (const PlanCurve &curve, const Axis &axis, double tol_point)
{
	std::vector<CurveHit> out;
	const double en = norm(axis.direction);
	if (!usableTolerance(tol_point) || !usableCurve(curve) || !finite(axis.origin) || !(en > 0.0 && std::isfinite(en)))
		return out;
	const Vector2d e = mul(axis.direction, 1.0 / en);
	const Vector2d &o = axis.origin;

	if (const auto *seg = std::get_if<PlanSegment>(&curve))
	{
		const double ca = cross(e, sub(seg->start, o));
		const double cb = cross(e, sub(seg->end, o));
		const bool onStart = std::fabs(ca) <= tol_point, onEnd = std::fabs(cb) <= tol_point;
		const double L = distance(seg->start, seg->end);
		if (onStart && onEnd)
			return out;
		// Signes strictement opposes : la traversee, meme si une extremite est a
		// tol de l'axe (segment rasant).
		if (ca != 0.0 && cb != 0.0 && (ca < 0.0) != (cb < 0.0))
		{
			const double t = ca / (ca - cb);
			const Vector2d p = add(seg->start, mul(sub(seg->end, seg->start), t));
			out.push_back(CurveHit{p, t * L, dot(sub(p, o), e), false});
		}
		else if (onStart)
			out.push_back(CurveHit{seg->start, 0.0, dot(sub(seg->start, o), e), false});
		else if (onEnd)
			out.push_back(CurveHit{seg->end, L, dot(sub(seg->end, o), e), false});
		for (CurveHit &h : out)
			snapToEnd(h.s1, h.at_end1, L, tol_point);
		return out;
	}

	const PlanArc &arc = std::get<PlanArc>(curve);
	const LineCircle lc = lineCircle(o, e, arc.center, arc.radius, tol_point);
	if (!lc.any)
		return out;
	const double L = Length(curve);
	// Extremites de l'arc a tol de l'axe. Pres de la tangence, elles remplacent
	// le pied, qui est a ~sqrt(2 r tol) du point commun reel (meme regle que
	// Intersect pour segment/arc).
	for (const auto &[p, s] : {std::make_pair(StartPoint(curve), 0.0), std::make_pair(EndPoint(curve), L)})
		if (std::fabs(cross(e, sub(p, o))) <= tol_point)
			out.push_back(CurveHit{p, s, dot(sub(p, o), e), lc.tangent});
	auto consider = [&](double s2) {
		const Vector2d p = add(lc.foot, mul(e, s2 - lc.along));
		if (const auto s1 = ParameterOf(curve, p, tol_point))
			out.push_back(CurveHit{p, *s1, s2, lc.tangent});
	};
	if (!lc.tangent)
	{
		consider(lc.along - lc.half);
		consider(lc.along + lc.half);
	}
	else if (out.empty())
		consider(lc.along);
	normalizeHits(out, tol_point);
	for (CurveHit &h : out)
		snapToEnd(h.s1, h.at_end1, L, tol_point);
	return out;
}

// ===========================================================================
//  Profil lateral
// ===========================================================================

ProfileResult BuildOgeeProfile (double length, double z_start, double z_end, double s_prc, double tol_height)
{
	ProfileResult result;
	if (!(std::isfinite(tol_height) && tol_height >= 0.0))
	{
		result.status = ProfileStatus::InvalidTolerance;
		return result;
	}
	if (!(std::isfinite(length) && std::isfinite(z_start) && std::isfinite(z_end) && std::isfinite(s_prc)))
	{
		result.status = ProfileStatus::NonFiniteInput;
		return result;
	}
	if (!(length > 0.0))
	{
		result.status = ProfileStatus::NonPositiveLength;
		return result;
	}
	if (!(s_prc > 0.0 && s_prc < length))
	{
		result.status = ProfileStatus::PrcOutOfRange;
		return result;
	}
	const double delta = z_end - z_start;
	if (std::fabs(delta) <= tol_height)
	{
		result.status = ProfileStatus::Flat;
		return result;
	}
	// Rayons par k : la forme (s^2 + dz^2) / (2 s) cumule des arrondis qui
	// font passer r_start sous s_prc pour k = 1.
	const double k = std::fabs(delta) / length;
	const double f = 0.5 * (1.0 + k * k);
	OgeeProfile p;
	p.length  = length;
	p.z_start = z_start;
	p.z_end   = z_end;
	p.s_prc   = s_prc;
	p.z_prc   = z_start + delta * (s_prc / length);
	p.r_start = s_prc * f;
	p.r_end   = (length - s_prc) * f;
	// Sur les entrees : un critere sur les rayons derives depend de leurs arrondis.
	p.monotonic = std::fabs(delta) >= length;
	if (!(std::isfinite(delta) && std::isfinite(p.r_start) && std::isfinite(p.r_end)))
	{
		result.status = ProfileStatus::NonFiniteInput;
		return result;
	}
	result.profile = p;
	return result;
}

namespace
{
	// Arc du profil qui porte l'abscisse s (rabattue dans [0, length]) :
	// distance u au bout de l'arc, rayon r. Le point est a la hauteur
	// z_bout + / - sqrt(u (2r - u)), forme sans annulation pres du bout.
	struct ProfilePiece
	{
		bool   first = true;
		double u     = 0.0;
		double r     = 0.0;
	};

	ProfilePiece pieceAt (const OgeeProfile &p, double s)
	{
		const double sc = std::clamp(s, 0.0, p.length);
		if (sc <= p.s_prc)
			return ProfilePiece{true, sc, p.r_start};
		return ProfilePiece{false, p.length - sc, p.r_end};
	}

	double rise (const ProfilePiece &piece)
	{
		return std::sqrt(std::max(0.0, piece.u * (2.0 * piece.r - piece.u)));
	}

	double profileSign (const OgeeProfile &p) { return p.z_end < p.z_start ? -1.0 : 1.0; }
}

double HeightAt (const OgeeProfile &profile, double s)
{
	if (std::isnan(s))
		return s;
	if (!(profile.length > 0.0))
		return profile.z_start;
	const ProfilePiece piece = pieceAt(profile, s);
	const double sigma = profileSign(profile);
	return piece.first ? profile.z_start + sigma * rise(piece) : profile.z_end - sigma * rise(piece);
}

Vector2d SideTangentAt (const OgeeProfile &profile, double s)
{
	if (std::isnan(s))
		return Vector2d(s, s);
	if (!(profile.length > 0.0))
		return Vector2d(0.0, 0.0);
	const ProfilePiece piece = pieceAt(profile, s);
	// Perpendiculaire au rayon : (rise, sigma (r - u)) / r sur les deux arcs.
	const double w = rise(piece);
	const double c = piece.r - piece.u;
	const double n = std::hypot(w, c);
	if (!(n > 0.0))
		return Vector2d(0.0, 0.0);
	return Vector2d(w / n, profileSign(profile) * c / n);
}

Vector2d StartCenter (const OgeeProfile &profile)
{
	return Vector2d(profile.r_start, profile.z_start);
}

Vector2d EndCenter (const OgeeProfile &profile)
{
	return Vector2d(profile.length - profile.r_end, profile.z_end);
}

// ===========================================================================
//  Diagnostics
// ===========================================================================

const char *MicroarchErrorName (MicroarchError kind)
{
	switch (kind)
	{
	case MicroarchError::JsonParse:               return "json_parse";
	case MicroarchError::JsonSchema:              return "json_schema";
	case MicroarchError::JsonType:                return "json_type";
	case MicroarchError::JsonValue:               return "json_value";
	case MicroarchError::JsonMissing:             return "json_missing";
	case MicroarchError::DuplicateId:             return "duplicate_id";
	case MicroarchError::UnknownReference:        return "unknown_reference";
	case MicroarchError::DegenerateCurve:         return "degenerate_curve";
	case MicroarchError::ArcEndpointMismatch:     return "arc_endpoint_mismatch";
	case MicroarchError::DegenerateScale:         return "degenerate_scale";
	case MicroarchError::UnsupportedMultiLevel:   return "unsupported_multi_level";
	case MicroarchError::SelfReversedReplica:     return "self_reversed_replica";
	case MicroarchError::RedundantBasicCurve:     return "redundant_basic_curve";
	case MicroarchError::PlanOverlap:             return "plan_overlap";
	case MicroarchError::ProfileFlat:             return "profile_flat";
	case MicroarchError::ProfileInvalid:          return "profile_invalid";
	case MicroarchError::PrcSelectionNoCandidate: return "prc_selection_no_candidate";
	case MicroarchError::SegmentEndpointMismatch: return "segment_endpoint_mismatch";
	case MicroarchError::NearlyFlatArc:           return "nearly_flat_arc";
	case MicroarchError::SampleLimitExceeded:     return "sample_limit_exceeded";
	case MicroarchError::TooManyReplicas:         return "too_many_replicas";
	case MicroarchError::TooManyContacts:         return "too_many_contacts";
	case MicroarchError::ExportWriteFailed:       return "export_write_failed";
	case MicroarchError::TubeInvalidParameter:    return "tube_invalid_parameter";
	case MicroarchError::TubeLimitExceeded:       return "tube_limit_exceeded";
	case MicroarchError::JsonTooLarge:            return "json_too_large";
	case MicroarchError::SolidInvalidParameter:   return "solid_invalid_parameter";
	case MicroarchError::SolidLimitExceeded:      return "solid_limit_exceeded";
	case MicroarchError::SolidGeneratorDefect:    return "solid_generator_defect";
	}
	return "unknown";
}

const char *MicroarchWarningName (MicroarchWarning kind)
{
	switch (kind)
	{
	case MicroarchWarning::FieldIgnored:           return "field_ignored";
	case MicroarchWarning::PrcMidpointFallback:    return "prc_midpoint_fallback";
	case MicroarchWarning::PrcFromOtherBasicCurve: return "prc_from_other_basic_curve";
	case MicroarchWarning::PrcTie:                 return "prc_tie";
	case MicroarchWarning::PrcOnFalseIntersection: return "prc_on_false_intersection";
	case MicroarchWarning::ProfileNotMonotonic:    return "profile_not_monotonic";
	case MicroarchWarning::TangentialContact:      return "tangential_contact";
	case MicroarchWarning::EndpointHeightConflict: return "endpoint_height_conflict";
	case MicroarchWarning::PrcNearEndpoint:        return "prc_near_endpoint";
	case MicroarchWarning::JunctionHeightSpread:   return "junction_height_spread";
	case MicroarchWarning::TubeRadiusHidesCrossing: return "tube_radius_hides_crossing";
	case MicroarchWarning::SolidUnplannedFusion:    return "solid_unplanned_fusion";
	case MicroarchWarning::SolidFloatingComponent:  return "solid_floating_component";
	case MicroarchWarning::SolidWallTooThin:        return "solid_wall_too_thin";
	case MicroarchWarning::SolidClosedCell:         return "solid_closed_cell";
	case MicroarchWarning::SolidForkNeck:           return "solid_fork_neck";
	case MicroarchWarning::SolidUnverified:         return "solid_unverified";
	case MicroarchWarning::SolidTriangleEstimateHigh: return "solid_triangle_estimate_high";
	case MicroarchWarning::SolidSealedCavity:       return "solid_sealed_cavity";
	case MicroarchWarning::SolidUnplannedSplit:     return "solid_unplanned_split";
	case MicroarchWarning::SolidDiscretizationUnchecked: return "solid_discretization_unchecked";
	}
	return "unknown";
}

namespace
{
	void appendEntry (std::vector<MicroarchDiagnostic> &entries, MicroarchDiagnostic d)
	{
		if (entries.size() < kMicroarchReportMaxEntries)
			entries.push_back(std::move(d));
	}
}

void MicroarchReport::Record (MicroarchError kind, std::string path, std::string message)
{
	++errorCounts[static_cast<std::size_t>(kind)];
	appendEntry(entries, MicroarchDiagnostic{true, static_cast<int>(kind), std::move(path), std::move(message)});
}

void MicroarchReport::Record (MicroarchWarning kind, std::string path, std::string message)
{
	++warningCounts[static_cast<std::size_t>(kind)];
	appendEntry(entries, MicroarchDiagnostic{false, static_cast<int>(kind), std::move(path), std::move(message)});
}

std::size_t MicroarchReport::ErrorCount () const
{
	std::size_t n = 0;
	for (std::size_t c : errorCounts)
		n += c;
	return n;
}

std::size_t MicroarchReport::WarningCount () const
{
	std::size_t n = 0;
	for (std::size_t c : warningCounts)
		n += c;
	return n;
}

void MicroarchReport::Merge (const MicroarchReport &other)
{
	for (std::size_t k = 0; k < errorCounts.size(); ++k)   errorCounts[k]   += other.errorCounts[k];
	for (std::size_t k = 0; k < warningCounts.size(); ++k) warningCounts[k] += other.warningCounts[k];
	for (const MicroarchDiagnostic &d : other.entries)
		appendEntry(entries, d);
}

// ===========================================================================
//  Specification : echelle et controles structurels
// ===========================================================================

double Scale (const MicroarchSpec &spec)
{
	double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0;
	bool any = false;
	auto take = [&](const Vector2d &p) {
		if (!finite(p))
			return;
		if (!any)
		{
			xmin = xmax = p.x;
			ymin = ymax = p.y;
			any = true;
			return;
		}
		xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
		ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
	};
	take(spec.symmetry.center);
	for (const PointSpec &p : spec.points)
		take(p.xy);
	return any ? std::hypot(xmax - xmin, ymax - ymin) : 0.0;
}

namespace detail
{

bool IsValidUtf8 (const std::string &text)
{
	const auto *s = reinterpret_cast<const unsigned char *>(text.data());
	const std::size_t n = text.size();
	std::size_t i = 0;
	while (i < n)
	{
		const unsigned char c = s[i];
		std::size_t len = 0;
		unsigned int cp = 0;
		if (c < 0x80)      { ++i; continue; }
		else if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1Fu; }
		else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0Fu; }
		else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07u; }
		else
			return false;
		if (i + len > n)
			return false;
		for (std::size_t k = 1; k < len; ++k)
		{
			if ((s[i + k] & 0xC0u) != 0x80u)
				return false;
			cp = (cp << 6) | (s[i + k] & 0x3Fu);
		}
		const bool overlong  = (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000);
		const bool surrogate = cp >= 0xD800 && cp <= 0xDFFF;
		if (overlong || surrogate || cp > 0x10FFFF)
			return false;
		i += len;
	}
	return true;
}

} // namespace detail

namespace
{
	// Contexte commun aux controles structurels d'une specification.
	struct StructureCheck
	{
		const MicroarchSpec &spec;
		MicroarchReport     &report;
		bool                 loaded;

		void Error (MicroarchError kind, const std::string &path, const std::string &message)
		{
			report.Record(kind, path, message);
		}

		// Id non vide, en UTF-8 valide, unique dans `seen`.
		void CheckId (const std::string &id, const std::string &path, const char *what, std::set<std::string> &seen)
		{
			if (id.empty())
			{
				if (!loaded)
					Error(MicroarchError::JsonValue, path, "empty id");
				return;
			}
			if (!detail::IsValidUtf8(id))
			{
				Error(MicroarchError::JsonValue, path, std::string("the ") + what + " id is not valid UTF-8");
				return;
			}
			if (!seen.insert(id).second)
				Error(MicroarchError::DuplicateId, path, std::string("duplicate ") + what + " id \"" + id + "\"");
		}
	};

	void checkSymmetry (StructureCheck &c)
	{
		const SymmetrySpec &sym = c.spec.symmetry;
		if (!finite(sym.center) && !c.loaded)
			c.Error(MicroarchError::JsonValue, "symmetry.center", "non-finite coordinates");
		if (sym.rotation_order < 1 || sym.rotation_order > SpecLimits::max_rotation_order)
			c.Error(MicroarchError::JsonValue, "symmetry.rotation_order",
			        std::to_string(sym.rotation_order) + " outside [1, " + std::to_string(SpecLimits::max_rotation_order) + "]");
		if (sym.reflection_axis_deg)
		{
			const double deg = *sym.reflection_axis_deg;
			if (!(std::isfinite(deg) && deg >= 0.0 && deg < 180.0))
				c.Error(MicroarchError::JsonValue, "symmetry.reflection_axis_deg", number(deg) + " outside [0, 180)");
		}
	}

	void checkTolerance (StructureCheck &c, const char *name, double value, double hi)
	{
		if (!(std::isfinite(value) && value > 0.0 && value <= hi))
			c.Error(MicroarchError::JsonValue, std::string("tolerances.") + name,
			        number(value) + " outside (0, " + number(hi) + "]");
	}

	void checkTolerances (StructureCheck &c)
	{
		const Tolerances &t = c.spec.tolerances;
		checkTolerance(c, "point",     t.point,     ToleranceBounds::point);
		checkTolerance(c, "parameter", t.parameter, ToleranceBounds::parameter);
		checkTolerance(c, "height",    t.height,    ToleranceBounds::height);
		checkTolerance(c, "prc_snap",  t.prc_snap,  ToleranceBounds::prc_snap);
	}

	// La v1 n'en genere qu'un. Rend false si la liste des niveaux n'est pas connue
	// (niveau illisible au chargement) : les references de niveau ne se controlent pas.
	bool checkLevels (StructureCheck &c)
	{
		const std::vector<LevelSpec> &levels = c.spec.levels;
		if (levels.empty() && !c.loaded)
			c.Error(MicroarchError::JsonValue, "levels", "at least one level is required");
		else if (levels.size() > 1)
			c.Error(MicroarchError::UnsupportedMultiLevel, "levels",
			        std::to_string(levels.size()) + " levels declared ; several levels are not supported");
		bool known = true;
		for (std::size_t i = 0; i < levels.size(); ++i)
		{
			const std::string path = indexed("levels", i);
			if (levels[i].id < 0)
			{
				known = known && !c.loaded;
				if (!c.loaded)
					c.Error(MicroarchError::JsonValue, path + ".id", std::to_string(levels[i].id) + " is negative");
			}
			if (!std::isfinite(levels[i].base) && !c.loaded)
				c.Error(MicroarchError::JsonValue, path + ".base", "non-finite base");
		}
		return known;
	}

	// Rend, pour chaque point, si ses coordonnees sont utilisables.
	std::vector<bool> checkPoints (StructureCheck &c)
	{
		const std::vector<PointSpec> &points = c.spec.points;
		std::vector<bool> usable(points.size(), true);
		if (points.empty() && !c.loaded)
			c.Error(MicroarchError::JsonValue, "points", "at least one point is required");
		if (points.size() > SpecLimits::max_points)
			c.Error(MicroarchError::JsonValue, "points",
			        std::to_string(points.size()) + " points ; at most " + std::to_string(SpecLimits::max_points));
		std::set<std::string> seen;
		for (std::size_t i = 0; i < points.size(); ++i)
		{
			const PointSpec &p = points[i];
			const std::string path = indexed("points", i);
			c.CheckId(p.id, path + ".id", "point", seen);
			if (!finite(p.xy))
			{
				usable[i] = false;
				if (!c.loaded)
					c.Error(MicroarchError::JsonValue, path + ".xy", "non-finite coordinates");
			}
			const bool heightReadable = p.height && std::isfinite(*p.height);
			if (p.height && !heightReadable && !c.loaded)
				c.Error(MicroarchError::JsonValue, path + ".height", "non-finite height");
			if (p.ground && heightReadable)
				c.Error(MicroarchError::JsonValue, path + ".height", "ground and height are exclusive");
			else if (!p.ground && !p.height)
				c.Error(MicroarchError::JsonMissing, path + ".height", "the point is neither on the ground nor given a height");
		}
		return usable;
	}

	// Rend l'echelle si elle est utilisable (finie et positive), 0 sinon.
	double checkScale (StructureCheck &c)
	{
		if (c.spec.points.empty())
			return 0.0;
		const double scale = Scale(c.spec);
		if (!(scale > 0.0 && std::isfinite(scale)))
		{
			c.Error(MicroarchError::DegenerateScale, "points",
			        std::isfinite(scale) ? "the points and the symmetry centre coincide : the drawing has no extent"
			                             : "the extent of the drawing overflows the doubles");
			return 0.0;
		}
		return scale;
	}

	void checkSegment (StructureCheck &c, const std::string &path, const PlanSegment &seg,
	                   const PointSpec &ps, const PointSpec &pe, double tolPoint)
	{
		if (distance(seg.start, ps.xy) > tolPoint)
			c.Error(MicroarchError::SegmentEndpointMismatch, path + ".start",
			        "the segment does not start at point \"" + ps.id + "\"");
		else if (distance(seg.end, pe.xy) > tolPoint)
			c.Error(MicroarchError::SegmentEndpointMismatch, path + ".end",
			        "the segment does not end at point \"" + pe.id + "\"");
	}

	void checkArc (StructureCheck &c, const std::string &path, const PlanCurve &curve,
	               const PointSpec &ps, const PointSpec &pe, double tolPoint, double scale)
	{
		const PlanArc &arc = std::get<PlanArc>(curve);
		const std::string centerPath = path + ".center";
		if (!finite(arc.center) || !std::isfinite(arc.radius) || !std::isfinite(arc.start_angle)
		    || !std::isfinite(arc.sweep))
		{
			c.Error(MicroarchError::DegenerateCurve, centerPath, "non-finite arc data");
			return;
		}
		if (arc.radius <= tolPoint)
		{
			c.Error(MicroarchError::DegenerateCurve, centerPath, "radius " + number(arc.radius) + " is not positive");
			return;
		}
		// Avant le controle des extremites : un arc presque plat les manque par
		// la seule imprecision de sa representation.
		if (arc.radius > SpecLimits::max_arc_radius_ratio * scale)
		{
			c.Error(MicroarchError::NearlyFlatArc, path,
			        "radius " + number(arc.radius) + " exceeds " + number(SpecLimits::max_arc_radius_ratio)
			        + " times the drawing scale " + number(scale) + " : the arc is nearly flat, use a segment");
			return;
		}
		const double startGap = distance(StartPoint(curve), ps.xy);
		const double endGap   = distance(EndPoint(curve), pe.xy);
		if (startGap > tolPoint || endGap > tolPoint)
		{
			c.Error(MicroarchError::ArcEndpointMismatch, centerPath,
			        "the arc misses its " + std::string(startGap > tolPoint ? "start" : "end")
			        + " point by " + number(std::max(startGap, endGap))
			        + " (|start - center| = " + number(distance(ps.xy, arc.center))
			        + ", |end - center| = " + number(distance(pe.xy, arc.center)) + ")");
			return;
		}
		if (!(std::fabs(arc.sweep) > 0.0 && std::fabs(arc.sweep) < kTwoPi))
			c.Error(MicroarchError::DegenerateCurve, centerPath, "sweep " + number(arc.sweep) + " outside (0, 2*pi)");
	}

	// Geometrie d'une courbe dont les deux points existent et sont finis.
	void checkCurveGeometry (StructureCheck &c, std::size_t i, double tolPoint, double scale)
	{
		const BasicCurveSpec &curve = c.spec.curves[i];
		const std::string path = indexed("curves", i);
		const PointSpec &ps = c.spec.points[static_cast<std::size_t>(curve.start_point)];
		const PointSpec &pe = c.spec.points[static_cast<std::size_t>(curve.end_point)];
		if (curve.start_point == curve.end_point || distance(ps.xy, pe.xy) <= tolPoint)
		{
			c.Error(MicroarchError::DegenerateCurve, path + ".end",
			        "start \"" + ps.id + "\" and end \"" + pe.id + "\" coincide");
			return;
		}
		if (const auto *seg = std::get_if<PlanSegment>(&curve.plan))
			checkSegment(c, path, *seg, ps, pe, tolPoint);
		else
			checkArc(c, path, curve.plan, ps, pe, tolPoint, scale);
	}

	void checkCurves (StructureCheck &c, const std::vector<bool> &usable, bool levelsKnown, double scale)
	{
		const MicroarchSpec &spec = c.spec;
		if (spec.curves.empty() && !c.loaded)
			c.Error(MicroarchError::JsonValue, "curves", "at least one curve is required");
		if (spec.curves.size() > SpecLimits::max_curves)
			c.Error(MicroarchError::JsonValue, "curves",
			        std::to_string(spec.curves.size()) + " curves ; at most " + std::to_string(SpecLimits::max_curves));
		const int order = spec.symmetry.rotation_order;
		if (order >= 1 && order <= SpecLimits::max_rotation_order)
		{
			const std::size_t elements = static_cast<std::size_t>(spec.symmetry.reflection_axis_deg ? 2 * order : order);
			if (spec.curves.size() * elements > SpecLimits::max_replicas)
				c.Error(MicroarchError::TooManyReplicas, "curves",
				        std::to_string(spec.curves.size()) + " curves x " + std::to_string(elements) + " symmetry elements = "
				        + std::to_string(spec.curves.size() * elements) + " replicas ; at most "
				        + std::to_string(SpecLimits::max_replicas));
		}

		std::set<int> levelIds;
		for (const LevelSpec &l : spec.levels)
			levelIds.insert(l.id);
		const double tolPoint = Resolve(spec.tolerances, scale).point;

		std::set<std::string> seen;
		for (std::size_t i = 0; i < spec.curves.size(); ++i)
		{
			const BasicCurveSpec &curve = spec.curves[i];
			const std::string path = indexed("curves", i);
			c.CheckId(curve.id, path + ".id", "curve", seen);

			// Au chargement, un niveau de courbe negatif est illisible et deja signale.
			const bool levelRead = !(c.loaded && curve.level < 0);
			if (levelsKnown && levelRead && levelIds.count(curve.level) == 0)
				c.Error(MicroarchError::UnknownReference, path + ".level",
				        "level " + std::to_string(curve.level) + " is not declared");

			if (curve.prc.u && !(std::isfinite(*curve.prc.u) && *curve.prc.u > 0.0 && *curve.prc.u < 1.0))
				c.Error(MicroarchError::JsonValue, path + ".prc.u", number(*curve.prc.u) + " outside (0, 1)");

			bool resolved = true;
			auto checkIndex = [&](int index, const char *field) {
				if (index >= 0 && static_cast<std::size_t>(index) < spec.points.size())
					return;
				resolved = false;
				if (!(c.loaded && index < 0))
					c.Error(MicroarchError::UnknownReference, path + "." + field,
					        "point index " + std::to_string(index) + " does not exist");
			};
			checkIndex(curve.start_point, "start");
			checkIndex(curve.end_point, "end");
			// Sans echelle, les controles geometriques n'ont pas de tolerance.
			if (!resolved || !(scale > 0.0))
				continue;
			if (!usable[static_cast<std::size_t>(curve.start_point)] || !usable[static_cast<std::size_t>(curve.end_point)])
				continue;
			checkCurveGeometry(c, i, tolPoint, scale);
		}
	}
}

namespace detail
{

bool ValidateStructure (const MicroarchSpec &spec, MicroarchReport &report, bool loaded)
{
	const std::size_t before = report.ErrorCount();
	StructureCheck c{spec, report, loaded};
	checkSymmetry(c);
	checkTolerances(c);
	const bool levelsKnown = checkLevels(c);
	const std::vector<bool> usable = checkPoints(c);
	const double scale = checkScale(c);
	checkCurves(c, usable, levelsKnown, scale);
	return report.ErrorCount() == before;
}

} // namespace detail

bool ValidateStructure (const MicroarchSpec &spec, MicroarchReport &report)
{
	return detail::ValidateStructure(spec, report, false);
}

// ===========================================================================
//  Developpement du plan
// ===========================================================================

namespace
{
	// Image d'une courbe de base reduite a ce qui la compare : type, extremites,
	// et cercle porteur pour un arc. Les extremites sont calculees par la
	// matrice de l'element, sans trigonometrie.
	struct CurveKey
	{
		bool     arc = false;
		Vector2d start, end;
		Vector2d center;
		double   radius = 0.0;
		double   sweep  = 0.0;
	};

	CurveKey keyOf (const PlanCurve &curve)
	{
		CurveKey k;
		k.start = StartPoint(curve);
		k.end   = EndPoint(curve);
		if (const auto *a = std::get_if<PlanArc>(&curve))
		{
			k.arc    = true;
			k.center = a->center;
			k.radius = a->radius;
			k.sweep  = a->sweep;
		}
		return k;
	}

	CurveKey imageKey (const ElementMap &m, const CurveKey &k)
	{
		CurveKey out = k;
		out.start = m(k.start);
		out.end   = m(k.end);
		if (k.arc)
		{
			out.center = m(k.center);
			out.sweep  = m.g.reflected ? -k.sweep : k.sweep;
		}
		return out;
	}

	bool sameCircle (const CurveKey &a, const CurveKey &b, double tol)
	{
		return distance(a.center, b.center) <= tol
		    && std::fabs(a.radius - b.radius) <= tol
		    && std::fabs(std::fabs(a.sweep) - std::fabs(b.sweep)) * a.radius <= tol;
	}

	// Meme courbe, meme sens de parcours, a tol pres.
	bool sameCurve (const CurveKey &a, const CurveKey &b, double tol)
	{
		if (a.arc != b.arc || distance(a.start, b.start) > tol || distance(a.end, b.end) > tol)
			return false;
		return !a.arc || (sameCircle(a, b, tol) && (a.sweep > 0.0) == (b.sweep > 0.0));
	}

	// Meme ensemble de points, parcouru en sens inverse.
	bool reversedCurve (const CurveKey &a, const CurveKey &b, double tol)
	{
		if (a.arc != b.arc || distance(a.start, b.end) > tol || distance(a.end, b.start) > tol)
			return false;
		return !a.arc || (sameCircle(a, b, tol) && (a.sweep > 0.0) != (b.sweep > 0.0));
	}

	// Index de points par abscisse : les points a tol pres d'une requete.
	class PointIndex
	{
	public:
		void Add (const Vector2d &p, std::size_t id) { m_items.push_back({p.x, p.y, id}); }
		void Build () { std::sort(m_items.begin(), m_items.end(), [](const Item &a, const Item &b) { return a.x < b.x; }); }

		template <class F>
		void Near (const Vector2d &q, double tol, F &&visit) const
		{
			auto it = std::lower_bound(m_items.begin(), m_items.end(), q.x - tol,
			                           [](const Item &a, double x) { return a.x < x; });
			for (; it != m_items.end() && it->x <= q.x + tol; ++it)
				if (std::hypot(it->x - q.x, it->y - q.y) <= tol)
					visit(it->id);
		}

	private:
		struct Item { double x, y; std::size_t id; };
		std::vector<Item> m_items;
	};

	// Donnees du developpement, calculees une fois.
	struct Development
	{
		const MicroarchSpec       &spec;
		const ResolvedTolerances  &tol;
		std::vector<ElementMap>    maps;     // un par element, identite en tete
		std::vector<CurveKey>      basics;   // une par courbe de base
	};

	Development prepare (const MicroarchSpec &spec, const ResolvedTolerances &tol)
	{
		Development d{spec, tol, {}, {}};
		for (SymmetryElement g : Elements(spec.symmetry))
			d.maps.push_back(makeMap(spec.symmetry, g));
		d.basics.reserve(spec.curves.size());
		for (const BasicCurveSpec &c : spec.curves)
			d.basics.push_back(keyOf(c.plan));
		return d;
	}

	// Une hauteur h(s) avec h(s) = h(L - s) contredit z_debut != z_fin.
	void checkSelfReversed (const Development &d, MicroarchReport &report)
	{
		for (std::size_t i = 0; i < d.basics.size(); ++i)
			for (const ElementMap &m : d.maps)
				if (reversedCurve(imageKey(m, d.basics[i]), d.basics[i], d.tol.point))
				{
					report.Record(MicroarchError::SelfReversedReplica, indexed("curves", i),
					              "the symmetry " + elementText(m.g) + " maps curve \"" + d.spec.curves[i].id
					              + "\" onto itself in reverse");
					break;
				}
	}

	// Les courbes de base sont deux a deux non deductibles par symetrie. La
	// courbe j est signalee une fois, avec la plus petite courbe i < j dont elle
	// est l'image et le premier element qui l'y envoie.
	void checkRedundant (const Development &d, MicroarchReport &report)
	{
		const std::size_t nb = d.basics.size();
		PointIndex starts, ends;
		for (std::size_t j = 0; j < nb; ++j)
		{
			starts.Add(d.basics[j].start, j);
			ends.Add(d.basics[j].end, j);
		}
		starts.Build();
		ends.Build();

		std::map<std::size_t, std::pair<std::size_t, SymmetryElement>> found;   // j -> (i, element)
		for (std::size_t i = 0; i < nb; ++i)
			for (const ElementMap &m : d.maps)
			{
				const CurveKey img = imageKey(m, d.basics[i]);
				auto consider = [&](std::size_t j, bool reversed) {
					if (j <= i || found.count(j))
						return;
					const bool match = reversed ? reversedCurve(img, d.basics[j], d.tol.point)
					                            : sameCurve(img, d.basics[j], d.tol.point);
					if (match)
						found.emplace(j, std::make_pair(i, m.g));
				};
				starts.Near(img.start, d.tol.point, [&](std::size_t j) { consider(j, false); });
				ends.Near(img.start, d.tol.point, [&](std::size_t j) { consider(j, true); });
			}
		for (const auto &[j, origin] : found)
			report.Record(MicroarchError::RedundantBasicCurve, indexed("curves", j),
			              "curve \"" + d.spec.curves[j].id + "\" is the image of curve \"" + d.spec.curves[origin.first].id
			              + "\" by the symmetry " + elementText(origin.second));
	}

	// Une extremite envoyee par symetrie sur une autre extremite doit en avoir la
	// hauteur. Paire (a, b), a < b : signalee une fois sur b, avec le premier
	// element qui envoie a sur b.
	void checkEndpointHeights (const Development &d, MicroarchReport &report)
	{
		const MicroarchSpec &spec = d.spec;
		std::vector<std::size_t> ends;
		for (const BasicCurveSpec &c : spec.curves)
			for (int p : {c.start_point, c.end_point})
				ends.push_back(static_cast<std::size_t>(p));
		std::sort(ends.begin(), ends.end());
		ends.erase(std::unique(ends.begin(), ends.end()), ends.end());

		PointIndex index;
		for (std::size_t b = 0; b < ends.size(); ++b)
			index.Add(spec.points[ends[b]].xy, b);
		index.Build();

		std::map<std::pair<std::size_t, std::size_t>, SymmetryElement> found;   // (b, a) -> element
		for (std::size_t a = 0; a < ends.size(); ++a)
		{
			const PointSpec &pa = spec.points[ends[a]];
			for (std::size_t e = 1; e < d.maps.size(); ++e)
				index.Near(d.maps[e](pa.xy), d.tol.point, [&](std::size_t b) {
					if (b <= a || std::fabs(pa.Z() - spec.points[ends[b]].Z()) <= d.tol.height)
						return;
					found.emplace(std::make_pair(b, a), d.maps[e].g);
				});
		}
		for (const auto &[ba, g] : found)
		{
			const PointSpec &pa = spec.points[ends[ba.second]];
			const PointSpec &pb = spec.points[ends[ba.first]];
			report.Record(MicroarchWarning::EndpointHeightConflict, indexed("points", ends[ba.first]),
			              "the symmetry " + elementText(g) + " maps point \"" + pa.id
			              + "\" (z = " + number(pa.Z()) + ") onto point \"" + pb.id
			              + "\" (z = " + number(pb.Z()) + ")");
		}
	}

	// Repliques : une image deja retenue pour la meme courbe de base appartient
	// au stabilisateur et n'est pas dupliquee.
	std::vector<PlanReplica> replicas (const Development &d)
	{
		std::vector<PlanReplica> out;
		out.reserve(d.basics.size() * d.maps.size());
		std::vector<CurveKey> kept;
		for (std::size_t i = 0; i < d.basics.size(); ++i)
		{
			kept.clear();
			for (const ElementMap &m : d.maps)
			{
				const CurveKey img = imageKey(m, d.basics[i]);
				const bool seen = std::any_of(kept.begin(), kept.end(),
				                              [&](const CurveKey &k) { return sameCurve(k, img, d.tol.point); });
				if (seen)
					continue;
				kept.push_back(img);
				out.push_back(PlanReplica{static_cast<int>(i), m.g, d.spec.curves[i].level,
				                          applyMap(m, d.spec.curves[i].plan)});
			}
		}
		return out;
	}
}

bool Validate (const MicroarchSpec &spec, MicroarchReport &report)
{
	return DevelopPlan(spec, report).has_value();
}

std::optional<DevelopedPlan> DevelopPlan (const MicroarchSpec &spec, MicroarchReport &report)
{
	if (!detail::ValidateStructure(spec, report, false))
		return std::nullopt;
	const std::size_t before = report.ErrorCount();
	DevelopedPlan plan;
	plan.tol = Resolve(spec.tolerances, Scale(spec));
	const Development d = prepare(spec, plan.tol);
	checkSelfReversed(d, report);
	checkRedundant(d, report);
	checkEndpointHeights(d, report);
	if (report.ErrorCount() != before)
		return std::nullopt;
	plan.curves = replicas(d);
	return plan;
}

// ===========================================================================
//  PRC (etape 2)
// ===========================================================================

namespace
{
	// Force d'un palier pour la fusion de candidats de meme s : point fixe, autre
	// courbe de base, croisement de replique, milieu.
	int tierStrength (PrcRule tier)
	{
		switch (tier)
		{
		case PrcRule::ReplicaFixedPoint: return 0;
		case PrcRule::OtherBasicCurve:   return 1;
		case PrcRule::ReplicaCrossing:   return 2;
		case PrcRule::Midpoint:          return 3;
		case PrcRule::Explicit:          return 4;
		}
		return 4;
	}

	const char *tierName (PrcRule tier)
	{
		switch (tier)
		{
		case PrcRule::ReplicaFixedPoint: return "replica fixed point";
		case PrcRule::OtherBasicCurve:   return "other basic curve";
		case PrcRule::Midpoint:          return "midpoint";
		case PrcRule::Explicit:          return "explicit";
		case PrcRule::ReplicaCrossing:   return "replica crossing";
		}
		return "unknown";
	}

	// Ajoute un candidat d'abscisse interieure (A-3) ; un candidat a tol.parameter
	// d'un candidat existant le remplace s'il est d'un palier plus fort.
	void addCandidate (std::vector<PrcCandidate> &out, const PrcCandidate &c, double length,
	                   const ResolvedTolerances &tol)
	{
		if (!(c.s > tol.point && c.s < length - tol.point))
			return;
		for (PrcCandidate &e : out)
			if (std::fabs(e.s - c.s) <= tol.parameter)
			{
				if (tierStrength(c.tier) < tierStrength(e.tier))
					e = c;
				return;
			}
		out.push_back(c);
	}

	// Tous les candidats d'une courbe de base, tries par s.
	std::vector<PrcCandidate> prcCandidates (const MicroarchSpec &spec, const DevelopedPlan &plan, std::size_t i)
	{
		const ResolvedTolerances &tol = plan.tol;
		const BasicCurveSpec &basic = spec.curves[i];
		const PlanCurve &l = basic.plan;
		const double length = Length(l);
		std::vector<PrcCandidate> out;

		// Palier 1 : points fixes du groupe (§6.2) ; un axe qui porte la courbe ne
		// compte pas. Un point est a tol.point de son reflet si et seulement si il
		// est a tol.point / 2 de l'axe : c'est la tolerance de l'intersection avec
		// l'axe, la meme que celle d'Intersect(l, Q(l)).
		for (const Axis &axis : MirrorAxes(spec.symmetry))
			for (const CurveHit &h : IntersectAxis(l, axis, 0.5 * tol.point))
				addCandidate(out, PrcCandidate{h.s1, h.xy, PrcRule::ReplicaFixedPoint, h.tangential}, length, tol);
		// Centre : |R^k x - x| = 2 |x - p| sin(pi k / n), minimal pour k = 1.
		if (spec.symmetry.rotation_order > 1)
			if (const auto s = ParameterOf(l, spec.symmetry.center,
			                               tol.point / (2.0 * std::sin(kPi / spec.symmetry.rotation_order))))
				addCandidate(out, PrcCandidate{*s, spec.symmetry.center, PrcRule::ReplicaFixedPoint, false}, length, tol);

		for (const PlanReplica &m : plan.curves)
		{
			if (m.level != basic.level)
				continue;
			const bool own = m.basic == static_cast<int>(i);
			if (own && m.element.k == 0 && !m.element.reflected)
				continue;
			for (const CurveHit &h : Intersect(l, m.plan, tol.point).hits)
			{
				// Palier 2 : autre courbe de base, sans filtre s = s' (A9).
				if (!own)
					addCandidate(out, PrcCandidate{h.s1, h.xy, PrcRule::OtherBasicCurve, h.tangential}, length, tol);
				// Replique de la courbe : fausse intersection si le point n'est pas fixe par
				// l'element (seuil complementaire du palier 1), selectionnable seulement.
				else if (distance(PointAt(l, h.s1), Apply(spec.symmetry, m.element, PointAt(l, h.s1))) > tol.point)
					addCandidate(out, PrcCandidate{h.s1, h.xy, PrcRule::ReplicaCrossing, h.tangential}, length, tol);
			}
		}
		addCandidate(out, PrcCandidate{0.5 * length, PointAt(l, 0.5 * length), PrcRule::Midpoint, false}, length, tol);
		std::sort(out.begin(), out.end(), [](const PrcCandidate &a, const PrcCandidate &b) { return a.s < b.s; });
		return out;
	}

	std::string uList (const std::vector<PrcCandidate> &candidates, double length)
	{
		std::ostringstream os;
		os.precision(6);
		for (std::size_t k = 0; k < candidates.size(); ++k)
			os << (k ? ", " : "") << candidates[k].s / length;
		return os.str();
	}

	// Algorithme 1 : premier palier non vide, candidat le plus proche du milieu (M5, A-2).
	PrcResult automaticPrc (const BasicCurveSpec &basic, std::size_t i, std::vector<PrcCandidate> candidates,
	                        const ResolvedTolerances &tol, MicroarchReport &report)
	{
		const double length = Length(basic.plan), mid = 0.5 * length;
		const std::string path = indexed("curves", i);
		PrcResult result;
		for (PrcRule tier : {PrcRule::ReplicaFixedPoint, PrcRule::OtherBasicCurve})
		{
			std::vector<const PrcCandidate *> pool;
			for (const PrcCandidate &c : candidates)
				if (c.tier == tier)
					pool.push_back(&c);
			if (pool.empty())
				continue;
			double best = std::numeric_limits<double>::infinity();
			for (const PrcCandidate *c : pool)
				best = std::min(best, std::fabs(c->s - mid));
			std::vector<const PrcCandidate *> tied;
			for (const PrcCandidate *c : pool)
				if (std::fabs(c->s - mid) <= best + tol.parameter)
					tied.push_back(c);
			// Pool trie par s : le premier des ex aequo est le plus petit s.
			result.s    = tied.front()->s;
			result.rule = tier;
			if (tied.size() > 1)
				report.Record(MicroarchWarning::PrcTie, path,
				              "curve \"" + basic.id + "\" : " + std::to_string(tied.size())
				              + " PRC candidates equally close to the middle ; the smallest s = " + number(result.s) + " is kept");
			const double u = result.s / length;
			if (std::min(u, 1.0 - u) < PrcLimits::near_endpoint_u)
				report.Record(MicroarchWarning::PrcNearEndpoint, path,
				              "curve \"" + basic.id + "\" : PRC at u = " + number(u) + ", closer than "
				              + number(PrcLimits::near_endpoint_u) + " to an end ; one profile arc is very short");
			if (tier == PrcRule::OtherBasicCurve)
				report.Record(MicroarchWarning::PrcFromOtherBasicCurve, path,
				              "curve \"" + basic.id + "\" : no true intersection with its replicas ; PRC at s = "
				              + number(result.s) + " on another basic curve");
			result.candidates = std::move(candidates);
			return result;
		}
		result.s    = mid;
		result.rule = PrcRule::Midpoint;
		report.Record(MicroarchWarning::PrcMidpointFallback, path,
		              "curve \"" + basic.id + "\" : no PRC candidate ; PRC at the middle, s = " + number(mid));
		result.candidates = std::move(candidates);
		return result;
	}

	// Selection explicite (M5) : candidat le plus proche de u parmi tous.
	std::optional<PrcResult> explicitPrc (const BasicCurveSpec &basic, std::size_t i, std::vector<PrcCandidate> candidates,
	                                      const ResolvedTolerances &tol, MicroarchReport &report)
	{
		const double length = Length(basic.plan), u = *basic.prc.u;
		const std::string path = indexed("curves", i) + ".prc.u";
		const PrcCandidate *best = nullptr;
		for (const PrcCandidate &c : candidates)
			if (!best || std::fabs(c.s / length - u) < std::fabs(best->s / length - u))
				best = &c;
		if (!best || std::fabs(best->s / length - u) > tol.prc_snap)
		{
			report.Record(MicroarchError::PrcSelectionNoCandidate, path,
			              "curve \"" + basic.id + "\" : no PRC candidate within " + number(tol.prc_snap) + " of u = "
			              + number(u) + " ; available u : " + uList(candidates, length));
			return std::nullopt;
		}
		const std::string what = "curve \"" + basic.id + "\" : the PRC selected at u = " + number(best->s / length)
		                         + " is a " + tierName(best->tier);
		if (best->tier == PrcRule::ReplicaCrossing)
			report.Record(MicroarchWarning::PrcOnFalseIntersection, path, what + " (s != s', not a true intersection)");
		else if (best->tier == PrcRule::OtherBasicCurve)
			report.Record(MicroarchWarning::PrcFromOtherBasicCurve, path, what);
		else if (best->tier == PrcRule::Midpoint)
			report.Record(MicroarchWarning::PrcMidpointFallback, path, what + " (not an intersection)");
		PrcResult result;
		result.s          = best->s;
		result.rule       = PrcRule::Explicit;
		result.candidates = std::move(candidates);
		return result;
	}
}

std::optional<std::vector<PrcResult>> ComputePrcs (const MicroarchSpec &spec, const DevelopedPlan &plan, MicroarchReport &report)
{
	const std::size_t before = report.ErrorCount();
	std::vector<PrcResult> out;
	out.reserve(spec.curves.size());
	for (std::size_t i = 0; i < spec.curves.size(); ++i)
	{
		const BasicCurveSpec &basic = spec.curves[i];
		std::vector<PrcCandidate> candidates = prcCandidates(spec, plan, i);
		if (basic.prc.u)
		{
			if (auto r = explicitPrc(basic, i, std::move(candidates), plan.tol, report))
				out.push_back(std::move(*r));
		}
		else
			out.push_back(automaticPrc(basic, i, std::move(candidates), plan.tol, report));
	}
	if (report.ErrorCount() != before)
		return std::nullopt;
	return out;
}

// ===========================================================================
//  Courbe 3D (etape 4)
// ===========================================================================

double LevelBase (const MicroarchSpec &spec, int level)
{
	for (const LevelSpec &l : spec.levels)
		if (l.id == level)
			return l.base;
	return 0.0;
}

namespace
{
	const char *profileStatusText (ProfileStatus status)
	{
		switch (status)
		{
		case ProfileStatus::Ok:                return "ok";
		case ProfileStatus::NonFiniteInput:    return "non-finite input";
		case ProfileStatus::NonPositiveLength: return "non-positive length";
		case ProfileStatus::PrcOutOfRange:     return "PRC outside the curve";
		case ProfileStatus::Flat:              return "flat";
		case ProfileStatus::InvalidTolerance:  return "invalid height tolerance";
		}
		return "unknown";
	}
}

std::optional<std::vector<BasicCurveResult>> BuildBasicCurves (const MicroarchSpec &spec, const DevelopedPlan &plan,
                                                               MicroarchReport &report)
{
	std::optional<std::vector<PrcResult>> prcs = ComputePrcs(spec, plan, report);
	if (!prcs)
		return std::nullopt;
	const std::size_t before = report.ErrorCount();
	std::vector<BasicCurveResult> out;
	out.reserve(spec.curves.size());
	for (std::size_t i = 0; i < spec.curves.size(); ++i)
	{
		const BasicCurveSpec &basic = spec.curves[i];
		const std::string path = indexed("curves", i);
		const double base = LevelBase(spec, basic.level);
		const double zStart = base + spec.points[static_cast<std::size_t>(basic.start_point)].Z();
		const double zEnd   = base + spec.points[static_cast<std::size_t>(basic.end_point)].Z();
		const double length = Length(basic.plan);
		const ProfileResult profile = BuildOgeeProfile(length, zStart, zEnd, (*prcs)[i].s, plan.tol.height);
		if (profile.status == ProfileStatus::Flat)
		{
			report.Record(MicroarchError::ProfileFlat, path,
			              "curve \"" + basic.id + "\" : start and end heights are equal (z = " + number(zStart) + ")");
			continue;
		}
		if (profile.status != ProfileStatus::Ok)
		{
			report.Record(MicroarchError::ProfileInvalid, path,
			              "curve \"" + basic.id + "\" : side profile refused (" + profileStatusText(profile.status) + ")");
			continue;
		}
		if (!profile.profile.monotonic)
			report.Record(MicroarchWarning::ProfileNotMonotonic, path,
			              "curve \"" + basic.id + "\" : |z_end - z_start| = " + number(std::fabs(zEnd - zStart))
			              + " < length " + number(length) + " ; the height is not monotonic");
		out.push_back(BasicCurveResult{std::move((*prcs)[i]), profile.profile});
	}
	if (report.ErrorCount() != before)
		return std::nullopt;
	return out;
}

std::vector<SpaceCurve> BuildSpaceCurves (const DevelopedPlan &plan, const std::vector<BasicCurveResult> &basics)
{
	std::vector<SpaceCurve> out;
	out.reserve(plan.curves.size());
	for (const PlanReplica &r : plan.curves)
	{
		if (r.basic < 0 || static_cast<std::size_t>(r.basic) >= basics.size())
			return {};
		out.push_back(SpaceCurve{r.basic, r.element, r.level, r.plan, basics[static_cast<std::size_t>(r.basic)].profile});
	}
	return out;
}

Vector3d PointAt (const SpaceCurve &curve, double s)
{
	const Vector2d xy = PointAt(curve.plan, s);
	return Vector3d(xy.x, xy.y, HeightAt(curve.profile, s));
}

namespace
{
	// Nombre de pas d'un arc du profil de rayon r, parcouru en phi sur
	// [phiLow, phiLow + sweep] avec phiLow + sweep = pi. Un pas dphi deplace s de
	// r |sin phi| dphi : sur un arc de plan de rayon R, l'angle balaye en plan par
	// pas est au plus r sweep max|sin phi| / (n R). Le maximum de |sin| vaut 1 si
	// la plage contient pi/2, sinon |sin phiLow|.
	int stepCount (double sweep, double r, double phiLow, double planRadius, double maxAngle)
	{
		double need = sweep;
		if (planRadius > 0.0)
		{
			const double maxSin = phiLow <= 0.5 * kPi ? 1.0 : std::sin(phiLow);
			need = std::max(need, r * sweep * maxSin / planRadius);
		}
		const double n = std::ceil(need / maxAngle);
		if (!(n >= 1.0))
			return 1;
		return static_cast<int>(std::min<double>(n, SamplingLimits::max_steps));
	}
}

namespace
{
	// Abscisses d'echantillonnage d'une courbe, strictement croissantes, passees
	// une a une a `emit` ; s'arrete si `emit` rend false. Partage entre
	// SampleParameters et le comptage de SampleNetwork.
	template <class Emit>
	void forEachSample (const SpaceCurve &curve, const SamplingParams &params, Emit &&emit)
	{
		const OgeeProfile &p = curve.profile;
		const double L = p.length, sc = p.s_prc;
		if (!(L > 0.0 && sc > 0.0 && sc < L && p.r_start > 0.0 && p.r_end > 0.0))
			return;
		const bool sampled = std::isfinite(params.max_angle) && params.max_angle > 0.0;
		const auto *arc = std::get_if<PlanArc>(&curve.plan);
		const double planRadius = arc ? arc->radius : 0.0;

		bool any = false, stop = false;
		double last = 0.0;
		auto push = [&](double s) {
			if (stop || (any && !(s > last)))
				return;
			any  = true;
			last = s;
			stop = !emit(s);
		};
		push(0.0);
		// Arc 1 : centre (r_A, z_A) ; s = r_A (1 + cos phi), phi de pi a phi_C.
		if (sampled)
		{
			const double ra = p.r_start;
			const double phiC = std::acos(std::clamp((sc - ra) / ra, -1.0, 1.0));
			const double sweep = kPi - phiC;
			const int n = stepCount(sweep, ra, phiC, planRadius, params.max_angle);
			for (int j = 1; j < n; ++j)
				push(ra * (1.0 + std::cos(kPi - sweep * j / n)));
		}
		push(sc);
		// Arc 2 : centre (L - r_B, z_B) ; L - s = r_B (1 + cos psi), psi de psi_C a pi.
		if (sampled)
		{
			const double rb = p.r_end, tc = L - sc;
			const double psiC = std::acos(std::clamp((tc - rb) / rb, -1.0, 1.0));
			const double sweep = kPi - psiC;
			const int n = stepCount(sweep, rb, psiC, planRadius, params.max_angle);
			for (int j = 1; j < n; ++j)
			{
				const double s = L - rb * (1.0 + std::cos(psiC + sweep * j / n));
				if (s < L)
					push(s);
				if (stop)
					return;
			}
		}
		if (last < L)
			push(L);
	}
}

std::vector<double> SampleParameters (const SpaceCurve &curve, const SamplingParams &params)
{
	std::vector<double> out;
	forEachSample(curve, params, [&](double s) { out.push_back(s); return true; });
	return out;
}

// ===========================================================================
//  Reseau (etape 5)
// ===========================================================================

namespace
{
	bool byCurveThenS (const Incidence &a, const Incidence &b)
	{
		return a.curve != b.curve ? a.curve < b.curve : a.s < b.s;
	}

	std::vector<Incidence> sortedIncidences (const std::vector<NodeIncidence> &group)
	{
		std::vector<Incidence> out;
		out.reserve(group.size());
		for (const NodeIncidence &n : group)
			out.push_back(n.incidence);
		std::sort(out.begin(), out.end(), byCurveThenS);
		return out;
	}

	double distance3 (const Vector3d &a, const Vector3d &b)
	{
		return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
	}
}

NodeClassification ClassifyNode (const Vector2d &xy, std::vector<NodeIncidence> incidences,
                                 double tol_abscissa, double tol_height)
{
	NodeClassification out;
	// Une incidence par point 3D d'une courbe : meme courbe, s a tol_abscissa pres et
	// z a tol_height pres.
	std::sort(incidences.begin(), incidences.end(),
	          [](const NodeIncidence &a, const NodeIncidence &b) { return byCurveThenS(a.incidence, b.incidence); });
	std::vector<NodeIncidence> unique;
	for (const NodeIncidence &n : incidences)
		if (unique.empty() || unique.back().incidence.curve != n.incidence.curve
		    || std::fabs(unique.back().incidence.s - n.incidence.s) > tol_abscissa
		    || std::fabs(unique.back().position.z - n.position.z) > tol_height)
			unique.push_back(n);
	if (unique.empty())
		return out;

	// Groupes de hauteur : tri par z, coupure ou l'ecart depasse tol_height.
	std::stable_sort(unique.begin(), unique.end(),
	                 [](const NodeIncidence &a, const NodeIncidence &b) { return a.position.z < b.position.z; });
	std::vector<std::vector<NodeIncidence>> groups(1);
	for (std::size_t k = 0; k < unique.size(); ++k)
	{
		if (k > 0 && unique[k].position.z - unique[k - 1].position.z > tol_height)
			groups.emplace_back();
		groups.back().push_back(unique[k]);
	}

	for (const std::vector<NodeIncidence> &g : groups)
	{
		std::set<int> curves;
		for (const NodeIncidence &n : g)
			curves.insert(n.incidence.curve);
		if (curves.size() < 2)
			continue;
		Junction j;
		j.incidences = sortedIncidences(g);
		for (const NodeIncidence &n : g)
			if (n.incidence.curve == j.incidences.front().curve && n.incidence.s == j.incidences.front().s)
			{
				j.position = n.position;
				break;
			}
		for (std::size_t a = 0; a < g.size(); ++a)
			for (std::size_t b = a + 1; b < g.size(); ++b)
				j.max_gap = std::max(j.max_gap, distance3(g[a].position, g[b].position));
		out.junctions.push_back(std::move(j));
	}

	if (groups.size() >= 2)
	{
		VisualCrossing x;
		x.xy = xy;
		x.min_dz = std::numeric_limits<double>::infinity();
		for (std::size_t k = 0; k < groups.size(); ++k)
		{
			double sum = 0.0;
			for (const NodeIncidence &n : groups[k])
				sum += n.position.z;
			x.groups.push_back(HeightGroup{sum / static_cast<double>(groups[k].size()), sortedIncidences(groups[k])});
			if (k > 0)
				x.min_dz = std::min(x.min_dz, groups[k].front().position.z - groups[k - 1].back().position.z);
		}
		out.crossing = std::move(x);
	}
	return out;
}

namespace
{
	struct Box
	{
		double x0, x1, y0, y1;
	};

	// Boite englobante d'une courbe du plan, elargie de tol.
	Box boxOf (const PlanCurve &curve, double tol)
	{
		const Vector2d a = StartPoint(curve), b = EndPoint(curve);
		Box box{std::min(a.x, b.x), std::max(a.x, b.x), std::min(a.y, b.y), std::max(a.y, b.y)};
		if (const auto *arc = std::get_if<PlanArc>(&curve))
		{
			const double span = std::fabs(arc->sweep);
			for (int k = 0; k < 4; ++k)
			{
				const double theta = 0.5 * kPi * k;
				if (wrapTwoPi(signOf(arc->sweep) * (theta - arc->start_angle)) <= span)
				{
					const Vector2d p = onCircle(arc->center, arc->radius, theta);
					box.x0 = std::min(box.x0, p.x); box.x1 = std::max(box.x1, p.x);
					box.y0 = std::min(box.y0, p.y); box.y1 = std::max(box.y1, p.y);
				}
			}
		}
		box.x0 -= tol; box.x1 += tol; box.y0 -= tol; box.y1 += tol;
		return box;
	}

	struct PlanPoint
	{
		Vector2d  xy;
		Incidence incidence;
	};
}

namespace
{
	double curvatureOf (const PlanCurve &c)
	{
		const auto *arc = std::get_if<PlanArc>(&c);
		return arc ? 1.0 / arc->radius : 0.0;
	}

	// Longueur, d'un cote du point de contact, de la portion ou les deux courbes
	// restent a tol l'une de l'autre (M17) : sqrt(2 tol / kappa), kappa etant la
	// courbure relative des supports, bornee par ce qui reste de chaque courbe de
	// ce cote. Un raccord bout a bout G1 donne 0 ; un rebroussement, la zone
	// entiere du cote commun.
	double tangentZone (const PlanCurve &a, double s1, const PlanCurve &b, double s2, double tol)
	{
		double kappa = 0.0;
		const auto *arcA = std::get_if<PlanArc>(&a);
		const auto *arcB = std::get_if<PlanArc>(&b);
		if (arcA && arcB)
		{
			const double d = distance(arcA->center, arcB->center);
			const bool external = std::fabs(d - (arcA->radius + arcB->radius)) <= std::fabs(d - std::fabs(arcA->radius - arcB->radius));
			kappa = external ? curvatureOf(a) + curvatureOf(b) : std::fabs(curvatureOf(a) - curvatureOf(b));
		}
		else
			kappa = curvatureOf(a) + curvatureOf(b);
		const double half = kappa > 0.0 ? std::sqrt(2.0 * tol / kappa) : std::numeric_limits<double>::infinity();
		const double la = Length(a), lb = Length(b);
		const bool same = dot(TangentAt(a, s1), TangentAt(b, s2)) >= 0.0;
		const double aFwd = la - s1, aBack = s1;
		const double bFwd = same ? lb - s2 : s2, bBack = same ? s2 : lb - s2;
		return std::max(std::min({half, aFwd, bFwd}), std::min({half, aBack, bBack}));
	}
}

std::optional<Network> BuildNetwork (const MicroarchSpec &spec, MicroarchReport &report, const NetworkLimits &limits)
{
	const std::optional<DevelopedPlan> plan = DevelopPlan(spec, report);
	if (!plan)
		return std::nullopt;
	std::optional<std::vector<BasicCurveResult>> basics = BuildBasicCurves(spec, *plan, report);
	if (!basics)
		return std::nullopt;

	Network net;
	net.tol    = plan->tol;
	net.levels = spec.levels;
	net.basics = std::move(*basics);
	net.curves = BuildSpaceCurves(*plan, net.basics);
	const ResolvedTolerances &tol = net.tol;
	const std::size_t n = net.curves.size();

	// Paires candidates : balayage en x des boites, puis recouvrement en y.
	std::vector<Box> boxes;
	boxes.reserve(n);
	for (const SpaceCurve &c : net.curves)
		boxes.push_back(boxOf(c.plan, 2.0 * tol.point));
	std::vector<std::size_t> order(n);
	for (std::size_t k = 0; k < n; ++k)
		order[k] = k;
	std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return boxes[a].x0 < boxes[b].x0; });

	std::vector<PlanPoint> points;
	std::set<std::pair<int, int>> overlapPairs;   // (base i, base j), i <= j
	std::set<std::pair<int, int>> tangentOverlapPairs;
	std::set<int> tangentialBasics;
	std::size_t contacts = 0;
	for (std::size_t p = 0; p < n; ++p)
	{
		const std::size_t a = order[p];
		for (std::size_t q = p + 1; q < n && boxes[order[q]].x0 <= boxes[a].x1; ++q)
		{
			const std::size_t b = order[q];
			if (boxes[b].y0 > boxes[a].y1 || boxes[a].y0 > boxes[b].y1)
				continue;
			const std::size_t i = std::min(a, b), j = std::max(a, b);
			const SpaceCurve &ci = net.curves[i], &cj = net.curves[j];
			if (ci.level != cj.level)
				continue;
			const Intersection x = Intersect(ci.plan, cj.plan, tol.point);
			if (!x.overlaps.empty())
				overlapPairs.insert({std::min(ci.basic, cj.basic), std::max(ci.basic, cj.basic)});
			contacts += x.hits.size();
			if (contacts > limits.max_contacts)
			{
				report.Record(MicroarchError::TooManyContacts, "curves",
				              "more than " + std::to_string(limits.max_contacts)
				              + " contact points between the replicas : the network is too dense");
				return std::nullopt;
			}
			for (const CurveHit &h : x.hits)
			{
				points.push_back(PlanPoint{h.xy, Incidence{static_cast<int>(i), h.s1}});
				points.push_back(PlanPoint{h.xy, Incidence{static_cast<int>(j), h.s2}});
				if (h.tangential && !(h.at_end1 && h.at_end2))
				{
					tangentialBasics.insert(ci.basic);
					tangentialBasics.insert(cj.basic);
				}
				if (h.tangential
				    && tangentZone(ci.plan, h.s1, cj.plan, h.s2, tol.point) > NetworkLimits::tangent_zone_ratio * tol.scale)
					tangentOverlapPairs.insert({std::min(ci.basic, cj.basic), std::max(ci.basic, cj.basic)});
			}
		}
	}
	for (int b : tangentialBasics)
		report.Record(MicroarchWarning::TangentialContact, indexed("curves", static_cast<std::size_t>(b)),
		              "curve \"" + spec.curves[static_cast<std::size_t>(b)].id
		              + "\" touches a curve tangentially : the topology depends on the tolerance");
	if (!overlapPairs.empty() || !tangentOverlapPairs.empty())
	{
		auto other = [&](int bi, int bj) {
			return bi == bj ? std::string("one of its own replicas") : "curve \"" + spec.curves[static_cast<std::size_t>(bj)].id + "\"";
		};
		for (const auto &[bi, bj] : overlapPairs)
			report.Record(MicroarchError::PlanOverlap, indexed("curves", static_cast<std::size_t>(bi)),
			              "curve \"" + spec.curves[static_cast<std::size_t>(bi)].id + "\" overlaps " + other(bi, bj) + " in plan");
		for (const auto &[bi, bj] : tangentOverlapPairs)
			if (!overlapPairs.count({bi, bj}))
				report.Record(MicroarchError::PlanOverlap, indexed("curves", static_cast<std::size_t>(bi)),
				              "curve \"" + spec.curves[static_cast<std::size_t>(bi)].id + "\" is tangent to " + other(bi, bj)
				              + " along more than " + number(NetworkLimits::tangent_zone_ratio * tol.scale)
				              + " within the point tolerance : treated as an overlap");
		return std::nullopt;
	}

	// Noeuds : tri par x, regroupement a tol.point d'un representant fixe.
	std::sort(points.begin(), points.end(), [](const PlanPoint &a, const PlanPoint &b) {
		return a.xy.x != b.xy.x ? a.xy.x < b.xy.x : a.xy.y < b.xy.y;
	});
	std::vector<std::size_t> nodeOf(points.size(), 0);
	std::vector<Vector2d> nodeRep;
	{
		std::vector<bool> assigned(points.size(), false);
		for (std::size_t r = 0; r < points.size(); ++r)
		{
			if (assigned[r])
				continue;
			const Vector2d rep = points[r].xy;
			const std::size_t id = nodeRep.size();
			nodeRep.push_back(rep);
			for (std::size_t k = r; k < points.size() && points[k].xy.x <= rep.x + tol.point; ++k)
				if (!assigned[k] && distance(points[k].xy, rep) <= tol.point)
				{
					assigned[k] = true;
					nodeOf[k] = id;
				}
		}
	}
	// Deux noeuds qui partagent une incidence sont un seul noeud : union-find, la
	// racine etant le plus petit indice, donc le premier noeud dans l'ordre des x.
	// Meme incidence : meme courbe, |ds| <= tol.parameter et |dz| <= tol.height.
	// Le seuil en s absorbe les ecarts d'abscisse d'un meme point vu par plusieurs
	// paires (jusqu'a ~2 tol.point a l'interieur d'une courbe, davantage pres d'une
	// tangence) ; le seuil en z separe deux points proches en s mais pas en z, pres
	// d'une tangente laterale verticale, ou dz ~ sqrt(2 r ds).
	std::vector<Vector3d> position(points.size());
	for (std::size_t k = 0; k < points.size(); ++k)
		position[k] = PointAt(net.curves[static_cast<std::size_t>(points[k].incidence.curve)], points[k].incidence.s);
	std::vector<std::size_t> parent(nodeRep.size());
	for (std::size_t k = 0; k < parent.size(); ++k)
		parent[k] = k;
	auto root = [&](std::size_t k) {
		while (parent[k] != k)
			k = parent[k] = parent[parent[k]];
		return k;
	};
	{
		std::vector<std::size_t> byIncidence(points.size());
		for (std::size_t k = 0; k < points.size(); ++k)
			byIncidence[k] = k;
		std::sort(byIncidence.begin(), byIncidence.end(), [&](std::size_t a, std::size_t b) {
			return byCurveThenS(points[a].incidence, points[b].incidence);
		});
		for (std::size_t k = 1; k < byIncidence.size(); ++k)
		{
			const Incidence &p = points[byIncidence[k - 1]].incidence, &q = points[byIncidence[k]].incidence;
			if (p.curve != q.curve || q.s - p.s > tol.parameter
			    || std::fabs(position[byIncidence[k]].z - position[byIncidence[k - 1]].z) > tol.height)
				continue;
			const std::size_t a = root(nodeOf[byIncidence[k - 1]]), b = root(nodeOf[byIncidence[k]]);
			if (a != b)
				parent[std::max(a, b)] = std::min(a, b);
		}
	}
	std::vector<std::vector<NodeIncidence>> merged(nodeRep.size());
	for (std::size_t k = 0; k < points.size(); ++k)
	{
		merged[root(nodeOf[k])].push_back(NodeIncidence{points[k].incidence, position[k]});
	}

	std::vector<std::vector<bool>> atJunction(n);
	for (std::size_t k = 0; k < n; ++k)
		atJunction[k].assign(2, false);
	for (std::size_t r = 0; r < merged.size(); ++r)
	{
		if (merged[r].empty())
			continue;
		NodeClassification nc = ClassifyNode(nodeRep[r], std::move(merged[r]), tol.parameter, tol.height);
		for (Junction &j : nc.junctions)
		{
			for (const Incidence &inc : j.incidences)
			{
				const SpaceCurve &c = net.curves[static_cast<std::size_t>(inc.curve)];
				if (std::fabs(inc.s - net.basics[static_cast<std::size_t>(c.basic)].prc.s) <= tol.parameter)
					j.prc = true;
				if (inc.s <= tol.parameter)
					atJunction[static_cast<std::size_t>(inc.curve)][0] = true;
				if (inc.s >= c.profile.length - tol.parameter)
					atJunction[static_cast<std::size_t>(inc.curve)][1] = true;
			}
			const SpaceCurve &first = net.curves[static_cast<std::size_t>(j.incidences.front().curve)];
			j.ground = std::fabs(j.position.z - LevelBase(spec, first.level)) <= tol.height;
			if (j.max_gap > tol.height)
			{
				int basic = first.basic;
				for (const Incidence &inc : j.incidences)
					basic = std::min(basic, net.curves[static_cast<std::size_t>(inc.curve)].basic);
				report.Record(MicroarchWarning::JunctionHeightSpread, indexed("curves", static_cast<std::size_t>(basic)),
				              "a junction at (" + number(j.position.x) + ", " + number(j.position.y) + ") spreads over "
				              + number(j.max_gap) + " > tol.height = " + number(tol.height) + " (chained height groups)");
			}
			net.junctions.push_back(std::move(j));
		}
		if (nc.crossing)
			net.crossings.push_back(std::move(*nc.crossing));
	}
	for (std::size_t k = 0; k < n; ++k)
	{
		if (!atJunction[k][0])
			net.free_ends.push_back(Incidence{static_cast<int>(k), 0.0});
		if (!atJunction[k][1])
			net.free_ends.push_back(Incidence{static_cast<int>(k), net.curves[k].profile.length});
	}
	return net;
}

std::size_t CountNetworkSamples (const Network &network, const SamplingParams &params, std::size_t limit)
{
	std::size_t total = 0;
	for (const SpaceCurve &c : network.curves)
	{
		forEachSample(c, params, [&](double) { return ++total <= limit; });
		if (total > limit)
			break;
	}
	return total;
}

std::vector<std::vector<Vector3d>> SampleNetwork (const Network &network, const SamplingParams &params,
                                                  MicroarchReport &report)
{
	const std::size_t total = CountNetworkSamples(network, params, SamplingLimits::max_network_points);
	if (total > SamplingLimits::max_network_points)
	{
		report.Record(MicroarchError::SampleLimitExceeded, "sampling.max_angle",
		              "sampling the network would produce more than " + std::to_string(SamplingLimits::max_network_points)
		              + " points (max_angle = " + number(params.max_angle) + ") ; increase max_angle");
		return {};
	}
	std::vector<std::vector<Vector3d>> out;
	out.reserve(network.curves.size());
	for (const SpaceCurve &c : network.curves)
	{
		std::vector<Vector3d> line;
		for (double s : SampleParameters(c, params))
			line.push_back(PointAt(c, s));
		out.push_back(std::move(line));
	}
	return out;
}

namespace
{
	// Longueur 3D du morceau [s0, s1] d'une courbe (64 cordes).
	double pieceLength (const SpaceCurve &c, double s0, double s1)
	{
		double length = 0.0;
		Vector3d prev = PointAt(c, s0);
		for (int k = 1; k <= 64; ++k)
		{
			const Vector3d next = PointAt(c, s0 + (s1 - s0) * k / 64.0);
			const double dx = next.x - prev.x, dy = next.y - prev.y, dz = next.z - prev.z;
			length += std::sqrt(dx * dx + dy * dy + dz * dz);
			prev = next;
		}
		return length;
	}
}

NetworkGraph BuildNetworkGraph (const Network &network, double contract_length)
{
	NetworkGraph g;
	const std::size_t J = network.junctions.size();
	g.vertices = J + network.free_ends.size();
	const double tp = network.tol.parameter;

	// Coupures de chaque courbe : (abscisse, sommet), sommet = jonction ou extremite libre.
	std::vector<std::vector<std::pair<double, std::size_t>>> cuts(network.curves.size());
	for (std::size_t j = 0; j < J; ++j)
		for (const Incidence &i : network.junctions[j].incidences)
			if (i.curve >= 0 && static_cast<std::size_t>(i.curve) < cuts.size())
				cuts[static_cast<std::size_t>(i.curve)].emplace_back(i.s, j);
	for (std::size_t k = 0; k < network.free_ends.size(); ++k)
	{
		const Incidence &i = network.free_ends[k];
		if (i.curve >= 0 && static_cast<std::size_t>(i.curve) < cuts.size())
			cuts[static_cast<std::size_t>(i.curve)].emplace_back(i.s, J + k);
	}

	std::vector<std::size_t> parent(g.vertices);
	for (std::size_t v = 0; v < g.vertices; ++v)
		parent[v] = v;
	auto root = [&](std::size_t v) {
		while (parent[v] != v)
			v = parent[v] = parent[parent[v]];
		return v;
	};
	// Identification des sommets (abscisses confondues, morceaux contractes).
	std::vector<std::size_t> same(g.vertices);
	for (std::size_t v = 0; v < g.vertices; ++v)
		same[v] = v;
	auto sameRoot = [&](std::size_t v) {
		while (same[v] != v)
			v = same[v] = same[same[v]];
		return v;
	};
	auto identify = [&](std::size_t a, std::size_t b) {
		parent[root(a)] = root(b);
		same[sameRoot(a)] = sameRoot(b);
	};
	std::vector<std::size_t> edgeEnds;   // un sommet de chaque arete comptee
	// Bouts 0 et L : coupures sans sommet propre, rattachees a l'incidence ou a
	// l'extremite libre qui les porte, a tol.parameter pres.
	const std::size_t none = std::numeric_limits<std::size_t>::max();
	for (std::size_t c = 0; c < cuts.size(); ++c)
	{
		std::vector<std::pair<double, std::size_t>> &cs = cuts[c];
		cs.emplace_back(0.0, none);
		cs.emplace_back(network.curves[c].profile.length, none);
		std::sort(cs.begin(), cs.end());
		// Une abscisse par coupure : la premiere d'une grappe a tol.parameter pres.
		std::vector<std::pair<double, std::size_t>> distinct;
		for (const auto &cut : cs)
			if (distinct.empty() || cut.first - distinct.back().first > tp)
				distinct.push_back(cut);
			else if (distinct.back().second == none)
				distinct.back().second = cut.second;
			else if (cut.second != none)
				identify(cut.second, distinct.back().second);
		for (std::size_t k = 0; k + 1 < distinct.size(); ++k)
		{
			const std::size_t a = distinct[k].second, b = distinct[k + 1].second;
			const bool ends = a != none && b != none;
			// La longueur 3D majore la longueur du plan s1 - s0 : pas de calcul au-dela.
			if (ends && contract_length > 0.0 && distinct[k + 1].first - distinct[k].first <= contract_length
			    && pieceLength(network.curves[c], distinct[k].first, distinct[k + 1].first) <= contract_length)
			{
				identify(b, a);
				continue;
			}
			++g.edges;
			edgeEnds.push_back(a != none ? a : b);
			if (ends)
				parent[root(b)] = root(a);
		}
	}

	auto vertexZ = [&](std::size_t v, int &level) {
		if (v < J)
		{
			const Junction &jn = network.junctions[v];
			level = jn.incidences.empty() ? 0 : network.curves[static_cast<std::size_t>(jn.incidences[0].curve)].level;
			return jn.position.z;
		}
		const Incidence &i = network.free_ends[v - J];
		const SpaceCurve &c = network.curves[static_cast<std::size_t>(i.curve)];
		level = c.level;
		return PointAt(c, i.s).z;
	};
	std::vector<std::size_t> id(g.vertices, g.vertices);
	g.vertex_component.resize(g.vertices);
	for (std::size_t v = 0; v < g.vertices; ++v)
	{
		const std::size_t r = root(v);
		if (id[r] == g.vertices)
		{
			id[r] = g.components++;
			g.component_touches_base.push_back(false);
		}
		g.vertex_component[v] = id[r];
		int level = 0;
		const double z = vertexZ(v, level);
		double base = 0.0;
		for (const LevelSpec &l : network.levels)
			if (l.id == level)
				base = l.base;
		if (z <= base + network.tol.height)
			g.component_touches_base[id[r]] = true;
	}
	g.component_euler.assign(g.components, 0);
	for (std::size_t v = 0; v < g.vertices; ++v)
		if (contract_length <= 0.0 || sameRoot(v) == v)
			g.component_euler[g.vertex_component[v]] += 2;
	for (std::size_t v : edgeEnds)
		if (v != none)
			g.component_euler[g.vertex_component[v]] -= 2;
	if (contract_length > 0.0)
	{
		std::size_t identified = 0;
		for (std::size_t v = 0; v < g.vertices; ++v)
			identified += sameRoot(v) == v;
		g.vertices = identified;
	}
	return g;
}

} // namespace cgarchi::microarch
