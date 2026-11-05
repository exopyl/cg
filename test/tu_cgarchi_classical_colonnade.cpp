#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_orders_data.h>
#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_entablature.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/polygon_extrude.h"

// ===========================================================================
//  Colonnade, entablement file et ornements d'entablement
// ===========================================================================

using namespace cgarchi::classical;

namespace {

void ExpectParseErrorNaming (std::string text, const std::string &from, const std::string &to, const std::string &needle)
{
	const std::size_t at = text.find (from);
	ASSERT_NE (at, std::string::npos) << from;
	text.replace (at, from.size (), to);
	try
	{
		ParseClassicalOrders (text);
		FAIL () << "accepte : " << from << " -> " << to;
	}
	catch (const std::runtime_error &e)
	{
		EXPECT_NE (std::string (e.what ()).find (needle), std::string::npos) << e.what ();
	}
}

const OrderData &Get (Order o) { return ClassicalOrders ().Get (o); }

}  // namespace

// O1
TEST (TEST_cgarchi_classical_colonnade, the_axis_spacing_and_the_ornament_rhythms_are_read)
{
	const double expected[kOrderCount] = { 80.0, 90.0, 117.0, 120.0, 120.0 };
	for (int o = 0; o < kOrderCount; ++o)
		EXPECT_DOUBLE_EQ (AxisSpacing (Get (static_cast<Order> (o))), expected[o]) << o;

	const EntablatureOrnaments &doric = Get (Order::Doric).entablatureOrnaments;
	ASSERT_TRUE (doric.triglyph && doric.guttae && doric.dentils && doric.mutules && doric.soffitGuttae);
	const TriglyphData &t = *doric.triglyph;
	EXPECT_EQ (t.width, 12.0);
	EXPECT_EQ (t.height, 18.0);
	EXPECT_EQ (std::accumulate (t.divisionWidths.begin (), t.divisionWidths.end (), 0.0), 12.0);
	ASSERT_EQ (t.divisionWidths.size (), 7u);
	EXPECT_TRUE (t.divisionIsChannel.front ());
	EXPECT_FALSE (t.divisionIsChannel[1]);
	EXPECT_EQ (t.channelAngleDeg, 90.0);
	EXPECT_EQ (t.channelDepth, 1.0);
	EXPECT_EQ (t.halfChannelDepth, 0.5);
	EXPECT_EQ (t.projection, 1.0);
	EXPECT_EQ (AxisSpacing (Get (Order::Doric)) / t.spacing, 3.0);
	// Metope carree : largeur = hauteur de la frise.
	EXPECT_EQ (t.metopeWidth, *Get (Order::Doric).FindBlock ("frise")->totalHeight);

	const GuttaeData &g = *doric.guttae;
	EXPECT_EQ (g.count, 6);
	EXPECT_EQ (g.regulaHeight, 0.5);
	EXPECT_EQ (g.guttaHeight, 1.5);
	EXPECT_EQ (g.diameterBottom, 1.5);
	EXPECT_EQ (g.diameterTop, 0.75);

	const MutuleData &m = *doric.mutules;
	EXPECT_EQ (m.rows, 6);
	EXPECT_EQ (m.perRow, 6);
	EXPECT_EQ (m.guttaPitch, 2.0);
	EXPECT_EQ (m.guttaHeight, 1.5);
	EXPECT_EQ (m.face, 18.5);
	EXPECT_EQ (m.back, 6.5);
	EXPECT_EQ (m.listelHeight, 0.5);

	const SoffitGuttaeData &sg = *doric.soffitGuttae;
	EXPECT_EQ (sg.rows, 3);
	EXPECT_EQ (sg.perRow, 6);
	EXPECT_EQ (sg.pitchAlong, 2.0);
	EXPECT_DOUBLE_EQ (sg.pitchDepth, 2.4);
	EXPECT_EQ (sg.from, 6.0);
	EXPECT_EQ (sg.to, 14.0);

	EXPECT_EQ (doric.dentils->back + doric.dentils->depth, 6.0);   // face = saillie du membre
	const EntablatureOrnaments &ionic = Get (Order::Ionic).entablatureOrnaments;
	ASSERT_TRUE (ionic.dentils);
	EXPECT_EQ (ionic.dentils->back + ionic.dentils->depth, 9.0);
	const EntablatureOrnaments &cor = Get (Order::Corinthian).entablatureOrnaments;
	ASSERT_TRUE (cor.modillions && cor.dentils);
	EXPECT_EQ (AxisSpacing (Get (Order::Corinthian)) / cor.modillions->spacing, 5.0);
	EXPECT_EQ (cor.modillions->spacing, 4.0 * (cor.dentils->width + cor.dentils->gap));
	EXPECT_EQ (cor.dentils->back + cor.dentils->depth, 9.5);
	const EntablatureOrnaments &com = Get (Order::Composite).entablatureOrnaments;
	ASSERT_TRUE (com.blocks);
	EXPECT_EQ (com.blocks->depth, 12.0);
	EXPECT_FALSE (Get (Order::Tuscan).entablatureOrnaments.triglyph.has_value ());
	EXPECT_FALSE (ClassicalOrders ().symmetricEntablatureRule.empty ());
}

TEST (TEST_cgarchi_classical_colonnade, unsupported_or_unlinked_ornament_data_is_refused)
{
	const std::string text (cgarchi::ClassicalOrdersJson ());
	ExpectParseErrorNaming (text, "\"canaux\": \"en V\"", "\"canaux\": \"en U\"", "ornements/triglyphe/canaux");
	// Membre lie renomme : absent.
	ExpectParseErrorNaming (text, "\"nom\": \"B \xC2\xB7 dentello (denticules)\"", "\"nom\": \"B \xC2\xB7 dentello\"",
	                        "membre lie absent");
	// Second membre du meme nom dans le bloc : ambigu.
	ExpectParseErrorNaming (text, "\"nom\": \"talon orn\xC3\xA9 de feuilles\"", "\"nom\": \"blocs \xC3\xA0 pomme de pin\"",
	                        "membre lie ambigu");
	// Cle manquante nommee par son chemin.
	ExpectParseErrorNaming (text, "\"fond_depuis_nu_frise\": \"5\"", "\"fond_depuis_nu_fris\": \"5\"",
	                        "ionique/ornements/denticules/fond_depuis_nu_frise");
	ExpectParseErrorNaming (text, "\"coupe_symetrique\"", "\"coupe_symetriqu\"", "face_arriere_entablement");
	// Angle des canaux pleins incompatible avec largeur 2 et profondeur 1.
	ExpectParseErrorNaming (text, "\"angle\": \"90\xC2\xB0\"", "\"angle\": \"60\xC2\xB0\"", "triglyphe/canaux_detail/angle");
	// Hauteur d'un ornement differente de celle de son membre lie.
	ExpectParseErrorNaming (text, "\"hauteur\": \"8\"", "\"hauteur\": \"7\"", "blocs_composite/hauteur");
	ExpectParseErrorNaming (text, "\"chapiteau_h\": \"2\"", "\"chapiteau_h\": \"3\"", "triglyphe/chapiteau_h");
}

