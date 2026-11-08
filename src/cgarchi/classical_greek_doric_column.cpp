#include <cgarchi/classical_greek_doric_column.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_flutes.h>
#include <cgarchi/classical_lathe.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
const char *const kCapitalBlock = "chapiteau";

[[noreturn]] void Fail (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("GreekDoricColumn : '" + field + "' " + rule);
}

// Une longueur non nulle est dans [FLT_MIN, kGreekDoricMaxLength] : le
// maillage est en float.
bool InLengthRange (double value)
{
	return value >= static_cast<double> (FLT_MIN) && value <= kGreekDoricMaxLength;
}

void RequirePositive (const std::string &field, double value)
{
	if (!(std::isfinite (value) && value > 0.0))
		Fail (field, "doit etre fini et strictement positif");
	if (!InLengthRange (value))
		Fail (field, "hors de la plage des longueurs representables en float");
}

void RequireNonNegative (const std::string &field, double value)
{
	if (!(std::isfinite (value) && value >= 0.0))
		Fail (field, "doit etre fini et positif ou nul");
	if (value != 0.0 && !InLengthRange (value))
		Fail (field, "hors de la plage des longueurs representables en float");
}

bool SameFloat (double a, double b)
{
	return static_cast<float> (a) == static_cast<float> (b);
}

std::string AnnuletField (std::size_t k, const char *member)
{
	return "annulets[" + std::to_string (k) + "]." + member;
}

double CapitalHeight (const GreekDoricColumnSettings &s)
{
	double h = s.hypotrachelionHeight;
	for (const GreekDoricAnnulet &a : s.annulets)
		h += a.height;
	return h + s.echinusHeight + s.abacusHeight;
}

void RequireCount (const char *field, int value, int lo, int hi)
{
	if (value < lo || value > hi)
		Fail (field, "= " + std::to_string (value) + " hors de [" + std::to_string (lo) + ", " + std::to_string (hi) + "]");
}

bool Same (double a, double b)
{
	return std::fabs (a - b) <= 1e-12 * std::max (1.0, std::max (std::fabs (a), std::fabs (b)));
}

double ShaftHeight (const GreekDoricColumnSettings &s)
{
	return s.height - CapitalHeight (s);
}

// Levre de l'echine (registre GD4a), en coordonnees (saillie, hauteur depuis le
// pied de l'echine) : arc de rayon R = echinusLipRadius, de centre C sur la
// mediatrice de la corde jonction-tete, du cote rentrant. Le centre n'est
// jamais forme : pour R grand devant la corde, ses coordonnees sont de l'ordre
// de R, et C + R (cos a, sin a) perdrait la position par annulation. Un point
// de l'arc s'ecrit depuis la jonction J, a l'ecart angulaire d depuis celle-ci :
// J + R (cos (a + d) - cos a, sin (a + d) - sin a), avec a l'angle polaire de J
// autour de C, les differences etant ecrites en produits de sinus.
struct Lip
{
	double halfChord = 0.0;
	double junctionAngle = 0.0;   // angle polaire de la jonction autour du centre
	double sweep = 0.0;           // ecart angulaire de la jonction a la tete, 2 asin (halfChord / R)
	bool   centreInside = false;  // centre moins saillant que la jonction et que la tete

	// Le point le plus saillant de l'arc (angle polaire nul) est interieur a la levre.
	bool HasInnerExtremum () const { return junctionAngle < 0.0 && junctionAngle + sweep > 0.0; }
};

// Sans controle : la validation verifie echinusLipRadius >= halfChord et
// centreInside avant toute construction.
Lip LipOf (const GreekDoricColumnSettings &s)
{
	const double ds = s.echinusTopProjection - s.echinusJunctionProjection;
	const double dy = s.echinusHeight - s.echinusJunctionHeight;
	const double chord = std::hypot (ds, dy);
	const double r = s.echinusLipRadius;
	Lip lip;
	lip.halfChord = 0.5 * chord;
	// Distance du centre au milieu de la corde, sans le carre de R.
	const double offset = std::sqrt (std::max (0.0, (r - lip.halfChord) * (r + lip.halfChord)));
	// J - C et T - C : demi-corde, plus offset fois la normale a la corde
	// orientee vers les saillies croissantes, (dy, -ds) / corde.
	const double jcs = -0.5 * ds + offset * dy / chord, jcy = -0.5 * dy - offset * ds / chord;
	const double tcs = 0.5 * ds + offset * dy / chord;
	lip.junctionAngle = std::atan2 (jcy, jcs);
	lip.sweep = 2.0 * std::asin (std::min (1.0, lip.halfChord / r));
	lip.centreInside = jcs > 0.0 && tcs > 0.0;
	return lip;
}

