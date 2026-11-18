#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/classical_greek_cella.h>
#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Cella et porches de l'Hephaisteion (lot C1, registre CE-1 a CE-9)
// ===========================================================================
//
// Donnee : le document embarque du temple d'Hephaistos (README section 7.8),
// converti par MonumentCellaSettings et MonumentPorchSettings, construit avec
// le temple exterieur (MonumentTempleSettings). Toutes les mesures sont lues
// sur le maillage, par piece (adresse TempleAddress).
//
// Tolerances :
//   - kFloatTolerance (1e-6 m) : cote d'un sommet float a ~16 m de l'origine
//     (demi-ulp 9,5e-7) contre la valeur calculee en double ;
//   - kPrintedMillimetre (1e-3 m) : dessus de la frise des porches (7,357,
//     somme des hauteurs) contre le dessus de l'assise XVI (7,358), README
//     section 7.8 ;
//   - kControlTolerance (1e-9 m) : controle du README recalcule depuis les
//     longueurs lues, sommes de nombres decimaux a quatre chiffres.

using namespace cgarchi::classical;
using nlohmann::json;

namespace {

constexpr double kFloatTolerance = 1e-6;
constexpr double kPrintedMillimetre = 1e-3;
constexpr double kControlTolerance = 1e-9;

struct CellaTemple
{
	TempleSettings settings;
	TempleLayout layout;
	std::vector<TempleMeshPart> parts;
	std::unique_ptr<Mesh> mesh;
};

// Temple du README avec sa cella et ses porches, construit une fois.
const CellaTemple &ReadmeTemple ()
{
	static const CellaTemple temple = [] {
		CellaTemple t;
		const MonumentData &d = HephaistosMonument();
		t.settings = MonumentTempleSettings(d);
		t.settings.cella = MonumentCellaSettings(d);
		t.settings.cella->porches = MonumentPorchSettings(d);
		t.mesh = BuildTempleMesh(t.settings, &t.layout, &t.parts);
		return t;
	}();
	return temple;
}

bool Is (const TempleAddress &a, TempleElement element, PeristyleSide side, int index)
{
	return a.element == element && a.side == side && a.index == index;
}

const TempleMeshPart &PartOf (const CellaTemple &t, TempleElement element, PeristyleSide side, int index = 0)
{
	for (const TempleMeshPart &p : t.parts)
		if (Is(p.address, element, side, index))
			return p;
	throw std::logic_error("partie absente");
}

struct Box3
{
	std::array<float, 3> lo {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
	                         std::numeric_limits<float>::max()};
	std::array<float, 3> hi {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
	                         std::numeric_limits<float>::lowest()};
};

Box3 BoxOf (const Mesh &m, const TempleMeshPart &p)
{
	Box3 b;
	for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		for (int k = 0; k < 3; ++k)
		{
			b.lo[k] = std::min(b.lo[k], q[k]);
			b.hi[k] = std::max(b.hi[k], q[k]);
		}
	}
	return b;
}

bool IsCellaElement (TempleElement e)
{
	return e == TempleElement::Toichobate || e == TempleElement::NaosFloor || e == TempleElement::LongWall ||
	       e == TempleElement::CrossWall || e == TempleElement::Threshold || e == TempleElement::Anta ||
	       e == TempleElement::PorchArchitrave || e == TempleElement::PorchFrieze || e == TempleElement::PorchColumn;
}

// Repere du README vers le repere du temple (MO2) : X = y, Y = -x.
double TempleY (double readmeX)
{
	return -readmeX;
}

} // namespace

