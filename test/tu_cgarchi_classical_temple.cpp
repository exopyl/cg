#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Temple : krepis et peristyle de colonnes doriques grecques, placements et
//  maillage
// ===========================================================================
//
// Reglages d'essai : SyntheticTempleSettings (greek_doric_test_support.h),
// valeurs synthetiques qui ne decrivent aucun monument (D3-4). Colonne d'angle
// et colonne courante y different, ainsi que les travees d'angle et courantes,
// en X et en Y. Les seuils des refus sont calcules ici depuis les demi-etendues
// de ComputeGreekDoricColumnHalfExtents, dont la justesse est portee par E1 et
// E2 (suite de la colonne) ; le cas (iv) de T4 les recalcule depuis les
// reglages.

using namespace cgarchi::classical;
using cgarchi_test::NarrowCapital;
using cgarchi_test::SyntheticGreekDoricColumnSettings;
using cgarchi_test::SyntheticEntablatureSettings;
using cgarchi_test::SyntheticTempleSettings;
using cgarchi_test::SyntheticTempleWithEntablature;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Stylobate ajuste aux axes d'angle, a `margin` au-dela de chacun.
void FitStylobate (TempleSettings &t, double margin = 0.61)
{
	const PeristyleLayout p = LayoutPeristyle (t.peristyle);
	t.krepis.stylobateHalfWidth = p.halfWidth + margin;
	t.krepis.stylobateHalfDepth = p.halfDepth + margin;
}

// Colonne fine : demi-etendue 0,17, tres inferieure a celle de la colonne
// synthetique (0,685).
GreekDoricColumnSettings SlenderColumn ()
{
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	s.lowerDiameter = 0.25;
	s.upperDiameter = 0.2;
	s.echinusJunctionProjection = 0.052;
	s.echinusTopProjection = 0.058;   // levre jusqu'a 0,0661
	s.abacusProjection = 0.07;
	return s;
}

// Colonne a l'annelet saillant : demi-etendue 0,765, superieure a celle de la
// colonne synthetique.
GreekDoricColumnSettings ProjectingAnnuletsColumn ()
{
	GreekDoricColumnSettings s = SyntheticGreekDoricColumnSettings ();
	s.annulets[1].projection = 0.35;
	return s;
}

std::string RefusalOf (const TempleSettings &t)
{
	std::string validate, layout;
	try
	{
		ValidateTempleSettings (t);
	}
	catch (const std::invalid_argument &e)
	{
		validate = e.what ();
	}
	try
	{
		LayoutTemple (t);
	}
	catch (const std::invalid_argument &e)
	{
		layout = e.what ();
	}
	EXPECT_EQ (validate, layout) << "ValidateTempleSettings et LayoutTemple divergent";
	return layout;
}

void ExpectRefused (const TempleSettings &t, const std::string &message)
{
	const std::string refusal = RefusalOf (t);
	EXPECT_NE (refusal.find (message), std::string::npos) << "attendu : " << message << " ; obtenu : '" << refusal << "'";
}

void ExpectAccepted (const TempleSettings &t)
{
	EXPECT_EQ (RefusalOf (t), "");
}

std::vector<std::pair<std::string, TempleSettings>> Variants ()
{
	std::vector<std::pair<std::string, TempleSettings>> v;
	v.push_back ({"synthetique", SyntheticTempleSettings ()});
	TempleSettings t = SyntheticTempleSettings ();
	t.peristyle = {2, 2, 1.9, 1.7, 1.9, 1.7};
	FitStylobate (t);
	v.push_back ({"2 x 2", t});
	t = SyntheticTempleSettings ();
	t.peristyle = {2, 7, 1.9, 1.7, 1.9, 1.61};
	FitStylobate (t);
	v.push_back ({"2 x 7", t});
	t = SyntheticTempleSettings ();
	t.peristyle.frontCornerSpacing = t.peristyle.frontSpacing;
	t.peristyle.flankCornerSpacing = t.peristyle.flankSpacing;
	FitStylobate (t);
	v.push_back ({"sans contraction", t});
	t = SyntheticTempleSettings ();
	t.cornerColumn = t.column;
	v.push_back ({"colonne d'angle identique", t});
	t = SyntheticTempleSettings ();
	t.krepis.lowerSteps.clear ();
	v.push_back ({"stylobate seul", t});
	t = SyntheticTempleSettings ();
	NarrowCapital (t.column, 0.01);
	v.push_back ({"colonne courante au pied plus large que l'abaque", t});
	return v;
}

void SmoothColumns (TempleSettings &t)
{
	for (GreekDoricColumnSettings *c : {&t.column, &t.cornerColumn})
	{
		c->fluteCount = 0;
		c->radialSegments = 16;
	}
}

// Jeu synthetique cannele, puis les autres variantes a fut lisse de 16
// segments : le cout de ValidateMesh suit le nombre de faces.
std::vector<std::pair<std::string, TempleSettings>> MeshVariants ()
{
	std::vector<std::pair<std::string, TempleSettings>> v = Variants ();
	for (std::size_t k = 1; k < v.size (); ++k)
	{
		SmoothColumns (v[k].second);
		v[k].first += ", fut lisse";
	}
	return v;
}

std::vector<float> Positions (const Mesh &m)
{
	return m.GetVertices ();
}

}  // namespace

// ---------------------------------------------------------------------------
//  Placements et adresses
// ---------------------------------------------------------------------------

// T1 (placements)
TEST (TEST_cgarchi_classical_temple, each_column_stands_on_the_stylobate_at_its_axis_with_its_own_settings)
{
	for (const auto &[name, t] : Variants ())
	{
		SCOPED_TRACE (name);
		const TempleLayout l = LayoutTemple (t);
		const PeristyleLayout p = LayoutPeristyle (t.peristyle);
		const KrepisLayout k = LayoutKrepis (t.krepis);
		EXPECT_EQ (l.peristyle.front, t.peristyle.front);
		EXPECT_EQ (l.peristyle.flank, t.peristyle.flank);
		EXPECT_EQ (l.krepis.steps.size (), t.krepis.lowerSteps.size () + 1);
		EXPECT_EQ (std::memcmp (&l.krepis.stylobateTop, &k.stylobateTop, sizeof (double)), 0);

		const GreekDoricColumnHalfExtents en = ComputeGreekDoricColumnHalfExtents (t.column);
		const GreekDoricColumnHalfExtents ec = ComputeGreekDoricColumnHalfExtents (t.cornerColumn);
		EXPECT_EQ (std::memcmp (&l.columnExtents, &en, sizeof en), 0);
		EXPECT_EQ (std::memcmp (&l.cornerColumnExtents, &ec, sizeof ec), 0);

		ASSERT_EQ (l.columns.size (), p.columns.size ());
		int corners = 0;
		for (std::size_t c = 0; c < l.columns.size (); ++c)
		{
			const TemplePlacement &pl = l.columns[c];
			EXPECT_EQ (std::memcmp (&pl.x, &p.columns[c].x, sizeof (double)), 0) << c;
			EXPECT_EQ (std::memcmp (&pl.y, &p.columns[c].y, sizeof (double)), 0) << c;
			EXPECT_EQ (std::memcmp (&pl.z, &k.stylobateTop, sizeof (double)), 0) << c;
			EXPECT_EQ (pl.corner, p.columns[c].corner) << c;
			corners += pl.corner ? 1 : 0;
		}
		EXPECT_EQ (corners, 4);
	}

	// Jeu synthetique, valeurs recalculees depuis les reglages : chaque jeu a son
	// propre pied, 1,09 a l'angle et 1,07 ailleurs, cannele a 20 cannelures.
	const TempleLayout l = LayoutTemple (SyntheticTempleSettings ());
	const double cosine = std::cos (kPi / 20.0);
	EXPECT_NEAR (l.cornerColumnExtents.foot, 0.5 * 1.09 * cosine, 1e-12);
	EXPECT_NEAR (l.columnExtents.foot, 0.5 * 1.07 * cosine, 1e-12);
	EXPECT_NEAR (l.krepis.stylobateTop, 0.37 + 0.29 + 0.31, 1e-12);
	EXPECT_NEAR (l.peristyle.halfWidth, 0.5 * 1.9 + 1.73, 1e-12);
	EXPECT_NEAR (l.peristyle.halfDepth, 1.7 + 1.61, 1e-12);

	// Maillage : chaque colonne pose au bit pres sur le dessus du stylobate ; la
	// demi-etendue de son pied, mesuree sur sa plage autour de son axe, est le
	// `foot` de son propre jeu de reglages.
	for (const auto &[name, t] : MeshVariants ())
	{
		SCOPED_TRACE (name);
		TempleLayout ml;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &ml, &parts);
		const std::vector<float> v = Positions (*m);
		const float top = static_cast<float> (ml.krepis.stylobateTop);
		const std::size_t steps = ml.krepis.steps.size ();
		ASSERT_EQ (parts.size (), steps + ml.columns.size ());
		for (std::size_t c = 0; c < ml.columns.size (); ++c)
		{
			const TempleMeshPart &part = parts[steps + c];
			const TemplePlacement &p = ml.columns[c];
			float zMin = std::numeric_limits<float>::infinity ();
			double foot[2] = {0.0, 0.0}, largest = 0.0;
			for (unsigned i = part.firstVertex; i < part.firstVertex + part.vertexCount; ++i)
			{
				const float *q = &v[3 * static_cast<std::size_t> (i)];
				zMin = std::min (zMin, q[2]);
				largest = std::max ({largest, std::fabs (static_cast<double> (q[0])), std::fabs (static_cast<double> (q[1]))});
				if (q[2] == top)
				{
					foot[0] = std::max (foot[0], std::fabs (q[0] - p.x));
					foot[1] = std::max (foot[1], std::fabs (q[1] - p.y));
				}
			}
			EXPECT_EQ (zMin, top) << c;
			const double expected = (p.corner ? ml.cornerColumnExtents : ml.columnExtents).foot;
			const double tol = 4.0 * FLT_EPSILON * largest;
			EXPECT_NEAR (foot[0], expected, tol) << c;
			EXPECT_NEAR (foot[1], expected, tol) << c;
		}
	}
}

// T2
TEST (TEST_cgarchi_classical_temple, each_element_has_a_stable_address_and_its_own_range_of_the_mesh)
{
	for (const auto &[name, t] : Variants ())
	{
		SCOPED_TRACE (name);
		const TempleLayout l = LayoutTemple (t);
		std::set<std::pair<int, int>> seen;
		int expectedRank = 0;
		for (std::size_t c = 0; c < l.columns.size (); ++c)
		{
			const TempleAddress &a = l.columns[c].address;
			EXPECT_EQ (a.element, TempleElement::Column) << c;
			EXPECT_EQ (a.side, l.peristyle.columns[c].side) << c;
			expectedRank = c > 0 && l.columns[c - 1].address.side == a.side ? expectedRank + 1 : 0;
			EXPECT_EQ (a.index, expectedRank) << c;
			EXPECT_EQ (a.index == 0, l.columns[c].corner) << c;
			EXPECT_TRUE (seen.insert ({static_cast<int> (a.side), a.index}).second) << c << " : adresse en double";
		}
		const int perSide[4] = {t.peristyle.front - 1, t.peristyle.flank - 1, t.peristyle.front - 1, t.peristyle.flank - 1};
		for (int side = 0; side < 4; ++side)
			EXPECT_EQ (seen.count ({side, perSide[side] - 1}), 1u) << side;
	}

	// Memes comptes, longueurs differentes : memes adresses.
	TempleSettings other = SyntheticTempleSettings ();
	other.peristyle.frontSpacing = 2.3;
	other.peristyle.flankCornerSpacing = 1.9;
	other.column.lowerDiameter = 1.13;
	other.cornerColumn.height = 6.1;
	FitStylobate (other, 0.83);
	other.krepis.lowerSteps[1].height = 0.53;
	const TempleLayout a = LayoutTemple (SyntheticTempleSettings ()), b = LayoutTemple (other);
	ASSERT_EQ (a.columns.size (), b.columns.size ());
	for (std::size_t c = 0; c < a.columns.size (); ++c)
	{
		EXPECT_EQ (a.columns[c].address.element, b.columns[c].address.element) << c;
		EXPECT_EQ (a.columns[c].address.side, b.columns[c].address.side) << c;
		EXPECT_EQ (a.columns[c].address.index, b.columns[c].address.index) << c;
	}

	// TempleAddress {} ne designe aucun element.
	EXPECT_EQ (TempleAddress {}.index, -1);

	// Plages du maillage : degres d'abord (contrat de K0 : 16 sommets et 12 faces
	// par degre), puis colonnes dans l'ordre des placements ; contigues, elles
	// couvrent le maillage. Chaque plage de colonne est le tour de son jeu,
	// translate : position = float (double (v) + translation) au bit pres.
	for (const auto &[name, t] : MeshVariants ())
	{
		SCOPED_TRACE (name);
		TempleLayout l;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		const std::unique_ptr<Mesh> krepis = BuildKrepisMesh (t.krepis);
		const std::unique_ptr<Mesh> towers[2] = {BuildGreekDoricColumnMesh (t.column), BuildGreekDoricColumnMesh (t.cornerColumn)};
		const std::size_t steps = l.krepis.steps.size ();
		ASSERT_EQ (parts.size (), steps + l.columns.size ());
		unsigned nextVertex = 0, nextFace = 0, krepisVertices = 0, krepisFaces = 0;
		for (std::size_t k = 0; k < parts.size (); ++k)
		{
			const TempleMeshPart &part = parts[k];
			EXPECT_EQ (part.firstVertex, nextVertex) << k;
			EXPECT_EQ (part.firstFace, nextFace) << k;
			nextVertex += part.vertexCount;
			nextFace += part.faceCount;
			EXPECT_NE (part.address.index, -1) << k;
			if (k < steps)
			{
				EXPECT_EQ (part.address.element, TempleElement::KrepisStep) << k;
				EXPECT_EQ (part.address.index, static_cast<int> (k)) << k;
				EXPECT_EQ (part.vertexCount, 16u) << k;
				EXPECT_EQ (part.faceCount, 12u) << k;
				krepisVertices += part.vertexCount;
				krepisFaces += part.faceCount;
				continue;
			}
			const TemplePlacement &p = l.columns[k - steps];
			EXPECT_EQ (part.address.element, TempleElement::Column) << k;
			EXPECT_EQ (part.address.side, p.address.side) << k;
			EXPECT_EQ (part.address.index, p.address.index) << k;
			const Mesh &tower = *towers[p.corner ? 1 : 0];
			ASSERT_EQ (part.vertexCount, tower.GetNVertices ()) << k;
			ASSERT_EQ (part.faceCount, tower.GetNFaces ()) << k;
			const std::vector<float> &tv = tower.GetVertices ();
			const std::vector<float> &mv = m->GetVertices ();
			const double translation[3] = {p.x, p.y, p.z};
			bool positions = true, faces = true;
			for (unsigned i = 0; positions && i < part.vertexCount; ++i)
				for (int d = 0; d < 3; ++d)
				{
					const float expected = static_cast<float> (static_cast<double> (tv[3 * i + d]) + translation[d]);
					const float actual = mv[3 * static_cast<std::size_t> (part.firstVertex + i) + d];
					positions = positions && std::memcmp (&expected, &actual, sizeof (float)) == 0;
				}
			for (unsigned f = 0; faces && f < part.faceCount; ++f)
				for (unsigned j = 0; j < 3; ++j)
					faces = faces && m->GetFaceVertex (part.firstFace + f, j) ==
					                     static_cast<int> (part.firstVertex) + tower.GetFaceVertex (f, j);
			EXPECT_TRUE (positions) << k << " : positions";
			EXPECT_TRUE (faces) << k << " : indices";
		}
		EXPECT_EQ (nextVertex, static_cast<unsigned> (m->GetNVertices ()));
		EXPECT_EQ (nextFace, static_cast<unsigned> (m->GetNFaces ()));
		EXPECT_EQ (krepisVertices, static_cast<unsigned> (krepis->GetNVertices ()));
		EXPECT_EQ (krepisFaces, static_cast<unsigned> (krepis->GetNFaces ()));
	}
}