// Point de la levre a l'ecart angulaire d de la jonction.
ProfilePoint LipPoint (const GreekDoricColumnSettings &s, const Lip &lip, double d)
{
	const double chord = 2.0 * s.echinusLipRadius * std::sin (0.5 * d);
	const double middle = lip.junctionAngle + 0.5 * d;
	return {s.echinusJunctionProjection - chord * std::sin (middle), s.echinusJunctionHeight + chord * std::cos (middle)};
}

// Point le plus saillant du cercle de la levre, angle polaire nul. Le controle
// de l'abaque et le sommet insere lisent cette meme valeur.
ProfilePoint LipExtremum (const GreekDoricColumnSettings &s, const Lip &lip)
{
	return LipPoint (s, lip, -lip.junctionAngle);
}

double MaxEchinusProjection (const GreekDoricColumnSettings &s, const Lip &lip)
{
	double m = std::max ({s.echinusFootProjection, s.echinusJunctionProjection, s.echinusTopProjection});
	if (lip.HasInnerExtremum ())
		m = std::max (m, LipExtremum (s, lip).s);
	return m;
}

// Cotes des anneaux du fut : grille reguliere, dont un point confondu en float
// avec zf est remplace par zf ; 0 et shaftHeight sont toujours presents.
std::vector<double> ShaftLevels (const GreekDoricColumnSettings &s, double shaftHeight)
{
	std::vector<double> zs;
	for (int i = 0; i <= s.shaftSegments; ++i)
	{
		const double z = i == s.shaftSegments ? shaftHeight : shaftHeight * i / s.shaftSegments;
		if (!SameFloat (z, s.entasisHeight))
			zs.push_back (z);
	}
	zs.push_back (s.entasisHeight);
	std::sort (zs.begin (), zs.end ());
	return zs;
}

// Rayon du fut a la cote z : droite des deux rayons plus l'entasis (registre GD1).
double ShaftRadius (const GreekDoricColumnSettings &s, double shaftHeight, double z)
{
	const double t = z / shaftHeight;
	const double line = 0.5 * s.lowerDiameter * (1.0 - t) + 0.5 * s.upperDiameter * t;
	const double zf = s.entasisHeight;
	const double u = z <= zf ? (z - zf) / zf : (z - zf) / (shaftHeight - zf);
	return line + s.entasisSagitta * (1.0 - u * u);
}

// Empilement des sommets du profil ; deux sommets consecutifs identiques sont
// fusionnes.
class Stack
{
public:
	explicit Stack (ColumnProfile &profile) : m_p (profile) {}

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

	// Membre cylindrique d'anneau `ring`, de hauteur h. Il rejoint le membre
	// precedent par une jonction plate.
	void AppendCylinder (const char *name, double h, const Ring &ring, PlanForm plan)
	{
		const int index = BeginMember (name, h, plan);
		Push (y, ring, index);
		Push (y + h, ring, index);
		y += h;
	}

	// Membre droit (listel ou face) de saillie s depuis le rayon `reference`.
	void AppendStraight (const char *name, MouldingForm form, double h, double s, PlanForm plan, double reference)
	{
		const int index = BeginMember (name, h, plan);
		for (const ProfilePoint &pt : TraceMember (form, h, {s, s}, 0.0, 1))
			Push (y + pt.y, plan == PlanForm::Square ? Ring::MakeSquare (reference + pt.s) : Ring::MakeCircle (reference + pt.s),
			      index);
		y += h;
	}

