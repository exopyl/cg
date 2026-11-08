#include <gtest/gtest.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_profile.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Colonne dorique grecque : une colonne maillee
// ===========================================================================
//
// Reglages d'essai : SyntheticGreekDoricColumnSettings, valeurs synthetiques
// hors des points fixes (non exactes en binaire, differentes des valeurs par
// defaut), qui ne decrivent aucun monument. Les cotes attendues sont
// recalculees ici depuis les reglages, sans appeler les fonctions du module
// autres que la construction.
//
// Tolerances : les positions du maillage sont en float ; kTol est rapportee a
// la hauteur de la colonne.

using namespace cgarchi::classical;
using cgarchi_test::FitGrooveUnderFlutes;
using cgarchi_test::FluteBottomRatio;
using cgarchi_test::NarrowCapital;
using cgarchi_test::SyntheticGreekDoricColumnSettings;
using cgarchi_test::ThreeCentredFlutes;

namespace {

constexpr double kPi = 3.14159265358979323846;

double Tol (const GreekDoricColumnSettings &s)
{
	return 1e-6 * s.height;
}

struct Point
{
	double x = 0.0, y = 0.0, z = 0.0;
	double Radius () const { return std::hypot (x, y); }
	double Angle () const
	{
		const double a = std::atan2 (y, x);
		return a < 0.0 ? a + 2.0 * kPi : a;
	}
};

std::vector<Point> Points (const Mesh &m)
{
	std::vector<Point> out;
	for (unsigned v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		out.push_back ({p[0], p[1], p[2]});
	}
	return out;
}

// Sommets regroupes par cote z (les sommets d'un anneau partagent le meme float).
std::map<float, std::vector<Point>> Levels (const Mesh &m)
{
	std::map<float, std::vector<Point>> levels;
	for (const Point &p : Points (m))
		levels[static_cast<float> (p.z)].push_back (p);
	return levels;
}

double ExpectedAnnuletsHeight (const GreekDoricColumnSettings &s)
{
	double h = 0.0;
	for (const GreekDoricAnnulet &a : s.annulets)
		h += a.height;
	return h;
}

double ExpectedCapitalHeight (const GreekDoricColumnSettings &s)
{
	return s.hypotrachelionHeight + ExpectedAnnuletsHeight (s) + s.echinusHeight + s.abacusHeight;
}

double ExpectedShaftHeight (const GreekDoricColumnSettings &s)
{
	return s.height - ExpectedCapitalHeight (s);
}

// Levre de l'echine, recalculee depuis les reglages : centre sur la mediatrice
// de la corde jonction-tete, du cote des saillies decroissantes, a la distance
// echinusLipRadius des deux bouts. Le point le plus saillant du cercle,
// (cs + r, cy), est sur la levre si et seulement si sa hauteur est entre
// celles des deux bouts.
struct ExpectedLip
{
	double cs = 0.0, cy = 0.0;
	bool   innerExtremum = false;
	double maxProjection = 0.0;
};

ExpectedLip LipOfSettings (const GreekDoricColumnSettings &s)
{
	const double js = s.echinusJunctionProjection, jy = s.echinusJunctionHeight;
	const double ts = s.echinusTopProjection, ty = s.echinusHeight;
	const double r = s.echinusLipRadius;
	const double length = std::hypot (ts - js, ty - jy);
	const double d = std::sqrt (r * r - 0.25 * length * length);
	ExpectedLip lip;
	lip.cs = 0.5 * (js + ts) - d * (ty - jy) / length;
	lip.cy = 0.5 * (jy + ty) + d * (ts - js) / length;
	lip.innerExtremum = lip.cy > jy && lip.cy < ty;
	lip.maxProjection = std::max ({s.echinusFootProjection, js, ts, lip.innerExtremum ? lip.cs + r : 0.0});
	return lip;
}

// Anneau du fut le plus proche de z (hors chapiteau) : sommets a moins de 1e-9
// du rayon de l'axe exclus (centres des eventails).
std::vector<Point> RingAt (const Mesh &m, double z, double tol)
{
	std::vector<Point> ring;
	for (const Point &p : Points (m))
		if (std::fabs (p.z - z) <= tol && p.Radius () > 1e-9)
			ring.push_back (p);
	return ring;
}

double MaxRadius (const std::vector<Point> &ring)
{
	double r = 0.0;
	for (const Point &p : ring)
		r = std::max (r, p.Radius ());
	return r;
}

// Nombre d'angles distincts a 1e-5 pres, sur le cercle : les positions sont en
// float, et un angle voisin de 0 peut se lire voisin de 2 pi.
std::size_t DistinctAngles (std::vector<double> angles)
{
	constexpr double kGap = 1e-5;
	if (angles.empty ())
		return 0;
	std::sort (angles.begin (), angles.end ());
	std::size_t n = 1;
	for (std::size_t k = 1; k < angles.size (); ++k)
		if (angles[k] - angles[k - 1] > kGap)
			++n;
	if (n > 1 && angles.front () + 2.0 * kPi - angles.back () <= kGap)
		--n;
	return n;
}

std::unique_ptr<Mesh> Build (const GreekDoricColumnSettings &s, ColumnProfile *profile = nullptr)
{
	std::unique_ptr<Mesh> m = BuildGreekDoricColumnMesh (s, profile);
	EXPECT_NE (m, nullptr);
	return m;
}

// Variantes couvertes par les tests de forme. Le jeu synthetique a une rainure,
// trois listels de saillies croissantes et une levre qui sort puis rentre.
std::vector<std::pair<std::string, GreekDoricColumnSettings>> Variants ()
{
	std::vector<std::pair<std::string, GreekDoricColumnSettings>> v;
	const GreekDoricColumnSettings base = SyntheticGreekDoricColumnSettings ();
	v.push_back ({"synthetique", base});
	GreekDoricColumnSettings s = base;
	s.fluteSection = FluteSection::Semicircle;
	FitGrooveUnderFlutes (s);
	v.push_back ({"section B", s});
	s = base;
	ThreeCentredFlutes (s);
	v.push_back ({"trois centres", s});
	// Sous le seuil d'etoilement de 28 cannelures (0,2963), gorgerin sans rainure.
	s = base;
	s.fluteCount = 28;
	s.grooveHeight = 0.0;
	s.grooveDepth = 0.0;
	ThreeCentredFlutes (s, 0.293);
	v.push_back ({"28 cannelures a trois centres, voisines du seuil d'etoilement", s});
	s = base;
	s.fluteCount = 0;
	v.push_back ({"fut lisse", s});
	s = base;
	s.annulets.clear ();
	v.push_back ({"sans annelet", s});
	s = base;
	s.fluteCount = 12;
	FitGrooveUnderFlutes (s);
	s.echinusTopProjection = 0.24;
	s.echinusLipRadius = 0.1;
	v.push_back ({"12 cannelures, levre sans point plus saillant que sa tete", s});
	s = base;
	s.fluteCount = 28;
	s.hypotrachelionHeight = 0.0;
	s.grooveHeight = 0.0;
	s.grooveDepth = 0.0;
	s.entasisSagitta = 0.0;
	v.push_back ({"28 cannelures, sans gorgerin, sans entasis", s});
	s = base;
	s.grooveHeight = 0.0;
	s.grooveDepth = 0.0;
	v.push_back ({"gorgerin sans rainure", s});
	s = base;
	s.annulets = {{0.0113, 0.049}, {0.0097, 0.037}, {0.0089, 0.023}};
	v.push_back ({"listels de saillies decroissantes", s});
	s = base;
	s.lowerDiameter = 0.79;
	s.upperDiameter = 1.07;
	FitGrooveUnderFlutes (s);
	v.push_back ({"fut evase vers le haut (GD7)", s});
	// Levre symetrique, curveSegments pair : le point le plus saillant tombe sur
	// le sommet du milieu, qu'il remplace.
	s = base;
	s.echinusTopProjection = s.echinusJunctionProjection;
	v.push_back ({"levre symetrique", s});
	// Levre de centre (0,18 ; 0,25) et de rayon 0,047, dont le point le plus
	// saillant (angle nul) est a 1e-7 rad de la tete, puis de la jonction : a
	// 4,7e-9 de hauteur, il se confond en float avec elle et n'est pas ajoute.
	auto onCircle = [&] (double junctionAngle, double topAngle) {
		GreekDoricColumnSettings c = base;
		c.echinusJunctionProjection = 0.18 + 0.047 * std::cos (junctionAngle);
		c.echinusJunctionHeight = 0.25 + 0.047 * std::sin (junctionAngle);
		c.echinusTopProjection = 0.18 + 0.047 * std::cos (topAngle);
		c.echinusHeight = 0.25 + 0.047 * std::sin (topAngle);
		c.echinusLipRadius = 0.047;
		return c;
	};
	// Sommet du milieu (curveSegments = 6) a 1e-7 rad du point le plus saillant :
	// confondu avec lui en float, mais distinct a la tolerance de fusion des
	// sommets identiques (1e-12 relatif), si bien que seul le remplacement evite
	// deux anneaux a la meme cote float. La levre symetrique ci-dessus, ou les
	// deux points coincident, ne le distingue pas de la fusion.
	v.push_back ({"point le plus saillant voisin du sommet du milieu", onCircle (-0.5, 0.5 + 2e-7)});
	v.push_back ({"point le plus saillant confondu avec la tete", onCircle (-1.02, 1e-7)});
	v.push_back ({"point le plus saillant confondu avec la jonction", onCircle (-1e-7, 1.0)});
	return v;
}

bool SameFloat (double a, double b)
{
	return static_cast<float> (a) == static_cast<float> (b);
}

// Reglages aux bornes du domaine : resolutions minimales, 4 cannelures A,
// 8 cannelures B, 4 a trois centres sous leur seuil d'etoilement, profondeur
// a trois centres minuscule (jusqu'a la fenetre sous-normale), abaque egal a la saillie maximale de l'echine (sa
// tete), gorgerin reduit a sa rainure, sans annelet.
std::vector<std::pair<std::string, GreekDoricColumnSettings>> BoundaryVariants ()
{
	std::vector<std::pair<std::string, GreekDoricColumnSettings>> v;
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	s.fluteCount = 4;
	s.fluteSection = FluteSection::Segment60;
	s.grooveHeight = s.hypotrachelionHeight;
	FitGrooveUnderFlutes (s);
	s.annulets.clear ();
	s.echinusTopProjection = 0.24;
	s.echinusLipRadius = 0.1;
	s.abacusProjection = s.echinusTopProjection;
	s.curveSegments = 1;
	s.shaftSegments = 2;
	s.fluteSamples = 1;
	v.push_back ({"4 cannelures A, resolutions minimales, abaque = echine, gorgerin = rainure", s});
	s.fluteCount = 8;
	s.fluteSection = FluteSection::Semicircle;
	FitGrooveUnderFlutes (s);
	v.push_back ({"8 cannelures B, resolutions minimales", s});
	// Trois centres : sous le seuil d'etoilement de 4 cannelures (0,1472), puis
	// rapport minuscule (rayon central de l'ordre de 1,6e11 cordes).
	s.fluteCount = 4;
	ThreeCentredFlutes (s, 0.146);
	v.push_back ({"4 cannelures a trois centres voisines du seuil, resolutions minimales", s});
	s.fluteCount = 20;
	ThreeCentredFlutes (s, 1e-12);
	v.push_back ({"20 cannelures a trois centres, profondeur / corde 1e-12", s});
	// Fenetre sous-normale : rayon central de 1,6e307 cordes.
	s.fluteCount = 48;
	ThreeCentredFlutes (s, 1e-308);
	v.push_back ({"48 cannelures a trois centres, profondeur / corde 1e-308", s});
	s.fluteCount = 0;
	s.radialSegments = 8;
	v.push_back ({"fut lisse, 8 segments radiaux", s});
	return v;
}

} // namespace

