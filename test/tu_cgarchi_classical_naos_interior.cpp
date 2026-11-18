#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
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

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_naos_interior.h>
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Interieur du naos de l'Hephaisteion (lot C2, registre CE-15 a CE-22)
// ===========================================================================
//
// Donnee : le document embarque du temple d'Hephaistos (README sections 7.8 et
// 7.9), converti par MonumentCellaSettings, MonumentPorchSettings et
// MonumentInteriorSettings, construit avec le temple exterieur
// (MonumentTempleSettings). Les mesures sont lues sur le maillage, par piece
// (adresse TempleAddress).
//
// Tolerances :
//   - kFloatTolerance (1e-6 m) : cote d'un sommet float a ~16 m de l'origine
//     (demi-ulp 9,5e-7) contre la valeur calculee en double ;
//   - kControlTolerance (1e-9 m) : controle du README recalcule depuis les
//     longueurs lues, sommes de nombres decimaux a quatre chiffres ;
//   - kConventionTolerance (1e-12 m) : grandeur imposee par une convention de
//     la conversion (abaque, hauteur du chapiteau), recalculee en double.

using namespace cgarchi::classical;
using nlohmann::json;

namespace {

constexpr double kFloatTolerance = 1e-6;
constexpr double kControlTolerance = 1e-9;
constexpr double kConventionTolerance = 1e-12;

struct BuiltTemple
{
	TempleSettings settings;
	TempleLayout layout;
	std::vector<TempleMeshPart> parts;
	std::unique_ptr<Mesh> mesh;
};

TempleSettings CellaSettingsOf (const MonumentData &d)
{
	TempleSettings s = MonumentTempleSettings(d);
	s.cella = MonumentCellaSettings(d);
	s.cella->porches = MonumentPorchSettings(d);
	return s;
}

// Temple du README avec la cella, les porches et l'interieur, construit une fois.
const BuiltTemple &InteriorTemple ()
{
	static const BuiltTemple temple = [] {
		BuiltTemple t;
		const MonumentData &d = HephaistosMonument();
		t.settings = CellaSettingsOf(d);
		t.settings.interior = MonumentInteriorSettings(d);
		t.mesh = BuildTempleMesh(t.settings, &t.layout, &t.parts);
		return t;
	}();
	return temple;
}

// Temple du lot C1 : cella et porches, sans interieur, construit une fois.
const BuiltTemple &CellaTemple ()
{
	static const BuiltTemple temple = [] {
		BuiltTemple t;
		t.settings = CellaSettingsOf(HephaistosMonument());
		t.mesh = BuildTempleMesh(t.settings, &t.layout, &t.parts);
		return t;
	}();
	return temple;
}

bool Is (const TempleAddress &a, TempleElement element, PeristyleSide side, int index)
{
	return a.element == element && a.side == side && a.index == index;
}

const TempleMeshPart &PartOf (const BuiltTemple &t, TempleElement element, PeristyleSide side, int index = 0)
{
	for (const TempleMeshPart &p : t.parts)
		if (Is(p.address, element, side, index))
			return p;
	throw std::logic_error("partie absente");
}

std::size_t IndexOf (const BuiltTemple &t, TempleElement element)
{
	for (std::size_t k = 0; k < t.parts.size(); ++k)
		if (t.parts[k].address.element == element)
			return k;
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

double Mid (const Box3 &b, int axis)
{
	return 0.5 * (static_cast<double>(b.lo[axis]) + static_cast<double>(b.hi[axis]));
}

bool IsInteriorElement (TempleElement e)
{
	return e == TempleElement::InteriorCourse || e == TempleElement::InteriorLowerEpistyle ||
	       e == TempleElement::InteriorUpperEpistyle || e == TempleElement::InteriorCornice ||
	       e == TempleElement::InteriorLowerColumn || e == TempleElement::InteriorUpperColumn;
}

bool IsCellaElement (TempleElement e)
{
	return e == TempleElement::Toichobate || e == TempleElement::NaosFloor || e == TempleElement::LongWall ||
	       e == TempleElement::CrossWall || e == TempleElement::Threshold || e == TempleElement::Anta ||
	       e == TempleElement::PorchArchitrave || e == TempleElement::PorchFrieze;
}

// Refus de `build`, ou "" s'il reussit.
std::string RefusalOf (const std::function<void ()> &build)
{
	try
	{
		build();
	}
	catch (const std::invalid_argument &e)
	{
		return e.what();
	}
	return std::string();
}

} // namespace

// N1 : parties et pieces. Hephaistos : interieur construit, 13 pieces puis 22 colonnes apres les colonnes des porches ;
// Ares et Parthenon : interieur absent.
TEST(TEST_cgarchi_classical_naos_interior, the_interior_is_built_for_hephaistos_after_the_porch_columns)
{
	const MonumentBuild heph = BuildMonumentMesh(HephaistosMonument());
	EXPECT_EQ(heph.report[static_cast<std::size_t>(MonumentPart::Interior)].state, MonumentPartState::Built);
	for (const char *name : {"temple-ares", "temple-parthenon"})
	{
		SCOPED_TRACE(name);
		EXPECT_EQ(BuildMonumentMesh(EmbeddedMonument(name)).report[static_cast<std::size_t>(MonumentPart::Interior)].state,
		          MonumentPartState::Absent);
	}

	const BuiltTemple &t = InteriorTemple();
	std::size_t last = 0;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
		if (t.parts[k].address.element == TempleElement::PorchColumn)
			last = k;
	ASSERT_EQ(t.parts.size(), last + 1 + 13 + 22);
	using E = TempleElement;
	using S = PeristyleSide;
	std::vector<TempleAddress> expected = {
		{E::InteriorCourse, S::Left, 0},          {E::InteriorCourse, S::Right, 0},         {E::InteriorCourse, S::Front, 0},
		{E::InteriorCourse, S::Back, 0},          {E::InteriorLowerEpistyle, S::Left, 0},   {E::InteriorLowerEpistyle, S::Right, 0},
		{E::InteriorLowerEpistyle, S::Back, 0},   {E::InteriorUpperEpistyle, S::Left, 0},   {E::InteriorUpperEpistyle, S::Right, 0},
		{E::InteriorUpperEpistyle, S::Back, 0},   {E::InteriorCornice, S::Left, 0},         {E::InteriorCornice, S::Right, 0},
		{E::InteriorCornice, S::Back, 0}};
	for (const E column : {E::InteriorLowerColumn, E::InteriorUpperColumn})
	{
		for (int k = 0; k < 5; ++k)
			expected.push_back({column, S::Left, k});
		expected.push_back({column, S::Back, 0});
		for (int k = 0; k < 5; ++k)
			expected.push_back({column, S::Right, k});
	}
	for (std::size_t k = 0; k < expected.size(); ++k)
	{
		SCOPED_TRACE(k);
		EXPECT_TRUE(Is(t.parts[last + 1 + k].address, expected[k].element, expected[k].side, expected[k].index));
	}
	ASSERT_TRUE(t.layout.interior);
	EXPECT_EQ(t.layout.interior->columns.size(), 22u);
	std::size_t vertices = 0, faces = 0;
	for (std::size_t k = last + 1; k < t.parts.size(); ++k)
	{
		vertices += t.parts[k].vertexCount;
		faces += t.parts[k].faceCount;
	}
	std::printf("interieur : %zu sommets, %zu faces ; temple : %u sommets, %u faces\n", vertices, faces,
	            t.mesh->GetNVertices(), t.mesh->GetNFaces());
	RecordProperty("interior_faces", std::to_string(faces));
}

// N2 (P2') : invariants. Le document sans bloc interior, ni couronnement ni prolongement (lot C3), redonne le temple
// du lot C1 (568 766 sommets, 1 124 316 faces, au memcmp). Avec l'interieur : tout ce qui precede le comble au
// memcmp ; le comble, ses 6 premiers sommets au bit pres, puis les 8 sommets de la poche, aux bornes de la poche en
// float ; la cella et les porches ensuite, au bit pres, decales des 8 sommets de la poche ; puis l'interieur.
// BuildMonumentMesh du document sans les blocs du lot C3 est ce temple.
TEST(TEST_cgarchi_classical_naos_interior, the_interior_changes_only_the_roof_and_appends_its_pieces)
{
	const MonumentData heph = MonumentWithoutPart(HephaistosMonument(), MonumentPart::Epicranitis);
	const MonumentData withoutInterior = MonumentWithoutPart(heph, MonumentPart::Interior);
	const MonumentBuild c1 = BuildMonumentMesh(withoutInterior);
	const BuiltTemple &cella = CellaTemple();
	ASSERT_TRUE(c1.mesh);
	EXPECT_EQ(c1.mesh->GetNVertices(), 568766u);
	EXPECT_EQ(c1.mesh->GetNFaces(), 1124316u);
	ASSERT_EQ(c1.mesh->GetVertices().size(), cella.mesh->GetVertices().size());
	EXPECT_EQ(std::memcmp(c1.mesh->GetVertices().data(), cella.mesh->GetVertices().data(),
	                      cella.mesh->GetVertices().size() * sizeof(float)), 0);
	EXPECT_EQ(c1.report[static_cast<std::size_t>(MonumentPart::Interior)].state, MonumentPartState::Absent);

	const BuiltTemple &t = InteriorTemple();
	const MonumentBuild all = BuildMonumentMesh(heph);
	ASSERT_EQ(all.mesh->GetVertices().size(), t.mesh->GetVertices().size());
	EXPECT_EQ(std::memcmp(all.mesh->GetVertices().data(), t.mesh->GetVertices().data(),
	                      t.mesh->GetVertices().size() * sizeof(float)), 0);
	ASSERT_EQ(all.mesh->GetNFaces(), t.mesh->GetNFaces());

	const std::size_t roof = IndexOf(t, TempleElement::Roof);
	ASSERT_EQ(roof, IndexOf(cella, TempleElement::Roof));
	const TempleMeshPart &r0 = cella.parts[roof], &r1 = t.parts[roof];
	ASSERT_EQ(r0.firstVertex, r1.firstVertex);
	ASSERT_EQ(r0.firstFace, r1.firstFace);
	const std::vector<float> &v0 = cella.mesh->GetVertices(), &v1 = t.mesh->GetVertices();
	// Avant le comble : sommets et faces au memcmp.
	EXPECT_EQ(std::memcmp(v0.data(), v1.data(), 3 * static_cast<std::size_t>(r0.firstVertex) * sizeof(float)), 0);
	bool sameFaces = true;
	for (unsigned f = 0; sameFaces && f < r0.firstFace; ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && cella.mesh->GetFaceVertex(f, j) == t.mesh->GetFaceVertex(f, j);
	EXPECT_TRUE(sameFaces);
	for (std::size_t k = 0; k < roof; ++k)
	{
		EXPECT_EQ(cella.parts[k].vertexCount, t.parts[k].vertexCount) << k;
		EXPECT_EQ(cella.parts[k].faceCount, t.parts[k].faceCount) << k;
	}
	// Comble : 6 sommets et 8 faces sans poche ; 14 et 24 avec, les 6 premiers au bit pres.
	ASSERT_EQ(r0.vertexCount, 6u);
	ASSERT_EQ(r0.faceCount, 8u);
	ASSERT_EQ(r1.vertexCount, 14u);
	ASSERT_EQ(r1.faceCount, 24u);
	EXPECT_EQ(std::memcmp(v0.data() + 3 * r0.firstVertex, v1.data() + 3 * r1.firstVertex, 18 * sizeof(float)), 0);
	const RoofPocket &pocket = t.layout.interior->pocket;
	float zBase = std::numeric_limits<float>::max();
	for (unsigned v = 0; v < 6; ++v)
		zBase = std::min(zBase, v1[3 * (r1.firstVertex + v) + 2]);
	const std::array<std::array<double, 2>, 4> corners = {{{pocket.x0, pocket.y0}, {pocket.x1, pocket.y0},
	                                                       {pocket.x1, pocket.y1}, {pocket.x0, pocket.y1}}};
	for (unsigned v = 6; v < 14; ++v)
	{
		SCOPED_TRACE(v);
		const float *q = v1.data() + 3 * (r1.firstVertex + v);
		const std::array<double, 2> &c = corners[(v - 6) % 4];
		EXPECT_EQ(q[0], static_cast<float>(c[0]));
		EXPECT_EQ(q[1], static_cast<float>(c[1]));
		EXPECT_EQ(q[2], v < 10 ? zBase : static_cast<float>(pocket.zTop));
	}
	// Cella et porches : memes sommets, decales de 8 ; faces decalees de 8 sommets.
	const std::size_t porch = [&] {
		std::size_t k = 0;
		for (std::size_t i = 0; i < cella.parts.size(); ++i)
			if (cella.parts[i].address.element == TempleElement::PorchColumn)
				k = i;
		return k;
	}();
	ASSERT_EQ(porch + 1, cella.parts.size());
	const unsigned tail = cella.mesh->GetNVertices() - (r0.firstVertex + r0.vertexCount);
	const unsigned after0 = r0.firstVertex + r0.vertexCount, after1 = r1.firstVertex + r1.vertexCount;
	EXPECT_EQ(after1, after0 + 8u);
	EXPECT_EQ(std::memcmp(v0.data() + 3 * after0, v1.data() + 3 * after1, 3 * static_cast<std::size_t>(tail) * sizeof(float)), 0);
	bool shifted = true;
	for (unsigned f = r0.firstFace + r0.faceCount; shifted && f < cella.mesh->GetNFaces(); ++f)
		for (unsigned j = 0; j < 3; ++j)
			shifted = shifted &&
			          t.mesh->GetFaceVertex(f + (r1.faceCount - r0.faceCount), j) == cella.mesh->GetFaceVertex(f, j) + 8;
	EXPECT_TRUE(shifted);
	for (std::size_t k = roof + 1; k <= porch; ++k)
	{
		EXPECT_EQ(cella.parts[k].firstVertex + 8u, t.parts[k].firstVertex) << k;
		EXPECT_TRUE(Is(t.parts[k].address, cella.parts[k].address.element, cella.parts[k].address.side,
		               cella.parts[k].address.index)) << k;
	}
	EXPECT_EQ(t.parts[porch + 1].address.element, TempleElement::InteriorCourse);
	EXPECT_EQ(t.parts[porch + 1].firstVertex, cella.mesh->GetNVertices() + 8u);
	std::printf("avec interieur : %u sommets, %u faces ; ajoutes %u sommets, %u faces (poche comprise)\n",
	            t.mesh->GetNVertices(), t.mesh->GetNFaces(), t.mesh->GetNVertices() - cella.mesh->GetNVertices(),
	            t.mesh->GetNFaces() - cella.mesh->GetNFaces());
}

// N3 : axes, hauteurs et abaques au maillage, contre les controles du README (section 7.9), au repere du temple
// (X = y, Y = -x) : axes des flancs et du fond a 1e-6, abaques de 0,824 et 0,60, dessus des abaques, des epistyles,
// de la corniche et de l'assise XVII ; pieds au bit pres sur ce qui les porte.
TEST(TEST_cgarchi_classical_naos_interior, the_colonnade_stands_on_the_axes_and_levels_of_the_readme)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentInteriorControls &c = d.interiorControls;
	const MonumentInterior &v = d.interior;
	const MonumentCellaControls &cc = d.cellaControls;
	const auto L = [](const MonumentValue &x) { return MonumentLength(x, "interior"); };
	// Controles recalcules depuis les longueurs lues.
	EXPECT_NEAR(cc.eastCrossWallWestX, c.eastWallX, kControlTolerance);
	EXPECT_NEAR(cc.westCrossWallEastX, c.westWallX, kControlTolerance);
	for (int k = 0; k < 5; ++k)
		EXPECT_NEAR(c.eastWallX - L(v.eastAxisOffset) - k * L(v.spacing), c.flankAxesX[static_cast<std::size_t>(k)],
		            kControlTolerance) << k;
	EXPECT_NEAR(c.westWallX + L(v.westAxisOffset), c.westAxisX, kControlTolerance);
	EXPECT_NEAR(c.flankAxesX[4], c.westAxisX, kControlTolerance);
	EXPECT_NEAR(c.eastWallX - c.westWallX, c.naosLength, kControlTolerance);
	EXPECT_NEAR(cc.longWallInnerY - L(v.wallAxisOffset), c.lateralAxisY, kControlTolerance);
	EXPECT_NEAR(L(v.naveLevel) + L(v.lowerColumnHeight), c.lowerAbacusTopZ, kControlTolerance);
	EXPECT_NEAR(c.lowerAbacusTopZ + L(v.lowerEpistyleHeight), c.lowerEpistyleTopZ, kControlTolerance);
	EXPECT_NEAR(c.lowerEpistyleTopZ + L(v.upperColumnHeight), c.upperAbacusTopZ, kControlTolerance);
	EXPECT_NEAR(c.upperAbacusTopZ + L(v.upperEpistyleHeight), c.upperEpistyleTopZ, kControlTolerance);
	EXPECT_NEAR(MonumentLength(d.cella.wallTop, "cella.wallTop") + L(v.courseHeight), c.courseTopZ, kControlTolerance);
	EXPECT_NEAR(c.upperEpistyleTopZ + L(v.corniceHeight), c.corniceTopZ, kControlTolerance);
	EXPECT_NEAR(c.eastWallX + L(v.pocketMargin), c.pocketEastX, kControlTolerance);
	EXPECT_NEAR(c.westWallX - L(v.pocketMargin), c.pocketWestX, kControlTolerance);
	EXPECT_NEAR(cc.longWallInnerY + L(v.pocketMargin), c.pocketHalfWidthY, kControlTolerance);

	const BuiltTemple &t = InteriorTemple();
	const Mesh &m = *t.mesh;
	const double top = t.layout.krepis.stylobateTop;
	const auto z = [top](float value) { return static_cast<double>(value) - top; };
	using E = TempleElement;
	using S = PeristyleSide;
	for (const E storey : {E::InteriorLowerColumn, E::InteriorUpperColumn})
	{
		const bool lower = storey == E::InteriorLowerColumn;
		const double abacus = lower ? 0.824 : 0.60;
		for (const S side : {S::Left, S::Right})
			for (int k = 0; k < 5; ++k)
			{
				SCOPED_TRACE(std::to_string(lower) + " " + std::to_string(static_cast<int>(side)) + " " + std::to_string(k));
				const Box3 b = BoxOf(m, PartOf(t, storey, side, k));
				EXPECT_NEAR(Mid(b, 0), side == S::Left ? -c.lateralAxisY : c.lateralAxisY, kFloatTolerance);
				EXPECT_NEAR(Mid(b, 1), -c.flankAxesX[static_cast<std::size_t>(k)], kFloatTolerance);
				EXPECT_NEAR(static_cast<double>(b.hi[0]) - static_cast<double>(b.lo[0]), abacus, kFloatTolerance);
				EXPECT_NEAR(z(b.hi[2]), lower ? c.lowerAbacusTopZ : c.upperAbacusTopZ, kFloatTolerance);
				EXPECT_NEAR(z(b.lo[2]), lower ? L(v.naveLevel) : c.lowerEpistyleTopZ, kFloatTolerance);
			}
		const Box3 b = BoxOf(m, PartOf(t, storey, S::Back, 0));
		EXPECT_NEAR(Mid(b, 0), 0.0, kFloatTolerance);
		EXPECT_NEAR(Mid(b, 1), -c.westAxisX, kFloatTolerance);
	}
	// Pieds et dessus au bit pres : colonnes inferieures sur le sol du naos, abaques sous les epistyles, colonnes
	// superieures sur l'epistyle inferieur, corniche sur l'epistyle superieur, assise XVII sur les murs.
	const Box3 floor = BoxOf(m, PartOf(t, E::NaosFloor, S::Front));
	const Box3 lowerEpistyle = BoxOf(m, PartOf(t, E::InteriorLowerEpistyle, S::Left));
	const Box3 upperEpistyle = BoxOf(m, PartOf(t, E::InteriorUpperEpistyle, S::Right));
	const Box3 cornice = BoxOf(m, PartOf(t, E::InteriorCornice, S::Back));
	const Box3 course = BoxOf(m, PartOf(t, E::InteriorCourse, S::Front));
	const Box3 wall = BoxOf(m, PartOf(t, E::CrossWall, S::Front));
	for (const TempleMeshPart &p : t.parts)
	{
		if (p.address.element != E::InteriorLowerColumn && p.address.element != E::InteriorUpperColumn)
			continue;
		const Box3 b = BoxOf(m, p);
		const bool lower = p.address.element == E::InteriorLowerColumn;
		EXPECT_EQ(b.lo[2], lower ? floor.hi[2] : lowerEpistyle.hi[2]);
		EXPECT_EQ(b.hi[2], lower ? lowerEpistyle.lo[2] : upperEpistyle.lo[2]);
	}
	EXPECT_EQ(cornice.lo[2], upperEpistyle.hi[2]);
	EXPECT_EQ(course.lo[2], wall.hi[2]);
	EXPECT_NEAR(z(lowerEpistyle.hi[2]), c.lowerEpistyleTopZ, kFloatTolerance);
	EXPECT_NEAR(z(upperEpistyle.hi[2]), c.upperEpistyleTopZ, kFloatTolerance);
	EXPECT_NEAR(z(cornice.hi[2]), c.corniceTopZ, kFloatTolerance);
	EXPECT_NEAR(z(course.hi[2]), c.courseTopZ, kFloatTolerance);
	// Sections : epistyles de 0,68 et 0,514 centres sur la ligne des axes ; corniche en saillie de 0,016 cote nef.
	EXPECT_NEAR(static_cast<double>(lowerEpistyle.hi[0]) - static_cast<double>(lowerEpistyle.lo[0]), 0.68, kFloatTolerance);
	EXPECT_NEAR(Mid(lowerEpistyle, 0), -c.lateralAxisY, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(upperEpistyle.hi[0]) - static_cast<double>(upperEpistyle.lo[0]), 0.514, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(upperEpistyle.lo[1]), -c.eastWallX, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(cornice.lo[1]), -c.westAxisX - 0.257 - 0.016, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(cornice.hi[1]), -c.westAxisX + 0.257, kFloatTolerance);
	// Assise XVII : 0,410 dans l'epaisseur du mur de refend est, de son nu ouest vers l'est.
	EXPECT_NEAR(static_cast<double>(course.hi[1]), -c.eastWallX, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(course.hi[1]) - static_cast<double>(course.lo[1]), 0.410, kFloatTolerance);
}

// N4 : aucun sommet de l'interieur au-dessus de float (8,073 + dessus du stylobate), plafond de la poche ; aucun
// au-dessus de float (7,694 + dessus du stylobate), base du comble, hors de l'emprise de la poche.
TEST(TEST_cgarchi_classical_naos_interior, nothing_of_the_interior_rises_above_the_pocket_or_outside_it_above_z11)
{
	const BuiltTemple &t = InteriorTemple();
	const Mesh &m = *t.mesh;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const float ceiling = static_cast<float>(stylobateTop + 8.073);
	const float z11 = static_cast<float>(stylobateTop + 7.694);
	const RoofPocket &p = t.layout.interior->pocket;
	EXPECT_EQ(static_cast<float>(p.zTop), ceiling);
	const float x0 = static_cast<float>(p.x0), x1 = static_cast<float>(p.x1);
	const float y0 = static_cast<float>(p.y0), y1 = static_cast<float>(p.y1);
	float highest = std::numeric_limits<float>::lowest();
	std::size_t above = 0, outside = 0, vertices = 0;
	for (const TempleMeshPart &part : t.parts)
	{
		if (!IsInteriorElement(part.address.element))
			continue;
		for (unsigned v = part.firstVertex; v < part.firstVertex + part.vertexCount; ++v)
		{
			float q[3];
			m.GetVertex(v, q);
			++vertices;
			highest = std::max(highest, q[2]);
			if (q[2] > z11)
			{
				++above;
				outside += (q[0] < x0 || q[0] > x1 || q[1] < y0 || q[1] > y1) ? 1 : 0;
			}
		}
	}
	EXPECT_GT(vertices, 0u);
	EXPECT_LT(highest, ceiling);
	EXPECT_GT(above, 0u);
	EXPECT_EQ(outside, 0u);
	RecordProperty("interior_top_m", std::to_string(static_cast<double>(highest) - stylobateTop));
	std::printf("interieur : sommet le plus haut a %.6f, %zu sommets au-dessus de z11, %zu hors de la poche\n",
	            static_cast<double>(highest) - stylobateTop, above, outside);
}

// N5 : liste fermee des recouvrements. Sous-assemblage : stylobate (degre 0), anneau, comble, les 15 pieces de la
// cella, les 13 pieces et les 22 colonnes de l'interieur, valide a la tolerance par defaut. Chaque recouvrement est une
// paire de faces de deux pieces d'une paire listee, dans un meme plan d'axe, ou dans le plan vertical a 45 degres d'un
// onglet (axe -1) ; chaque contact liste est present ; aucun recouvrement hors liste.
TEST(TEST_cgarchi_classical_naos_interior, the_only_overlaps_of_the_interior_are_its_listed_contacts)
{
	const BuiltTemple &t = InteriorTemple();
	const Mesh &m = *t.mesh;
	std::vector<std::size_t> chosen;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
	{
		const TempleAddress &a = t.parts[k].address;
		if ((a.element == TempleElement::KrepisStep && a.index == 0) || a.element == TempleElement::Entablature ||
		    a.element == TempleElement::Roof || IsCellaElement(a.element) || IsInteriorElement(a.element))
			chosen.push_back(k);
	}
	ASSERT_EQ(chosen.size(), 3u + 15u + 13u + 22u);
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

	// Les six sommets des deux faces dans un meme plan d'axe ; -1 sinon (plan d'un onglet).
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
	// Onglet : les six sommets dans le plan vertical |X| - Y = a - yW, a 2e-6 pres (deux arrondis float de
	// coordonnees inferieures a 8 m, demi-ulp 4,8e-7 chacune).
	const auto mitre = [&](unsigned f0, unsigned f1) {
		double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest();
		for (const unsigned f : {f0, f1})
			for (unsigned j = 0; j < 3; ++j)
			{
				float q[3];
				sub.GetVertex(static_cast<unsigned>(sub.GetFaceVertex(f, j)), q);
				const double value = std::fabs(static_cast<double>(q[0])) - static_cast<double>(q[1]);
				lo = std::min(lo, value);
				hi = std::max(hi, value);
			}
		return hi - lo <= 2e-6;
	};
	using E = TempleElement;
	// Contacts listes, par paire d'elements (ordre de l'enumeration) et axe du plan commun : C1 a C8 du lot C1 sans
	// les colonnes des porches, hors du sous-assemblage ; L16 ; I1 a I7 de l'interieur (classical_temple.h).
	const std::map<std::tuple<E, E, int>, const char *> listed = {
		{{E::KrepisStep, E::Toichobate, 2}, "C1"},
		{{E::Toichobate, E::NaosFloor, 2}, "C2"},
		{{E::Toichobate, E::LongWall, 2}, "C2"},
		{{E::Toichobate, E::CrossWall, 2}, "C2"},
		{{E::Toichobate, E::Threshold, 2}, "C2"},
		{{E::Toichobate, E::Anta, 2}, "C2"},
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
		{{E::LongWall, E::PorchFrieze, 1}, "C8"},
		{{E::PorchArchitrave, E::PorchFrieze, 2}, "C8"},
		{{E::Entablature, E::Roof, 2}, "L16"},
		{{E::NaosFloor, E::InteriorLowerColumn, 2}, "I1"},
		{{E::InteriorLowerEpistyle, E::InteriorLowerColumn, 2}, "I2"},
		{{E::InteriorLowerEpistyle, E::InteriorUpperColumn, 2}, "I2"},
		{{E::InteriorUpperEpistyle, E::InteriorUpperColumn, 2}, "I2"},
		{{E::InteriorLowerEpistyle, E::InteriorLowerEpistyle, -1}, "I3"},
		{{E::InteriorUpperEpistyle, E::InteriorUpperEpistyle, -1}, "I3"},
		{{E::InteriorCornice, E::InteriorCornice, -1}, "I3"},
		{{E::CrossWall, E::InteriorLowerEpistyle, 1}, "I4"},
		{{E::InteriorCourse, E::InteriorUpperEpistyle, 1}, "I4"},
		{{E::InteriorUpperEpistyle, E::InteriorCornice, 2}, "I5"},
		{{E::LongWall, E::InteriorCourse, 2}, "I6"},
		{{E::CrossWall, E::InteriorCourse, 2}, "I6"},
		{{E::InteriorCourse, E::InteriorCourse, 0}, "I6"},
		{{E::Roof, E::InteriorCourse, 0}, "I7"},
		{{E::Roof, E::InteriorCourse, 1}, "I7"},
	};
	std::map<std::tuple<E, E, int>, std::size_t> seen;
	std::map<std::tuple<E, E, int>, std::size_t> outsideKeys;
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
		int axis = coplanarAxis(rec.faces[0], rec.faces[1]);
		if (axis < 0 && !mitre(rec.faces[0], rec.faces[1]))
			axis = -2;   // ni plan d'axe, ni onglet : traversee
		const std::tuple<E, E, int> key {a, b, axis};
		if (listed.count(key))
			++seen[key];
		else
		{
			++outside;
			++outsideKeys[key];
		}
	}
	for (const auto &[key, count] : outsideKeys)
		ADD_FAILURE() << "recouvrement hors liste : elements " << static_cast<int>(std::get<0>(key)) << " et "
		              << static_cast<int>(std::get<1>(key)) << ", axe " << std::get<2>(key) << " : " << count;
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
	std::printf("sous-assemblage : %u faces, %zu recouvrements listes, %zu faces confondues\n", sub.GetNFaces(), records,
	            duplicates);
}

