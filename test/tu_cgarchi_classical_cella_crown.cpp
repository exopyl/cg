#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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

#include <cgarchi/classical_greek_cella.h>
#include <cgarchi/classical_greek_cella_crown.h>
#include <cgarchi/classical_greek_prism_detail.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Couronnement de la cella de l'Hephaisteion (lot C3, registre CE-23 et
//  suivantes) : assise XVII exterieure et prolongement est
// ===========================================================================
//
// Donnee : le document embarque du temple d'Hephaistos (README section 7.10),
// converti par MonumentEpicranitisSettings et MonumentPorchExtensionSettings,
// construit avec le temple des lots C1 et C2. Les mesures sont lues sur le
// maillage, par piece (adresse TempleAddress).
//
// Tolerances :
//   - au bit pres (float) la ou deux pieces partagent une expression en double
//     (faces coplanaires, dessus commun) ;
//   - kFloatTolerance (1e-6 m) : cote d'un sommet float a ~16 m de l'origine
//     (demi-ulp 9,5e-7) contre la valeur calculee en double ;
//   - kPrintedHalfUnit (5e-4 m) : demi-unite du millimetre d'un controle
//     imprime par le README ;
//   - kMitreTolerance (4e-6 m) : six sommets d'onglet dans un plan vertical a
//     45 degres, |X| et |Y| inferieurs a 12 m (deux demi-ulp de 9,5e-7 par
//     coordonnee, deux coordonnees).

using namespace cgarchi::classical;
using nlohmann::json;

