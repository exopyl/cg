#include <cgarchi/classical_profile.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include <cgarchi/classical_diminution.h>
#include <cgarchi/classical_flutes.h>

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kQuarter = kPi / 4.0;

bool Same (double a, double b)
{
	return std::fabs (a - b) <= 1e-12 * std::max (1.0, std::max (std::fabs (a), std::fabs (b)));
}

// Arc de 60 degres de A a B (corde c, rayon c). `bulgeRight` : l'arc bombe a
// droite du sens A -> B. Ajoute les points apres A, B compris.
void AppendArc60 (std::vector<ProfilePoint> &out, ProfilePoint A, ProfilePoint B, bool bulgeRight, int segments)
{
	const double dx = B.s - A.s, dy = B.y - A.y;
	const double c = std::hypot (dx, dy);
	if (c == 0.0)
		return;
	// Normale a gauche du sens A -> B ; le centre est du cote oppose au bombement.
	const double nx = -dy / c, ny = dx / c;
	const double side = bulgeRight ? 1.0 : -1.0;
	const double h = c * std::sqrt (3.0) / 2.0;
	const double cx = 0.5 * (A.s + B.s) + side * nx * h;
	const double cy = 0.5 * (A.y + B.y) + side * ny * h;
	const double a0 = std::atan2 (A.y - cy, A.s - cx);
	double a1 = std::atan2 (B.y - cy, B.s - cx);
	// Arc court : |a1 - a0| = pi/3.
	while (a1 - a0 > kPi) a1 -= 2.0 * kPi;
	while (a1 - a0 < -kPi) a1 += 2.0 * kPi;
	for (int i = 1; i <= segments; ++i)
	{
		if (i == segments)
		{
			out.push_back (B);
			break;
		}
		const double a = a0 + (a1 - a0) * i / segments;
		out.push_back ({cx + c * std::cos (a), cy + c * std::sin (a)});
	}
}

// Trace dans la boite standard (s_bas <= s_haut : rentrant en bas, saillant en
// haut), de (x0, 0) a (x1, h).
std::vector<ProfilePoint> TraceStandard (MouldingForm form, double x0, double x1, double h, int n)
{
	const double a = x1 - x0;
	std::vector<ProfilePoint> pts;
	pts.push_back ({x0, 0.0});
	switch (form)
	{
	case MouldingForm::Ovolo:
		for (int i = 1; i <= n; ++i)
		{
			const double phi = 0.5 * kPi * i / n;
			pts.push_back ({i == n ? x1 : x0 + a * std::sin (phi), i == n ? h : h * (1.0 - std::cos (phi))});
		}
		break;
	case MouldingForm::Cavetto:
	case MouldingForm::Conge:
		for (int i = 1; i <= n; ++i)
		{
			const double phi = 0.5 * kPi * i / n;
			pts.push_back ({i == n ? x1 : x0 + a * (1.0 - std::cos (phi)), i == n ? h : h * std::sin (phi)});
		}
		break;
	case MouldingForm::CymaRecta:
	case MouldingForm::CymaReversa:
	{
		const int half = std::max (1, n / 2);
		const ProfilePoint M {0.5 * (x0 + x1), 0.5 * h};
		// Dans le sens de montee, la droite du trace est le cote de l'air :
		// bomber a droite, c'est etre convexe.
		const bool lowerConvex = form == MouldingForm::CymaRecta;
		AppendArc60 (pts, pts.back (), M, lowerConvex, half);
		AppendArc60 (pts, M, {x1, h}, !lowerConvex, half);
		break;
	}
	default:
		pts.push_back ({x1, h});
		break;
	}
	return pts;
}

} // namespace