// ===========================================================================
//  O2 : Classical Column inchangee (empreinte portable)
// ===========================================================================

namespace {

struct Fingerprint
{
	int order, cornice, pedestal, entablature, envelope;
	unsigned vertices, faces;
	double volume, lo[3], hi[3], weighted;   // weighted : somme de i |v_i|^2
};

// Empreintes des 48 configurations de Classical Column (parametres par defaut,
// Volutes coche, Flutes decoche), donnees du 2026-11-05.
const Fingerprint kColumnFingerprints[] = {
	{ 0, 0, 0, 0, 0, 3358u, 6712u, 44.244948832821507, -1.375, -1.375, 0, 1.375, 1.375, 14, 767472374.72254503 },
	{ 0, 0, 0, 0, 1, 3358u, 6712u, 44.244948832821507, -1.375, -1.375, 0, 1.375, 1.375, 14, 767472374.72254503 },
	{ 0, 0, 0, 1, 0, 4846u, 9688u, 66.887107408897549, -2.2916667461395264, -2.2916667461395264, 0, 2.2916667461395264, 2.2916667461395264, 17.5, 2479529883.3394346 },
	{ 0, 0, 0, 1, 1, 4846u, 9688u, 66.887107408897549, -2.2916667461395264, -2.2916667461395264, 0, 2.2916667461395264, 2.2916667461395264, 17.5, 2479529883.3394346 },
	{ 0, 0, 1, 0, 0, 4030u, 8056u, 83.177073254301689, -1.7083333730697632, -1.7083333730697632, 0, 1.7083333730697632, 1.7083333730697632, 18.666667938232422, 1899702761.2718143 },
	{ 0, 0, 1, 0, 1, 4030u, 8056u, 83.177073254301689, -1.7083333730697632, -1.7083333730697632, 0, 1.7083333730697632, 1.7083333730697632, 18.666667938232422, 1899702761.2718143 },
	{ 0, 0, 1, 1, 0, 5518u, 11032u, 105.81925742954981, -2.2916667461395264, -2.2916667461395264, 0, 2.2916667461395264, 2.2916667461395264, 22.166667938232422, 5144958256.2253399 },
	{ 0, 0, 1, 1, 1, 5518u, 11032u, 105.81925742954981, -2.2916667461395264, -2.2916667461395264, 0, 2.2916667461395264, 2.2916667461395264, 22.166667938232422, 5144958256.2253399 },
	{ 1, 0, 0, 0, 0, 4034u, 8064u, 51.295813704420709, -1.4166667461395264, -1.4166667461395264, 0, 1.4166667461395264, 1.4166667461395264, 16, 1505407625.7421517 },
	{ 1, 0, 0, 0, 1, 4034u, 8064u, 51.295813704420709, -1.4166667461395264, -1.4166667461395264, 0, 1.4166667461395264, 1.4166667461395264, 16, 1505407625.7421517 },
	{ 1, 0, 0, 1, 0, 5858u, 11712u, 83.308997172237142, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 20, 4824528752.6474524 },
	{ 1, 0, 0, 1, 1, 5858u, 11712u, 83.308997172237142, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 20, 4824528752.6474524 },
	{ 1, 0, 1, 0, 0, 5954u, 11904u, 99.181317011220699, -1.9166667461395264, -1.9166667461395264, 0, 1.9166667461395264, 1.9166667461395264, 21.333333969116211, 4843182920.4364548 },
	{ 1, 0, 1, 0, 1, 5954u, 11904u, 99.181317011220699, -1.9166667461395264, -1.9166667461395264, 0, 1.9166667461395264, 1.9166667461395264, 21.333333969116211, 4843182920.4364548 },
	{ 1, 0, 1, 1, 0, 7778u, 15552u, 131.19453804672193, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 25.333333969116211, 12332034428.982773 },
	{ 1, 0, 1, 1, 1, 7778u, 15552u, 131.19453804672193, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 25.333333969116211, 12332034428.982773 },
	{ 1, 1, 0, 0, 0, 4034u, 8064u, 51.295813704420709, -1.4166667461395264, -1.4166667461395264, 0, 1.4166667461395264, 1.4166667461395264, 16, 1505407625.7421517 },
	{ 1, 1, 0, 0, 1, 4034u, 8064u, 51.295813704420709, -1.4166667461395264, -1.4166667461395264, 0, 1.4166667461395264, 1.4166667461395264, 16, 1505407625.7421517 },
	{ 1, 1, 0, 1, 0, 6098u, 12192u, 88.456887364318789, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 20, 5333701325.9157085 },
	{ 1, 1, 0, 1, 1, 6098u, 12192u, 88.456887364318789, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 20, 5333701325.9157085 },
	{ 1, 1, 1, 0, 0, 5954u, 11904u, 99.181317011220699, -1.9166667461395264, -1.9166667461395264, 0, 1.9166667461395264, 1.9166667461395264, 21.333333969116211, 4843182920.4364548 },
	{ 1, 1, 1, 0, 1, 5954u, 11904u, 99.181317011220699, -1.9166667461395264, -1.9166667461395264, 0, 1.9166667461395264, 1.9166667461395264, 21.333333969116211, 4843182920.4364548 },
	{ 1, 1, 1, 1, 0, 8018u, 16032u, 136.34242790485172, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 25.333333969116211, 13432731433.249727 },
	{ 1, 1, 1, 1, 1, 8018u, 16032u, 136.34242790485172, -2.8333334922790527, -2.8333334922790527, 0, 2.8333334922790527, 2.8333334922790527, 25.333333969116211, 13432731433.249727 },
	{ 2, 0, 0, 0, 0, 8546u, 17080u, 62.906927780283361, -1.4444444179534912, -1.388888955116272, 0, 1.4444444179534912, 1.388888955116272, 18, 8501877662.7372169 },
	{ 2, 0, 0, 0, 1, 8546u, 17080u, 62.906927780283361, -1.4444444179534912, -1.388888955116272, 0, 1.4444444179534912, 1.388888955116272, 18, 8501877662.7372169 },
	{ 2, 0, 0, 1, 0, 11514u, 23016u, 95.01594493503714, -2.5555555820465088, -2.5555555820465088, 0, 2.5555555820465088, 2.5555555820465088, 22.5, 20870956800.907051 },
	{ 2, 0, 0, 1, 1, 11514u, 23016u, 95.01594493503714, -2.5555555820465088, -2.5555555820465088, 0, 2.5555555820465088, 2.5555555820465088, 22.5, 20870956800.907051 },
	{ 2, 0, 1, 0, 0, 10898u, 21784u, 113.71726220929371, -1.9444444179534912, -1.9444444179534912, 0, 1.9444444179534912, 1.9444444179534912, 24, 22208442009.203705 },
	{ 2, 0, 1, 0, 1, 10898u, 21784u, 113.71726220929371, -1.9444444179534912, -1.9444444179534912, 0, 1.9444444179534912, 1.9444444179534912, 24, 22208442009.203705 },
	{ 2, 0, 1, 1, 0, 13866u, 27720u, 145.82627936404808, -2.5555555820465088, -2.5555555820465088, 0, 2.5555555820465088, 2.5555555820465088, 28.5, 47952698878.377922 },
	{ 2, 0, 1, 1, 1, 13866u, 27720u, 145.82627936404808, -2.5555555820465088, -2.5555555820465088, 0, 2.5555555820465088, 2.5555555820465088, 28.5, 47952698878.377922 },
	{ 3, 0, 0, 0, 0, 6826u, 13648u, 65.726641230866946, -1.4166666269302368, -1.4166666269302368, 0, 1.4166666269302368, 1.4166666269302368, 20, 4366828820.8944664 },
	{ 3, 0, 0, 0, 1, 5858u, 11712u, 73.982768757025966, -1.8565891981124878, -1.8565891981124878, 0, 1.8565891981124878, 1.8565891981124878, 20, 3226631245.1525583 },
	{ 3, 0, 0, 1, 0, 11194u, 22384u, 114.15936942449437, -2.9444444179534912, -2.9444444179534912, 0, 2.9444444179534912, 2.9444444179534912, 25, 25643942454.692272 },
	{ 3, 0, 0, 1, 1, 9602u, 19200u, 122.41549695065137, -2.9444444179534912, -2.9444444179534912, 0, 2.9444444179534912, 2.9444444179534912, 25, 18873328810.081703 },
	{ 3, 0, 1, 0, 0, 10186u, 20368u, 124.01872843166292, -1.8333333730697632, -1.8333333730697632, 0, 1.8333333730697632, 1.8333333730697632, 27, 15320860688.618883 },
	{ 3, 0, 1, 0, 1, 8738u, 17472u, 132.27485595818146, -1.8565891981124878, -1.8565891981124878, 0, 1.8565891981124878, 1.8565891981124878, 27, 11303496077.574646 },
	{ 3, 0, 1, 1, 0, 14554u, 29104u, 172.45145432906205, -2.9444444179534912, -2.9444444179534912, 0, 2.9444444179534912, 2.9444444179534912, 32, 64460530703.456543 },
	{ 3, 0, 1, 1, 1, 12482u, 24960u, 180.70758185558026, -2.9444444179534912, -2.9444444179534912, 0, 2.9444444179534912, 2.9444444179534912, 32, 47430600595.757309 },
	{ 4, 0, 0, 0, 0, 7162u, 14320u, 65.916156135073592, -1.4142135381698608, -1.4142135381698608, 0, 1.4142135381698608, 1.4142135381698608, 20, 5846154185.107132 },
	{ 4, 0, 0, 0, 1, 7162u, 14320u, 70.199100365376907, -1.5697674751281738, -1.5697674751281738, 0, 1.5697674751281738, 1.5697674751281738, 20, 5845182706.4663162 },
	{ 4, 0, 0, 1, 0, 11922u, 23840u, 108.90524209271206, -2.8333332538604736, -2.8333332538604736, 0, 2.8333332538604736, 2.8333332538604736, 25, 30426671308.233345 },
	{ 4, 0, 0, 1, 1, 11922u, 23840u, 113.18818632301515, -2.8333332538604736, -2.8333332538604736, 0, 2.8333332538604736, 2.8333332538604736, 25, 30425699829.592529 },
	{ 4, 0, 1, 0, 0, 10634u, 21264u, 124.31164089729847, -1.8333333730697632, -1.8333333730697632, 0, 1.8333333730697632, 1.8333333730697632, 27, 19711695982.579254 },
	{ 4, 0, 1, 0, 1, 10634u, 21264u, 128.59458512760193, -1.8333333730697632, -1.8333333730697632, 0, 1.8333333730697632, 1.8333333730697632, 27, 19708551007.935642 },
	{ 4, 0, 1, 1, 0, 15394u, 30784u, 167.30072685493809, -2.8333332538604736, -2.8333332538604736, 0, 2.8333332538604736, 2.8333332538604736, 32, 76114741812.975342 },
	{ 4, 0, 1, 1, 1, 15394u, 30784u, 171.58367108524158, -2.8333332538604736, -2.8333332538604736, 0, 2.8333332538604736, 2.8333332538604736, 32, 76111596838.331741 },
};

Fingerprint Measure (int order, int cornice, int pedestal, int entablature, int envelope, int curveSegments = 6)
{
	cgarchi::ParameterizedClassicalColumn c;
	std::vector<Parameter> ps = c.GetParameters ();
	for (Parameter &p : ps)
	{
		if (p.GetName () == "Order") p.SetInt (order);
		if (p.GetName () == "Doric cornice") p.SetInt (cornice);
		if (p.GetName () == "Pedestal") p.SetBool (pedestal != 0);
		if (p.GetName () == "Entablature") p.SetBool (entablature != 0);
		if (p.GetName () == "Ornament envelope") p.SetBool (envelope != 0);
		if (p.GetName () == "Curve segments") p.SetInt (curveSegments);
	}
	c.Regenerate ();
	const Mesh &m = *c.GetMesh ();
	Fingerprint f {order, cornice, pedestal, entablature, envelope, m.GetNVertices (), m.GetNFaces (), 0.0,
	               {1e300, 1e300, 1e300}, {-1e300, -1e300, -1e300}, 0.0};
	for (unsigned i = 0; i < m.GetNVertices (); ++i)
	{
		float v[3];
		m.GetVertex (i, v);
		f.weighted += static_cast<double> (i) * (static_cast<double> (v[0]) * v[0] + static_cast<double> (v[1]) * v[1] +
		                                         static_cast<double> (v[2]) * v[2]);
		for (int k = 0; k < 3; ++k)
		{
			f.lo[k] = std::min (f.lo[k], static_cast<double> (v[k]));
			f.hi[k] = std::max (f.hi[k], static_cast<double> (v[k]));
		}
	}
	for (unsigned t = 0; t < m.GetNFaces (); ++t)
	{
		float a[3], b[3], c2[3];
		m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (t, 0)), a);
		m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (t, 1)), b);
		m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (t, 2)), c2);
		f.volume += (static_cast<double> (a[0]) * (static_cast<double> (b[1]) * c2[2] - static_cast<double> (b[2]) * c2[1]) -
		             static_cast<double> (a[1]) * (static_cast<double> (b[0]) * c2[2] - static_cast<double> (b[2]) * c2[0]) +
		             static_cast<double> (a[2]) * (static_cast<double> (b[0]) * c2[1] - static_cast<double> (b[1]) * c2[0])) /
		            6.0;
	}
	return f;
}