namespace {

constexpr double kFloatTolerance = 1e-6;
constexpr double kPrintedHalfUnit = 5e-4;
constexpr double kMitreTolerance = 4e-6;

constexpr unsigned kC2Vertices = 805466u, kC2Faces = 1597576u;

struct BuiltTemple
{
	TempleSettings settings;
	TempleLayout layout;
	std::vector<TempleMeshPart> parts;
	std::unique_ptr<Mesh> mesh;
};

// Reglages du temple des lots C1 et C2 (cella, porches, interieur).
TempleSettings InteriorSettingsOf (const MonumentData &d)
{
	TempleSettings s = MonumentTempleSettings(d);
	s.cella = MonumentCellaSettings(d);
	s.cella->porches = MonumentPorchSettings(d);
	s.interior = MonumentInteriorSettings(d);
	return s;
}

TempleSettings CrownSettingsOf (const MonumentData &d)
{
	TempleSettings s = InteriorSettingsOf(d);
	s.epicranitis = MonumentEpicranitisSettings(d);
	s.porchExtension = MonumentPorchExtensionSettings(d);
	return s;
}

// Temple du README complet, construit une fois.
const BuiltTemple &CrownTemple ()
{
	static const BuiltTemple temple = [] {
		BuiltTemple t;
		t.settings = CrownSettingsOf(HephaistosMonument());
		t.mesh = BuildTempleMesh(t.settings, &t.layout, &t.parts);
		return t;
	}();
	return temple;
}

bool IsCrownElement (TempleElement e)
{
	return e == TempleElement::EpicranitisCourse || e == TempleElement::EpicranitisPorchCourse ||
	       e == TempleElement::EpicranitisCrossCourse || e == TempleElement::PorchCrown || e == TempleElement::CrownMoulding;
}

bool IsExtensionElement (TempleElement e)
{
	return e == TempleElement::ExtensionArchitrave || e == TempleElement::ExtensionFrieze ||
	       e == TempleElement::ExtensionCrown || e == TempleElement::ExtensionMoulding;
}

bool IsNew (TempleElement e)
{
	return IsCrownElement(e) || IsExtensionElement(e);
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

// Piece isolee : sommets et faces de sa plage, indices ramenes a 0.
std::unique_ptr<Mesh> PieceMesh (const Mesh &m, const TempleMeshPart &p)
{
	std::vector<float> vertices(m.GetVertices().begin() + 3 * p.firstVertex,
	                            m.GetVertices().begin() + 3 * (p.firstVertex + p.vertexCount));
	std::vector<unsigned> faces;
	faces.reserve(3 * p.faceCount);
	for (unsigned f = p.firstFace; f < p.firstFace + p.faceCount; ++f)
		for (unsigned j = 0; j < 3; ++j)
			faces.push_back(static_cast<unsigned>(m.GetFaceVertex(f, j)) - p.firstVertex);
	auto piece = std::make_unique<Mesh>();
	piece->SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
	piece->SetFaces(static_cast<unsigned>(faces.size() / 3), 3, faces.data());
	return piece;
}

// Volume signe d'une piece fermee (theoreme de flux, sommets en double).
double SignedVolume (const Mesh &m, const TempleMeshPart &p)
{
	double sum = 0.0;
	for (unsigned f = p.firstFace; f < p.firstFace + p.faceCount; ++f)
	{
		std::array<std::array<double, 3>, 3> v {};
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			m.GetVertex(static_cast<unsigned>(m.GetFaceVertex(f, j)), q);
			v[j] = {q[0], q[1], q[2]};
		}
		sum += v[0][0] * (v[1][1] * v[2][2] - v[1][2] * v[2][1]) - v[0][1] * (v[1][0] * v[2][2] - v[1][2] * v[2][0]) +
		       v[0][2] * (v[1][0] * v[2][1] - v[1][1] * v[2][0]);
	}
	return sum / 6.0;
}

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

// K1 : parties, pieces et comptes. Les 26 pieces du couronnement puis les 10 du prolongement suivent les colonnes de
// l'interieur, dans l'ordre de BuildGreekEpicranitisPieces et BuildGreekPorchExtensionPieces ; prediction C3-1 et
// C3-2 (plan 3 section 32.1) : +272 sommets et +440 faces, puis +96 et +152 ; temple 805 834 sommets et 1 598 168
// faces, sur le document d'Hephaistos sans le plafond est (MonumentWithoutPart), qui suit le prolongement. Chaque
// piece, isolee, est un maillage valide (ferme, oriente, sans defaut) de volume positif.
TEST(TEST_cgarchi_classical_cella_crown, the_crown_and_the_extension_follow_the_interior_columns)
{
	const MonumentBuild heph = BuildMonumentMesh(MonumentWithoutPart(HephaistosMonument(), MonumentPart::EastCeiling));
	EXPECT_EQ(heph.report[static_cast<std::size_t>(MonumentPart::Epicranitis)].state, MonumentPartState::Built);
	EXPECT_EQ(heph.report[static_cast<std::size_t>(MonumentPart::PorchExtension)].state, MonumentPartState::Built);
	for (const char *name : {"temple-ares", "temple-parthenon"})
	{
		SCOPED_TRACE(name);
		const MonumentBuild b = BuildMonumentMesh(EmbeddedMonument(name));
		EXPECT_EQ(b.report[static_cast<std::size_t>(MonumentPart::Epicranitis)].state, MonumentPartState::Absent);
		EXPECT_EQ(b.report[static_cast<std::size_t>(MonumentPart::PorchExtension)].state, MonumentPartState::Absent);
	}

	const BuiltTemple &t = CrownTemple();
	std::size_t last = 0;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
		if (t.parts[k].address.element == TempleElement::InteriorUpperColumn)
			last = k;
	ASSERT_EQ(t.parts.size(), last + 1 + 26 + 10);
	using E = TempleElement;
	using S = PeristyleSide;
	std::vector<TempleAddress> expected = {
		{E::EpicranitisCourse, S::Left, 0},       {E::EpicranitisCourse, S::Right, 0},
		{E::EpicranitisPorchCourse, S::Front, 0}, {E::EpicranitisPorchCourse, S::Front, 1},
		{E::EpicranitisPorchCourse, S::Back, 0},  {E::EpicranitisPorchCourse, S::Back, 1},
		{E::EpicranitisCrossCourse, S::Front, 0}, {E::EpicranitisCrossCourse, S::Back, 0},
		{E::PorchCrown, S::Front, 0},             {E::PorchCrown, S::Back, 0},
		{E::CrownMoulding, S::Left, 0},           {E::CrownMoulding, S::Right, 0}};
	for (const S side : {S::Front, S::Back})
		for (int k = 0; k < 7; ++k)
			expected.push_back({E::CrownMoulding, side, k});
	for (const S side : {S::Left, S::Right})
	{
		expected.push_back({E::ExtensionArchitrave, side, 0});
		expected.push_back({E::ExtensionFrieze, side, 0});
		expected.push_back({E::ExtensionCrown, side, 0});
		expected.push_back({E::ExtensionMoulding, side, 0});
		expected.push_back({E::ExtensionMoulding, side, 1});
	}
	ASSERT_EQ(expected.size(), 36u);
	std::size_t crownVertices = 0, crownFaces = 0, extensionVertices = 0, extensionFaces = 0;
	for (std::size_t k = 0; k < expected.size(); ++k)
	{
		const TempleMeshPart &p = t.parts[last + 1 + k];
		SCOPED_TRACE(k);
		EXPECT_TRUE(Is(p.address, expected[k].element, expected[k].side, expected[k].index));
		const bool strip = p.address.element == E::CrownMoulding || p.address.element == E::ExtensionMoulding;
		EXPECT_EQ(p.vertexCount, strip ? 12u : 8u);
		EXPECT_EQ(p.faceCount, strip ? 20u : 12u);
		(k < 26 ? crownVertices : extensionVertices) += p.vertexCount;
		(k < 26 ? crownFaces : extensionFaces) += p.faceCount;
		EXPECT_GT(SignedVolume(*t.mesh, p), 0.0);
		const std::unique_ptr<Mesh> piece = PieceMesh(*t.mesh, p);
		const MeshValidationReport r = ValidateMesh(*piece);
		for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
			EXPECT_EQ(r.counts[kind], 0u) << MeshDefectName(static_cast<MeshDefect>(kind));
	}
	EXPECT_EQ(crownVertices, 272u);
	EXPECT_EQ(crownFaces, 440u);
	EXPECT_EQ(extensionVertices, 96u);
	EXPECT_EQ(extensionFaces, 152u);
	EXPECT_EQ(t.mesh->GetNVertices(), 805834u);
	EXPECT_EQ(t.mesh->GetNFaces(), 1598168u);
	EXPECT_EQ(CountTempleMeshFaces(t.settings), t.mesh->GetNFaces());
	EXPECT_EQ(t.parts.back().firstVertex + t.parts.back().vertexCount, t.mesh->GetNVertices());
	ASSERT_EQ(heph.mesh->GetNVertices(), t.mesh->GetNVertices());
	EXPECT_EQ(std::memcmp(heph.mesh->GetVertices().data(), t.mesh->GetVertices().data(),
	                      t.mesh->GetVertices().size() * sizeof(float)), 0);
	std::printf("couronnement : %zu sommets, %zu faces ; prolongement : %zu sommets, %zu faces ; temple : %u, %u\n",
	            crownVertices, crownFaces, extensionVertices, extensionFaces, t.mesh->GetNVertices(), t.mesh->GetNFaces());
}

// K2 : dessus unique. Le plus haut sommet de chaque piece, hors architrave et frise prolongees, est au bit pres la
// meme cote float, a 1e-6 de stylobateTop + 7,565 (controle topZ) ; dessous des couronnements a 7,357 (controle),
// hauteur 0,208 ; face interieure de l'epicranitis, nus des bandes des murs et des couronnements contre les controles
// du README (millimetre imprime) et contre la valeur calculee (1e-6).
TEST(TEST_cgarchi_classical_cella_crown, the_crown_has_a_single_top_and_the_readme_band_faces)
{
	const BuiltTemple &t = CrownTemple();
	const MonumentData &d = HephaistosMonument();
	const MonumentEpicranitisControls &c = d.epicranitisControls;
	const EpicranitisLayout &l = *t.layout.epicranitis;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const float top = static_cast<float>(l.top);
	EXPECT_NEAR(static_cast<double>(top), stylobateTop + c.topZ, kFloatTolerance);
	std::size_t pieces = 0;
	for (const TempleMeshPart &p : t.parts)
	{
		if (!IsNew(p.address.element) || p.address.element == TempleElement::ExtensionArchitrave ||
		    p.address.element == TempleElement::ExtensionFrieze)
			continue;
		++pieces;
		EXPECT_EQ(BoxOf(*t.mesh, p).hi[2], top) << static_cast<int>(p.address.element) << " / " << p.address.index;
	}
	EXPECT_EQ(pieces, 26u + 6u);

	const Box3 crown = BoxOf(*t.mesh, PartOf(t, TempleElement::PorchCrown, PeristyleSide::Front));
	EXPECT_NEAR(static_cast<double>(crown.lo[2]) - stylobateTop, c.porchCrownBottomZ, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(crown.hi[2]) - static_cast<double>(crown.lo[2]), c.porchCrownHeight, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(crown.lo[2]), l.porch.friezeTop, kFloatTolerance);

	const Box3 course = BoxOf(*t.mesh, PartOf(t, TempleElement::EpicranitisCourse, PeristyleSide::Right));
	EXPECT_NEAR(static_cast<double>(course.lo[0]), c.innerFaceY, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(course.lo[0]), l.innerFace, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(course.lo[2]) - stylobateTop, c.topZ - t.settings.epicranitis->height, kPrintedHalfUnit);

	const EpicranitisSettings &s = *t.settings.epicranitis;
	const Box3 wall = BoxOf(*t.mesh, PartOf(t, TempleElement::CrownMoulding, PeristyleSide::Right, 0));
	EXPECT_NEAR(static_cast<double>(wall.hi[0]), c.wallBandY, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(wall.hi[0]), l.naos.outer + s.bandProjection, kFloatTolerance);
	// Repere du temple : Y = -x du README.
	const Box3 east = BoxOf(*t.mesh, PartOf(t, TempleElement::CrownMoulding, PeristyleSide::Front, 0));
	EXPECT_NEAR(-static_cast<double>(east.lo[1]), c.eastBandX, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(east.lo[1]), l.porch.front - s.bandProjection, kFloatTolerance);
	const Box3 west = BoxOf(*t.mesh, PartOf(t, TempleElement::CrownMoulding, PeristyleSide::Back, 0));
	EXPECT_NEAR(-static_cast<double>(west.hi[1]), c.westBandX, kPrintedHalfUnit);
	EXPECT_NEAR(static_cast<double>(west.hi[1]), l.porch.back + s.bandProjection, kFloatTolerance);
	// Profil : bande en saillie bandProjection jusqu'au gradin, listel au-dessus.
	EXPECT_NEAR(l.top - l.step, s.listelHeight + s.filletHeight, 1e-12);
	EXPECT_NEAR(s.listelHeight, 1.15 * kMonumentInch, 1e-15);
	EXPECT_NEAR(s.bandProjection, 1.9 * kMonumentInch, 1e-15);
	// Profil, sommet par sommet : couples (saillie depuis le nu, z) contre les six attendus, le gradin calcule depuis
	// les reglages lus (dessus moins listel et filet), une bande de mur long et la bande exterieure d'un couronnement.
	const double expectedTop = l.naos.wallTop + s.height;
	const double expectedStep = expectedTop - (s.listelHeight + s.filletHeight);
	const auto expectProfile = [&](const TempleMeshPart &p, const std::function<double (const float *)> &projection, double z0) {
		const std::array<std::array<double, 2>, 6> expected = {{{0.0, z0},
		                                                        {s.bandProjection, z0},
		                                                        {s.bandProjection, expectedStep},
		                                                        {s.listelProjection, expectedStep},
		                                                        {s.listelProjection, expectedTop},
		                                                        {0.0, expectedTop}}};
		std::array<bool, 6> found {};
		for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
		{
			float q[3];
			t.mesh->GetVertex(v, q);
			const double d = projection(q), z = static_cast<double>(q[2]);
			bool matched = false;
			for (std::size_t k = 0; k < expected.size(); ++k)
				if (std::fabs(d - expected[k][0]) <= kFloatTolerance && std::fabs(z - expected[k][1]) <= kFloatTolerance)
					matched = found[k] = true;
			EXPECT_TRUE(matched) << "sommet (" << d << ", " << z << ")";
		}
		for (std::size_t k = 0; k < expected.size(); ++k)
			EXPECT_TRUE(found[k]) << "sommet attendu " << k;
	};
	const double outer = l.naos.outer, porchFront = l.porch.front;
	expectProfile(PartOf(t, TempleElement::CrownMoulding, PeristyleSide::Right, 0),
	              [outer](const float *q) { return static_cast<double>(q[0]) - outer; }, l.naos.wallTop);
	expectProfile(PartOf(t, TempleElement::CrownMoulding, PeristyleSide::Front, 0),
	              [porchFront](const float *q) { return porchFront - static_cast<double>(q[1]); }, l.porch.friezeTop);
	RecordProperty("crown_top_m", std::to_string(static_cast<double>(top) - stylobateTop));
}

// K3 : le maillage du lot C2 est un prefixe exact. Le document sans les blocs du lot redonne 805 466 sommets et
// 1 597 576 faces, au memcmp contre le debut du maillage complet, faces comprises ; le comble garde 14 sommets et
// 24 faces ; les plages du lot C2 sont les premieres.
TEST(TEST_cgarchi_classical_cella_crown, the_c2_mesh_is_an_exact_prefix)
{
	const MonumentBuild c2 = BuildMonumentMesh(MonumentWithoutPart(HephaistosMonument(), MonumentPart::Epicranitis));
	const BuiltTemple &t = CrownTemple();
	ASSERT_TRUE(c2.mesh);
	EXPECT_EQ(c2.mesh->GetNVertices(), kC2Vertices);
	EXPECT_EQ(c2.mesh->GetNFaces(), kC2Faces);
	EXPECT_EQ(c2.report[static_cast<std::size_t>(MonumentPart::Epicranitis)].state, MonumentPartState::Absent);
	EXPECT_EQ(c2.report[static_cast<std::size_t>(MonumentPart::PorchExtension)].state, MonumentPartState::Absent);
	ASSERT_GT(t.mesh->GetNVertices(), c2.mesh->GetNVertices());
	EXPECT_EQ(std::memcmp(c2.mesh->GetVertices().data(), t.mesh->GetVertices().data(),
	                      c2.mesh->GetVertices().size() * sizeof(float)), 0);
	bool sameFaces = true;
	for (unsigned f = 0; sameFaces && f < c2.mesh->GetNFaces(); ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && c2.mesh->GetFaceVertex(f, j) == t.mesh->GetFaceVertex(f, j);
	EXPECT_TRUE(sameFaces);
	ASSERT_LT(c2.parts.size(), t.parts.size());
	for (std::size_t k = 0; k < c2.parts.size(); ++k)
	{
		EXPECT_TRUE(Is(t.parts[k].address, c2.parts[k].address.element, c2.parts[k].address.side, c2.parts[k].address.index))
			<< k;
		EXPECT_EQ(t.parts[k].firstVertex, c2.parts[k].firstVertex) << k;
		EXPECT_EQ(t.parts[k].faceCount, c2.parts[k].faceCount) << k;
	}
	EXPECT_EQ(t.parts[c2.parts.size()].address.element, TempleElement::EpicranitisCourse);
	EXPECT_EQ(t.parts[c2.parts.size()].firstVertex, kC2Vertices);
	const TempleMeshPart &roof = PartOf(t, TempleElement::Roof, PeristyleSide::Front);
	EXPECT_EQ(roof.vertexCount, 14u);
	EXPECT_EQ(roof.faceCount, 24u);
}

// K4 : rien du lot au-dessus de z11. Le plus haut sommet des pieces nouvelles est a float (top), sous float
// (stylobateTop + 7,694), base du comble, et sous le dessus de l'anneau.
TEST(TEST_cgarchi_classical_cella_crown, nothing_of_the_crown_rises_above_z11)
{
	const BuiltTemple &t = CrownTemple();
	const double stylobateTop = t.layout.krepis.stylobateTop;
	const float z11 = static_cast<float>(stylobateTop + 7.694);
	const float ringTop = BoxOf(*t.mesh, PartOf(t, TempleElement::Entablature, PeristyleSide::Front)).hi[2];
	float highest = std::numeric_limits<float>::lowest();
	std::size_t vertices = 0;
	for (const TempleMeshPart &p : t.parts)
		if (IsNew(p.address.element))
		{
			highest = std::max(highest, BoxOf(*t.mesh, p).hi[2]);
			vertices += p.vertexCount;
		}
	EXPECT_EQ(vertices, 272u + 96u);
	EXPECT_EQ(highest, static_cast<float>(t.layout.epicranitis->top));
	EXPECT_LT(highest, z11);
	EXPECT_LT(highest, ringTop);
	std::printf("couronnement : sommet le plus haut a %.6f, base du comble a %.6f\n",
	            static_cast<double>(highest) - stylobateTop, static_cast<double>(z11) - stylobateTop);
}

// K5 : la face interieure de l'epicranitis est coplanaire, au bit pres, a la face exterieure de l'assise XVII
// interieure ; les bouts des assises des porches et des murs de refend sont, au bit pres, sur les faces avant et
// arriere de l'assise XVII interieure ; le dessous des assises est le dessus des murs.
TEST(TEST_cgarchi_classical_cella_crown, the_epicranitis_is_coplanar_with_the_inner_course_xvii)
{
	const BuiltTemple &t = CrownTemple();
	const Mesh &m = *t.mesh;
	using E = TempleElement;
	using S = PeristyleSide;
	const Box3 innerLeft = BoxOf(m, PartOf(t, E::InteriorCourse, S::Left)), innerRight = BoxOf(m, PartOf(t, E::InteriorCourse, S::Right));
	const Box3 courseLeft = BoxOf(m, PartOf(t, E::EpicranitisCourse, S::Left));
	const Box3 courseRight = BoxOf(m, PartOf(t, E::EpicranitisCourse, S::Right));
	EXPECT_EQ(courseRight.lo[0], innerRight.hi[0]);
	EXPECT_EQ(courseLeft.hi[0], innerLeft.lo[0]);
	EXPECT_EQ(BoxOf(m, PartOf(t, E::EpicranitisPorchCourse, S::Front, 1)).hi[1], innerRight.lo[1]);
	EXPECT_EQ(BoxOf(m, PartOf(t, E::EpicranitisPorchCourse, S::Back, 0)).lo[1], innerLeft.hi[1]);
	EXPECT_EQ(BoxOf(m, PartOf(t, E::EpicranitisCrossCourse, S::Front)).hi[1],
	          BoxOf(m, PartOf(t, E::InteriorCourse, S::Front)).lo[1]);
	EXPECT_EQ(BoxOf(m, PartOf(t, E::EpicranitisCrossCourse, S::Back)).lo[1],
	          BoxOf(m, PartOf(t, E::InteriorCourse, S::Back)).hi[1]);
	EXPECT_EQ(courseRight.lo[2], innerRight.lo[2]);
	EXPECT_EQ(courseRight.lo[2], BoxOf(m, PartOf(t, E::LongWall, S::Right)).hi[2]);
	EXPECT_EQ(courseRight.hi[0], BoxOf(m, PartOf(t, E::LongWall, S::Right)).hi[0]);
	// Epaisseur restante de l'epicranitis : 0,351 m, dans la fourchette 0,335/0,365 de [O] p. 75.
	const double outer = static_cast<double>(courseRight.hi[0]) - static_cast<double>(courseRight.lo[0]);
	EXPECT_NEAR(outer, 0.351, kFloatTolerance);
	EXPECT_GT(outer, 0.335);
	EXPECT_LT(outer, 0.365);
}

// K6 : le prolongement est pose sur les abaques des 3es colonnes des flancs et bute contre l'anneau. Dessous de
// l'architrave prolongee au bit pres au dessus des abaques (le sommet le plus haut de ces colonnes) et au dessous de
// l'anneau ; bout cote anneau au bit pres sur la face interieure de l'anneau ; bouts cote antes sur les bouts de
// l'entablement des porches ; controles du README : face interieure a y = +-5,778, dessous a 5,715.
TEST(TEST_cgarchi_classical_cella_crown, the_extension_rests_on_the_abaci_and_meets_the_ring)
{
	const BuiltTemple &t = CrownTemple();
	const Mesh &m = *t.mesh;
	const MonumentPorchExtensionControls &c = HephaistosMonument().porchExtensionControls;
	const double stylobateTop = t.layout.krepis.stylobateTop;
	using E = TempleElement;
	using S = PeristyleSide;
	const Box3 ring = BoxOf(m, PartOf(t, E::Entablature, S::Front));
	float ringInner = std::numeric_limits<float>::max();
	const TempleMeshPart &ringPart = PartOf(t, E::Entablature, S::Front);
	for (unsigned v = ringPart.firstVertex; v < ringPart.firstVertex + ringPart.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		if (q[0] > 0.0f)
			ringInner = std::min(ringInner, q[0]);
	}
	const Box3 frieze = BoxOf(m, PartOf(t, E::PorchFrieze, S::Front));
	for (const S side : {S::Left, S::Right})
	{
		SCOPED_TRACE(static_cast<int>(side));
		const Box3 architrave = BoxOf(m, PartOf(t, E::ExtensionArchitrave, side));
		EXPECT_EQ(architrave.lo[2], ring.lo[2]);
		EXPECT_NEAR(static_cast<double>(architrave.lo[2]), t.layout.entablatureFoot, kFloatTolerance);
		EXPECT_NEAR(static_cast<double>(architrave.lo[2]) - stylobateTop, c.architraveFootZ, kPrintedHalfUnit);
		EXPECT_EQ(side == S::Right ? architrave.hi[0] : -architrave.lo[0], ringInner);
		EXPECT_EQ(side == S::Right ? architrave.lo[0] : -architrave.hi[0], frieze.hi[0]);
		EXPECT_NEAR(static_cast<double>(ringInner), c.ringInnerY, kPrintedHalfUnit);
		// Abaques : chaque colonne du peristyle dont l'emprise rencontre l'architrave prolongee a son sommet au
		// dessous de l'architrave ; il y en a une par cote.
		int under = 0;
		for (const TempleMeshPart &p : t.parts)
		{
			if (p.address.element != E::Column)
				continue;
			const Box3 b = BoxOf(m, p);
			if (b.hi[0] <= architrave.lo[0] || b.lo[0] >= architrave.hi[0] || b.hi[1] <= architrave.lo[1] ||
			    b.lo[1] >= architrave.hi[1])
				continue;
			++under;
			EXPECT_EQ(b.hi[2], architrave.lo[2]);
		}
		EXPECT_EQ(under, 1);
	}
	// Ressaut de 6 mm sur l'architrave des porches (5,709), convention D-C3-10.
	const Box3 porch = BoxOf(m, PartOf(t, E::PorchArchitrave, S::Front));
	EXPECT_NEAR(static_cast<double>(ring.lo[2]) - static_cast<double>(porch.lo[2]), 0.006, kFloatTolerance + 5e-4);
	EXPECT_NEAR(t.layout.porchExtension->outer - t.layout.porchExtension->inner, t.settings.porchExtension->eastSpan,
	            kPorchExtensionClosure);
	std::printf("prolongement : fermeture %.6f m (face interieure de l'anneau %.6f, faces des antes + eastSpan %.6f)\n",
	            t.layout.porchExtension->outer - (t.layout.porchExtension->inner + t.settings.porchExtension->eastSpan),
	            t.layout.porchExtension->outer, t.layout.porchExtension->inner + t.settings.porchExtension->eastSpan);
}

// K7 : refus nommes. Mise en place du couronnement (cles faussees du document), du temple (interieur d'autre
// epaisseur, dessus au-dessus de l'anneau, emprise hors de l'anneau, couronnement sans cella, prolongement sans
// couronnement ou sans entablement, raffinement), du prolongement (fermeture), du lecteur (blocs sans leur partie, cle
// inconnue) et du rapport (cle manquante).
TEST(TEST_cgarchi_classical_cella_crown, a_wrong_crown_key_is_refused_by_name)
{
	const MonumentData &heph = HephaistosMonument();
	const auto layoutRefusal = [](const MonumentData &d) {
		return RefusalOf([&] { LayoutTemple(CrownSettingsOf(d)); });
	};
	EXPECT_EQ(layoutRefusal(heph), "");
	// Debut du message (prefixe et champ) et motif du refus.
	const auto expectRefused = [&](const std::string &head, const char *motif, auto edit) {
		MonumentData d = heph;
		edit(d);
		const std::string what = layoutRefusal(d);
		EXPECT_EQ(what.rfind(head, 0), 0u) << what;
		EXPECT_NE(what.find(motif), std::string::npos) << motif << " : " << what;
	};
	expectRefused("Couronnement : 'epicranitis.innerOffset' ", "sort la face interieure de l'epicranitis",
	              [](MonumentData &d) { d.epicranitis.innerOffset.value = 0.8; });
	expectRefused("Couronnement : 'epicranitis.listelHeight' ", "ne laissent pas de bande a meandre",
	              [](MonumentData &d) { d.epicranitis.listelHeight.value = 8.0; });
	expectRefused("Couronnement : 'epicranitis.bandProjection' ", "atteint la face exterieure des antes",
	              [](MonumentData &d) { d.epicranitis.bandProjection.value = 4.0; });
	expectRefused("Couronnement : 'epicranitis.listelProjection' ", "atteint la face exterieure des antes",
	              [](MonumentData &d) { d.epicranitis.listelProjection.value = 4.0; });
	expectRefused("Temple : 'epicranitis.innerOffset' ", "differe de 'interior.courseThickness'",
	              [](MonumentData &d) { d.epicranitis.innerOffset.value = 0.409; });
	expectRefused("Temple : 'epicranitis' ", "depasse le dessus de l'entablement",
	              [](MonumentData &d) { d.epicranitis.height.value = 0.5; });
	expectRefused("Temple : 'epicranitis' ", "sort de la face interieure de l'anneau",
	              [](MonumentData &d) { d.cella.porchEntablatureFront.value = 4.53; });
	expectRefused("Prolongement : 'porchExtension.eastSpan' ", "ne ferme pas l'ecart",
	              [](MonumentData &d) { d.porchExtension.eastSpan.value = 1.83; });

	// Dessus au plus au dessus de la frise : mise en place seule, sur une cella dont la frise passe les murs.
	CellaLayout cella = LayoutTemple(CrownSettingsOf(heph)).cella.value();
	EpicranitisSettings low = MonumentEpicranitisSettings(heph);
	cella.porchEntablature.friezeTop = cella.naos.wallTop + low.height;
	const std::string height = RefusalOf([&] { LayoutGreekEpicranitis(low, cella); });
	EXPECT_EQ(height.rfind("Couronnement : 'epicranitis.height'", 0), 0u) << height;
	low.height = -1.0;
	EXPECT_EQ(RefusalOf([&] { LayoutGreekEpicranitis(low, cella); }).rfind("Couronnement : 'epicranitis.height' n'est pas", 0), 0u);

	TempleSettings s = CrownSettingsOf(heph);
	s.cella.reset();
	s.interior.reset();
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'epicranitis' demande une cella", 0), 0u);
	s = CrownSettingsOf(heph);
	s.epicranitis.reset();
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'porchExtension' demande le couronnement", 0), 0u);
	s = CrownSettingsOf(heph);
	s.pediment.reset();
	s.entablature.reset();
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'porchExtension' demande un entablement", 0), 0u);
	s = CrownSettingsOf(heph);
	s.interior.reset();
	s.cella.reset();
	s.porchExtension.reset();
	s.refinement = TempleRefinementSettings {};
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'epicranitis' et 'refinement'", 0), 0u);
	s = CrownSettingsOf(heph);
	s.interior.reset();
	s.cella.reset();
	s.epicranitis.reset();
	s.refinement = TempleRefinementSettings {};
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }).rfind("Temple : 'porchExtension' et 'refinement' ne se combinent pas", 0), 0u);
	// Sans interieur, une autre epaisseur est acceptee.
	s = CrownSettingsOf(heph);
	s.interior.reset();
	s.epicranitis->innerOffset = 0.409;
	EXPECT_EQ(RefusalOf([&] { LayoutTemple(s); }), "");

	// Cle manquante : partie incomplete de cette seule cle, conversion refusee en la nommant ; prolongement bloque.
	MonumentData missing = heph;
	missing.epicranitis.filletHeight.value.reset();
	missing.epicranitis.filletHeight.status = MonumentStatus::Missing;
	missing.epicranitis.filletHeight.sources.clear();
	EXPECT_EQ(MissingMonumentKeys(missing, MonumentPart::Epicranitis), std::vector<std::string>{"epicranitis.filletHeight"});
	EXPECT_NE(RefusalOf([&] { MonumentEpicranitisSettings(missing); }).find("'epicranitis.filletHeight'"), std::string::npos);
	const auto report = ReportMonumentParts(missing);
	EXPECT_EQ(report[static_cast<std::size_t>(MonumentPart::Epicranitis)].state, MonumentPartState::Incomplete);
	EXPECT_EQ(report[static_cast<std::size_t>(MonumentPart::PorchExtension)].state, MonumentPartState::Blocked);
	EXPECT_TRUE(report[static_cast<std::size_t>(MonumentPart::PorchExtension)].blockedBy ==
	            std::optional<MonumentPart>(MonumentPart::Epicranitis));

	// Lecteur : blocs sans leur partie, cle inconnue, unite fausse.
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
	const std::string schema(kMonumentSchema);
	json noCella = doc;
	noCella.erase("cella");
	EXPECT_NE(readerRefusal(noCella).find(schema + " epicranitis : requires block \"cella\""), std::string::npos)
		<< readerRefusal(noCella);
	json noCrown = doc;
	noCrown.erase("epicranitis");
	EXPECT_NE(readerRefusal(noCrown).find(schema + " porchExtension : requires block \"epicranitis\""), std::string::npos);
	EXPECT_NE(readerRefusal(noCrown).find(schema + " epicranitisControls : requires block \"epicranitis\""),
	          std::string::npos);
	json noExtension = doc;
	noExtension.erase("porchExtension");
	EXPECT_NE(readerRefusal(noExtension).find(schema + " porchExtensionControls : requires block \"porchExtension\""),
	          std::string::npos);
	json unknown = doc;
	unknown["epicranitis"]["bandHeight"] = unknown["epicranitis"]["listelHeight"];
	EXPECT_NE(readerRefusal(unknown).find("epicranitis.bandHeight : unknown field"), std::string::npos);
	json unit = doc;
	unit["porchExtension"]["eastSpan"]["unit"] = "1";
	EXPECT_NE(readerRefusal(unit).find("porchExtension.eastSpan.unit"), std::string::npos) << readerRefusal(unit);
}