// ---------------------------------------------------------------------------
//  Hauteurs et rayons
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_greek_doric_column, the_column_stands_on_the_stylobate_and_reaches_the_requested_height)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		ColumnProfile profile;
		const std::unique_ptr<Mesh> m = Build (s, &profile);
		double zMin = std::numeric_limits<double>::infinity (), zMax = -zMin;
		for (const Point &p : Points (*m))
		{
			zMin = std::min (zMin, p.z);
			zMax = std::max (zMax, p.z);
		}
		EXPECT_EQ (zMin, 0.0);
		EXPECT_EQ (zMax, static_cast<double> (static_cast<float> (s.height)));
		EXPECT_EQ (profile.height, s.height);
		EXPECT_EQ (profile.vertices.front ().y, 0.0);
		EXPECT_EQ (profile.vertices.back ().y, s.height);
	}
}

TEST (TEST_cgarchi_classical_greek_doric_column, the_shaft_radii_at_the_stylobate_and_under_the_capital_are_the_settings)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		const double tol = Tol (s);
		const double shaftHeight = ExpectedShaftHeight (s);
		ColumnProfile profile;
		const std::unique_ptr<Mesh> m = Build (s, &profile);

		// Profil : premier et dernier sommet du fut.
		const ProfileVertex *top = nullptr;
		for (const ProfileVertex &v : profile.vertices)
			if (v.member < 0)
				top = &v;
		ASSERT_NE (top, nullptr);
		EXPECT_EQ (profile.vertices.front ().member, -1);
		EXPECT_EQ (profile.vertices.front ().ring.radius, 0.5 * s.lowerDiameter);
		EXPECT_EQ (top->ring.radius, 0.5 * s.upperDiameter);
		EXPECT_NEAR (top->y, shaftHeight, tol);
		EXPECT_NEAR (profile.shaft.top, shaftHeight, tol);

		// Maillage : aretes au pied ; au sommet du fut, aretes (ou anneau lisse)
		// au rayon superieur, et rien entre lui et le membre suivant : rainure ou
		// gorgerin (au plus le rayon superieur), annelet ou pied de l'echine.
		const std::vector<Point> foot = RingAt (*m, 0.0, tol);
		EXPECT_NEAR (MaxRadius (foot), 0.5 * s.lowerDiameter, tol);
		const std::vector<Point> head = RingAt (*m, shaftHeight, tol);
		const double R1 = 0.5 * s.upperDiameter;
		const double above = s.hypotrachelionHeight > 0.0 ? R1
		                     : !s.annulets.empty ()      ? R1 + s.annulets.front ().projection
		                                                 : R1 + s.echinusFootProjection;
		std::size_t atR1 = 0;
		for (const Point &p : head)
		{
			const double r = p.Radius ();
			if (std::fabs (r - R1) <= tol)
				++atR1;
			EXPECT_FALSE (r > R1 + tol && r < above - tol) << r;
			EXPECT_FALSE (r > R1 + tol && s.hypotrachelionHeight > 0.0) << r;
		}
		const std::size_t expected = s.fluteCount > 0 ? static_cast<std::size_t> (s.fluteCount)
		                                              : static_cast<std::size_t> ((s.radialSegments + 7) / 8 * 8);
		EXPECT_EQ (atR1, expected);
	}
}

