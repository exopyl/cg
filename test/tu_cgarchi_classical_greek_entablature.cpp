#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_partition.h>
#include <cgarchi/classical_peristyle.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Entablement peripherique grec : anneau balaye, section en bandes, frise
//  monotriglyphe, prismes des triglyphes
// ===========================================================================
//
// Reglages d'essai : SyntheticEntablatureSettings (greek_doric_test_support.h),
// sans valeur d'un monument, sur le peristyle synthetique de 4 x 5 colonnes
// (SyntheticTempleSettings). La section attendue du jeu synthetique est ecrite
// ici sommet par sommet, independamment de GreekEntablatureSection.

using namespace cgarchi::classical;
using cgarchi_test::SyntheticEntablatureSettings;
using cgarchi_test::SyntheticTempleSettings;

namespace {

bool SameBits (double a, double b)
{
	return std::memcmp (&a, &b, sizeof (double)) == 0;
}

PeristyleLayout SyntheticPeristyle ()
{
	return LayoutPeristyle (SyntheticTempleSettings ().peristyle);
}

// Cotes des jonctions du jeu synthetique, somme courante ecrite ici.
std::vector<double> SyntheticBounds ()
{
	const double h[] = {0.55, 0.07, 0.06, 0.58, 0.06, 0.03, 0.05, 0.17, 0.03};
	std::vector<double> z {0.0};
	for (const double v : h)
		z.push_back (z.back () + v);
	return z;
}

// Section du jeu synthetique, sens direct : 19 sommets (9 bandes, une
// jonction a saillie egale derriere la face inclinee de l'architrave).
std::vector<RingSectionPoint> SyntheticSection ()
{
	const std::vector<double> z = SyntheticBounds ();
	return {{0.475, z[0]}, {0.47, z[1]},  {0.47, z[2]},  {0.50, z[2]},  {0.50, z[3]},  {0.43, z[3]},  {0.43, z[4]},
	        {0.495, z[4]}, {0.495, z[5]}, {0.505, z[5]}, {0.505, z[6]}, {0.78, z[6]},  {0.78, z[7]},  {0.83, z[7]},
	        {0.83, z[8]},  {0.85, z[8]},  {0.85, z[9]},  {-0.44, z[9]}, {-0.44, z[0]}};
}

// Volume du balayage par un oracle independant de RectangularRingVolume :
// eventail de triangles depuis le premier sommet, chacun d'aire signee a et de
// centroide u_t, perimetre du rectangle a la saillie u : 4 (hx + hy) + 8 u.
double FanVolume (const std::vector<RingSectionPoint> &s, double hx, double hy)
{
	double v = 0.0;
	for (std::size_t i = 1; i + 1 < s.size (); ++i)
	{
		const RingSectionPoint &a = s[0], &b = s[i], &c = s[i + 1];
		const double area = 0.5 * ((b.u - a.u) * (c.z - a.z) - (c.u - a.u) * (b.z - a.z));
		const double ut = (a.u + b.u + c.u) / 3.0;
		v += area * (4.0 * (hx + hy) + 8.0 * ut);
	}
	return v;
}

std::size_t UniqueEdges (const Mesh &m)
{
	std::set<std::pair<int, int>> edges;
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
		for (unsigned j = 0; j < 3; ++j)
		{
			const int a = m.GetFaceVertex (f, j), b = m.GetFaceVertex (f, (j + 1) % 3);
			edges.insert ({std::min (a, b), std::max (a, b)});
		}
	return edges.size ();
}

std::vector<std::array<float, 3>> SortedVertices (const Mesh &m)
{
	std::vector<std::array<float, 3>> v;
	for (unsigned i = 0; i < m.GetNVertices (); ++i)
	{
		float q[3];
		m.GetVertex (i, q);
		v.push_back ({q[0], q[1], q[2]});
	}
	std::sort (v.begin (), v.end ());
	return v;
}

std::string RefusalOf (const std::function<void ()> &call)
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

// Axes d'un cote, a croissant (X sur Front et Back, Y sur Right et Left),
// angles compris : les colonnes de la rangee extreme du cote (Y minimal en
// facade, X maximal a droite, etc.), lues sur LayoutPeristyle.
std::vector<double> AxesAlong (const PeristyleLayout &p, PeristyleSide side)
{
	const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
	double extreme = 0.0;
	for (const PeristyleColumn &c : p.columns)
	{
		const double across = alongX ? c.y : c.x;
		const bool low = side == PeristyleSide::Front || side == PeristyleSide::Left;
		extreme = low ? std::min (extreme, across) : std::max (extreme, across);
	}
	std::set<double> axes;
	for (const PeristyleColumn &c : p.columns)
		if (SameBits (alongX ? c.y : c.x, extreme))
			axes.insert (alongX ? c.x : c.y);
	return {axes.begin (), axes.end ()};
}

// Pieces d'un cote, triees par a0.
std::vector<FriezePiece> SidePieces (const FriezeLayout &f, PeristyleSide side)
{
	std::vector<FriezePiece> out;
	for (const auto *list : {&f.triglyphs, &f.metopes})
		for (const FriezePiece &q : *list)
			if (q.side == side)
				out.push_back (q);
	std::sort (out.begin (), out.end (), [] (const FriezePiece &a, const FriezePiece &b) { return a.a0 < b.a0; });
	return out;
}

const PeristyleSide kSides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};

}  // namespace

// N1
TEST (TEST_cgarchi_classical_greek_entablature, the_ring_is_a_closed_shell_of_the_section_volume)
{
	const std::vector<RingSectionPoint> square = {{-0.3, 0.0}, {0.4, 0.0}, {0.4, 0.5}, {-0.3, 0.5}};
	struct Case
	{
		std::string name;
		std::vector<RingSectionPoint> section;
		double hx, hy, z0;
	};
	const std::vector<Case> cases = {
		{"synthetique, halfX != halfY", SyntheticSection (), 2.68, 3.31, 6.27},
		{"carre", square, 2.0, 2.0, 0.0},
		{"synthetique, halfX = halfY", SyntheticSection (), 3.0, 3.0, 1.0 / 3.0},
		{"carre, halfX != halfY", square, 1.3, 4.1, -0.7},
	};
	// La section construite est celle ecrite ici (le cas synthetique teste donc
	// aussi GreekEntablatureSection).
	const std::vector<RingSectionPoint> built = GreekEntablatureSection (SyntheticEntablatureSettings ());
	ASSERT_EQ (built.size (), SyntheticSection ().size ());

	for (const Case &c : cases)
	{
		SCOPED_TRACE (c.name);
		const std::vector<RingSectionPoint> section = c.name.rfind ("synthetique", 0) == 0 ? built : c.section;
		const std::unique_ptr<Mesh> m = SweepRectangularRing (section, c.hx, c.hy, c.z0);
		ASSERT_EQ (m->GetNVertices (), 4 * section.size ());
		ASSERT_EQ (m->GetNFaces (), 8 * section.size ());
		const MeshValidationReport r = ValidateMesh (*m);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.components, 1u);
		const long long euler = static_cast<long long> (m->GetNVertices ()) - static_cast<long long> (UniqueEdges (*m)) +
		                        static_cast<long long> (m->GetNFaces ());
		EXPECT_EQ (euler, 0) << "anneau de genre 1";
		const double expected = FanVolume (c.section, c.hx, c.hy);
		EXPECT_NEAR (RectangularRingVolume (c.section, c.hx, c.hy), expected, 1e-12 * expected);
		EXPECT_NEAR (r.signedVolume, expected, 1e-6 * expected);
		RecordProperty ("volume_" + std::to_string (&c - cases.data ()), std::to_string (r.signedVolume));
	}

	// Refus de l'anneau, prefixe "Anneau : ".
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	auto expectRingRefused = [] (const std::vector<RingSectionPoint> &s, double hx, double hy, const std::string &what) {
		const std::string refusal = RefusalOf ([&] { SweepRectangularRing (s, hx, hy, 0.0); });
		EXPECT_EQ (refusal.rfind ("Anneau : ", 0), 0u) << what << " : '" << refusal << "'";
	};
	expectRingRefused ({{0.1, 0.0}, {0.2, 0.0}}, 2.0, 2.0, "deux sommets");
	expectRingRefused ({{0.1, 0.0}, {0.2, nan}, {0.2, 1.0}}, 2.0, 2.0, "non fini");
	expectRingRefused (square, nan, 2.0, "halfX non fini");
	expectRingRefused (square, 0.3, 2.0, "halfX + u <= 0");
	expectRingRefused (square, 2.0, 0.3, "halfY + u <= 0");
	expectRingRefused ({{-0.3, 0.0}, {0.4, 0.0}, {0.4, 0.0}, {0.4, 0.5}}, 2.0, 2.0, "sommets voisins confondus");
	expectRingRefused ({{-0.3, 0.0}, {0.4, 0.5}, {0.4, 0.0}, {-0.3, 0.5}}, 2.0, 2.0, "noeud papillon");
	expectRingRefused ({{-0.3, 0.0}, {0.4, 0.0}, {0.4, 0.5}, {0.4, 0.2}, {-0.3, 0.5}}, 2.0, 2.0, "repli");
	expectRingRefused ({{-0.3, 0.0}, {-0.3, 0.5}, {0.4, 0.5}, {0.4, 0.0}}, 2.0, 2.0, "sens indirect");
}

// N2
TEST (TEST_cgarchi_classical_greek_entablature, each_section_vertex_becomes_a_mitred_rectangle)
{
	const double hx = 2.68, hy = 3.31, z0 = 6.27;
	const std::unique_ptr<Mesh> m =
		SweepRectangularRing (GreekEntablatureSection (SyntheticEntablatureSettings ()), hx, hy, z0);
	std::vector<std::array<float, 3>> expected;
	for (const RingSectionPoint &q : SyntheticSection ())
	{
		const float x = static_cast<float> (hx + q.u), y = static_cast<float> (hy + q.u), z = static_cast<float> (z0 + q.z);
		const float mx = static_cast<float> (-(hx + q.u)), my = static_cast<float> (-(hy + q.u));
		expected.push_back ({mx, my, z});
		expected.push_back ({x, my, z});
		expected.push_back ({x, y, z});
		expected.push_back ({mx, y, z});
	}
	std::sort (expected.begin (), expected.end ());
	EXPECT_EQ (SortedVertices (*m), expected);

	// Les quatre coins de chaque rectangle sur les diagonales des angles :
	// |x| - hx = |y| - hy (onglets a 45 degres).
	for (unsigned i = 0; i < m->GetNVertices (); ++i)
	{
		float q[3];
		m->GetVertex (i, q);
		EXPECT_NEAR (std::fabs (q[0]) - hx, std::fabs (q[1]) - hy, 1e-5) << i;
	}
}

// N3
TEST (TEST_cgarchi_classical_greek_entablature, the_band_section_merges_equal_projections)
{
	// Jeu synthetique : b = 9, j = 1 (face inclinee puis face verticale de meme
	// saillie), v = 0 : 2b + 2 - j - v = 19 sommets.
	const GreekEntablatureSettings s = SyntheticEntablatureSettings ();
	const std::vector<RingSectionPoint> section = GreekEntablatureSection (s);
	const std::vector<RingSectionPoint> expected = SyntheticSection ();
	ASSERT_EQ (section.size (), 19u);
	for (std::size_t i = 0; i < section.size (); ++i)
	{
		EXPECT_TRUE (SameBits (section[i].u, expected[i].u)) << i;
		EXPECT_TRUE (SameBits (section[i].z, expected[i].z)) << i;
	}
	// Cotes au bit pres de CumulativeBounds (0, hauteurs).
	std::vector<double> heights;
	for (const EntablatureBand &b : s.bands)
		heights.push_back (b.height);
	const std::vector<double> bounds = CumulativeBounds (0.0, heights, "bands");
	EXPECT_EQ (GreekEntablatureBandBounds (s), bounds);
	for (const RingSectionPoint &q : section)
		EXPECT_TRUE (std::any_of (bounds.begin (), bounds.end (), [&] (double z) { return SameBits (z, q.z); })) << q.z;

	// Deux faces verticales de meme saillie : sommet de jonction retire
	// (j = 1, v = 1) ; deux faces verticales de saillies differentes : ressaut.
	GreekEntablatureSettings v = s;
	v.bands = {{0.2, 0.4, 0.4}, {0.3, 0.4, 0.4}, {0.25, 0.3, 0.3}, {0.1, 0.6, 0.6}};
	v.friezeBand = 2;
	v.triglyph.face = 0.36;
	v.triglyph.channelDepth = 0.05;
	v.triglyph.halfChannelDepth = 0.055;
	const std::vector<RingSectionPoint> merged = GreekEntablatureSection (v);
	ASSERT_EQ (merged.size (), 2u * 4u + 2u - 1u - 1u);
	const double z1 = 0.2, z2 = z1 + 0.3, z3 = z2 + 0.25, z4 = z3 + 0.1;
	const std::vector<RingSectionPoint> mergedExpected = {{0.4, 0.0}, {0.4, z2}, {0.3, z2}, {0.3, z3}, {0.6, z3},
	                                                      {0.6, z4},  {-0.44, z4}, {-0.44, 0.0}};
	for (std::size_t i = 0; i < merged.size (); ++i)
	{
		EXPECT_TRUE (SameBits (merged[i].u, mergedExpected[i].u)) << i;
		EXPECT_TRUE (SameBits (merged[i].z, mergedExpected[i].z)) << i;
	}

	// Aucun sommet repete, dans les deux sections.
	for (const auto *sec : {&section, &merged})
		for (std::size_t i = 0; i < sec->size (); ++i)
			for (std::size_t k = i + 1; k < sec->size (); ++k)
				EXPECT_FALSE ((*sec)[i].u == (*sec)[k].u && (*sec)[i].z == (*sec)[k].z) << i << " " << k;
}