// CE1 : parties et pieces. Hephaistos : cella et porches construits, 15 pieces de maconnerie puis 4 colonnes, apres
// le comble ; Ares et Parthenon : cella et porches absents.
TEST(TEST_cgarchi_classical_cella, the_cella_and_porches_are_built_for_hephaistos_and_absent_elsewhere)
{
	const MonumentBuild heph = BuildMonumentMesh(HephaistosMonument());
	EXPECT_EQ(heph.report[static_cast<std::size_t>(MonumentPart::Cella)].state, MonumentPartState::Built);
	EXPECT_EQ(heph.report[static_cast<std::size_t>(MonumentPart::Porches)].state, MonumentPartState::Built);
	for (const char *name : {"temple-ares", "temple-parthenon"})
	{
		SCOPED_TRACE(name);
		const MonumentBuild b = BuildMonumentMesh(EmbeddedMonument(name));
		EXPECT_EQ(b.report[static_cast<std::size_t>(MonumentPart::Cella)].state, MonumentPartState::Absent);
		EXPECT_EQ(b.report[static_cast<std::size_t>(MonumentPart::Porches)].state, MonumentPartState::Absent);
	}

	const CellaTemple &t = ReadmeTemple();
	std::size_t roof = t.parts.size();
	for (std::size_t k = 0; k < t.parts.size(); ++k)
		if (t.parts[k].address.element == TempleElement::Roof)
			roof = k;
	ASSERT_EQ(t.parts.size(), roof + 1 + 15 + 4);
	using E = TempleElement;
	using S = PeristyleSide;
	const std::vector<TempleAddress> expected = {
		{E::Toichobate, S::Front, 0}, {E::NaosFloor, S::Front, 0},       {E::LongWall, S::Left, 0},
		{E::LongWall, S::Right, 0},   {E::CrossWall, S::Front, 0},       {E::CrossWall, S::Back, 0},
		{E::Threshold, S::Front, 0},  {E::Anta, S::Front, 0},            {E::Anta, S::Front, 1},
		{E::Anta, S::Back, 0},        {E::Anta, S::Back, 1},             {E::PorchArchitrave, S::Front, 0},
		{E::PorchFrieze, S::Front, 0}, {E::PorchArchitrave, S::Back, 0}, {E::PorchFrieze, S::Back, 0},
		{E::PorchColumn, S::Front, 0}, {E::PorchColumn, S::Front, 1},    {E::PorchColumn, S::Back, 0},
		{E::PorchColumn, S::Back, 1}};
	for (std::size_t k = 0; k < expected.size(); ++k)
	{
		SCOPED_TRACE(k);
		const TempleAddress &a = t.parts[roof + 1 + k].address;
		EXPECT_TRUE(Is(a, expected[k].element, expected[k].side, expected[k].index));
	}
	ASSERT_TRUE(t.layout.cella);
	EXPECT_EQ(t.layout.cella->porchColumns.size(), 4u);
}