// N6 : une cle interior.* faussee est refusee et nommee : valeur qui casse l'interieur (mise en place), unite fausse
// (lecteur), cle manquante (rapport et conversion), poche hors du comble (maillage) ; interieur sans cella et interieur
// avec raffinement refuses, la cause nommee.
TEST(TEST_cgarchi_classical_naos_interior, a_wrong_interior_key_is_refused_by_name)
{
	const MonumentData &heph = HephaistosMonument();
	const auto layoutRefusal = [](const MonumentData &d) {
		return RefusalOf([&] {
			TempleSettings s = CellaSettingsOf(d);
			s.interior = MonumentInteriorSettings(d);
			LayoutTemple(s);
		});
	};
	EXPECT_EQ(layoutRefusal(heph), "");
	const auto expectRefused = [&](const char *field, auto edit) {
		MonumentData d = heph;
		edit(d.interior);
		const std::string what = layoutRefusal(d);
		EXPECT_NE(what.find(field), std::string::npos) << field << " : " << what;
		EXPECT_EQ(what.rfind("Interieur : ", 0), 0u) << what;
	};
	expectRefused("'interior.spacing'", [](MonumentInterior &i) { i.spacing.value = 2.3; });
	expectRefused("'interior.backAxisOffset'", [](MonumentInterior &i) { i.westAxisOffset.value = 1.6; });
	expectRefused("'interior.wallAxisOffset'", [](MonumentInterior &i) { i.wallAxisOffset.value = 0.5; });
	expectRefused("'interior.naveLevel'", [](MonumentInterior &i) { i.naveLevel.value = 0.26; });
	expectRefused("'interior.lowerEpistyleSoffit'", [](MonumentInterior &i) { i.lowerEpistyleSoffit.value = 1.6; });
	expectRefused("'interior.upperEpistyleSoffit'", [](MonumentInterior &i) { i.upperEpistyleSoffit.value = 4.8; });
	expectRefused("'interior.corniceProjection'", [](MonumentInterior &i) { i.corniceProjection.value = 2.2; });
	expectRefused("'interior.courseThickness'", [](MonumentInterior &i) { i.courseThickness.value = 0.8; });
	expectRefused("'interior.pocketMargin'", [](MonumentInterior &i) { i.pocketMargin.value = 0.3; });
	expectRefused("'interior.pocketCeiling'", [](MonumentInterior &i) { i.pocketCeiling.value = 7.9; });

	// Poche qui sort du comble : acceptee a la mise en place, refusee au maillage par le fronton.
	MonumentData wide = heph;
	wide.interior.pocketMargin.value = 6.0;
	EXPECT_EQ(layoutRefusal(wide), "");
	const std::string roof = RefusalOf([&] { BuildMonumentMesh(wide); });
	EXPECT_NE(roof.find("'roofPocket"), std::string::npos) << roof;

	// Cle manquante : partie incomplete de cette seule cle, conversion refusee en la nommant.
	MonumentData missing = heph;
	missing.interior.corniceHeight.value.reset();
	missing.interior.corniceHeight.status = MonumentStatus::Missing;
	missing.interior.corniceHeight.sources.clear();
	EXPECT_EQ(MissingMonumentKeys(missing, MonumentPart::Interior), std::vector<std::string>{"interior.corniceHeight"});
	EXPECT_NE(RefusalOf([&] { MonumentInteriorSettings(missing); }).find("'interior.corniceHeight'"), std::string::npos);
	EXPECT_EQ(ReportMonumentParts(missing)[static_cast<std::size_t>(MonumentPart::Interior)].state,
	          MonumentPartState::Incomplete);

	// Unite fausse, interieur sans cella : refus du lecteur, chemin nomme.
	const std::string_view text = cgarchi::TempleHephaistosJson();
	const json doc = json::parse(text.begin(), text.end());
	const auto readerRefusal = [](const json &d) {
		try
		{
			ParseMonument(d.dump());
		}
		catch (const std::runtime_error &e)
		{
			return std::string(e.what());
		}
		return std::string();
	};
	json unit = doc;
	unit["interior"]["spacing"]["unit"] = "1";
	EXPECT_NE(readerRefusal(unit).find("interior.spacing.unit"), std::string::npos) << readerRefusal(unit);
	json flutes = doc;
	flutes["interior"]["fluteCount"]["value"] = 16.5;
	EXPECT_NE(readerRefusal(flutes).find("interior.fluteCount.value"), std::string::npos) << readerRefusal(flutes);
	json noCella = doc;
	noCella.erase("cella");
	noCella.erase("cellaControls");
	EXPECT_NE(readerRefusal(noCella).find(std::string(kMonumentSchema) + " interior : requires block \"cella\""),
	          std::string::npos) << readerRefusal(noCella);
	json noInterior = doc;
	noInterior.erase("interior");
	EXPECT_NE(readerRefusal(noInterior).find("interiorControls : requires block \"interior\""), std::string::npos)
		<< readerRefusal(noInterior);

	TempleSettings s = InteriorTemple().settings;
	s.cella.reset();
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'interior' demande une cella", 0), 0u);
	s = InteriorTemple().settings;
	s.refinement = TempleRefinementSettings {};
	const std::string refined = RefusalOf([&] { LayoutTemple(s); });
	EXPECT_EQ(refined.rfind("Temple : 'interior' et 'refinement'", 0), 0u) << refined;
	EXPECT_NE(refined.find("CE-9"), std::string::npos) << refined;
	s.interior.reset();
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'cella' et 'refinement'", 0), 0u);
}