// N4
TEST (TEST_cgarchi_classical_greek_entablature, invalid_entablature_settings_are_refused_naming_the_field)
{
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	const PeristyleLayout p = SyntheticPeristyle ();
	using Edit = std::function<void (GreekEntablatureSettings &)>;
	const std::vector<std::pair<std::string, Edit>> cases = {
		{"bands", [] (GreekEntablatureSettings &e) { e.bands.resize (1); }},
		{"bands[2].height", [] (GreekEntablatureSettings &e) { e.bands[2].height = 0.0; }},
		{"bands[5].height", [&] (GreekEntablatureSettings &e) { e.bands[5].height = nan; }},
		{"bands[0].projection", [] (GreekEntablatureSettings &e) { e.bands[0].projection = -1.0; }},
		{"bands[8].projection", [&] (GreekEntablatureSettings &e) { e.bands[8].projection = nan; }},
		{"bands[0].topProjection", [] (GreekEntablatureSettings &e) { e.bands[0].topProjection = 0.0; }},
		{"bands[1].topProjection", [] (GreekEntablatureSettings &e) { e.bands[1].topProjection = std::numeric_limits<double>::infinity (); }},
		{"bands[3].topProjection", [] (GreekEntablatureSettings &e) { e.bands[3].topProjection = 0.44; }},
		{"innerFace", [] (GreekEntablatureSettings &e) { e.innerFace = 0.0; }},
		{"innerFace", [&] (GreekEntablatureSettings &e) { e.innerFace = nan; }},
		{"friezeBand", [] (GreekEntablatureSettings &e) { e.friezeBand = 0; }},
		{"friezeBand", [] (GreekEntablatureSettings &e) { e.friezeBand = 8; }},
		{"friezeBand", [] (GreekEntablatureSettings &e) { e.friezeBand = -1; }},
		{"triglyph.width", [] (GreekEntablatureSettings &e) { e.triglyph.width = 0.0; }},
		{"triglyph.divisionWidths", [] (GreekEntablatureSettings &e) {
			 e.triglyph.divisionWidths = {0.37};
			 e.triglyph.divisionIsChannel = {true};
		 }},
		{"triglyph.divisionIsChannel", [] (GreekEntablatureSettings &e) { e.triglyph.divisionIsChannel.pop_back (); }},
		{"triglyph.divisionWidths[2]", [&] (GreekEntablatureSettings &e) { e.triglyph.divisionWidths[2] = nan; }},
		{"triglyph.divisionWidths[4]", [] (GreekEntablatureSettings &e) { e.triglyph.divisionWidths[4] = -0.075; }},
		{"triglyph.divisionWidths", [] (GreekEntablatureSettings &e) { e.triglyph.width = 0.371; }},
		{"triglyph.divisionIsChannel", [] (GreekEntablatureSettings &e) { e.triglyph.divisionIsChannel.front () = false; }},
		{"triglyph.divisionIsChannel", [] (GreekEntablatureSettings &e) { e.triglyph.divisionIsChannel.back () = false; }},
		{"triglyph.face", [] (GreekEntablatureSettings &e) { e.triglyph.face = 0.43; }},
		{"triglyph.face", [&] (GreekEntablatureSettings &e) { e.triglyph.face = nan; }},
		{"triglyph.channelDepth", [] (GreekEntablatureSettings &e) { e.triglyph.channelDepth = 0.0; }},
		{"triglyph.channelDepth", [&] (GreekEntablatureSettings &e) { e.triglyph.channelDepth = nan; }},
		{"triglyph.channelDepth", [] (GreekEntablatureSettings &e) { e.triglyph.channelDepth = e.triglyph.face - 0.43; }},
		{"triglyph.halfChannelDepth", [] (GreekEntablatureSettings &e) { e.triglyph.halfChannelDepth = -0.01; }},
		{"triglyph.halfChannelDepth", [] (GreekEntablatureSettings &e) { e.triglyph.halfChannelDepth = e.triglyph.face - 0.43; }},
	};
	for (const auto &[field, edit] : cases)
	{
		SCOPED_TRACE (field);
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		edit (e);
		const std::string expected = "Entablement : '" + field + "'";
		for (const std::string &refusal :
		     {RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), RefusalOf ([&] { GreekEntablatureSection (e); }),
		      RefusalOf ([&] { LayoutGreekFrieze (e, p); })})
			EXPECT_EQ (refusal.rfind (expected, 0), 0u) << "obtenu : '" << refusal << "'";
	}
	EXPECT_EQ (RefusalOf ([] { ValidateGreekEntablatureSettings (SyntheticEntablatureSettings ()); }), "");

	// Bornes des profondeurs : face - nu refusee (cas ci-dessus), la valeur
	// double immediatement inferieure acceptee.
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		e.triglyph.channelDepth = std::nextafter (e.triglyph.face - 0.43, 0.0);
		e.triglyph.halfChannelDepth = std::nextafter (e.triglyph.face - 0.43, 0.0);
		EXPECT_EQ (RefusalOf ([&] { LayoutGreekFrieze (e, p); }), "");
	}
	// Un demi-canal plus profond que le canal est accepte (aucune relation
	// d'ordre entre les deux profondeurs).
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		ASSERT_GT (e.triglyph.halfChannelDepth, e.triglyph.channelDepth);
		EXPECT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
		e.triglyph.halfChannelDepth = 0.059;
		e.triglyph.channelDepth = 0.001;
		EXPECT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
	}

	// LayoutGreekFrieze : largeur / 2 = nu des metopes refuse, pour cette raison
	// (sur un peristyle large, ou aucune metope courante n'est vide).
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		e.triglyph.width = 0.86;
		e.triglyph.divisionWidths = {0.43, 0.43};
		e.triglyph.divisionIsChannel = {true, true};
		ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
		const PeristyleLayout wide = LayoutPeristyle (4, 5, 3.0, 3.0);
		const std::string refusal = RefusalOf ([&] { LayoutGreekFrieze (e, wide); });
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("triglyphes d'angle"), std::string::npos) << refusal;
	}
	// Une metope courante vide : refus nomme, raison distincte.
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const PeristyleLayout tight = LayoutPeristyle (4, 5, 0.7, 0.74);
		const std::string refusal = RefusalOf ([&] { LayoutGreekFrieze (e, tight); });
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("metope vide"), std::string::npos) << refusal;
	}
	// Face interieure au-dela de la ligne des axes opposee.
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		e.innerFace = p.halfWidth;
		EXPECT_EQ (RefusalOf ([&] { LayoutGreekFrieze (e, p); }).rfind ("Entablement : 'innerFace'", 0), 0u);
		e.innerFace = std::nextafter (p.halfWidth, 0.0);
		EXPECT_EQ (RefusalOf ([&] { LayoutGreekFrieze (e, p); }), "");
	}
	// BuildTriglyphMesh refuse une metope.
	{
		const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		EXPECT_EQ (RefusalOf ([&] { BuildTriglyphMesh (e, p, f.metopes.front (), 0.0); }).rfind ("Frise : ", 0), 0u);
	}
	// Face interieure entre halfDepth et halfWidth, sur un peristyle plus large
	// que profond : refusee (le minimum des deux demi-distances).
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const PeristyleLayout shallow = LayoutPeristyle (6, 3, 1.9, 1.7);
		ASSERT_LT (shallow.halfDepth, shallow.halfWidth);
		e.innerFace = 0.5 * (shallow.halfDepth + shallow.halfWidth);
		EXPECT_EQ (RefusalOf ([&] { LayoutGreekFrieze (e, shallow); }).rfind ("Entablement : 'innerFace'", 0), 0u);
	}
	// Metope d'angle vide en double : width / 2 = nextafter (b, 0), coin a 4,5 + b
	// confondu avec l'axe d'angle 4,5 plus width / 2.
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const double half = std::nextafter (0.43, 0.0);
		e.triglyph.width = 2.0 * half;
		e.triglyph.divisionWidths = {half, half};
		e.triglyph.divisionIsChannel = {true, true};
		const PeristyleLayout wide = LayoutPeristyle (4, 5, 3.0, 3.0);
		ASSERT_EQ (4.5 + 0.43, 4.5 + half);
		const std::string refusal = RefusalOf ([&] { LayoutGreekFrieze (e, wide); });
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("metope d'angle vide"), std::string::npos) << refusal;
	}
	// BuildTriglyphMesh : placement vide ou non fini refuse.
	{
		const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		FriezePiece empty = f.triglyphs[2];
		empty.a1 = empty.a0;
		EXPECT_EQ (RefusalOf ([&] { BuildTriglyphMesh (e, p, empty, 6.27); }).rfind ("Frise : ", 0), 0u);
		EXPECT_EQ (RefusalOf ([&] { BuildTriglyphMesh (e, p, f.triglyphs[2], nan); }).rfind ("Frise : ", 0), 0u);
		FriezePiece flat = f.triglyphs[2];
		flat.z1 = flat.z0;
		EXPECT_EQ (RefusalOf ([&] { BuildTriglyphMesh (e, p, flat, 6.27); }).rfind ("Frise : ", 0), 0u);
	}

	// Bornes vues sur le maillage : une profondeur acceptee en double dont le
	// fond se confond avec le dos une fois arrondi en float, et deux triglyphes
	// d'angle voisins separes en double et au contact en float, sont refuses
	// par BuildTriglyphMesh ; juste en deca, le prisme est valide.
	auto meshVerdict = [] (const std::function<std::unique_ptr<Mesh> ()> &build) {
		try
		{
			const std::unique_ptr<Mesh> m = build ();
			const MeshValidationReport r = ValidateMesh (*m);
			std::string out = "accepte : valide " + std::to_string (r.IsValid ()) + ", composantes " +
			                  std::to_string (r.components);
			for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
				if (r.counts[kind] > 0)
					out += ", " + std::string (MeshDefectName (static_cast<MeshDefect> (kind))) + " " +
					       std::to_string (r.counts[kind]);
			return out;
		}
		catch (const std::invalid_argument &e)
		{
			return std::string (e.what ());
		}
	};
	for (const bool half : {false, true})
	{
		SCOPED_TRACE (half ? "demi-canal" : "canal");
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		double &depth = half ? e.triglyph.halfChannelDepth : e.triglyph.channelDepth;
		depth = std::nextafter (e.triglyph.face - 0.43, 0.0);
		const FriezeLayout f = LayoutGreekFrieze (e, p);   // accepte en double
		for (const FriezePiece &q : f.triglyphs)
		{
			const std::string verdict = meshVerdict ([&] { return BuildTriglyphMesh (e, p, q, 6.27); });
			EXPECT_EQ (verdict.rfind ("Frise : ", 0), 0u) << static_cast<int> (q.side) << " " << q.rank << " : " << verdict;
			RecordProperty (std::string (half ? "half" : "full") + "_depth_degenerate_verdict", verdict);
		}
		depth = e.triglyph.face - 0.43 - 1e-5;
		const FriezeLayout g = LayoutGreekFrieze (e, p);
		for (const FriezePiece &q : g.triglyphs)
			EXPECT_EQ (meshVerdict ([&] { return BuildTriglyphMesh (e, p, q, 6.27); }).rfind ("accepte : valide 1, composantes 1", 0), 0u);
	}
	{
		// width / 2 = b - 1e-9 : separes en double, confondus en float a 4,93.
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		const double half = 0.43 - 1e-9;
		e.triglyph.width = 2.0 * half;
		e.triglyph.divisionWidths = {half, half};
		e.triglyph.divisionIsChannel = {true, true};
		const PeristyleLayout wide = LayoutPeristyle (4, 5, 3.0, 3.0);
		ASSERT_EQ (static_cast<float> (4.5 + 0.43), static_cast<float> (4.5 + half));
		const FriezeLayout f = LayoutGreekFrieze (e, wide);   // accepte en double
		int corners = 0, refused = 0;
		for (const FriezePiece &q : f.triglyphs)
		{
			const int last = q.side == PeristyleSide::Front || q.side == PeristyleSide::Back ? 6 : 8;
			const bool corner = q.rank == 0 || q.rank == last;
			const std::string verdict = meshVerdict ([&] { return BuildTriglyphMesh (e, wide, q, 6.27); });
			if (corner)
			{
				++corners;
				refused += verdict.rfind ("Frise : ", 0) == 0 ? 1 : 0;
				RecordProperty ("corner_verdict", verdict);
			}
			else
				EXPECT_EQ (verdict.rfind ("accepte : valide 1", 0), 0u) << verdict;
		}
		EXPECT_EQ (corners, 8);
		EXPECT_EQ (refused, 8);

		// Les deux prismes d'angle voisins (facade, rang 6 ; flanc droit, rang 0)
		// assembles en un maillage : deux composantes disjointes attendues.
		auto pairVerdict = [&] (const GreekEntablatureSettings &s, const FriezeLayout &layout) {
			const FriezePiece *a = nullptr, *b = nullptr;
			for (const FriezePiece &q : layout.triglyphs)
			{
				if (q.side == PeristyleSide::Front && q.rank == 6)
					a = &q;
				if (q.side == PeristyleSide::Right && q.rank == 0)
					b = &q;
			}
			return meshVerdict ([&] {
				const std::unique_ptr<Mesh> ma = BuildTriglyphMesh (s, wide, *a, 6.27), mb = BuildTriglyphMesh (s, wide, *b, 6.27);
				std::vector<float> v = ma->GetVertices ();
				v.insert (v.end (), mb->GetVertices ().begin (), mb->GetVertices ().end ());
				std::vector<unsigned> faces;
				for (const Mesh *m : {ma.get (), mb.get ()})
				{
					const unsigned offset = m == ma.get () ? 0u : ma->GetNVertices ();
					for (unsigned face = 0; face < m->GetNFaces (); ++face)
						for (unsigned j = 0; j < 3; ++j)
							faces.push_back (offset + static_cast<unsigned> (m->GetFaceVertex (face, j)));
				}
				auto pair = std::make_unique<Mesh> ();
				pair->SetVertices (static_cast<unsigned> (v.size () / 3), v.data ());
				pair->SetFaces (static_cast<unsigned> (faces.size () / 3), 3, faces.data ());
				return pair;
			});
		};
		const std::string contact = pairVerdict (e, f);
		EXPECT_EQ (contact.rfind ("Frise : ", 0), 0u) << contact;
		RecordProperty ("corner_pair_verdict", contact);
		// Juste en deca : les deux prismes sont separes, deux composantes.
		GreekEntablatureSettings apart = e;
		apart.triglyph.width = 2.0 * (0.43 - 1e-3);
		apart.triglyph.divisionWidths = {0.43 - 1e-3, 0.43 - 1e-3};
		const std::string separate = pairVerdict (apart, LayoutGreekFrieze (apart, wide));
		EXPECT_EQ (separate, "accepte : valide 1, composantes 2") << separate;
	}

	// Strategies d'angle (N1a). Chaque refus est pris loin de son seuil et nomme
	// son motif ; les memes reglages sont acceptes sous AxisTriglyph.
	auto refusals = [&] (const GreekEntablatureSettings &e) {
		return std::vector<std::string> {RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }),
		                                 RefusalOf ([&] { GreekEntablatureSection (e); }),
		                                 RefusalOf ([&] { LayoutGreekFrieze (e, p); })};
	};
	{
		GreekEntablatureSettings e = SyntheticEntablatureSettings ();
		e.corner = static_cast<DoricCornerStrategy> (3);
		for (const std::string &refusal : refusals (e))
			EXPECT_EQ (refusal.rfind ("Entablement : 'corner'", 0), 0u) << "obtenu : '" << refusal << "'";
	}
	// Largeur 0,05 < face - nu = 0,06 : refusee sur la largeur, avant la
	// simplicite (la section d'angle n'y est pas simple non plus : les bouts
	// interieurs des deux bras se croisent).
	auto narrow = [] (GreekEntablatureSettings &e) {
		e.triglyph.width = 0.05;
		e.triglyph.divisionWidths = {0.01, 0.01, 0.01, 0.01, 0.01};
		e.triglyph.divisionIsChannel = {true, false, true, false, true};
	};
	// Division de bord (premiere division) de largeur `edge` : au-dela de
	// 2 (face - nu) = 0,12, le chanfrein d'arete passe derriere les deux nus.
	auto edgeDivision = [] (GreekEntablatureSettings &e, double edge) {
		if (edge > 0.12)
			e.triglyph.divisionWidths = {edge, 0.03, 0.05, 0.02, 0.05, 0.03, 0.04};
		else
			e.triglyph.divisionWidths = {edge, 0.03, 0.06, 0.03, 0.06, 0.03, 0.06};
	};
	for (const DoricCornerStrategy strategy :
	     {DoricCornerStrategy::CornerTriglyphEqualMetopes, DoricCornerStrategy::CornerTriglyphMidBay})
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		GreekEntablatureSettings thin = SyntheticEntablatureSettings ();
		narrow (thin);
		thin.corner = strategy;
		for (const std::string &refusal : refusals (thin))
		{
			EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << "obtenu : '" << refusal << "'";
			EXPECT_NE (refusal.find ("face de frise"), std::string::npos) << refusal;
		}
		GreekEntablatureSettings folded = SyntheticEntablatureSettings ();
		edgeDivision (folded, 0.15);
		folded.corner = strategy;
		for (const std::string &refusal : refusals (folded))
		{
			EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.divisionWidths'", 0), 0u) << "obtenu : '" << refusal << "'";
			EXPECT_NE (refusal.find ("non simple"), std::string::npos) << refusal;
		}
		GreekEntablatureSettings broad = SyntheticEntablatureSettings ();
		edgeDivision (broad, 0.10);
		broad.corner = strategy;
		for (const std::string &refusal : refusals (broad))
			EXPECT_EQ (refusal, "");
	}
	{
		GreekEntablatureSettings thin = SyntheticEntablatureSettings (), folded = SyntheticEntablatureSettings ();
		narrow (thin);
		edgeDivision (folded, 0.15);
		ASSERT_EQ (thin.corner, DoricCornerStrategy::AxisTriglyph);
		for (const GreekEntablatureSettings &e : {thin, folded})
			for (const std::string &refusal : refusals (e))
				EXPECT_EQ (refusal, "");
	}
	// Simplicite aux quatre angles : la division de bord de 0,15 en derniere
	// position n'est a l'arete que sur l'angle Back (les deux bras portent la
	// derniere division), pas sur l'angle Front.
	{
		GreekEntablatureSettings mirrored = SyntheticEntablatureSettings ();
		mirrored.triglyph.divisionWidths = {0.04, 0.03, 0.05, 0.02, 0.05, 0.03, 0.15};
		ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (mirrored); }), "");
		mirrored.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;
		for (const std::string &refusal : refusals (mirrored))
		{
			EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.divisionWidths'", 0), 0u) << "obtenu : '" << refusal << "'";
			EXPECT_NE (refusal.find ("non simple"), std::string::npos) << refusal;
		}
	}
	// BuildTriglyphMesh, piece d'angle : sous AxisTriglyph ; bras de retour non
	// fini ; deux sommets du chanfrein confondus en float (division de bord de
	// 1e-8 a l'arete des deux bras de l'angle Front, sous l'ulp float de 3,66).
	{
		GreekEntablatureSettings equal = SyntheticEntablatureSettings ();
		equal.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;
		const FriezeLayout f = LayoutGreekFrieze (equal, p);
		const FriezePiece *corner = nullptr;
		for (const FriezePiece &q : f.triglyphs)
			if (q.kind == FriezeElement::CornerTriglyph && q.side == PeristyleSide::Front)
				corner = &q;
		ASSERT_NE (corner, nullptr);
		const std::string axisRefusal = RefusalOf ([&] { BuildTriglyphMesh (SyntheticEntablatureSettings (), p, *corner, 6.27); });
		EXPECT_EQ (axisRefusal.rfind ("Frise : piece d'angle sous la strategie AxisTriglyph", 0), 0u) << axisRefusal;
		FriezePiece open = *corner;
		open.ra0 = nan;
		const std::string returnRefusal = RefusalOf ([&] { BuildTriglyphMesh (equal, p, open, 6.27); });
		EXPECT_EQ (returnRefusal.rfind ("Frise : bras de retour", 0), 0u) << returnRefusal;

		GreekEntablatureSettings fine = equal;
		fine.triglyph.divisionWidths = {1e-8, 0.03 + (0.05 - 1e-8), 0.075, 0.06, 0.075, 0.05, 0.03};
		fine.triglyph.divisionIsChannel = {true, false, true, false, true, false, true};
		ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (fine); }), "");
		const FriezeLayout g = LayoutGreekFrieze (fine, p);
		for (const FriezePiece &q : g.triglyphs)
		{
			if (q.kind != FriezeElement::CornerTriglyph)
				continue;
			const std::string verdict = meshVerdict ([&] { return BuildTriglyphMesh (fine, p, q, 6.27); });
			RecordProperty ("fine_edge_verdict_" + std::to_string (static_cast<int> (q.side)), verdict);
			if (q.side == PeristyleSide::Front)
				EXPECT_EQ (verdict.rfind ("Frise : 'triglyph.divisionWidths' confond deux sommets", 0), 0u) << verdict;
		}
	}
	// Ornements (lot N2a), sur SyntheticOrnamentSettings : un refus par champ.
	{
		const std::vector<std::pair<std::string, Edit>> ornamentCases = {
			{"regulae", [] (GreekEntablatureSettings &e) {
				 e.friezeBand = 1;
				 e.triglyph.face = 0.55;
			 }},
			{"bands[1].topProjection", [] (GreekEntablatureSettings &e) { e.bands[1].topProjection = 0.46; }},
			{"regulae.height", [] (GreekEntablatureSettings &e) { e.regulae->height = 0.0; }},
			{"regulae.height", [] (GreekEntablatureSettings &e) { e.regulae->height = 0.07; }},
			{"regulae.height", [&] (GreekEntablatureSettings &e) { e.regulae->height = nan; }},
			{"regulae.face", [] (GreekEntablatureSettings &e) { e.regulae->face = 0.47; }},
			{"regulae.face", [] (GreekEntablatureSettings &e) { e.regulae->face = 0.5001; }},
			{"regulae.guttaCount", [] (GreekEntablatureSettings &e) { e.regulae->guttaCount = 0; }},
			{"regulae.guttaCount", [] (GreekEntablatureSettings &e) { e.regulae->guttaCount = 11; }},
			{"regulae.guttaGap", [] (GreekEntablatureSettings &e) { e.regulae->guttaGap = 0.0; }},
			{"regulae.guttaGap", [&] (GreekEntablatureSettings &e) { e.regulae->guttaGap = nan; }},
			{"regulae.gutta.footDiameter", [] (GreekEntablatureSettings &e) { e.regulae->gutta.footDiameter = 0.0; }},
			{"regulae.gutta.footDiameter", [] (GreekEntablatureSettings &e) { e.regulae->gutta.footDiameter = 0.03; }},
			{"regulae.gutta.footProjection", [] (GreekEntablatureSettings &e) { e.regulae->gutta.footProjection = 0.501; }},
			{"regulae.gutta.footProjection", [&] (GreekEntablatureSettings &e) { e.regulae->gutta.footProjection = nan; }},
			{"regulae.gutta.topProjection", [] (GreekEntablatureSettings &e) { e.regulae->gutta.topProjection = 0.5001; }},
			{"regulae.gutta.topProjection", [] (GreekEntablatureSettings &e) { e.regulae->gutta.topProjection = 0.4875; }},
			{"regulae.gutta.topProjection", [&] (GreekEntablatureSettings &e) { e.regulae->gutta.topProjection = nan; }},
			{"regulae.gutta.sides", [] (GreekEntablatureSettings &e) { e.regulae->gutta.sides = 6; }},
			{"regulae.gutta.sides", [] (GreekEntablatureSettings &e) { e.regulae->gutta.sides = 0; }},
			{"mutules.band", [] (GreekEntablatureSettings &e) { e.mutules->band = 3; }},
			{"mutules.band", [] (GreekEntablatureSettings &e) { e.mutules->band = 8; }},
			{"bands[6].topProjection", [] (GreekEntablatureSettings &e) { e.bands[6].topProjection = 0.49; }},
			{"mutules.width", [] (GreekEntablatureSettings &e) { e.mutules->width = 0.0; }},
			{"mutules.width", [&] (GreekEntablatureSettings &e) { e.mutules->width = nan; }},
			{"mutules.face", [] (GreekEntablatureSettings &e) { e.mutules->face = 0.48; }},
			{"mutules.face", [] (GreekEntablatureSettings &e) { e.mutules->face = 0.84; }},
		};
		for (const auto &c : ornamentCases)
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticOrnamentSettings ();
			c.second (e);
			const std::string refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			EXPECT_EQ (refusal.rfind ("Entablement : '" + c.first + "'", 0), 0u) << c.first << " ; obtenu : '" << refusal << "'";
		}
		// Selon la strategie : sous AxisTriglyph, width / 2 >= dos des regulae ;
		// sous les strategies d'angle, face - width >= dos, et gouttes d'angle
		// jointives (vide 0,04 : marge 0,01, face - marge = 0,48 >= 0,475),
		// acceptees sous AxisTriglyph.
		GreekEntablatureSettings narrowBack = cgarchi_test::SyntheticOrnamentSettings ();
		narrowBack.bands[1] = {0.07, 0.18, 0.18};
		std::string refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (narrowBack); });
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("regulae d'angle"), std::string::npos) << refusal;
		GreekEntablatureSettings deepArm = cgarchi_test::SyntheticOrnamentSettings ();
		deepArm.bands[1] = {0.07, 0.12, 0.12};
		deepArm.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;
		refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (deepArm); });
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("regula d'angle degeneree"), std::string::npos) << refusal;
		GreekEntablatureSettings joined = cgarchi_test::SyntheticOrnamentSettings ();
		joined.regulae->guttaGap = 0.04;
		EXPECT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (joined); }), "");
		for (const DoricCornerStrategy strategy : {DoricCornerStrategy::CornerTriglyphEqualMetopes, DoricCornerStrategy::CornerTriglyphMidBay})
		{
			joined.corner = strategy;
			refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (joined); });
			EXPECT_EQ (refusal.rfind ("Entablement : 'regulae.guttaGap'", 0), 0u) << refusal;
			EXPECT_NE (refusal.find ("gouttes d'angle"), std::string::npos) << refusal;
		}
		// Dos des mutules (0,48) en retrait du listel de la bande du dessous
		// (0,505) : accepte sous les trois strategies, placements compris.
		for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes,
		                                           DoricCornerStrategy::CornerTriglyphMidBay})
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticOrnamentSettings ();
			e.corner = strategy;
			ASSERT_LT (e.bands[6].projection, e.bands[5].projection);
			EXPECT_EQ (RefusalOf ([&] { LayoutGreekOrnaments (e, p); }), "");
		}
		// BuildOrnamentMesh : regula d'angle sous AxisTriglyph ; plage de gouttes
		// hors des placements ; piece vide.
		GreekEntablatureSettings equal = cgarchi_test::SyntheticOrnamentSettings ();
		equal.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;
		const OrnamentLayout o = LayoutGreekOrnaments (equal, p);
		ASSERT_FALSE (o.regulae.empty ());
		const OrnamentPiece &first = o.regulae.front ();
		ASSERT_EQ (first.kind, OrnamentElement::CornerRegula);
		refusal = RefusalOf ([&] { BuildOrnamentMesh (cgarchi_test::SyntheticOrnamentSettings (), p, o, first, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : regula d'angle sous la strategie AxisTriglyph", 0), 0u) << refusal;
		OrnamentPiece beyond = first;
		beyond.firstGutta = static_cast<int> (o.guttae.size ()) - 1;
		refusal = RefusalOf ([&] { BuildOrnamentMesh (equal, p, o, beyond, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : plage de gouttes", 0), 0u) << refusal;
		OrnamentPiece empty = o.mutules.front ();
		empty.a1 = empty.a0;
		refusal = RefusalOf ([&] { BuildOrnamentMesh (equal, p, o, empty, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : piece vide", 0), 0u) << refusal;

		// Pied des gouttes au-dessus de la bande f - 3 : refus loin du seuil
		// (0,49 contre l'arriere du pied, 0,475) et au seuil (0,475 : pied pose sur
		// le ressaut) ; f = 2 refuse.
		auto validation = [] (const Edit &edit) {
			GreekEntablatureSettings e = cgarchi_test::SyntheticOrnamentSettings ();
			edit (e);
			return RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
		};
		for (const double top : {0.49, 0.475})
		{
			refusal = validation ([&] (GreekEntablatureSettings &e) { e.bands[0].topProjection = top; });
			EXPECT_EQ (refusal.rfind ("Entablement : 'bands[0].topProjection'", 0), 0u) << top << " ; " << refusal;
			EXPECT_NE (refusal.find ("pied des gouttes"), std::string::npos) << refusal;
		}
		refusal = validation ([] (GreekEntablatureSettings &e) {
			e.friezeBand = 2;
			e.triglyph.face = 0.56;
		});
		EXPECT_EQ (refusal.rfind ("Entablement : 'regulae'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("friezeBand >= 3"), std::string::npos) << refusal;

		// Bornes d'interface du nombre de gouttes et de leurs cotes, motif nomme.
		for (const int count : {kGreekMaxGuttae + 1, 1 << 30})
		{
			refusal = validation ([&] (GreekEntablatureSettings &e) { e.regulae->guttaCount = count; });
			EXPECT_EQ (refusal.rfind ("Entablement : 'regulae.guttaCount'", 0), 0u) << refusal;
			EXPECT_NE (refusal.find ("hors de"), std::string::npos) << refusal;
		}
		refusal = validation ([] (GreekEntablatureSettings &e) { e.regulae->gutta.sides = kGreekMaxGuttaSides + 4; });
		EXPECT_EQ (refusal.rfind ("Entablement : 'regulae.gutta.sides'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("hors de"), std::string::npos) << refusal;

		// Au-dela des seuils exacts de la premiere liste (gardes : une egalite y
		// est un contact refuse) : arriere de la goutte franchement derriere le
		// dos, diametre de tete franchement negatif, regula d'angle retournee.
		refusal = validation ([] (GreekEntablatureSettings &e) { e.regulae->gutta.footDiameter = 0.05; });
		EXPECT_EQ (refusal.rfind ("Entablement : 'regulae.gutta.footDiameter'", 0), 0u) << refusal;
		refusal = validation ([] (GreekEntablatureSettings &e) { e.regulae->gutta.topProjection = 0.48; });
		EXPECT_EQ (refusal.rfind ("Entablement : 'regulae.gutta.topProjection'", 0), 0u) << refusal;
		refusal = validation ([] (GreekEntablatureSettings &e) {
			e.bands[1] = {0.07, 0.08, 0.08};
			e.bands[0].topProjection = 0.07;
			e.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;
		});
		EXPECT_EQ (refusal.rfind ("Entablement : 'triglyph.width'", 0), 0u) << refusal;
		EXPECT_NE (refusal.find ("regula d'angle degeneree"), std::string::npos) << refusal;

		// Mutule portant des gouttes : refuse avant toute lecture des gouttes.
		OrnamentPiece carrying = o.mutules.front ();
		carrying.guttaCount = 1;
		refusal = RefusalOf ([&] { BuildOrnamentMesh (equal, p, o, carrying, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : mutule portant des gouttes", 0), 0u) << refusal;

		// Regula d'axe d'angle au contact, en float, du plan de dos des regulae du
		// cote voisin : width / 2 = 0,185 sous un dos de 0,185 + 1e-8 (accepte en
		// double, confondu en float a H = 2,68).
		GreekEntablatureSettings touching = cgarchi_test::SyntheticOrnamentSettings ();
		touching.bands[1] = {0.07, 0.185 + 1e-8, 0.185 + 1e-8};
		touching.bands[0].topProjection = 0.185;
		ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (touching); }), "");
		const OrnamentLayout ot = LayoutGreekOrnaments (touching, p);
		const OrnamentPiece *axisCorner = nullptr, *inner = nullptr;
		for (const OrnamentPiece &r : ot.regulae)
			if (r.side == PeristyleSide::Front && r.rank == 0)
				axisCorner = &r;
			else if (r.side == PeristyleSide::Front && r.rank == 1)
				inner = &r;
		ASSERT_TRUE (axisCorner && inner);
		refusal = RefusalOf ([&] { BuildOrnamentMesh (touching, p, ot, *axisCorner, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : 'triglyph.width' met la regula au contact", 0), 0u) << refusal;
		EXPECT_EQ (RefusalOf ([&] { BuildOrnamentMesh (touching, p, ot, *inner, 6.27); }), "");

		// BuildEntablaturePieceMeshes appelle afterEach des qu'une piece est
		// construite : sur le jeu touching (triglyphes acceptes, premiere regula
		// refusee en float), une exception levee au premier appel arrete la
		// construction avant la regula fautive.
		const FriezeLayout ft = LayoutGreekFrieze (touching, p);
		int calls = 0;
		std::string stop;
		try
		{
			BuildEntablaturePieceMeshes (touching, p, ft, ot, 6.27, [&] (const Mesh &) {
				++calls;
				throw std::runtime_error ("arret");
			});
		}
		catch (const std::exception &e)
		{
			stop = e.what ();
		}
		EXPECT_EQ (stop, "arret");
		EXPECT_EQ (calls, 1);
		// Jeu accepte : un appel par piece rendue, dans l'ordre.
		const GreekEntablatureSettings accepted = cgarchi_test::SyntheticOrnamentSettings ();
		const OrnamentLayout oa = LayoutGreekOrnaments (accepted, p);
		const FriezeLayout fa = LayoutGreekFrieze (accepted, p);
		std::vector<const Mesh *> seen;
		const std::vector<std::unique_ptr<Mesh>> pieces =
			BuildEntablaturePieceMeshes (accepted, p, fa, oa, 6.27, [&] (const Mesh &m) { seen.push_back (&m); });
		ASSERT_EQ (pieces.size (), fa.triglyphs.size () + oa.regulae.size () + oa.mutules.size ());
		ASSERT_EQ (seen.size (), pieces.size ());
		for (std::size_t k = 0; k < pieces.size (); ++k)
			EXPECT_EQ (seen[k], pieces[k].get ()) << k;

		// Gardes float du pied des gouttes : haut de la bande f - 3 a 1e-9 sous
		// l'arriere du pied (0,475), et arriere du pied a 1e-9 devant la face de
		// la bande f - 2 (0,47) : acceptes en double, contacts en float.
		GreekEntablatureSettings nearBand = cgarchi_test::SyntheticOrnamentSettings ();
		nearBand.bands[0].topProjection = 0.475 - 1e-9;
		GreekEntablatureSettings nearFace = cgarchi_test::SyntheticOrnamentSettings ();
		nearFace.regulae->gutta.footDiameter = 0.03 - 1e-9;
		const std::pair<const GreekEntablatureSettings *, std::string> nearCases[] = {
			{&nearBand, "Ornement : 'bands[0].topProjection' met le pied d'une goutte au contact"},
			{&nearFace, "Ornement : 'regulae.gutta.footDiameter' met l'arriere du pied d'une goutte au contact"}};
		for (const auto &c : nearCases)
		{
			ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (*c.first); }), "");
			const OrnamentLayout on = LayoutGreekOrnaments (*c.first, p);
			const OrnamentPiece *regula = nullptr;
			for (const OrnamentPiece &r : on.regulae)
				if (r.side == PeristyleSide::Front && r.rank == 1)
					regula = &r;
			ASSERT_NE (regula, nullptr);
			refusal = RefusalOf ([&] { BuildOrnamentMesh (*c.first, p, on, *regula, 6.27); });
			EXPECT_EQ (refusal.rfind (c.second, 0), 0u) << refusal;
		}
	}
	// Sous-face inclinee et gouttes des mutules (lot N3a, D3-76), sur
	// SyntheticInclinedSoffitSettings : un refus par champ, dans l'ordre du
	// contrat ; garde du pied (D3-70) a la hauteur limite 0,06223.
	{
		const std::vector<std::pair<std::string, Edit>> inclinedCases = {
			{"mutules.rise", [] (GreekEntablatureSettings &e) { e.mutules->rise = -0.01; }},
			{"mutules.rise", [&] (GreekEntablatureSettings &e) { e.mutules->rise = nan; }},
			{"mutules.rise", [] (GreekEntablatureSettings &e) { e.mutules->rise = 0.25; }},
			{"mutules.guttae.rowDiameters", [] (GreekEntablatureSettings &e) { e.mutules->guttae->rowDiameters.clear (); }},
			{"mutules.guttae.rowDiameters", [] (GreekEntablatureSettings &e) {
				 e.mutules->guttae->rowDiameters.assign (kGreekMaxGuttae + 1, 0.001);
			 }},
			{"mutules.guttae.rowDiameters", [] (GreekEntablatureSettings &e) { e.mutules->guttae->rowDiameters[1] = 0.0; }},
			{"mutules.guttae.rowGaps", [] (GreekEntablatureSettings &e) { e.mutules->guttae->rowGaps = {0.08}; }},
			{"mutules.guttae.rowGaps", [&] (GreekEntablatureSettings &e) { e.mutules->guttae->rowGaps[0] = nan; }},
			{"mutules.guttae.conicity", [] (GreekEntablatureSettings &e) { e.mutules->guttae->conicity = -0.001; }},
			{"mutules.guttae.conicity", [&] (GreekEntablatureSettings &e) { e.mutules->guttae->conicity = nan; }},
			{"mutules.guttae.conicity", [] (GreekEntablatureSettings &e) { e.mutules->guttae->rowGaps = {0.08, 0.003}; }},
			{"mutules.guttae.perRow", [] (GreekEntablatureSettings &e) { e.mutules->guttae->perRow = 1; }},
			{"mutules.guttae.perRow", [] (GreekEntablatureSettings &e) { e.mutules->guttae->perRow = kGreekMaxGuttae + 1; }},
			{"mutules.guttae.perRow", [] (GreekEntablatureSettings &e) { e.mutules->guttae->perRow = 7; }},
			{"mutules.guttae.height", [] (GreekEntablatureSettings &e) { e.mutules->guttae->height = 0.0; }},
			{"mutules.guttae.edgeAxis", [] (GreekEntablatureSettings &e) { e.mutules->guttae->edgeAxis = 0.016; }},
			{"mutules.guttae.edgeAxis", [] (GreekEntablatureSettings &e) { e.mutules->guttae->edgeAxis = 0.019; }},
			{"mutules.guttae.sides", [] (GreekEntablatureSettings &e) { e.mutules->guttae->sides = 6; }},
			{"mutules.guttae", [] (GreekEntablatureSettings &e) { e.mutules->guttae->height = 0.067; }},
			{"mutules.guttae", [] (GreekEntablatureSettings &e) { e.mutules->guttae->height = 0.08; }},
			{"mutules.guttae", [] (GreekEntablatureSettings &e) { e.mutules->guttae->conicity = 0.016; }},
		};
		for (const auto &c : inclinedCases)
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			c.second (e);
			const std::string refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			EXPECT_EQ (refusal.rfind ("Entablement : '" + c.first + "'", 0), 0u) << c.first << " ; obtenu : '" << refusal << "'";
		}
		// Acceptes : 6 gouttes par rang, hauteur 0,06 (sous la limite de la
		// garde), gouttes sous une sous-face horizontale, montee seule.
		const std::vector<Edit> accepted = {
			[] (GreekEntablatureSettings &e) { e.mutules->guttae->perRow = 6; },
			[] (GreekEntablatureSettings &e) { e.mutules->guttae->height = 0.06; },
			[] (GreekEntablatureSettings &e) { e.mutules->rise = 0.0; },
			[] (GreekEntablatureSettings &e) { e.mutules->guttae.reset (); },
		};
		for (std::size_t k = 0; k < accepted.size (); ++k)
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			accepted[k] (e);
			EXPECT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "") << k;
			EXPECT_EQ (RefusalOf ([&] { LayoutGreekOrnaments (e, p); }), "") << k;
		}
		// Plage de gouttes de mutule hors de mutuleGuttae.
		const GreekEntablatureSettings inclined = cgarchi_test::SyntheticInclinedSoffitSettings ();
		const OrnamentLayout oi = LayoutGreekOrnaments (inclined, p);
		ASSERT_FALSE (oi.mutules.empty ());
		OrnamentPiece beyond = oi.mutules.back ();
		beyond.firstGutta = static_cast<int> (oi.mutuleGuttae.size ()) - 1;
		std::string refusal = RefusalOf ([&] { BuildOrnamentMesh (inclined, p, oi, beyond, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : plage de gouttes", 0), 0u) << refusal;

		// Garde du pied : face des triglyphes dans M (pieds a 0,512 contre une
		// face a 0,60, sous rise = 0 et une hauteur de 0,15), et point le plus
		// bas des pieds au-dessus du pied de l'entablement (hauteur 1,5) ; la
		// hauteur 0,15 sous la face du jeu (0,49) est acceptee.
		const std::pair<double, std::string> footCases[] = {{0.60, "triglyphe"}, {0.49, ""}};
		for (const auto &fc : footCases)
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->rise = 0.0;
			e.mutules->guttae->height = 0.15;
			e.triglyph.face = fc.first;
			refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			if (fc.second.empty ())
				EXPECT_EQ (refusal, "") << fc.first;
			else
			{
				EXPECT_EQ (refusal.rfind ("Entablement : 'mutules.guttae' ", 0), 0u) << refusal;
				EXPECT_NE (refusal.find (fc.second), std::string::npos) << refusal;
			}
		}
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->rise = 0.0;
			e.mutules->guttae->height = 1.5;
			refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			EXPECT_EQ (refusal.rfind ("Entablement : 'mutules.guttae' ", 0), 0u) << refusal;
			EXPECT_NE (refusal.find ("pied de l'entablement"), std::string::npos) << refusal;
		}
		// Borne des gouttes par mutule : 3 rangs de 22 = 66 > kGreekMaxGuttae,
		// refuse par le produit avant le pas ; 3 x 6 = 18 accepte (ci-dessus).
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->guttae->perRow = 22;
			refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			EXPECT_EQ (refusal.rfind ("Entablement : 'mutules.guttae.perRow'", 0), 0u) << refusal;
			EXPECT_NE (refusal.find ("rangs x perRow = 66"), std::string::npos) << refusal;
		}
		// 2 rangs de 33 = 66 : refuse par la borne seule (pas 32 x 0,004 = 0,128 <
		// 0,22 ; bord et garde tenus).
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->guttae->rowDiameters = {0.003, 0.003};
			e.mutules->guttae->rowGaps = {0.08};
			e.mutules->guttae->conicity = 0.0005;
			e.mutules->guttae->perRow = 33;
			refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
			EXPECT_EQ (refusal.rfind ("Entablement : 'mutules.guttae.perRow'", 0), 0u) << refusal;
			EXPECT_NE (refusal.find ("rangs x perRow = 66"), std::string::npos) << refusal;
		}
		// Garde en z pres de sa frontiere, sous rise = 0,001 et c = 0,002 : fenetre
		// H D / L < z6 < (H D + c rise) / L de 6,7e-6 ; H au milieu refuse, H a
		// 3,3e-6 sous la fenetre accepte (n3r/cases2.py).
		{
			const double rise = 0.001, c = 0.002, depth = 0.30, z6 = GreekEntablatureBandBounds (inclined)[6];
			const double length = std::hypot (depth, rise);
			const std::pair<double, bool> heights[] = {{(z6 * length - 0.5 * c * rise) / depth, false},
			                                           {(z6 * length - 1.5 * c * rise) / depth, true}};
			for (const auto &h : heights)
			{
				GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
				e.mutules->rise = rise;
				e.mutules->guttae->conicity = c;
				e.mutules->guttae->height = h.first;
				refusal = RefusalOf ([&] { ValidateGreekEntablatureSettings (e); });
				if (h.second)
					EXPECT_EQ (refusal, "") << h.first;
				else
				{
					EXPECT_EQ (refusal.rfind ("Entablement : 'mutules.guttae' ", 0), 0u) << refusal;
					EXPECT_NE (refusal.find ("pied de l'entablement"), std::string::npos) << refusal;
				}
			}
		}
		// Garde float en z : rise = 0, H = z6 - 1e-9, accepte en double ; l'avant
		// du pied tombe sur float (zFoot) : refus au maillage, sur chaque cote.
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->rise = 0.0;
			e.mutules->guttae->height = GreekEntablatureBandBounds (e)[6] - 1e-9;
			ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
			const OrnamentLayout o = LayoutGreekOrnaments (e, p);
			int sides = 0;
			for (const OrnamentPiece &m : o.mutules)
				if (m.rank == 1)
				{
					++sides;
					refusal = RefusalOf ([&] { BuildOrnamentMesh (e, p, o, m, 6.27); });
					EXPECT_EQ (refusal.rfind ("Ornement : 'mutules.guttae' met le pied d'une goutte au contact, en float, du pied de l'entablement", 0), 0u)
						<< static_cast<int> (m.side) << " ; " << refusal;
				}
			EXPECT_EQ (sides, 4);
		}
		// Refus de BuildOrnamentMesh : montee non finie d'un mutule, rang de
		// goutte hors des reglages, sommets d'une tete confondus en float
		// (diametre de tete 1e-9).
		const OrnamentPiece &mutule = oi.mutules.front ();
		OrnamentPiece nanRise = mutule;
		nanRise.rise = nan;
		refusal = RefusalOf ([&] { BuildOrnamentMesh (inclined, p, oi, nanRise, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : piece vide ou non finie (montee", 0), 0u) << refusal;
		OrnamentLayout badRow = oi;
		badRow.mutuleGuttae[static_cast<std::size_t> (mutule.firstGutta)].row = 3;
		refusal = RefusalOf ([&] { BuildOrnamentMesh (inclined, p, badRow, mutule, 6.27); });
		EXPECT_EQ (refusal.rfind ("Ornement : goutte de mutule d'un rang hors", 0), 0u) << refusal;
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			e.mutules->guttae->rowDiameters[0] = 1e-9;
			ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
			const OrnamentLayout o = LayoutGreekOrnaments (e, p);
			refusal = RefusalOf ([&] { BuildOrnamentMesh (e, p, o, o.mutules.front (), 6.27); });
			EXPECT_EQ (refusal.rfind ("Ornement : deux sommets d'une goutte de mutule confondus", 0), 0u) << refusal;
		}
		// Garde float du pied : listel (bande 5) a 1e-9 derriere l'arriere du pied
		// de la derniere goutte, P - (S' D + H rise) / L ; accepte en double, au
		// contact en float sur chaque cote.
		{
			GreekEntablatureSettings e = cgarchi_test::SyntheticInclinedSoffitSettings ();
			const double depth = 0.30, rise = 0.06, reach = 0.266 + 0.002;
			const double footBack = 0.78 - (reach * depth + 0.02 * rise) / std::hypot (depth, rise);
			e.bands[5].projection = e.bands[5].topProjection = footBack - 1e-9;
			ASSERT_EQ (RefusalOf ([&] { ValidateGreekEntablatureSettings (e); }), "");
			const OrnamentLayout o = LayoutGreekOrnaments (e, p);
			int sides = 0;
			for (const OrnamentPiece &m : o.mutules)
				if (m.rank == 1)
				{
					++sides;
					refusal = RefusalOf ([&] { BuildOrnamentMesh (e, p, o, m, 6.27); });
					EXPECT_EQ (refusal.rfind ("Ornement : 'mutules.guttae' met l'arriere du pied d'une goutte au contact, en float", 0), 0u)
						<< static_cast<int> (m.side) << " ; " << refusal;
				}
			EXPECT_EQ (sides, 4);
		}
	}
}

// N5
TEST (TEST_cgarchi_classical_greek_entablature, the_frieze_has_a_triglyph_on_every_axis_and_mid_bay)
{
	const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	const double w = e.triglyph.width, half = 0.5 * w, back = e.bands[3].projection;
	const std::vector<double> z = SyntheticBounds ();
	auto check = [&] (const PeristyleLayout &p, std::size_t &triglyphs, std::size_t &metopes) {
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		triglyphs = 0;
		metopes = 0;
		for (const PeristyleSide side : kSides)
		{
			SCOPED_TRACE (static_cast<int> (side));
			const std::vector<double> axes = AxesAlong (p, side);
			const int n = static_cast<int> (axes.size ());
			ASSERT_EQ (n, side == PeristyleSide::Front || side == PeristyleSide::Back ? p.front : p.flank);
			std::vector<double> centres;
			for (int i = 0; i + 1 < n; ++i)
			{
				centres.push_back (axes[static_cast<std::size_t> (i)]);
				centres.push_back (EqualParts (axes[static_cast<std::size_t> (i)], axes[static_cast<std::size_t> (i) + 1], 2, "a")[1]);
			}
			centres.push_back (axes.back ());
			const std::vector<Interval> gaps = GapsBetweenCentred (centres, w, "w");

			std::vector<FriezePiece> t, m;
			for (const FriezePiece &q : f.triglyphs)
				if (q.side == side)
					t.push_back (q);
			for (const FriezePiece &q : f.metopes)
				if (q.side == side)
					m.push_back (q);
			ASSERT_EQ (t.size (), static_cast<std::size_t> (2 * n - 1));
			ASSERT_EQ (m.size (), static_cast<std::size_t> (2 * n));
			triglyphs += t.size ();
			metopes += m.size ();
			auto byA = [] (const FriezePiece &a, const FriezePiece &b) { return a.a0 < b.a0; };
			std::sort (t.begin (), t.end (), byA);
			std::sort (m.begin (), m.end (), byA);
			for (std::size_t k = 0; k < t.size (); ++k)
			{
				EXPECT_EQ (t[k].kind, FriezeElement::Triglyph);
				EXPECT_TRUE (SameBits (t[k].a0, centres[k] - half)) << k;
				EXPECT_TRUE (SameBits (t[k].a1, centres[k] + half)) << k;
				EXPECT_TRUE (SameBits (t[k].d0, back) && SameBits (t[k].d1, e.triglyph.face)) << k;
				EXPECT_TRUE (SameBits (t[k].z0, z[3]) && SameBits (t[k].z1, z[4])) << k;
			}
			// Metopes courantes : les intervalles entre triglyphes.
			for (std::size_t k = 0; k < gaps.size (); ++k)
			{
				EXPECT_EQ (m[k + 1].kind, FriezeElement::Metope);
				EXPECT_TRUE (SameBits (m[k + 1].a0, gaps[k].lo) && SameBits (m[k + 1].a1, gaps[k].hi)) << k;
				EXPECT_TRUE (SameBits (m[k + 1].d0, back) && SameBits (m[k + 1].d1, back)) << k;
			}
			// Travees d'angle contractees : metopes differentes des courantes.
			if (n >= 4)
			{
				const double corner = m[1].a1 - m[1].a0, current = m[3].a1 - m[3].a0;
				EXPECT_GT (std::fabs (corner - current), 1e-3) << corner << " " << current;
			}
		}
	};

	// Peristyle synthetique 4 x 5 : metopes de 0,58 et 0,495 en facade, 0,48 et
	// 0,435 sur le flanc.
	const PeristyleLayout p = SyntheticPeristyle ();
	std::size_t t45 = 0, m45 = 0;
	check (p, t45, m45);
	EXPECT_EQ (t45, 2u * (7u + 9u));
	EXPECT_EQ (m45, 2u * (8u + 10u));
	const FriezeLayout f = LayoutGreekFrieze (e, p);
	// SidePieces : metopes aux rangs pairs (0 : demi-metope d'angle ; 2 :
	// travee d'angle ; 6 : premiere travee courante).
	const std::vector<FriezePiece> front = SidePieces (f, PeristyleSide::Front);
	EXPECT_NEAR (front[2].a1 - front[2].a0, 0.495, 1e-12);   // travee d'angle, entraxe 1,73
	EXPECT_NEAR (front[6].a1 - front[6].a0, 0.58, 1e-12);    // travee courante, entraxe 1,9
	const std::vector<FriezePiece> right = SidePieces (f, PeristyleSide::Right);
	EXPECT_NEAR (right[2].a1 - right[2].a0, 0.435, 1e-12);
	EXPECT_NEAR (right[6].a1 - right[6].a0, 0.48, 1e-12);

	// 6 x 13, entraxes quelconques : 11 et 25 triglyphes, 10 et 24 metopes
	// courantes par cote, 72 prismes.
	PeristyleSettings big;
	big.front = 6;
	big.flank = 13;
	big.frontSpacing = 2.5837;
	big.flankSpacing = 2.5811;
	big.frontCornerSpacing = 2.4013;
	big.flankCornerSpacing = 2.4039;
	const PeristyleLayout p613 = LayoutPeristyle (big);
	std::size_t t613 = 0, m613 = 0;
	check (p613, t613, m613);
	EXPECT_EQ (t613, 72u);
	EXPECT_EQ (m613, 2u * (10u + 2u) + 2u * (24u + 2u));
	RecordProperty ("triglyphs_6x13", static_cast<int> (t613));
}

// N6
TEST (TEST_cgarchi_classical_greek_entablature, the_corner_keeps_its_axis_triglyph_and_a_half_metope)
{
	const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	const PeristyleLayout p = SyntheticPeristyle ();
	const FriezeLayout f = LayoutGreekFrieze (e, p);
	const double half = 0.5 * e.triglyph.width, back = e.bands[3].projection;

	// Boite monde (x0, x1, y0, y1) d'un triglyphe.
	auto box = [&] (const FriezePiece &q) {
		switch (q.side)
		{
		case PeristyleSide::Front: return std::array<double, 4> {q.a0, q.a1, -(p.halfDepth + q.d1), -(p.halfDepth + q.d0)};
		case PeristyleSide::Right: return std::array<double, 4> {p.halfWidth + q.d0, p.halfWidth + q.d1, q.a0, q.a1};
		case PeristyleSide::Back:  return std::array<double, 4> {q.a0, q.a1, p.halfDepth + q.d0, p.halfDepth + q.d1};
		case PeristyleSide::Left:  return std::array<double, 4> {-(p.halfWidth + q.d1), -(p.halfWidth + q.d0), q.a0, q.a1};
		}
		return std::array<double, 4> {};
	};
	auto disjoint = [] (const std::array<double, 4> &a, const std::array<double, 4> &b) {
		return a[1] < b[0] || b[1] < a[0] || a[3] < b[2] || b[3] < a[2];
	};

	for (int s = 0; s < 4; ++s)
	{
		const PeristyleSide side = kSides[s], next = kSides[(s + 1) % 4];
		SCOPED_TRACE (s);
		const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
		const double H = alongX ? p.halfWidth : p.halfDepth;
		// Rang 0 : triglyphe centre sur l'axe de l'angle qui ouvre le cote.
		const PeristyleColumn *opening = nullptr;
		for (const PeristyleColumn &c : p.columns)
			if (c.side == side && c.corner)
				opening = &c;
		ASSERT_NE (opening, nullptr);
		const double axis = alongX ? opening->x : opening->y;
		const FriezePiece *first = nullptr, *last = nullptr;
		int lastRank = -1;
		for (const FriezePiece &q : f.triglyphs)
			if (q.side == side)
			{
				if (q.rank == 0)
					first = &q;
				if (q.rank > lastRank)
				{
					lastRank = q.rank;
					last = &q;
				}
			}
		ASSERT_NE (first, nullptr);
		EXPECT_TRUE (SameBits (first->a0, axis - half) && SameBits (first->a1, axis + half));

		// Les triglyphes d'angle de deux cotes voisins sont disjoints.
		const FriezePiece *nextFirst = nullptr;
		for (const FriezePiece &q : f.triglyphs)
			if (q.side == next && q.rank == 0)
				nextFirst = &q;
		ASSERT_NE (nextFirst, nullptr);
		EXPECT_TRUE (disjoint (box (*last), box (*nextFirst)));
		// Cas positif du detecteur : les memes triglyphes elargis a half > b se
		// recouvrent.
		FriezePiece wideLast = *last, wideNext = *nextFirst;
		const double over = back + 0.01;
		wideLast.a0 = 0.5 * (last->a0 + last->a1) - over;
		wideLast.a1 = 0.5 * (last->a0 + last->a1) + over;
		wideNext.a0 = 0.5 * (nextFirst->a0 + nextFirst->a1) - over;
		wideNext.a1 = 0.5 * (nextFirst->a0 + nextFirst->a1) + over;
		EXPECT_FALSE (disjoint (box (wideLast), box (wideNext)));

		// Metopes d'angle : du bord du triglyphe d'angle au coin du nu des
		// metopes, sur les quatre cotes ; demi-metope de largeur b - w / 2.
		const std::vector<FriezePiece> pieces = SidePieces (f, side);
		ASSERT_GE (pieces.size (), 3u);
		const FriezePiece &lo = pieces.front (), &hi = pieces.back ();
		EXPECT_EQ (lo.kind, FriezeElement::Metope);
		EXPECT_EQ (hi.kind, FriezeElement::Metope);
		EXPECT_TRUE (SameBits (lo.a0, -(H + back)));
		EXPECT_TRUE (SameBits (hi.a1, H + back));
		EXPECT_NEAR (lo.a1 - lo.a0, back - half, 1e-12);
		EXPECT_NEAR (hi.a1 - hi.a0, back - half, 1e-12);
	}
}

// N7
TEST (TEST_cgarchi_classical_greek_entablature, the_triglyph_prism_stays_in_front_of_its_back)
{
	const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	const GreekTriglyphSettings &t = e.triglyph;
	const PeristyleLayout p = SyntheticPeristyle ();
	const FriezeLayout f = LayoutGreekFrieze (e, p);
	const double back = e.bands[3].projection, zFoot = 6.27;
	const std::vector<double> z = SyntheticBounds ();

	for (const FriezePiece &q : f.triglyphs)
	{
		if (q.rank != 2)
			continue;
		SCOPED_TRACE (static_cast<int> (q.side));
		// Profil attendu, a croissant : demi-canal (fond au bord a face -
		// halfChannelDepth), bande, canal (fond au milieu a face -
		// channelDepth), bande, canal, bande, demi-canal.
		std::vector<double> b {q.a0};
		for (const double wdiv : t.divisionWidths)
			b.push_back (b.back () + wdiv);
		b.back () = q.a1;
		const double fc = t.face - t.channelDepth, fh = t.face - t.halfChannelDepth;
		const std::vector<std::array<double, 2>> profile = {
			{b[0], fh},           {b[1], t.face}, {b[2], t.face}, {0.5 * (b[2] + b[3]), fc}, {b[3], t.face},
			{b[4], t.face},       {0.5 * (b[4] + b[5]), fc}, {b[5], t.face}, {b[6], t.face}, {b[7], fh}};
		for (const auto &pt : profile)
			EXPECT_GT (pt[1], back);

		auto world = [&] (double a, double d) -> std::array<double, 2> {
			switch (q.side)
			{
			case PeristyleSide::Front: return {a, -(p.halfDepth + d)};
			case PeristyleSide::Right: return {p.halfWidth + d, a};
			case PeristyleSide::Back:  return {a, p.halfDepth + d};
			case PeristyleSide::Left:  return {-(p.halfWidth + d), a};
			}
			return {};
		};
		std::vector<std::array<float, 3>> expected;
		for (const auto &pt : profile)
			for (const double d : {back, pt[1]})
				for (const double zz : {zFoot + z[3], zFoot + z[4]})
				{
					const std::array<double, 2> xy = world (pt[0], d);
					expected.push_back ({static_cast<float> (xy[0]), static_cast<float> (xy[1]), static_cast<float> (zz)});
				}
		std::sort (expected.begin (), expected.end ());

		const std::unique_ptr<Mesh> m = BuildTriglyphMesh (e, p, q, zFoot);
		EXPECT_EQ (SortedVertices (*m), expected);
		EXPECT_EQ (m->GetNFaces (), 76u);
		RecordProperty ("prism_faces", static_cast<int> (m->GetNFaces ()));

		const MeshValidationReport r = ValidateMesh (*m);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.components, 1u);
		double area = 0.0, perimeter = 0.0;   // section : profil, puis le fond de droite a gauche
		std::vector<std::array<double, 2>> poly = profile;
		poly.push_back ({profile.back ()[0], back});
		poly.push_back ({profile.front ()[0], back});
		for (std::size_t i = 0; i < poly.size (); ++i)
		{
			const auto &a = poly[i], &c = poly[(i + 1) % poly.size ()];
			area += 0.5 * (a[0] * c[1] - c[0] * a[1]);
			perimeter += std::hypot (c[0] - a[0], c[1] - a[1]);
		}
		const double height = z[4] - z[3], volume = std::fabs (area) * height;
		// Positions arrondies une fois en float : |x| et |y| < 4 (ulp de [2, 4) :
		// 2,4e-7), z de 6,95 a 7,53 (ulp de [4, 8) : 4,8e-7). La tolerance prend
		// l'ulp de [4, 8), majorant des deux, fois le perimetre de la section et
		// la hauteur : deplacement des cotes de la section et des deux faces.
		const double ulp = static_cast<double> (std::nextafter (4.0f, 5.0f) - 4.0f);
		const double tolerance = perimeter * ulp * height;
		EXPECT_NEAR (r.signedVolume, volume, tolerance);
		RecordProperty ("volume_gap_" + std::to_string (static_cast<int> (q.side)), r.signedVolume - volume);
		RecordProperty ("volume_tolerance", tolerance);
	}
}

// N8
TEST (TEST_cgarchi_classical_greek_entablature, triglyphs_and_metopes_tile_each_side_of_the_frieze)
{
	const GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	const double back = e.bands[3].projection;
	PeristyleSettings big;
	big.front = 6;
	big.flank = 13;
	big.frontSpacing = 2.5837;
	big.flankSpacing = 2.5811;
	big.frontCornerSpacing = 2.4013;
	big.flankCornerSpacing = 2.4039;
	for (const PeristyleLayout &p : {SyntheticPeristyle (), LayoutPeristyle (big), LayoutPeristyle (2, 2, 1.9, 1.7)})
	{
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		for (const PeristyleSide side : kSides)
		{
			SCOPED_TRACE (static_cast<int> (side));
			const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
			const double H = alongX ? p.halfWidth : p.halfDepth;
			const std::vector<FriezePiece> pieces = SidePieces (f, side);
			ASSERT_FALSE (pieces.empty ());
			EXPECT_TRUE (SameBits (pieces.front ().a0, -(H + back)));
			EXPECT_TRUE (SameBits (pieces.back ().a1, H + back));
			for (std::size_t k = 0; k < pieces.size (); ++k)
			{
				EXPECT_LT (pieces[k].a0, pieces[k].a1) << k;
				// Metope, triglyphe, metope, ... : alternance d'un coin a l'autre.
				EXPECT_EQ (pieces[k].kind, k % 2 == 0 ? FriezeElement::Metope : FriezeElement::Triglyph) << k;
				if (k + 1 < pieces.size ())
					EXPECT_TRUE (SameBits (pieces[k].a1, pieces[k + 1].a0)) << k;
			}
			// Rangs : 0 a l'angle qui ouvre le cote, dans le sens de parcours.
			const bool descending = side == PeristyleSide::Back || side == PeristyleSide::Left;
			int tri = 0, met = 0;
			std::vector<FriezePiece> ordered = pieces;
			if (descending)
				std::reverse (ordered.begin (), ordered.end ());
			for (const FriezePiece &q : ordered)
				EXPECT_EQ (q.rank, q.kind == FriezeElement::Triglyph ? tri++ : met++);
		}
	}
}

// ---------------------------------------------------------------------------
//  Angle de la frise dorique (lot N1a) : strategies, piece d'angle en L,
//  contraction calculee et rapport. Jeu synthetique seulement.
// ---------------------------------------------------------------------------

namespace {

const DoricCornerStrategy kCornerStrategies[] = {DoricCornerStrategy::CornerTriglyphEqualMetopes,
                                                 DoricCornerStrategy::CornerTriglyphMidBay};

GreekEntablatureSettings WithCorner (DoricCornerStrategy strategy)
{
	GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	e.corner = strategy;
	return e;
}

// Peristyle 6 x 13 a entraxes quelconques (celui de N5 et N8).
PeristyleLayout Peristyle6x13 ()
{
	PeristyleSettings big;
	big.front = 6;
	big.flank = 13;
	big.frontSpacing = 2.5837;
	big.flankSpacing = 2.5811;
	big.frontCornerSpacing = 2.4013;
	big.flankCornerSpacing = 2.4039;
	return LayoutPeristyle (big);
}

bool AlongXSide (PeristyleSide side)
{
	return side == PeristyleSide::Front || side == PeristyleSide::Back;
}

// Front et Right sont parcourus a croissant : angle ouvrant a l'extremite negative.
bool AscendingSide (PeristyleSide side)
{
	return side == PeristyleSide::Front || side == PeristyleSide::Right;
}

PeristyleSide PreviousSide (PeristyleSide side)
{
	return static_cast<PeristyleSide> ((static_cast<int> (side) + 3) % 4);
}

PeristyleSide NextSide (PeristyleSide side)
{
	return static_cast<PeristyleSide> ((static_cast<int> (side) + 1) % 4);
}

// Demi-distance des axes le long du cote, lue dans PeristyleLayout.
double AlongHalf (const PeristyleLayout &p, PeristyleSide side)
{
	return AlongXSide (side) ? p.halfWidth : p.halfDepth;
}

// Placements d'un cote, tries par rang.
std::vector<FriezePiece> ByRank (const std::vector<FriezePiece> &list, PeristyleSide side)
{
	std::vector<FriezePiece> out;
	for (const FriezePiece &q : list)
		if (q.side == side)
			out.push_back (q);
	std::sort (out.begin (), out.end (), [] (const FriezePiece &a, const FriezePiece &b) { return a.rank < b.rank; });
	return out;
}

struct Arm
{
	double a0 = 0.0, a1 = 0.0;
};

// Bras attendus de la piece d'angle qui ouvre `side`, par des sommes seules :
// bout exterieur a +-(H + F), bout interieur a (H + F) - w ; le retour, a
// l'extremite fermante du cote precedent, sur sa demi-distance.
Arm ExpectedOpenArm (const PeristyleLayout &p, const GreekEntablatureSettings &e, PeristyleSide side)
{
	const double outer = AlongHalf (p, side) + e.triglyph.face, inner = outer - e.triglyph.width;
	return AscendingSide (side) ? Arm {-outer, -inner} : Arm {inner, outer};
}

Arm ExpectedReturnArm (const PeristyleLayout &p, const GreekEntablatureSettings &e, PeristyleSide side)
{
	const PeristyleSide previous = PreviousSide (side);
	const double outer = AlongHalf (p, previous) + e.triglyph.face, inner = outer - e.triglyph.width;
	return AscendingSide (previous) ? Arm {inner, outer} : Arm {-outer, -inner};
}

// Axe de l'angle qui ouvre le cote et son voisin, depuis AxesAlong (a croissant).
std::pair<double, double> OpeningBayAxes (const PeristyleLayout &p, PeristyleSide side)
{
	const std::vector<double> axes = AxesAlong (p, side);
	return AscendingSide (side) ? std::make_pair (axes[0], axes[1])
	                            : std::make_pair (axes[axes.size () - 1], axes[axes.size () - 2]);
}

// Decalage du centre d'un placement depuis le milieu de la travee d'angle
// ouvrante, compte positivement vers l'angle.
double ShiftTowardOpeningCorner (const PeristyleLayout &p, PeristyleSide side, const FriezePiece &q)
{
	const std::pair<double, double> bay = OpeningBayAxes (p, side);
	const double middle = 0.5 * (bay.first + bay.second), centre = 0.5 * (q.a0 + q.a1);
	return AscendingSide (side) ? middle - centre : centre - middle;
}

// Profil de face d'un bras [a0, a1] du jeu synthetique (sept divisions), a
// croissant, ecrit comme en N7 : demi-canal, bande, canal, bande, canal,
// bande, demi-canal.
std::vector<std::array<double, 2>> SyntheticArmProfile (const GreekTriglyphSettings &t, double a0, double a1)
{
	std::vector<double> b {a0};
	for (const double wdiv : t.divisionWidths)
		b.push_back (b.back () + wdiv);
	b.back () = a1;
	const double fc = t.face - t.channelDepth, fh = t.face - t.halfChannelDepth;
	return {{b[0], fh}, {b[1], t.face}, {b[2], t.face}, {0.5 * (b[2] + b[3]), fc}, {b[3], t.face},
	        {b[4], t.face}, {0.5 * (b[4] + b[5]), fc}, {b[5], t.face}, {b[6], t.face}, {b[7], fh}};
}

std::array<double, 2> World (const PeristyleLayout &p, PeristyleSide side, double a, double d)
{
	switch (side)
	{
	case PeristyleSide::Front: return {a, -(p.halfDepth + d)};
	case PeristyleSide::Right: return {p.halfWidth + d, a};
	case PeristyleSide::Back:  return {a, p.halfDepth + d};
	case PeristyleSide::Left:  return {-(p.halfWidth + d), a};
	}
	return {};
}

// Section en L attendue de la piece d'angle, en (x, y), dans l'ordre du
// contrat : dos du bras ouvert, son profil vers l'arete sans le point d'arete,
// profil du retour depuis l'arete sans le point d'arete, dos du retour, coin.
std::vector<std::array<double, 2>> ExpectedCornerSection (const PeristyleLayout &p, const GreekEntablatureSettings &e,
                                                          const FriezePiece &q)
{
	const PeristyleSide side = q.side, previous = PreviousSide (side);
	const double b = e.bands[static_cast<std::size_t> (e.friezeBand)].projection;
	std::vector<std::array<double, 2>> open = SyntheticArmProfile (e.triglyph, q.a0, q.a1);
	if (AscendingSide (side))
		std::reverse (open.begin (), open.end ());
	std::vector<std::array<double, 2>> ret = SyntheticArmProfile (e.triglyph, q.ra0, q.ra1);
	if (AscendingSide (previous))
		std::reverse (ret.begin (), ret.end ());
	const double H = AlongHalf (p, side);
	std::vector<std::array<double, 2>> s;
	s.push_back (World (p, side, AscendingSide (side) ? q.a1 : q.a0, b));
	for (std::size_t i = 0; i + 1 < open.size (); ++i)
		s.push_back (World (p, side, open[i][0], open[i][1]));
	for (std::size_t i = 1; i < ret.size (); ++i)
		s.push_back (World (p, previous, ret[i][0], ret[i][1]));
	s.push_back (World (p, previous, AscendingSide (previous) ? q.ra0 : q.ra1, b));
	s.push_back (World (p, side, AscendingSide (side) ? -(H + b) : H + b, b));
	return s;
}

}  // namespace

// CN1
TEST (TEST_cgarchi_classical_greek_entablature, the_corner_triglyph_closes_each_side_at_the_triglyph_face)
{
	struct Case
	{
		std::string name;
		PeristyleLayout p;
		std::size_t total;
	};
	const std::vector<Case> cases = {
		{"4 x 5", SyntheticPeristyle (), 28u}, {"6 x 13", Peristyle6x13 (), 68u}, {"2 x 2", LayoutPeristyle (2, 2, 1.9, 1.7), 8u}};
	for (const DoricCornerStrategy strategy : kCornerStrategies)
		for (const Case &c : cases)
		{
			SCOPED_TRACE (c.name + ", strategie " + std::to_string (static_cast<int> (strategy)));
			const GreekEntablatureSettings e = WithCorner (strategy);
			const double F = e.triglyph.face, back = e.bands[3].projection;
			const FriezeLayout f = LayoutGreekFrieze (e, c.p);
			EXPECT_EQ (f.triglyphs.size (), c.total);
			EXPECT_EQ (f.metopes.size (), c.total);
			for (const PeristyleSide side : kSides)
			{
				SCOPED_TRACE (static_cast<int> (side));
				const int n = AlongXSide (side) ? c.p.front : c.p.flank;
				const std::vector<FriezePiece> t = ByRank (f.triglyphs, side), m = ByRank (f.metopes, side);
				ASSERT_EQ (t.size (), static_cast<std::size_t> (2 * n - 2));
				ASSERT_EQ (m.size (), static_cast<std::size_t> (2 * n - 2));
				for (std::size_t k = 0; k < t.size (); ++k)
				{
					EXPECT_EQ (t[k].rank, static_cast<int> (k));
					EXPECT_EQ (t[k].kind, k == 0 ? FriezeElement::CornerTriglyph : FriezeElement::Triglyph) << k;
					EXPECT_EQ (m[k].rank, static_cast<int> (k));
					EXPECT_EQ (m[k].kind, FriezeElement::Metope) << k;
				}
				const Arm open = ExpectedOpenArm (c.p, e, side), ret = ExpectedReturnArm (c.p, e, side);
				EXPECT_TRUE (SameBits (t[0].a0, open.a0) && SameBits (t[0].a1, open.a1)) << t[0].a0 << " " << t[0].a1;
				EXPECT_TRUE (SameBits (t[0].ra0, ret.a0) && SameBits (t[0].ra1, ret.a1)) << t[0].ra0 << " " << t[0].ra1;
				EXPECT_TRUE (SameBits (t[0].d0, back) && SameBits (t[0].d1, F));

				// Aucun placement centre sur un axe d'angle.
				const double H = AlongHalf (c.p, side);
				for (const FriezePiece &q : t)
					EXPECT_GT (std::fabs (std::fabs (0.5 * (q.a0 + q.a1)) - H), 1e-9) << q.rank;

				// Pavage de [-(H + F), H + F] : pieces du cote et retour de la piece
				// d'angle du cote suivant, bornes contigues au bit pres, bras et
				// triglyphes alternant avec les metopes.
				std::vector<std::pair<Arm, bool>> tiles;   // intervalle, metope
				for (const FriezePiece &q : t)
					tiles.push_back ({{q.a0, q.a1}, false});
				for (const FriezePiece &q : m)
					tiles.push_back ({{q.a0, q.a1}, true});
				const std::vector<FriezePiece> following = ByRank (f.triglyphs, NextSide (side));
				ASSERT_FALSE (following.empty ());
				ASSERT_EQ (following.front ().kind, FriezeElement::CornerTriglyph);
				tiles.push_back ({{following.front ().ra0, following.front ().ra1}, false});
				std::sort (tiles.begin (), tiles.end (),
				           [] (const std::pair<Arm, bool> &a, const std::pair<Arm, bool> &b) { return a.first.a0 < b.first.a0; });
				const double outer = H + F;
				EXPECT_TRUE (SameBits (tiles.front ().first.a0, -outer));
				EXPECT_TRUE (SameBits (tiles.back ().first.a1, outer));
				for (std::size_t k = 0; k < tiles.size (); ++k)
				{
					EXPECT_LT (tiles[k].first.a0, tiles[k].first.a1) << k;
					EXPECT_EQ (tiles[k].second, k % 2 == 1) << k;
					if (k + 1 < tiles.size ())
						EXPECT_TRUE (SameBits (tiles[k].first.a1, tiles[k + 1].first.a0)) << k;
				}
			}
		}
}

// CN2
TEST (TEST_cgarchi_classical_greek_entablature, equal_metopes_shift_the_intermediate_triglyph_by_half_the_corner_offset)
{
	const GreekEntablatureSettings e = WithCorner (DoricCornerStrategy::CornerTriglyphEqualMetopes);
	const PeristyleLayout p = SyntheticPeristyle ();
	const FriezeLayout f = LayoutGreekFrieze (e, p), axis = LayoutGreekFrieze (SyntheticEntablatureSettings (), p);
	for (const PeristyleSide side : kSides)
	{
		SCOPED_TRACE (static_cast<int> (side));
		// Deux metopes de 0,5 (a + c) - w : entraxes d'angle 1,73 et 1,61,
		// c = 0,49 - 0,185 = 0,305, w = 0,37.
		const double expected = AlongXSide (side) ? 0.6475 : 0.5875;
		const std::vector<FriezePiece> m = ByRank (f.metopes, side), t = ByRank (f.triglyphs, side),
		                               ta = ByRank (axis.triglyphs, side);
		ASSERT_GE (m.size (), 4u);
		const std::size_t last = m.size () - 1;
		for (const std::pair<std::size_t, std::size_t> &bay :
		     {std::make_pair (std::size_t (0), std::size_t (1)), std::make_pair (last, last - 1)})
		{
			const double outer = m[bay.first].a1 - m[bay.first].a0, inner = m[bay.second].a1 - m[bay.second].a0;
			EXPECT_NEAR (outer, inner, 1e-12) << bay.first;
			EXPECT_NEAR (outer, expected, 1e-12) << bay.first;
			RecordProperty ("corner_metope_" + std::to_string (static_cast<int> (side)) + "_" + std::to_string (bay.first), outer);
		}
		EXPECT_NEAR (ShiftTowardOpeningCorner (p, side, t[1]), 0.1525, 1e-12);
		// Placements interieurs (rangs 2 a 2n - 4) : ceux de AxisTriglyph.
		ASSERT_EQ (ta.size (), t.size () + 1);
		for (std::size_t r = 2; r + 1 < t.size (); ++r)
			EXPECT_TRUE (SameBits (t[r].a0, ta[r].a0) && SameBits (t[r].a1, ta[r].a1)) << r;
	}
	// Cotes opposes : placements opposes au bit pres.
	for (const std::pair<PeristyleSide, PeristyleSide> &pair :
	     {std::make_pair (PeristyleSide::Front, PeristyleSide::Back), std::make_pair (PeristyleSide::Right, PeristyleSide::Left)})
	{
		SCOPED_TRACE (static_cast<int> (pair.first));
		for (const auto *list : {&f.triglyphs, &f.metopes})
		{
			const std::vector<FriezePiece> a = ByRank (*list, pair.first), b = ByRank (*list, pair.second);
			ASSERT_EQ (a.size (), b.size ());
			for (std::size_t r = 0; r < a.size (); ++r)
				EXPECT_TRUE (SameBits (b[r].a0, -a[r].a1) && SameBits (b[r].a1, -a[r].a0)) << r;
		}
		// Bras de retour des deux pieces d'angle opposees.
		const std::vector<FriezePiece> a = ByRank (f.triglyphs, pair.first), b = ByRank (f.triglyphs, pair.second);
		ASSERT_EQ (a.front ().kind, FriezeElement::CornerTriglyph);
		ASSERT_EQ (b.front ().kind, FriezeElement::CornerTriglyph);
		EXPECT_TRUE (SameBits (b.front ().ra0, -a.front ().ra1) && SameBits (b.front ().ra1, -a.front ().ra0));
	}
}

// CN3
TEST (TEST_cgarchi_classical_greek_entablature, mid_bay_keeps_the_intermediate_triglyph_on_the_bay_middle)
{
	const GreekEntablatureSettings e = WithCorner (DoricCornerStrategy::CornerTriglyphMidBay);
	const PeristyleLayout p = SyntheticPeristyle ();
	const FriezeLayout f = LayoutGreekFrieze (e, p), axis = LayoutGreekFrieze (SyntheticEntablatureSettings (), p);
	for (const PeristyleSide side : kSides)
	{
		SCOPED_TRACE (static_cast<int> (side));
		// a / 2 + c - w du cote de l'angle, a / 2 - w de l'autre.
		const double outerExpected = AlongXSide (side) ? 0.8 : 0.74, innerExpected = AlongXSide (side) ? 0.495 : 0.435;
		const std::vector<FriezePiece> m = ByRank (f.metopes, side), t = ByRank (f.triglyphs, side),
		                               ta = ByRank (axis.triglyphs, side);
		ASSERT_GE (m.size (), 4u);
		const std::size_t last = m.size () - 1;
		EXPECT_NEAR (m[0].a1 - m[0].a0, outerExpected, 1e-12);
		EXPECT_NEAR (m[1].a1 - m[1].a0, innerExpected, 1e-12);
		EXPECT_NEAR (m[last].a1 - m[last].a0, outerExpected, 1e-12);
		EXPECT_NEAR (m[last - 1].a1 - m[last - 1].a0, innerExpected, 1e-12);
		// Tous les placements droits, intermediaires d'angle compris, sont ceux
		// de AxisTriglyph (rangs 1 a 2n - 3).
		ASSERT_EQ (ta.size (), t.size () + 1);
		for (std::size_t r = 1; r < t.size (); ++r)
			EXPECT_TRUE (SameBits (t[r].a0, ta[r].a0) && SameBits (t[r].a1, ta[r].a1)) << r;
	}
}

// CN4
TEST (TEST_cgarchi_classical_greek_entablature, the_computed_contraction_makes_every_metope_current)
{
	const GreekEntablatureSettings equal = WithCorner (DoricCornerStrategy::CornerTriglyphEqualMetopes);
	const std::optional<double> front = ComputedCornerSpacing (equal, 1.9), flank = ComputedCornerSpacing (equal, 1.7);
	ASSERT_TRUE (front.has_value () && flank.has_value ());
	EXPECT_NEAR (*front, 1.595, 1e-12);
	EXPECT_NEAR (*flank, 1.395, 1e-12);

	PeristyleSettings contracted = SyntheticTempleSettings ().peristyle;
	contracted.frontCornerSpacing = *front;
	contracted.flankCornerSpacing = *flank;
	const PeristyleLayout p = LayoutPeristyle (contracted);
	const FriezeLayout f = LayoutGreekFrieze (equal, p);
	ASSERT_EQ (f.metopes.size (), 28u);
	for (const FriezePiece &m : f.metopes)
	{
		const double current = AlongXSide (m.side) ? 0.58 : 0.48;   // s / 2 - w
		EXPECT_NEAR (m.a1 - m.a0, current, 1e-12) << static_cast<int> (m.side) << " " << m.rank;
	}

	const std::optional<double> axis = ComputedCornerSpacing (SyntheticEntablatureSettings (), 1.9);
	ASSERT_TRUE (axis.has_value ());
	EXPECT_TRUE (SameBits (*axis, 1.9));
	EXPECT_FALSE (ComputedCornerSpacing (WithCorner (DoricCornerStrategy::CornerTriglyphMidBay), 1.9).has_value ());

	const double nan = std::numeric_limits<double>::quiet_NaN ();
	for (const double spacing : {nan, 0.0})
		EXPECT_EQ (RefusalOf ([&] { ComputedCornerSpacing (equal, spacing); }).rfind ("Entablement : 'spacing'", 0), 0u) << spacing;
	const double halfOffset = 0.5 * CornerTriglyphOffset (equal);
	EXPECT_EQ (RefusalOf ([&] { ComputedCornerSpacing (equal, halfOffset); }).rfind ("Entablement : 'triglyph.width'", 0), 0u);
}

// CN5
TEST (TEST_cgarchi_classical_greek_entablature, the_corner_report_is_measured_on_the_layout)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes,
	                                           DoricCornerStrategy::CornerTriglyphMidBay})
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		const GreekEntablatureSettings e = WithCorner (strategy);
		const std::array<DoricCornerReport, 4> reports = ReportDoricCorners (e, p);
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		const std::size_t outerRank = strategy == DoricCornerStrategy::AxisTriglyph ? 1 : 0;
		for (std::size_t k = 0; k < 4; ++k)
		{
			const PeristyleSide side = kSides[k];
			SCOPED_TRACE (static_cast<int> (side));
			const DoricCornerReport &r = reports[k];
			const bool alongX = AlongXSide (side);
			EXPECT_EQ (r.side, side);
			EXPECT_TRUE (SameBits (r.spacing, alongX ? p.frontSpacing : p.flankSpacing));
			EXPECT_TRUE (SameBits (r.cornerSpacing, alongX ? p.frontCornerSpacing : p.flankCornerSpacing));
			EXPECT_NEAR (r.currentMetope, alongX ? 0.58 : 0.48, 1e-12);

			// Contre les placements.
			const std::vector<FriezePiece> m = ByRank (f.metopes, side), t = ByRank (f.triglyphs, side);
			EXPECT_NEAR (r.outerMetope, m[outerRank].a1 - m[outerRank].a0, 1e-12);
			EXPECT_NEAR (r.innerMetope, m[outerRank + 1].a1 - m[outerRank + 1].a0, 1e-12);
			EXPECT_NEAR (r.intermediateShift, ShiftTowardOpeningCorner (p, side, t[1]), 1e-12);

			const double a = alongX ? 1.73 : 1.61;
			switch (strategy)
			{
			case DoricCornerStrategy::AxisTriglyph:
				EXPECT_EQ (r.cornerTriglyphOffset, 0.0);
				EXPECT_NEAR (r.outerMetope, alongX ? 0.495 : 0.435, 1e-12);
				EXPECT_NEAR (r.innerMetope, alongX ? 0.495 : 0.435, 1e-12);
				EXPECT_NEAR (r.intermediateShift, 0.0, 1e-12);
				ASSERT_TRUE (r.computedCornerSpacing.has_value () && r.cornerSpacingGap.has_value ());
				EXPECT_TRUE (SameBits (*r.computedCornerSpacing, r.spacing));
				EXPECT_NEAR (*r.cornerSpacingGap, alongX ? -0.17 : -0.09, 1e-12);
				EXPECT_NEAR (*r.cornerSpacingGap, a - r.spacing, 1e-12);
				break;
			case DoricCornerStrategy::CornerTriglyphEqualMetopes:
				EXPECT_NEAR (r.cornerTriglyphOffset, 0.305, 1e-12);
				EXPECT_NEAR (r.outerMetope, alongX ? 0.6475 : 0.5875, 1e-12);
				EXPECT_NEAR (r.innerMetope, alongX ? 0.6475 : 0.5875, 1e-12);
				EXPECT_NEAR (r.intermediateShift, 0.1525, 1e-12);
				ASSERT_TRUE (r.computedCornerSpacing.has_value () && r.cornerSpacingGap.has_value ());
				EXPECT_NEAR (*r.computedCornerSpacing, alongX ? 1.595 : 1.395, 1e-12);
				EXPECT_NEAR (*r.cornerSpacingGap, alongX ? 0.135 : 0.215, 1e-12);
				break;
			case DoricCornerStrategy::CornerTriglyphMidBay:
				EXPECT_NEAR (r.cornerTriglyphOffset, 0.305, 1e-12);
				EXPECT_NEAR (r.outerMetope, alongX ? 0.8 : 0.74, 1e-12);
				EXPECT_NEAR (r.innerMetope, alongX ? 0.495 : 0.435, 1e-12);
				EXPECT_NEAR (r.intermediateShift, 0.0, 1e-12);
				EXPECT_FALSE (r.computedCornerSpacing.has_value ());
				EXPECT_FALSE (r.cornerSpacingGap.has_value ());
				break;
			}
			if (r.cornerSpacingGap)
				RecordProperty ("gap_" + std::to_string (static_cast<int> (strategy)) + "_" + std::to_string (k), *r.cornerSpacingGap);
		}
	}

	// Entraxe courant au plus egal a c (w = 0,1, F = 0,5, b = 0,45, c = 0,45,
	// s = 0,3) : placement accepte, rapport sans contraction calculee ni ecart,
	// ComputedCornerSpacing refuse.
	{
		GreekEntablatureSettings e = WithCorner (DoricCornerStrategy::CornerTriglyphEqualMetopes);
		e.bands[3].projection = e.bands[3].topProjection = 0.45;
		e.innerFace = 0.2;
		e.triglyph.width = 0.1;
		e.triglyph.face = 0.5;
		e.triglyph.divisionWidths = {0.02, 0.02, 0.02, 0.02, 0.02};
		e.triglyph.divisionIsChannel = {true, false, true, false, true};
		e.triglyph.channelDepth = 0.02;
		e.triglyph.halfChannelDepth = 0.02;
		const PeristyleLayout tight = LayoutPeristyle (4, 5, 0.3, 0.3);
		ASSERT_EQ (RefusalOf ([&] { LayoutGreekFrieze (e, tight); }), "");
		std::array<DoricCornerReport, 4> reports {};
		ASSERT_EQ (RefusalOf ([&] { reports = ReportDoricCorners (e, tight); }), "");
		for (const DoricCornerReport &r : reports)
		{
			EXPECT_NEAR (r.cornerTriglyphOffset, 0.45, 1e-12) << static_cast<int> (r.side);
			EXPECT_FALSE (r.computedCornerSpacing.has_value ()) << static_cast<int> (r.side);
			EXPECT_FALSE (r.cornerSpacingGap.has_value ()) << static_cast<int> (r.side);
		}
		EXPECT_EQ (RefusalOf ([&] { ComputedCornerSpacing (e, 0.3); }).rfind ("Entablement : 'triglyph.width'", 0), 0u);
	}

	// Initialisation par agregat des quatre champs de N0a : strategie par defaut.
	{
		const GreekEntablatureSettings s = SyntheticEntablatureSettings ();
		const GreekEntablatureSettings aggregate {s.bands, s.innerFace, s.friezeBand, s.triglyph};
		EXPECT_EQ (aggregate.corner, DoricCornerStrategy::AxisTriglyph);
	}
}

// CN6
TEST (TEST_cgarchi_classical_greek_entablature, the_corner_triglyph_is_one_closed_l_prism_with_a_common_chamfer)
{
	const GreekEntablatureSettings e = WithCorner (DoricCornerStrategy::CornerTriglyphEqualMetopes);
	const PeristyleLayout p = SyntheticPeristyle ();
	const FriezeLayout f = LayoutGreekFrieze (e, p);
	const double zFoot = 6.27, back = e.bands[3].projection, F = e.triglyph.face;
	const double edge = e.triglyph.divisionWidths.front ();
	ASSERT_EQ (edge, e.triglyph.divisionWidths.back ());
	ASSERT_NE (edge, e.triglyph.halfChannelDepth);
	const std::vector<double> z = SyntheticBounds ();
	int corners = 0;
	for (const FriezePiece &q : f.triglyphs)
	{
		if (q.kind != FriezeElement::CornerTriglyph)
			continue;
		++corners;
		SCOPED_TRACE (static_cast<int> (q.side));
		const std::vector<std::array<double, 2>> section = ExpectedCornerSection (p, e, q);
		ASSERT_EQ (section.size (), 21u);
		std::vector<std::array<float, 3>> expected;
		for (const std::array<double, 2> &xy : section)
			for (const double zz : {zFoot + z[3], zFoot + z[4]})
				expected.push_back ({static_cast<float> (xy[0]), static_cast<float> (xy[1]), static_cast<float> (zz)});
		std::sort (expected.begin (), expected.end ());

		const std::unique_ptr<Mesh> m = BuildTriglyphMesh (e, p, q, zFoot);
		EXPECT_EQ (m->GetNVertices (), 42u);
		EXPECT_EQ (SortedVertices (*m), expected);
		EXPECT_EQ (m->GetNFaces (), 80u);
		RecordProperty ("corner_prism_faces", static_cast<int> (m->GetNFaces ()));
		RecordProperty ("corner_prism_vertices", static_cast<int> (m->GetNVertices ()));

		const MeshValidationReport r = ValidateMesh (*m);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.components, 1u);
		double area = 0.0, perimeter = 0.0;
		for (std::size_t i = 0; i < section.size (); ++i)
		{
			const auto &a = section[i], &c = section[(i + 1) % section.size ()];
			area += 0.5 * (a[0] * c[1] - c[0] * a[1]);
			perimeter += std::hypot (c[0] - a[0], c[1] - a[1]);
		}
		const double height = z[4] - z[3], volume = std::fabs (area) * height;
		// Tolerance de N7 : ulp de [4, 8) fois le perimetre et la hauteur.
		const double ulp = static_cast<double> (std::nextafter (4.0f, 5.0f) - 4.0f);
		EXPECT_NEAR (r.signedVolume, volume, perimeter * ulp * height);
		// Aire independante de l'oracle : 0,033525 m2, calculee en arithmetique
		// exacte a la conception (plan 3, MS-46).
		EXPECT_NEAR (std::fabs (area), 0.033525, 1e-12);
		EXPECT_NEAR (r.signedVolume, 0.033525 * height, perimeter * ulp * height);
		RecordProperty ("corner_volume_gap_" + std::to_string (static_cast<int> (q.side)), r.signedVolume - volume);
		RecordProperty ("corner_section_area", std::fabs (area));

		// Coordonnees locales : distances au-dela des deux lignes d'axes de
		// l'angle. Chanfrein a la largeur de la division de bord (0,03) de
		// l'arete, et non a halfChannelDepth (0,04) ; arete absente ; aucun
		// sommet derriere les deux nus a la fois.
		int chamfer = 0, halfChannel = 0, arete = 0, behind = 0;
		for (unsigned i = 0; i < m->GetNVertices (); ++i)
		{
			float v[3];
			m->GetVertex (i, v);
			const double X = std::fabs (static_cast<double> (v[0])) - p.halfWidth;
			const double Y = std::fabs (static_cast<double> (v[1])) - p.halfDepth;
			const double hi = std::max (X, Y), lo = std::min (X, Y);
			if (std::fabs (hi - F) < 1e-6 && std::fabs (lo - (F - edge)) < 1e-6)
				++chamfer;
			if (std::fabs (hi - F) < 1e-6 && std::fabs (lo - (F - e.triglyph.halfChannelDepth)) < 1e-6)
				++halfChannel;
			if (std::fabs (hi - F) < 1e-6 && std::fabs (lo - F) < 1e-6)
				++arete;
			if (X < back - 1e-6 && Y < back - 1e-6)
				++behind;
		}
		EXPECT_EQ (chamfer, 4);
		EXPECT_EQ (halfChannel, 0);
		EXPECT_EQ (arete, 0);
		EXPECT_EQ (behind, 0);
	}
	EXPECT_EQ (corners, 4);

	// Division non symetrique (bord 0,02 en premier, 0,04 en dernier) : chaque
	// bras porte la division d'un triglyphe droit de son cote, depuis a0 ; la
	// profondeur du chanfrein sur chaque face est la division a l'arete de ce
	// bras : la premiere sur un cote parcouru a croissant (Front, Right) quand il
	// ouvre l'angle, la derniere quand il le ferme ; l'inverse sur Back et Left.
	{
		GreekEntablatureSettings asym = e;
		asym.triglyph.divisionWidths = {0.02, 0.05, 0.075, 0.06, 0.075, 0.05, 0.04};
		const FriezeLayout g = LayoutGreekFrieze (asym, p);
		const double first = asym.triglyph.divisionWidths.front (), last = asym.triglyph.divisionWidths.back ();
		int checked = 0;
		for (const FriezePiece &q : g.triglyphs)
		{
			if (q.kind != FriezeElement::CornerTriglyph)
				continue;
			SCOPED_TRACE (static_cast<int> (q.side));
			++checked;
			const PeristyleSide previous = PreviousSide (q.side);
			const double openEdge = AscendingSide (q.side) ? first : last;
			const double returnEdge = AscendingSide (previous) ? last : first;
			const std::unique_ptr<Mesh> m = BuildTriglyphMesh (asym, p, q, zFoot);
			const MeshValidationReport r = ValidateMesh (*m);
			EXPECT_TRUE (r.IsValid ());
			// Sommet du chanfrein sur la face d'un cote : a F de sa ligne d'axes, a
			// F - bord de la ligne d'axes perpendiculaire.
			auto onFace = [&] (PeristyleSide side, double edgeWidth) {
				int count = 0;
				for (unsigned i = 0; i < m->GetNVertices (); ++i)
				{
					float v[3];
					m->GetVertex (i, v);
					const double X = std::fabs (static_cast<double> (v[0])) - p.halfWidth;
					const double Y = std::fabs (static_cast<double> (v[1])) - p.halfDepth;
					const double across = AlongXSide (side) ? Y : X, along = AlongXSide (side) ? X : Y;
					if (std::fabs (across - F) < 1e-6 && std::fabs (along - (F - edgeWidth)) < 1e-6)
						++count;
				}
				return count;
			};
			EXPECT_EQ (onFace (q.side, openEdge), 2) << "face du cote ouvert, bord " << openEdge;
			EXPECT_EQ (onFace (previous, returnEdge), 2) << "face du retour, bord " << returnEdge;
			RecordProperty ("asymmetric_edges_" + std::to_string (static_cast<int> (q.side)),
			                std::to_string (openEdge) + " " + std::to_string (returnEdge));
		}
		EXPECT_EQ (checked, 4);
	}

	// Deux bandes consecutives : trois sommets alignes sur la face de chaque bras.
	{
		GreekEntablatureSettings aligned = e;
		aligned.triglyph.divisionWidths = {0.03, 0.05, 0.05, 0.075, 0.06, 0.075, 0.03};
		aligned.triglyph.divisionIsChannel = {true, false, false, true, false, true, true};
		const FriezeLayout g = LayoutGreekFrieze (aligned, p);
		for (const FriezePiece &q : g.triglyphs)
		{
			if (q.kind != FriezeElement::CornerTriglyph)
				continue;
			SCOPED_TRACE (static_cast<int> (q.side));
			const std::unique_ptr<Mesh> m = BuildTriglyphMesh (aligned, p, q, zFoot);
			EXPECT_EQ (m->GetNFaces (), 80u);
			const MeshValidationReport r = ValidateMesh (*m);
			EXPECT_TRUE (r.IsValid ());
			EXPECT_EQ (r.components, 1u);
			EXPECT_GT (r.signedVolume, 0.0);
		}
	}
}

// ---------------------------------------------------------------------------
//  Ornements de l'entablement (lot N2a) : regulae, gouttes, mutules. Jeu
//  SyntheticOrnamentSettings seulement (bande 6 a 0,48).
// ---------------------------------------------------------------------------

namespace {

using cgarchi_test::SyntheticOrnamentSettings;

GreekEntablatureSettings OrnamentsWith (DoricCornerStrategy strategy)
{
	GreekEntablatureSettings e = SyntheticOrnamentSettings ();
	e.corner = strategy;
	return e;
}

const DoricCornerStrategy kAllStrategies[] = {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes,
                                              DoricCornerStrategy::CornerTriglyphMidBay};

// Bras portant des gouttes : le triglyphe droit, ou les deux bras du triglyphe
// d'une piece d'angle (ouvert, puis retour sur le cote precedent).
struct GuttaArm
{
	PeristyleSide side = PeristyleSide::Front;
	double a0 = 0.0, a1 = 0.0;
};

std::vector<GuttaArm> GuttaArms (const FriezePiece &q)
{
	if (q.kind == FriezeElement::CornerTriglyph)
		return {{q.side, q.a0, q.a1}, {PreviousSide (q.side), q.ra0, q.ra1}};
	return {{q.side, q.a0, q.a1}};
}

// Sous-maillage des sommets [v0, v0 + nv) et des faces [f0, f0 + nf), faces
// reindexees.
std::unique_ptr<Mesh> SubMesh (const Mesh &m, unsigned v0, unsigned nv, unsigned f0, unsigned nf)
{
	std::vector<float> v;
	for (unsigned i = 0; i < nv; ++i)
	{
		float q[3];
		m.GetVertex (v0 + i, q);
		v.insert (v.end (), {q[0], q[1], q[2]});
	}
	std::vector<unsigned> f;
	for (unsigned k = 0; k < nf; ++k)
		for (unsigned j = 0; j < 3; ++j)
		{
			const int index = m.GetFaceVertex (f0 + k, j);
			EXPECT_GE (index, static_cast<int> (v0));
			EXPECT_LT (index, static_cast<int> (v0 + nv));
			f.push_back (static_cast<unsigned> (index) - v0);
		}
	auto out = std::make_unique<Mesh> ();
	out->SetVertices (nv, v.data ());
	out->SetFaces (nf, 3, f.data ());
	return out;
}

std::set<std::array<float, 3>> VertexSet (const Mesh &m)
{
	const std::vector<std::array<float, 3>> v = SortedVertices (m);
	return {v.begin (), v.end ()};
}

std::array<float, 3> FloatPoint (const std::array<double, 2> &xy, double z)
{
	return {static_cast<float> (xy[0]), static_cast<float> (xy[1]), static_cast<float> (z)};
}

// ulp float de [4, 8) : majorant de celui des coordonnees du jeu (|x|, |y| < 4,
// z < 8).
double FloatUlp ()
{
	return static_cast<double> (std::nextafter (4.0f, 5.0f) - 4.0f);
}

}  // namespace

// OR1 : une regula sous chaque placement de triglyphe, sous la tenia, dos
// contre la face de la bande 1 ; bornes copiees du triglyphe, ou etendues au
// plan de face de la regula pour une regula d'angle.
TEST (TEST_cgarchi_classical_greek_entablature, a_regula_hangs_under_the_taenia_below_each_triglyph)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	for (const DoricCornerStrategy strategy : kAllStrategies)
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		const GreekEntablatureSettings e = OrnamentsWith (strategy);
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		const OrnamentLayout o = LayoutGreekOrnaments (e, p);
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		const bool axis = strategy == DoricCornerStrategy::AxisTriglyph;
		ASSERT_EQ (o.regulae.size (), f.triglyphs.size ());
		EXPECT_EQ (o.regulae.size (), axis ? 32u : 28u);
		const double R = e.regulae->face, dos = e.bands[1].projection;
		int corners = 0;
		for (std::size_t k = 0; k < o.regulae.size (); ++k)
		{
			const FriezePiece &q = f.triglyphs[k];
			const OrnamentPiece &r = o.regulae[k];
			EXPECT_EQ (r.side, q.side) << k;
			EXPECT_EQ (r.rank, q.rank) << k;
			EXPECT_TRUE (SameBits (r.d0, 0.47)) << k;
			EXPECT_TRUE (SameBits (r.d1, 0.50)) << k;
			EXPECT_TRUE (SameBits (r.z1, z[2])) << k;
			EXPECT_NEAR (r.z0, z[2] - 0.05, 1e-12) << k;
			if (q.kind == FriezeElement::CornerTriglyph)
			{
				++corners;
				EXPECT_EQ (r.kind, OrnamentElement::CornerRegula) << k;
				const double H = AlongHalf (p, q.side), Hr = AlongHalf (p, PreviousSide (q.side));
				const Arm open = ExpectedOpenArm (p, e, q.side), ret = ExpectedReturnArm (p, e, q.side);
				if (AscendingSide (q.side))
				{
					EXPECT_TRUE (SameBits (r.a0, -(H + R))) << k;
					EXPECT_TRUE (SameBits (r.a1, open.a1)) << k;
				}
				else
				{
					EXPECT_TRUE (SameBits (r.a0, open.a0)) << k;
					EXPECT_TRUE (SameBits (r.a1, H + R)) << k;
				}
				if (AscendingSide (PreviousSide (q.side)))
				{
					EXPECT_TRUE (SameBits (r.ra0, ret.a0)) << k;
					EXPECT_TRUE (SameBits (r.ra1, Hr + R)) << k;
				}
				else
				{
					EXPECT_TRUE (SameBits (r.ra0, -(Hr + R))) << k;
					EXPECT_TRUE (SameBits (r.ra1, ret.a1)) << k;
				}
			}
			else
			{
				EXPECT_EQ (r.kind, OrnamentElement::Regula) << k;
				EXPECT_TRUE (SameBits (r.a0, q.a0)) << k;
				EXPECT_TRUE (SameBits (r.a1, q.a1)) << k;
				EXPECT_TRUE (SameBits (r.ra0, 0.0) && SameBits (r.ra1, 0.0)) << k;
			}
		}
		EXPECT_EQ (corners, axis ? 0 : 4);
		static_cast<void> (dos);
	}
}

// OR2 : 6 gouttes par bras, a marges egales, au nu de la regula au pied ;
// pied au bas de la bande 1, tete au dessous de la regula.
TEST (TEST_cgarchi_classical_greek_entablature, the_guttae_are_spread_with_equal_margins_and_flush_with_the_regula_face)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	for (const DoricCornerStrategy strategy : kAllStrategies)
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		const GreekEntablatureSettings e = OrnamentsWith (strategy);
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		const OrnamentLayout o = LayoutGreekOrnaments (e, p);
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		std::size_t next = 0;
		for (std::size_t k = 0; k < o.regulae.size (); ++k)
		{
			const OrnamentPiece &r = o.regulae[k];
			const std::vector<GuttaArm> arms = GuttaArms (f.triglyphs[k]);
			ASSERT_EQ (r.firstGutta, static_cast<int> (next)) << k;
			ASSERT_EQ (r.guttaCount, static_cast<int> (6 * arms.size ())) << k;
			for (const GuttaArm &arm : arms)
			{
				for (int g = 0; g < 6; ++g)
				{
					const GuttaPlacement &q = o.guttae[next + static_cast<std::size_t> (g)];
					EXPECT_EQ (q.side, arm.side) << k;
					EXPECT_NEAR (q.axis, 0.4875, 1e-12) << k;
					EXPECT_TRUE (SameBits (q.z0, z[1])) << k;
					EXPECT_TRUE (SameBits (q.z1, r.z0)) << k;
					if (g == 0)
					{
						EXPECT_NEAR ((q.a - 0.0125) - arm.a0, 0.0225, 1e-12) << k;
					}
					else
					{
						EXPECT_NEAR ((q.a - 0.0125) - (o.guttae[next + static_cast<std::size_t> (g) - 1].a + 0.0125), 0.035, 1e-12) << k;
					}
					if (g == 5)
					{
						EXPECT_NEAR (arm.a1 - (q.a + 0.0125), 0.0225, 1e-12) << k;
					}
				}
				next += 6;
			}
		}
		EXPECT_EQ (next, o.guttae.size ());
		EXPECT_EQ (o.guttae.size (), 192u);
	}
}