// CE2 : controles du README (section 7.8) recalcules depuis les longueurs lues, puis lus sur le maillage : faces du
// toichobate, nus des murs longs, des murs de refend et des antes, au repere du temple.
TEST(TEST_cgarchi_classical_cella, the_faces_of_the_mesh_are_the_signed_controls_of_the_readme)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentCellaControls &c = d.cellaControls;
	const MonumentCella &v = d.cella;
	const auto L = [](const MonumentValue &x) { return MonumentLength(x, "cella"); };
	const double S = 0.5 * MonumentLength(d.krepis.stylobateLength, "krepis.stylobateLength");
	// Recalcul des controles, en metres, depuis les valeurs.
	EXPECT_NEAR(S - L(v.pteronEast), c.toichobateEastX, kControlTolerance);
	EXPECT_NEAR(L(v.pteronWest) - S, c.toichobateWestX, kControlTolerance);
	EXPECT_NEAR(L(v.longWallSpan) / 2.0, c.longWallInnerY, kControlTolerance);
	EXPECT_NEAR(L(v.longWallSpan) / 2.0 + L(v.longWallThickness), c.longWallOuterY, kControlTolerance);
	EXPECT_NEAR(2.0 * (c.longWallOuterY + L(v.toichobateOverhang)), c.toichobateWidth, kControlTolerance);
	EXPECT_NEAR((c.toichobateEastX - L(v.eastAntaRetreat)) - (c.toichobateWestX + L(v.westAntaRetreat)), c.longWallLength,
	            kControlTolerance);
	EXPECT_NEAR(c.toichobateEastX - L(v.eastCrossWallPosition), c.eastCrossWallEastX, kControlTolerance);
	EXPECT_NEAR(c.eastCrossWallEastX - L(v.eastCrossWallThickness), c.eastCrossWallWestX, kControlTolerance);
	EXPECT_NEAR(c.toichobateWestX + L(v.westCrossWallPosition), c.westCrossWallWestX, kControlTolerance);
	EXPECT_NEAR(c.westCrossWallWestX + L(v.westCrossWallThickness), c.westCrossWallEastX, kControlTolerance);
	EXPECT_NEAR(c.eastCrossWallWestX - c.westCrossWallEastX, c.naosLength, kControlTolerance);
	EXPECT_NEAR(c.toichobateEastX - L(v.eastAntaRetreat), c.eastAntaX, kControlTolerance);
	EXPECT_NEAR(c.toichobateWestX + L(v.westAntaRetreat), c.westAntaX, kControlTolerance);
	// Repere des antes (registre CE-1) : debord interieur compte sur le nu du mur construit ; largeur de face [S].
	EXPECT_NEAR(c.longWallInnerY - L(v.antaInnerProjection), c.antaInnerY, kControlTolerance);
	EXPECT_NEAR(L(v.antaInnerProjection) + L(v.longWallThickness) + L(v.antaOuterProjection), c.antaFaceWidth,
	            kControlTolerance);
	EXPECT_NEAR(L(v.naosFloorHeight) + L(v.lintelHeight), c.doorLintelZ, kControlTolerance);
	EXPECT_NEAR(L(v.toichobateRise) + L(v.porchColumnHeight), c.porchArchitraveBottomZ, kControlTolerance);
	EXPECT_NEAR(c.porchArchitraveBottomZ + L(v.porchArchitraveHeight), c.porchArchitraveTopZ, kControlTolerance);
	EXPECT_NEAR(c.porchArchitraveTopZ + L(v.porchFriezeHeight), c.porchFriezeTopZ, kControlTolerance);
	// Dessous du toichobate : partie visible moins la hauteur de l'assise I (0,358, README section 7.8, ligne
	// « Toichobate : hauteur », valeur non portee au document : le dessous n'est pas construit, CE-3).
	EXPECT_NEAR(L(v.toichobateRise) - 0.358, c.toichobateBottomZ, kControlTolerance);

	// Au maillage, repere du temple : X = y, Y = -x, Z = z + stylobateTop.
	const CellaTemple &t = ReadmeTemple();
	const Mesh &m = *t.mesh;
	const double top = t.layout.krepis.stylobateTop;
	const Box3 toichobate = BoxOf(m, PartOf(t, TempleElement::Toichobate, PeristyleSide::Front));
	EXPECT_NEAR(toichobate.lo[1], TempleY(c.toichobateEastX), kFloatTolerance);
	EXPECT_NEAR(toichobate.hi[1], TempleY(c.toichobateWestX), kFloatTolerance);
	EXPECT_NEAR(toichobate.hi[0] - toichobate.lo[0], c.toichobateWidth, kFloatTolerance);
	const Box3 right = BoxOf(m, PartOf(t, TempleElement::LongWall, PeristyleSide::Right));
	const Box3 left = BoxOf(m, PartOf(t, TempleElement::LongWall, PeristyleSide::Left));
	EXPECT_NEAR(right.lo[0], c.longWallInnerY, kFloatTolerance);
	EXPECT_NEAR(right.hi[0], c.longWallOuterY, kFloatTolerance);
	EXPECT_NEAR(left.hi[0], -c.longWallInnerY, kFloatTolerance);
	EXPECT_NEAR(left.lo[0], -c.longWallOuterY, kFloatTolerance);
	const Box3 front = BoxOf(m, PartOf(t, TempleElement::CrossWall, PeristyleSide::Front));
	const Box3 back = BoxOf(m, PartOf(t, TempleElement::CrossWall, PeristyleSide::Back));
	EXPECT_NEAR(front.lo[1], TempleY(c.eastCrossWallEastX), kFloatTolerance);
	EXPECT_NEAR(front.hi[1], TempleY(c.eastCrossWallWestX), kFloatTolerance);
	EXPECT_NEAR(back.lo[1], TempleY(c.westCrossWallEastX), kFloatTolerance);
	EXPECT_NEAR(back.hi[1], TempleY(c.westCrossWallWestX), kFloatTolerance);
	EXPECT_NEAR(back.lo[1] - front.hi[1], c.naosLength, kFloatTolerance);
	const Box3 frontAnta = BoxOf(m, PartOf(t, TempleElement::Anta, PeristyleSide::Front, 1));
	const Box3 backAnta = BoxOf(m, PartOf(t, TempleElement::Anta, PeristyleSide::Back, 0));
	EXPECT_NEAR(frontAnta.lo[1], TempleY(c.eastAntaX), kFloatTolerance);
	EXPECT_NEAR(backAnta.hi[1], TempleY(c.westAntaX), kFloatTolerance);
	EXPECT_NEAR(backAnta.hi[1] - frontAnta.lo[1], c.longWallLength, kFloatTolerance);
	EXPECT_NEAR(frontAnta.lo[0], c.antaInnerY, kFloatTolerance);
	EXPECT_NEAR(backAnta.hi[0], -c.antaInnerY, kFloatTolerance);
	EXPECT_NEAR(frontAnta.hi[0] - frontAnta.lo[0], c.antaFaceWidth, kFloatTolerance);
	// Dessous du toichobate enfoui : non construit (CE-3) ; le toichobate part du dessus du stylobate.
	EXPECT_EQ(toichobate.lo[2], static_cast<float>(top));
	RecordProperty("naos_length_m", std::to_string(back.lo[1] - front.hi[1]));
}