// K8 : liste fermee des recouvrements du lot. Sous-assemblage : anneau, comble, les 15 pieces de la cella, les 13
// pieces de l'interieur, les colonnes du peristyle sous le prolongement, et les 36 pieces du lot ; valide a la
// tolerance par defaut. Chaque recouvrement (SelfIntersection ou DuplicateFace) qui touche une piece du lot est une
// paire de faces de deux elements d'une paire listee, dans un meme plan d'axe, ou dans un plan vertical a 45 degres
// (onglet, axe -1) ; chaque contact liste est present ; aucun recouvrement hors liste.
TEST(TEST_cgarchi_classical_cella_crown, the_only_overlaps_of_the_crown_are_its_listed_contacts)
{
	const BuiltTemple &t = CrownTemple();
	const Mesh &m = *t.mesh;
	using E = TempleElement;
	Box3 extension;
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == E::ExtensionArchitrave)
		{
			const Box3 b = BoxOf(m, p);
			for (int k = 0; k < 3; ++k)
			{
				extension.lo[k] = std::min(extension.lo[k], b.lo[k]);
				extension.hi[k] = std::max(extension.hi[k], b.hi[k]);
			}
		}
	const std::set<E> cellaElements = {E::Toichobate, E::NaosFloor, E::LongWall, E::CrossWall, E::Threshold, E::Anta,
	                                   E::PorchArchitrave, E::PorchFrieze};
	const std::set<E> interiorPieces = {E::InteriorCourse, E::InteriorLowerEpistyle, E::InteriorUpperEpistyle,
	                                    E::InteriorCornice};
	std::vector<std::size_t> chosen;
	std::size_t columns = 0;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
	{
		const TempleAddress &a = t.parts[k].address;
		bool take = a.element == E::Entablature || a.element == E::Roof || cellaElements.count(a.element) ||
		            interiorPieces.count(a.element) || IsNew(a.element);
		if (a.element == E::Column)
		{
			const Box3 b = BoxOf(m, t.parts[k]);
			take = !(b.hi[0] < extension.lo[0] || b.lo[0] > extension.hi[0] || b.hi[1] < extension.lo[1] ||
			         b.lo[1] > extension.hi[1]);
			columns += take ? 1 : 0;
		}
		if (take)
			chosen.push_back(k);
	}
	EXPECT_EQ(columns, 2u);
	ASSERT_EQ(chosen.size(), 2u + 15u + 13u + columns + 36u);
	std::vector<float> vertices;
	std::vector<unsigned> faces;
	std::vector<E> owner;
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
	// Onglet : les six sommets dans un plan vertical X + Y = c ou X - Y = c.
	const auto mitre = [&](unsigned f0, unsigned f1) {
		for (const double sign : {1.0, -1.0})
		{
			double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest();
			for (const unsigned f : {f0, f1})
				for (unsigned j = 0; j < 3; ++j)
				{
					float q[3];
					sub.GetVertex(static_cast<unsigned>(sub.GetFaceVertex(f, j)), q);
					const double value = static_cast<double>(q[0]) + sign * static_cast<double>(q[1]);
					lo = std::min(lo, value);
					hi = std::max(hi, value);
				}
			if (hi - lo <= kMitreTolerance)
				return true;
		}
		return false;
	};
	// Contacts du lot, par paire d'elements (ordre de l'enumeration) et axe du plan commun (classical_temple.h).
	const std::map<std::tuple<E, E, int>, const char *> listed = {
		{{E::LongWall, E::EpicranitisCourse, 2}, "K1"},
		{{E::LongWall, E::EpicranitisPorchCourse, 2}, "K1"},
		{{E::CrossWall, E::EpicranitisCrossCourse, 2}, "K1"},
		{{E::PorchFrieze, E::PorchCrown, 2}, "K2"},
		{{E::LongWall, E::PorchCrown, 1}, "K2"},
		{{E::InteriorCourse, E::EpicranitisCourse, 0}, "K3"},
		{{E::InteriorCourse, E::EpicranitisPorchCourse, 1}, "K3"},
		{{E::InteriorCourse, E::EpicranitisCrossCourse, 1}, "K3"},
		{{E::EpicranitisCourse, E::PorchCrown, 1}, "K4"},
		{{E::EpicranitisPorchCourse, E::PorchCrown, 1}, "K4"},
		{{E::EpicranitisCourse, E::EpicranitisPorchCourse, 0}, "K5"},
		{{E::EpicranitisPorchCourse, E::EpicranitisCrossCourse, 0}, "K5"},
		{{E::EpicranitisCourse, E::CrownMoulding, 0}, "K5"},
		{{E::EpicranitisPorchCourse, E::CrownMoulding, 0}, "K5"},
		{{E::EpicranitisCrossCourse, E::CrownMoulding, 1}, "K5"},
		{{E::PorchCrown, E::CrownMoulding, 1}, "K5"},
		{{E::CrownMoulding, E::CrownMoulding, -1}, "K5"},
		{{E::Column, E::ExtensionArchitrave, 2}, "K6"},
		{{E::Entablature, E::ExtensionArchitrave, 0}, "K6"},
		{{E::Entablature, E::ExtensionFrieze, 0}, "K6"},
		{{E::Entablature, E::ExtensionCrown, 0}, "K6"},
		{{E::Entablature, E::ExtensionMoulding, 0}, "K6"},
		{{E::PorchArchitrave, E::ExtensionArchitrave, 0}, "K7"},
		{{E::PorchFrieze, E::ExtensionFrieze, 0}, "K7"},
		{{E::PorchCrown, E::ExtensionCrown, 0}, "K8"},
		{{E::CrownMoulding, E::ExtensionMoulding, 0}, "K8"},
		{{E::ExtensionArchitrave, E::ExtensionFrieze, 2}, "K7"},
		{{E::ExtensionFrieze, E::ExtensionCrown, 2}, "K8"},
		{{E::ExtensionCrown, E::ExtensionMoulding, 1}, "K8"},
	};
	std::map<std::tuple<E, E, int>, std::size_t> seen, outsideKeys;
	std::map<std::tuple<E, E, int>, std::size_t> seenDuplicates;
	std::size_t records = 0, duplicates = 0, outside = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection && rec.kind != MeshDefect::DuplicateFace)
			continue;
		ASSERT_EQ(rec.faces.size(), 2u);
		E a = owner[rec.faces[0]], b = owner[rec.faces[1]];
		if (!IsNew(a) && !IsNew(b))
			continue;
		++(rec.kind == MeshDefect::SelfIntersection ? records : duplicates);
		if (static_cast<int>(a) > static_cast<int>(b))
			std::swap(a, b);
		int axis = coplanarAxis(rec.faces[0], rec.faces[1]);
		if (axis < 0 && !mitre(rec.faces[0], rec.faces[1]))
			axis = -2;   // ni plan d'axe, ni onglet : traversee
		const std::tuple<E, E, int> key {a, b, axis};
		if (listed.count(key))
		{
			++seen[key];
			if (rec.kind == MeshDefect::DuplicateFace)
				++seenDuplicates[key];
		}
		else
		{
			++outside;
			++outsideKeys[key];
		}
	}
	for (const auto &[key, count] : outsideKeys)
		ADD_FAILURE() << "recouvrement hors liste : elements " << static_cast<int>(std::get<0>(key)) << " et "
		              << static_cast<int>(std::get<1>(key)) << ", axe " << std::get<2>(key) << " : " << count;
	EXPECT_EQ(outside, 0u);
	for (const auto &[key, name] : listed)
		EXPECT_GE(seen[key], 1u) << name << " absent : elements " << static_cast<int>(std::get<0>(key)) << " et "
		                         << static_cast<int>(std::get<1>(key)) << ", axe " << std::get<2>(key);
	for (const auto &[key, count] : seen)
		std::printf("  %d-%d axe %d : %zu (dont %zu faces confondues)\n", static_cast<int>(std::get<0>(key)),
		            static_cast<int>(std::get<1>(key)), std::get<2>(key), count, seenDuplicates[key]);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (r.counts[kind] != 0)
			std::printf("  %s : %zu\n", MeshDefectName(static_cast<MeshDefect>(kind)), r.counts[kind]);
	RecordProperty("crown_records", std::to_string(records));
	RecordProperty("crown_duplicates", std::to_string(duplicates));
	std::printf("sous-assemblage : %u faces, %zu recouvrements du lot listes, %zu faces confondues\n", sub.GetNFaces(),
	            records, duplicates);
}