// ---------------------------------------------------------------------------
//  Entasis
// ---------------------------------------------------------------------------

namespace {

struct EntasisReading
{
	double maxDeviation = -std::numeric_limits<double>::infinity ();
	double atZ = 0.0;
	double minDeviation = std::numeric_limits<double>::infinity ();
	double footDeviation = 0.0, headDeviation = 0.0;
	int rings = 0;
	std::vector<std::pair<double, double>> deviations;   // (z, ecart)
};

// Loi GD1 : f (1 - u^2), u = (z - zf) / zf sous zf, (z - zf) / (H - zf) au-dessus.
double ExpectedEntasis (const GreekDoricColumnSettings &s, double z)
{
	const double H = ExpectedShaftHeight (s), zf = s.entasisHeight;
	const double u = z <= zf ? (z - zf) / zf : (z - zf) / (H - zf);
	return s.entasisSagitta * (1.0 - u * u);
}

// Ecart de chaque anneau du fut (rayon aux aretes) a la droite des deux rayons.
EntasisReading ReadEntasis (const Mesh &m, const GreekDoricColumnSettings &s)
{
	const double shaftHeight = ExpectedShaftHeight (s);
	const double tol = Tol (s);
	const double R0 = 0.5 * s.lowerDiameter, R1 = 0.5 * s.upperDiameter;
	EntasisReading out;
	for (const auto &[zf, points] : Levels (m))
	{
		const double z = zf;
		if (z > shaftHeight + tol)
			continue;
		double r = 0.0;
		for (const Point &p : points)
			if (std::fabs (z - shaftHeight) > tol || p.Radius () <= R1 + tol)
				r = std::max (r, p.Radius ());
		const double t = z / shaftHeight;
		const double d = r - (R0 * (1.0 - t) + R1 * t);
		++out.rings;
		out.deviations.push_back ({z, d});
		if (d > out.maxDeviation)
		{
			out.maxDeviation = d;
			out.atZ = z;
		}
		out.minDeviation = std::min (out.minDeviation, d);
		if (z == 0.0)
			out.footDeviation = d;
		if (std::fabs (z - shaftHeight) <= tol)
			out.headDeviation = d;
	}
	return out;
}

} // namespace

TEST (TEST_cgarchi_classical_greek_doric_column, the_entasis_departs_from_the_line_of_the_radii_by_its_sagitta_at_its_height)
{
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	for (const double zf : {s.entasisHeight, 3.1, 0.7})
	{
		for (const FluteSection section : {FluteSection::Segment60, FluteSection::Semicircle})
		{
			s.entasisHeight = zf;
			s.fluteSection = section;
			FitGrooveUnderFlutes (s);
			SCOPED_TRACE (zf);
			SCOPED_TRACE (section == FluteSection::Segment60 ? "A" : "B");
			const double tol = Tol (s);
			const std::unique_ptr<Mesh> m = Build (s);
			const EntasisReading e = ReadEntasis (*m, s);
			// shaftSegments + 1 anneaux reguliers, plus celui de zf.
			EXPECT_EQ (e.rings, s.shaftSegments + 2);
			EXPECT_NEAR (e.maxDeviation, s.entasisSagitta, tol);
			EXPECT_NEAR (e.atZ, zf, tol);
			EXPECT_GE (e.minDeviation, -tol);
			EXPECT_NEAR (e.footDeviation, 0.0, tol);
			EXPECT_NEAR (e.headDeviation, 0.0, tol);
			for (const auto &[z, d] : e.deviations)
				EXPECT_NEAR (d, ExpectedEntasis (s, z), tol) << "z = " << z;
		}
	}
}

TEST (TEST_cgarchi_classical_greek_doric_column, without_sagitta_the_shaft_follows_the_line_of_the_radii)
{
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	s.entasisSagitta = 0.0;
	for (const int flutes : {20, 0})
	{
		s.fluteCount = flutes;
		SCOPED_TRACE (flutes);
		const std::unique_ptr<Mesh> m = Build (s);
		const EntasisReading e = ReadEntasis (*m, s);
		EXPECT_NEAR (e.maxDeviation, 0.0, Tol (s));
		EXPECT_NEAR (e.minDeviation, 0.0, Tol (s));
	}
}

// ---------------------------------------------------------------------------
//  Cannelures
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_greek_doric_column, the_flutes_meet_in_as_many_sharp_arrises_as_set)
{
	for (const int count : {20, 12, 28})
	{
		for (const FluteSection section : {FluteSection::Segment60, FluteSection::Semicircle, FluteSection::ThreeCentred})
		{
			GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
			s.fluteCount = count;
			s.fluteSection = section;
			// Lu pour la section a trois centres seulement : 0,37 n'est etoile
			// pour aucun de ces nombres, et serait refuse s'il etait lu.
			s.fluteDepthRatio = section == FluteSection::ThreeCentred ? 0.17 : 0.37;
			FitGrooveUnderFlutes (s);
			SCOPED_TRACE (count);
			SCOPED_TRACE (section == FluteSection::Segment60 ? "A" : section == FluteSection::Semicircle ? "B" : "trois centres");
			const double tol = Tol (s);
			const std::unique_ptr<Mesh> m = Build (s);

			std::vector<Point> ring = RingAt (*m, s.entasisHeight, tol);
			ASSERT_FALSE (ring.empty ());
			std::sort (ring.begin (), ring.end (), [] (const Point &a, const Point &b) { return a.Angle () < b.Angle (); });
			const double R = MaxRadius (ring);

			std::vector<double> arrises;
			double rMin = R;
			for (std::size_t k = 0; k < ring.size (); ++k)
			{
				const double r = ring[k].Radius ();
				rMin = std::min (rMin, r);
				if (R - r > tol)
					continue;
				arrises.push_back (ring[k].Angle ());
				// Arete vive : ses deux voisins sont dans les cannelures.
				const Point &prev = ring[(k + ring.size () - 1) % ring.size ()];
				const Point &next = ring[(k + 1) % ring.size ()];
				EXPECT_GT (R - prev.Radius (), 1e-3 * R) << k;
				EXPECT_GT (R - next.Radius (), 1e-3 * R) << k;
			}
			ASSERT_EQ (arrises.size (), static_cast<std::size_t> (count));
			for (std::size_t k = 0; k < arrises.size (); ++k)
			{
				const double next = k + 1 < arrises.size () ? arrises[k + 1] : arrises[0] + 2.0 * kPi;
				EXPECT_NEAR (next - arrises[k], 2.0 * kPi / count, 1e-5) << k;
			}

			// Profondeur au milieu de la cannelure : section A, arc de 60 degres
			// creux de rayon egal a la corde ; section B, demi-cercle sur la corde ;
			// trois centres, profondeur 0,17 corde.
			const double beta = kPi / count;
			const double chord = 2.0 * R * std::sin (beta);
			const double depth = section == FluteSection::Segment60    ? chord * (1.0 - std::sqrt (3.0) / 2.0)
			                     : section == FluteSection::Semicircle ? 0.5 * chord
			                                                           : 0.17 * chord;
			const double expected = R * std::cos (beta) - depth;
			EXPECT_NEAR (rMin, expected, tol);
		}
	}
}