// CE3 : aucun sommet de la cella ni des porches au-dessus de float (7,694) (z11, dessus de l'anneau, sous le comble) ;
// pieds du toichobate a float (dessus du stylobate), pieds des murs, des antes, du seuil, du sol du naos et des colonnes
// des porches a float (dessus du toichobate), au bit pres.
TEST(TEST_cgarchi_classical_cella, nothing_of_the_cella_rises_above_z11_and_every_foot_rests_on_its_course)
{
	const CellaTemple &t = ReadmeTemple();
	const Mesh &m = *t.mesh;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const float z11 = static_cast<float>(stylobateTop + 7.694);
	const float ringTop =
		static_cast<float>(t.layout.entablatureFoot + GreekEntablatureBandBounds(*t.settings.entablature).back());
	EXPECT_NEAR(static_cast<double>(ringTop) - stylobateTop, 7.694, 5e-4);
	float highest = std::numeric_limits<float>::lowest();
	std::size_t vertices = 0;
	for (const TempleMeshPart &p : t.parts)
	{
		if (!IsCellaElement(p.address.element))
			continue;
		const Box3 b = BoxOf(m, p);
		highest = std::max(highest, b.hi[2]);
		vertices += p.vertexCount;
	}
	EXPECT_GT(vertices, 0u);
	EXPECT_LT(highest, z11);
	EXPECT_LT(highest, ringTop);
	RecordProperty("cella_top_m", std::to_string(static_cast<double>(highest) - stylobateTop));

	const float stylobate = static_cast<float>(stylobateTop);
	const float toichobate = static_cast<float>(t.layout.cella->toichobateTop);
	EXPECT_NEAR(static_cast<double>(toichobate) - stylobateTop, 0.098, kFloatTolerance);
	// Le dessus du stylobate est le dessus du degre 0 de la krepis.
	EXPECT_EQ(BoxOf(m, PartOf(t, TempleElement::KrepisStep, PeristyleSide::Front, 0)).hi[2], stylobate);
	EXPECT_EQ(BoxOf(m, PartOf(t, TempleElement::Toichobate, PeristyleSide::Front)).lo[2], stylobate);
	EXPECT_EQ(BoxOf(m, PartOf(t, TempleElement::Toichobate, PeristyleSide::Front)).hi[2], toichobate);
	for (const TempleMeshPart &p : t.parts)
	{
		const TempleElement e = p.address.element;
		if (e == TempleElement::LongWall || e == TempleElement::CrossWall || e == TempleElement::Anta ||
		    e == TempleElement::Threshold || e == TempleElement::NaosFloor || e == TempleElement::PorchColumn)
		{
			SCOPED_TRACE(static_cast<int>(e));
			EXPECT_EQ(BoxOf(m, p).lo[2], toichobate);
		}
	}
}

// CE4 : porte et seuil relevés au maillage. Largeur de la porte 2,705 et dessous du linteau 5,710 au-dessus du
// stylobate (5,410 au-dessus du sol du naos), a 1e-6 ; porte centree sur l'axe (convention, README section 7.8) ;
// seuil arase au sol du naos (CE-3), de 0,369 en avant du mur de refend a son nu arriere.
TEST(TEST_cgarchi_classical_cella, the_door_and_its_threshold_are_measured_on_the_mesh)
{
	const MonumentData &d = HephaistosMonument();
	const CellaTemple &t = ReadmeTemple();
	const Mesh &m = *t.mesh;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const TempleMeshPart &wall = PartOf(t, TempleElement::CrossWall, PeristyleSide::Front);
	// Le linteau : cote la plus haute sous le dessus du mur ; la porte, les deux X a cette cote.
	const Box3 box = BoxOf(m, wall);
	float lintel = std::numeric_limits<float>::lowest();
	for (unsigned v = wall.firstVertex; v < wall.firstVertex + wall.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		if (q[2] < box.hi[2])
			lintel = std::max(lintel, q[2]);
	}
	std::set<float> jambs;
	for (unsigned v = wall.firstVertex; v < wall.firstVertex + wall.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		if (q[2] == lintel)
			jambs.insert(q[0]);
	}
	ASSERT_EQ(jambs.size(), 2u);
	const double width = static_cast<double>(*jambs.rbegin()) - static_cast<double>(*jambs.begin());
	const double lintelZ = static_cast<double>(lintel) - stylobateTop;
	EXPECT_NEAR(width, MonumentLength(d.cella.doorWidth, "cella.doorWidth"), kFloatTolerance);
	EXPECT_NEAR(width, 2.705, kFloatTolerance);
	EXPECT_NEAR(lintelZ, d.cellaControls.doorLintelZ, kFloatTolerance);
	EXPECT_NEAR(lintelZ, 5.710, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(*jambs.rbegin()) + static_cast<double>(*jambs.begin()), 0.0, kFloatTolerance);
	RecordProperty("door_width_m", std::to_string(width));
	RecordProperty("lintel_z_m", std::to_string(lintelZ));

	const Box3 sill = BoxOf(m, PartOf(t, TempleElement::Threshold, PeristyleSide::Front));
	EXPECT_NEAR(static_cast<double>(sill.hi[0]) - static_cast<double>(sill.lo[0]), 3.810, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(sill.hi[2]) - stylobateTop, 0.300, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(box.lo[1]) - static_cast<double>(sill.lo[1]), 0.369, kFloatTolerance);
	EXPECT_EQ(sill.hi[1], box.hi[1]);
}