bool Matches (const Fingerprint &a, const Fingerprint &b, std::string &why)
{
	auto rel = [] (double x, double y) { return std::fabs (x - y) <= 1e-9 * std::max (1.0, std::fabs (y)); };
	if (a.vertices != b.vertices || a.faces != b.faces)
		why = "comptes";
	else if (!rel (a.volume, b.volume))
		why = "volume";
	else if (!rel (a.weighted, b.weighted))
		why = "somme ponderee";
	for (int k = 0; k < 3 && why.empty (); ++k)
		if (!rel (a.lo[k], b.lo[k]) || !rel (a.hi[k], b.hi[k]))
			why = "boite";
	return why.empty ();
}

}  // namespace

TEST (TEST_cgarchi_classical_colonnade, the_classical_column_keeps_its_fingerprint_in_all_48_configurations)
{
	ASSERT_EQ (sizeof (kColumnFingerprints) / sizeof (kColumnFingerprints[0]), 48u);
	for (const Fingerprint &ref : kColumnFingerprints)
	{
		const Fingerprint now = Measure (ref.order, ref.cornice, ref.pedestal, ref.entablature, ref.envelope);
		std::string why;
		EXPECT_TRUE (Matches (now, ref, why)) << ref.order << ref.cornice << ref.pedestal << ref.entablature << ref.envelope
		                                      << " : " << why;
	}
	// Negatif : un segment de courbe de plus change l'empreinte.
	const Fingerprint &first = kColumnFingerprints[0];
	std::string why;
	EXPECT_FALSE (Matches (Measure (first.order, first.cornice, first.pedestal, first.entablature, first.envelope, 7), first, why));
}