// ---------------------------------------------------------------------------
//  Refus
// ---------------------------------------------------------------------------

// T4 : TE4, les sortes de paires, sur chaque axe. Les cas a egalite exacte
// prennent des rangees dont les ecarts places sont exacts : rangee de deux ou
// trois colonnes (axes +-s/2, ou -s, 0, s), ecart interieur d'une rangee de
// quatre ou cinq colonnes.
TEST (TEST_cgarchi_classical_temple, columns_closer_than_their_half_extents_are_refused)
{
	const double margin = 1e-3;

	// (i) Deux colonnes courantes voisines : s = 2 e_n refuse, au-dela accepte.
	{
		SCOPED_TRACE ("(i) colonnes courantes");
		const TempleSettings base = SyntheticTempleSettings ();
		const double en = ComputeGreekDoricColumnHalfExtents (base.column).overall;
		for (const bool front : {true, false})
		{
			TempleSettings t = base;
			double &s = front ? t.peristyle.frontSpacing : t.peristyle.flankSpacing;
			s = 2.0 * en;
			FitStylobate (t);
			ExpectRefused (t, front ? "Temple : 'peristyle.frontSpacing'" : "Temple : 'peristyle.flankSpacing'");
			s = 2.0 * en + margin;
			FitStylobate (t);
			ExpectAccepted (t);
		}
	}

	// (ii) Une colonne d'angle et sa voisine, e_c != e_n, rangee contractee :
	// l'ecart place vaut a a un arrondi pres ; refus juste sous e_c + e_n.
	{
		SCOPED_TRACE ("(ii) angle et voisine, rangee contractee");
		TempleSettings base = SyntheticTempleSettings ();
		base.cornerColumn = ProjectingAnnuletsColumn ();
		const double en = ComputeGreekDoricColumnHalfExtents (base.column).overall;
		const double ec = ComputeGreekDoricColumnHalfExtents (base.cornerColumn).overall;
		ASSERT_GT (ec, en + 0.05);
		for (const bool front : {true, false})
		{
			TempleSettings t = base;
			double &a = front ? t.peristyle.frontCornerSpacing : t.peristyle.flankCornerSpacing;
			a = ec + en - 1e-9;
			FitStylobate (t);
			ExpectRefused (t, front ? "Temple : 'peristyle.frontCornerSpacing'" : "Temple : 'peristyle.flankCornerSpacing'");
			a = ec + en + margin;
			FitStylobate (t);
			ExpectAccepted (t);
		}
	}

	// (ii-b) Rangee de trois colonnes, axes -s, 0, s : ses deux travees sont
	// d'angle ; s = e_c + e_n refuse a egalite exacte, au-dela accepte. Colonne
	// d'angle fine (e_c != e_n), puis identique a la courante.
	{
		SCOPED_TRACE ("(ii-b) rangee de trois colonnes");
		for (const bool slender : {true, false})
			for (const bool front : {true, false})
			{
				TempleSettings t = SyntheticTempleSettings ();
				if (slender)
					t.cornerColumn = SlenderColumn ();
				else
					t.cornerColumn = t.column;
				const double en = ComputeGreekDoricColumnHalfExtents (t.column).overall;
				const double ec = ComputeGreekDoricColumnHalfExtents (t.cornerColumn).overall;
				const double s = ec + en;
				t.peristyle = front ? PeristyleSettings {3, 5, s, 1.7, s, 1.61} : PeristyleSettings {4, 3, 1.9, s, 1.73, s};
				FitStylobate (t);
				ExpectRefused (t, front ? "Temple : 'peristyle.frontCornerSpacing'" : "Temple : 'peristyle.flankCornerSpacing'");
				(front ? t.peristyle.frontSpacing : t.peristyle.flankSpacing) = s + margin;
				(front ? t.peristyle.frontCornerSpacing : t.peristyle.flankCornerSpacing) = s + margin;
				FitStylobate (t);
				ExpectAccepted (t);
			}
		// Rangee de trois colonnes identiques a l'entraxe 1,0 : la travee d'angle,
		// sous 2 e_n, est refusee comme travee d'angle.
		TempleSettings t = SyntheticTempleSettings ();
		t.cornerColumn = t.column;
		t.peristyle = {3, 5, 1.0, 1.7, 1.0, 1.61};
		FitStylobate (t);
		ExpectRefused (t, "Temple : 'peristyle.frontCornerSpacing'");
	}

	// (iii) Les deux voisines d'un angle, decalees sur les deux axes : refus si
	// les deux travees d'angle sont sous 2 e_n, accepte si l'une la depasse. La
	// colonne d'angle est fine, pour que (ii) n'interdise pas ces travees.
	{
		SCOPED_TRACE ("(iii) voisines d'un angle");
		TempleSettings t = SyntheticTempleSettings ();
		t.cornerColumn = SlenderColumn ();
		const double en = ComputeGreekDoricColumnHalfExtents (t.column).overall;
		const double ec = ComputeGreekDoricColumnHalfExtents (t.cornerColumn).overall;
		const double between = 0.5 * ((ec + en) + 2.0 * en);
		ASSERT_GT (between, ec + en + margin);
		ASSERT_LT (between, 2.0 * en - margin);
		t.peristyle.frontCornerSpacing = t.peristyle.flankCornerSpacing = between;
		FitStylobate (t);
		ExpectRefused (t, "Temple : 'peristyle.frontCornerSpacing'");
		for (const bool front : {true, false})
		{
			TempleSettings u = t;
			(front ? u.peristyle.frontCornerSpacing : u.peristyle.flankCornerSpacing) = 2.0 * en + margin;
			FitStylobate (u);
			ExpectAccepted (u);
		}
		// Egalite exacte : 3 x 3, decalages des voisines -s, 0 exacts, s = 2 e_n.
		TempleSettings u = t;
		u.peristyle = {3, 3, 2.0 * en, 2.0 * en, 2.0 * en, 2.0 * en};
		FitStylobate (u);
		ExpectRefused (u, "Temple : 'peristyle.frontCornerSpacing'");
		u.peristyle.flankSpacing = u.peristyle.flankCornerSpacing = 2.0 * en + margin;
		FitStylobate (u);
		ExpectAccepted (u);
	}

	// (iv) Pied plus large que l'abaque, cannele : la demi-etendue est le rayon du
	// pied aux aretes fois cos (pi / 20), recalcule ici ; un entraxe sous le
	// double est refuse, un entraxe entre ce double et 2 x le rayon aux aretes
	// (le majorant) est accepte.
	{
		SCOPED_TRACE ("(iv) pied plus large que l'abaque");
		TempleSettings t = SyntheticTempleSettings ();
		NarrowCapital (t.column, 0.01);
		t.cornerColumn = t.column;
		const double radius = 0.5 * t.column.lowerDiameter;
		const double exact = radius * std::cos (kPi / t.column.fluteCount);
		t.peristyle.frontSpacing = 2.0 * exact - margin;
		FitStylobate (t);
		ExpectRefused (t, "Temple : 'peristyle.frontSpacing'");
		t.peristyle.frontSpacing = 2.0 * exact + margin;
		ASSERT_LT (t.peristyle.frontSpacing, 2.0 * radius);
		FitStylobate (t);
		ExpectAccepted (t);
	}

	// (v) Rangee de deux colonnes : les colonnes des deux rangees opposees se font
	// face a travers sa travee (axes +-s/2, ecart s exact), qui doit depasser
	// 2 e_n et non seulement 2 e_c quand ces rangees ont des colonnes courantes.
	{
		SCOPED_TRACE ("(v) rangees qui se font face");
		for (const bool front : {true, false})
		{
			TempleSettings t = SyntheticTempleSettings ();
			t.cornerColumn = SlenderColumn ();
			const double en = ComputeGreekDoricColumnHalfExtents (t.column).overall;
			const double ec = ComputeGreekDoricColumnHalfExtents (t.cornerColumn).overall;
			ASSERT_GT (en + ec, 2.0 * ec + margin);
			ASSERT_LT (en + ec, 2.0 * en - margin);
			for (const double gap : {en + ec, 2.0 * en})
			{
				t.peristyle = front ? PeristyleSettings {2, 5, gap, 1.7, gap, 1.7} : PeristyleSettings {4, 2, 1.9, gap, 1.9, gap};
				FitStylobate (t);
				ExpectRefused (t, front ? "Temple : 'peristyle.frontCornerSpacing'" : "Temple : 'peristyle.flankCornerSpacing'");
			}
			const double clear = 2.0 * en + margin;
			if (front)
				t.peristyle.frontSpacing = t.peristyle.frontCornerSpacing = clear;
			else
				t.peristyle.flankSpacing = t.peristyle.flankCornerSpacing = clear;
			FitStylobate (t);
			ExpectAccepted (t);
		}
		// Rangee de deux colonnes face a des rangees de trois : la travee, au-dessus
		// de 2 e_c mais sous 2 e_n, est refusee.
		TempleSettings t = SyntheticTempleSettings ();
		t.cornerColumn = SlenderColumn ();
		t.peristyle = {2, 3, 1.0, 1.7, 1.0, 1.7};
		FitStylobate (t);
		ExpectRefused (t, "Temple : 'peristyle.frontCornerSpacing'");
	}

	// (vi) Le controle porte sur les axes places, pas sur les entraxes saisis :
	// entraxe juste au-dessus de 2 e_n, dont l'arrondi des axes places donne
	// deux voisins a exactement 2 e_n.
	{
		SCOPED_TRACE ("(vi) axes places");
		TempleSettings t = SyntheticTempleSettings ();
		const double en = ComputeGreekDoricColumnHalfExtents (t.column).overall;
		const double s = std::nextafter (2.0 * en, std::numeric_limits<double>::infinity ());
		t.peristyle = {8, 5, s, 1.7, s, 1.61};
		const PeristyleLayout p = LayoutPeristyle (t.peristyle);
		double smallest = std::numeric_limits<double>::infinity ();
		std::vector<double> xs;
		for (const PeristyleColumn &c : p.columns)
			if (c.side == PeristyleSide::Front)
				xs.push_back (c.x);
		xs.push_back (p.halfWidth);
		for (std::size_t k = 1; k < xs.size (); ++k)
			smallest = std::min (smallest, xs[k] - xs[k - 1]);
		ASSERT_LE (smallest, 2.0 * en) << "jeu d'essai : aucun ecart place n'atteint 2 e_n";
		FitStylobate (t);
		ExpectRefused (t, "Temple : 'peristyle.");
	}
}