// CE5 : colonnes des porches. Axes (+-1,247 ; x = 10,26 et -11,05 au README) et hauteur 5,611 au maillage, a 1e-6 ;
// sommet de l'abaque a la cote du dessous de l'architrave des porches, au bit pres ; dessus de la frise a 7,357, a
// 1 mm du dessus des murs (7,358).
TEST(TEST_cgarchi_classical_cella, the_porch_columns_stand_on_their_axes_under_the_porch_architrave)
{
	const CellaTemple &t = ReadmeTemple();
	const Mesh &m = *t.mesh;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const std::array<std::array<double, 2>, 4> axes = {{{-1.247, -10.26}, {1.247, -10.26}, {-1.247, 11.05}, {1.247, 11.05}}};
	const std::array<std::pair<PeristyleSide, int>, 4> addresses = {
		{{PeristyleSide::Front, 0}, {PeristyleSide::Front, 1}, {PeristyleSide::Back, 0}, {PeristyleSide::Back, 1}}};
	const float foot = static_cast<float>(t.layout.cella->porchFoot);
	for (std::size_t k = 0; k < 4; ++k)
	{
		SCOPED_TRACE(k);
		const TempleMeshPart &p = PartOf(t, TempleElement::PorchColumn, addresses[k].first, addresses[k].second);
		const Box3 b = BoxOf(m, p);
		// Le carre de l'abaque est centre sur l'axe.
		EXPECT_NEAR(0.5 * (static_cast<double>(b.lo[0]) + static_cast<double>(b.hi[0])), axes[k][0], kFloatTolerance);
		EXPECT_NEAR(0.5 * (static_cast<double>(b.lo[1]) + static_cast<double>(b.hi[1])), axes[k][1], kFloatTolerance);
		EXPECT_NEAR(static_cast<double>(b.hi[2]) - static_cast<double>(b.lo[2]), 5.611, kFloatTolerance);
		EXPECT_EQ(b.hi[2], foot);
	}
	for (const PeristyleSide side : {PeristyleSide::Front, PeristyleSide::Back})
	{
		SCOPED_TRACE(static_cast<int>(side));
		const Box3 architrave = BoxOf(m, PartOf(t, TempleElement::PorchArchitrave, side));
		const Box3 frieze = BoxOf(m, PartOf(t, TempleElement::PorchFrieze, side));
		EXPECT_EQ(architrave.lo[2], foot);
		EXPECT_EQ(frieze.lo[2], architrave.hi[2]);
		EXPECT_NEAR(static_cast<double>(architrave.lo[2]) - stylobateTop, 5.709, kFloatTolerance);
		EXPECT_NEAR(static_cast<double>(architrave.hi[2]) - stylobateTop, 6.549, kFloatTolerance);
		const double friezeTop = static_cast<double>(frieze.hi[2]) - stylobateTop;
		EXPECT_NEAR(friezeTop, 7.357, kFloatTolerance);
		EXPECT_NEAR(friezeTop, 7.358, kPrintedMillimetre + kFloatTolerance);
		EXPECT_NEAR(friezeTop, HephaistosMonument().cellaControls.porchFriezeTopZ, kFloatTolerance);
		// Section de 0,999 centree sur la ligne des axes (convention, README section 7.8).
		const double axis = side == PeristyleSide::Front ? -10.26 : 11.05;
		EXPECT_NEAR(0.5 * (static_cast<double>(architrave.lo[1]) + static_cast<double>(architrave.hi[1])), axis,
		            kFloatTolerance);
		EXPECT_NEAR(static_cast<double>(architrave.hi[1]) - static_cast<double>(architrave.lo[1]), 0.999, kFloatTolerance);
		RecordProperty(side == PeristyleSide::Front ? "front_frieze_top_m" : "back_frieze_top_m", std::to_string(friezeTop));
	}
	// Colonne des porches (CE-6) : celle du peristyle, D et H changes, entasis a l'echelle du fut.
	const GreekPorchColumnsSettings porch = MonumentPorchSettings(HephaistosMonument());
	const GreekDoricColumnSettings peristyle = MonumentColumnSettings(HephaistosMonument(), false);
	EXPECT_EQ(porch.column.lowerDiameter, 1.006);
	EXPECT_EQ(porch.column.height, 5.611);
	EXPECT_EQ(porch.column.upperDiameter, peristyle.upperDiameter);
	EXPECT_EQ(porch.column.fluteCount, peristyle.fluteCount);
	EXPECT_EQ(porch.column.fluteDepthRatio, peristyle.fluteDepthRatio);
	EXPECT_EQ(porch.column.abacusProjection, peristyle.abacusProjection);
	EXPECT_NEAR(porch.column.entasisHeight / peristyle.entasisHeight, porch.column.entasisSagitta / peristyle.entasisSagitta,
	            1e-15);
	EXPECT_LT(porch.column.entasisHeight, peristyle.entasisHeight);
}