// N7 (MO-P3) : la table des dependances couvre toutes les combinaisons de completude des douze parties. Pour chacune
// des 4 096, le rapport (ReportMonumentParts) donne a chaque partie l'etat que la table impose : Incomplete si elle
// l'est ; sinon Blocked par sa premiere dependance incomplete, dans l'ordre de la table ; sinon Built, le peristyle
// CompleteNotBuilt sans colonnes construites. L'interieur depend de la krepis, du peristyle, des colonnes et de la
// cella ; l'epicranitis de la krepis et de la cella ; le prolongement est de la krepis, du peristyle, des colonnes, de
// l'entablement, de la cella et de l'epicranitis ; le plafond est des memes et du prolongement ; les plafonds des
// flancs des memes et du plafond est. Quelques combinaisons sont recoupees par BuildMonumentMesh. Duree des 4 096
// rapports affichee.
TEST(TEST_cgarchi_classical_naos_interior, the_dependency_table_covers_every_combination_of_the_twelve_parts)
{
	using P = MonumentPart;
	const std::map<P, std::vector<P>> table = {
		{P::Krepis, {}},
		{P::Peristyle, {P::Krepis}},
		{P::Columns, {P::Krepis, P::Peristyle}},
		{P::Entablature, {P::Krepis, P::Peristyle, P::Columns}},
		{P::Pediment, {P::Krepis, P::Peristyle, P::Columns, P::Entablature}},
		{P::Cella, {P::Krepis}},
		{P::Porches, {P::Krepis, P::Peristyle, P::Columns, P::Cella}},
		{P::Interior, {P::Krepis, P::Peristyle, P::Columns, P::Cella}},
		{P::Epicranitis, {P::Krepis, P::Cella}},
		{P::PorchExtension, {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis}},
		{P::EastCeiling, {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension}},
		{P::FlankCeilings,
		 {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension, P::EastCeiling}},
	};
	for (const auto &[part, dependencies] : table)
		EXPECT_EQ(MonumentPartDependencies(part), dependencies) << static_cast<int>(part);

	const MonumentData &heph = HephaistosMonument();
	const auto spoil = [](MonumentData &d, P part) {
		MonumentValue *v = nullptr;
		switch (part)
		{
		case P::Krepis:      v = &d.krepis.stylobateHeight; break;
		case P::Peristyle:   v = &d.peristyle.frontSpacing; break;
		case P::Columns:     v = &d.columns.height; break;
		case P::Entablature: v = &d.entablature.dripHeight; break;
		case P::Pediment:    v = &d.pediment.tympanumFace; break;
		case P::Cella:       v = &d.cella.wallTop; break;
		case P::Porches:     v = &d.cella.porchColumnDiameter; break;
		case P::Interior:    v = &d.interior.spacing; break;
		case P::Epicranitis: v = &d.epicranitis.height; break;
		case P::PorchExtension: v = &d.porchExtension.eastSpan; break;
		case P::EastCeiling: v = &d.eastCeiling.beamSpacing; break;
		case P::FlankCeilings: v = &d.flankCeilings.beamSpacing; break;
		}
		v->value.reset();
		v->status = MonumentStatus::Missing;
		v->sources.clear();
	};
	int cases = 0;
	const auto start = std::chrono::steady_clock::now();
	for (unsigned mask = 0; mask < (1u << kMonumentPartCount); ++mask)
	{
		MonumentData d = heph;
		for (std::size_t i = 0; i < kMonumentPartCount; ++i)
			if (mask & (1u << i))
				spoil(d, static_cast<P>(i));
		const auto complete = [mask](P part) { return (mask & (1u << static_cast<unsigned>(part))) == 0; };
		const auto report = ReportMonumentParts(d);
		const auto buildable = [&](P part) {
			if (!complete(part))
				return false;
			for (const P dependency : table.at(part))
				if (!complete(dependency))
					return false;
			return true;
		};
		for (std::size_t i = 0; i < kMonumentPartCount; ++i)
		{
			const P part = static_cast<P>(i);
			SCOPED_TRACE(std::to_string(mask) + " / " + std::to_string(i));
			const MonumentPartReport &r = report[i];
			EXPECT_EQ(r.part, part);
			if (!complete(part))
			{
				EXPECT_EQ(r.state, MonumentPartState::Incomplete);
				continue;
			}
			std::optional<P> blocker;
			for (const P dependency : table.at(part))
				if (!blocker && !complete(dependency))
					blocker = dependency;
			if (blocker)
			{
				EXPECT_EQ(r.state, MonumentPartState::Blocked);
				EXPECT_TRUE(r.blockedBy == blocker);
			}
			else if (part == P::Peristyle && !buildable(P::Columns))
				EXPECT_EQ(r.state, MonumentPartState::CompleteNotBuilt);
			else
				EXPECT_EQ(r.state, MonumentPartState::Built);
		}
		++cases;
	}
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	EXPECT_EQ(cases, 4096);
	std::printf("N7 : %d combinaisons en %.3f s\n", cases, seconds);
	RecordProperty("combinations_seconds", std::to_string(seconds));

	// Recoupement par la construction : cella incomplete, interieur et epicranitis bloques par la cella ; interieur
	// incomplet, le temple du lot C1 augmente du couronnement et du prolongement (+ 592 faces, lot C3), du plafond est
	// (+ 19 980 faces), des plafonds des flancs (+ 20 856 faces) et du comble en 7 tranches au lieu du prisme (poches du
	// pteron est, du porche ouest et des flancs : 120 faces au lieu de 8) ; colonnes incompletes, interieur bloque par
	// les colonnes.
	MonumentData noWallTop = heph;
	spoil(noWallTop, P::Cella);
	const MonumentBuild b1 = BuildMonumentMesh(noWallTop);
	EXPECT_EQ(b1.report[static_cast<std::size_t>(P::Interior)].state, MonumentPartState::Blocked);
	EXPECT_TRUE(b1.report[static_cast<std::size_t>(P::Interior)].blockedBy == std::optional<P>(P::Cella));
	EXPECT_EQ(b1.mesh->GetNFaces(), 1016552u);
	MonumentData noSpacing = heph;
	spoil(noSpacing, P::Interior);
	const MonumentBuild b2 = BuildMonumentMesh(noSpacing);
	EXPECT_EQ(b2.report[static_cast<std::size_t>(P::Interior)].state, MonumentPartState::Incomplete);
	EXPECT_EQ(b2.mesh->GetNFaces(), 1124316u + 440u + 152u + 19980u + 20856u - 8u + 120u);
	EXPECT_EQ(b1.report[static_cast<std::size_t>(P::Epicranitis)].state, MonumentPartState::Blocked);
	MonumentData noHeight = heph;
	spoil(noHeight, P::Columns);
	const MonumentBuild b3 = BuildMonumentMesh(noHeight);
	EXPECT_EQ(b3.report[static_cast<std::size_t>(P::Interior)].state, MonumentPartState::Blocked);
	EXPECT_TRUE(b3.report[static_cast<std::size_t>(P::Interior)].blockedBy == std::optional<P>(P::Columns));
	for (const MonumentBuild *b : {&b1, &b2, &b3})
		for (std::size_t i = 0; i < kMonumentPartCount; ++i)
			EXPECT_EQ(b->report[i].state, ReportMonumentParts(b == &b1 ? noWallTop : b == &b2 ? noSpacing : noHeight)[i].state);
}