// K9 : gardes de mise en place atteintes directement, chacune par son motif. Couronnement : bandes d'onglet croisees
// dans un pronaos de 5 cm (les antes restant hors d'atteinte) ; bout d'onglet sur la face des antes en float seulement
// (le double passe d'un ulp), refuse a la mise en place, alors que BuildGreekEpicranitisPieces leverait le
// std::logic_error de detail::StripMesh. Prolongement : eastSpan non finie ou <= 0, anneau a la face des antes (en
// double, puis en float seulement), fermeture (ecart ecrit depuis kPorchExtensionClosure), pied sur l'architrave des
// porches.
TEST(TEST_cgarchi_classical_cella_crown, the_crown_and_extension_layouts_refuse_their_limit_cases)
{
	const BuiltTemple &t = CrownTemple();
	const EpicranitisSettings &s = *t.settings.epicranitis;
	const CellaLayout &cella = *t.layout.cella;
	const auto crownRefusal = [&](const CellaLayout &c) { return RefusalOf([&] { LayoutGreekEpicranitis(s, c); }); };
	EXPECT_EQ(crownRefusal(cella), "");

	CellaLayout shallow = cella;
	shallow.naos.frontWall = shallow.porchEntablature.frontBack + 0.05;
	EXPECT_EQ(crownRefusal(shallow), "Couronnement : 'epicranitis.bandProjection' croise deux bandes d'onglet d'un porche");

	CellaLayout tight = cella;
	tight.porchEntablature.halfWidth = std::nextafter(cella.naos.outer + s.bandProjection, 10.0);
	ASSERT_TRUE(cella.naos.outer + s.bandProjection < tight.porchEntablature.halfWidth);
	ASSERT_EQ(static_cast<float>(cella.naos.outer + s.bandProjection), static_cast<float>(tight.porchEntablature.halfWidth));
	EXPECT_EQ(crownRefusal(tight),
	          "Couronnement : 'epicranitis.bandProjection' atteint la face exterieure des antes au dos du couronnement");
	EpicranitisLayout forced = LayoutGreekEpicranitis(s, cella);
	forced.porch.halfWidth = tight.porchEntablature.halfWidth;
	EXPECT_THROW(BuildGreekEpicranitisPieces(s, forced), std::logic_error);

	const EpicranitisLayout &crown = *t.layout.epicranitis;
	const PorchExtensionLayout &ext = *t.layout.porchExtension;
	const PorchExtensionSettings e = *t.settings.porchExtension;
	const auto extensionRefusal = [&](double span, double ring, double foot) {
		PorchExtensionSettings x = e;
		x.eastSpan = span;
		return RefusalOf([&] { LayoutGreekPorchExtension(x, crown, ring, foot); });
	};
	EXPECT_EQ(extensionRefusal(e.eastSpan, ext.outer, ext.foot), "");
	for (const double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), 0.0, -e.eastSpan})
		EXPECT_EQ(extensionRefusal(bad, ext.outer, ext.foot),
		          "Prolongement : 'porchExtension.eastSpan' n'est pas une longueur finie > 0") << bad;
	const std::string ring = "Prolongement : 'porchExtension' ne trouve pas la face interieure de l'anneau au-dela de la face "
	                         "exterieure des antes";
	EXPECT_EQ(extensionRefusal(e.eastSpan, crown.porch.halfWidth, ext.foot), ring);
	const double justBeyond = std::nextafter(crown.porch.halfWidth, 10.0);
	ASSERT_EQ(static_cast<float>(justBeyond), static_cast<float>(crown.porch.halfWidth));
	EXPECT_EQ(extensionRefusal(e.eastSpan, justBeyond, ext.foot), ring);
	char closure[32];
	std::snprintf(closure, sizeof closure, "%g", kPorchExtensionClosure);
	EXPECT_EQ(extensionRefusal(e.eastSpan + 2.0 * kPorchExtensionClosure, ext.outer, ext.foot),
	          std::string("Prolongement : 'porchExtension.eastSpan' ne ferme pas l'ecart de la face exterieure des antes a la "
	                      "face interieure de l'anneau a ") + closure + " m pres");
	EXPECT_EQ(extensionRefusal(e.eastSpan, ext.outer, crown.porch.architraveTop),
	          "Prolongement : 'porchExtension' pose l'architrave prolongee au-dessus de l'architrave des porches");
}