// CE6 : une cle de la cella faussee est refusee et nommee : manquante (rapport Incomplete, conversion refusee),
// d'unite fausse (lecteur), ou de valeur qui casse la cella (porte plus large que le naos, seuil plus profond que le
// mur : mise en place refusee).
TEST(TEST_cgarchi_classical_cella, a_wrong_cella_key_is_refused_by_name)
{
	const MonumentData &heph = HephaistosMonument();
	const auto refusalOf = [](const MonumentData &d) {
		try
		{
			BuildMonumentMesh(d);
		}
		catch (const std::invalid_argument &e)
		{
			return std::string(e.what());
		}
		return std::string();
	};
	MonumentData wide = heph;
	wide.cella.doorWidth.value = 6.5;
	EXPECT_NE(refusalOf(wide).find("'cella.doorWidth'"), std::string::npos) << refusalOf(wide);
	MonumentData deep = heph;
	deep.cella.thresholdDepth.value = 0.9;
	EXPECT_NE(refusalOf(deep).find("'cella.thresholdDepth'"), std::string::npos) << refusalOf(deep);
	MonumentData high = heph;
	high.cella.wallTop.value = 8.0;
	EXPECT_NE(refusalOf(high).find("'cella'"), std::string::npos) << refusalOf(high);
	EXPECT_NE(refusalOf(high).find("CE-4"), std::string::npos) << refusalOf(high);
	EXPECT_EQ(refusalOf(heph), "");

	MonumentData missing = heph;
	missing.cella.lintelHeight.value.reset();
	missing.cella.lintelHeight.status = MonumentStatus::Missing;
	missing.cella.lintelHeight.sources.clear();
	EXPECT_EQ(MissingMonumentKeys(missing, MonumentPart::Cella), std::vector<std::string>{"cella.lintelHeight"});
	try
	{
		MonumentCellaSettings(missing);
		ADD_FAILURE() << "conversion acceptee";
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE(std::string(e.what()).find("'cella.lintelHeight'"), std::string::npos) << e.what();
	}

	const std::string_view text = cgarchi::TempleHephaistosJson();
	json doc = json::parse(text.begin(), text.end());
	doc["cella"]["wallTop"]["unit"] = "1";
	try
	{
		ParseMonument(doc.dump());
		ADD_FAILURE() << "document accepte";
	}
	catch (const std::runtime_error &e)
	{
		EXPECT_NE(std::string(e.what()).find("cella.wallTop.unit"), std::string::npos) << e.what();
	}
}

// CE7 (D8) : la cella et le raffinement ne se combinent pas ; le refus nomme la cause, avant tout refus du raffinement.
TEST(TEST_cgarchi_classical_cella, the_cella_refuses_the_refinement_and_names_the_cause)
{
	TempleSettings s = ReadmeTemple().settings;
	s.refinement = TempleRefinementSettings {};
	try
	{
		LayoutTemple(s);
		ADD_FAILURE() << "cella et raffinement acceptes";
	}
	catch (const std::invalid_argument &e)
	{
		const std::string what = e.what();
		EXPECT_EQ(what.rfind("Temple : 'cella' et 'refinement'", 0), 0u) << what;
		EXPECT_NE(what.find("D8"), std::string::npos) << what;
		EXPECT_NE(what.find("CE-9"), std::string::npos) << what;
	}
	EXPECT_THROW(BuildTempleMesh(s), std::invalid_argument);
	s.cella.reset();
	try
	{
		LayoutTemple(s);
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_EQ(std::string(e.what()).find("'cella'"), std::string::npos) << e.what();
	}
}