// T4b : oracle independant de TE4. Sur les variantes et sur une grille de
// reglages, la validation refuse un temple pour TE4 si et seulement si deux
// colonnes placees ont des boites (alignees sur les axes, de demi-cote
// `overall`, fermees) qui se recouvrent. Les placements sont recalcules par
// LayoutPeristyle, les paires examinees toutes.
TEST (TEST_cgarchi_classical_temple, te4_refuses_exactly_the_layouts_whose_boxes_overlap)
{
	auto overlapping = [] (const TempleSettings &t) {
		const PeristyleLayout p = LayoutPeristyle (t.peristyle);
		const double en = ComputeGreekDoricColumnHalfExtents (t.column).overall;
		const double ec = ComputeGreekDoricColumnHalfExtents (t.cornerColumn).overall;
		for (std::size_t i = 0; i < p.columns.size (); ++i)
			for (std::size_t j = i + 1; j < p.columns.size (); ++j)
			{
				const PeristyleColumn &a = p.columns[i], &b = p.columns[j];
				const double e = (a.corner ? ec : en) + (b.corner ? ec : en);
				if (std::fabs (a.x - b.x) <= e && std::fabs (a.y - b.y) <= e)
					return true;
			}
		return false;
	};
	auto check = [&] (const TempleSettings &t, const std::string &what, int &refused, int &accepted) {
		const std::string refusal = RefusalOf (t);
		const bool te4 = refusal.rfind ("Temple : 'peristyle.", 0) == 0;
		ASSERT_TRUE (refusal.empty () || te4) << what << " : refus hors TE4 : " << refusal;
		EXPECT_EQ (te4, overlapping (t)) << what << " : " << refusal;
		++(te4 ? refused : accepted);
	};

	int refused = 0, accepted = 0;
	for (const auto &[name, t] : Variants ())
		check (t, name, refused, accepted);

	const std::pair<const char *, GreekDoricColumnSettings> columns[] = {
		{"synthetique", SyntheticGreekDoricColumnSettings ()}, {"fine", SlenderColumn ()}, {"annelets", ProjectingAnnuletsColumn ()}};
	unsigned state = 12345u;
	auto next = [&state] (unsigned n) {
		state = state * 1103515245u + 12345u;
		return (state >> 8) % n;
	};
	for (const auto &[cname, column] : columns)
		for (const auto &[kname, corner] : columns)
		{
			const double en = ComputeGreekDoricColumnHalfExtents (column).overall;
			const double ec = ComputeGreekDoricColumnHalfExtents (corner).overall;
			const double candidates[] = {2.0 * ec, ec + en, 2.0 * en, 2.0 * std::max (ec, en),
			                             2.0 * ec + 1e-3, ec + en + 1e-3, 2.0 * en + 1e-3, 0.5 * (ec + 3.0 * en), 2.0};
			const int count = static_cast<int> (std::size (candidates));
			for (int front = 2; front <= 5; ++front)
				for (int flank = 2; flank <= 5; ++flank)
					for (int draw = 0; draw < 60; ++draw)
					{
						TempleSettings t = SyntheticTempleSettings ();
						t.column = column;
						t.cornerColumn = corner;
						const double sf = candidates[next (count)], sl = candidates[next (count)];
						const double af = front >= 4 ? candidates[next (count)] : sf;
						const double al = flank >= 4 ? candidates[next (count)] : sl;
						t.peristyle = {front, flank, sf, sl, af, al};
						FitStylobate (t);
						check (t, std::string (cname) + " / " + kname + ", " + std::to_string (front) + " x " + std::to_string (flank) +
						              ", " + std::to_string (sf) + " " + std::to_string (sl) + " " + std::to_string (af) + " " +
						              std::to_string (al),
						       refused, accepted);
					}
		}
	// Garde : la grille exerce les deux verdicts.
	EXPECT_GT (refused, 1000);
	EXPECT_GT (accepted, 1000);
	RecordProperty ("te4_refused", refused);
	RecordProperty ("te4_accepted", accepted);
}

// T5 : TE3, sur chaque axe et pour chaque sorte de colonne.
TEST (TEST_cgarchi_classical_temple, a_foot_beyond_the_stylobate_is_refused)
{
	// Le pied le plus large est celui de l'angle (jeu synthetique), puis celui de
	// la colonne courante (jeux permutes).
	TempleSettings swapped = SyntheticTempleSettings ();
	std::swap (swapped.column, swapped.cornerColumn);
	const std::pair<std::string, TempleSettings> cases[] = {{"pied d'angle le plus large", SyntheticTempleSettings ()},
	                                                        {"pied courant le plus large", swapped}};
	for (const auto &[name, base] : cases)
	{
		SCOPED_TRACE (name);
		const PeristyleLayout p = LayoutPeristyle (base.peristyle);
		const GreekDoricColumnHalfExtents en = ComputeGreekDoricColumnHalfExtents (base.column);
		const GreekDoricColumnHalfExtents ec = ComputeGreekDoricColumnHalfExtents (base.cornerColumn);
		double need[2] = {0.0, 0.0}, needCorners[2] = {0.0, 0.0}, abacus[2] = {0.0, 0.0};
		for (const PeristyleColumn &c : p.columns)
		{
			const GreekDoricColumnHalfExtents &e = c.corner ? ec : en;
			const double at[2] = {std::fabs (c.x), std::fabs (c.y)};
			for (int axis = 0; axis < 2; ++axis)
			{
				need[axis] = std::max (need[axis], at[axis] + e.foot);
				abacus[axis] = std::max (abacus[axis], at[axis] + e.overall);
				if (c.corner)
					needCorners[axis] = std::max (needCorners[axis], at[axis] + e.foot);
			}
		}
		if (en.foot > ec.foot)
			for (int axis = 0; axis < 2; ++axis)
				EXPECT_GT (need[axis], needCorners[axis]) << axis << " : le pied courant ne decide pas";

		for (int axis = 0; axis < 2; ++axis)
		{
			SCOPED_TRACE (axis == 0 ? "X" : "Y");
			TempleSettings t = base;
			double &half = axis == 0 ? t.krepis.stylobateHalfWidth : t.krepis.stylobateHalfDepth;
			half = need[axis];
			ExpectAccepted (t);
			// L'abaque deborde du stylobate, le pied est dedans.
			EXPECT_GT (abacus[axis], half);
			half = std::nextafter (need[axis], 0.0);
			ExpectRefused (t, axis == 0 ? "Temple : 'krepis.stylobateHalfWidth'" : "Temple : 'krepis.stylobateHalfDepth'");
		}
	}
}

// T7 : un champ fautif par composant ; les exceptions des composants remontent
// avec leur prefixe, ValidateTempleSettings et LayoutTemple refusent de meme.
TEST (TEST_cgarchi_classical_temple, invalid_settings_are_refused_before_any_output)
{
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	using Edit = std::function<void (TempleSettings &)>;
	const std::vector<std::pair<std::string, Edit>> cases = {
		{"Krepis : 'stylobateHeight'", [] (TempleSettings &t) { t.krepis.stylobateHeight = 0.0; }},
		{"Krepis : 'lowerSteps[1].outsetY'", [&] (TempleSettings &t) { t.krepis.lowerSteps[1].outsetY = nan; }},
		{"LayoutPeristyle : 'front'", [] (TempleSettings &t) { t.peristyle.front = 1; }},
		{"LayoutPeristyle : 'flankCornerSpacing'", [&] (TempleSettings &t) { t.peristyle.flankCornerSpacing = nan; }},
		{"LayoutPeristyle : 'frontCornerSpacing'", [] (TempleSettings &t) { t.peristyle.front = 3; }},
		{"GreekDoricColumn : 'abacusHeight'", [] (TempleSettings &t) { t.column.abacusHeight = 0.0; }},
		{"GreekDoricColumn : 'lowerDiameter'", [&] (TempleSettings &t) { t.cornerColumn.lowerDiameter = nan; }},
		{"Temple : 'peristyle.frontSpacing'", [] (TempleSettings &t) { t.peristyle.frontSpacing = 1.0; }},
		{"Temple : 'krepis.stylobateHalfDepth'", [] (TempleSettings &t) { t.krepis.stylobateHalfDepth = 3.5; }},
	};
	for (const auto &[message, edit] : cases)
	{
		SCOPED_TRACE (message);
		TempleSettings t = SyntheticTempleSettings ();
		edit (t);
		ExpectRefused (t, message);
	}
	ExpectAccepted (SyntheticTempleSettings ());

	// BuildTempleMesh : meme refus, sorties sentinelles intactes.
	auto expectBuildRefused = [] (const TempleSettings &t, const std::string &message) {
		TempleLayout layout;
		layout.columns.resize (1);
		layout.columns[0].x = -7.0;
		std::vector<TempleMeshPart> parts (1);
		parts[0].firstFace = 77u;
		try
		{
			BuildTempleMesh (t, &layout, &parts);
			ADD_FAILURE () << message << " : accepte";
		}
		catch (const std::invalid_argument &e)
		{
			EXPECT_NE (std::string (e.what ()).find (message), std::string::npos) << e.what ();
		}
		ASSERT_EQ (layout.columns.size (), 1u);
		EXPECT_EQ (layout.columns[0].x, -7.0);
		ASSERT_EQ (parts.size (), 1u);
		EXPECT_EQ (parts[0].firstFace, 77u);
	};
	for (const auto &[message, edit] : cases)
	{
		SCOPED_TRACE (message);
		TempleSettings t = SyntheticTempleSettings ();
		edit (t);
		expectBuildRefused (t, message);
	}

	// Borne de faces : 1 024 x 1 024 colonnes synthetiques (4 092), environ
	// 92 x 10^6 faces prevues, refusees avant l'allocation du resultat.
	TempleSettings big = SyntheticTempleSettings ();
	big.peristyle = {kPeristyleMaxColumnsPerSide, kPeristyleMaxColumnsPerSide, 1.9, 1.7, 1.73, 1.61};
	FitStylobate (big);
	ExpectAccepted (big);
	expectBuildRefused (big, "Temple : 'peristyle'");

	// La borne a sa valeur exacte, comprise.
	EXPECT_TRUE (TempleMeshFacesWithinBound (kTempleMaxFaces));
	EXPECT_FALSE (TempleMeshFacesWithinBound (kTempleMaxFaces + 1));

	// Compte sans construire le maillage : krepis plus un tour par colonne,
	// recalcule ici depuis les pieces ; encadrement de la borne par les deux
	// peristyles les plus proches, de 742 et 744 colonnes synthetiques.
	auto expectedFaces = [] (const TempleSettings &t) {
		const PeristyleLayout p = LayoutPeristyle (t.peristyle);
		const std::size_t column = BuildGreekDoricColumnMesh (t.column)->GetNFaces ();
		const std::size_t corner = BuildGreekDoricColumnMesh (t.cornerColumn)->GetNFaces ();
		return static_cast<std::size_t> (BuildKrepisMesh (t.krepis)->GetNFaces ()) + 4 * corner + (p.columns.size () - 4) * column;
	};
	const TempleSettings synthetic = SyntheticTempleSettings ();
	EXPECT_EQ (CountTempleMeshFaces (synthetic), expectedFaces (synthetic));
	TempleSettings under = SyntheticTempleSettings ();
	under.peristyle = {186, 187, 1.9, 1.7, 1.73, 1.61};
	FitStylobate (under);
	const std::size_t underFaces = expectedFaces (under);
	ASSERT_LE (underFaces, kTempleMaxFaces);
	EXPECT_EQ (CountTempleMeshFaces (under), underFaces);
	TempleSettings over = under;
	over.peristyle.flank = 188;
	FitStylobate (over);
	ASSERT_GT (expectedFaces (over), kTempleMaxFaces);
	RecordProperty ("faces_742_columns", std::to_string (underFaces));
	RecordProperty ("faces_744_columns", std::to_string (expectedFaces (over)));
	try
	{
		CountTempleMeshFaces (over);
		ADD_FAILURE () << "744 colonnes : accepte";
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("Temple : 'peristyle'"), std::string::npos) << e.what ();
	}
	expectBuildRefused (over, "Temple : 'peristyle'");
}

// ---------------------------------------------------------------------------
//  Maillage
// ---------------------------------------------------------------------------

namespace {

// Les trois sommets de la face `f` sont a la cote `z`.
bool Horizontal (const Mesh &m, unsigned f, float z)
{
	for (unsigned j = 0; j < 3; ++j)
	{
		float q[3];
		m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (f, j)), q);
		if (q[2] != z)
			return false;
	}
	return true;
}

}  // namespace

// T3 : liste fermee des recouvrements (P3-3) : L1, contact plan entre degres ;
// L2, pied d'une colonne sur le stylobate. Tous les enregistrements sont lus.
TEST (TEST_cgarchi_classical_temple, the_only_overlaps_are_step_contacts_and_column_feet)
{
	for (const auto &[name, t] : MeshVariants ())
	{
		SCOPED_TRACE (name);
		TempleLayout l;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		MeshValidationOptions options;
		options.maxRecordsPerKind = m->GetNFaces ();
		const MeshValidationReport r = ValidateMesh (*m, options);
		ASSERT_TRUE (r.selfIntersectionChecked);
		ASSERT_FALSE (r.selfIntersectionTruncated);
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection)
				EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
		EXPECT_EQ (r.components, parts.size ());

		std::vector<std::size_t> owner (m->GetNFaces ());
		for (std::size_t k = 0; k < parts.size (); ++k)
			for (unsigned f = parts[k].firstFace; f < parts[k].firstFace + parts[k].faceCount; ++f)
				owner[f] = k;
		const std::size_t steps = l.krepis.steps.size ();
		std::vector<int> feet (l.columns.size (), 0);
		std::size_t records = 0, l1 = 0, l2 = 0;
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::SelfIntersection)
				continue;
			++records;
			ASSERT_EQ (rec.faces.size (), 2u);
			unsigned fa = rec.faces[0], fb = rec.faces[1];
			std::size_t a = owner[fa], b = owner[fb];
			if (a > b)
			{
				std::swap (a, b);
				std::swap (fa, fb);
			}
			if (b < steps && b == a + 1)
			{
				const float z = static_cast<float> (l.krepis.steps[b].zTop);
				EXPECT_TRUE (Horizontal (*m, fa, z) && Horizontal (*m, fb, z)) << "L1 non horizontal : " << a << ", " << b;
				++l1;
			}
			else if (a == 0 && b >= steps)
			{
				const float z = static_cast<float> (l.krepis.stylobateTop);
				EXPECT_TRUE (Horizontal (*m, fa, z) && Horizontal (*m, fb, z)) << "L2 non horizontal : colonne " << b - steps;
				++feet[b - steps];
				++l2;
			}
			else
				ADD_FAILURE () << "recouvrement hors liste : pieces " << a << " et " << b;
		}
		EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
		for (std::size_t c = 0; c < feet.size (); ++c)
			EXPECT_GE (feet[c], 1) << "colonne " << c << " sans contact L2";
		if (steps > 1)
			EXPECT_GE (l1, steps - 1);
		RecordProperty ("l1_records", static_cast<int> (l1));
		RecordProperty ("l2_records", static_cast<int> (l2));
	}
}