// ===========================================================================
//  O3 : entablement file
// ===========================================================================

namespace {

std::string DefectList (const MeshValidationReport &r)
{
	std::string out;
	for (std::size_t k = 0; k < kMeshDefectCount; ++k)
		if (r.counts[k])
			out += std::string (" ") + MeshDefectName (static_cast<MeshDefect> (k)) + "=" + std::to_string (r.counts[k]);
	if (r.selfIntersectionTruncated)
		out += " truncated";
	return out;
}

}  // namespace

TEST (TEST_cgarchi_classical_colonnade, the_swept_entablature_is_a_closed_solid_of_the_exact_volume)
{
	for (int o = 0; o < kOrderCount; ++o)
		for (int cornice = 0; cornice < (o == 1 ? 2 : 1); ++cornice)
			for (int ornaments = 0; ornaments < 2; ++ornaments)
				for (int n : { 1, 2, 4 })
				{
					const OrderData &order = Get (static_cast<Order> (o));
					EntablatureOptions options;
					options.doricCornice = cornice ? DoricCornice::Mutules : DoricCornice::Dentils;
					options.ornaments = ornaments != 0;
					const ColumnProfile section = BuildEntablatureSection (order, options);
					const double L = 0.5 * (n - 1) * AxisSpacing (order);
					const std::unique_ptr<Mesh> mesh = SweepEntablature (section, L, 1.0);
					MeshValidationOptions one;
					one.expectedComponents = 1u;
					const MeshValidationReport r = ValidateMesh (*mesh, one);
					const std::string what = order.key + " " + std::to_string (cornice) + std::to_string (ornaments) +
					                         " N=" + std::to_string (n);
					EXPECT_TRUE (r.IsValid ()) << what << " :" << DefectList (r);
					// Sommets stockes en float : 1e-6 relatif.
					const double exact = SweptVolume (section, L);
					EXPECT_NEAR (r.signedVolume, exact, 1e-6 * exact) << what;
					// Hauteur et longueur hors tout.
					EXPECT_NEAR (r.bbMax[2] - r.bbMin[2], order.general.entablature_M * order.partsPerModule, 1e-4) << what;
					const Block *cor = order.FindBlock (o == 1 ? (cornice ? "corniche_mutules" : "corniche_denticules") : "corniche");
					const double overall = (n - 1) * AxisSpacing (order) + 2.0 * (cor->axisDistance + *cor->totalProjection);
					EXPECT_NEAR (r.bbMax[0] - r.bbMin[0], overall, 1e-4) << what;
					if (n == 1 && !ornaments)
					{
						// N = 1 : l'entablement en ressaut carre du tour.
						LatheOptions lathe;
						const MeshValidationReport l = ValidateMesh (*LatheColumnProfile (section, lathe));
						EXPECT_NEAR (l.signedVolume, exact, 1e-6 * exact) << what;
					}
				}
}

TEST (TEST_cgarchi_classical_colonnade, the_entablature_checks_can_fail)
{
	const OrderData &order = Get (Order::Ionic);
	const ColumnProfile section = BuildEntablatureSection (order, EntablatureOptions {});
	const double L = 1.5 * AxisSpacing (order);
	// Sans faces de fermeture : aretes de bord.
	const MeshValidationReport open = ValidateMesh (*SweepEntablature (section, L, 1.0, 0.0, false));
	EXPECT_GT (open.Count (MeshDefect::BoundaryEdge), 0u);
	// Volume calcule pour une autre longueur : divergent.
	const MeshValidationReport closed = ValidateMesh (*SweepEntablature (section, L, 1.0));
	EXPECT_GT (std::fabs (closed.signedVolume - SweptVolume (section, L + 1.0)), 1e-3 * closed.signedVolume);
	// Ornements : la bande des denticules recule a leur fond (5 du nu de la frise).
	EntablatureOptions withOrnaments;
	withOrnaments.ornaments = true;
	EXPECT_LT (SweptVolume (BuildEntablatureSection (order, withOrnaments), L), SweptVolume (section, L));
}