// CE8 : liste fermee des recouvrements. Sous-assemblage : stylobate (degre 0), anneau, comble, les 15 pieces de la
// cella et les 4 colonnes des porches, valide a la tolerance par defaut. Chaque recouvrement est une paire de faces
// coplanaires dans un meme plan d'axe, de deux pieces d'une paire listee (C1 a C8, et L16 entre l'anneau et le
// comble) ; chaque contact liste est present ; aucun recouvrement hors liste.
TEST(TEST_cgarchi_classical_cella, the_only_overlaps_of_the_cella_are_its_listed_contacts)
{
	const CellaTemple &t = ReadmeTemple();
	const Mesh &m = *t.mesh;
	std::vector<std::size_t> chosen;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
	{
		const TempleAddress &a = t.parts[k].address;
		if ((a.element == TempleElement::KrepisStep && a.index == 0) || a.element == TempleElement::Entablature ||
		    a.element == TempleElement::Roof || IsCellaElement(a.element))
			chosen.push_back(k);
	}
	ASSERT_EQ(chosen.size(), 3u + 15u + 4u);
	std::vector<float> vertices;
	std::vector<unsigned> faces;
	std::vector<TempleElement> owner;
	for (const std::size_t k : chosen)
	{
		const TempleMeshPart &part = t.parts[k];
		const unsigned base = static_cast<unsigned>(vertices.size() / 3);
		vertices.insert(vertices.end(), m.GetVertices().begin() + 3 * part.firstVertex,
		                m.GetVertices().begin() + 3 * (part.firstVertex + part.vertexCount));
		for (unsigned f = part.firstFace; f < part.firstFace + part.faceCount; ++f)
		{
			for (unsigned j = 0; j < 3; ++j)
				faces.push_back(base + static_cast<unsigned>(m.GetFaceVertex(f, j)) - part.firstVertex);
			owner.push_back(part.address.element);
		}
	}
	Mesh sub;
	sub.SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
	sub.SetFaces(static_cast<unsigned>(faces.size() / 3), 3, faces.data());
	MeshValidationOptions options;
	options.maxRecordsPerKind = sub.GetNFaces();
	const MeshValidationReport r = ValidateMesh(sub, options);
	ASSERT_TRUE(r.selfIntersectionChecked);
	ASSERT_FALSE(r.selfIntersectionTruncated);

	// Les six sommets des deux faces dans un meme plan d'axe.
	const auto coplanarAxis = [&](unsigned f0, unsigned f1) {
		for (int axis = 0; axis < 3; ++axis)
		{
			float q[3];
			sub.GetVertex(static_cast<unsigned>(sub.GetFaceVertex(f0, 0)), q);
			const float value = q[axis];
			bool same = true;
			for (const unsigned f : {f0, f1})
				for (unsigned j = 0; j < 3; ++j)
				{
					sub.GetVertex(static_cast<unsigned>(sub.GetFaceVertex(f, j)), q);
					same = same && q[axis] == value;
				}
			if (same)
				return axis;
		}
		return -1;
	};
	using E = TempleElement;
	// Contacts listes, par paire d'elements (ordre de l'enumeration) et axe du plan commun.
	const std::map<std::tuple<E, E, int>, const char *> listed = {
		{{E::KrepisStep, E::Toichobate, 2}, "C1"},
		{{E::Toichobate, E::NaosFloor, 2}, "C2"},
		{{E::Toichobate, E::LongWall, 2}, "C2"},
		{{E::Toichobate, E::CrossWall, 2}, "C2"},
		{{E::Toichobate, E::Threshold, 2}, "C2"},
		{{E::Toichobate, E::Anta, 2}, "C2"},
		{{E::Toichobate, E::PorchColumn, 2}, "C2"},
		{{E::NaosFloor, E::LongWall, 0}, "C3"},
		{{E::NaosFloor, E::CrossWall, 1}, "C3"},
		{{E::NaosFloor, E::Threshold, 1}, "C3"},
		{{E::LongWall, E::CrossWall, 0}, "C4"},
		{{E::CrossWall, E::Threshold, 0}, "C5"},
		{{E::CrossWall, E::Threshold, 2}, "C5"},
		{{E::LongWall, E::Anta, 1}, "C6"},
		{{E::LongWall, E::PorchArchitrave, 2}, "C7"},
		{{E::LongWall, E::PorchArchitrave, 1}, "C7"},
		{{E::Anta, E::PorchArchitrave, 2}, "C7"},
		{{E::PorchArchitrave, E::PorchColumn, 2}, "C7"},
		{{E::LongWall, E::PorchFrieze, 1}, "C8"},
		{{E::PorchArchitrave, E::PorchFrieze, 2}, "C8"},
		{{E::Entablature, E::Roof, 2}, "L16"},
	};
	// Deux faces confondues, de sens opposes (dessus de l'architrave et dessous de la frise, memes quatre sommets) sont
	// signalees DuplicateFace, et non SelfIntersection : elles relevent de la meme liste.
	std::map<std::tuple<E, E, int>, std::size_t> seen;
	std::size_t records = 0, duplicates = 0, outside = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection && rec.kind != MeshDefect::DuplicateFace)
			continue;
		++(rec.kind == MeshDefect::SelfIntersection ? records : duplicates);
		ASSERT_EQ(rec.faces.size(), 2u);
		E a = owner[rec.faces[0]], b = owner[rec.faces[1]];
		if (static_cast<int>(a) > static_cast<int>(b))
			std::swap(a, b);
		const std::tuple<E, E, int> key {a, b, coplanarAxis(rec.faces[0], rec.faces[1])};
		if (listed.count(key))
			++seen[key];
		else
		{
			++outside;
			ADD_FAILURE() << "recouvrement hors liste : elements " << static_cast<int>(a) << " et " << static_cast<int>(b)
			              << ", axe " << std::get<2>(key) << ", faces " << rec.faces[0] << " et " << rec.faces[1];
		}
	}
	EXPECT_EQ(records, r.Count(MeshDefect::SelfIntersection));
	EXPECT_EQ(duplicates, r.Count(MeshDefect::DuplicateFace));
	EXPECT_EQ(outside, 0u);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (r.counts[kind] != 0)
			std::printf("  %s : %zu\n", MeshDefectName(static_cast<MeshDefect>(kind)), r.counts[kind]);
	for (const auto &[key, name] : listed)
		EXPECT_GE(seen[key], 1u) << name << " absent : elements " << static_cast<int>(std::get<0>(key)) << " et "
		                         << static_cast<int>(std::get<1>(key)) << ", axe " << std::get<2>(key);
	RecordProperty("listed_records", std::to_string(records));
	std::printf("sous-assemblage : %u faces, %zu recouvrements listes, %zu aretes non manifold\n", sub.GetNFaces(),
	            records, r.Count(MeshDefect::NonManifoldEdge));
}