// T6 : test hors condition (D3-19). Un annelet de hauteur comprise entre la
// tolerance de soudure de la colonne seule et celle du temple : la colonne
// seule est valide, le temple ne l'est pas. Controle de frontiere de contrat,
// pas une preuve de justesse.
TEST (TEST_cgarchi_classical_temple, validity_needs_every_separation_above_the_assembled_weld_tolerance)
{
	TempleSettings t = SyntheticTempleSettings ();
	SmoothColumns (t);
	t.peristyle = {6, 9, 1.9, 1.7, 1.73, 1.61};
	FitStylobate (t);
	const double tColumn = DefaultWeldTolerance (*BuildGreekDoricColumnMesh (t.column));
	const double tTemple = DefaultWeldTolerance (*BuildTempleMesh (t));
	ASSERT_GT (tTemple, 2.0 * tColumn) << tColumn << " " << tTemple;
	t.column.annulets[1].height = std::sqrt (tColumn * tTemple);
	RecordProperty ("annulet_height", std::to_string (t.column.annulets[1].height));

	const MeshValidationReport column = ValidateMesh (*BuildGreekDoricColumnMesh (t.column));
	EXPECT_TRUE (column.IsValid ());
	const std::unique_ptr<Mesh> m = BuildTempleMesh (t);
	const MeshValidationReport temple = ValidateMesh (*m);
	std::size_t others = 0;
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection)
			others += temple.counts[kind];
	EXPECT_GT (others, 0u) << "temple valide hors condition";
	EXPECT_GE (temple.weldTolerance, t.column.annulets[1].height);
}

// T8 : regle 2. Croissance de 16 a 64 colonnes : lineaire, rapport des minima
// au plus 4 (moins, la krepis et les deux tours etant construites une fois) ;
// quadratique avec une concatenation qui recopie le resultat (16 au plus).
// Seuil a mi-chemin, en rapport.
TEST (TEST_cgarchi_classical_temple, the_assembly_grows_linearly_with_the_number_of_columns)
{
	auto settings = [] (int side) {
		TempleSettings t = SyntheticTempleSettings ();
		t.peristyle = {side, side, 1.9, 1.7, 1.9, 1.7};
		FitStylobate (t);
		return t;
	};
	auto elapsedMs = [] (const TempleSettings &t) {
		const auto start = std::chrono::steady_clock::now ();
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t);
		const auto stop = std::chrono::steady_clock::now ();
		EXPECT_GT (m->GetNFaces (), 0u);
		return std::chrono::duration<double, std::milli> (stop - start).count ();
	};
	// Les deux tailles en alternance, rapport des minima : une preemption ne fait
	// qu'allonger une mesure.
	const TempleSettings smallSettings = settings (5), largeSettings = settings (17);
	double small = std::numeric_limits<double>::infinity (), large = small;
	for (int k = 0; k < 5; ++k)
	{
		small = std::min (small, elapsedMs (smallSettings));
		large = std::min (large, elapsedMs (largeSettings));
	}
	RecordProperty ("ms_16_columns", std::to_string (small));
	RecordProperty ("ms_64_columns", std::to_string (large));
	EXPECT_LT (large / small, 8.0) << small << " ms, " << large << " ms";
}

// T9
TEST (TEST_cgarchi_classical_temple, two_identical_builds_are_equal_bit_for_bit)
{
	const TempleSettings t = SyntheticTempleSettings ();
	TempleLayout la, lb;
	std::vector<TempleMeshPart> pa, pb;
	const std::unique_ptr<Mesh> a = BuildTempleMesh (t, &la, &pa);
	const std::unique_ptr<Mesh> b = BuildTempleMesh (t, &lb, &pb);
	ASSERT_EQ (a->GetNVertices (), b->GetNVertices ());
	ASSERT_EQ (a->GetNFaces (), b->GetNFaces ());
	EXPECT_EQ (std::memcmp (a->GetVertices ().data (), b->GetVertices ().data (), a->GetVertices ().size () * sizeof (float)), 0);
	bool faces = true;
	for (unsigned f = 0; faces && f < a->GetNFaces (); ++f)
		for (unsigned j = 0; j < 3; ++j)
			faces = faces && a->GetFaceVertex (f, j) == b->GetFaceVertex (f, j);
	EXPECT_TRUE (faces);
	ASSERT_EQ (pa.size (), pb.size ());
	for (std::size_t k = 0; k < pa.size (); ++k)
	{
		EXPECT_EQ (pa[k].firstVertex, pb[k].firstVertex) << k;
		EXPECT_EQ (pa[k].firstFace, pb[k].firstFace) << k;
		EXPECT_EQ (pa[k].address.index, pb[k].address.index) << k;
	}
	ASSERT_EQ (la.columns.size (), lb.columns.size ());
	for (std::size_t c = 0; c < la.columns.size (); ++c)
	{
		EXPECT_EQ (std::memcmp (&la.columns[c].x, &lb.columns[c].x, sizeof (double)), 0) << c;
		EXPECT_EQ (std::memcmp (&la.columns[c].y, &lb.columns[c].y, sizeof (double)), 0) << c;
	}
}

// ---------------------------------------------------------------------------
//  Entablement (lot N0a)
// ---------------------------------------------------------------------------

namespace {

// Maillage du temple sans entablement recompose ici, selon le contrat de K1 :
// krepis telle quelle, puis, par placement, le tour de son jeu translate,
// chaque position float (double (v) + translation).
struct Assembled
{
	std::vector<float> vertices;
	std::vector<unsigned> faces;
};

Assembled K1Oracle (const TempleSettings &t, const TempleLayout &l)
{
	Assembled out;
	auto append = [&] (const Mesh &piece, double tx, double ty, double tz) {
		const unsigned first = static_cast<unsigned> (out.vertices.size () / 3);
		const std::vector<float> &v = piece.GetVertices ();
		for (std::size_t i = 0; i + 2 < v.size (); i += 3)
		{
			out.vertices.push_back (static_cast<float> (static_cast<double> (v[i]) + tx));
			out.vertices.push_back (static_cast<float> (static_cast<double> (v[i + 1]) + ty));
			out.vertices.push_back (static_cast<float> (static_cast<double> (v[i + 2]) + tz));
		}
		for (unsigned f = 0; f < piece.GetNFaces (); ++f)
			for (unsigned j = 0; j < 3; ++j)
				out.faces.push_back (first + static_cast<unsigned> (piece.GetFaceVertex (f, j)));
	};
	append (*BuildKrepisMesh (t.krepis), 0.0, 0.0, 0.0);
	const std::unique_ptr<Mesh> column = BuildGreekDoricColumnMesh (t.column);
	const std::unique_ptr<Mesh> corner = BuildGreekDoricColumnMesh (t.cornerColumn);
	for (const TemplePlacement &p : l.columns)
		append (p.corner ? *corner : *column, p.x, p.y, p.z);
	return out;
}

std::vector<unsigned> FacesOf (const Mesh &m)
{
	std::vector<unsigned> faces;
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
		for (unsigned j = 0; j < 3; ++j)
			faces.push_back (static_cast<unsigned> (m.GetFaceVertex (f, j)));
	return faces;
}

// Cotes extremes d'une plage du maillage.
std::pair<float, float> ZRange (const Mesh &m, const TempleMeshPart &part)
{
	float lo = std::numeric_limits<float>::infinity (), hi = -lo;
	for (unsigned v = part.firstVertex; v < part.firstVertex + part.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex (v, q);
		lo = std::min (lo, q[2]);
		hi = std::max (hi, q[2]);
	}
	return {lo, hi};
}

bool SameAddress (const TempleAddress &a, const TempleAddress &b)
{
	return a.element == b.element && a.side == b.side && a.index == b.index;
}

}  // namespace

// T10 : EN10. Hauteur cherchee ici telle que les deux expressions du pied,
// float (h + stylobateTop) et float (double (float (h)) + stylobateTop),
// different : seule la seconde est celle du sommet des colonnes.
TEST (TEST_cgarchi_classical_temple, the_entablature_rests_on_the_column_tops)
{
	TempleSettings t = SyntheticTempleWithEntablature ();
	const double st = LayoutTemple (t).krepis.stylobateTop;
	double h = 0.0;
	bool found = false;
	for (int k = 0; k < 100000 && !found; ++k)
	{
		h = 5.3 + k * 1e-7;
		found = static_cast<float> (h + st) != static_cast<float> (static_cast<double> (static_cast<float> (h)) + st);
	}
	ASSERT_TRUE (found) << "aucune hauteur discriminante";
	std::ostringstream height;
	height << std::setprecision (17) << h;
	RecordProperty ("discriminating_height", height.str ());
	t.column.height = h;
	t.cornerColumn.height = h;

	auto expectRingOnColumns = [] (const TempleSettings &s) {
		TempleLayout l;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (s, &l, &parts);
		const double foot = static_cast<double> (static_cast<float> (s.column.height)) + l.krepis.stylobateTop;
		EXPECT_EQ (std::memcmp (&l.entablatureFoot, &foot, sizeof (double)), 0);
		const TempleMeshPart *ring = nullptr;
		for (const TempleMeshPart &p : parts)
			if (p.address.element == TempleElement::Entablature)
				ring = &p;
		ASSERT_NE (ring, nullptr);
		const float bottom = ZRange (*m, *ring).first;
		EXPECT_EQ (bottom, static_cast<float> (l.entablatureFoot));
		int columns = 0;
		for (const TempleMeshPart &p : parts)
			if (p.address.element == TempleElement::Column)
			{
				++columns;
				EXPECT_EQ (ZRange (*m, p).second, bottom) << p.address.index;
			}
		EXPECT_EQ (columns, 14);
	};
	expectRingOnColumns (t);

	// Deux hauteurs differentes en float : refus.
	TempleSettings other = t;
	other.cornerColumn.height = h + 0.01;
	ExpectRefused (other, "Temple : 'cornerColumn.height'");
	// Egales en float, differentes en double : acceptees, l'anneau pose sur les
	// deux jeux.
	double near = std::nextafter (h, 10.0);
	if (static_cast<float> (near) != static_cast<float> (h))
		near = std::nextafter (h, 0.0);
	ASSERT_EQ (static_cast<float> (near), static_cast<float> (h));
	ASSERT_NE (near, h);
	other.cornerColumn.height = near;
	ExpectAccepted (other);
	expectRingOnColumns (other);
}

// T11 : DG-4. Degres, colonnes, anneau, triglyphes ; les metopes ne sont pas
// des pieces. Sans entablement, la sortie est celle de K1 au bit pres.
TEST (TEST_cgarchi_classical_temple, the_entablature_parts_follow_the_columns_with_stable_addresses)
{
	auto addresses = [] (const TempleSettings &t) {
		TempleLayout l;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		// Plages contigues couvrant le maillage.
		unsigned v = 0, f = 0;
		for (const TempleMeshPart &p : parts)
		{
			EXPECT_EQ (p.firstVertex, v);
			EXPECT_EQ (p.firstFace, f);
			v += p.vertexCount;
			f += p.faceCount;
		}
		EXPECT_EQ (v, m->GetNVertices ());
		EXPECT_EQ (f, m->GetNFaces ());
		std::vector<TempleAddress> out;
		for (const TempleMeshPart &p : parts)
			out.push_back (p.address);
		return std::make_pair (out, l);
	};

	const TempleSettings with = SyntheticTempleWithEntablature ();
	const auto [a, l] = addresses (with);
	ASSERT_TRUE (l.frieze.has_value ());
	const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), nt = l.frieze->triglyphs.size ();
	ASSERT_EQ (a.size (), steps + nc + 1 + nt);
	for (std::size_t k = 0; k < steps; ++k)
		EXPECT_TRUE (SameAddress (a[k], {TempleElement::KrepisStep, PeristyleSide::Front, static_cast<int> (k)})) << k;
	for (std::size_t c = 0; c < nc; ++c)
		EXPECT_TRUE (SameAddress (a[steps + c], l.columns[c].address)) << c;
	EXPECT_TRUE (SameAddress (a[steps + nc], {TempleElement::Entablature, PeristyleSide::Front, 0}));
	for (std::size_t k = 0; k < nt; ++k)
	{
		const FriezePiece &q = l.frieze->triglyphs[k];
		EXPECT_TRUE (SameAddress (a[steps + nc + 1 + k], {TempleElement::Triglyph, q.side, q.rank})) << k;
	}
	EXPECT_EQ (nt, 32u);
	EXPECT_EQ (l.frieze->metopes.size (), 36u);

	// Adresses inchangees sous un changement de longueurs (comptes egaux).
	TempleSettings longer = with;
	longer.peristyle.frontSpacing = 2.03;
	longer.peristyle.flankSpacing = 1.83;
	longer.peristyle.frontCornerSpacing = 1.87;
	longer.peristyle.flankCornerSpacing = 1.71;
	FitStylobate (longer);
	longer.entablature->bands[3].height = 0.61;
	longer.entablature->innerFace = 0.41;
	const auto [b, lb] = addresses (longer);
	ASSERT_EQ (b.size (), a.size ());
	for (std::size_t k = 0; k < a.size (); ++k)
		EXPECT_TRUE (SameAddress (a[k], b[k])) << k;

	// Sans entablement : sortie de K1 au bit pres, et prefixe de la sortie avec
	// entablement.
	const TempleSettings without = SyntheticTempleSettings ();
	TempleLayout l0;
	std::vector<TempleMeshPart> p0;
	const std::unique_ptr<Mesh> m0 = BuildTempleMesh (without, &l0, &p0);
	EXPECT_FALSE (l0.frieze.has_value ());
	const double zero = 0.0;
	EXPECT_EQ (std::memcmp (&l0.entablatureFoot, &zero, sizeof (double)), 0);
	ASSERT_EQ (p0.size (), steps + nc);
	const Assembled oracle = K1Oracle (without, l0);
	ASSERT_EQ (m0->GetVertices ().size (), oracle.vertices.size ());
	EXPECT_EQ (std::memcmp (m0->GetVertices ().data (), oracle.vertices.data (), oracle.vertices.size () * sizeof (float)), 0);
	EXPECT_EQ (FacesOf (*m0), oracle.faces);
	for (std::size_t c = 0; c < nc; ++c)
	{
		EXPECT_EQ (std::memcmp (&l0.columns[c].x, &l.columns[c].x, sizeof (double)), 0) << c;
		EXPECT_EQ (std::memcmp (&l0.columns[c].y, &l.columns[c].y, sizeof (double)), 0) << c;
	}

	TempleLayout l1;
	std::vector<TempleMeshPart> p1;
	const std::unique_ptr<Mesh> m1 = BuildTempleMesh (with, &l1, &p1);
	ASSERT_GE (p1.size (), p0.size ());
	for (std::size_t k = 0; k < p0.size (); ++k)
	{
		EXPECT_TRUE (SameAddress (p0[k].address, p1[k].address)) << k;
		EXPECT_EQ (p0[k].firstVertex, p1[k].firstVertex) << k;
		EXPECT_EQ (p0[k].vertexCount, p1[k].vertexCount) << k;
		EXPECT_EQ (p0[k].firstFace, p1[k].firstFace) << k;
		EXPECT_EQ (p0[k].faceCount, p1[k].faceCount) << k;
	}
	ASSERT_GE (m1->GetVertices ().size (), m0->GetVertices ().size ());
	EXPECT_EQ (std::memcmp (m1->GetVertices ().data (), m0->GetVertices ().data (), m0->GetVertices ().size () * sizeof (float)), 0);
	const std::vector<unsigned> f0 = FacesOf (*m0), f1 = FacesOf (*m1);
	ASSERT_GE (f1.size (), f0.size ());
	EXPECT_TRUE (std::equal (f0.begin (), f0.end (), f1.begin ()));
}