// ===========================================================================
//  O4 : colonnade sans ornements
// ===========================================================================

namespace {

std::unique_ptr<cgarchi::ParameterizedClassicalColonnade> MakeColonnade (int order, int columns, bool ornaments,
                                                                         bool pedestal = false, int cornice = 0)
{
	auto c = std::make_unique<cgarchi::ParameterizedClassicalColonnade> ();
	std::vector<Parameter> ps = c->GetParameters ();
	for (Parameter &p : ps)
	{
		if (p.GetName () == "Order") p.SetInt (order);
		if (p.GetName () == "Columns") p.SetInt (columns);
		if (p.GetName () == "Ornaments") p.SetBool (ornaments);
		if (p.GetName () == "Pedestal") p.SetBool (pedestal);
		if (p.GetName () == "Doric cornice") p.SetInt (cornice);
	}
	c->Regenerate ();
	return c;
}

}  // namespace

TEST (TEST_cgarchi_classical_colonnade, a_plain_colonnade_is_n_translated_columns_under_the_swept_entablature)
{
	for (int o = 0; o < kOrderCount; ++o)
		for (int pedestal = 0; pedestal < 2; ++pedestal)
		{
			const int n = 3;
			const auto colonnade = MakeColonnade (o, n, false, pedestal != 0);
			ASSERT_TRUE (colonnade->LastError ().empty ()) << colonnade->LastError ();
			const OrderData &order = Get (static_cast<Order> (o));
			const double scale = 1.0 / order.partsPerModule;

			cgarchi::ClassicalColumnSettings s;
			s.order = o;
			s.pedestal = pedestal != 0;
			ColumnProfile profile;
			const std::unique_ptr<Mesh> column = cgarchi::BuildClassicalColumnMesh (s, &profile);
			const ColumnProfile section = BuildEntablatureSection (order, EntablatureOptions {});
			const ColonnadeLayout layout = LayoutColonnade (order, n);
			const std::unique_ptr<Mesh> ent = SweepEntablature (section, layout.halfLength, scale, profile.height);
			const Mesh &m = *colonnade->GetMesh ();
			ASSERT_EQ (m.GetNFaces (), n * column->GetNFaces () + ent->GetNFaces ()) << o << pedestal;

			// Chaque copie : la colonne translatee de son axe.
			double worst = 0.0;
			for (int i = 0; i < n; ++i)
				for (unsigned v = 0; v < column->GetNVertices (); ++v)
				{
					float a[3], b[3];
					m.GetVertex (static_cast<unsigned> (i) * column->GetNVertices () + v, a);
					column->GetVertex (v, b);
					worst = std::max ({worst, std::fabs (static_cast<double> (a[0]) - b[0] - layout.axes[i] * scale),
					                   std::fabs (static_cast<double> (a[1]) - b[1]),
					                   std::fabs (static_cast<double> (a[2]) - b[2])});
				}
			EXPECT_LT (worst, 1e-6) << o << pedestal;
			// Pied de l'entablement = sommet du chapiteau (piedestal compris).
			double low = 1e30;
			for (unsigned v = 0; v < ent->GetNVertices (); ++v)
			{
				float p[3];
				ent->GetVertex (v, p);
				low = std::min (low, static_cast<double> (p[2]));
			}
			const MeshValidationReport cr = ValidateMesh (*column);
			EXPECT_NEAR (low, cr.bbMax[2], 1e-5) << o << pedestal;
			EXPECT_NEAR (cr.bbMax[2] - cr.bbMin[2], order.general.column_M + (pedestal ? order.general.pedestal_M : 0.0), 1e-4)
				<< o << pedestal;
			if (o == 0 && !pedestal)
				std::cout << "[ mesure ] entablement toscan balaye : " << ent->GetNFaces () << " triangles, "
				          << section.vertices.size () << " sommets de profil\n";
		}
}

// ===========================================================================
//  O5 : placement des ornements
// ===========================================================================

namespace {

std::vector<OrnamentPlacement> Placements (Order o, int n, DoricCornice cornice, const ColonnadeLayout *custom = nullptr)
{
	const OrderData &order = Get (o);
	EntablatureOptions options;
	options.doricCornice = cornice;
	options.ornaments = true;
	const ColumnProfile section = BuildEntablatureSection (order, options);
	const ColonnadeLayout layout = custom ? *custom : LayoutColonnade (order, n);
	return PlaceEntablatureOrnaments (order, layout, section, cornice);
}

std::size_t Count (const std::vector<OrnamentPlacement> &p, OrnamentKind kind)
{
	return static_cast<std::size_t> (
		std::count_if (p.begin (), p.end (), [&] (const OrnamentPlacement &q) { return q.kind == kind; }));
}

bool OnEveryAxis (const std::vector<OrnamentPlacement> &p, OrnamentKind kind, const ColonnadeLayout &layout)
{
	for (ColonnadeFace face : { ColonnadeFace::Front, ColonnadeFace::Back })
		for (double axis : layout.axes)
			if (std::none_of (p.begin (), p.end (), [&] (const OrnamentPlacement &q) {
				    return q.kind == kind && q.face == face && std::fabs (q.centre - axis) < 1e-9;
			    }))
				return false;
	return true;
}

// Emprise au sol (x, y) d'un element.
struct Footprint
{
	double x0, x1, y0, y1;
};

Footprint FootprintOf (const OrnamentPlacement &q, const ColonnadeLayout &layout)
{
	switch (q.face)
	{
	case ColonnadeFace::Front: return {q.a0, q.a1, -q.d1, -q.d0};
	case ColonnadeFace::Back:  return {q.a0, q.a1, q.d0, q.d1};
	case ColonnadeFace::Left:  return {layout.axes.front () - q.d1, layout.axes.front () - q.d0, q.a0, q.a1};
	case ColonnadeFace::Right: return {layout.axes.back () + q.d0, layout.axes.back () + q.d1, q.a0, q.a1};
	}
	return {};
}

bool AnyOverlap (const std::vector<OrnamentPlacement> &p, OrnamentKind kind, const ColonnadeLayout &layout)
{
	std::vector<Footprint> f;
	for (const OrnamentPlacement &q : p)
		if (q.kind == kind)
			f.push_back (FootprintOf (q, layout));
	for (std::size_t i = 0; i < f.size (); ++i)
		for (std::size_t j = i + 1; j < f.size (); ++j)
			if (f[i].x0 < f[j].x1 - 1e-9 && f[j].x0 < f[i].x1 - 1e-9 && f[i].y0 < f[j].y1 - 1e-9 && f[j].y0 < f[i].y1 - 1e-9)
				return true;
	return false;
}

}  // namespace