// ---------------------------------------------------------------------------
//  Chapiteau
// ---------------------------------------------------------------------------

// G6 : membres du chapiteau, rayons des cylindres, echine en droite et levre.
TEST (TEST_cgarchi_classical_greek_doric_column, the_capital_stacks_annulets_neck_echinus_and_abacus_at_their_settings)
{
	constexpr double kExact = 1e-9;
	int replaced = 0, skippedAtJunction = 0, skippedAtTop = 0;
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		const double tol = Tol (s);
		const double R1 = 0.5 * s.upperDiameter;
		ColumnProfile profile;
		const std::unique_ptr<Mesh> m = Build (s, &profile);

		// radius : rayon de tous les sommets du membre (NaN : echine, lue a part).
		// shapeChecked : la forme de l'anneau du gorgerin est l'affaire de G10.
		const double nan = std::numeric_limits<double>::quiet_NaN ();
		struct Expected { std::string name; double h; RingShape shape; bool shapeChecked; double radius; };
		std::vector<Expected> expected;
		if (s.grooveHeight > 0.0)
			expected.push_back ({"rainure", s.grooveHeight, RingShape::Circle, true, R1 - s.grooveDepth});
		if (s.hypotrachelionHeight > s.grooveHeight)
			expected.push_back ({"gorgerin", s.hypotrachelionHeight - s.grooveHeight, RingShape::Circle, false, R1});
		for (const GreekDoricAnnulet &a : s.annulets)
			expected.push_back ({"annelet", a.height, RingShape::Circle, true, R1 + a.projection});
		expected.push_back ({"echine", s.echinusHeight, RingShape::Circle, true, nan});
		expected.push_back ({"abaque", s.abacusHeight, RingShape::Square, true, R1 + s.abacusProjection});

		ASSERT_EQ (profile.members.size (), expected.size ());
		double y = ExpectedShaftHeight (s);
		for (std::size_t k = 0; k < expected.size (); ++k)
		{
			const ProfileMember &member = profile.members[k];
			SCOPED_TRACE (expected[k].name);
			EXPECT_EQ (member.block, "chapiteau");
			EXPECT_EQ (member.name, expected[k].name);
			EXPECT_NEAR (member.y0, y, tol);
			EXPECT_NEAR (member.y1, y + expected[k].h, tol);
			y += expected[k].h;

			const ProfileVertex *last = nullptr;
			for (const ProfileVertex &v : profile.vertices)
				if (v.member == static_cast<int> (k))
				{
					if (expected[k].shapeChecked)
						EXPECT_EQ (v.ring.shape, expected[k].shape);
					if (!std::isnan (expected[k].radius))
						EXPECT_NEAR (v.ring.radius, expected[k].radius, kExact) << v.y;
					last = &v;
				}
			ASSERT_NE (last, nullptr);
			EXPECT_NEAR (last->y, member.y1, tol);
		}

		// Echine : pied, jonction, puis levre sur son cercle jusqu'a la tete ;
		// hauteur strictement croissante ; le point le plus saillant de la
		// levre est un sommet s'il est interieur, et la saillie decroit alors
		// au-dessus de lui.
		const ExpectedLip lip = LipOfSettings (s);
		std::size_t echinusIndex = 0;
		while (profile.members[echinusIndex].name != "echine")
			++echinusIndex;
		const double y0 = profile.members[echinusIndex].y0;
		std::vector<ProfilePoint> echinus;
		for (const ProfileVertex &v : profile.vertices)
			if (v.member == static_cast<int> (echinusIndex))
				echinus.push_back ({v.ring.radius - R1, v.y - y0});
		ASSERT_GE (echinus.size (), 3u);
		EXPECT_NEAR (echinus[0].s, s.echinusFootProjection, kExact);
		EXPECT_NEAR (echinus[0].y, 0.0, kExact);
		EXPECT_NEAR (echinus[1].s, s.echinusJunctionProjection, kExact);
		EXPECT_NEAR (echinus[1].y, s.echinusJunctionHeight, kExact);
		EXPECT_NEAR (echinus.back ().s, s.echinusTopProjection, kExact);
		EXPECT_NEAR (echinus.back ().y, s.echinusHeight, kExact);
		// Nombre exact de sommets : pied, jonction, curveSegments sommets de la
		// levre (tete comprise), plus le point le plus saillant s'il est
		// interieur et ne se confond en float, a sa cote absolue, ni avec une
		// extremite (il n'est pas ajoute) ni avec un sommet de la levre (il le
		// remplace). Sommets recalcules ici sur le cercle, a angles egaux.
		const double lipRadius = s.echinusLipRadius;
		const double ja = std::atan2 (s.echinusJunctionHeight - lip.cy, s.echinusJunctionProjection - lip.cs);
		const double ta = std::atan2 (s.echinusHeight - lip.cy, s.echinusTopProjection - lip.cs);
		const bool atJunction = SameFloat (y0 + lip.cy, y0 + s.echinusJunctionHeight);
		const bool atTop = SameFloat (y0 + lip.cy, y0 + s.echinusHeight);
		bool onSample = false;
		for (int i = 1; i < s.curveSegments; ++i)
			onSample = onSample || SameFloat (y0 + lip.cy + lipRadius * std::sin (ja + (ta - ja) * i / s.curveSegments), y0 + lip.cy);
		const bool extremumVertex = lip.innerExtremum && !atJunction && !atTop;
		EXPECT_EQ (echinus.size (), static_cast<std::size_t> (s.curveSegments + 2 + (extremumVertex && !onSample ? 1 : 0)));
		bool extremum = false, decreasing = false;
		for (std::size_t k = 1; k < echinus.size (); ++k)
		{
			EXPECT_GT (echinus[k].y, echinus[k - 1].y) << k;
			if (echinus[k].s < echinus[k - 1].s)
				decreasing = true;
			if (k < 2)
				continue;
			EXPECT_NEAR (std::hypot (echinus[k].s - lip.cs, echinus[k].y - lip.cy), s.echinusLipRadius, kExact) << k;
			if (std::fabs (echinus[k].s - (lip.cs + s.echinusLipRadius)) <= kExact && std::fabs (echinus[k].y - lip.cy) <= kExact)
				extremum = true;
		}
		EXPECT_EQ (extremum, extremumVertex);
		EXPECT_EQ (decreasing, lip.innerExtremum && !atTop);
		replaced += lip.innerExtremum && extremumVertex && onSample ? 1 : 0;
		skippedAtJunction += lip.innerExtremum && atJunction ? 1 : 0;
		skippedAtTop += lip.innerExtremum && atTop ? 1 : 0;

		// Abaque : emprise du maillage en X et en Y. Elle egale le demi-cote de
		// l'abaque a condition que ce demi-cote majore le rayon de tous les autres
		// anneaux, fut (entasis comprise) et annelets ; toutes les variantes de
		// Variants () satisfont cette condition, que la validation n'impose pas.
		const MeshValidationReport r = ValidateMesh (*m);
		for (int axis = 0; axis < 2; ++axis)
		{
			EXPECT_NEAR (r.bbMax[axis], R1 + s.abacusProjection, tol) << axis;
			EXPECT_NEAR (r.bbMin[axis], -(R1 + s.abacusProjection), tol) << axis;
		}
	}
	// Chaque regle du point le plus saillant confondu en float est exercee.
	EXPECT_GE (replaced, 1);
	EXPECT_GE (skippedAtJunction, 1);
	EXPECT_GE (skippedAtTop, 1);
}