std::vector<ProfilePoint> TraceMember (MouldingForm form, double h, Span span, double extremum, int curveSegments)
{
	if (!(h > 0.0))
		throw std::invalid_argument ("TraceMember : hauteur non positive");
	const int n = std::max (1, curveSegments);

	if (form == MouldingForm::Torus)
	{
		std::vector<ProfilePoint> pts;
		const double attach = span.bottom;
		const int m = std::max (2, n);
		for (int i = 0; i <= m; ++i)
		{
			const double phi = kPi * i / m;
			const double s = i == 0 || i == m ? attach : attach + (extremum - attach) * std::sin (phi);
			pts.push_back ({s, i == m ? h : 0.5 * h * (1.0 - std::cos (phi))});
		}
		return pts;
	}
	if (form == MouldingForm::Scotia)
	{
		std::vector<ProfilePoint> pts;
		const double yf = 3.0 * h / 5.0;
		const double bottom = span.bottom, top = span.top, deep = extremum;
		for (int i = 0; i <= n; ++i)
		{
			const double phi = 0.5 * kPi * (n - i) / n;   // pi/2 -> 0
			pts.push_back ({i == 0 ? bottom : bottom - (bottom - deep) * std::cos (phi),
			                i == 0 ? 0.0 : yf - yf * std::sin (phi)});
		}
		for (int i = 1; i <= n; ++i)
		{
			const double phi = 0.5 * kPi * i / n;
			pts.push_back ({i == n ? top : top - (top - deep) * std::cos (phi),
			                i == n ? h : yf + (h - yf) * std::sin (phi)});
		}
		return pts;
	}

	const double x0 = std::min (span.bottom, span.top);
	const double x1 = std::max (span.bottom, span.top);
	const bool curved = form != MouldingForm::Fillet && form != MouldingForm::Face && form != MouldingForm::Ornament;
	if (!curved || x1 == x0)
		return {{span.bottom, 0.0}, {span.top, h}};

	std::vector<ProfilePoint> pts = TraceStandard (form, x0, x1, h, n);
	if (span.bottom > span.top)
	{
		// Boite retournee : (s, y) -> (s, h - y), puis remise de bas en haut.
		for (ProfilePoint &p : pts)
			p.y = h - p.y;
		std::reverse (pts.begin (), pts.end ());
	}
	// Les arcs de 60 degres d'une boite trop plate debordent en y : un trace
	// qui sort de [0, h] chevaucherait les membres voisins.
	const double tolerance = 1e-9 * h;
	for (const ProfilePoint &p : pts)
		if (p.y < -tolerance || p.y > h + tolerance)
			throw std::invalid_argument ("TraceMember : le trace sort de sa boite (diagonale trop plate pour des arcs de 60 degres)");
	pts.front ().y = 0.0;
	pts.back ().y = h;
	return pts;
}

// ---------------------------------------------------------------------------
//  Ring
// ---------------------------------------------------------------------------

Ring Ring::MakeCircle (double r)
{
	if (!(r > 0.0))
		throw std::invalid_argument ("Ring : rayon non positif");
	return {RingShape::Circle, r, 0.0};
}

Ring Ring::MakeSquare (double halfSide)
{
	if (!(halfSide > 0.0))
		throw std::invalid_argument ("Ring : demi-cote non positif");
	return {RingShape::Square, halfSide, 0.0};
}

Ring Ring::MakeFluted (double R, int count, double halfAngle, FluteSection section)
{
	constexpr double pi = 3.14159265358979323846;
	if (!(R > 0.0) || count < 3 || count % 4 != 0 || !(halfAngle > 0.0) || halfAngle > pi / count * (1.0 + 1e-12))
		throw std::invalid_argument ("Ring : cannelures hors domaine");
	Ring ring;
	ring.shape = RingShape::Fluted;
	ring.radius = R;
	ring.flutes = count;
	ring.halfAngle = halfAngle;
	ring.section = section;
	return ring;
}

Ring Ring::MakeConcaveSquare (double corner, double face)
{
	if (!(corner > 0.0) || !(face > 0.0) || face > corner * (1.0 + 1e-12))
		throw std::invalid_argument ("Ring : carre concave hors domaine (0 < face <= corne)");
	const double a = corner / std::sqrt (2.0);
	if (Same (face, corner))
		return MakeCircle (corner);
	if (Same (face, a))
		return MakeSquare (a);
	const double xc = (face * face - 2.0 * a * a) / (2.0 * (face - a));
	const double rho = std::fabs (face - xc);
	// Face creusee (face < a) : le rayon a 45 degres coupe le cercle en
	// a sqrt2 (la corne) et en (xc sqrt2 - a sqrt2) ; la corne est la plus proche,
	// donc sur l'arc vu depuis l'axe, si et seulement si xc >= 2a, soit
	// face >= (2 - sqrt2) a. Face bombee (a < face < corne) : l'axe doit etre
	// dans le cercle (rho > |xc|) pour que chaque rayon ne le coupe qu'une fois.
	if (face < a ? xc < 2.0 * a * (1.0 - 1e-12) : rho <= std::fabs (xc))
		throw std::invalid_argument ("Ring : face de carre concave non etoilee depuis l'axe");
	return {RingShape::ConcaveSquare, corner, face};
}

