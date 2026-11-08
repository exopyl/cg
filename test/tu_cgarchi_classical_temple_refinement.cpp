#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_greek_pediment_detail.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_temple.h>
#include <cgarchi/classical_temple_refinement.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Raffinements du temple (lot R0a) : champ de Coons, cisaillement, stations,
//  pieces a stations
// ===========================================================================
//
// Reglages d'essai : SyntheticRefinementSettings et le temple synthetique a
// fronton (greek_doric_test_support.h), valeurs synthetiques qui ne decrivent
// aucun monument (D3-4).
//
// Contraction FMA : le champ et le cisaillement entre les bouts sont
// contractables ; ils ne se comparent ici qu'a une tolerance. Les egalites au
// bit pres ne portent que sur les identites du cisaillement (pied inchange,
// sommet de l'abaque deplace de i), sur des copies et sur des comptes.

using namespace cgarchi::classical;
using cgarchi_test::SyntheticRefinementSettings;

namespace {

double FloatUlp ()
{
	return static_cast<double> (std::nextafter (8.0f, 9.0f) - 8.0f);
}

std::string InvalidOf (const std::function<void ()> &call)
{
	try
	{
		call ();
	}
	catch (const std::invalid_argument &e)
	{
		return e.what ();
	}
	return "";
}

std::size_t Inside (const std::vector<double> &s, double lo, double hi)
{
	return static_cast<std::size_t> (std::count_if (s.begin (), s.end (), [&] (double v) { return lo < v && v < hi; }));
}

// Sous-maillage des faces [first, first + count) et de leurs sommets.
std::unique_ptr<Mesh> SubMesh (const Mesh &m, unsigned firstVertex, unsigned vertexCount, unsigned firstFace, unsigned faceCount)
{
	std::vector<float> v (m.GetVertices ().begin () + 3 * firstVertex, m.GetVertices ().begin () + 3 * (firstVertex + vertexCount));
	std::vector<unsigned> f;
	for (unsigned q = firstFace; q < firstFace + faceCount; ++q)
		for (unsigned k = 0; k < 3; ++k)
			f.push_back (static_cast<unsigned> (m.GetFaceVertex (q, k)) - firstVertex);
	auto out = std::make_unique<Mesh> ();
	out->SetVertices (vertexCount, v.data ());
	out->SetFaces (static_cast<unsigned> (f.size () / 3), 3, f.data ());
	return out;
}

std::size_t DefectCount (const MeshValidationReport &r)
{
	std::size_t n = 0;
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		n += r.counts[kind];
	return n;
}

// Stations du temple synthetique a fronton, deplacees hors des coordonnees de
// l'anneau, des degres, du polygone rampant et du faite (sous-ensemble de
// StructuralCoordinates, interne au temple).
RefinementStations SyntheticStations (TempleLayout &layout)
{
	const TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	layout = LayoutTemple (t);
	std::vector<double> fx {0.0}, fy;
	for (const KrepisBox &b : layout.krepis.steps)
	{
		fx.insert (fx.end (), {b.halfWidth, -b.halfWidth});
		fy.insert (fy.end (), {b.halfDepth, -b.halfDepth});
	}
	for (const RingSectionPoint &q : GreekEntablatureSection (*t.entablature))
	{
		fx.insert (fx.end (), {layout.peristyle.halfWidth + q.u, -(layout.peristyle.halfWidth + q.u)});
		fy.insert (fy.end (), {layout.peristyle.halfDepth + q.u, -(layout.peristyle.halfDepth + q.u)});
	}
	for (const RingSectionPoint &q : t.pediment->rakingSection)
	{
		const double d = layout.peristyle.halfDepth + (t.pediment->tympanumFace + q.u);
		fy.insert (fy.end (), {d, -d});
	}
	return DisplaceRefinementStations (*layout.stations, fx, fy);
}

}  // namespace

// RF1 : TempleRefinementLift est le carreau de Coons des quatre paraboles de
// bord : 0 exactement aux quatre angles du stylobate ; la fleche de chaque bord
// au milieu de ce bord, a 1e-15 relatif ; (s_R + s_L) / 2 + (s_F + s_B) / 2 au
// centre ; hors du stylobate, la formule ecrite ici, a 1e-12 ; le miroir X -> -X
// echange s_R et s_L.
TEST (TEST_cgarchi_classical_temple_refinement, the_lift_is_a_coons_patch_of_four_parabolas_zero_at_the_stylobate_corners)
{
	const TempleRefinementSettings r = SyntheticRefinementSettings ();
	const double a = 3.29, b = 3.98;
	for (const double sx : {-1.0, 1.0})
		for (const double sy : {-1.0, 1.0})
			EXPECT_EQ (TempleRefinementLift (r, a, b, sx * a, sy * b), 0.0) << sx << " " << sy;
	EXPECT_NEAR (TempleRefinementLift (r, a, b, a, 0.0), r.sagittaRight, 1e-15 * r.sagittaRight);
	EXPECT_NEAR (TempleRefinementLift (r, a, b, -a, 0.0), r.sagittaLeft, 1e-15 * r.sagittaLeft);
	EXPECT_NEAR (TempleRefinementLift (r, a, b, 0.0, -b), r.sagittaFront, 1e-15 * r.sagittaFront);
	EXPECT_NEAR (TempleRefinementLift (r, a, b, 0.0, b), r.sagittaBack, 1e-15 * r.sagittaBack);
	const double centre = (r.sagittaRight + r.sagittaLeft) / 2.0 + (r.sagittaFront + r.sagittaBack) / 2.0;
	EXPECT_NEAR (TempleRefinementLift (r, a, b, 0.0, 0.0), centre, 1e-15 * centre);
	for (const auto &[x, y] : {std::pair<double, double> {4.13, -4.4}, {-3.9, 1.7}, {0.37, 4.2}, {2.2, -1.1}})
	{
		const double u = x / a, v = y / b;
		const double formula = 0.5 * (1.0 + u) * r.sagittaRight * (1.0 - v * v) + 0.5 * (1.0 - u) * r.sagittaLeft * (1.0 - v * v) +
		                       0.5 * (1.0 - v) * r.sagittaFront * (1.0 - u * u) + 0.5 * (1.0 + v) * r.sagittaBack * (1.0 - u * u);
		EXPECT_NEAR (TempleRefinementLift (r, a, b, x, y), formula, 1e-12) << x << " " << y;
		TempleRefinementSettings m = r;
		std::swap (m.sagittaRight, m.sagittaLeft);
		EXPECT_NEAR (TempleRefinementLift (m, a, b, -x, y), TempleRefinementLift (r, a, b, x, y), 1e-15) << x << " " << y;
	}
	EXPECT_LT (TempleRefinementLift (r, a, b, 4.13, 4.4), 0.0) << "prolongement negatif au-dela du stylobate";
}