// G11 : levre de tres grand rayon, que la validation accepte jusqu'a
// kGreekDoricMaxLength. Un point de l'arc est a moins de sa fleche de la corde ;
// la fleche vaut 4,9e-7 pour R = 1e3 (controle positif) et au plus 5e-18 pour
// R >= 1e14 (demi-corde 0,0313). Tolerance ajoutee, 1e-12 : quelques ulp de cotes
// de l'ordre de 5 (1e-15), avec une marge de 1000, et neuf ordres de grandeur sous
// le millimetre.
TEST (TEST_cgarchi_classical_greek_doric_column, a_lip_of_very_large_radius_stays_on_its_chord_and_rises)
{
	constexpr double kChordTol = 1e-12;
	// Corde inclinee (tete plus saillante que la jonction : aucun point plus
	// saillant que la tete) et corde verticale (point le plus saillant a
	// mi-hauteur, confondu en float avec le sommet du milieu, qu'il remplace) :
	// curveSegments + 2 sommets dans les deux cas.
	for (const double radius : {1e3, 1e14, 1e16, kGreekDoricMaxLength})
	for (const bool vertical : {false, true})
	{
		SCOPED_TRACE (radius);
		SCOPED_TRACE (vertical ? "corde verticale" : "corde inclinee");
		GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
		s.echinusLipRadius = radius;
		if (vertical)
			s.echinusTopProjection = s.echinusJunctionProjection;
		ColumnProfile profile;
		std::unique_ptr<Mesh> m;
		try
		{
			m = BuildGreekDoricColumnMesh (s, &profile);
		}
		catch (const std::exception &e)
		{
			ADD_FAILURE () << "refuse : " << e.what ();
			continue;
		}
		std::size_t echinusIndex = 0;
		while (profile.members[echinusIndex].name != "echine")
			++echinusIndex;
		const double y0 = profile.members[echinusIndex].y0, R1 = 0.5 * s.upperDiameter;
		std::vector<ProfilePoint> echinus;
		for (const ProfileVertex &v : profile.vertices)
			if (v.member == static_cast<int> (echinusIndex))
				echinus.push_back ({v.ring.radius - R1, v.y - y0});
		EXPECT_EQ (echinus.size (), static_cast<std::size_t> (s.curveSegments + 2));
		const double js = s.echinusJunctionProjection, jy = s.echinusJunctionHeight;
		const double ds = s.echinusTopProjection - js, dy = s.echinusHeight - jy, length = std::hypot (ds, dy);
		// Fleche de l'arc, majorant de la distance d'un point de l'arc a sa corde.
		const double sagitta = radius - std::sqrt ((radius - 0.5 * length) * (radius + 0.5 * length));
		for (std::size_t k = 1; k < echinus.size (); ++k)
		{
			EXPECT_GT (echinus[k].y, echinus[k - 1].y) << k;
			if (k < 2)
				continue;
			const double es = echinus[k].s - js, ey = echinus[k].y - jy;
			EXPECT_NEAR ((ds * ey - dy * es) / length, 0.0, sagitta + kChordTol) << k;
			const double along = (ds * es + dy * ey) / length;
			EXPECT_GT (along, 0.0) << k;
			EXPECT_LE (along, length + kChordTol) << k;
		}
		EXPECT_NEAR (ComputeGreekDoricColumnHalfExtents (s).overall, R1 + s.abacusProjection, 1e-12);
		EXPECT_TRUE (ValidateMesh (*m).IsValid ());
	}
}

// G10 : gorgerin cannele au rayon superieur, rainure circulaire sous le fond des
// cannelures.
TEST (TEST_cgarchi_classical_greek_doric_column, the_neck_is_fluted_at_the_upper_radius_and_the_groove_lies_below_the_flutes)
{
	std::vector<std::pair<std::string, GreekDoricColumnSettings>> all = Variants ();
	for (const auto &b : BoundaryVariants ())
		all.push_back (b);
	std::size_t necks = 0, grooves = 0;
	for (const auto &[name, s] : all)
	{
		SCOPED_TRACE (name);
		const double tol = Tol (s);
		const double R1 = 0.5 * s.upperDiameter;
		const double shaftHeight = ExpectedShaftHeight (s);
		ColumnProfile profile;
		const std::unique_ptr<Mesh> m = Build (s, &profile);

		if (s.fluteCount > 0)
		{
			ASSERT_TRUE (profile.flutes.has_value ());
			EXPECT_NEAR (profile.flutes->headBottom, shaftHeight + s.hypotrachelionHeight, tol);
		}
		if (s.fluteCount > 0 && s.hypotrachelionHeight > s.grooveHeight)
		{
			++necks;
			std::size_t vertices = 0;
			for (const ProfileVertex &v : profile.vertices)
				if (v.member >= 0 && profile.members[static_cast<std::size_t> (v.member)].name == "gorgerin")
				{
					++vertices;
					EXPECT_EQ (v.ring.shape, RingShape::Fluted);
					EXPECT_EQ (v.ring.radius, R1);
					EXPECT_EQ (v.ring.flutes, s.fluteCount);
					EXPECT_EQ (v.ring.section, s.fluteSection);
					EXPECT_NEAR (v.ring.halfAngle, kPi / s.fluteCount, 1e-15);
				}
			EXPECT_GE (vertices, 1u);
			// Maillage, sommet du gorgerin : autant de sommets au rayon superieur
			// que de cannelures, les aretes.
			std::size_t atR1 = 0;
			for (const Point &p : RingAt (*m, shaftHeight + s.hypotrachelionHeight, tol))
				if (std::fabs (p.Radius () - R1) <= tol)
					++atR1;
			EXPECT_EQ (atR1, static_cast<std::size_t> (s.fluteCount));
		}
		if (s.grooveHeight > 0.0)
		{
			++grooves;
			const double grooveRadius = R1 - s.grooveDepth;
			if (s.fluteCount > 0)
				EXPECT_LT (grooveRadius, R1 * FluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio) - tol);
			// Aux deux cotes de la rainure : un sommet au rayon de la rainure a
			// chaque angle du tour, et tout autre sommet plus loin de l'axe.
			for (const double z : {shaftHeight, shaftHeight + s.grooveHeight})
			{
				SCOPED_TRACE (z);
				std::vector<double> angles;
				std::size_t groovePoints = 0;
				for (const Point &p : RingAt (*m, z, tol))
				{
					angles.push_back (p.Angle ());
					if (std::fabs (p.Radius () - grooveRadius) <= tol)
						++groovePoints;
					else
						EXPECT_GT (p.Radius (), grooveRadius + tol) << p.Angle ();
				}
				EXPECT_GT (groovePoints, 0u);
				EXPECT_EQ (groovePoints, DistinctAngles (angles));
			}
		}
		// Fonction publique du fond des cannelures, contre l'oracle du test.
		if (s.fluteCount > 0)
			EXPECT_NEAR (GreekDoricFluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio),
			             FluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio), 1e-15);
	}
	EXPECT_GE (necks, 1u);
	EXPECT_GE (grooves, 1u);
	// Garde : beta = pi / N n'existe pas sans cannelure.
	for (const int count : {0, -4})
	{
		SCOPED_TRACE (count);
		try
		{
			GreekDoricFluteBottomRatio (count, FluteSection::ThreeCentred, 0.19);
			ADD_FAILURE () << "aucun refus";
		}
		catch (const std::invalid_argument &e)
		{
			EXPECT_NE (std::string (e.what ()).find ("'fluteCount'"), std::string::npos) << e.what ();
		}
	}
}