TEST (TEST_cgarchi_classical_colonnade, the_ornaments_fall_on_the_axes_with_the_predicted_counts)
{
	const int n = 4;
	const auto dd = Placements (Order::Doric, n, DoricCornice::Dentils);
	const auto dm = Placements (Order::Doric, n, DoricCornice::Mutules);
	const auto io = Placements (Order::Ionic, n, DoricCornice::Dentils);
	const auto co = Placements (Order::Corinthian, n, DoricCornice::Dentils);
	const auto cp = Placements (Order::Composite, n, DoricCornice::Dentils);
	EXPECT_EQ (Count (dd, OrnamentKind::Triglyph), 22u);
	EXPECT_EQ (Count (dd, OrnamentKind::Regula), 22u);
	EXPECT_EQ (Count (dd, OrnamentKind::Gutta), 132u);
	EXPECT_EQ (Count (dd, OrnamentKind::Dentil), 216u);
	EXPECT_EQ (Count (dd, OrnamentKind::SoffitGutta), 396u);
	EXPECT_EQ (Count (dd, OrnamentKind::Mutule), 0u);
	EXPECT_EQ (Count (dm, OrnamentKind::Mutule), 22u);
	EXPECT_EQ (Count (dm, OrnamentKind::MutuleGutta), 792u);
	EXPECT_EQ (Count (dm, OrnamentKind::MutuleListel), 22u);
	EXPECT_EQ (Count (dm, OrnamentKind::Dentil), 0u);
	EXPECT_EQ (Count (io, OrnamentKind::Dentil), 144u);
	EXPECT_EQ (Count (co, OrnamentKind::Dentil), 148u);
	EXPECT_EQ (Count (co, OrnamentKind::Modillion), 42u);
	EXPECT_EQ (Count (cp, OrnamentKind::CompositeBlock), 92u);
	EXPECT_TRUE (Placements (Order::Tuscan, n, DoricCornice::Dentils).empty ());

	const ColonnadeLayout ld = LayoutColonnade (Get (Order::Doric), n);
	const ColonnadeLayout lc = LayoutColonnade (Get (Order::Corinthian), n);
	EXPECT_TRUE (OnEveryAxis (dd, OrnamentKind::Triglyph, ld));
	EXPECT_TRUE (OnEveryAxis (dm, OrnamentKind::Mutule, ld));
	EXPECT_TRUE (OnEveryAxis (co, OrnamentKind::Modillion, lc));
	EXPECT_TRUE (OnEveryAxis (co, OrnamentKind::Dentil, lc));   // un denticule sous chaque modillon
	// Entre deux triglyphes consecutifs : une metope de 18 = hauteur de la frise.
	std::vector<double> front;
	for (const OrnamentPlacement &q : dd)
		if (q.kind == OrnamentKind::Triglyph && q.face == ColonnadeFace::Front)
			front.push_back (q.centre);
	std::sort (front.begin (), front.end ());
	for (std::size_t i = 1; i < front.size (); ++i)
		EXPECT_NEAR (front[i] - front[i - 1] - 12.0, 18.0, 1e-12);
	// Ionique et composite : symetriques par rapport a X = 0.
	for (const std::vector<OrnamentPlacement> *p : { &io, &cp })
	{
		double sum = 0.0;
		for (const OrnamentPlacement &q : *p)
			if (q.face == ColonnadeFace::Front)
				sum += q.centre;
		EXPECT_NEAR (sum, 0.0, 1e-9);
	}
	// Aucun recouvrement, angles compris.
	for (OrnamentKind k : { OrnamentKind::Triglyph, OrnamentKind::Dentil, OrnamentKind::Mutule })
	{
		EXPECT_FALSE (AnyOverlap (dd, k, ld));
		EXPECT_FALSE (AnyOverlap (dm, k, ld));
	}
	EXPECT_FALSE (AnyOverlap (io, OrnamentKind::Dentil, LayoutColonnade (Get (Order::Ionic), n)));
	EXPECT_FALSE (AnyOverlap (co, OrnamentKind::Modillion, lc));
	std::cout << "[ mesure ] N = 4 : triglyphes " << Count (dd, OrnamentKind::Triglyph) << ", gouttes "
	          << Count (dd, OrnamentKind::Gutta) << ", gouttes du plafond " << Count (dd, OrnamentKind::SoffitGutta)
	          << ", gouttes des mutules " << Count (dm, OrnamentKind::MutuleGutta) << '\n';
}

TEST (TEST_cgarchi_classical_colonnade, the_placement_checks_can_fail)
{
	// Entraxe pris pour l'entrecolonnement seul (66 au dorique) : axes manques.
	ColonnadeLayout wrong = LayoutColonnade (Get (Order::Doric), 4);
	for (std::size_t i = 0; i < wrong.axes.size (); ++i)
		wrong.axes[i] = (static_cast<double> (i) - 1.5) * 66.0;
	const auto p = Placements (Order::Doric, 4, DoricCornice::Dentils, &wrong);
	EXPECT_FALSE (OnEveryAxis (p, OrnamentKind::Triglyph, wrong));
	// Regle d'angle retiree : un denticule de facade pousse jusqu'au plan de
	// face du retour recouvre le premier denticule du retour.
	const ColonnadeLayout l = LayoutColonnade (Get (Order::Doric), 4);
	std::vector<OrnamentPlacement> dentils;
	for (const OrnamentPlacement &q : Placements (Order::Doric, 4, DoricCornice::Dentils))
		if (q.kind == OrnamentKind::Dentil)
			dentils.push_back (q);
	EXPECT_FALSE (AnyOverlap (dentils, OrnamentKind::Dentil, l));
	const OrnamentPlacement &ret = *std::find_if (dentils.begin (), dentils.end (),
	                                              [] (const OrnamentPlacement &q) { return q.face == ColonnadeFace::Left; });
	OrnamentPlacement corner = dentils.front ();
	corner.face = ColonnadeFace::Front;
	corner.a1 = l.axes.front () - ret.d0;
	corner.a0 = corner.a1 - (ret.d1 - ret.d0);
	corner.d0 = -ret.a1;
	corner.d1 = -ret.a0;
	dentils.push_back (corner);
	EXPECT_TRUE (AnyOverlap (dentils, OrnamentKind::Dentil, l));
}