double Ring::RadiusAt (double theta) const
{
	if (shape == RingShape::Circle)
		return radius;
	if (shape == RingShape::Fluted)
		return FlutedRadius (radius, flutes, halfAngle, section, theta);
	double t = std::fmod (theta, 0.5 * kPi);
	if (t < 0.0)
		t += 0.5 * kPi;
	if (t > kQuarter)
		t = 0.5 * kPi - t;
	const double c = std::cos (t);
	if (shape == RingShape::Square)
		return radius / c;
	const double a = radius / std::sqrt (2.0);
	const double xc = (face * face - 2.0 * a * a) / (2.0 * (face - a));
	const double rho = std::fabs (face - xc);
	const double disc = std::max (0.0, xc * xc * c * c - xc * xc + rho * rho);
	return face < a ? xc * c - std::sqrt (disc) : xc * c + std::sqrt (disc);
}

bool Ring::operator== (const Ring &o) const
{
	return shape == o.shape && Same (radius, o.radius) && Same (face, o.face) && flutes == o.flutes &&
	       Same (halfAngle, o.halfAngle) && section == o.section;
}

std::vector<double> RingCrossings (const Ring &a, const Ring &b)
{
	std::vector<double> roots;
	if (a == b || (a.shape == RingShape::Circle && b.shape == RingShape::Circle))
		return roots;
	const double scale = std::max (a.radius, b.radius);
	const double eps = 1e-12 * scale;
	auto f = [&] (double t) { return a.RadiusAt (t) - b.RadiusAt (t); };

	constexpr int kSamples = 2048;
	double lastT = 0.0;
	double lastF = f (0.0);
	for (int i = 1; i <= kSamples; ++i)
	{
		const double t = kQuarter * i / kSamples;
		const double ft = f (t);
		if (std::fabs (ft) <= eps)
			continue;
		if (std::fabs (lastF) > eps && (ft > 0.0) != (lastF > 0.0))
		{
			double lo = lastT, hi = t, flo = lastF;
			for (int k = 0; k < 200 && hi - lo > 1e-15; ++k)
			{
				const double mid = 0.5 * (lo + hi);
				const double fm = f (mid);
				if ((fm > 0.0) == (flo > 0.0))
				{
					lo = mid;
					flo = fm;
				}
				else
					hi = mid;
			}
			const double root = 0.5 * (lo + hi);
			if (root > 1e-9 && root < kQuarter - 1e-9)
				roots.push_back (root);
		}
		lastT = t;
		lastF = ft;
	}
	return roots;
}

// ---------------------------------------------------------------------------
//  Assemblage
// ---------------------------------------------------------------------------

namespace
{

Ring MakeRing (PlanForm plan, double main, double face, bool twoCuts)
{
	switch (plan)
	{
	case PlanForm::Revolution:
		if (twoCuts)
			throw std::invalid_argument ("saillie a deux coupes sur un plan de revolution");
		return Ring::MakeCircle (main);
	case PlanForm::Square:
	case PlanForm::Linear:
	case PlanForm::IonicOriented:
		if (twoCuts)
			throw std::invalid_argument ("saillie a deux coupes sur un plan carre");
		return Ring::MakeSquare (main);
	case PlanForm::ConcaveSquare:
		return twoCuts ? Ring::MakeConcaveSquare (main, face) : Ring::MakeCircle (main);
	}
	throw std::invalid_argument ("plan inconnu");
}

class Assembler
{
public:
	Assembler (ColumnProfile &profile, const ColumnOptions &options, std::optional<double> ionicBand,
	           const std::vector<BandOverride> *overrides = nullptr)
		: m_p (profile), m_o (options), m_ionicBand (ionicBand), m_overrides (overrides)
	{
	}