// T12 : P3-3, liste fermee des recouvrements avec l'entablement : L1 et L2
// (T3), plus L3 (dessous de l'anneau sur le dessus d'un abaque), L4 (dos d'un
// triglyphe sur la face de frise de l'anneau), L5 (dessous d'un triglyphe sur
// le ressaut de la tenia), L6 (dessus d'un triglyphe sous le ressaut du
// chapiteau). Tous les enregistrements sont lus et classes par les plages et
// par leur plan.
TEST (TEST_cgarchi_classical_temple, the_only_overlaps_with_the_entablature_are_listed_contacts)
{
	TempleSettings smooth = SyntheticTempleWithEntablature ();
	SmoothColumns (smooth);
	const std::vector<std::pair<std::string, TempleSettings>> variants = {{"fut lisse", smooth},
	                                                                        {"synthetique cannele", SyntheticTempleWithEntablature ()}};
	for (std::size_t variant = 0; variant < variants.size (); ++variant)
	{
		const std::string &name = variants[variant].first;
		const TempleSettings &t = variants[variant].second;
		const std::string suffix = std::to_string (variant);
		SCOPED_TRACE (name);
		TempleLayout l;
		std::vector<TempleMeshPart> parts;
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		MeshValidationOptions options;
		options.maxRecordsPerKind = m->GetNFaces ();
		const MeshValidationReport r = ValidateMesh (*m, options);
		ASSERT_TRUE (r.selfIntersectionChecked);
		ASSERT_FALSE (r.selfIntersectionTruncated);
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection)
				EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
		EXPECT_EQ (r.components, parts.size ());

		const GreekEntablatureSettings &e = *t.entablature;
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		const std::size_t f = static_cast<std::size_t> (e.friezeBand);
		const float zRing = static_cast<float> (l.entablatureFoot);
		const float zLow = static_cast<float> (l.entablatureFoot + z[f]);
		const float zHigh = static_cast<float> (l.entablatureFoot + z[f + 1]);
		const double b = e.bands[f].projection, hx = l.peristyle.halfWidth, hy = l.peristyle.halfDepth;
		// Plan de la face de frise d'un cote, par les expressions de l'anneau.
		auto onFriezePlane = [&] (unsigned face, PeristyleSide side) {
			for (unsigned j = 0; j < 3; ++j)
			{
				float q[3];
				m->GetVertex (static_cast<unsigned> (m->GetFaceVertex (face, j)), q);
				const bool on = side == PeristyleSide::Front   ? q[1] == static_cast<float> (-(hy + b))
				                : side == PeristyleSide::Back  ? q[1] == static_cast<float> (hy + b)
				                : side == PeristyleSide::Right ? q[0] == static_cast<float> (hx + b)
				                                               : q[0] == static_cast<float> (-(hx + b));
				if (!on)
					return false;
			}
			return true;
		};

		std::vector<std::size_t> owner (m->GetNFaces ());
		for (std::size_t k = 0; k < parts.size (); ++k)
			for (unsigned face = parts[k].firstFace; face < parts[k].firstFace + parts[k].faceCount; ++face)
				owner[face] = k;
		const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), ring = steps + nc;
		ASSERT_EQ (parts.size (), ring + 1 + l.frieze->triglyphs.size ());
		std::vector<int> l3 (nc, 0), l4 (l.frieze->triglyphs.size (), 0);
		std::size_t records = 0, n1 = 0, n2 = 0, n3 = 0, n4 = 0, n5 = 0, n6 = 0;
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::SelfIntersection)
				continue;
			++records;
			ASSERT_EQ (rec.faces.size (), 2u);
			unsigned fa = rec.faces[0], fb = rec.faces[1];
			std::size_t pa = owner[fa], pb = owner[fb];
			if (pa > pb)
			{
				std::swap (pa, pb);
				std::swap (fa, fb);
			}
			if (pb < steps && pb == pa + 1)
			{
				const float zs = static_cast<float> (l.krepis.steps[pb].zTop);
				EXPECT_TRUE (Horizontal (*m, fa, zs) && Horizontal (*m, fb, zs)) << "L1 : " << pa << ", " << pb;
				++n1;
			}
			else if (pa == 0 && pb >= steps && pb < ring)
			{
				const float zs = static_cast<float> (l.krepis.stylobateTop);
				EXPECT_TRUE (Horizontal (*m, fa, zs) && Horizontal (*m, fb, zs)) << "L2 : colonne " << pb - steps;
				++n2;
			}
			else if (pa >= steps && pa < ring && pb == ring)
			{
				EXPECT_TRUE (Horizontal (*m, fa, zRing) && Horizontal (*m, fb, zRing)) << "L3 : colonne " << pa - steps;
				++l3[pa - steps];
				++n3;
			}
			else if (pa == ring && pb > ring)
			{
				const std::size_t k = pb - ring - 1;
				const PeristyleSide side = l.frieze->triglyphs[k].side;
				if (Horizontal (*m, fa, zLow) && Horizontal (*m, fb, zLow))
					++n5;
				else if (Horizontal (*m, fa, zHigh) && Horizontal (*m, fb, zHigh))
					++n6;
				else if (onFriezePlane (fa, side) && onFriezePlane (fb, side))
				{
					++l4[k];
					++n4;
				}
				else
					ADD_FAILURE () << "anneau et triglyphe " << k << " : recouvrement hors de L4 a L6";
			}
			else
				ADD_FAILURE () << "recouvrement hors liste : pieces " << pa << " et " << pb;
		}
		EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
		for (std::size_t c = 0; c < l3.size (); ++c)
			EXPECT_GE (l3[c], 1) << "colonne " << c << " sans contact L3";
		for (std::size_t k = 0; k < l4.size (); ++k)
			EXPECT_GE (l4[k], 1) << "triglyphe " << k << " sans contact L4";
		RecordProperty ("l1_records_" + suffix, static_cast<int> (n1));
		RecordProperty ("l2_records_" + suffix, static_cast<int> (n2));
		RecordProperty ("l3_records_" + suffix, static_cast<int> (n3));
		RecordProperty ("l4_records_" + suffix, static_cast<int> (n4));
		RecordProperty ("l5_records_" + suffix, static_cast<int> (n5));
		RecordProperty ("l6_records_" + suffix, static_cast<int> (n6));
		RecordProperty ("faces_" + suffix, static_cast<int> (m->GetNFaces ()));
	}
}

// T13 : CountTempleMeshFaces avec et sans entablement ; deux constructions
// egales au bit pres (T9 etendu a l'entablement).
TEST (TEST_cgarchi_classical_temple, the_face_count_includes_the_entablature)
{
	const TempleSettings without = SyntheticTempleSettings ();
	const TempleSettings with = SyntheticTempleWithEntablature ();
	const std::size_t f0 = CountTempleMeshFaces (without), f1 = CountTempleMeshFaces (with);
	EXPECT_EQ (f0, static_cast<std::size_t> (BuildTempleMesh (without)->GetNFaces ()));
	EXPECT_EQ (f1, static_cast<std::size_t> (BuildTempleMesh (with)->GetNFaces ()));

	// Entablement recompte ici : anneau de 19 sommets de section (8 faces par
	// sommet), 32 prismes de 76 faces.
	const TempleLayout l = LayoutTemple (with);
	const std::size_t ring = SweepRectangularRing (GreekEntablatureSection (*with.entablature), l.peristyle.halfWidth,
	                                               l.peristyle.halfDepth, l.entablatureFoot)
	                             ->GetNFaces ();
	EXPECT_EQ (ring, 8u * 19u);
	std::size_t prisms = 0;
	for (const FriezePiece &q : l.frieze->triglyphs)
		prisms += BuildTriglyphMesh (*with.entablature, l.peristyle, q, l.entablatureFoot)->GetNFaces ();
	EXPECT_EQ (prisms, 32u * 76u);
	EXPECT_EQ (f1, f0 + ring + prisms);
	RecordProperty ("faces_without", static_cast<int> (f0));
	RecordProperty ("faces_with", static_cast<int> (f1));

	TempleLayout la, lb;
	std::vector<TempleMeshPart> pa, pb;
	const std::unique_ptr<Mesh> a = BuildTempleMesh (with, &la, &pa);
	const std::unique_ptr<Mesh> b = BuildTempleMesh (with, &lb, &pb);
	ASSERT_EQ (a->GetNVertices (), b->GetNVertices ());
	ASSERT_EQ (a->GetNFaces (), b->GetNFaces ());
	EXPECT_EQ (std::memcmp (a->GetVertices ().data (), b->GetVertices ().data (), a->GetVertices ().size () * sizeof (float)), 0);
	EXPECT_EQ (FacesOf (*a), FacesOf (*b));
	ASSERT_EQ (pa.size (), pb.size ());
	for (std::size_t k = 0; k < pa.size (); ++k)
	{
		EXPECT_TRUE (SameAddress (pa[k].address, pb[k].address)) << k;
		EXPECT_EQ (pa[k].firstVertex, pb[k].firstVertex) << k;
		EXPECT_EQ (pa[k].firstFace, pb[k].firstFace) << k;
	}
	ASSERT_TRUE (la.frieze && lb.frieze);
	ASSERT_EQ (la.frieze->triglyphs.size (), lb.frieze->triglyphs.size ());
	for (std::size_t k = 0; k < la.frieze->triglyphs.size (); ++k)
	{
		EXPECT_EQ (std::memcmp (&la.frieze->triglyphs[k].a0, &lb.frieze->triglyphs[k].a0, sizeof (double)), 0) << k;
		EXPECT_EQ (std::memcmp (&la.frieze->triglyphs[k].a1, &lb.frieze->triglyphs[k].a1, sizeof (double)), 0) << k;
	}
}