// RF2 : les placements portent l'inclinaison vers l'interieur, perpendiculaire a
// la rangee (Front +Y, Back -Y, Right -X, Left +X), sur les deux axes a l'angle,
// au bit pres. Champ nul (fleches 0), i = 0,0237 : sommets du pied de chaque
// colonne au bit pres ceux du temple sans raffinement ; sommets du dessus de
// l'abaque a float ((double (v) + t) + i) au bit pres ; ailleurs, i z_l / H a
// 1e-6.
TEST (TEST_cgarchi_classical_temple_refinement, columns_lean_inward_by_a_shear_that_keeps_the_foot)
{
	TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	const double i = t.refinement->inclination;
	const TempleLayout l = LayoutTemple (t);
	std::size_t corners = 0;
	for (const TemplePlacement &p : l.columns)
	{
		double ex = 0.0, ey = 0.0;
		if (p.corner)
		{
			ex = p.x > 0.0 ? -i : i;
			ey = p.y > 0.0 ? -i : i;
			++corners;
		}
		else if (p.address.side == PeristyleSide::Front)
			ey = i;
		else if (p.address.side == PeristyleSide::Back)
			ey = -i;
		else if (p.address.side == PeristyleSide::Right)
			ex = -i;
		else
			ex = i;
		EXPECT_EQ (p.inclinationX, ex) << p.x << " " << p.y;
		EXPECT_EQ (p.inclinationY, ey) << p.x << " " << p.y;
	}
	EXPECT_EQ (corners, 4u);

	t.refinement->sagittaFront = t.refinement->sagittaRight = t.refinement->sagittaBack = t.refinement->sagittaLeft = 0.0;
	TempleLayout lr;
	std::vector<TempleMeshPart> parts;
	const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &lr, &parts);
	const std::unique_ptr<Mesh> tour = BuildGreekDoricColumnMesh (t.column), cornerTour = BuildGreekDoricColumnMesh (t.cornerColumn);
	std::size_t feet = 0, tops = 0, middles = 0;
	for (std::size_t c = 0; c < lr.columns.size (); ++c)
	{
		const TemplePlacement &p = lr.columns[c];
		const TempleMeshPart &part = parts[lr.krepis.steps.size () + c];
		const Mesh &local = p.corner ? *cornerTour : *tour;
		ASSERT_EQ (part.vertexCount, local.GetNVertices ());
		const float H = static_cast<float> (p.corner ? t.cornerColumn.height : t.column.height);
		for (unsigned k = 0; k < part.vertexCount; ++k)
		{
			float v[3], q[3];
			local.GetVertex (k, v);
			m->GetVertex (part.firstVertex + k, q);
			const double bx = static_cast<double> (v[0]) + p.x, by = static_cast<double> (v[1]) + p.y;
			EXPECT_EQ (q[2], static_cast<float> (static_cast<double> (v[2]) + p.z));
			if (v[2] == 0.0f)
			{
				++feet;
				EXPECT_EQ (q[0], static_cast<float> (bx));
				EXPECT_EQ (q[1], static_cast<float> (by));
			}
			else if (v[2] == H)
			{
				++tops;
				EXPECT_EQ (q[0], static_cast<float> (bx + p.inclinationX));
				EXPECT_EQ (q[1], static_cast<float> (by + p.inclinationY));
			}
			else
			{
				++middles;
				const double t01 = static_cast<double> (v[2]) / static_cast<double> (H);
				EXPECT_NEAR (q[0], bx + p.inclinationX * t01, 1e-6);
				EXPECT_NEAR (q[1], by + p.inclinationY * t01, 1e-6);
			}
		}
	}
	EXPECT_GT (feet, 0u);
	EXPECT_GT (tops, 0u);
	EXPECT_GT (middles, 0u);
}