// OR3 : une goutte est un tronc de cone ferme de 18 sommets et 32 faces ;
// sommets avant et arriere exacts ; diametre de tete 0,021 ; volume du tronc
// polygonal. Une regula de rang 1 par cote (les quatre correspondances).
TEST (TEST_cgarchi_classical_greek_entablature, a_gutta_is_a_closed_frustum)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	const GreekEntablatureSettings e = SyntheticOrnamentSettings ();
	const OrnamentLayout o = LayoutGreekOrnaments (e, p);
	const std::vector<double> z = GreekEntablatureBandBounds (e);
	const double zFoot = 6.27;
	int checked = 0;
	for (const OrnamentPiece &r : o.regulae)
	{
		if (r.rank != 1)
			continue;
		SCOPED_TRACE (static_cast<int> (r.side));
		const std::unique_ptr<Mesh> m = BuildOrnamentMesh (e, p, o, r, zFoot);
		ASSERT_EQ (m->GetNVertices (), 8u + 6u * 18u);
		ASSERT_EQ (m->GetNFaces (), 12u + 6u * 32u);
		for (unsigned k = 0; k < 6; ++k)
		{
			const GuttaPlacement &g = o.guttae[static_cast<std::size_t> (r.firstGutta) + k];
			const std::unique_ptr<Mesh> gutta = SubMesh (*m, 8 + 18 * k, 18, 12 + 32 * k, 32);
			const MeshValidationReport report = ValidateMesh (*gutta);
			EXPECT_TRUE (report.IsValid ()) << k;
			EXPECT_EQ (report.components, 1u) << k;
			float q[3];
			gutta->GetVertex (0, q);
			EXPECT_EQ ((std::array<float, 3> {q[0], q[1], q[2]}), FloatPoint (World (p, r.side, g.a, 0.50), zFoot + z[1])) << k;
			gutta->GetVertex (4, q);
			EXPECT_EQ ((std::array<float, 3> {q[0], q[1], q[2]}), FloatPoint (World (p, r.side, g.a, 0.50 - 0.025), zFoot + z[1])) << k;
			gutta->GetVertex (8, q);
			EXPECT_EQ ((std::array<float, 3> {q[0], q[1], q[2]}), FloatPoint (World (p, r.side, g.a, 0.498), zFoot + r.z0)) << k;
			float left[3], right[3];
			gutta->GetVertex (8 + 2, left);
			gutta->GetVertex (8 + 6, right);
			EXPECT_NEAR (std::hypot (left[0] - right[0], left[1] - right[1]), 0.021, 1e-6) << k;
			// Tronc d'octogones reguliers semblables : aire 2 sqrt2 r^2.
			const double rb = 0.0125, rt = 0.0105, h = r.z0 - z[1];
			const double ab = 2.0 * std::sqrt (2.0) * rb * rb, at = 2.0 * std::sqrt (2.0) * rt * rt;
			const double volume = h / 3.0 * (ab + at + std::sqrt (ab * at));
			EXPECT_NEAR (report.signedVolume, volume, report.area * FloatUlp ()) << k;
			if (k == 0)
				RecordProperty ("gutta_volume_gap_" + std::to_string (static_cast<int> (r.side)), report.signedVolume - volume);
		}
		++checked;
	}
	EXPECT_EQ (checked, 4);
	RecordProperty ("gutta_faces", 32);
	RecordProperty ("gutta_vertices", 18);
}