	double y = 0.0;

	void Push (double vy, const Ring &ring, int member)
	{
		if (!m_p.vertices.empty ())
		{
			const ProfileVertex &last = m_p.vertices.back ();
			if (Same (last.y, vy) && last.ring == ring)
				return;
		}
		m_p.vertices.push_back ({vy, ring, member});
	}

	void AppendMember (const Block &block, const Member &m)
	{
		const Projection &p = m.projection;
		Span span = p.main;
		if (p.kind == ProjectionKind::CoreEnvelope && m_o.ornamentEnvelope)
			span = p.envelope;
		if (m_overrides)
			for (const BandOverride &o : *m_overrides)
				if (o.block == block.key && o.member == m.name)
					span = {o.s, o.s};
		const bool twoCuts = p.kind == ProjectionKind::TwoCuts;
		const std::vector<ProfilePoint> pts = TraceMember (m.form, m.h, span, p.extremum, m_o.curveSegments);

		const int index = static_cast<int> (m_p.members.size ());
		m_p.members.push_back ({block.key, m.name, y, y + m.h, m.plan});
		for (const ProfilePoint &pt : pts)
		{
			double face = 0.0;
			if (twoCuts)
			{
				// Affinite horizontale de la coupe diagonale sur la coupe par la face.
				const double dd = span.top - span.bottom;
				const double df = p.face.top - p.face.bottom;
				if (dd == 0.0 && df != 0.0)
					throw std::invalid_argument ("coupes incompatibles : " + m.name);
				const double w = dd == 0.0 ? 0.0 : (pt.s - span.bottom) / dd;
				face = block.axisDistance + p.face.bottom + w * df;
			}
			try
			{
				if (m.plan == PlanForm::IonicOriented && m_ionicBand)
					Push (y + pt.y, Ring::MakeSquare (*m_ionicBand), index);
				else
					Push (y + pt.y, MakeRing (m.plan, block.axisDistance + pt.s, face, twoCuts), index);
			}
			catch (const std::invalid_argument &e)
			{
				throw std::invalid_argument (block.key + "/" + m.name + " : " + e.what ());
			}
		}
		y += m.h;
	}

	// Membres du bloc de bas en haut (le JSON les donne de haut en bas).
	void AppendBlock (const Block &block, bool outsideModule)
	{
		for (auto it = block.members.rbegin (); it != block.members.rend (); ++it)
			if (block.IsOutsideModule (*it) == outsideModule)
				AppendMember (block, *it);
	}

private:
	ColumnProfile &m_p;
	const ColumnOptions &m_o;
	std::optional<double> m_ionicBand;
	const std::vector<BandOverride> *m_overrides = nullptr;
};

const Block &Require (const OrderData &order, const char *key);

// Architrave, frise et la corniche de l'ordre (au dorique, celle de
// `doricCornice`), de bas en haut.
void AppendEntablature (Assembler &as, const OrderData &order, const ColumnOptions &options)
{
	as.AppendBlock (Require (order, "architrave"), false);
	as.AppendBlock (Require (order, "frise"), false);
	if (order.order == Order::Doric)
		as.AppendBlock (Require (order, options.doricCornice == DoricCornice::Mutules ? "corniche_mutules"
		                                                                         : "corniche_denticules"),
		                false);
	else
		as.AppendBlock (Require (order, "corniche"), false);
}

const Block &Require (const OrderData &order, const char *key)
{
	const Block *b = order.FindBlock (key);
	if (!b)
		throw std::invalid_argument (std::string ("bloc absent : ") + order.key + "/" + key);
	return *b;
}

double SumHeights (const Block &block)
{
	double h = 0.0;
	for (const Member &m : block.members)
		h += m.h;
	return h;
}

} // namespace