// RF3 : N = ceil (len / p) segments par axe, len = 2 L (degre le plus bas),
// p = h sqrt (4 e / s) ; un cas ou len / p vaut exactement 4 ; S' aux milieux de
// S (bouts -L et L compris) ; une coordonnee interdite posee sur une station
// nominale la fait deplacer de plus de p / 16, dans +-p / 4, les autres
// stations inchangees ; une fenetre sans place : repli, puis refus nomme sous
// le plancher float ; seconde famille (registre RF5) : blocs, contrainte dure
// sur S, repli hors des blocs, refus nomme sans morceau.
TEST (TEST_cgarchi_classical_temple_refinement, stations_have_closed_form_counts_and_avoid_structural_coordinates)
{
	KrepisSettings k;
	k.stylobateHalfWidth = 3.29;
	k.stylobateHalfDepth = 3.98;
	k.stylobateHeight = 0.37;
	TempleRefinementSettings r;
	r.sagittaFront = r.sagittaRight = r.sagittaBack = r.sagittaLeft = 0.0009765625;   // 2^-10
	r.chordDeviation = 0.00006103515625;                                                // 2^-14 : p = h / 2
	RefinementStations s = NominalRefinementStations (r, LayoutKrepis (k));
	EXPECT_EQ (s.pitchX, 3.29 * 0.5);
	EXPECT_EQ (s.x.size (), 3u) << "len / p = 4 exactement";
	EXPECT_EQ (s.xLower.size (), 4u);
	EXPECT_EQ (s.y.size (), 3u);
	EXPECT_EQ (s.yLower.size (), 4u);

	const TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	const KrepisLayout kl = LayoutKrepis (t.krepis);
	s = NominalRefinementStations (*t.refinement, kl);
	for (const bool alongX : {true, false})
	{
		const double L = alongX ? kl.steps.back ().halfWidth : kl.steps.back ().halfDepth;
		const double h = alongX ? kl.steps.front ().halfWidth : kl.steps.front ().halfDepth;
		// Courbure effective : parabole de l'axe et terme croise ramene a l'axe.
		const double a = kl.steps.front ().halfWidth, b = kl.steps.front ().halfDepth;
		const double sag = alongX ? std::max (t.refinement->sagittaFront, t.refinement->sagittaBack) +
		                                std::fabs (t.refinement->sagittaRight - t.refinement->sagittaLeft) * (a / b)
		                          : std::max (t.refinement->sagittaRight, t.refinement->sagittaLeft) +
		                                std::fabs (t.refinement->sagittaFront - t.refinement->sagittaBack) * (b / a);
		const double p = h * std::sqrt (4.0 * t.refinement->chordDeviation / sag);
		const std::size_t n = static_cast<std::size_t> (std::ceil (2.0 * L / p));
		const std::vector<double> &upper = alongX ? s.x : s.y, &lower = alongX ? s.xLower : s.yLower;
		ASSERT_EQ (upper.size (), n - 1) << alongX;
		ASSERT_EQ (lower.size (), n) << alongX;
		std::vector<double> ends {-L};
		ends.insert (ends.end (), upper.begin (), upper.end ());
		ends.push_back (L);
		for (std::size_t j = 0; j < n; ++j)
			EXPECT_NEAR (lower[j], 0.5 * (ends[j] + ends[j + 1]), 1e-12) << alongX << " " << j;
	}
	RecordProperty ("synthetic_segments_x", static_cast<int> (s.x.size () + 1));

	const double p = s.pitchX, target = s.x[3];
	const RefinementStations d = DisplaceRefinementStations (s, {target}, {});
	for (std::size_t j = 0; j < s.x.size (); ++j)
		if (j == 3)
		{
			EXPECT_GT (std::fabs (d.x[j] - target), p / 16.0);
			EXPECT_LE (std::fabs (d.x[j] - target), p / 4.0);
		}
		else
			EXPECT_EQ (d.x[j], s.x[j]) << j;
	for (std::size_t j = 0; j < s.xLower.size (); ++j)
		EXPECT_GT (std::fabs (d.xLower[j] - d.x[std::min (j, d.x.size () - 1)]), 0.0) << j;
	// Interdites plus serrees que 2 p / 16 : repli total, au milieu du plus grand
	// intervalle libre de la fenetre (p / 128 des interdites, espacees de p / 64).
	std::vector<double> dense;
	for (int j = -40; j <= 40; ++j)
		dense.push_back (target + (p * j) / 64.0);
	const RefinementStations packed = DisplaceRefinementStations (s, dense, {});
	EXPECT_LE (std::fabs (packed.x[3] - target), p / 4.0);
	double nearest = 1e300;
	for (const double f : dense)
		nearest = std::min (nearest, std::fabs (packed.x[3] - f));
	EXPECT_NEAR (nearest, p / 128.0, 1e-12);
	// Plancher float (8 FLT_EPSILON fois la plus grande coordonnee) : interdites
	// espacees de 1e-6 autour d'une station en 1, pas 1e-5 (fenetre 2,5e-6) ; le
	// meilleur point est a 5e-7 d'elles, sous le plancher 9,5e-7 : refus nomme.
	RefinementStations tight;
	tight.x = {1.0};
	tight.pitchX = 1e-5;
	std::vector<double> comb;
	for (int j = -10; j <= 10; ++j)
		comb.push_back (1.0 + 1e-6 * j);
	const std::string floorMsg = InvalidOf ([&] { DisplaceRefinementStations (tight, comb, {}); });
	EXPECT_EQ (floorMsg.rfind ("Raffinement : 'refinement.chordDeviation' ne laisse a une station aucune place", 0), 0u) << floorMsg;
	// Plancher dur du repli des stations (registre RF5) : kRefinementMaxTolerance.
	// Station en 0, pas 1 (fenetre 0,25) ; interdites espacees de 1,5e-3 sur
	// toute la fenetre : meilleur point a 7,5e-4, au-dessus du plancher float,
	// sous kRefinementMaxTolerance : refus nomme. Espacees de 2,5e-3 : 1,25e-3,
	// accepte.
	RefinementStations lone;
	lone.x = {0.0};
	lone.pitchX = 1.0;
	std::vector<double> close, apart;
	for (int j = -200; j <= 200; ++j)
	{
		close.push_back (0.00075 + 1.5e-3 * j);
		apart.push_back (0.00125 + 2.5e-3 * j);
	}
	const std::string toleranceMsg = InvalidOf ([&] { DisplaceRefinementStations (lone, close, {}); });
	EXPECT_EQ (toleranceMsg.rfind ("Raffinement : 'refinement.chordDeviation' ne laisse a une station aucune place a plus de kRefinementMaxTolerance", 0), 0u)
	    << toleranceMsg;
	const RefinementStations spaced = DisplaceRefinementStations (lone, apart, {});
	double gapToFirst = 1e300;
	for (const double f : apart)
		gapToFirst = std::min (gapToFirst, std::fabs (spaced.x[0] - f));
	EXPECT_GT (gapToFirst, kRefinementMaxTolerance);

	// Seconde famille (registre RF5) : blocs fermes [at - d, at + d] fusionnes,
	// ecart propre a chaque coordonnee ; contrainte dure sur S, S' inchangee.
	// Station en 0, pas 1 : at = 0,001, d = 0,002, bloc [-0,001 ; 0,003],
	// candidats -0,003 et 0,005 : -0,003.
	EXPECT_EQ (LongestClearanceBlock ({{0.01, 0.002}, {0.0, 0.002}, {0.003, 0.002}}), (0.003 + 0.002) - (0.0 - 0.002));
	EXPECT_EQ (LongestClearanceBlock ({{0.0, 0.002}, {0.003, 0.0005}}), 0.004) << "bloc large qui contient un etroit";
	EXPECT_EQ (LongestClearanceBlock ({{0.0, 0.002}, {0.005, 0.004}}), (0.005 + 0.004) - (0.0 - 0.002));
	EXPECT_EQ (LongestClearanceBlock ({}), 0.0);
	EXPECT_EQ (kRefinementClearance, 2.0 * kRefinementMaxTolerance);
	RefinementStations one;
	one.x = {0.0};
	one.xLower = {-0.5, 0.5};
	one.pitchX = 1.0;
	StationClearance near;
	near.x = {{0.001, 0.002}};
	const RefinementStations cleared = DisplaceRefinementStations (one, {}, {}, near);
	EXPECT_EQ (cleared.x[0], (0.001 - 0.002) - 0.002);
	EXPECT_EQ (cleared.xLower, one.xLower);
	// Repli : premiere famille espacee de 1 / 64 (aucun candidat a plus de
	// 1 / 16), bloc [-0,2 ; 0,2] : le plus eloigne de la premiere famille dans
	// les morceaux [-0,25 ; -0,2[ et ]0,2 ; 0,25], milieu de -14 / 64 et
	// -13 / 64, le plus petit a egalite.
	std::vector<double> sixtyFourths;
	for (int j = -20; j <= 20; ++j)
		sixtyFourths.push_back (j / 64.0);
	StationClearance wide;
	wide.x = {{0.0, 0.2}};
	const RefinementStations aside = DisplaceRefinementStations (one, sixtyFourths, {}, wide);
	EXPECT_EQ (aside.x[0], 0.5 * (-14.0 / 64.0 + -13.0 / 64.0));
	// Bloc plus large que la fenetre (+-0,25) : refus nomme.
	wide.x = {{0.0, 0.3}};
	const std::string fullMsg = InvalidOf ([&] { DisplaceRefinementStations (one, {}, {}, wide); });
	EXPECT_EQ (fullMsg.rfind ("Raffinement : 'refinement.chordDeviation' ne laisse a une station aucune place a plus de 0.300000 des sommets", 0), 0u)
	    << fullMsg;
}

// RF4 : krepis a stations : par degre, (t_x + 2)(t_y + 2) + (b_x + 2)(b_y + 2)
// sommets et 2 (t_x + 1)(t_y + 1) + 2 (b_x + 1)(b_y + 1) + 2 (t_x + b_x + 2) +
// 2 (t_y + b_y + 2) triangles ; chaque degre seul valide, une composante, volume
// de sa boite a l'aire fois l'ulp ; aucun sommet du dessous d'un degre au (x, y)
// d'un sommet du dessus du degre inferieur.
TEST (TEST_cgarchi_classical_temple_refinement, krepis_with_stations_is_closed_and_keeps_its_volume)
{
	TempleLayout l;
	const RefinementStations s = SyntheticStations (l);
	const TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	KrepisLayout kl;
	std::vector<KrepisStepSize> sizes;
	const std::unique_ptr<Mesh> m = BuildKrepisMesh (t.krepis, s, &kl, &sizes);
	ASSERT_EQ (sizes.size (), kl.steps.size ());
	unsigned firstVertex = 0, firstFace = 0;
	std::vector<std::set<std::pair<float, float>>> tops, bottoms;
	for (std::size_t k = 0; k < kl.steps.size (); ++k)
	{
		const KrepisBox &b = kl.steps[k];
		const std::size_t tx = Inside (s.x, -b.halfWidth, b.halfWidth), ty = Inside (s.y, -b.halfDepth, b.halfDepth);
		const std::size_t bx = Inside (s.xLower, -b.halfWidth, b.halfWidth), by = Inside (s.yLower, -b.halfDepth, b.halfDepth);
		EXPECT_EQ (sizes[k].vertices, (tx + 2) * (ty + 2) + (bx + 2) * (by + 2)) << k;
		EXPECT_EQ (sizes[k].faces, 2 * (tx + 1) * (ty + 1) + 2 * (bx + 1) * (by + 1) + 2 * (tx + bx + 2) + 2 * (ty + by + 2)) << k;
		const std::unique_ptr<Mesh> step = SubMesh (*m, firstVertex, sizes[k].vertices, firstFace, sizes[k].faces);
		const MeshValidationReport r = ValidateMesh (*step);
		EXPECT_TRUE (r.IsValid ()) << k;
		EXPECT_EQ (r.components, 1u) << k;
		const double volume = 4.0 * b.halfWidth * b.halfDepth * (b.zTop - b.zBottom);
		EXPECT_NEAR (r.signedVolume, volume, r.area * FloatUlp ()) << k;
		std::set<std::pair<float, float>> top, bottom;
		for (unsigned v = 0; v < sizes[k].vertices; ++v)
		{
			float q[3];
			step->GetVertex (v, q);
			(q[2] == static_cast<float> (b.zTop) ? top : bottom).insert ({q[0], q[1]});
		}
		tops.push_back (top);
		bottoms.push_back (bottom);
		firstVertex += sizes[k].vertices;
		firstFace += sizes[k].faces;
	}
	EXPECT_EQ (firstVertex, m->GetNVertices ());
	EXPECT_EQ (firstFace, m->GetNFaces ());
	for (std::size_t k = 0; k + 1 < kl.steps.size (); ++k)
	{
		std::size_t shared = 0;
		for (const auto &xy : bottoms[k])
			shared += tops[k + 1].count (xy);
		EXPECT_EQ (shared, 0u) << "dessous du degre " << k << " et dessus du degre " << k + 1 << " partagent des noeuds";
	}
}