// OR4 : la regula d'angle est un prisme en L de 12 sommets et 20 faces ; aire
// de section (R - dos)(2w + R + dos - 2F) ; coins (R, R) et (dos, dos) du
// dessus sur des sommets d'onglet de l'anneau, au bit pres.
TEST (TEST_cgarchi_classical_greek_entablature, the_corner_regula_is_one_l_prism_around_the_band_corner)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	const double zFoot = 6.27;
	for (const DoricCornerStrategy strategy : kCornerStrategies)
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		const GreekEntablatureSettings e = OrnamentsWith (strategy);
		const OrnamentLayout o = LayoutGreekOrnaments (e, p);
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		const std::set<std::array<float, 3>> ring =
			VertexSet (*SweepRectangularRing (GreekEntablatureSection (e), p.halfWidth, p.halfDepth, zFoot));
		const double R = 0.50, dos = 0.47, F = e.triglyph.face, w = e.triglyph.width;
		const double expectedArea = (R - dos) * (2.0 * w + R + dos - 2.0 * F);
		int corners = 0;
		for (const OrnamentPiece &r : o.regulae)
		{
			if (r.kind != OrnamentElement::CornerRegula)
				continue;
			++corners;
			SCOPED_TRACE (static_cast<int> (r.side));
			const PeristyleSide side = r.side, previous = PreviousSide (side);
			const std::unique_ptr<Mesh> m = BuildOrnamentMesh (e, p, o, r, zFoot);
			ASSERT_EQ (r.guttaCount, 12);
			ASSERT_EQ (m->GetNVertices (), 12u + 12u * 18u);
			ASSERT_EQ (m->GetNFaces (), 20u + 12u * 32u);
			const std::unique_ptr<Mesh> body = SubMesh (*m, 0, 12, 0, 20);
			const MeshValidationReport report = ValidateMesh (*body);
			EXPECT_TRUE (report.IsValid ());
			EXPECT_EQ (report.components, 1u);

			// Hexagone de la section, en double, depuis les bornes de la piece.
			const bool up = AscendingSide (side), upReturn = AscendingSide (previous);
			const double H = AlongHalf (p, side);
			const double openInner = up ? r.a1 : r.a0, openCorner = up ? r.a0 : r.a1, returnInner = upReturn ? r.ra0 : r.ra1;
			const std::vector<std::array<double, 2>> hexagon = {
				World (p, side, openInner, dos),        World (p, side, openInner, R),        World (p, side, openCorner, R),
				World (p, previous, returnInner, R),    World (p, previous, returnInner, dos), World (p, side, up ? -(H + dos) : H + dos, dos)};
			double twice = 0.0;
			for (std::size_t i = 0; i < hexagon.size (); ++i)
			{
				const auto &a = hexagon[i], &b = hexagon[(i + 1) % hexagon.size ()];
				twice += a[0] * b[1] - b[0] * a[1];
			}
			EXPECT_NEAR (std::fabs (0.5 * twice), expectedArea, 1e-12);
			EXPECT_NEAR (report.signedVolume, expectedArea * (r.z1 - r.z0), report.area * FloatUlp ());

			// Coins du dessus : sommets de la regula et de l'anneau, au bit pres.
			const std::set<std::array<float, 3>> own = VertexSet (*body);
			for (const double d : {R, dos})
			{
				const std::array<float, 3> c = FloatPoint (World (p, side, up ? -(H + d) : H + d, d), zFoot + z[2]);
				EXPECT_EQ (own.count (c), 1u) << d;
				EXPECT_EQ (ring.count (c), 1u) << d;
			}
			int onSide = 0, onPrevious = 0;
			for (int k = 0; k < r.guttaCount; ++k)
				(o.guttae[static_cast<std::size_t> (r.firstGutta + k)].side == side ? onSide : onPrevious) += 1;
			EXPECT_EQ (onSide, 6);
			EXPECT_EQ (onPrevious, 6);
		}
		EXPECT_EQ (corners, 4);
	}
	RecordProperty ("corner_regula_faces", 20);
	RecordProperty ("corner_regula_vertices", 12);
}