// CT1 : triglyphes d'angle (lot N1a), sous CornerTriglyphEqualMetopes. Prefixe
// des degres et des colonnes au bit pres celui de la meme construction sous
// AxisTriglyph (T11 transpose) ; 28 placements de triglyphe, dont 4 pieces
// d'angle en L ; recouvrements seulement dans la liste fermee L1 a L6 (T12) ;
// chaque piece d'angle a un contact L4 avec chacune des deux faces de frise de
// son angle.
TEST (TEST_cgarchi_classical_temple, the_corner_triglyphs_rest_on_the_ring_corner_with_listed_contacts)
{
	const TempleSettings axis = SyntheticTempleWithEntablature ();
	TempleSettings t = axis;
	t.entablature->corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;

	TempleLayout la, l;
	std::vector<TempleMeshPart> pa, parts;
	const std::unique_ptr<Mesh> ma = BuildTempleMesh (axis, &la, &pa);
	const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
	const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), ring = steps + nc;
	ASSERT_TRUE (l.frieze.has_value ());

	// Prefixe : degres et colonnes, au bit pres.
	ASSERT_GE (pa.size (), ring);
	ASSERT_GE (parts.size (), ring);
	for (std::size_t k = 0; k < ring; ++k)
	{
		EXPECT_TRUE (SameAddress (pa[k].address, parts[k].address)) << k;
		EXPECT_EQ (pa[k].firstVertex, parts[k].firstVertex) << k;
		EXPECT_EQ (pa[k].vertexCount, parts[k].vertexCount) << k;
		EXPECT_EQ (pa[k].firstFace, parts[k].firstFace) << k;
		EXPECT_EQ (pa[k].faceCount, parts[k].faceCount) << k;
	}
	const unsigned prefixVertices = parts[ring].firstVertex, prefixFaces = parts[ring].firstFace;
	ASSERT_EQ (pa[ring].firstVertex, prefixVertices);
	EXPECT_EQ (std::memcmp (ma->GetVertices ().data (), m->GetVertices ().data (), 3 * prefixVertices * sizeof (float)), 0);
	const std::vector<unsigned> fa = FacesOf (*ma), f = FacesOf (*m);
	EXPECT_TRUE (std::equal (f.begin (), f.begin () + 3 * prefixFaces, fa.begin ()));
	for (std::size_t c = 0; c < nc; ++c)
	{
		EXPECT_EQ (std::memcmp (&la.columns[c].x, &l.columns[c].x, sizeof (double)), 0) << c;
		EXPECT_EQ (std::memcmp (&la.columns[c].y, &l.columns[c].y, sizeof (double)), 0) << c;
	}

	// Adresses des triglyphes : {cote, 0 .. 2n - 3}, la piece d'angle au rang 0.
	ASSERT_EQ (parts.size (), ring + 1 + l.frieze->triglyphs.size ());
	EXPECT_EQ (l.frieze->triglyphs.size (), 28u);
	EXPECT_EQ (l.frieze->metopes.size (), 28u);
	const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
	std::size_t k = ring + 1;
	int cornerPieces = 0;
	for (const PeristyleSide side : sides)
	{
		const int n = side == PeristyleSide::Front || side == PeristyleSide::Back ? l.peristyle.front : l.peristyle.flank;
		for (int rank = 0; rank < 2 * n - 2; ++rank, ++k)
		{
			ASSERT_LT (k, parts.size ());
			EXPECT_TRUE (SameAddress (parts[k].address, {TempleElement::Triglyph, side, rank})) << k;
			const FriezePiece &q = l.frieze->triglyphs[k - ring - 1];
			EXPECT_EQ (q.kind, rank == 0 ? FriezeElement::CornerTriglyph : FriezeElement::Triglyph) << k;
			if (q.kind == FriezeElement::CornerTriglyph)
			{
				++cornerPieces;
				EXPECT_EQ (parts[k].faceCount, 80u) << k;
			}
			else
				EXPECT_EQ (parts[k].faceCount, 76u) << k;
		}
	}
	EXPECT_EQ (k, parts.size ());
	EXPECT_EQ (cornerPieces, 4);

	// Faces : avec moins sans entablement, 152 + 24 x 76 + 4 x 80.
	const std::size_t without = CountTempleMeshFaces (SyntheticTempleSettings ()), with = CountTempleMeshFaces (t);
	EXPECT_EQ (with, static_cast<std::size_t> (m->GetNFaces ()));
	EXPECT_EQ (with - without, 2296u);
	RecordProperty ("entablature_faces", static_cast<int> (with - without));
	RecordProperty ("faces", static_cast<int> (m->GetNFaces ()));

	// Recouvrements, classes comme en T12 ; plus L7 (registre TE5) : l'arete du
	// coin de chaque piece d'angle, soudee a l'arete d'onglet de la frise de
	// l'anneau, est portee par quatre faces.
	MeshValidationOptions options;
	options.maxRecordsPerKind = m->GetNFaces ();
	const MeshValidationReport r = ValidateMesh (*m, options);
	ASSERT_TRUE (r.selfIntersectionChecked);
	ASSERT_FALSE (r.selfIntersectionTruncated);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection &&
		    static_cast<MeshDefect> (kind) != MeshDefect::NonManifoldEdge)
			EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
	EXPECT_EQ (r.Count (MeshDefect::NonManifoldEdge), 4u);
	EXPECT_EQ (r.components, parts.size () - 4);

	const GreekEntablatureSettings &e = *t.entablature;
	const std::vector<double> z = GreekEntablatureBandBounds (e);
	const std::size_t fb = static_cast<std::size_t> (e.friezeBand);
	const float zRing = static_cast<float> (l.entablatureFoot);
	const float zLow = static_cast<float> (l.entablatureFoot + z[fb]);
	const float zHigh = static_cast<float> (l.entablatureFoot + z[fb + 1]);
	const double b = e.bands[fb].projection, hx = l.peristyle.halfWidth, hy = l.peristyle.halfDepth;
	{
		std::vector<std::size_t> owners;
		for (std::size_t p = 0; p < parts.size (); ++p)
			owners.insert (owners.end (), parts[p].faceCount, p);
		std::set<std::size_t> cornersOnMitre;
		std::size_t l7 = 0;
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::NonManifoldEdge)
				continue;
			++l7;
			ASSERT_EQ (rec.faces.size (), 4u);
			ASSERT_EQ (rec.vertices.size (), 2u);
			std::size_t onRing = 0, piece = 0;
			for (const unsigned face : rec.faces)
			{
				if (owners[face] == ring)
					++onRing;
				else
					piece = owners[face];
			}
			EXPECT_EQ (onRing, 2u);
			ASSERT_GT (piece, ring);
			EXPECT_EQ (l.frieze->triglyphs[piece - ring - 1].kind, FriezeElement::CornerTriglyph);
			cornersOnMitre.insert (piece);
			std::set<float> heights;
			for (const unsigned v : rec.vertices)
			{
				float q[3];
				m->GetVertex (v, q);
				EXPECT_EQ (std::fabs (q[0]), static_cast<float> (hx + b));
				EXPECT_EQ (std::fabs (q[1]), static_cast<float> (hy + b));
				heights.insert (q[2]);
			}
			EXPECT_EQ (heights, (std::set<float> {zLow, zHigh}));
		}
		EXPECT_EQ (l7, 4u);
		EXPECT_EQ (cornersOnMitre.size (), 4u);
	}
	auto onFriezePlane = [&] (unsigned face, PeristyleSide side) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			m->GetVertex (static_cast<unsigned> (m->GetFaceVertex (face, j)), q);
			const bool on = side == PeristyleSide::Front   ? q[1] == static_cast<float> (-(hy + b))
			                : side == PeristyleSide::Back  ? q[1] == static_cast<float> (hy + b)
			                : side == PeristyleSide::Right ? q[0] == static_cast<float> (hx + b)
			                                               : q[0] == static_cast<float> (-(hx + b));
			if (!on)
				return false;
		}
		return true;
	};
	std::vector<std::size_t> owner (m->GetNFaces ());
	for (std::size_t p = 0; p < parts.size (); ++p)
		for (unsigned face = parts[p].firstFace; face < parts[p].firstFace + parts[p].faceCount; ++face)
			owner[face] = p;
	const std::size_t nt = l.frieze->triglyphs.size ();
	std::vector<int> l4Own (nt, 0), l4Return (nt, 0);
	std::size_t records = 0, listed = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection)
			continue;
		++records;
		ASSERT_EQ (rec.faces.size (), 2u);
		unsigned f0 = rec.faces[0], f1 = rec.faces[1];
		std::size_t p0 = owner[f0], p1 = owner[f1];
		if (p0 > p1)
		{
			std::swap (p0, p1);
			std::swap (f0, f1);
		}
		if (p1 < steps && p1 == p0 + 1)
		{
			const float zs = static_cast<float> (l.krepis.steps[p1].zTop);
			EXPECT_TRUE (Horizontal (*m, f0, zs) && Horizontal (*m, f1, zs)) << "L1 : " << p0 << ", " << p1;
			++listed;
		}
		else if (p0 == 0 && p1 >= steps && p1 < ring)
		{
			const float zs = static_cast<float> (l.krepis.stylobateTop);
			EXPECT_TRUE (Horizontal (*m, f0, zs) && Horizontal (*m, f1, zs)) << "L2 : colonne " << p1 - steps;
			++listed;
		}
		else if (p0 >= steps && p0 < ring && p1 == ring)
		{
			EXPECT_TRUE (Horizontal (*m, f0, zRing) && Horizontal (*m, f1, zRing)) << "L3 : colonne " << p0 - steps;
			++listed;
		}
		else if (p0 == ring && p1 > ring)
		{
			const std::size_t q = p1 - ring - 1;
			const PeristyleSide side = l.frieze->triglyphs[q].side;
			const PeristyleSide previous = static_cast<PeristyleSide> ((static_cast<int> (side) + 3) % 4);
			const bool corner = l.frieze->triglyphs[q].kind == FriezeElement::CornerTriglyph;
			if ((Horizontal (*m, f0, zLow) && Horizontal (*m, f1, zLow)) || (Horizontal (*m, f0, zHigh) && Horizontal (*m, f1, zHigh)))
				++listed;
			else if (onFriezePlane (f0, side) && onFriezePlane (f1, side))
			{
				++l4Own[q];
				++listed;
			}
			else if (corner && onFriezePlane (f0, previous) && onFriezePlane (f1, previous))
			{
				++l4Return[q];
				++listed;
			}
			else
				ADD_FAILURE () << "anneau et triglyphe " << q << " : recouvrement hors de L4 a L6";
		}
		else
			ADD_FAILURE () << "recouvrement hors liste : pieces " << p0 << " et " << p1;
	}
	EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
	EXPECT_EQ (listed, records);
	for (std::size_t q = 0; q < nt; ++q)
	{
		EXPECT_GE (l4Own[q], 1) << "triglyphe " << q << " sans contact L4";
		if (l.frieze->triglyphs[q].kind == FriezeElement::CornerTriglyph)
			EXPECT_GE (l4Return[q], 1) << "piece d'angle " << q << " sans contact L4 sur la frise du cote precedent";
		else
			EXPECT_EQ (l4Return[q], 0) << q;
	}
	RecordProperty ("records", static_cast<int> (records));
}