// K10 : detail::StripMesh. Les quatre combinaisons axe x sens, d'onglet aux deux bouts : maillage sans defaut, volume
// positif, egal a l'aire du profil fois la longueur moins deux fois son moment en d (bouts d'onglet), saillie du cote
// demande ; refus std::logic_error : profil en papillon (non simple), bouts croises par deux onglets, saillie negative.
TEST(TEST_cgarchi_classical_cella_crown, the_strip_mesh_is_closed_outward_and_refuses_bad_profiles)
{
	using detail::StripAxis;
	using detail::StripMesh;
	using detail::StripOutward;
	const std::vector<std::array<double, 2>> profile = {{0.0, 0.0}, {0.05, 0.0}, {0.05, 0.16}, {0.045, 0.16}, {0.045, 0.2},
	                                                    {0.0, 0.2}};
	// Aire et moment en d du profil (rectangles 0,05 x 0,16 et 0,045 x 0,04).
	const double area = 0.05 * 0.16 + 0.045 * 0.04;
	const double moment = 0.05 * 0.05 / 2.0 * 0.16 + 0.045 * 0.045 / 2.0 * 0.04;
	const double face = 2.0, t0 = -1.0, t1 = 1.0;
	for (const StripAxis axis : {StripAxis::AlongX, StripAxis::AlongY})
		for (const StripOutward outward : {StripOutward::Increasing, StripOutward::Decreasing})
		{
			SCOPED_TRACE(std::to_string(static_cast<int>(axis)) + " / " + std::to_string(static_cast<int>(outward)));
			const std::unique_ptr<Mesh> m = StripMesh(profile, axis, face, outward, t0, true, t1, true);
			const MeshValidationReport r = ValidateMesh(*m);
			for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
				EXPECT_EQ(r.counts[kind], 0u) << MeshDefectName(static_cast<MeshDefect>(kind));
			TempleMeshPart whole;
			whole.vertexCount = m->GetNVertices();
			whole.faceCount = m->GetNFaces();
			EXPECT_NEAR(SignedVolume(*m, whole), area * (t1 - t0) - 2.0 * moment, 1e-6);
			const int other = axis == StripAxis::AlongX ? 1 : 0;
			for (unsigned v = 0; v < m->GetNVertices(); ++v)
			{
				float q[3];
				m->GetVertex(v, q);
				const double d = static_cast<double>(q[other]) - face;
				EXPECT_TRUE(outward == StripOutward::Increasing ? d >= 0.0 : d <= 0.0) << v;
			}
		}
	EXPECT_THROW(StripMesh({{0.0, 0.0}, {0.05, 0.2}, {0.05, 0.0}, {0.0, 0.2}}, StripAxis::AlongX, face,
	                       StripOutward::Increasing, t0, false, t1, false),
	             std::logic_error);
	EXPECT_THROW(StripMesh(profile, StripAxis::AlongX, face, StripOutward::Increasing, 0.0, true, 0.05, true), std::logic_error);
	EXPECT_THROW(StripMesh({{-0.01, 0.0}, {0.05, 0.0}, {0.05, 0.2}, {0.0, 0.2}}, StripAxis::AlongY, face,
	                       StripOutward::Decreasing, t0, false, t1, false),
	             std::logic_error);
}