// OR5 : un mutule centre sur chaque triglyphe (chaque bras d'angle, sur sa
// face) et sur chaque metope ; vias et degagement d'angle, a deux largeurs
// (0,10 : vias larges, une erreur de centre ne tombe pas dans le refus de via) ;
// refus de la via nulle et du recouvrement a l'angle.
TEST (TEST_cgarchi_classical_greek_entablature, a_mutule_is_centred_on_each_triglyph_and_each_metope)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	for (const double width : {0.10, 0.30})
	for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes})
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		SCOPED_TRACE (width);
		const bool axis = strategy == DoricCornerStrategy::AxisTriglyph;
		GreekEntablatureSettings e = OrnamentsWith (strategy);
		e.mutules->width = width;
		const FriezeLayout f = LayoutGreekFrieze (e, p);
		OrnamentLayout o;
		const std::string layoutRefusal = RefusalOf ([&] { o = LayoutGreekOrnaments (e, p); });
		if (!layoutRefusal.empty ())
		{
			ADD_FAILURE () << "refus inattendu : " << layoutRefusal;
			continue;
		}
		const std::vector<double> z = GreekEntablatureBandBounds (e);
		double minVia = 1.0;
		for (const PeristyleSide side : kSides)
		{
			SCOPED_TRACE (static_cast<int> (side));
			const int n = AlongXSide (side) ? p.front : p.flank;
			std::vector<double> centres;
			for (const FriezePiece &q : f.triglyphs)
			{
				if (q.side == side)
					centres.push_back (0.5 * (q.a0 + q.a1));
				if (q.kind == FriezeElement::CornerTriglyph && NextSide (side) == q.side)
					centres.push_back (0.5 * (q.ra0 + q.ra1));
			}
			for (const FriezePiece &q : f.metopes)
				if (q.side == side)
					centres.push_back (0.5 * (q.a0 + q.a1));
			std::sort (centres.begin (), centres.end ());
			const std::vector<OrnamentPiece> mutules = [&] {
				std::vector<OrnamentPiece> out;
				for (const OrnamentPiece &m : o.mutules)
					if (m.side == side)
						out.push_back (m);
				return out;
			} ();
			ASSERT_EQ (mutules.size (), static_cast<std::size_t> (axis ? 4 * n - 1 : 4 * n - 3));
			ASSERT_EQ (centres.size (), mutules.size ());
			const std::size_t count = mutules.size ();
			for (std::size_t r = 0; r < count; ++r)
			{
				const OrnamentPiece &m = mutules[r];
				EXPECT_EQ (m.kind, OrnamentElement::Mutule);
				EXPECT_EQ (m.rank, static_cast<int> (r));
				const double c = centres[AscendingSide (side) ? r : count - 1 - r];
				EXPECT_TRUE (SameBits (m.a0, c - 0.5 * width)) << r;
				EXPECT_TRUE (SameBits (m.a1, c + 0.5 * width)) << r;
				EXPECT_TRUE (SameBits (m.d0, 0.48)) << r;
				EXPECT_TRUE (SameBits (m.d1, 0.78)) << r;
				EXPECT_TRUE (SameBits (m.z0, z[6])) << r;
				EXPECT_TRUE (SameBits (m.z1, z[7])) << r;
				EXPECT_EQ (m.guttaCount, 0);
			}
			for (std::size_t r = 0; r + 1 < count; ++r)
			{
				const double lo = std::min (mutules[r].a1, mutules[r + 1].a1), hi = std::max (mutules[r].a0, mutules[r + 1].a0);
				minVia = std::min (minVia, hi - lo);
			}
			const double limit = AlongHalf (p, side) + 0.48;
			double low = 1e9, high = -1e9;
			for (const OrnamentPiece &m : mutules)
			{
				low = std::min (low, m.a0);
				high = std::max (high, m.a1);
			}
			// Degagement d'angle : 0,0225 (AxisTriglyph) et 0,025 (EqualMetopes) a
			// la largeur 0,30, plus 0,1 a la largeur 0,10.
			EXPECT_NEAR (low + limit, (axis ? 0.1725 : 0.175) - 0.5 * width, 1e-12);
			EXPECT_NEAR (limit - high, (axis ? 0.1725 : 0.175) - 0.5 * width, 1e-12);
		}
		// Via minimale : ecart des centres moins la largeur, 0,0075 et 0,125 a 0,30.
		EXPECT_NEAR (minVia, (axis ? 0.3075 : 0.425) - width, 1e-12);
		if (width == 0.30)
			RecordProperty (std::string ("min_via_") + (axis ? "axis" : "equal"), minVia);
	}

	// Refus 'mutules.width' : largeur 0,48 (via nulle et recouvrement d'angle) ;
	// largeur 0,32 (via seule : -0,0125 entre le triglyphe d'angle et la
	// demi-metope, degagement d'angle 0,0125) ; dos des mutules a 0,44
	// (recouvrement d'angle de 0,0175, vias intactes).
	auto refusal = [&] (const GreekEntablatureSettings &e) { return RefusalOf ([&] { LayoutGreekOrnaments (e, p); }); };
	GreekEntablatureSettings wide = SyntheticOrnamentSettings ();
	wide.mutules->width = 0.48;
	EXPECT_EQ (refusal (wide).rfind ("Entablement : 'mutules.width'", 0), 0u) << refusal (wide);
	GreekEntablatureSettings viaOnly = SyntheticOrnamentSettings ();
	viaOnly.mutules->width = 0.32;
	const std::string viaRefusal = refusal (viaOnly);
	EXPECT_EQ (viaRefusal.rfind ("Entablement : 'mutules.width'", 0), 0u) << viaRefusal;
	EXPECT_NE (viaRefusal.find ("via"), std::string::npos) << viaRefusal;
	GreekEntablatureSettings deep = SyntheticOrnamentSettings ();
	deep.bands[6].projection = 0.44;
	deep.bands[6].topProjection = 0.44;
	const std::string cornerRefusal = refusal (deep);
	EXPECT_EQ (cornerRefusal.rfind ("Entablement : 'mutules.width'", 0), 0u) << cornerRefusal;
	EXPECT_NE (cornerRefusal.find ("recouvrent a l'angle"), std::string::npos) << cornerRefusal;
}