// CE9 : gardes de la mise en place, chacune eprouvee sur un cas qui la viole (detecteur valide sur cas negatif).
TEST(TEST_cgarchi_classical_cella, the_layout_refuses_a_cella_that_would_overlap)
{
	const TempleSettings base = ReadmeTemple().settings;
	const auto refusalOf = [](const TempleSettings &s) {
		try
		{
			LayoutTemple(s);
		}
		catch (const std::invalid_argument &e)
		{
			return std::string(e.what());
		}
		return std::string();
	};
	EXPECT_EQ(refusalOf(base), "");
	const auto expectRefused = [&](const char *field, auto edit) {
		TempleSettings s = base;
		edit(*s.cella);
		const std::string what = refusalOf(s);
		EXPECT_NE(what.find(field), std::string::npos) << field << " : " << what;
	};
	expectRefused("'cella.porches.column.height'", [](CellaSettings &c) { c.porches->column.height = 5.7; });
	expectRefused("'cella.porches.offset'", [](CellaSettings &c) { c.porches->offset = 0.3; });
	expectRefused("'cella.porches.offset'", [](CellaSettings &c) { c.porches->offset = 2.8; });
	expectRefused("'cella.frontCrossWallPosition'", [](CellaSettings &c) { c.frontCrossWallPosition = 0.5; });
	expectRefused("'cella.backCrossWallPosition'", [](CellaSettings &c) { c.backCrossWallPosition = 16.5; });
	expectRefused("'cella.thresholdLength'", [](CellaSettings &c) { c.thresholdLength = 2.0; });
	expectRefused("'cella.lintelHeight'", [](CellaSettings &c) { c.lintelHeight = 7.2; });
	expectRefused("'cella.naosFloorHeight'", [](CellaSettings &c) { c.naosFloorHeight = 0.05; });
	expectRefused("'cella.toichobateOverhang'", [](CellaSettings &c) { c.toichobateOverhang = 3.0; });
	expectRefused("'cella.frontPorchAxis'", [](CellaSettings &c) { c.frontPorchAxis = 6.5; });
	expectRefused("'cella.wallTop'", [](CellaSettings &c) { c.wallTop = -1.0; });
	// Emprise au contact d'une colonne du peristyle : le toichobate elargi jusqu'aux colonnes du flanc.
	expectRefused("'cella'", [](CellaSettings &c) { c.toichobateOverhang = 2.6; });
}