// RF5 : anneau a stations sur la section de SyntheticEntablatureSettings coupee
// par RefineRingSectionWidth : V = somme (4 + 2 (c_X + c_Y)), F = 2V ; valide,
// une composante ; volume de la section d'origine a l'aire fois l'ulp ; sans
// station, identique au bit pres a la fonction a quatre arguments ; une
// station a moins de la marge du coin d'une boucle inseree en est absente et
// presente dans sa voisine ; une station hors de toute boucle est ignoree ;
// refus nommes.
TEST (TEST_cgarchi_classical_temple_refinement, ring_with_stations_is_the_same_solid)
{
	TempleLayout l;
	const RefinementStations s = SyntheticStations (l);
	const std::vector<RingSectionPoint> section = GreekEntablatureSection (cgarchi_test::SyntheticEntablatureSettings ());
	std::vector<double> sectionU;
	for (const RingSectionPoint &q : section)
		sectionU.push_back (q.u);
	const std::vector<RingSectionPoint> refined = RefineRingSectionWidth (section, std::min (s.pitchX, s.pitchY), {}, sectionU);
	ASSERT_GT (refined.size (), section.size ());
	const double hx = l.peristyle.halfWidth, hy = l.peristyle.halfDepth, mx = s.pitchX / 16.0, my = s.pitchY / 16.0;
	std::size_t V = 0;
	for (const RingSectionPoint &q : refined)
		V += 4 + 2 * (Inside (s.x, -(hx + q.u - mx), hx + q.u - mx) + Inside (s.y, -(hy + q.u - my), hy + q.u - my));
	const std::unique_ptr<Mesh> ring = SweepRectangularRing (refined, hx, hy, 0.0, s.x, s.y, mx, my);
	EXPECT_EQ (ring->GetNVertices (), V);
	EXPECT_EQ (ring->GetNFaces (), 2 * V);
	const MeshValidationReport r = ValidateMesh (*ring);
	EXPECT_EQ (DefectCount (r), 0u);
	EXPECT_EQ (r.components, 1u);
	EXPECT_NEAR (r.signedVolume, RectangularRingVolume (section, hx, hy), r.area * FloatUlp ());
	RecordProperty ("ring_faces", static_cast<int> (ring->GetNFaces ()));

	// Sans station : la fonction a quatre arguments, au bit pres.
	const std::unique_ptr<Mesh> bare = SweepRectangularRing (refined, hx, hy, 0.0, {}, {}, mx, my);
	const std::unique_ptr<Mesh> plain = SweepRectangularRing (refined, hx, hy, 0.0);
	ASSERT_EQ (bare->GetNVertices (), plain->GetNVertices ());
	ASSERT_EQ (bare->GetNFaces (), plain->GetNFaces ());
	EXPECT_EQ (std::memcmp (bare->GetVertices ().data (), plain->GetVertices ().data (), bare->GetVertices ().size () * sizeof (float)), 0);
	bool sameFaces = true;
	for (unsigned f = 0; f < bare->GetNFaces (); ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && bare->GetFaceVertex (f, j) == plain->GetFaceVertex (f, j);
	EXPECT_TRUE (sameFaces);

	// Marge : une station a la moitie de la marge du coin d'une boucle inseree.
	std::size_t inserted = refined.size ();
	for (std::size_t i = 0; i < refined.size () && inserted == refined.size (); ++i)
		if (std::none_of (section.begin (), section.end (), [&] (const RingSectionPoint &q) { return q.u == refined[i].u && q.z == refined[i].z; }))
			inserted = i;
	ASSERT_LT (inserted, refined.size ());
	const std::size_t next = (inserted + 1) % refined.size(), prev = (inserted + refined.size () - 1) % refined.size ();
	const std::size_t wider = refined[next].u > refined[prev].u ? next : prev;
	ASSERT_GT (refined[wider].u - refined[inserted].u, mx);
	const double station = hx + refined[inserted].u - mx / 2.0;
	const std::unique_ptr<Mesh> probe = SweepRectangularRing (refined, hx, hy, 0.0, {station}, {}, mx, my);
	auto present = [&] (const RingSectionPoint &q) {
		const float x = static_cast<float> (station), y = static_cast<float> (-(hy + q.u)), z = static_cast<float> (q.z);
		for (unsigned v = 0; v < probe->GetNVertices (); ++v)
		{
			float p[3];
			probe->GetVertex (v, p);
			if (p[0] == x && p[1] == y && p[2] == z)
				return true;
		}
		return false;
	};
	EXPECT_FALSE (present (refined[inserted])) << "station dans la marge du coin gardee";
	EXPECT_TRUE (present (refined[wider])) << "station absente de la boucle voisine";
	EXPECT_EQ (DefectCount (ValidateMesh (*probe)), 0u);

	// Station hors de toute boucle : ignoree.
	const std::unique_ptr<Mesh> outside = SweepRectangularRing (refined, hx, hy, 0.0, {hx + 10.0}, {-(hy + 10.0)}, mx, my);
	EXPECT_EQ (outside->GetNVertices (), plain->GetNVertices ());

	// Refus.
	auto refusal = [&] (std::vector<double> sx, double marginX) {
		return InvalidOf ([&] { SweepRectangularRing (refined, hx, hy, 0.0, sx, {}, marginX, my); });
	};
	EXPECT_EQ (refusal ({std::nan ("")}, mx).rfind ("Anneau : station X 0 non finie", 0), 0u);
	EXPECT_EQ (refusal ({0.5, 0.5}, mx).rfind ("Anneau : stations X non strictement croissantes", 0), 0u);
	EXPECT_EQ (refusal ({0.5}, std::nan ("")).rfind ("Anneau : marge", 0), 0u);
	EXPECT_EQ (refusal ({0.5}, -1.0).rfind ("Anneau : marge", 0), 0u);
	const double corner = hx + refined[inserted].u;
	EXPECT_EQ (refusal ({corner - 1e-12}, 0.0).rfind ("Anneau : station confondue en float", 0), 0u);
	EXPECT_EQ (refusal ({0.5}, std::numeric_limits<double>::infinity ()), "");
}

// RF6 : fronton a stations : Lambda de 3n + P sommets et 6n - 4 + 2P
// triangles, P les stations de ]-foot[k], foot[k][ cumulees sur les n
// trajectoires (pieds lus par detail::PedimentFeet) ; comble de m stations dans
// ]-xTop, xTop[ et n_y abscisses en Y : (2m + 3) n_y sommets, 2 (2m + 3)
// (n_y - 1) + 2 (2m + 1) triangles ; pieces seules valides, volumes des
// formules de PG2 et PG3 a l'aire fois l'ulp ; sous-assemblage anneau a
// stations et fronton, sans champ, a la tolerance par defaut : recouvrements en
// L16, L17 et L18 seuls, 2 (m + 2) aretes non manifold (aretes haut-dos coupees
// aux stations), 0 sommet non manifold, 2 composantes, aucun autre defaut.
TEST (TEST_cgarchi_classical_temple_refinement, pediment_with_stations_keeps_its_shared_ridge_edges)
{
	TempleLayout l;
	const RefinementStations s = SyntheticStations (l);
	const TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	const GreekEntablatureSettings &e = *t.entablature;
	const GreekPedimentSettings &pd = *t.pediment;
	const std::vector<std::unique_ptr<Mesh>> pieces = BuildGreekPedimentPieceMeshes (pd, e, l.peristyle, l.entablatureFoot, s);
	ASSERT_EQ (pieces.size (), 3u);
	const detail::CheckedEntablature ce = detail::CheckEntablature (e);
	const std::vector<double> feet = detail::PedimentFeet (detail::CheckPediment (pd, ce));
	const std::size_t n = pd.rakingSection.size ();
	ASSERT_EQ (feet.size (), n);
	const double xTop = feet.back (), yT = l.peristyle.halfDepth + pd.tympanumFace;
	std::size_t stations = 0;
	for (const double f : feet)
		stations += Inside (s.x, -f, f);
	const std::size_t m = Inside (s.x, -xTop, xTop);
	ASSERT_GT (m, 2u) << "jeu sans station sur le haut du comble";
	ASSERT_LT (stations, n * m) << "jeu dont toutes les trajectoires ont les memes stations";
	double twiceArea = 0.0, momentH = 0.0;
	for (std::size_t k = 0; k < n; ++k)
	{
		const RingSectionPoint &a = pd.rakingSection[k], &b = pd.rakingSection[(k + 1) % n];
		twiceArea += a.u * b.z - b.u * a.z;
		momentH += (a.u * b.z - b.u * a.z) * (a.z + b.z) / 6.0;
	}
	const double scale = std::hypot (pd.rakingRun, 1.0) / pd.rakingRun;
	const double baseRise = pd.apexRise - pd.rakingSection.back ().z * scale;
	const double lambdaVolume = 2.0 * std::hypot (pd.rakingRun, 1.0) * (baseRise * 0.5 * twiceArea + scale * momentH);
	for (std::size_t k = 0; k < 2; ++k)
	{
		const Mesh &lambda = *pieces[k];
		EXPECT_EQ (lambda.GetNVertices (), 3 * n + stations) << k;
		EXPECT_EQ (lambda.GetNFaces (), 6 * n - 4 + 2 * stations) << k;
		const MeshValidationReport r = ValidateMesh (lambda);
		EXPECT_TRUE (r.IsValid ()) << k;
		EXPECT_EQ (r.components, 1u) << k;
		EXPECT_GT (r.signedVolume, 0.0) << k;
		EXPECT_NEAR (r.signedVolume, lambdaVolume, r.area * FloatUlp ()) << k;
	}
	// Le Lambda Back a stations est le miroir du Front : y oppose au bit pres, faces retournees.
	ASSERT_EQ (pieces[0]->GetNVertices (), pieces[1]->GetNVertices ());
	ASSERT_EQ (pieces[0]->GetNFaces (), pieces[1]->GetNFaces ());
	for (unsigned i = 0; i < pieces[0]->GetNVertices (); ++i)
	{
		float a[3], b[3];
		pieces[0]->GetVertex (i, a);
		pieces[1]->GetVertex (i, b);
		EXPECT_TRUE (a[0] == b[0] && a[1] == -b[1] && a[2] == b[2] && std::signbit (a[1]) != std::signbit (b[1])) << i;
	}
	for (unsigned f = 0; f < pieces[0]->GetNFaces (); ++f)
		EXPECT_TRUE (pieces[1]->GetFaceVertex (f, 0) == pieces[0]->GetFaceVertex (f, 0) &&
		             pieces[1]->GetFaceVertex (f, 1) == pieces[0]->GetFaceVertex (f, 2) &&
		             pieces[1]->GetFaceVertex (f, 2) == pieces[0]->GetFaceVertex (f, 1))
			<< f;
	const std::size_t ny = Inside (s.yLower, -yT, yT) + 2;
	const Mesh &roof = *pieces[2];
	EXPECT_EQ (roof.GetNVertices (), (2 * m + 3) * ny);
	EXPECT_EQ (roof.GetNFaces (), 2 * (2 * m + 3) * (ny - 1) + 2 * (2 * m + 1));
	const MeshValidationReport rr = ValidateMesh (roof);
	EXPECT_TRUE (rr.IsValid ());
	EXPECT_GT (rr.signedVolume, 0.0);
	EXPECT_NEAR (rr.signedVolume, pd.rakingRun * pd.apexRise * pd.apexRise * 2.0 * yT, rr.area * FloatUlp ());
	RecordProperty ("lambda_stations", static_cast<int> (stations));
	RecordProperty ("roof_stations", static_cast<int> (m));

	// Sous-assemblage anneau a stations (section d'origine) et fronton, sans champ.
	const std::unique_ptr<Mesh> ring = SweepRectangularRing (GreekEntablatureSection (e), l.peristyle.halfWidth, l.peristyle.halfDepth,
	                                                         l.entablatureFoot, s.x, s.y, s.pitchX / 16.0, s.pitchY / 16.0);
	std::vector<float> vertices;
	std::vector<unsigned> faces, owner;
	const Mesh *parts[] = {ring.get (), pieces[0].get (), pieces[1].get (), pieces[2].get ()};
	for (unsigned p = 0; p < 4; ++p)
	{
		const unsigned base = static_cast<unsigned> (vertices.size () / 3);
		vertices.insert (vertices.end (), parts[p]->GetVertices ().begin (), parts[p]->GetVertices ().end ());
		for (unsigned f = 0; f < parts[p]->GetNFaces (); ++f)
		{
			for (unsigned j = 0; j < 3; ++j)
				faces.push_back (base + static_cast<unsigned> (parts[p]->GetFaceVertex (f, j)));
			owner.push_back (p);
		}
	}
	Mesh sub;
	sub.SetVertices (static_cast<unsigned> (vertices.size () / 3), vertices.data ());
	sub.SetFaces (static_cast<unsigned> (faces.size () / 3), 3, faces.data ());
	MeshValidationOptions o;
	o.maxRecordsPerKind = sub.GetNFaces ();
	const MeshValidationReport r = ValidateMesh (sub, o);
	ASSERT_FALSE (r.selfIntersectionTruncated);
	const float zTop = static_cast<float> (l.entablatureFoot + GreekEntablatureBandBounds (e).back ());
	auto onPlane = [&] (unsigned f, int axis, float c) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			sub.GetVertex (static_cast<unsigned> (sub.GetFaceVertex (f, j)), q);
			if (q[axis] != c)
				return false;
		}
		return true;
	};
	std::size_t l16 = 0, l17 = 0, l18 = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection)
			continue;
		unsigned f0 = rec.faces[0], f1 = rec.faces[1];
		if (owner[f0] > owner[f1])
			std::swap (f0, f1);
		const float y = owner[f0] == 1 ? -static_cast<float> (yT) : static_cast<float> (yT);
		if (owner[f0] == 0 && owner[f1] == 3 && onPlane (f0, 2, zTop) && onPlane (f1, 2, zTop))
			++l16;
		else if ((owner[f0] == 1 || owner[f0] == 2) && owner[f1] == 3 && onPlane (f0, 1, y) && onPlane (f1, 1, y))
			++l17;
		else if (owner[f0] == 0 && (owner[f1] == 1 || owner[f1] == 2) && onPlane (f0, 2, zTop) && onPlane (f1, 2, zTop))
			++l18;
		else
			ADD_FAILURE () << "recouvrement hors liste : pieces " << owner[f0] << " et " << owner[f1];
	}
	EXPECT_GE (l16, 1u);
	EXPECT_GE (l17, 2u);
	EXPECT_GE (l18, 4u) << "une face de pied par bras et par facade";
	EXPECT_EQ (r.Count (MeshDefect::NonManifoldEdge), 2 * (m + 2));
	EXPECT_EQ (r.Count (MeshDefect::NonManifoldVertex), 0u);
	EXPECT_EQ (r.components, 2u);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection && static_cast<MeshDefect> (kind) != MeshDefect::NonManifoldEdge)
			EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
	RecordProperty ("l18_records", static_cast<int> (l18));
	RecordProperty ("pediment_nonmanifold_edges", static_cast<int> (r.Count (MeshDefect::NonManifoldEdge)));
}