// CT2 : ornements (lot N2a), sous AxisTriglyph puis CornerTriglyphEqualMetopes.
// Prefixe au bit pres celui du meme temple sans regulae ni mutules ; faces des
// ornements 7 344 et 7 232 ; adresses {Regula, cote, rang du triglyphe} puis
// {Mutule, cote, rang} ; recouvrements seulement dans la liste fermee L1 a L14
// (registre TE5) : au moins un L8 et un L9 par regula (L9 sur les deux faces de
// son angle pour une regula d'angle), un L10 par goutte, un L11, un L12 et un
// L14 par mutule ; L7 et L13 (D3-64) en aretes non manifold exactes.
TEST (TEST_cgarchi_classical_temple, the_ornaments_rest_on_the_entablature_with_listed_contacts)
{
	for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes})
	{
		const bool axis = strategy == DoricCornerStrategy::AxisTriglyph;
		const std::string suffix = axis ? "_axis" : "_equal";
		SCOPED_TRACE (suffix);
		TempleSettings t = cgarchi_test::SyntheticTempleWithOrnaments ();
		t.entablature->corner = strategy;
		TempleSettings plain = t;
		plain.entablature->regulae.reset ();
		plain.entablature->mutules.reset ();

		TempleLayout lp, l;
		std::vector<TempleMeshPart> pp, parts;
		const std::unique_ptr<Mesh> mp = BuildTempleMesh (plain, &lp, &pp);
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		ASSERT_FALSE (lp.ornaments.has_value ());
		ASSERT_TRUE (l.ornaments.has_value ());
		ASSERT_TRUE (l.frieze.has_value ());

		// Prefixe : toutes les parties du temple sans ornements, au bit pres.
		ASSERT_GE (parts.size (), pp.size ());
		for (std::size_t k = 0; k < pp.size (); ++k)
		{
			EXPECT_TRUE (SameAddress (pp[k].address, parts[k].address)) << k;
			EXPECT_EQ (pp[k].firstVertex, parts[k].firstVertex) << k;
			EXPECT_EQ (pp[k].vertexCount, parts[k].vertexCount) << k;
			EXPECT_EQ (pp[k].firstFace, parts[k].firstFace) << k;
			EXPECT_EQ (pp[k].faceCount, parts[k].faceCount) << k;
		}
		ASSERT_LE (mp->GetNVertices (), m->GetNVertices ());
		EXPECT_EQ (std::memcmp (mp->GetVertices ().data (), m->GetVertices ().data (), mp->GetVertices ().size () * sizeof (float)), 0);
		const std::vector<unsigned> fp = FacesOf (*mp), f = FacesOf (*m);
		EXPECT_TRUE (std::equal (fp.begin (), fp.end (), f.begin ()));

		// Faces des ornements, et CountTempleMeshFaces.
		const std::size_t with = CountTempleMeshFaces (t), without = CountTempleMeshFaces (plain);
		EXPECT_EQ (with, static_cast<std::size_t> (m->GetNFaces ()));
		EXPECT_EQ (without, static_cast<std::size_t> (mp->GetNFaces ()));
		EXPECT_EQ (with - without, axis ? 7344u : 7232u);
		RecordProperty ("ornament_faces" + suffix, static_cast<int> (with - without));

		// Adresses et tailles des parties d'ornements.
		const OrnamentLayout &o = *l.ornaments;
		const std::size_t nr = o.regulae.size (), nm = o.mutules.size (), first = pp.size ();
		EXPECT_EQ (nr, axis ? 32u : 28u);
		EXPECT_EQ (nm, axis ? 68u : 60u);
		ASSERT_EQ (parts.size (), first + nr + nm);
		for (std::size_t k = 0; k < nr; ++k)
		{
			const OrnamentPiece &r = o.regulae[k];
			EXPECT_TRUE (SameAddress (parts[first + k].address, {TempleElement::Regula, r.side, r.rank})) << k;
			EXPECT_TRUE (SameAddress (parts[first + k].address,
			                          {TempleElement::Regula, l.frieze->triglyphs[k].side, l.frieze->triglyphs[k].rank})) << k;
			EXPECT_EQ (parts[first + k].faceCount, r.kind == OrnamentElement::CornerRegula ? 404u : 204u) << k;
		}
		for (const PeristyleSide side : {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left})
		{
			const int n = side == PeristyleSide::Front || side == PeristyleSide::Back ? l.peristyle.front : l.peristyle.flank;
			int rank = 0;
			for (std::size_t k = 0; k < nm; ++k)
				if (o.mutules[k].side == side)
				{
					EXPECT_TRUE (SameAddress (parts[first + nr + k].address, {TempleElement::Mutule, side, rank})) << k;
					EXPECT_EQ (parts[first + nr + k].faceCount, 12u) << k;
					++rank;
				}
			EXPECT_EQ (rank, axis ? 4 * n - 1 : 4 * n - 3);
		}

		// Validation : SelfIntersection, et sous la strategie d'angle les aretes
		// non manifold L7 et L13 ; aucun autre defaut, aucun sommet non manifold.
		MeshValidationOptions options;
		options.maxRecordsPerKind = m->GetNFaces ();
		const MeshValidationReport r = ValidateMesh (*m, options);
		ASSERT_TRUE (r.selfIntersectionChecked);
		ASSERT_FALSE (r.selfIntersectionTruncated);
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection &&
			    static_cast<MeshDefect> (kind) != MeshDefect::NonManifoldEdge)
				EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
		EXPECT_EQ (r.Count (MeshDefect::NonManifoldVertex), 0u);
		EXPECT_EQ (r.Count (MeshDefect::NonManifoldEdge), axis ? 0u : 8u);
		EXPECT_EQ (o.guttae.size (), 192u);
		EXPECT_EQ (r.components, parts.size () + o.guttae.size () - (axis ? 0u : 8u));
		RecordProperty ("components" + suffix, static_cast<int> (r.components));

		const GreekEntablatureSettings &e = *t.entablature;
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		const std::size_t fb = static_cast<std::size_t> (e.friezeBand), mb = static_cast<std::size_t> (e.mutules->band);
		const double foot = l.entablatureFoot, hx = l.peristyle.halfWidth, hy = l.peristyle.halfDepth;
		const double b = e.bands[fb].projection, dos = e.bands[fb - 2].projection, R = e.regulae->face;
		const double mutuleBack = e.bands[mb].projection;
		const float zRing = static_cast<float> (foot), zLow = static_cast<float> (foot + z[fb]),
		            zHigh = static_cast<float> (foot + z[fb + 1]), zTaenia = static_cast<float> (foot + z[fb - 1]),
		            zMutuleLow = static_cast<float> (foot + z[mb]), zMutuleHigh = static_cast<float> (foot + z[mb + 1]);
		const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), ring = steps + nc;
		const std::size_t nt = l.frieze->triglyphs.size (), firstRegula = ring + 1 + nt, firstMutule = firstRegula + nr;
		ASSERT_EQ (first, firstRegula);
		std::vector<std::size_t> owner (m->GetNFaces ());
		for (std::size_t p = 0; p < parts.size (); ++p)
			for (unsigned face = parts[p].firstFace; face < parts[p].firstFace + parts[p].faceCount; ++face)
				owner[face] = p;

		// Aretes non manifold : L7 (piece d'angle de la frise) et L13 (regula
		// d'angle), chacune portee par 2 faces de l'anneau et 2 d'une piece.
		{
			std::set<std::size_t> l7Pieces, l13Pieces;
			for (const MeshDefectRecord &rec : r.records)
			{
				if (rec.kind != MeshDefect::NonManifoldEdge)
					continue;
				ASSERT_EQ (rec.faces.size (), 4u);
				ASSERT_EQ (rec.vertices.size (), 2u);
				std::size_t onRing = 0, piece = 0;
				for (const unsigned face : rec.faces)
				{
					if (owner[face] == ring)
						++onRing;
					else
						piece = owner[face];
				}
				EXPECT_EQ (onRing, 2u);
				for (const unsigned face : rec.faces)
					if (owner[face] != ring)
						EXPECT_EQ (owner[face], piece);
				std::array<float, 3> q0, q1;
				m->GetVertex (rec.vertices[0], q0.data ());
				m->GetVertex (rec.vertices[1], q1.data ());
				if (piece > ring && piece < firstRegula)
				{
					EXPECT_EQ (l.frieze->triglyphs[piece - ring - 1].kind, FriezeElement::CornerTriglyph);
					l7Pieces.insert (piece);
					for (const auto &q : {q0, q1})
					{
						EXPECT_EQ (std::fabs (q[0]), static_cast<float> (hx + b));
						EXPECT_EQ (std::fabs (q[1]), static_cast<float> (hy + b));
					}
					EXPECT_EQ ((std::set<float> {q0[2], q1[2]}), (std::set<float> {zLow, zHigh}));
				}
				else if (piece >= firstRegula && piece < firstMutule)
				{
					const OrnamentPiece &reg = o.regulae[piece - firstRegula];
					EXPECT_EQ (reg.kind, OrnamentElement::CornerRegula);
					l13Pieces.insert (piece);
					// Faces porteuses de la regula : son couvercle du dessus.
					for (const unsigned face : rec.faces)
						if (owner[face] == piece)
							EXPECT_TRUE (Horizontal (*m, face, zTaenia));
					EXPECT_EQ (q0[2], zTaenia);
					EXPECT_EQ (q1[2], zTaenia);
					std::set<std::pair<float, float>> ends;
					for (const auto &q : {q0, q1})
						ends.insert ({std::fabs (q[0]), std::fabs (q[1])});
					EXPECT_EQ (ends, (std::set<std::pair<float, float>> {{static_cast<float> (hx + dos), static_cast<float> (hy + dos)},
					                                                    {static_cast<float> (hx + R), static_cast<float> (hy + R)}}));
					// Les deux extremites dans le quadrant de l'angle de la regula.
					EXPECT_TRUE ((q0[0] < 0.0f) == (q1[0] < 0.0f) && (q0[1] < 0.0f) == (q1[1] < 0.0f));
				}
				else
					ADD_FAILURE () << "arete non manifold hors de L7 et L13 : piece " << piece;
			}
			EXPECT_EQ (l7Pieces.size (), axis ? 0u : 4u);
			EXPECT_EQ (l13Pieces.size (), axis ? 0u : 4u);
			RecordProperty ("l13_edges" + suffix, static_cast<int> (l13Pieces.size ()));
		}

		// Plans verticaux d'un cote, par les expressions de l'anneau.
		auto onPlane = [&] (unsigned face, PeristyleSide side, double d) {
			for (unsigned j = 0; j < 3; ++j)
			{
				float q[3];
				m->GetVertex (static_cast<unsigned> (m->GetFaceVertex (face, j)), q);
				const bool on = side == PeristyleSide::Front   ? q[1] == static_cast<float> (-(hy + d))
				                : side == PeristyleSide::Back  ? q[1] == static_cast<float> (hy + d)
				                : side == PeristyleSide::Right ? q[0] == static_cast<float> (hx + d)
				                                               : q[0] == static_cast<float> (-(hx + d));
				if (!on)
					return false;
			}
			return true;
		};
		auto previousOf = [] (PeristyleSide side) { return static_cast<PeristyleSide> ((static_cast<int> (side) + 3) % 4); };
		std::vector<int> l8 (nr, 0), l9 (nr, 0), l9Return (nr, 0), l11 (nm, 0), l12 (nm, 0), l14 (nm, 0);
		std::vector<int> l10 (o.guttae.size (), 0);
		std::size_t records = 0, listed = 0;
		std::array<std::size_t, 15> classes {};
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::SelfIntersection)
				continue;
			++records;
			ASSERT_EQ (rec.faces.size (), 2u);
			unsigned f0 = rec.faces[0], f1 = rec.faces[1];
			std::size_t p0 = owner[f0], p1 = owner[f1];
			if (p0 > p1 || (p0 == p1 && f0 > f1))
			{
				std::swap (p0, p1);
				std::swap (f0, f1);
			}
			auto both = [&] (float zz) { return Horizontal (*m, f0, zz) && Horizontal (*m, f1, zz); };
			std::size_t cls = 0;
			if (p1 < steps && p1 == p0 + 1 && both (static_cast<float> (l.krepis.steps[p1].zTop)))
				cls = 1;
			else if (p0 == 0 && p1 >= steps && p1 < ring && both (static_cast<float> (l.krepis.stylobateTop)))
				cls = 2;
			else if (p0 >= steps && p0 < ring && p1 == ring && both (zRing))
				cls = 3;
			else if (p0 == ring && p1 > ring && p1 < firstRegula)
			{
				const FriezePiece &q = l.frieze->triglyphs[p1 - ring - 1];
				if (both (zLow))
					cls = 5;
				else if (both (zHigh))
					cls = 6;
				else if ((onPlane (f0, q.side, b) && onPlane (f1, q.side, b)) ||
				         (q.kind == FriezeElement::CornerTriglyph && onPlane (f0, previousOf (q.side), b) &&
				          onPlane (f1, previousOf (q.side), b)))
					cls = 4;
			}
			else if (p0 == ring && p1 >= firstRegula && p1 < firstMutule)
			{
				const std::size_t k = p1 - firstRegula;
				const OrnamentPiece &reg = o.regulae[k];
				if (both (zTaenia))
				{
					cls = 8;
					++l8[k];
				}
				else if (onPlane (f0, reg.side, dos) && onPlane (f1, reg.side, dos))
				{
					cls = 9;
					++l9[k];
				}
				else if (reg.kind == OrnamentElement::CornerRegula && onPlane (f0, previousOf (reg.side), dos) &&
				         onPlane (f1, previousOf (reg.side), dos))
				{
					cls = 9;
					++l9Return[k];
				}
			}
			else if (p0 == p1 && p0 >= firstRegula && p0 < firstMutule)
			{
				// Goutte (f1, faces apres celles de la regula) sous le dessous de sa regula.
				const std::size_t k = p0 - firstRegula;
				const OrnamentPiece &reg = o.regulae[k];
				const unsigned bodyFaces = reg.kind == OrnamentElement::CornerRegula ? 20u : 12u;
				const float zBottom = static_cast<float> (foot + reg.z0);
				const unsigned local0 = f0 - parts[p0].firstFace, local1 = f1 - parts[p0].firstFace;
				if (local0 < bodyFaces && local1 >= bodyFaces && both (zBottom))
				{
					cls = 10;
					++l10[static_cast<std::size_t> (reg.firstGutta) + (local1 - bodyFaces) / 32u];
				}
			}
			else if (p0 == ring && p1 >= firstMutule)
			{
				const std::size_t k = p1 - firstMutule;
				const OrnamentPiece &mut = o.mutules[k];
				if (both (zMutuleHigh))
				{
					cls = 11;
					++l11[k];
				}
				else if (onPlane (f0, mut.side, mutuleBack) && onPlane (f1, mut.side, mutuleBack))
				{
					cls = 12;
					++l12[k];
				}
				else if (both (zMutuleLow))
				{
					cls = 14;
					++l14[k];
				}
			}
			if (cls == 0)
				ADD_FAILURE () << "recouvrement hors liste : pieces " << p0 << " et " << p1 << ", faces " << f0 << " et " << f1;
			else
			{
				++listed;
				++classes[cls];
			}
		}
		EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
		EXPECT_EQ (listed, records);
		for (std::size_t k = 0; k < nr; ++k)
		{
			EXPECT_GE (l8[k], 1) << "regula " << k << " sans L8";
			EXPECT_GE (l9[k], 1) << "regula " << k << " sans L9";
			if (o.regulae[k].kind == OrnamentElement::CornerRegula)
				EXPECT_GE (l9Return[k], 1) << "regula d'angle " << k << " sans L9 sur la face du cote precedent";
			else
				EXPECT_EQ (l9Return[k], 0) << k;
		}
		for (std::size_t g = 0; g < l10.size (); ++g)
			EXPECT_GE (l10[g], 1) << "goutte " << g << " sans L10";
		for (std::size_t k = 0; k < nm; ++k)
		{
			EXPECT_GE (l11[k], 1) << "mutule " << k << " sans L11";
			EXPECT_GE (l12[k], 1) << "mutule " << k << " sans L12";
			EXPECT_GE (l14[k], 1) << "mutule " << k << " sans L14";
		}
		for (std::size_t c = 1; c < classes.size (); ++c)
			if (classes[c])
				RecordProperty ("l" + std::to_string (c) + "_records" + suffix, static_cast<int> (classes[c]));
	}
}