	// Echine (registre GD4a) : droite du pied a la jonction, puis levre de
	// curveSegments segments egaux en angle, plus son point le plus saillant
	// s'il est interieur. Ce point remplace un sommet de la levre confondu avec
	// lui en float ; il n'est pas ajoute s'il se confond avec une extremite.
	void AppendEchinus (const GreekDoricColumnSettings &s, const Lip &lip, double reference)
	{
		const int index = BeginMember ("echine", s.echinusHeight, PlanForm::Revolution);
		Push (y, Ring::MakeCircle (reference + s.echinusFootProjection), index);
		Push (y + s.echinusJunctionHeight, Ring::MakeCircle (reference + s.echinusJunctionProjection), index);

		std::vector<ProfilePoint> arc;
		const int n = s.curveSegments;
		for (int i = 1; i <= n; ++i)
		{
			if (i == n)
			{
				arc.push_back ({s.echinusTopProjection, s.echinusHeight});
				break;
			}
			arc.push_back (LipPoint (s, lip, lip.sweep * i / n));
		}
		if (lip.HasInnerExtremum ())
		{
			const ProfilePoint extremum = LipExtremum (s, lip);
			const bool atJunction = SameFloat (y + extremum.y, y + s.echinusJunctionHeight);
			const bool atTop = SameFloat (y + extremum.y, y + s.echinusHeight);
			if (!atJunction && !atTop)
			{
				const auto same = std::find_if (arc.begin (), arc.end () - 1,
				                                [&] (const ProfilePoint &p) { return SameFloat (y + p.y, y + extremum.y); });
				if (same != arc.end () - 1)
					*same = extremum;
				else
					arc.insert (std::upper_bound (arc.begin (), arc.end (), extremum,
					                              [] (const ProfilePoint &a, const ProfilePoint &b) { return a.y < b.y; }),
					            extremum);
			}
		}
		for (const ProfilePoint &p : arc)
			Push (y + p.y, Ring::MakeCircle (reference + p.s), index);
		y += s.echinusHeight;
	}

private:
	int BeginMember (const char *name, double h, PlanForm plan)
	{
		const int index = static_cast<int> (m_p.members.size ());
		m_p.members.push_back ({kCapitalBlock, name, y, y + h, plan});
		return index;
	}

	ColumnProfile &m_p;
};

} // namespace

double GreekDoricFluteBottomRatio (int fluteCount, FluteSection section, double fluteDepthRatio)
{
	if (fluteCount < 1)
		Fail ("fluteCount", "= " + std::to_string (fluteCount) + " : au moins une cannelure");
	const double beta = kPi / fluteCount;
	switch (section)
	{
	case FluteSection::Segment60:
		return std::cos (beta) - 2.0 * std::sin (beta) * (1.0 - std::sqrt (3.0) / 2.0);
	case FluteSection::Semicircle:
		return std::cos (beta) - std::sin (beta);
	case FluteSection::ThreeCentred:
		return std::cos (beta) - 2.0 * std::sin (beta) * fluteDepthRatio;
	}
	Fail ("fluteSection", "hors de l'enumeration");
}