namespace {

using cgarchi_test::SyntheticInclinedSoffitSettings;

// Saillie d depuis la ligne des axes du cote, et coordonnee a le long du cote,
// d'une position du repere du temple (inverse de World).
std::array<double, 2> SideCoordinates (const PeristyleLayout &p, PeristyleSide side, const float q[3])
{
	switch (side)
	{
	case PeristyleSide::Front: return {q[0], -q[1] - p.halfDepth};
	case PeristyleSide::Right: return {q[1], q[0] - p.halfWidth};
	case PeristyleSide::Back:  return {q[0], q[1] - p.halfDepth};
	case PeristyleSide::Left:  return {q[1], -q[0] - p.halfWidth};
	}
	return {};
}

double TwiceArea (const std::vector<RingSectionPoint> &s)
{
	double sum = 0.0;
	for (std::size_t i = 0; i < s.size (); ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % s.size ()];
		sum += a.u * b.z - b.u * a.z;
	}
	return sum;
}

// Mutule de rang 1 de chaque cote.
std::vector<const OrnamentPiece *> RankOneMutules (const OrnamentLayout &o)
{
	std::vector<const OrnamentPiece *> out;
	for (const OrnamentPiece &m : o.mutules)
		if (m.rank == 1)
			out.push_back (&m);
	return out;
}

}  // namespace