// ===========================================================================
//  O6 : coques des ornements
// ===========================================================================

TEST (TEST_cgarchi_classical_colonnade, each_ornament_family_is_a_set_of_closed_disjoint_shells_in_its_band)
{
	struct Case
	{
		Order order;
		DoricCornice cornice;
	};
	for (const Case c : { Case {Order::Doric, DoricCornice::Dentils}, Case {Order::Doric, DoricCornice::Mutules},
	                      Case {Order::Ionic, DoricCornice::Dentils}, Case {Order::Corinthian, DoricCornice::Dentils},
	                      Case {Order::Composite, DoricCornice::Dentils} })
	{
		const OrderData &order = Get (c.order);
		EntablatureOptions options;
		options.doricCornice = c.cornice;
		options.ornaments = true;
		const ColumnProfile section = BuildEntablatureSection (order, options);
		const ColonnadeLayout layout = LayoutColonnade (order, 2);
		const auto placements = PlaceEntablatureOrnaments (order, layout, section, c.cornice);
		// Chaque element dans la hauteur de l'entablement, d'emprise non vide.
		for (const OrnamentPlacement &q : placements)
		{
			EXPECT_GE (q.z0, -1e-9);
			EXPECT_LE (q.z1, section.height + 1e-9);
			EXPECT_LT (q.d0, q.d1);
			EXPECT_LT (q.a0, q.a1);
		}
		std::size_t faces = 0;
		for (const auto &family : BuildOrnamentMeshes (order, layout, placements, 0.0, 1.0, 12))
		{
			const std::size_t count = Count (placements, family.first);
			MeshValidationOptions v;
			v.expectedComponents = static_cast<unsigned> (count);
			const MeshValidationReport r = ValidateMesh (*family.second, v);
			EXPECT_TRUE (r.IsValid ()) << order.key << " " << OrnamentKindName (family.first) << " :" << DefectList (r);
			EXPECT_EQ (r.components, count) << OrnamentKindName (family.first);
			EXPECT_GT (r.signedVolume, 0.0);
			faces += family.second->GetNFaces ();
		}
		std::cout << "[ mesure ] ornements " << order.key << (c.cornice == DoricCornice::Mutules ? " (mutules)" : "")
		          << " N = 2 : " << faces << " triangles\n";
	}
}

TEST (TEST_cgarchi_classical_colonnade, a_regula_merged_with_its_guttae_reports_the_contact)
{
	const OrderData &order = Get (Order::Doric);
	EntablatureOptions options;
	options.ornaments = true;
	const ColumnProfile section = BuildEntablatureSection (order, options);
	const ColonnadeLayout layout = LayoutColonnade (order, 1);
	const auto placements = PlaceEntablatureOrnaments (order, layout, section, DoricCornice::Dentils);
	std::vector<OrnamentPlacement> merged;
	for (const OrnamentPlacement &q : placements)
		if (q.kind == OrnamentKind::Regula || q.kind == OrnamentKind::Gutta)
			merged.push_back (q);
	auto families = BuildOrnamentMeshes (order, layout, merged, 0.0, 1.0, 12);
	ASSERT_EQ (families.size (), 2u);
	Mesh one (*families[0].second);
	appendMesh (one, *families[1].second);
	const MeshValidationReport r = ValidateMesh (one);
	EXPECT_FALSE (r.IsValid ());
	EXPECT_GT (r.Count (MeshDefect::SelfIntersection) + r.Count (MeshDefect::NonManifoldEdge) +
	               r.Count (MeshDefect::NonManifoldVertex),
	           0u)
		<< DefectList (r);
}

TEST (TEST_cgarchi_classical_colonnade, the_cost_is_measured_and_large_meshes_are_flagged)
{
	struct Case { const char *label; int order, columns, cornice; bool flutes; };
	const Case cases[] = { {"defaut (toscan, 4)", 0, 4, 0, false},
	                       {"dorique a mutules, 4", 1, 4, 1, false},
	                       {"corinthien, 4", 3, 4, 0, false},
	                       {"ionique cannele a volutes, 6", 2, 6, 0, true},
	                       {"ionique cannele a volutes, 12", 2, 12, 0, true} };
	for (const Case &c : cases)
	{
		auto col = std::make_unique<cgarchi::ParameterizedClassicalColonnade> ();
		std::vector<Parameter> ps = col->GetParameters ();
		for (Parameter &p : ps)
		{
			if (p.GetName () == "Order") p.SetInt (c.order);
			if (p.GetName () == "Columns") p.SetInt (c.columns);
			if (p.GetName () == "Doric cornice") p.SetInt (c.cornice);
			if (p.GetName () == "Flutes") p.SetBool (c.flutes);
		}
		const auto t0 = std::chrono::steady_clock::now ();
		col->Regenerate ();
		const auto t1 = std::chrono::steady_clock::now ();
		ASSERT_TRUE (col->LastError ().empty ()) << c.label << " : " << col->LastError ();
		const unsigned faces = col->GetMesh ()->GetNFaces ();
		EXPECT_EQ (col->Warnings ().empty (), faces <= cgarchi::ParameterizedClassicalColonnade::kColonnadeFaceWarning) << c.label;
		if (c.order == 1)
		{
			std::vector<Parameter> off = col->GetParameters ();
			for (Parameter &p : off)
				if (p.GetName () == "Ornaments")
					p.SetBool (false);
			col->Regenerate ();
			std::cout << "[ mesure ] ornements de la colonnade " << c.label << " : " << faces - col->GetMesh ()->GetNFaces ()
			          << " triangles\n";
		}
		std::cout << "[ mesure ] colonnade " << c.label << " : " << faces << " triangles, Regenerate "
		          << std::chrono::duration<double, std::milli> (t1 - t0).count () << " ms (Debug)"
		          << (col->Warnings ().empty () ? "" : " ; avertissement") << '\n';
	}
}

// ===========================================================================
//  O6 : position des ornements contre l'entablement balaye
// ===========================================================================