// RF7 : refus de ValidateTempleRefinementSettings, prefixe "Raffinement : " et
// champ : NaN et -0,001 pour chaque fleche et pour l'inclinaison ;
// chordDeviation 0, -1, NaN ; plus de kRefinementMaxSegments segments, sans
// maillage. Une travee d'angle au-dessus de la somme des demi-etendues mais sous
// cette somme plus i : "Temple : 'refinement'".
TEST (TEST_cgarchi_classical_temple_refinement, invalid_refinement_settings_are_refused_naming_the_field)
{
	const TempleSettings t = cgarchi_test::SyntheticRefinedTemple ();
	const KrepisLayout kl = LayoutKrepis (t.krepis);
	const TempleRefinementSettings good = SyntheticRefinementSettings ();
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleRefinementSettings (good, kl); }), "");
	const std::pair<const char *, double TempleRefinementSettings::*> fields[] = {
	    {"refinement.sagittaFront", &TempleRefinementSettings::sagittaFront}, {"refinement.sagittaRight", &TempleRefinementSettings::sagittaRight},
	    {"refinement.sagittaBack", &TempleRefinementSettings::sagittaBack},   {"refinement.sagittaLeft", &TempleRefinementSettings::sagittaLeft},
	    {"refinement.inclination", &TempleRefinementSettings::inclination}};
	for (const auto &[name, member] : fields)
		for (const double bad : {std::nan (""), -0.001})
		{
			TempleRefinementSettings r = good;
			r.*member = bad;
			const std::string msg = InvalidOf ([&] { ValidateTempleRefinementSettings (r, kl); });
			EXPECT_EQ (msg.rfind (std::string ("Raffinement : '") + name + "'", 0), 0u) << name << " " << bad << " : " << msg;
		}
	for (const double bad : {0.0, -1.0, std::nan ("")})
	{
		TempleRefinementSettings r = good;
		r.chordDeviation = bad;
		const std::string msg = InvalidOf ([&] { ValidateTempleRefinementSettings (r, kl); });
		EXPECT_EQ (msg.rfind ("Raffinement : 'refinement.chordDeviation' doit etre fini et strictement positif", 0), 0u) << bad << " : " << msg;
	}
	// Borne basse : p / 16 >= kRefinementMaxTolerance.
	TempleRefinementSettings fine = good;
	fine.chordDeviation = 1e-8;
	const std::string lowMsg = InvalidOf ([&] { ValidateTempleRefinementSettings (fine, kl); });
	EXPECT_EQ (lowMsg.rfind ("Raffinement : 'refinement.chordDeviation' donne sur l'axe X un pas dont le seizieme", 0), 0u) << lowMsg;
	// Borne de segments, sur une krepis de 100 m de demi-cote (p ~ 0,0195, p / 16
	// au-dessus de la borne basse, 2 L / p ~ 1e4 segments).
	KrepisSettings big = t.krepis;
	big.stylobateHalfWidth = big.stylobateHalfDepth = 100.0;
	const KrepisLayout bigLayout = LayoutKrepis (big);
	fine.chordDeviation = 1.5e-10;
	const std::string msg = InvalidOf ([&] { ValidateTempleRefinementSettings (fine, bigLayout); });
	EXPECT_EQ (msg.rfind ("Raffinement : 'refinement.chordDeviation' donne plus de 4096 segments", 0), 0u) << msg;

	// TE4 elargie de |i|.
	TempleSettings te4 = cgarchi_test::SyntheticTempleSettings ();
	const double en = ComputeGreekDoricColumnHalfExtents (te4.column).overall, ec = ComputeGreekDoricColumnHalfExtents (te4.cornerColumn).overall;
	te4.peristyle.frontCornerSpacing = en + ec + good.inclination / 2.0;
	EXPECT_EQ (InvalidOf ([&] { LayoutTemple (te4); }), "");
	te4.refinement = good;
	const std::string te4msg = InvalidOf ([&] { LayoutTemple (te4); });
	EXPECT_EQ (te4msg.rfind ("Temple : 'refinement'", 0), 0u) << te4msg;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleSettings (te4); }), te4msg);

	// Majorant des faces des pieces a stations, avant tout maillage : 30 degres de
	// plus, e* 2e-7 (p ~ 0,024, au-dessus de la borne basse) : environ 1,9e7 faces
	// de grilles, chaque axe restant sous kRefinementMaxSegments.
	TempleSettings heavy = cgarchi_test::SyntheticRefinedTemple ();
	for (int k = 0; k < 30; ++k)
		heavy.krepis.lowerSteps.push_back ({0.05, 0.01, 0.01});
	heavy.refinement->chordDeviation = 2e-7;
	const KrepisLayout heavyKrepis = LayoutKrepis (heavy.krepis);
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleRefinementSettings (*heavy.refinement, heavyKrepis); }), "");
	const std::string heavyMsg = InvalidOf ([&] { LayoutTemple (heavy); });
	EXPECT_EQ (heavyMsg.rfind ("Raffinement : 'refinement.chordDeviation' donne plus de 16777216 faces", 0), 0u) << heavyMsg;

	// Bornes de Q3-99 (a2) : chordDeviation <= kRefinementMaxTolerance / 10 ;
	// calotte de l'abaque, 1,1 F2 <= kRefinementMaxTolerance / 2.
	EXPECT_EQ (kRefinementMaxTolerance, 1e-3);
	for (const double coarse : {7.3e-4, std::nextafter (1e-4, 1.0)})
	{
		TempleRefinementSettings r = good;
		r.chordDeviation = coarse;
		const std::string m = InvalidOf ([&] { ValidateTempleRefinementSettings (r, kl); });
		EXPECT_EQ (m.rfind ("Raffinement : 'refinement.chordDeviation' depasse kRefinementMaxTolerance / 10", 0), 0u) << coarse << " : " << m;
	}
	TempleRefinementSettings atBound = good;
	atBound.chordDeviation = 1e-4;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleRefinementSettings (atBound, kl); }), "");
	TempleSettings bent = cgarchi_test::SyntheticRefinedTemple ();
	for (double *s : {&bent.refinement->sagittaFront, &bent.refinement->sagittaRight, &bent.refinement->sagittaBack, &bent.refinement->sagittaLeft})
		*s *= 2.0;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleRefinementSettings (*bent.refinement, kl); }), "") << "la calotte est un refus du temple";
	const std::string bentMsg = InvalidOf ([&] { LayoutTemple (bent); });
	EXPECT_EQ (bentMsg.rfind ("Raffinement : 'refinement.sagittaFront', 'refinement.sagittaRight'", 0), 0u) << bentMsg;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleSettings (bent); }), bentMsg);
	EXPECT_EQ (InvalidOf ([&] { LayoutTemple (cgarchi_test::SyntheticRefinedTemple ()); }), "");
	// N2 : stylobate elargi, fleches egales a 0,0658 (|v_max| < 1) : la borne de
	// calotte compte (1 - v) min (s) ; refus.
	TempleSettings wide = cgarchi_test::SyntheticRefinedTemple ();
	wide.krepis.stylobateHalfWidth = 5.0;
	wide.krepis.stylobateHalfDepth = 5.8;
	wide.refinement->sagittaFront = wide.refinement->sagittaRight = wide.refinement->sagittaBack = wide.refinement->sagittaLeft = 0.0658;
	const std::string wideMsg = InvalidOf ([&] { LayoutTemple (wide); });
	EXPECT_EQ (wideMsg.rfind ("Raffinement : 'refinement.sagittaFront', 'refinement.sagittaRight'", 0), 0u) << wideMsg;
	// Borne basse calculee de la seconde famille : e* 1e-7 au synthetique, p / 16
	// au-dessus de kRefinementMaxTolerance et faces sous la borne, mais p_X / 2
	// (8,5e-3) sous l'emprise des sommets de pied et d'abaque (1,7e-2).
	TempleSettings dense = cgarchi_test::SyntheticRefinedTemple ();
	dense.refinement->chordDeviation = 1e-7;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleRefinementSettings (*dense.refinement, kl); }), "");
	const std::string denseMsg = InvalidOf ([&] { LayoutTemple (dense); });
	EXPECT_EQ (denseMsg.rfind ("Raffinement : 'refinement.chordDeviation' donne sur l'axe X une fenetre de station", 0), 0u) << denseMsg;
	EXPECT_EQ (InvalidOf ([&] { ValidateTempleSettings (dense); }), denseMsg);
	// Fronton presque plat : e* 2,2e-6 (p_X / 2 vers 4e-2) laisse passer la pente
	// 1 : 4 (ecart 8e-3 autour de chaque pied) ; a la pente 1 : 15 (montee 0,23,
	// pied du haut a 3,45), l'ecart de 3e-2 des pieds seul allonge le plus long
	// bloc ; le refus nomme aussi 'pediment.rakingRun'.
	TempleSettings steep = cgarchi_test::SyntheticRefinedTemple ();
	steep.refinement->chordDeviation = 2.2e-6;
	EXPECT_EQ (InvalidOf ([&] { LayoutTemple (steep); }), "");
	TempleSettings flat = steep;
	flat.pediment->rakingRun = 15.0;
	flat.pediment->apexRise = 0.23;
	const std::string flatMsg = InvalidOf ([&] { LayoutTemple (flat); });
	EXPECT_EQ (flatMsg.rfind ("Raffinement : 'refinement.chordDeviation' et 'pediment.rakingRun' donnent sur l'axe X une fenetre de station", 0), 0u)
	    << flatMsg;
}