void ValidateGreekDoricColumnSettings (const GreekDoricColumnSettings &s)
{
	RequirePositive ("lowerDiameter", s.lowerDiameter);
	RequirePositive ("upperDiameter", s.upperDiameter);
	RequireNonNegative ("entasisSagitta", s.entasisSagitta);
	if (s.fluteCount != 0)
	{
		RequireCount ("fluteCount", s.fluteCount, 4, kGreekDoricMaxFlutes);
		if (s.fluteCount % 4 != 0)
			Fail ("fluteCount", "= " + std::to_string (s.fluteCount) + " n'est pas un multiple de 4");
	}
	if (s.fluteSection != FluteSection::Segment60 && s.fluteSection != FluteSection::Semicircle &&
	    s.fluteSection != FluteSection::ThreeCentred)
		Fail ("fluteSection", "hors de l'enumeration");
	if (s.fluteCount != 0 && s.fluteSection == FluteSection::ThreeCentred)
	{
		if (!(std::isfinite (s.fluteDepthRatio) && s.fluteDepthRatio > 0.0 && s.fluteDepthRatio < 0.5))
			Fail ("fluteDepthRatio", "doit etre fini dans ]0, 1/2[ pour la section a trois centres");
		if (!IsThreeCentredFluteStarShaped (kPi / s.fluteCount, s.fluteDepthRatio))
			Fail ("fluteDepthRatio", "donne avec fluteCount = " + std::to_string (s.fluteCount) +
			                             " une section a trois centres non etoilee depuis l'axe");
	}
	if (s.fluteCount != 0 && !(GreekDoricFluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio) >= kGreekDoricMinFluteBottomRatio))
		Fail ("fluteCount", "= " + std::to_string (s.fluteCount) + " : avec cette section, le fond des cannelures est a moins de " +
		                        std::to_string (kGreekDoricMinFluteBottomRatio) + " fois le rayon de l'axe");

	const double upperRadius = 0.5 * s.upperDiameter;
	RequireNonNegative ("hypotrachelionHeight", s.hypotrachelionHeight);
	RequireNonNegative ("grooveHeight", s.grooveHeight);
	RequireNonNegative ("grooveDepth", s.grooveDepth);
	if (s.grooveHeight > s.hypotrachelionHeight)
		Fail ("grooveHeight", "depasse hypotrachelionHeight");
	if (s.grooveHeight == 0.0 && s.grooveDepth != 0.0)
		Fail ("grooveDepth", "non nulle sans rainure (grooveHeight = 0)");
	if (s.grooveHeight > 0.0)
	{
		if (s.grooveDepth == 0.0)
			Fail ("grooveDepth", "nulle avec une rainure");
		if (!(s.grooveDepth < upperRadius))
			Fail ("grooveDepth", "doit etre inferieure au rayon upperDiameter / 2");
		if (s.fluteCount != 0 &&
		    upperRadius - s.grooveDepth > upperRadius * GreekDoricFluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio))
			Fail ("grooveDepth", "met le fond de la rainure au-dessus du fond des cannelures (anneau non representable)");
	}

	if (s.annulets.size () > static_cast<std::size_t> (kGreekDoricMaxAnnulets))
		Fail ("annulets", "compte " + std::to_string (s.annulets.size ()) + " listels, au-dela de " +
		                      std::to_string (kGreekDoricMaxAnnulets));
	for (std::size_t k = 0; k < s.annulets.size (); ++k)
	{
		RequirePositive (AnnuletField (k, "height"), s.annulets[k].height);
		RequireNonNegative (AnnuletField (k, "projection"), s.annulets[k].projection);
	}

	RequirePositive ("echinusHeight", s.echinusHeight);
	RequirePositive ("echinusJunctionHeight", s.echinusJunctionHeight);
	if (!(s.echinusJunctionHeight < s.echinusHeight))
		Fail ("echinusJunctionHeight", "doit etre dans ]0, echinusHeight[");
	RequireNonNegative ("echinusFootProjection", s.echinusFootProjection);
	RequireNonNegative ("echinusJunctionProjection", s.echinusJunctionProjection);
	RequireNonNegative ("echinusTopProjection", s.echinusTopProjection);
	RequirePositive ("echinusLipRadius", s.echinusLipRadius);
	const Lip lip = LipOf (s);
	if (!(s.echinusLipRadius >= lip.halfChord))
		Fail ("echinusLipRadius", "inferieur a la demi-distance de la jonction a la tete de l'echine");
	if (!lip.centreInside)
		Fail ("echinusLipRadius", "place le centre de la levre a une saillie au moins egale a celle de la jonction ou de la tete");

	RequirePositive ("abacusHeight", s.abacusHeight);
	if (!(std::isfinite (s.abacusProjection) && s.abacusProjection >= MaxEchinusProjection (s, lip)))
		Fail ("abacusProjection", "doit etre fini et au moins egal a la saillie maximale de l'echine");
	RequirePositive ("abacusProjection", s.abacusProjection);
	RequireCount ("curveSegments", s.curveSegments, 1, kGreekDoricMaxCurveSegments);
	RequireCount ("shaftSegments", s.shaftSegments, 2, kGreekDoricMaxShaftSegments);
	RequireCount ("fluteSamples", s.fluteSamples, 1, kGreekDoricMaxFluteSamples);
	RequireCount ("radialSegments", s.radialSegments, 8, kGreekDoricMaxRadialSegments);

	RequirePositive ("height", s.height);
	const double shaftHeight = ShaftHeight (s);
	if (!(std::isfinite (shaftHeight) && shaftHeight > 0.0))
		Fail ("height", "doit depasser la hauteur du chapiteau");
	if (!(std::isfinite (s.entasisHeight) && s.entasisHeight > 0.0 && s.entasisHeight < shaftHeight))
		Fail ("entasisHeight", "doit etre fini dans ]0, hauteur du fut[");
	if (SameFloat (s.entasisHeight, 0.0) || SameFloat (s.entasisHeight, shaftHeight))
		Fail ("entasisHeight", "confondu en float avec le pied ou le sommet du fut");

	// Cotes distinctes en float : chaque anneau du maillage a sa hauteur.
	const std::vector<double> zs = ShaftLevels (s, shaftHeight);
	for (std::size_t i = 1; i < zs.size (); ++i)
		if (SameFloat (zs[i - 1], zs[i]))
			Fail ("height", "donne deux anneaux du fut confondus en float");
	double y = shaftHeight;
	auto requireStep = [&] (const std::string &field, double h) {
		if (SameFloat (y, y + h))
			Fail (field, "absorbe par l'arrondi en float a la cote du membre");
		y += h;
	};
	if (s.grooveHeight > 0.0)
		requireStep ("grooveHeight", s.grooveHeight);
	if (s.hypotrachelionHeight > s.grooveHeight)
		requireStep ("hypotrachelionHeight", s.hypotrachelionHeight - s.grooveHeight);
	for (std::size_t k = 0; k < s.annulets.size (); ++k)
		requireStep (AnnuletField (k, "height"), s.annulets[k].height);
	requireStep ("echinusJunctionHeight", s.echinusJunctionHeight);
	requireStep ("echinusHeight", s.echinusHeight - s.echinusJunctionHeight);
	if (SameFloat (y, s.height))
		Fail ("abacusHeight", "absorbe par l'arrondi en float a la cote du membre");
}