// ---------------------------------------------------------------------------
//  Maillage
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_greek_doric_column, the_mesh_is_closed_consistently_oriented_and_of_positive_volume)
{
	std::vector<std::pair<std::string, GreekDoricColumnSettings>> all = Variants ();
	for (const auto &b : BoundaryVariants ())
		all.push_back (b);
	for (const auto &[name, s] : all)
	{
		SCOPED_TRACE (name);
		const std::unique_ptr<Mesh> m = Build (s);
		const MeshValidationReport r = ValidateMesh (*m);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.Count (MeshDefect::BoundaryEdge), 0u);
		EXPECT_EQ (r.Count (MeshDefect::InconsistentOrientation), 0u);
		EXPECT_EQ (r.Count (MeshDefect::InvertedShell), 0u);
		EXPECT_EQ (r.Count (MeshDefect::SelfIntersection), 0u);
		EXPECT_EQ (r.components, 1u);
		EXPECT_GT (r.signedVolume, 0.0);

		// Encadrement : fut entre le cylindre du fond des cannelures au plus
		// petit rayon et le cylindre du plus grand rayon ; gorgerin entre le
		// cylindre du plus petit de ses rayons (rainure ou fond des cannelures)
		// et celui du rayon superieur ; annelets, echine et abaque entre le
		// cylindre du rayon superieur et la somme de leurs cylindres (ou prisme
		// pour l'abaque) exterieurs.
		const double H = ExpectedShaftHeight (s);
		const double R1 = 0.5 * s.upperDiameter;
		const double rMax = 0.5 * std::max (s.lowerDiameter, s.upperDiameter) + s.entasisSagitta;
		const double bottomRatio = s.fluteCount > 0 ? FluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio) : 1.0;
		const double rMin = 0.5 * std::min (s.lowerDiameter, s.upperDiameter) * bottomRatio;
		const double rNeck = s.grooveHeight > 0.0 ? std::min (R1 - s.grooveDepth, R1 * bottomRatio) : R1 * bottomRatio;
		double annulets = 0.0;
		for (const GreekDoricAnnulet &a : s.annulets)
			annulets += a.height * (R1 + a.projection) * (R1 + a.projection);
		const double echinusOuter = R1 + LipOfSettings (s).maxProjection;
		const double capitalUpper = kPi * (s.hypotrachelionHeight * R1 * R1 + annulets + s.echinusHeight * echinusOuter * echinusOuter) +
		                            4.0 * (R1 + s.abacusProjection) * (R1 + s.abacusProjection) * s.abacusHeight;
		// Le tour a au moins 8 angles, espaces d'au plus pi/4 : chaque anneau
		// contient le cercle de son plus petit rayon reduit de cos(pi/8).
		const double c = std::cos (kPi / 8.0);
		const double lower = kPi * (rMin * c) * (rMin * c) * H + kPi * (rNeck * c) * (rNeck * c) * s.hypotrachelionHeight +
		                     kPi * (R1 * c) * (R1 * c) * (ExpectedCapitalHeight (s) - s.hypotrachelionHeight);
		const double upper = kPi * rMax * rMax * H + capitalUpper;
		EXPECT_GT (r.signedVolume, lower);
		EXPECT_LT (r.signedVolume, upper);
	}
}