namespace {

// Saillie de l'entablement a la hauteur z (interieur d'un segment du profil).
double SectionDepth (const ColumnProfile &section, double z)
{
	double best = -1.0;
	for (std::size_t i = 1; i < section.vertices.size (); ++i)
	{
		const double a = section.vertices[i - 1].y, b = section.vertices[i].y;
		if (b - a < 1e-12 || z <= a || z >= b)
			continue;
		const double t = (z - a) / (b - a);
		best = std::max (best, section.vertices[i - 1].ring.radius + t * (section.vertices[i].ring.radius - section.vertices[i - 1].ring.radius));
	}
	return best;
}

bool InSweptSolid (const ColumnProfile &section, double L, double x, double y, double z)
{
	const double d = SectionDepth (section, z);
	return d > 0.0 && std::fabs (x) < L + d && std::fabs (y) < d;
}

struct WorldBox
{
	double x0, x1, y0, y1, z0, z1;
};

WorldBox BoxOf (const OrnamentPlacement &q, const ColonnadeLayout &l)
{
	const Footprint f = FootprintOf (q, l);
	return {f.x0, f.x1, f.y0, f.y1, q.z0, q.z1};
}

bool IsGutta (OrnamentKind k)
{
	return k == OrnamentKind::Gutta || k == OrnamentKind::MutuleGutta || k == OrnamentKind::SoffitGutta;
}

struct PositionReport
{
	std::size_t interiorInSolid = 0;   // elements dont un point interieur est dans l'entablement
	std::size_t unsupported = 0;       // elements sans appui au-dessus
};

// (a) aucun point interieur d'un element dans le solide balaye ; (b) appui :
// un point juste au-dessus du centre est dans le solide pour les elements
// pendus a l'entablement ; une goutte pend d'un hote (regula, listel de
// mutule) dont le dessous est a son sommet, ou de la sous-face du larmier.
PositionReport CheckPositions (const std::vector<OrnamentPlacement> &placements, const ColumnProfile &section,
                               const ColonnadeLayout &layout)
{
	PositionReport r;
	const double L = layout.halfLength;
	for (const OrnamentPlacement &q : placements)
	{
		const WorldBox b = BoxOf (q, layout);
		const double cx = 0.5 * (b.x0 + b.x1), cy = 0.5 * (b.y0 + b.y1);
		bool inside = false;
		const int S = 5;
		for (int i = 0; i < S && !inside; ++i)
			for (int j = 0; j < S && !inside; ++j)
				for (int m = 0; m < S && !inside; ++m)
				{
					const double x = b.x0 + (b.x1 - b.x0) * (i + 0.5) / S, y = b.y0 + (b.y1 - b.y0) * (j + 0.5) / S;
					const double z = b.z0 + (b.z1 - b.z0) * (m + 0.5) / S;
					if (IsGutta (q.kind) && std::hypot (x - cx, y - cy) > q.radiusTop)
						continue;
					inside = InSweptSolid (section, L, x, y, z);
				}
		r.interiorInSolid += inside ? 1 : 0;

		bool supported = InSweptSolid (section, L, cx, cy, q.z1 + 1e-6);
		if (!supported && IsGutta (q.kind))
			for (const OrnamentPlacement &h : placements)
				if ((h.kind == OrnamentKind::Regula || h.kind == OrnamentKind::MutuleListel) && h.face == q.face &&
				    std::fabs (h.z0 - q.z1) < 1e-9)
				{
					const WorldBox hb = BoxOf (h, layout);
					supported = supported || (cx > hb.x0 && cx < hb.x1 && cy > hb.y0 && cy < hb.y1);
				}
		if (!supported && q.kind == OrnamentKind::MutuleListel)
			for (const OrnamentPlacement &h : placements)
				if (h.kind == OrnamentKind::Mutule && h.face == q.face && std::fabs (h.z0 - q.z1) < 1e-9 &&
				    std::fabs (h.centre - q.centre) < 1e-9)
					supported = true;
		r.unsupported += supported ? 0 : 1;
	}
	return r;
}

}  // namespace

TEST (TEST_cgarchi_classical_colonnade, every_ornament_sits_against_the_entablature_without_entering_it)
{
	struct Case
	{
		Order order;
		DoricCornice cornice;
	};
	for (const Case c : { Case {Order::Doric, DoricCornice::Dentils}, Case {Order::Doric, DoricCornice::Mutules},
	                      Case {Order::Ionic, DoricCornice::Dentils}, Case {Order::Corinthian, DoricCornice::Dentils},
	                      Case {Order::Composite, DoricCornice::Dentils} })
	{
		const OrderData &order = Get (c.order);
		EntablatureOptions options;
		options.doricCornice = c.cornice;
		options.ornaments = true;
		const ColumnProfile section = BuildEntablatureSection (order, options);
		const ColonnadeLayout layout = LayoutColonnade (order, 2);
		const auto placements = PlaceEntablatureOrnaments (order, layout, section, c.cornice);
		ASSERT_FALSE (placements.empty ());
		std::set<std::pair<int, int>> seen;   // famille x face couvertes
		for (const OrnamentPlacement &q : placements)
			seen.insert ({static_cast<int> (q.kind), static_cast<int> (q.face)});
		const PositionReport r = CheckPositions (placements, section, layout);
		EXPECT_EQ (r.interiorInSolid, 0u) << order.key;
		EXPECT_EQ (r.unsupported, 0u) << order.key;
		// Les quatre faces portent chaque famille.
		std::set<int> kinds;
		for (const OrnamentPlacement &q : placements)
			kinds.insert (static_cast<int> (q.kind));
		for (int k : kinds)
			for (int f = 0; f < 4; ++f)
				EXPECT_TRUE (seen.count ({k, f})) << order.key << " " << OrnamentKindName (static_cast<OrnamentKind> (k)) << " face " << f;
	}
}

TEST (TEST_cgarchi_classical_colonnade, a_mutule_moved_up_is_caught_by_the_position_check)
{
	const OrderData &order = Get (Order::Doric);
	EntablatureOptions options;
	options.doricCornice = DoricCornice::Mutules;
	options.ornaments = true;
	const ColumnProfile section = BuildEntablatureSection (order, options);
	const ColonnadeLayout layout = LayoutColonnade (order, 2);
	auto placements = PlaceEntablatureOrnaments (order, layout, section, DoricCornice::Mutules);
	for (OrnamentPlacement &q : placements)
		if (q.kind == OrnamentKind::Mutule)
		{
			q.z0 += 3.0;
			q.z1 += 3.0;
		}
	const PositionReport r = CheckPositions (placements, section, layout);
	EXPECT_GT (r.interiorInSolid + r.unsupported, 0u);
}