ColumnProfile BuildColumnProfile (const OrderData &order, const ColumnOptions &options)
{
	ColumnProfile profile;
	std::optional<double> ionicBand;
	if (options.ionicVolutes && order.volute)
		ionicBand = order.volute->faceSpacing / 2.0;
	Assembler as (profile, options, ionicBand);

	if (options.pedestal)
		as.AppendBlock (Require (order, "piedestal"), false);

	const Block &base = Require (order, "base");
	as.AppendBlock (base, false);
	const double shaftFoot = as.y;
	as.AppendBlock (base, true);

	const ShaftProfile shaft (order);
	const double H = shaft.Height ();
	const Block &shaftTop = Require (order, "fut_haut");
	const double start = as.y - shaftFoot;
	const double end = H - SumHeights (shaftTop);
	if (!(end > start))
		throw std::invalid_argument ("fut de hauteur non positive : " + order.key);

	// Conges du fut (conventions.conges_du_fut) : quart de cercle de rayon
	// egal a la saillie du listel raccorde sur le vif local, pris dans la
	// hauteur du fut. L'ecart au vif suit le quart de cercle : s = rho (1 -
	// sin phi) en bas, rho (1 - cos phi) en haut ; le rayon est r(y) + s, tangent
	// au listel (horizontale) et au vif diminue (pente de r).
	double rhoFoot = 0.0, rhoTop = 0.0;
	if (options.shaftFillets)
	{
		if (profile.vertices.empty () || profile.vertices.back ().ring.shape != RingShape::Circle)
			throw std::invalid_argument ("listel du pied du fut non circulaire : " + order.key);
		rhoFoot = profile.vertices.back ().ring.radius - shaft.Radius (start);
		const Member &topListel = shaftTop.members.back ();
		if (topListel.form != MouldingForm::Fillet || topListel.projection.kind != ProjectionKind::Constant ||
		    topListel.plan != PlanForm::Revolution)
			throw std::invalid_argument ("le fut_haut ne commence pas par un listel de revolution : " + order.key);
		rhoTop = shaftTop.axisDistance + topListel.projection.main.bottom - shaft.Radius (end);
		if (!(rhoFoot > 0.0) || !(rhoTop > 0.0) || !(rhoFoot + rhoTop < end - start))
			throw std::invalid_argument ("conges du fut hors domaine : " + order.key);
	}
	const double vifBottom = start + rhoFoot;
	const double vifTop = end - rhoTop;
	const int curve = std::max (1, options.curveSegments);
	constexpr double kHalfPi = 1.57079632679489661923;

	for (int i = 0; options.shaftFillets && i < curve; ++i)
	{
		const double phi = kHalfPi * i / curve;
		const double yl = start + rhoFoot * (1.0 - std::cos (phi));
		as.Push (shaftFoot + yl, Ring::MakeCircle (shaft.Radius (yl) + rhoFoot * (1.0 - std::sin (phi))), -1);
	}

	// Cannelures : du sommet du conge inferieur (arret droit) au pied du conge
	// superieur, ou la tete en demi-cercle de rayon R beta (developpe) est
	// tangente. Au-dessus de headBottom, l'anneau k a des aretes a +-beta k/m,
	// angles de la grille : sa hauteur est celle ou la tete a cette demi-largeur.
	std::optional<FluteGrid> grid;
	if (options.flutes && order.flutes)
	{
		FluteGrid g;
		g.count = order.flutes->count;
		g.halfAngle = FluteHalfAngle (order);
		g.samples = options.fluteSamples > 0 ? options.fluteSamples : std::max (6, curve);
		g.section = order.order == Order::Doric ? options.doricFlutes : FluteSection::Semicircle;
		g.offsets = FluteGridOffsets (g.halfAngle, g.section, g.samples, options.fluteHeadRings);
		g.headHalfAngles = HeadHalfAngles (g.halfAngle, g.section, g.samples, options.fluteHeadRings);
		if (std::find (order.flutes->sections.begin (), order.flutes->sections.end (), g.section) ==
		    order.flutes->sections.end ())
			throw std::invalid_argument ("section de cannelure absente des donnees : " + order.key);
		const double head = shaft.Radius (vifTop) * g.halfAngle;
		g.headBottom = shaftFoot + vifTop - head;
		if (!(g.headBottom > shaftFoot + vifBottom))
			throw std::invalid_argument ("vif trop court pour les cannelures : " + order.key);
		grid = g;
	}
	auto ringAt = [&] (double yl, double halfAngle) {
		return grid ? Ring::MakeFluted (shaft.Radius (yl), grid->count, halfAngle, grid->section)
		            : Ring::MakeCircle (shaft.Radius (yl));
	};

	std::vector<double> ys;
	const int segments = std::max (2, options.shaftSegments);
	for (int i = 0; i <= segments; ++i)
		ys.push_back (vifBottom + (vifTop - vifBottom) * i / segments);
	if (H / 3.0 > vifBottom && H / 3.0 < vifTop)
		ys.push_back (H / 3.0);
	const double headBottom = grid ? grid->headBottom - shaftFoot : vifTop;
	if (grid)
	{
		ys.erase (std::remove_if (ys.begin (), ys.end (), [&] (double y) { return y > headBottom; }), ys.end ());
		ys.push_back (headBottom);
	}
	std::sort (ys.begin (), ys.end ());
	ys.erase (std::unique (ys.begin (), ys.end (), Same), ys.end ());
	if (grid)
		as.Push (shaftFoot + vifBottom, Ring::MakeCircle (shaft.Radius (vifBottom)), -1);
	for (double yl : ys)
		as.Push (shaftFoot + yl, ringAt (yl, grid ? grid->halfAngle : 0.0), -1);
	if (grid)
	{
		const double head = vifTop - headBottom;
		for (double half : grid->headHalfAngles)
		{
			const double t = half / grid->halfAngle;
			const double yl = headBottom + head * std::sqrt (1.0 - t * t);
			as.Push (shaftFoot + yl, ringAt (yl, half), -1);
		}
		as.Push (shaftFoot + vifTop, Ring::MakeCircle (shaft.Radius (vifTop)), -1);
	}
	profile.flutes = grid;

	for (int i = 1; options.shaftFillets && i <= curve; ++i)
	{
		const double phi = kHalfPi * i / curve;
		const double yl = i == curve ? end : vifTop + rhoTop * std::sin (phi);
		as.Push (shaftFoot + yl, Ring::MakeCircle (shaft.Radius (yl) + rhoTop * (1.0 - std::cos (phi))), -1);
	}
	as.y = shaftFoot + end;
	profile.shaft = {shaftFoot + start, shaftFoot + vifBottom, shaftFoot + vifTop, shaftFoot + end, rhoFoot, rhoTop};

	as.AppendBlock (shaftTop, false);
	as.AppendBlock (Require (order, "chapiteau"), false);

	if (options.entablature)
		AppendEntablature (as, order, options);
	profile.height = as.y;
	return profile;
}