// ---------------------------------------------------------------------------
//  Domaines
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_greek_doric_column, invalid_settings_are_refused_naming_the_field)
{
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	const double inf = std::numeric_limits<double>::infinity ();
	using Edit = std::function<void (GreekDoricColumnSettings &)>;
	struct Case { std::string field; Edit edit; };
	// Chapiteau reduit a l'echine et a l'abaque, pour les cas d'absorption.
	auto bareCapital = [] (GreekDoricColumnSettings &s) {
		s.hypotrachelionHeight = 0.0;
		s.grooveHeight = 0.0;
		s.grooveDepth = 0.0;
		s.annulets.clear ();
	};
	const std::vector<Case> cases = {
		{"height", [] (GreekDoricColumnSettings &s) { s.height = 0.0; }},
		{"height", [&] (GreekDoricColumnSettings &s) { s.height = nan; }},
		{"height", [] (GreekDoricColumnSettings &s) { s.height = 0.5; }},   // sous le chapiteau
		{"lowerDiameter", [] (GreekDoricColumnSettings &s) { s.lowerDiameter = -1.07; }},
		{"lowerDiameter", [&] (GreekDoricColumnSettings &s) { s.lowerDiameter = inf; }},
		{"upperDiameter", [] (GreekDoricColumnSettings &s) { s.upperDiameter = 0.0; }},
		{"upperDiameter", [&] (GreekDoricColumnSettings &s) { s.upperDiameter = nan; }},
		{"entasisSagitta", [] (GreekDoricColumnSettings &s) { s.entasisSagitta = -0.013; }},
		{"entasisSagitta", [&] (GreekDoricColumnSettings &s) { s.entasisSagitta = nan; }},
		{"entasisHeight", [] (GreekDoricColumnSettings &s) { s.entasisHeight = 0.0; }},
		{"entasisHeight", [] (GreekDoricColumnSettings &s) { s.entasisHeight = 5.0; }},   // au-dessus du fut
		{"entasisHeight", [&] (GreekDoricColumnSettings &s) { s.entasisHeight = nan; }},
		{"entasisHeight", [] (GreekDoricColumnSettings &s) { s.entasisHeight = 1e-46; }},   // 0 en float
		{"entasisHeight", [] (GreekDoricColumnSettings &s) { s.entasisHeight = ExpectedShaftHeight (s) * (1.0 - 1e-9); }},   // sommet du fut en float
		{"height", [] (GreekDoricColumnSettings &s) { s.height = 1e31; }},   // hors de la plage des float
		{"annulets[0].projection", [] (GreekDoricColumnSettings &s) { s.annulets[0].projection = 1e-39; }},   // sous-normal en float
		{"grooveHeight", [] (GreekDoricColumnSettings &s) { s.height = 1e9; }},   // absorbee a la cote 1e9
		{"hypotrachelionHeight", [] (GreekDoricColumnSettings &s) {
			 s.height = 1e9;
			 s.grooveHeight = 0.0;
			 s.grooveDepth = 0.0;
		 }},
		{"annulets[0].height", [&] (GreekDoricColumnSettings &s) {
			 s.height = 1e9;
			 bareCapital (s);
			 s.annulets = {{0.0113, 0.023}};
		 }},
		{"echinusJunctionHeight", [&] (GreekDoricColumnSettings &s) {
			 s.height = 1e9;
			 bareCapital (s);
		 }},
		{"echinusHeight", [&] (GreekDoricColumnSettings &s) {   // levre de 0,06 absorbee a la cote 1e9
			 s.height = 1e9;
			 bareCapital (s);
			 s.echinusJunctionHeight = 500.0;
			 s.echinusHeight = 500.06;
		 }},
		{"abacusHeight", [&] (GreekDoricColumnSettings &s) {
			 s.height = 1e9;
			 bareCapital (s);
			 s.echinusJunctionHeight = 500.0;
			 s.echinusHeight = 1000.0;
			 s.echinusLipRadius = 300.0;   // levre jusqu'a 134,4
			 s.abacusProjection = 1000.0;
		 }},
		{"fluteCount", [] (GreekDoricColumnSettings &s) { s.fluteCount = 18; }},
		{"fluteCount", [] (GreekDoricColumnSettings &s) { s.fluteCount = -4; }},
		{"fluteCount", [] (GreekDoricColumnSettings &s) { s.fluteCount = kGreekDoricMaxFlutes + 4; }},
		{"fluteCount", [] (GreekDoricColumnSettings &s) { s.fluteCount = 4; s.fluteSection = FluteSection::Semicircle; }},   // fond sur l'axe
		{"fluteSection", [] (GreekDoricColumnSettings &s) { s.fluteSection = static_cast<FluteSection> (7); }},
		{"fluteDepthRatio", [] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s, 0.0); }},
		{"fluteDepthRatio", [] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s, -0.19); }},
		{"fluteDepthRatio", [] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s, 0.5); }},
		{"fluteDepthRatio", [&] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s); s.fluteDepthRatio = nan; }},
		{"fluteDepthRatio", [&] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s); s.fluteDepthRatio = inf; }},
		// Non etoilee : seuil 0,2839 a 20 cannelures, 0,1472 a 4.
		{"fluteDepthRatio", [] (GreekDoricColumnSettings &s) { ThreeCentredFlutes (s, 0.29); }},
		{"fluteDepthRatio", [] (GreekDoricColumnSettings &s) { s.fluteCount = 4; ThreeCentredFlutes (s, 0.15); }},
		{"hypotrachelionHeight", [] (GreekDoricColumnSettings &s) { s.hypotrachelionHeight = -0.097; }},
		{"hypotrachelionHeight", [&] (GreekDoricColumnSettings &s) { s.hypotrachelionHeight = nan; }},
		{"grooveHeight", [] (GreekDoricColumnSettings &s) { s.grooveHeight = 0.0971; }},   // plus haute que le gorgerin
		{"grooveHeight", [] (GreekDoricColumnSettings &s) { s.grooveHeight = -0.0071; }},
		{"grooveDepth", [] (GreekDoricColumnSettings &s) { s.grooveHeight = 0.0; }},   // profondeur sans rainure
		{"grooveDepth", [] (GreekDoricColumnSettings &s) { s.grooveDepth = 0.0; }},    // rainure sans profondeur
		{"grooveDepth", [] (GreekDoricColumnSettings &s) { s.fluteCount = 0; s.grooveDepth = 0.415; }},   // fond sur l'axe
		{"grooveDepth", [&] (GreekDoricColumnSettings &s) { s.grooveDepth = inf; }},
		{"grooveDepth", [] (GreekDoricColumnSettings &s) { s.grooveDepth = 0.0219; }},   // au-dessus du fond des cannelures (0,0225)
		{"grooveDepth", [] (GreekDoricColumnSettings &s) { s.fluteSection = FluteSection::Semicircle; }},   // B : fond a 0,0700
		{"annulets", [] (GreekDoricColumnSettings &s) {
			 s.annulets.assign (static_cast<std::size_t> (kGreekDoricMaxAnnulets) + 1, {0.0031, 0.017});
		 }},
		{"annulets[1].height", [] (GreekDoricColumnSettings &s) { s.annulets[1].height = 0.0; }},
		{"annulets[2].height", [&] (GreekDoricColumnSettings &s) { s.annulets[2].height = nan; }},
		{"annulets[2].projection", [] (GreekDoricColumnSettings &s) { s.annulets[2].projection = -0.049; }},
		{"annulets[1].projection", [&] (GreekDoricColumnSettings &s) { s.annulets[1].projection = inf; }},
		{"echinusHeight", [] (GreekDoricColumnSettings &s) { s.echinusHeight = 0.0; }},
		{"echinusJunctionHeight", [] (GreekDoricColumnSettings &s) { s.echinusJunctionHeight = 0.0; }},
		{"echinusJunctionHeight", [] (GreekDoricColumnSettings &s) { s.echinusJunctionHeight = 0.27; }},   // a la tete
		{"echinusJunctionHeight", [] (GreekDoricColumnSettings &s) { s.echinusJunctionHeight = 0.31; }},
		{"echinusJunctionHeight", [&] (GreekDoricColumnSettings &s) { s.echinusJunctionHeight = nan; }},
		{"echinusFootProjection", [] (GreekDoricColumnSettings &s) { s.echinusFootProjection = -0.061; }},
		{"echinusJunctionProjection", [] (GreekDoricColumnSettings &s) { s.echinusJunctionProjection = -0.205; }},
		{"echinusTopProjection", [] (GreekDoricColumnSettings &s) { s.echinusTopProjection = -0.223; }},
		{"echinusTopProjection", [&] (GreekDoricColumnSettings &s) { s.echinusTopProjection = inf; }},
		{"echinusLipRadius", [] (GreekDoricColumnSettings &s) { s.echinusLipRadius = 0.0; }},
		{"echinusLipRadius", [&] (GreekDoricColumnSettings &s) { s.echinusLipRadius = nan; }},
		{"echinusLipRadius", [] (GreekDoricColumnSettings &s) { s.echinusLipRadius = 0.03; }},   // sous la demi-corde, 0,0313
		{"echinusLipRadius", [] (GreekDoricColumnSettings &s) {   // centre plus saillant que la jonction
			 s.echinusJunctionHeight = 0.265;
			 s.echinusJunctionProjection = 0.1;
			 s.echinusLipRadius = 0.07;
		 }},
		{"echinusLipRadius", [] (GreekDoricColumnSettings &s) { s.echinusTopProjection = 0.24; s.echinusLipRadius = 0.04; }},   // centre a 0,2054
		{"abacusHeight", [&] (GreekDoricColumnSettings &s) { s.abacusHeight = inf; }},
		{"abacusProjection", [] (GreekDoricColumnSettings &s) { s.abacusProjection = 0.2; }},     // < tete de l'echine
		{"abacusProjection", [] (GreekDoricColumnSettings &s) { s.abacusProjection = 0.225; }},   // entre la tete et la levre (0,2274)
		{"curveSegments", [] (GreekDoricColumnSettings &s) { s.curveSegments = 0; }},
		{"curveSegments", [] (GreekDoricColumnSettings &s) { s.curveSegments = kGreekDoricMaxCurveSegments + 1; }},
		{"shaftSegments", [] (GreekDoricColumnSettings &s) { s.shaftSegments = 1; }},
		{"shaftSegments", [] (GreekDoricColumnSettings &s) { s.shaftSegments = kGreekDoricMaxShaftSegments + 1; }},
		{"fluteSamples", [] (GreekDoricColumnSettings &s) { s.fluteSamples = 0; }},
		{"fluteSamples", [] (GreekDoricColumnSettings &s) { s.fluteSamples = kGreekDoricMaxFluteSamples + 1; }},
		{"radialSegments", [] (GreekDoricColumnSettings &s) { s.radialSegments = 7; }},
		{"radialSegments", [] (GreekDoricColumnSettings &s) { s.radialSegments = kGreekDoricMaxRadialSegments + 1; }},
	};
	for (const Case &c : cases)
	{
		GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
		c.edit (s);
		ColumnProfile profile;
		profile.height = -1.0;
		try
		{
			BuildGreekDoricColumnMesh (s, &profile);
			ADD_FAILURE () << c.field << " : accepte";
		}
		catch (const std::invalid_argument &e)
		{
			EXPECT_NE (std::string (e.what ()).find ("'" + c.field + "'"), std::string::npos) << c.field << " : " << e.what ();
		}
		EXPECT_EQ (profile.height, -1.0) << c.field << " : profil ecrit avant le refus";
	}
}