ColumnProfile BuildGreekDoricColumnProfile (const GreekDoricColumnSettings &s)
{
	ValidateGreekDoricColumnSettings (s);

	const double shaftHeight = ShaftHeight (s);
	const double upperRadius = 0.5 * s.upperDiameter;

	ColumnProfile profile;
	if (s.fluteCount > 0)
	{
		FluteGrid g;
		g.count = s.fluteCount;
		g.halfAngle = kPi / s.fluteCount;
		g.samples = s.fluteSamples;
		g.section = s.fluteSection;
		g.headBottom = shaftHeight + s.hypotrachelionHeight;
		g.offsets = g.section == FluteSection::ThreeCentred
		                ? ThreeCentredFluteOffsets (g.halfAngle, SolveThreeCentredFlute (s.fluteDepthRatio), g.samples)
		                : FluteGridOffsets (g.halfAngle, g.section, g.samples, 0);
		profile.flutes = g;
	}
	auto flutedOrCircle = [&] (double r) {
		return profile.flutes ? Ring::MakeFluted (r, profile.flutes->count, profile.flutes->halfAngle, profile.flutes->section,
		                                          s.fluteDepthRatio)
		                      : Ring::MakeCircle (r);
	};

	Stack stack (profile);
	for (double z : ShaftLevels (s, shaftHeight))
		stack.Push (z, flutedOrCircle (z == shaftHeight ? upperRadius : ShaftRadius (s, shaftHeight, z)), -1);
	profile.shaft = {0.0, 0.0, shaftHeight, shaftHeight, 0.0, 0.0};

	stack.y = shaftHeight;
	if (s.grooveHeight > 0.0)
		stack.AppendCylinder ("rainure", s.grooveHeight, Ring::MakeCircle (upperRadius - s.grooveDepth), PlanForm::Revolution);
	if (s.hypotrachelionHeight > s.grooveHeight)
		stack.AppendCylinder ("gorgerin", s.hypotrachelionHeight - s.grooveHeight, flutedOrCircle (upperRadius),
		                      PlanForm::Revolution);
	for (const GreekDoricAnnulet &a : s.annulets)
		stack.AppendStraight ("annelet", MouldingForm::Fillet, a.height, a.projection, PlanForm::Revolution, upperRadius);
	stack.AppendEchinus (s, LipOf (s), upperRadius);
	stack.AppendStraight ("abaque", MouldingForm::Face, s.height - stack.y, s.abacusProjection, PlanForm::Square,
	                      upperRadius);
	// Sommet a la cote demandee, sans l'arrondi de la somme des hauteurs.
	profile.vertices.back ().y = s.height;
	profile.members.back ().y1 = s.height;
	profile.height = s.height;
	return profile;
}

namespace
{

// Demi-etendue selon X d'un anneau du profil grec, sur la grille du tour : les
// angles 0 (carre) et demi-angle (aretes des cannelures) sont dans la grille.
double RingHalfExtent (const Ring &ring)
{
	switch (ring.shape)
	{
	case RingShape::Circle:
	case RingShape::Square:
		return ring.radius;
	case RingShape::Fluted:
		return ring.radius * std::cos (ring.halfAngle);
	case RingShape::ConcaveSquare:
		break;
	}
	throw std::logic_error ("GreekDoricColumn : anneau hors des formes du profil grec");
}

} // namespace

GreekDoricColumnHalfExtents ComputeGreekDoricColumnHalfExtents (const GreekDoricColumnSettings &settings)
{
	const ColumnProfile profile = BuildGreekDoricColumnProfile (settings);
	GreekDoricColumnHalfExtents extents;
	extents.foot = RingHalfExtent (profile.vertices.front ().ring);
	for (const ProfileVertex &v : profile.vertices)
		extents.overall = std::max (extents.overall, RingHalfExtent (v.ring));
	return extents;
}

std::unique_ptr<Mesh> BuildGreekDoricColumnMesh (const GreekDoricColumnSettings &settings, ColumnProfile *profile)
{
	ColumnProfile built = BuildGreekDoricColumnProfile (settings);
	LatheOptions lathe;
	lathe.radialSegments = settings.radialSegments;
	lathe.splitAtPlanCrossings = true;
	lathe.scale = 1.0;
	std::unique_ptr<Mesh> mesh = LatheColumnProfile (built, lathe);
	if (profile)
		*profile = std::move (built);
	return mesh;
}

} // namespace classical
} // namespace cgarchi