// RF8 : RefineRingSectionWidth : N_e = max (1, ceil (w / p)) par arete
// horizontale, dont un cas w / p = 4 exactement ; aretes verticales et
// inclinees inchangees ; une arete protegee plus large que p inchangee ; sommets
// inseres dans leur fenetre, a plus de w / (16 N_e) des interdites, strictement
// ordonnes ; p = +infini : section inchangee ; refus de p NaN ou <= 0 ; une
// fenetre sans place : repli ; plancher a l'echelle de coordinateMagnitude,
// refus nomme en dessous (peigne de 2e-6).
TEST (TEST_cgarchi_classical_temple_refinement, ring_section_width_cuts_horizontal_edges_only)
{
	// Dessous de -0,5 a 0,5 (w = 1), face inclinee, dessus de 0,5 a -0,5, face verticale.
	const std::vector<RingSectionPoint> s = {{-0.5, 0.0}, {0.5, 0.0}, {0.6, 0.5}, {0.5, 1.0}, {-0.5, 1.0}};
	const std::vector<RingSectionPoint> r = RefineRingSectionWidth (s, 0.25, {}, {});
	ASSERT_EQ (r.size (), s.size () + 6) << "w / p = 4 : trois sommets par arete horizontale";
	EXPECT_EQ (r[0].u, -0.5);
	EXPECT_EQ (r[4].u, 0.5);
	EXPECT_EQ (r[5].u, 0.6);
	EXPECT_EQ (r[6].u, 0.5);
	EXPECT_EQ (r[10].u, -0.5);
	for (std::size_t k = 0; k < 3; ++k)
	{
		EXPECT_EQ (r[1 + k].z, 0.0);
		EXPECT_NEAR (r[1 + k].u, -0.5 + 0.25 * static_cast<double> (k + 1), 1e-15);
		EXPECT_EQ (r[7 + k].z, 1.0);
		EXPECT_NEAR (r[7 + k].u, 0.5 - 0.25 * static_cast<double> (k + 1), 1e-15);
	}
	const std::vector<RingSectionPoint> wide = RefineRingSectionWidth (s, 0.3, {}, {});
	EXPECT_EQ (wide.size (), s.size () + 6) << "ceil (1 / 0,3) = 4";

	// Arete protegee : le dessus, plus large que p, inchange.
	const std::vector<RingSectionPoint> prot = RefineRingSectionWidth (s, 0.25, {1.0}, {});
	EXPECT_EQ (prot.size (), s.size () + 3);
	EXPECT_EQ (prot[7].u, -0.5);

	// Interdite sur un sommet nominal : deplace dans sa fenetre.
	const std::vector<double> U = {0.0, -0.26};
	const std::vector<RingSectionPoint> moved = RefineRingSectionWidth (s, 0.25, {}, U);
	ASSERT_EQ (moved.size (), r.size ());
	const double step = 0.25;
	for (std::size_t k = 0; k < 3; ++k)
	{
		const double u = moved[1 + k].u, nominal = -0.5 + step * static_cast<double> (k + 1);
		EXPECT_LE (std::fabs (u - nominal), step / 4.0) << k;
		for (const double f : U)
			EXPECT_GT (std::fabs (u - f), step / 16.0) << k;
		EXPECT_LT (moved[k].u, u) << k;
	}
	EXPECT_NE (moved[2].u, 0.0);

	// p infini, refus, fenetre sans place.
	const std::vector<RingSectionPoint> same = RefineRingSectionWidth (s, std::numeric_limits<double>::infinity (), {}, {});
	ASSERT_EQ (same.size (), s.size ());
	for (const double bad : {std::nan (""), 0.0, -1.0})
		EXPECT_EQ (InvalidOf ([&] { RefineRingSectionWidth (s, bad, {}, {}); }).rfind ("Raffinement : 'pitch'", 0), 0u) << bad;
	// Interdites plus serrees que 2 pas / 16 autour du sommet nominal 0 : repli
	// total dans la fenetre (0,25 / 4), a 0,25 / 128 des interdites.
	std::vector<double> dense;
	for (int j = -30; j <= 30; ++j)
		dense.push_back ((0.25 * j) / 64.0);
	const std::vector<RingSectionPoint> packed = RefineRingSectionWidth (s, 0.25, {}, dense);
	ASSERT_EQ (packed.size (), r.size ());
	EXPECT_LE (std::fabs (packed[2].u), 0.25 / 4.0);
	double nearest = 1e300;
	for (const double f : dense)
		nearest = std::min (nearest, std::fabs (packed[2].u - f));
	EXPECT_NEAR (nearest, 0.25 / 128.0, 1e-12);

	// Plancher du repli a l'echelle des sommets construits : dessous de 0 a 0,5
	// (dessus protege), p = 0,25, un sommet nominal en 0,25 ; peigne d'interdites
	// espacees de 2e-6 sur toute sa fenetre : meilleur point a 1e-6 d'elles.
	// coordinateMagnitude 0 : plancher 8 FLT_EPSILON x 0,33 (3,1e-7), accepte ;
	// coordinateMagnitude 4 : plancher 3,8e-6, refus nomme.
	const std::vector<RingSectionPoint> band = {{0.0, 0.0}, {0.5, 0.0}, {0.5, 1.0}, {0.0, 1.0}};
	std::vector<double> fine;
	for (int j = -40000; j <= 40000; ++j)
		fine.push_back (0.25 + 2e-6 * j);
	const std::vector<RingSectionPoint> kept = RefineRingSectionWidth (band, 0.25, {1.0}, fine, 0.0);
	ASSERT_EQ (kept.size (), band.size () + 1);
	EXPECT_LE (std::fabs (kept[1].u - 0.25), 0.25 / 4.0);
	double gap = 1e300;
	for (const double f : fine)
		gap = std::min (gap, std::fabs (kept[1].u - f));
	EXPECT_NEAR (gap, 1e-6, 1e-9);
	const std::string combMsg = InvalidOf ([&] { RefineRingSectionWidth (band, 0.25, {1.0}, fine, 4.0); });
	EXPECT_EQ (combMsg.rfind ("Raffinement : 'refinement.chordDeviation' ne laisse a une station aucune place a plus du plancher float", 0), 0u)
	    << combMsg;
}