// MG1 : la sous-face inclinee remplace, dans la section de l'anneau, le haut de
// la bande des mutules par (0,48 ; z7 + 0,06) puis (0,78 ; z7) ; montee nulle :
// section de N2 au bit pres.
TEST (TEST_cgarchi_classical_greek_entablature, an_inclined_soffit_replaces_the_mutule_band_face_in_the_ring_section)
{
	const GreekEntablatureSettings flat = SyntheticOrnamentSettings ();
	GreekEntablatureSettings zero = SyntheticInclinedSoffitSettings ();
	zero.mutules->rise = 0.0;
	const std::vector<RingSectionPoint> n2 = GreekEntablatureSection (flat), s0 = GreekEntablatureSection (zero);
	ASSERT_EQ (s0.size (), n2.size ());
	EXPECT_EQ (n2.size (), 19u);
	for (std::size_t i = 0; i < n2.size (); ++i)
		EXPECT_TRUE (SameBits (s0[i].u, n2[i].u) && SameBits (s0[i].z, n2[i].z)) << i;

	const GreekEntablatureSettings e = SyntheticInclinedSoffitSettings ();
	const std::vector<double> z = GreekEntablatureBandBounds (e);
	const std::vector<RingSectionPoint> s = GreekEntablatureSection (e);
	ASSERT_EQ (s.size (), 20u);
	std::size_t at = s.size ();
	for (std::size_t i = 0; i + 3 < s.size (); ++i)
		if (SameBits (s[i].u, 0.48) && SameBits (s[i].z, z[6]))
			at = i;
	ASSERT_LT (at + 3, s.size ());
	EXPECT_TRUE (SameBits (s[at + 1].u, 0.48) && SameBits (s[at + 1].z, z[7] + 0.06));
	EXPECT_TRUE (SameBits (s[at + 2].u, 0.78) && SameBits (s[at + 2].z, z[7]));
	EXPECT_TRUE (SameBits (s[at + 3].u, 0.83) && SameBits (s[at + 3].z, z[7]));
	// Face des mutules au pied du bec (0,83) : ressaut omis, 19 sommets.
	GreekEntablatureSettings flush = SyntheticInclinedSoffitSettings ();
	flush.mutules->face = 0.83;
	const std::vector<RingSectionPoint> f = GreekEntablatureSection (flush);
	ASSERT_EQ (f.size (), 19u);
	EXPECT_TRUE (SameBits (f[at + 1].u, 0.48) && SameBits (f[at + 1].z, z[7] + 0.06));
	EXPECT_TRUE (SameBits (f[at + 2].u, 0.83) && SameBits (f[at + 2].z, z[7]));
	EXPECT_TRUE (SameBits (f[at + 3].u, 0.83) && SameBits (f[at + 3].z, z[8]));
	// Triangle retire a la section (sous la sous-face) : 0,5 x (0,78 - 0,48) x 0,06.
	EXPECT_NEAR (0.5 * (TwiceArea (n2) - TwiceArea (s)), 0.009, 1e-12);
	RecordProperty ("section_vertices", static_cast<int> (s.size ()));
}