// CT3 : sous-face inclinee et gouttes des mutules (lot N3a), sous AxisTriglyph
// puis CornerTriglyphEqualMetopes. Prefixe au bit pres celui du meme temple sans
// regulae ni mutules ; anneau de 160 faces ; ornements 33 456 et 30 272 ;
// validation a 1e-4 m (D3-80) : recouvrements seulement dans la liste fermee
// L1 a L13 (L11 : dessus du mutule sur la face inclinee de l'anneau, memes
// couples (d, z)), L12, L15 ; aucun
// L14 ; au moins un L15 par goutte de mutule. Garde de D3-80 : a la tolerance
// par defaut et a 1e-4, memes sommets soudes, composantes, defauts hors
// SelfIntersection et paires d'unites en contact (une unite : une partie, ou
// une goutte de regula ou de mutule).
TEST (TEST_cgarchi_classical_temple, the_inclined_soffit_and_the_mutule_guttae_rest_with_listed_contacts)
{
	for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes})
	{
		const bool axis = strategy == DoricCornerStrategy::AxisTriglyph;
		const std::string suffix = axis ? "_axis" : "_equal";
		SCOPED_TRACE (suffix);
		TempleSettings t = cgarchi_test::SyntheticTempleWithInclinedSoffit ();
		t.entablature->corner = strategy;
		TempleSettings plain = t;
		plain.entablature->regulae.reset ();
		plain.entablature->mutules.reset ();

		TempleLayout lp, l;
		std::vector<TempleMeshPart> pp, parts;
		const std::unique_ptr<Mesh> mp = BuildTempleMesh (plain, &lp, &pp);
		const std::unique_ptr<Mesh> m = BuildTempleMesh (t, &l, &parts);
		ASSERT_TRUE (l.ornaments.has_value ());
		ASSERT_TRUE (l.frieze.has_value ());

		// Prefixe : krepis et colonnes au bit pres (l'anneau, lui, change).
		const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), ring = steps + nc;
		ASSERT_GE (pp.size (), ring);
		for (std::size_t k = 0; k < ring; ++k)
		{
			EXPECT_TRUE (SameAddress (pp[k].address, parts[k].address)) << k;
			EXPECT_EQ (pp[k].firstFace, parts[k].firstFace) << k;
			EXPECT_EQ (pp[k].faceCount, parts[k].faceCount) << k;
		}
		const unsigned prefixVertices = parts[ring].firstVertex;
		EXPECT_EQ (std::memcmp (mp->GetVertices ().data (), m->GetVertices ().data (), 3u * prefixVertices * sizeof (float)), 0);
		EXPECT_EQ (parts[ring].faceCount, 160u);

		// Faces des ornements, et CountTempleMeshFaces.
		const std::size_t with = CountTempleMeshFaces (t), without = CountTempleMeshFaces (plain);
		EXPECT_EQ (with, static_cast<std::size_t> (m->GetNFaces ()));
		EXPECT_EQ (without + 8u, static_cast<std::size_t> (with - (axis ? 33456u : 30272u)));
		RecordProperty ("ornament_faces" + suffix, static_cast<int> (with - without - 8u));
		const OrnamentLayout &o = *l.ornaments;
		const std::size_t nr = o.regulae.size (), nm = o.mutules.size ();
		const std::size_t nt = l.frieze->triglyphs.size (), firstRegula = ring + 1 + nt, firstMutule = firstRegula + nr;
		ASSERT_EQ (parts.size (), firstMutule + nm);
		EXPECT_EQ (nm, axis ? 68u : 60u);
		EXPECT_EQ (o.mutuleGuttae.size (), 12u * nm);
		for (std::size_t k = 0; k < nm; ++k)
			EXPECT_EQ (parts[firstMutule + k].faceCount, 12u + 12u * 32u) << k;

		// Unites de contact : partie, ou goutte (plage de 32 faces).
		std::vector<std::size_t> owner (m->GetNFaces ()), unit (m->GetNFaces ());
		for (std::size_t q = 0; q < parts.size (); ++q)
			for (unsigned face = parts[q].firstFace; face < parts[q].firstFace + parts[q].faceCount; ++face)
			{
				owner[face] = q;
				unit[face] = q;
				const unsigned local = face - parts[q].firstFace;
				if (q >= firstRegula && q < firstMutule)
				{
					const OrnamentPiece &reg = o.regulae[q - firstRegula];
					const unsigned body = reg.kind == OrnamentElement::CornerRegula ? 20u : 12u;
					if (local >= body)
						unit[face] = parts.size () + static_cast<std::size_t> (reg.firstGutta) + (local - body) / 32u;
				}
				else if (q >= firstMutule && local >= 12u)
					unit[face] = parts.size () + o.guttae.size () + static_cast<std::size_t> (o.mutules[q - firstMutule].firstGutta) +
					             (local - 12u) / 32u;
			}

		MeshValidationOptions byDefault, explicitTolerance;
		byDefault.maxRecordsPerKind = m->GetNFaces ();
		explicitTolerance.maxRecordsPerKind = m->GetNFaces ();
		explicitTolerance.weldTolerance = 1e-4;
		const MeshValidationReport r0 = ValidateMesh (*m, byDefault), r = ValidateMesh (*m, explicitTolerance);
		ASSERT_TRUE (r0.selfIntersectionChecked && r.selfIntersectionChecked);
		ASSERT_FALSE (r0.selfIntersectionTruncated || r.selfIntersectionTruncated);
		RecordProperty ("default_weld_tolerance" + suffix, std::to_string (r0.weldTolerance));

		// Garde de D3-80.
		auto contacts = [&] (const MeshValidationReport &report) {
			std::set<std::pair<std::size_t, std::size_t>> pairs;
			for (const MeshDefectRecord &rec : report.records)
				if (rec.kind == MeshDefect::SelfIntersection)
					pairs.insert ({std::min (unit[rec.faces[0]], unit[rec.faces[1]]), std::max (unit[rec.faces[0]], unit[rec.faces[1]])});
			return pairs;
		};
		EXPECT_EQ (r0.weldedVertices, r.weldedVertices);
		EXPECT_EQ (r0.components, r.components);
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection)
				EXPECT_EQ (r0.counts[kind], r.counts[kind]) << MeshDefectName (static_cast<MeshDefect> (kind));
		const std::set<std::pair<std::size_t, std::size_t>> pairs0 = contacts (r0), pairs = contacts (r);
		EXPECT_TRUE (pairs0 == pairs) << "paires d'unites en contact : " << pairs0.size () << " a la tolerance par defaut, "
		                              << pairs.size () << " a 1e-4";
		RecordProperty ("contact_pairs" + suffix, static_cast<int> (pairs.size ()));

		// Defauts a 1e-4 : SelfIntersection, et sous la strategie d'angle les
		// aretes non manifold L7 et L13.
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			if (static_cast<MeshDefect> (kind) != MeshDefect::SelfIntersection &&
			    static_cast<MeshDefect> (kind) != MeshDefect::NonManifoldEdge)
				EXPECT_EQ (r.counts[kind], 0u) << MeshDefectName (static_cast<MeshDefect> (kind));
		EXPECT_EQ (r.Count (MeshDefect::NonManifoldEdge), axis ? 0u : 8u);
		EXPECT_EQ (r.components, parts.size () + o.guttae.size () + o.mutuleGuttae.size () - (axis ? 0u : 8u));
		RecordProperty ("components" + suffix, static_cast<int> (r.components));

		const GreekEntablatureSettings &e = *t.entablature;
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		const std::size_t fb = static_cast<std::size_t> (e.friezeBand);
		const double foot = l.entablatureFoot, hx = l.peristyle.halfWidth, hy = l.peristyle.halfDepth;
		const double b = e.bands[fb].projection, dos = e.bands[fb - 2].projection;
		const float zRing = static_cast<float> (foot), zLow = static_cast<float> (foot + z[fb]),
		            zHigh = static_cast<float> (foot + z[fb + 1]), zTaenia = static_cast<float> (foot + z[fb - 1]);
		auto perpendicular = [&] (PeristyleSide side, double d) {
			return static_cast<float> (side == PeristyleSide::Front   ? -(hy + d)
			                           : side == PeristyleSide::Back  ? hy + d
			                           : side == PeristyleSide::Right ? hx + d
			                                                          : -(hx + d));
		};
		auto onPlane = [&] (unsigned face, PeristyleSide side, double d) {
			for (unsigned j = 0; j < 3; ++j)
			{
				float q[3];
				m->GetVertex (static_cast<unsigned> (m->GetFaceVertex (face, j)), q);
				const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
				if ((alongX ? q[1] : q[0]) != perpendicular (side, d))
					return false;
			}
			return true;
		};
		// Chaque sommet de la face sur l'un des couples (d, z) du cote, au bit pres.
		auto onCouples = [&] (unsigned face, PeristyleSide side, double d0, double z0, double d1, double z1) {
			for (unsigned j = 0; j < 3; ++j)
			{
				float q[3];
				m->GetVertex (static_cast<unsigned> (m->GetFaceVertex (face, j)), q);
				const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
				const float across = alongX ? q[1] : q[0];
				if (!((across == perpendicular (side, d0) && q[2] == static_cast<float> (foot + z0)) ||
				      (across == perpendicular (side, d1) && q[2] == static_cast<float> (foot + z1))))
					return false;
			}
			return true;
		};
		auto previousOf = [] (PeristyleSide side) { return static_cast<PeristyleSide> ((static_cast<int> (side) + 3) % 4); };
		std::vector<int> l8 (nr, 0), l9 (nr, 0), l11 (nm, 0), l12 (nm, 0);
		std::vector<int> l10 (o.guttae.size (), 0), l15 (o.mutuleGuttae.size (), 0);
		std::size_t records = 0, listed = 0;
		std::array<std::size_t, 16> classes {};
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::SelfIntersection)
				continue;
			++records;
			ASSERT_EQ (rec.faces.size (), 2u);
			unsigned f0 = rec.faces[0], f1 = rec.faces[1];
			std::size_t p0 = owner[f0], p1 = owner[f1];
			if (p0 > p1 || (p0 == p1 && f0 > f1))
			{
				std::swap (p0, p1);
				std::swap (f0, f1);
			}
			auto both = [&] (float zz) { return Horizontal (*m, f0, zz) && Horizontal (*m, f1, zz); };
			std::size_t cls = 0;
			if (p1 < steps && p1 == p0 + 1 && both (static_cast<float> (l.krepis.steps[p1].zTop)))
				cls = 1;
			else if (p0 == 0 && p1 >= steps && p1 < ring && both (static_cast<float> (l.krepis.stylobateTop)))
				cls = 2;
			else if (p0 >= steps && p0 < ring && p1 == ring && both (zRing))
				cls = 3;
			else if (p0 == ring && p1 > ring && p1 < firstRegula)
			{
				const FriezePiece &q = l.frieze->triglyphs[p1 - ring - 1];
				if (both (zLow))
					cls = 5;
				else if (both (zHigh))
					cls = 6;
				else if ((onPlane (f0, q.side, b) && onPlane (f1, q.side, b)) ||
				         (q.kind == FriezeElement::CornerTriglyph && onPlane (f0, previousOf (q.side), b) &&
				          onPlane (f1, previousOf (q.side), b)))
					cls = 4;
			}
			else if (p0 == ring && p1 >= firstRegula && p1 < firstMutule)
			{
				const std::size_t k = p1 - firstRegula;
				const OrnamentPiece &reg = o.regulae[k];
				if (both (zTaenia))
				{
					cls = 8;
					++l8[k];
				}
				else if ((onPlane (f0, reg.side, dos) && onPlane (f1, reg.side, dos)) ||
				         (reg.kind == OrnamentElement::CornerRegula && onPlane (f0, previousOf (reg.side), dos) &&
				          onPlane (f1, previousOf (reg.side), dos)))
				{
					cls = 9;
					++l9[k];
				}
			}
			else if (p0 == p1 && p0 >= firstRegula && p0 < firstMutule)
			{
				const std::size_t k = p0 - firstRegula;
				const OrnamentPiece &reg = o.regulae[k];
				const unsigned bodyFaces = reg.kind == OrnamentElement::CornerRegula ? 20u : 12u;
				const unsigned local0 = f0 - parts[p0].firstFace, local1 = f1 - parts[p0].firstFace;
				if (local0 < bodyFaces && local1 >= bodyFaces && both (static_cast<float> (foot + reg.z0)))
				{
					cls = 10;
					++l10[static_cast<std::size_t> (reg.firstGutta) + (local1 - bodyFaces) / 32u];
				}
			}
			else if (p0 == ring && p1 >= firstMutule)
			{
				const std::size_t k = p1 - firstMutule;
				const OrnamentPiece &mut = o.mutules[k];
				if (onCouples (f0, mut.side, mut.d0, mut.z1 + mut.rise, mut.d1, mut.z1) &&
				    onCouples (f1, mut.side, mut.d0, mut.z1 + mut.rise, mut.d1, mut.z1))
				{
					cls = 11;
					++l11[k];
				}
				else if (onPlane (f0, mut.side, mut.d0) && onPlane (f1, mut.side, mut.d0))
				{
					cls = 12;
					++l12[k];
				}
				else if (both (static_cast<float> (foot + mut.z0)))
					cls = 14;   // classe de N2, affirmee absente ci-dessous
			}
			else if (p0 == p1 && p0 >= firstMutule)
			{
				// Tete d'une goutte (triangle d'eventail de tete, 4 i + 3) sur le
				// dessous de son mutule.
				const std::size_t k = p0 - firstMutule;
				const OrnamentPiece &mut = o.mutules[k];
				const unsigned local0 = f0 - parts[p0].firstFace, local1 = f1 - parts[p0].firstFace;
				if (local0 < 12u && local1 >= 12u && (local1 - 12u) % 4u == 3u &&
				    onCouples (f0, mut.side, mut.d0, mut.z0 + mut.rise, mut.d1, mut.z0))
				{
					cls = 15;
					++l15[static_cast<std::size_t> (mut.firstGutta) + (local1 - 12u) / 32u];
				}
			}
			if (cls == 0)
				ADD_FAILURE () << "recouvrement hors liste : pieces " << p0 << " et " << p1 << ", faces " << f0 << " et " << f1;
			else
			{
				++listed;
				++classes[cls];
			}
		}
		EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
		EXPECT_EQ (listed, records);
		EXPECT_EQ (classes[14], 0u);
		for (std::size_t k = 0; k < nr; ++k)
		{
			EXPECT_GE (l8[k], 1) << "regula " << k << " sans L8";
			EXPECT_GE (l9[k], 1) << "regula " << k << " sans L9";
		}
		for (std::size_t g = 0; g < l10.size (); ++g)
			EXPECT_GE (l10[g], 1) << "goutte " << g << " sans L10";
		for (std::size_t k = 0; k < nm; ++k)
		{
			EXPECT_GE (l11[k], 1) << "mutule " << k << " sans L11";
			EXPECT_GE (l12[k], 1) << "mutule " << k << " sans L12";
		}
		for (std::size_t g = 0; g < l15.size (); ++g)
			EXPECT_GE (l15[g], 1) << "goutte de mutule " << g << " sans L15";
		for (std::size_t c = 1; c < classes.size (); ++c)
			if (classes[c])
				RecordProperty ("l" + std::to_string (c) + "_records" + suffix, static_cast<int> (classes[c]));
	}
}