// N8 (CE-16 a CE-18) : colonnes des deux etages. Abaques de 0,824 et 0,60 et chapiteaux de 0,367 et 0,275 imposes ;
// gorgerin au sommet du fut (sans marche, par construction) ; reglages acceptes par ValidateGreekDoricColumnSettings.
// Les deux variantes ecartees sont refusees par la meme validation (detecteur valide sur cas negatif) : rayons depuis
// l'axe reduits au rapport des abaques (annelets sous le sommet du fut a l'etage), saillies reduites au rapport des
// diametres inferieurs (GD5) puis abaque impose (echine hors de l'abaque a l'etage).
TEST(TEST_cgarchi_classical_naos_interior, the_interior_capitals_take_the_given_abacus_and_height)
{
	const MonumentData &d = HephaistosMonument();
	const NaosInteriorSettings s = MonumentInteriorSettings(d);
	const GreekDoricColumnSettings peristyle = MonumentColumnSettings(d, false);
	const auto capitalOf = [](const GreekDoricColumnSettings &c) {
		double h = c.hypotrachelionHeight + c.echinusHeight + c.abacusHeight;
		for (const GreekDoricAnnulet &a : c.annulets)
			h += a.height;
		return h;
	};
	const struct
	{
		const GreekDoricColumnSettings *column;
		double lower, upper, height, capital, abacus;
	} storeys[] = {{&s.lowerColumn, 0.734, 0.571, 3.912, 0.367, 0.824}, {&s.upperColumn, 0.557, 0.447, 2.566, 0.275, 0.60}};
	for (const auto &st : storeys)
	{
		SCOPED_TRACE(st.lower);
		const GreekDoricColumnSettings &c = *st.column;
		EXPECT_NO_THROW(ValidateGreekDoricColumnSettings(c));
		EXPECT_EQ(c.lowerDiameter, st.lower);
		EXPECT_EQ(c.upperDiameter, st.upper);
		EXPECT_EQ(c.height, st.height);
		EXPECT_EQ(c.fluteCount, 16);
		EXPECT_EQ(c.fluteDepthRatio, peristyle.fluteDepthRatio);
		EXPECT_NEAR(2.0 * (0.5 * c.upperDiameter + c.abacusProjection), st.abacus, kConventionTolerance);
		EXPECT_NEAR(capitalOf(c), st.capital, kConventionTolerance);
		// Saillies au meme rapport que celle de l'abaque, hauteurs au meme rapport que le chapiteau.
		const double kp = c.abacusProjection / peristyle.abacusProjection, kh = capitalOf(c) / capitalOf(peristyle);
		EXPECT_NEAR(c.echinusTopProjection, peristyle.echinusTopProjection * kp, kConventionTolerance);
		EXPECT_NEAR(c.annulets.front().projection, peristyle.annulets.front().projection * kp, kConventionTolerance);
		EXPECT_NEAR(c.echinusHeight, peristyle.echinusHeight * kh, kConventionTolerance);
		const double groove = c.grooveDepth / (0.5 * c.upperDiameter);
		std::printf("etage %.3f : kp %.5f, kh %.5f, rainure %.4f du rayon, levre %.5f m, entasis %.5f m a %.4f m\n", st.lower,
		            kp, kh, groove, c.echinusLipRadius, c.entasisSagitta, c.entasisHeight);
		RecordProperty(st.lower > 0.6 ? "lower_groove_ratio" : "upper_groove_ratio", std::to_string(groove));
	}

	// Variante (b) : rayons du chapiteau depuis l'axe reduits au rapport des abaques, 0,60 / 1,140 a l'etage ; le
	// premier annelet passe sous le sommet du fut (saillie < 0).
	{
		const GreekDoricColumnSettings &c = s.upperColumn;
		GreekDoricColumnSettings b = c;
		const double R = 0.5 * peristyle.upperDiameter, r = 0.5 * c.upperDiameter;
		const double k = (0.5 * 0.60) / (R + peristyle.abacusProjection);
		for (std::size_t i = 0; i < b.annulets.size(); ++i)
			b.annulets[i].projection = k * (R + peristyle.annulets[i].projection) - r;
		b.echinusFootProjection = k * (R + peristyle.echinusFootProjection) - r;
		b.echinusJunctionProjection = k * (R + peristyle.echinusJunctionProjection) - r;
		b.echinusTopProjection = k * (R + peristyle.echinusTopProjection) - r;
		EXPECT_LT(b.annulets.front().projection, 0.0);
		const std::string what = RefusalOf([&] { ValidateGreekDoricColumnSettings(b); });
		EXPECT_NE(what.find("annulets[0].projection"), std::string::npos) << what;
	}
	// Variante (a) : saillies reduites au rapport des diametres inferieurs (GD5), abaque de 0,60 impose ensuite :
	// l'echine deborde de l'abaque.
	{
		const GreekDoricColumnSettings &c = s.upperColumn;
		GreekDoricColumnSettings a = c;
		const double k = c.lowerDiameter / peristyle.lowerDiameter;
		for (std::size_t i = 0; i < a.annulets.size(); ++i)
			a.annulets[i].projection = peristyle.annulets[i].projection * k;
		a.echinusFootProjection = peristyle.echinusFootProjection * k;
		a.echinusJunctionProjection = peristyle.echinusJunctionProjection * k;
		a.echinusTopProjection = peristyle.echinusTopProjection * k;
		a.echinusLipRadius = peristyle.echinusLipRadius * k;
		const std::string what = RefusalOf([&] { ValidateGreekDoricColumnSettings(a); });
		EXPECT_NE(what.find("abacusProjection"), std::string::npos) << what;
	}
}