// MG2 : sous une sous-face inclinee, un mutule est le prisme d'un
// parallelogramme : 8 sommets, 12 faces, ferme ; volume largeur x hauteur x
// profondeur ; dessus au dos a float (zFoot + (z7 + 0,06)), dessous a l'avant a
// float (zFoot + z6), sur chaque cote.
TEST (TEST_cgarchi_classical_greek_entablature, a_mutule_under_an_inclined_soffit_is_a_parallelogram_prism)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	GreekEntablatureSettings e = SyntheticInclinedSoffitSettings ();
	e.mutules->guttae.reset ();
	const OrnamentLayout o = LayoutGreekOrnaments (e, p);
	const std::vector<double> z = GreekEntablatureBandBounds (e);
	const double zFoot = 6.27;
	const std::vector<const OrnamentPiece *> mutules = RankOneMutules (o);
	ASSERT_EQ (mutules.size (), 4u);
	for (const OrnamentPiece *m : mutules)
	{
		SCOPED_TRACE (static_cast<int> (m->side));
		EXPECT_TRUE (SameBits (m->rise, 0.06));
		EXPECT_EQ (m->guttaCount, 0);
		const std::unique_ptr<Mesh> mesh = BuildOrnamentMesh (e, p, o, *m, zFoot);
		ASSERT_EQ (mesh->GetNVertices (), 8u);
		ASSERT_EQ (mesh->GetNFaces (), 12u);
		const MeshValidationReport r = ValidateMesh (*mesh);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.components, 1u);
		EXPECT_NEAR (r.signedVolume, 0.30 * 0.05 * 0.30, r.area * FloatUlp ());
		float q[3];
		mesh->GetVertex (2, q);   // dos, haut, premier bout
		EXPECT_EQ ((std::array<float, 3> {q[0], q[1], q[2]}), FloatPoint (World (p, m->side, m->a0, 0.48), zFoot + (z[7] + 0.06)));
		mesh->GetVertex (1, q);   // face, bas, premier bout
		EXPECT_EQ ((std::array<float, 3> {q[0], q[1], q[2]}), FloatPoint (World (p, m->side, m->a0, 0.78), zFoot + z[6]));
	}

	// Montee nulle : le mutule est, sommets et faces dans l'ordre, le prisme droit
	// de PrismSection, celui d'une regula droite de memes bornes.
	GreekEntablatureSettings flat = SyntheticInclinedSoffitSettings ();
	flat.mutules->rise = 0.0;
	flat.mutules->guttae.reset ();
	const OrnamentLayout of = LayoutGreekOrnaments (flat, p);
	for (const OrnamentPiece *m : RankOneMutules (of))
	{
		SCOPED_TRACE (static_cast<int> (m->side));
		OrnamentPiece straight = *m;
		straight.kind = OrnamentElement::Regula;
		const std::unique_ptr<Mesh> mutule = BuildOrnamentMesh (flat, p, of, *m, zFoot), prism = BuildOrnamentMesh (flat, p, of, straight, zFoot);
		ASSERT_EQ (mutule->GetVertices ().size (), prism->GetVertices ().size ());
		EXPECT_EQ (std::memcmp (mutule->GetVertices ().data (), prism->GetVertices ().data (), prism->GetVertices ().size () * sizeof (float)), 0);
		ASSERT_EQ (mutule->GetNFaces (), prism->GetNFaces ());
		for (unsigned face = 0; face < prism->GetNFaces (); ++face)
			for (unsigned j = 0; j < 3; ++j)
				EXPECT_EQ (mutule->GetFaceVertex (face, j), prism->GetFaceVertex (face, j)) << face << " " << j;
	}
}

// MG3 : 12 gouttes par mutule, en 3 rangs de 4 depuis l'arete avant : axes a
// 0,015, 0,127 et 0,25 le long de la sous-face (avant de la tete avant sur
// l'arete) ; axes extremes a 0,04 des bords, pas 0,22 / 3.
TEST (TEST_cgarchi_classical_greek_entablature, the_mutule_guttae_are_rows_laid_from_the_front_along_the_soffit)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	for (const DoricCornerStrategy strategy : {DoricCornerStrategy::AxisTriglyph, DoricCornerStrategy::CornerTriglyphEqualMetopes})
	{
		SCOPED_TRACE (static_cast<int> (strategy));
		GreekEntablatureSettings e = SyntheticInclinedSoffitSettings ();
		e.corner = strategy;
		const OrnamentLayout o = LayoutGreekOrnaments (e, p);
		const double alongs[] = {0.015, 0.127, 0.25};
		std::size_t next = 0;
		for (std::size_t k = 0; k < o.mutules.size (); ++k)
		{
			const OrnamentPiece &m = o.mutules[k];
			ASSERT_EQ (m.firstGutta, static_cast<int> (next)) << k;
			ASSERT_EQ (m.guttaCount, 12) << k;
			for (int row = 0; row < 3; ++row)
				for (int i = 0; i < 4; ++i)
				{
					const MutuleGuttaPlacement &g = o.mutuleGuttae[next + static_cast<std::size_t> (4 * row + i)];
					EXPECT_EQ (g.side, m.side) << k;
					EXPECT_EQ (g.row, row) << k;
					EXPECT_NEAR (g.along, alongs[row], 1e-12) << k;
					if (i == 0)
						EXPECT_TRUE (SameBits (g.a, m.a0 + 0.04)) << k;
					else
						EXPECT_NEAR (g.a - o.mutuleGuttae[next + static_cast<std::size_t> (4 * row + i - 1)].a, 0.22 / 3.0, 1e-12) << k;
					if (i == 3)
						EXPECT_TRUE (SameBits (g.a, m.a1 - 0.04)) << k;
				}
			// Avant de la tete du rang avant sur l'arete (D3-76).
			EXPECT_NEAR (o.mutuleGuttae[next].along - 0.5 * 0.030, 0.0, 1e-12) << k;
			next += 12;
		}
		EXPECT_EQ (next, o.mutuleGuttae.size ());
		EXPECT_EQ (o.mutuleGuttae.size (), strategy == DoricCornerStrategy::AxisTriglyph ? 816u : 720u);
	}
}

// MG4 : une goutte de mutule est un tronc de cone ferme de 18 sommets et
// 32 faces, d'axe normal a la sous-face (-s, -c) ; tete sur la sous-face, de
// diametre Ø, pied de diametre Ø + 0,004 a 0,02 sous elle ; avant de la tete
// avant sur l'arete avant-basse ; volume du tronc polygonal. Montee nulle :
// tete et pied horizontaux.
TEST (TEST_cgarchi_classical_greek_entablature, a_mutule_gutta_is_a_closed_frustum_normal_to_the_soffit)
{
	const PeristyleLayout p = SyntheticPeristyle ();
	const GreekEntablatureSettings e = SyntheticInclinedSoffitSettings ();
	const OrnamentLayout o = LayoutGreekOrnaments (e, p);
	const std::vector<double> z = GreekEntablatureBandBounds (e);
	const double zFoot = 6.27, depth = 0.30, rise = 0.06, length = std::sqrt (depth * depth + rise * rise);
	const double c = depth / length, s = rise / length, zEdge = zFoot + z[6];
	const double diameters[] = {0.030, 0.034, 0.032};
	const std::vector<const OrnamentPiece *> mutules = RankOneMutules (o);
	ASSERT_EQ (mutules.size (), 4u);
	for (const OrnamentPiece *m : mutules)
	{
		SCOPED_TRACE (static_cast<int> (m->side));
		const std::unique_ptr<Mesh> mesh = BuildOrnamentMesh (e, p, o, *m, zFoot);
		ASSERT_EQ (mesh->GetNVertices (), 8u + 12u * 18u);
		ASSERT_EQ (mesh->GetNFaces (), 12u + 12u * 32u);
		for (unsigned k = 0; k < 12; ++k)
		{
			SCOPED_TRACE (k);
			const double diameter = diameters[k / 4];
			const std::unique_ptr<Mesh> gutta = SubMesh (*mesh, 8 + 18 * k, 18, 12 + 32 * k, 32);
			const MeshValidationReport report = ValidateMesh (*gutta);
			EXPECT_TRUE (report.IsValid ());
			EXPECT_EQ (report.components, 1u);
			float footCentre[3], headCentre[3];
			gutta->GetVertex (16, footCentre);
			gutta->GetVertex (17, headCentre);
			const std::array<double, 2> foot = SideCoordinates (p, m->side, footCentre), head = SideCoordinates (p, m->side, headCentre);
			const double dd = foot[1] - head[1], dz = static_cast<double> (footCentre[2]) - headCentre[2];
			// Axe parallele a (-s, -c), de longueur 0,02.
			EXPECT_NEAR (dd * c - dz * s, 0.0, 1e-6);
			EXPECT_NEAR (dd * -s + dz * -c, 0.02, 1e-6);
			// Sommets de la tete (8 a 15) sur la sous-face : droite par (0,78 ; z6),
			// de normale (s, c).
			for (unsigned i = 8; i < 16; ++i)
			{
				float q[3];
				gutta->GetVertex (i, q);
				const std::array<double, 2> ad = SideCoordinates (p, m->side, q);
				EXPECT_NEAR ((ad[1] - 0.78) * s + (q[2] - zEdge) * c, 0.0, 2e-6) << i;
			}
			float left[3], right[3];
			gutta->GetVertex (8 + 2, left);
			gutta->GetVertex (8 + 6, right);
			EXPECT_NEAR (std::hypot (left[0] - right[0], left[1] - right[1]), diameter, 1e-6);
			gutta->GetVertex (2, left);
			gutta->GetVertex (6, right);
			EXPECT_NEAR (std::hypot (left[0] - right[0], left[1] - right[1]), diameter + 0.004, 1e-6);
			if (k < 4)
			{
				// Avant de la tete du rang avant (sommet 8) sur l'arete avant-basse.
				float q[3];
				gutta->GetVertex (8, q);
				const std::array<double, 2> ad = SideCoordinates (p, m->side, q);
				EXPECT_NEAR (ad[1], 0.78, 1e-6);
				EXPECT_NEAR (q[2], zEdge, 1e-6);
			}
			const double rt = 0.5 * diameter, rb = rt + 0.002;
			const double at = 2.0 * std::sqrt (2.0) * rt * rt, ab = 2.0 * std::sqrt (2.0) * rb * rb;
			const double volume = 0.02 / 3.0 * (ab + at + std::sqrt (ab * at));
			EXPECT_NEAR (report.signedVolume, volume, report.area * FloatUlp ());
		}
	}
	RecordProperty ("mutule_gutta_faces", 32);
	RecordProperty ("mutule_gutta_vertices", 18);

	// Montee nulle : c = 1, s = 0 exactement ; tete et pied horizontaux.
	GreekEntablatureSettings flat = SyntheticInclinedSoffitSettings ();
	flat.mutules->rise = 0.0;
	const OrnamentLayout of = LayoutGreekOrnaments (flat, p);
	const std::unique_ptr<Mesh> mesh = BuildOrnamentMesh (flat, p, of, of.mutules.front (), zFoot);
	ASSERT_EQ (mesh->GetNVertices (), 8u + 12u * 18u);
	for (unsigned k = 0; k < 12; ++k)
		for (unsigned level = 0; level < 2; ++level)
		{
			float first[3];
			mesh->GetVertex (8 + 18 * k + 8 * level, first);
			for (unsigned i = 0; i < 8; ++i)
			{
				float q[3];
				mesh->GetVertex (8 + 18 * k + 8 * level + i, q);
				EXPECT_EQ (q[2], first[2]) << k << " " << level << " " << i;
			}
			EXPECT_EQ (first[2], static_cast<float> (level ? zFoot + z[6] : zFoot + (z[6] - 0.02))) << k;
		}
}