// ---------------------------------------------------------------------------
//  Emprise horizontale
// ---------------------------------------------------------------------------

namespace {

// Reglages acceptes dont un anneau autre que l'abaque est le plus large (plan 3
// section 12.3), avec la demi-etendue attendue, calculee ici depuis les
// reglages : rayon aux aretes R, et R cos (pi / N) pour un anneau cannele.
struct OutOfAbacus
{
	std::string name;
	GreekDoricColumnSettings settings;
	double overall = 0.0;
};

std::vector<OutOfAbacus> OutOfAbacusVariants ()
{
	std::vector<OutOfAbacus> v;
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	NarrowCapital (s, 0.01);
	v.push_back ({"pied plus large que l'abaque", s, 0.5 * s.lowerDiameter * std::cos (kPi / s.fluteCount)});
	s.fluteCount = 0;
	v.push_back ({"pied plus large que l'abaque, fut lisse", s, 0.5 * s.lowerDiameter});
	s = SyntheticGreekDoricColumnSettings ();
	s.annulets[1].projection = 0.35;
	v.push_back ({"annelet en saillie de 0,35", s, 0.5 * s.upperDiameter + 0.35});
	s = SyntheticGreekDoricColumnSettings ();
	s.lowerDiameter = s.upperDiameter = 0.83;
	s.entasisSagitta = 0.05;
	NarrowCapital (s, 0.02);
	v.push_back ({"fut cylindrique a entasis", s, (0.5 * s.upperDiameter + s.entasisSagitta) * std::cos (kPi / s.fluteCount)});
	return v;
}

}  // namespace

// E1 : la valeur exacte du maillage, et non un majorant. Tolerance : les
// positions du maillage sont en float.
TEST (TEST_cgarchi_classical_greek_doric_column, the_half_extents_are_those_of_the_mesh)
{
	std::vector<std::pair<std::string, GreekDoricColumnSettings>> all = Variants ();
	for (const auto &b : BoundaryVariants ())
		all.push_back (b);
	for (const OutOfAbacus &o : OutOfAbacusVariants ())
		all.push_back ({o.name, o.settings});
	for (const auto &[name, s] : all)
	{
		SCOPED_TRACE (name);
		const GreekDoricColumnHalfExtents e = ComputeGreekDoricColumnHalfExtents (s);
		const std::unique_ptr<Mesh> m = Build (s);
		double overall[2] = {0.0, 0.0}, foot[2] = {0.0, 0.0}, largest = 0.0;
		for (const Point &p : Points (*m))
		{
			const double c[2] = {std::fabs (p.x), std::fabs (p.y)};
			for (int axis = 0; axis < 2; ++axis)
			{
				overall[axis] = std::max (overall[axis], c[axis]);
				if (p.z == 0.0)
					foot[axis] = std::max (foot[axis], c[axis]);
			}
			largest = std::max ({largest, c[0], c[1], std::fabs (p.z)});
		}
		const double tol = 4.0 * FLT_EPSILON * largest;
		for (int axis = 0; axis < 2; ++axis)
		{
			EXPECT_NEAR (e.overall, overall[axis], tol) << axis;
			EXPECT_NEAR (e.foot, foot[axis], tol) << axis;
		}
	}
}

// E2 : l'emprise n'est celle de l'abaque que sous condition (D3-13).
TEST (TEST_cgarchi_classical_greek_doric_column, the_half_extent_is_not_the_abacus_when_the_shaft_or_an_annulet_projects_beyond_it)
{
	for (const OutOfAbacus &o : OutOfAbacusVariants ())
	{
		SCOPED_TRACE (o.name);
		const GreekDoricColumnHalfExtents e = ComputeGreekDoricColumnHalfExtents (o.settings);
		const double abacus = 0.5 * o.settings.upperDiameter + o.settings.abacusProjection;
		EXPECT_GT (e.overall, abacus + 1e-3);
		EXPECT_NEAR (e.overall, o.overall, 1e-12 * o.overall);
	}
	const GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	const double abacus = 0.5 * s.upperDiameter + s.abacusProjection;
	EXPECT_NEAR (ComputeGreekDoricColumnHalfExtents (s).overall, abacus, 1e-12 * abacus);
}

// E3
TEST (TEST_cgarchi_classical_greek_doric_column, the_half_extents_refuse_invalid_settings_naming_the_field)
{
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	using Edit = std::function<void (GreekDoricColumnSettings &)>;
	const std::vector<std::pair<std::string, Edit>> cases = {
		{"height", [] (GreekDoricColumnSettings &s) { s.height = 0.0; }},
		{"lowerDiameter", [&] (GreekDoricColumnSettings &s) { s.lowerDiameter = nan; }},
		{"abacusProjection", [] (GreekDoricColumnSettings &s) { s.abacusProjection = 0.2; }},
		{"fluteCount", [] (GreekDoricColumnSettings &s) { s.fluteCount = 18; }},
		{"grooveHeight", [] (GreekDoricColumnSettings &s) { s.height = 1e9; }},
	};
	for (const auto &[field, edit] : cases)
	{
		GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
		edit (s);
		std::string mesh, extents;
		try
		{
			BuildGreekDoricColumnMesh (s);
		}
		catch (const std::invalid_argument &e)
		{
			mesh = e.what ();
		}
		try
		{
			ComputeGreekDoricColumnHalfExtents (s);
			ADD_FAILURE () << field << " : accepte";
		}
		catch (const std::invalid_argument &e)
		{
			extents = e.what ();
		}
		EXPECT_NE (extents.find ("'" + field + "'"), std::string::npos) << field << " : " << extents;
		EXPECT_EQ (extents, mesh) << field;
	}
}

// ---------------------------------------------------------------------------
//  Determinisme
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_greek_doric_column, two_identical_builds_are_equal_bit_for_bit)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		ColumnProfile pa, pb;
		const std::unique_ptr<Mesh> a = Build (s, &pa);
		const std::unique_ptr<Mesh> b = Build (s, &pb);
		ASSERT_EQ (a->GetNVertices (), b->GetNVertices ());
		ASSERT_EQ (a->GetNFaces (), b->GetNFaces ());
		for (unsigned v = 0; v < a->GetNVertices (); ++v)
		{
			float va[3], vb[3];
			a->GetVertex (v, va);
			b->GetVertex (v, vb);
			ASSERT_EQ (std::memcmp (va, vb, sizeof va), 0) << v;
		}
		for (unsigned f = 0; f < a->GetNFaces (); ++f)
			for (unsigned k = 0; k < 3; ++k)
				ASSERT_EQ (a->GetFaceVertex (f, k), b->GetFaceVertex (f, k)) << f;
		ASSERT_EQ (pa.vertices.size (), pb.vertices.size ());
		for (std::size_t k = 0; k < pa.vertices.size (); ++k)
		{
			EXPECT_EQ (std::memcmp (&pa.vertices[k].y, &pb.vertices[k].y, sizeof (double)), 0) << k;
			EXPECT_EQ (std::memcmp (&pa.vertices[k].ring.radius, &pb.vertices[k].ring.radius, sizeof (double)), 0) << k;
		}
	}
}