// K11 : document exterieur et dependances de blocs. MonumentExterior retire la cella et tout bloc qui l'exige :
// cella, porches, interieur, epicranitis, prolongement et plafonds Absent (et non Blocked), maillage du temple
// exterieur (514 846 sommets, 1 016 552 faces). MonumentWithoutPart (Epicranitis) retire aussi le prolongement et
// le plafond est ; MonumentWithoutPart (EastCeiling) ne retire que le plafond est et ses controles. Un drapeau
// `present` contraire a la table MO-P2 du lecteur est refuse par ReportMonumentParts et BuildMonumentMesh.
TEST(TEST_cgarchi_classical_cella_crown, the_exterior_document_drops_every_block_that_requires_the_cella)
{
	using P = MonumentPart;
	const MonumentData &heph = HephaistosMonument();
	const MonumentData exterior = MonumentExterior(heph);
	const auto report = ReportMonumentParts(exterior);
	for (const P part : {P::Cella, P::Porches, P::Interior, P::Epicranitis, P::PorchExtension, P::EastCeiling,
	                     P::FlankCeilings})
		EXPECT_EQ(report[static_cast<std::size_t>(part)].state, MonumentPartState::Absent) << static_cast<int>(part);
	EXPECT_FALSE(exterior.present.eastCeiling || exterior.present.eastCeilingControls);
	for (const P part : {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Pediment})
		EXPECT_EQ(report[static_cast<std::size_t>(part)].state, MonumentPartState::Built) << static_cast<int>(part);
	const MonumentBuild b = BuildMonumentMesh(exterior);
	ASSERT_TRUE(b.mesh);
	EXPECT_EQ(b.mesh->GetNVertices(), 514846u);
	EXPECT_EQ(b.mesh->GetNFaces(), 1016552u);

	const auto crownless = ReportMonumentParts(MonumentWithoutPart(heph, P::Epicranitis));
	EXPECT_EQ(crownless[static_cast<std::size_t>(P::Epicranitis)].state, MonumentPartState::Absent);
	EXPECT_EQ(crownless[static_cast<std::size_t>(P::PorchExtension)].state, MonumentPartState::Absent);
	EXPECT_EQ(crownless[static_cast<std::size_t>(P::EastCeiling)].state, MonumentPartState::Absent);
	const MonumentData ceilingless = MonumentWithoutPart(heph, P::EastCeiling);
	EXPECT_FALSE(ceilingless.present.eastCeiling || ceilingless.present.eastCeilingControls);
	EXPECT_TRUE(ceilingless.present.porchExtension && ceilingless.present.porchExtensionControls);
	EXPECT_EQ(ReportMonumentParts(ceilingless)[static_cast<std::size_t>(P::PorchExtension)].state, MonumentPartState::Built);
	EXPECT_EQ(crownless[static_cast<std::size_t>(P::Interior)].state, MonumentPartState::Built);

	MonumentData orphan = exterior;
	orphan.present.epicranitis = true;
	const std::string expected = "Monument : 'epicranitis' demande le bloc 'cella'";
	EXPECT_EQ(RefusalOf([&] { ReportMonumentParts(orphan); }), expected);
	EXPECT_EQ(RefusalOf([&] { BuildMonumentMesh(orphan); }), expected);
	MonumentData extensionOnly = MonumentWithoutPart(heph, P::Epicranitis);
	extensionOnly.present.porchExtension = true;
	EXPECT_EQ(RefusalOf([&] { ReportMonumentParts(extensionOnly); }), "Monument : 'porchExtension' demande le bloc 'epicranitis'");
	MonumentData ceilingOnly = MonumentWithoutPart(heph, P::PorchExtension);
	EXPECT_FALSE(ceilingOnly.present.eastCeiling);
	ceilingOnly.present.eastCeiling = true;
	EXPECT_EQ(RefusalOf([&] { BuildMonumentMesh(ceilingOnly); }), "Monument : 'eastCeiling' demande le bloc 'porchExtension'");
}