ColumnProfile BuildEntablatureProfile (const OrderData &order, const ColumnOptions &options,
                                       const std::vector<BandOverride> &overrides)
{
	for (const BandOverride &o : overrides)
	{
		const Block *b = order.FindBlock (o.block);
		if (!b || std::none_of (b->members.begin (), b->members.end (), [&] (const Member &m) { return m.name == o.member; }))
			throw std::invalid_argument ("BuildEntablatureProfile : membre inconnu " + o.block + "/" + o.member);
	}
	ColumnProfile profile;
	Assembler as (profile, options, std::nullopt, &overrides);
	AppendEntablature (as, order, options);
	profile.height = as.y;
	return profile;
}

std::vector<PlanConflict> FindPlanConflicts (const ColumnProfile &profile)
{
	auto label = [&] (int member) {
		if (member < 0)
			return std::string ("fut");
		const ProfileMember &m = profile.members[member];
		return m.block + "/" + m.name;
	};
	std::vector<PlanConflict> conflicts;
	for (std::size_t i = 1; i < profile.vertices.size (); ++i)
	{
		const ProfileVertex &lo = profile.vertices[i - 1];
		const ProfileVertex &hi = profile.vertices[i];
		if (!Same (lo.y, hi.y))
			continue;
		std::vector<double> angles = RingCrossings (lo.ring, hi.ring);
		if (angles.empty ())
			continue;
		conflicts.push_back ({i, label (lo.member), label (hi.member), hi.y, std::move (angles)});
	}
	return conflicts;
}

} // namespace classical
} // namespace cgarchi