// RF9 : sans station, les constructeurs a stations rendent ceux d'origine, au bit
// pres, dans le meme binaire : krepis (16 sommets et 12 faces par degre), Lambda
// et comble, aussi avec des stations hors de ]-xTop, xTop[ ; et un raffinement nul (fleches et inclinaison nulles : aucune
// station) rend le temple sans raffinement, sommets, faces et parties.
TEST (TEST_cgarchi_classical_temple_refinement, empty_stations_reproduce_the_original_constructors_bit_for_bit)
{
	auto same = [] (const Mesh &a, const Mesh &b) {
		if (a.GetNVertices () != b.GetNVertices () || a.GetNFaces () != b.GetNFaces ())
			return false;
		if (std::memcmp (a.GetVertices ().data (), b.GetVertices ().data (), a.GetVertices ().size () * sizeof (float)) != 0)
			return false;
		for (unsigned f = 0; f < a.GetNFaces (); ++f)
			for (unsigned j = 0; j < 3; ++j)
				if (a.GetFaceVertex (f, j) != b.GetFaceVertex (f, j))
					return false;
		return true;
	};
	const TempleSettings t = cgarchi_test::SyntheticTempleWithPediment ();
	const RefinementStations none;
	std::vector<KrepisStepSize> sizes;
	EXPECT_TRUE (same (*BuildKrepisMesh (t.krepis, none, nullptr, &sizes), *BuildKrepisMesh (t.krepis))) << "krepis";
	ASSERT_EQ (sizes.size (), t.krepis.lowerSteps.size () + 1);
	for (const KrepisStepSize &s : sizes)
	{
		EXPECT_EQ (s.vertices, 16u);
		EXPECT_EQ (s.faces, 12u);
	}
	const TempleLayout l = LayoutTemple (t);
	const std::vector<std::unique_ptr<Mesh>> bare = BuildGreekPedimentPieceMeshes (*t.pediment, *t.entablature, l.peristyle, l.entablatureFoot, none);
	const std::vector<std::unique_ptr<Mesh>> plain = BuildGreekPedimentPieceMeshes (*t.pediment, *t.entablature, l.peristyle, l.entablatureFoot);
	ASSERT_EQ (bare.size (), plain.size ());
	for (std::size_t k = 0; k < bare.size (); ++k)
		EXPECT_TRUE (same (*bare[k], *plain[k])) << "piece du fronton " << k;
	// Stations au-dela du pied du haut (xTop = 3,4) et en deca de X_e = 3,53 :
	// aucune trajectoire ne les recoit, pieces d'origine au bit pres.
	RefinementStations beyond;
	beyond.x = {-3.5, 3.5};
	const std::vector<std::unique_ptr<Mesh>> outside =
		BuildGreekPedimentPieceMeshes (*t.pediment, *t.entablature, l.peristyle, l.entablatureFoot, beyond);
	ASSERT_EQ (outside.size (), plain.size ());
	for (std::size_t k = 0; k < outside.size (); ++k)
		EXPECT_TRUE (same (*outside[k], *plain[k])) << "piece du fronton " << k << ", stations hors des trajectoires";

	TempleSettings flat = t;
	flat.refinement = SyntheticRefinementSettings ();
	flat.refinement->sagittaFront = flat.refinement->sagittaRight = flat.refinement->sagittaBack = flat.refinement->sagittaLeft = 0.0;
	flat.refinement->inclination = 0.0;
	std::vector<TempleMeshPart> pf, pt;
	const std::unique_ptr<Mesh> mf = BuildTempleMesh (flat, nullptr, &pf), mt = BuildTempleMesh (t, nullptr, &pt);
	EXPECT_TRUE (same (*mf, *mt)) << "temple au raffinement nul";
	ASSERT_EQ (pf.size (), pt.size ());
	for (std::size_t k = 0; k < pf.size (); ++k)
	{
		EXPECT_EQ (pf[k].firstVertex, pt[k].firstVertex) << k;
		EXPECT_EQ (pf[k].vertexCount, pt[k].vertexCount) << k;
		EXPECT_EQ (pf[k].firstFace, pt[k].firstFace) << k;
		EXPECT_EQ (pf[k].faceCount, pt[k].faceCount) << k;
	}
}
