#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <cgarchi/classical_greek_ceiling.h>
#include <cgarchi/classical_greek_entablature_detail.h>
#include <cgarchi/classical_greek_pediment_detail.h>
#include <cgarchi/classical_greek_prism_detail.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Plafonds du peristyle et couverture de l'Hephaisteion (lot C4, registre
//  CE-32 et suivantes)
// ===========================================================================
//
// Donnee : le document embarque du temple d'Hephaistos (README sections 6.2,
// 7.10 et 7.11). La face interieure de l'entablement du peristyle y est lue
// (19,891 in, deduit [S], decision C4-9) ; la convention EN1 (nu du pied de
// l'architrave) vaut en repli, pour une face manquante (CE-32).
//
// Tolerances :
//   - kFloatTolerance (1e-6 m) : cote d'un sommet float a ~16 m de l'origine
//     (demi-ulp 9,5e-7) contre la valeur calculee en double ;
//   - kPrintedHalfUnit (5e-4 m) : demi-unite du millimetre d'un controle
//     imprime par le README.

using namespace cgarchi::classical;

namespace {

constexpr double kFloatTolerance = 1e-6;
constexpr double kPrintedHalfUnit = 5e-4;

const TempleMeshPart &RingOf (const MonumentBuild &b)
{
	for (const TempleMeshPart &p : b.parts)
		if (p.address.element == TempleElement::Entablature && p.address.side == PeristyleSide::Front)
			return p;
	throw std::logic_error("anneau absent");
}

// Plus petite coordonnee positive des sommets de l'anneau sur l'axe `axis` : la face interieure de ce cote.
float RingInnerOf (const MonumentBuild &b, int axis)
{
	const TempleMeshPart &ring = RingOf(b);
	float inner = std::numeric_limits<float>::max();
	for (unsigned v = ring.firstVertex; v < ring.firstVertex + ring.vertexCount; ++v)
	{
		float q[3];
		b.mesh->GetVertex(v, q);
		if (q[axis] > 0.0f)
			inner = std::min(inner, q[axis]);
	}
	return inner;
}

} // namespace

// PL6 (C4a) : face interieure lue. L'anneau du temple exterieur a sa face interieure a peristyle.halfWidth - innerFace
// en X et peristyle.halfDepth - innerFace en Y (1e-6), et au controle du README, y = +-5,778 (5e-4). EN1 forcee (face
// mise a manquant) contre face lue : memes faces, au memcmp ; seuls des sommets de l'anneau changent, de innerFace -
// pied de l'architrave en X et en Y.
TEST(TEST_cgarchi_classical_ceiling, the_ring_inner_face_is_read_and_en1_is_only_a_fallback)
{
	const MonumentData &heph = HephaistosMonument();
	const MonumentBuild read = BuildMonumentMesh(MonumentExterior(heph));
	MonumentData forcedData = heph;
	forcedData.entablature.innerFace.value.reset();
	forcedData.entablature.innerFace.status = MonumentStatus::Missing;
	forcedData.entablature.innerFace.sources.clear();
	const MonumentBuild forced = BuildMonumentMesh(MonumentExterior(forcedData));
	ASSERT_TRUE(read.mesh);
	ASSERT_TRUE(forced.mesh);

	const float innerX = RingInnerOf(read, 0);
	const float innerY = RingInnerOf(read, 1);
	const double readFace = MonumentEntablatureSettings(heph).innerFace;
	const PeristyleLayout peristyle = LayoutTemple(MonumentTempleSettings(heph)).peristyle;
	EXPECT_NEAR(static_cast<double>(innerX), peristyle.halfWidth - readFace, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(innerY), peristyle.halfDepth - readFace, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(innerX), heph.porchExtensionControls.ringInnerY, kPrintedHalfUnit);

	const double fallbackFace = MonumentEntablatureSettings(forcedData).innerFace;
	EXPECT_EQ(fallbackFace, MonumentLength(heph.entablature.architraveFootProjection, "entablature.architraveFootProjection"));
	EXPECT_NEAR(static_cast<double>(RingInnerOf(forced, 0)) - static_cast<double>(innerX), readFace - fallbackFace,
	            2.0 * kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(RingInnerOf(forced, 1)) - static_cast<double>(innerY), readFace - fallbackFace,
	            2.0 * kFloatTolerance);

	ASSERT_EQ(read.mesh->GetNVertices(), forced.mesh->GetNVertices());
	ASSERT_EQ(read.mesh->GetNFaces(), forced.mesh->GetNFaces());
	bool sameFaces = true;
	for (unsigned f = 0; sameFaces && f < read.mesh->GetNFaces(); ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && read.mesh->GetFaceVertex(f, j) == forced.mesh->GetFaceVertex(f, j);
	EXPECT_TRUE(sameFaces);

	const TempleMeshPart &ring = RingOf(read);
	unsigned moved = 0, movedOutsideRing = 0;
	const float *a = read.mesh->GetVertices().data();
	const float *b = forced.mesh->GetVertices().data();
	for (unsigned v = 0; v < static_cast<unsigned>(read.mesh->GetNVertices()); ++v)
		if (std::memcmp(a + 3 * v, b + 3 * v, 3 * sizeof(float)) != 0)
		{
			++moved;
			if (v < ring.firstVertex || v >= ring.firstVertex + ring.vertexCount)
				++movedOutsideRing;
		}
	EXPECT_EQ(moved, 8u);
	EXPECT_EQ(movedOutsideRing, 0u);
	std::printf("PL6 : face interieure de l'anneau X %.6f, Y %.6f ; EN1 forcee : %u sommets deplaces sur %d\n",
	            static_cast<double>(innerX), static_cast<double>(innerY), moved, read.mesh->GetNVertices());
	RecordProperty("ring_inner_x_m", std::to_string(static_cast<double>(innerX)));
	RecordProperty("en1_moved_vertices", std::to_string(moved));
}

// ===========================================================================
//  Plafond est (lot C4c) : reglages litteraux du README, section 7.11
// ===========================================================================
//
// Le temple est celui du couronnement (lot C3), plus le plafond est sur des
// reglages ecrits ici avec les valeurs du README (section 7.11). Controles :
// expressions de la mise en place a kFloatTolerance, cotes imprimees a
// kPrintedHalfUnit. Volume d'une dalle : boite moins caissons, a
// kSlabVolumeTolerance, majorant de l'effet de l'arrondi en float des sommets
// (aire de la dalle, au plus 10 m2, fois deux demi-ulp a 16 m).

namespace {

constexpr double kSlabVolumeTolerance = 1e-5;
constexpr double kMitreTolerance = 4e-6;

EastCeilingSettings LiteralEastCeiling ()
{
	EastCeilingSettings s;
	s.beamCount = 7;
	s.beamSpacing = 1.417;
	s.beamWidth = 0.525;
	s.beamHeight = 0.525;
	s.beamWestBearing = 0.4995;
	s.endMassifWidth = 0.391;
	s.cornice = {0.232, 2.634 * kMonumentInch, 0.089, 0.4995};
	s.coffers = {0.426, 9.55 * kMonumentInch, 2.6 * kMonumentInch, 0.132};
	s.cofferRows = 2;
	return s;
}

// Reglages du temple du couronnement (lots C1 a C3).
TempleSettings CrownSettings ()
{
	const MonumentData &d = HephaistosMonument();
	TempleSettings s = MonumentTempleSettings(d);
	s.cella = MonumentCellaSettings(d);
	s.cella->porches = MonumentPorchSettings(d);
	s.interior = MonumentInteriorSettings(d);
	s.epicranitis = MonumentEpicranitisSettings(d);
	s.porchExtension = MonumentPorchExtensionSettings(d);
	return s;
}

FlankCeilingSettings LiteralFlankCeilings ()
{
	FlankCeilingSettings s;
	s.beamSpacing = 1.33;
	s.firstBeamOffset = 0.665;
	s.frontBeamCount = 7;
	s.backBeamCount = 8;
	s.beamWidth = 0.525;
	s.beamHeight = 0.525;
	s.peristyleGap = 0.170;
	s.cofferRows = 1;
	return s;
}

TempleSettings CeilingSettings ()
{
	TempleSettings s = CrownSettings();
	s.eastCeiling = LiteralEastCeiling();
	s.flankCeilings = LiteralFlankCeilings();
	return s;
}

struct BuiltTemple
{
	TempleSettings settings;
	TempleLayout layout;
	std::vector<TempleMeshPart> parts;
	std::unique_ptr<Mesh> mesh;
};

const BuiltTemple &CeilingTemple ()
{
	static const BuiltTemple temple = [] {
		BuiltTemple t;
		t.settings = CeilingSettings();
		t.mesh = BuildTempleMesh(t.settings, &t.layout, &t.parts);
		return t;
	}();
	return temple;
}

bool IsCeiling (TempleElement e)
{
	return e == TempleElement::CeilingBeam || e == TempleElement::PorchCornice || e == TempleElement::PorchBand ||
	       e == TempleElement::CeilingSlab;
}

const TempleMeshPart &PartOf (const BuiltTemple &t, TempleElement element, PeristyleSide side, int index = 0)
{
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == element && p.address.side == side && p.address.index == index)
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

std::string RefusalOf (const std::function<void ()> &call)
{
	try
	{
		call();
	}
	catch (const std::invalid_argument &e)
	{
		return e.what();
	}
	return std::string();
}

bool Starts (const std::string &s, const std::string &prefix)
{
	return s.rfind(prefix, 0) == 0;
}

// Coordonnees distinctes, sur l'axe `axis`, des sommets d'une piece a la cote float `z`, hors de ses faces de bord
// verticales (sommets strictement a l'interieur de sa boite en X et en Y).
std::set<float> CoordinatesAt (const Mesh &m, const TempleMeshPart &p, int axis, float z)
{
	const Box3 box = BoxOf(m, p);
	std::set<float> values;
	for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		if (q[2] == z && q[0] > box.lo[0] && q[0] < box.hi[0] && q[1] > box.lo[1] && q[1] < box.hi[1])
			values.insert(q[axis]);
	}
	return values;
}

} // namespace

// PL1 : les 33 pieces du plafond est suivent celles du prolongement est, dans l'ordre de BuildGreekEastCeilingPieces,
// puis les 58 des flancs, dans l'ordre de BuildGreekFlankCeilingPieces ; chacune, isolee, est fermee, sans defaut, de
// volume positif ; les dalles sont de genre 0 (F = 2V - 4) et de volume boite moins caissons. Le comble passe en 9
// tranches (poches de l'interieur, du pteron est, du porche ouest et des deux flancs). Le total des faces est celui
// de CountTempleMeshFaces.
TEST(TEST_cgarchi_classical_ceiling, the_east_ceiling_pieces_are_closed_and_counted)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const EastCeilingLayout &l = *t.layout.eastCeiling;
	const EastCeilingSettings &s = *t.settings.eastCeiling;
	const FlankCeilingLayout &fl = *t.layout.flankCeilings;
	const FlankCeilingSettings &fs = *t.settings.flankCeilings;
	std::size_t first = t.parts.size();
	for (std::size_t k = 0; k < t.parts.size(); ++k)
		if (IsCeiling(t.parts[k].address.element))
		{
			first = std::min(first, k);
		}
	ASSERT_EQ(t.parts.size() - first, 33u + 58u);
	EXPECT_EQ(t.parts[first - 1].address.element, TempleElement::ExtensionMoulding);
	using E = TempleElement;
	using S = PeristyleSide;
	const std::vector<std::tuple<E, S, int>> order = [] {
		std::vector<std::tuple<E, S, int>> o;
		for (int k = 0; k < 7; ++k)
			o.emplace_back(E::CeilingBeam, S::Front, k);
		for (int k = 0; k < 8; ++k)
			o.emplace_back(E::PorchCornice, S::Front, k);
		for (int k = 0; k < 8; ++k)
			o.emplace_back(E::PorchBand, S::Front, k);
		o.emplace_back(E::PorchCornice, S::Back, 0);
		o.emplace_back(E::PorchBand, S::Back, 0);
		for (int k = 0; k < 8; ++k)
			o.emplace_back(E::CeilingSlab, S::Front, k);
		for (const S side : {S::Left, S::Right})
		{
			for (int k = 0; k < 15; ++k)
				o.emplace_back(E::CeilingBeam, side, k);
			for (int k = 0; k < 14; ++k)
				o.emplace_back(E::CeilingSlab, side, k);
		}
		return o;
	}();
	unsigned vertices = 0, faces = 0, flankVertices = 0, flankFaces = 0;
	const double coffer = (s.coffers.opening * s.coffers.opening +
	                       (s.coffers.opening - 2.0 * s.coffers.frame) * (s.coffers.opening - 2.0 * s.coffers.frame)) *
	                      s.coffers.depth / 2.0;
	for (std::size_t k = 0; k < order.size(); ++k)
	{
		const TempleMeshPart &p = t.parts[first + k];
		SCOPED_TRACE(k);
		EXPECT_EQ(p.address.element, std::get<0>(order[k]));
		EXPECT_EQ(p.address.side, std::get<1>(order[k]));
		EXPECT_EQ(p.address.index, std::get<2>(order[k]));
		const std::unique_ptr<Mesh> piece = PieceMesh(m, p);
		const MeshValidationReport r = ValidateMesh(*piece);
		EXPECT_TRUE(r.IsValid());
		EXPECT_EQ(r.components, 1u);
		EXPECT_GT(r.signedVolume, 0.0);
		const bool flank = p.address.side == S::Left || p.address.side == S::Right;
		(flank ? flankVertices : vertices) += p.vertexCount;
		(flank ? flankFaces : faces) += p.faceCount;
		if (p.address.element == E::CeilingSlab && flank)
		{
			EXPECT_EQ(p.faceCount, 2u * p.vertexCount - 4u);
			const std::size_t bay = static_cast<std::size_t>(p.address.index);
			const double y0 = fl.beamAxes[bay] + fs.beamWidth / 2.0, y1 = fl.beamAxes[bay + 1] - fs.beamWidth / 2.0;
			const double box = (fl.peristyleEnd - fl.wallEnd) * (y1 - y0) * (fl.beamTop - fl.slabFoot);
			EXPECT_NEAR(r.signedVolume, box - fs.cofferRows * fl.cofferColumns * coffer, kSlabVolumeTolerance);
		}
		else if (p.address.element == E::CeilingSlab)
		{
			EXPECT_EQ(p.faceCount, 2u * p.vertexCount - 4u);
			const std::array<double, 2> &bay = l.bays[static_cast<std::size_t>(p.address.index)];
			const double box = (bay[1] - bay[0]) * (l.westEnd - l.eastEnd) * (l.beamTop - l.bandTop);
			EXPECT_NEAR(r.signedVolume, box - s.cofferRows * l.cofferColumns * coffer, kSlabVolumeTolerance);
		}
	}
	const TempleMeshPart &slab = PartOf(t, E::CeilingSlab, S::Front, 1);
	std::size_t roofSlices = 0;
	unsigned roofVertices = 0, roofFaces = 0;
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == E::Roof)
		{
			EXPECT_EQ(p.address.index, static_cast<int>(roofSlices));
			++roofSlices;
			roofVertices += p.vertexCount;
			roofFaces += p.faceCount;
		}
	EXPECT_EQ(roofSlices, 9u);
	EXPECT_EQ(m.GetNFaces(), CountTempleMeshFaces(t.settings));
	const TempleMeshPart &flankSlab = PartOf(t, E::CeilingSlab, S::Left, 1);
	std::printf("PL1 : plafond est %u sommets, %u faces ; dalle %u / %u ; flancs %u / %u, dalle %u / %u ; comble %zu "
	            "tranches, %u / %u ; temple %d / %d\n",
	            vertices, faces, slab.vertexCount, slab.faceCount, flankVertices, flankFaces, flankSlab.vertexCount,
	            flankSlab.faceCount, roofSlices, roofVertices, roofFaces, m.GetNVertices(), m.GetNFaces());
	RecordProperty("temple_vertices", std::to_string(m.GetNVertices()));
	RecordProperty("temple_faces", std::to_string(m.GetNFaces()));
}

// PL2 : controles du README (section 7.11, regle R13) au maillage, chacun lu dans le document (eastCeilingControls)
// et confronte a la cote mesuree, a sa demi-unite imprimee : axe de la poutre extreme, dessous et dessus des poutres,
// bouts ouest et est, longueur, travee courante, plan du massif, travee de rive, dessus de la corniche et du bandeau,
// epaisseur de la dalle, fond plein, course et marge des caissons. Les cotes du maillage sont aussi comparees, a 1e-6,
// a la mise en place (meme expression : controle de coherence, non independant) ; le bout est l'est, au bit pres, a la
// face interieure de l'anneau mesuree sur l'anneau (independant). Caissons retrouves sur la dalle : 2 rangs, 9 le long
// de la course, cotes des couches lues sur ses sommets.
TEST(TEST_cgarchi_classical_ceiling, the_east_ceiling_meets_the_controls_of_the_readme)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const EastCeilingLayout &l = *t.layout.eastCeiling;
	const EastCeilingSettings &s = *t.settings.eastCeiling;
	const MonumentEastCeilingControls &c = HephaistosMonument().eastCeilingControls;
	const double z0 = t.layout.krepis.stylobateTop;
	using E = TempleElement;
	using S = PeristyleSide;
	const auto d = [](float v) { return static_cast<double>(v); };
	const Box3 outer = BoxOf(m, PartOf(t, E::CeilingBeam, S::Front, 6));
	const Box3 inner = BoxOf(m, PartOf(t, E::CeilingBeam, S::Front, 5));
	const double axis = (d(outer.lo[0]) + d(outer.hi[0])) / 2.0;
	EXPECT_NEAR(axis, l.beamAxes[6], kFloatTolerance);
	EXPECT_NEAR(axis, c.beamOuterAxisY, kPrintedHalfUnit);
	EXPECT_NEAR(d(outer.lo[2]), l.beamFoot, kFloatTolerance);
	EXPECT_NEAR(d(outer.lo[2]) - z0, c.beamBottomZ, kPrintedHalfUnit);
	EXPECT_NEAR(d(outer.hi[2]), l.beamTop, kFloatTolerance);
	EXPECT_NEAR(d(outer.hi[2]) - z0, c.beamTopZ, kPrintedHalfUnit);
	EXPECT_NEAR(d(outer.hi[1]), l.westEnd, kFloatTolerance);
	EXPECT_NEAR(-d(outer.hi[1]), c.beamWestEndX, kPrintedHalfUnit);
	EXPECT_NEAR(-d(outer.lo[1]), c.beamEastEndX, kPrintedHalfUnit);
	EXPECT_NEAR(d(outer.hi[1]) - d(outer.lo[1]), c.beamLength, kPrintedHalfUnit);
	float ringInnerY = std::numeric_limits<float>::max();
	const TempleMeshPart &ring = PartOf(t, E::Entablature, S::Front);
	for (unsigned v = ring.firstVertex; v < ring.firstVertex + ring.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		if (q[1] > 0.0f)
			ringInnerY = std::min(ringInnerY, q[1]);
	}
	EXPECT_EQ(outer.lo[1], -ringInnerY);
	EXPECT_NEAR(d(outer.lo[0]) - d(inner.hi[0]), s.beamSpacing - s.beamWidth, 2.0 * kFloatTolerance);
	EXPECT_NEAR(d(outer.lo[0]) - d(inner.hi[0]), c.clearBay, kPrintedHalfUnit);
	const Box3 edge = BoxOf(m, PartOf(t, E::CeilingSlab, S::Front, 7));
	EXPECT_NEAR(d(edge.hi[0]), l.massif, kFloatTolerance);
	EXPECT_NEAR(d(edge.hi[0]), c.massifFaceY, kPrintedHalfUnit);
	EXPECT_NEAR(d(edge.hi[0]) - d(outer.hi[0]), c.edgeBay, kPrintedHalfUnit);
	const Box3 cornice = BoxOf(m, PartOf(t, E::PorchCornice, S::Front, 3));
	const Box3 band = BoxOf(m, PartOf(t, E::PorchBand, S::Front, 3));
	EXPECT_NEAR(d(cornice.hi[2]) - z0, c.corniceTopZ, kPrintedHalfUnit);
	EXPECT_NEAR(d(band.hi[2]), l.bandTop, kFloatTolerance);
	EXPECT_NEAR(d(band.hi[2]) - z0, c.bandTopZ, kPrintedHalfUnit);
	EXPECT_NEAR(d(cornice.lo[1]), l.crown.porch.front - s.cornice.projection, kFloatTolerance);

	// Couches de la dalle lues sur ses sommets : pied, mi-profondeur, fond des caissons, dessus.
	const TempleMeshPart &slab = PartOf(t, E::CeilingSlab, S::Front, 1);
	std::set<float> layers;
	for (unsigned v = slab.firstVertex; v < slab.firstVertex + slab.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		layers.insert(q[2]);
	}
	ASSERT_EQ(layers.size(), 4u);
	const std::vector<float> z(layers.begin(), layers.end());
	EXPECT_NEAR(d(z[3]) - d(z[0]), c.slabThickness, kPrintedHalfUnit);
	EXPECT_NEAR(d(z[3]) - d(z[2]), c.cofferFloor, kPrintedHalfUnit);
	EXPECT_NEAR(d(z[2]), l.bandTop + s.coffers.depth, kFloatTolerance);

	// Caissons : a la cote des fonds, les seuls sommets interieurs sont les coins des fonds ; deux lignes par caisson.
	const std::set<float> ys = CoordinatesAt(m, slab, 1, z[2]), xs = CoordinatesAt(m, slab, 0, z[2]);
	EXPECT_EQ(ys.size(), 2u * 9u);
	EXPECT_EQ(xs.size(), 2u * 2u);
	const double run = d(band.lo[1]) - d(outer.lo[1]);
	EXPECT_NEAR(run, l.crown.porch.front - l.eastEnd, kFloatTolerance);
	EXPECT_NEAR(run, c.cofferRun, kPrintedHalfUnit);
	EXPECT_EQ(static_cast<int>(std::floor(run / s.coffers.pitch)), 9);
	const double fond = s.coffers.opening - 2.0 * s.coffers.frame;
	const double margin = d(*ys.begin()) - l.eastEnd - (s.coffers.pitch - fond) / 2.0;
	EXPECT_NEAR(margin, (run - 9 * s.coffers.pitch) / 2.0, 2.0 * kFloatTolerance);
	EXPECT_NEAR(margin, c.cofferMargin, kPrintedHalfUnit);
	RecordProperty("coffer_margin_m", std::to_string(margin));
}

// PL4 : tout sommet des plafonds au-dessus de la base du comble est dans une poche (boites en float, bords compris) ;
// la poche des flancs est bornee a |X| <= peristyleEnd, et aucun sommet d'une piece des flancs n'est au-dela ; le
// plus haut est float (beamTop), a 8,090 du stylobate ; les pieces du porche ouest montent a float (bandTop), 7,886.
TEST(TEST_cgarchi_classical_ceiling, every_east_ceiling_vertex_above_the_roof_base_is_in_a_pocket)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const EastCeilingLayout &l = *t.layout.eastCeiling;
	float zBase = std::numeric_limits<float>::max();
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == TempleElement::Roof)
			zBase = std::min(zBase, BoxOf(m, p).lo[2]);
	const FlankCeilingLayout &fl = *t.layout.flankCeilings;
	std::vector<RoofPocket> pockets = {l.eastPocket, l.westPocket, fl.pockets[0], fl.pockets[1]};
	// Bout cote peristyle recalcule depuis la face interieure de l'anneau et le jeu, par l'expression de
	// SweepRectangularRing, independamment de la mise en place des flancs.
	const double expectedEnd = (t.layout.peristyle.halfWidth + (-t.settings.entablature->innerFace)) -
	                           t.settings.flankCeilings->peristyleGap;
	EXPECT_EQ(fl.pockets[0].x0, -expectedEnd);
	EXPECT_EQ(fl.pockets[1].x1, expectedEnd);
	float flankReach = 0.0f;
	if (t.layout.interior)
		pockets.push_back(t.layout.interior->pocket);
	const auto inside = [&](const float *q) {
		for (const RoofPocket &p : pockets)
			if (q[0] >= static_cast<float>(p.x0) && q[0] <= static_cast<float>(p.x1) && q[1] >= static_cast<float>(p.y0) &&
			    q[1] <= static_cast<float>(p.y1) && q[2] <= static_cast<float>(p.zTop))
				return true;
		return false;
	};
	float top = std::numeric_limits<float>::lowest(), westTop = top;
	std::size_t above = 0, outside = 0;
	for (const TempleMeshPart &p : t.parts)
	{
		if (!IsCeiling(p.address.element))
			continue;
		for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
		{
			float q[3];
			m.GetVertex(v, q);
			top = std::max(top, q[2]);
			if (p.address.side == PeristyleSide::Back)
				westTop = std::max(westTop, q[2]);
			if (p.address.side == PeristyleSide::Left || p.address.side == PeristyleSide::Right)
				flankReach = std::max(flankReach, std::fabs(q[0]));
			if (q[2] > zBase)
			{
				++above;
				outside += inside(q) ? 0 : 1;
			}
		}
	}
	EXPECT_GT(above, 0u);
	EXPECT_EQ(outside, 0u);
	EXPECT_EQ(flankReach, static_cast<float>(expectedEnd));
	EXPECT_EQ(top, static_cast<float>(l.beamTop));
	EXPECT_NEAR(static_cast<double>(top) - t.layout.krepis.stylobateTop, 8.090, kPrintedHalfUnit);
	EXPECT_EQ(westTop, static_cast<float>(l.bandTop));
	EXPECT_NEAR(static_cast<double>(westTop) - t.layout.krepis.stylobateTop, 7.886, kPrintedHalfUnit);
}

// PL5 : coplanarites au bit pres : pied des poutres et des corniches au dessus du couronnement de la frise ; dessus
// des poutres au plafond de la poche est (le plus haut sommet du comble a cet endroit) ; dessus des dalles au dessus
// des poutres ; dessous des dalles au dessus des bandeaux.
TEST(TEST_cgarchi_classical_ceiling, the_east_ceiling_pieces_are_coplanar_to_the_bit)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	using E = TempleElement;
	using S = PeristyleSide;
	const Box3 crown = BoxOf(m, PartOf(t, E::PorchCrown, S::Front));
	const Box3 westCrown = BoxOf(m, PartOf(t, E::PorchCrown, S::Back));
	const float pocketTop = static_cast<float>(t.layout.eastCeiling->eastPocket.zTop);
	bool roofAtPocketTop = false;
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == E::Roof)
			for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
			{
				float q[3];
				m.GetVertex(v, q);
				roofAtPocketTop = roofAtPocketTop || q[2] == pocketTop;
			}
	EXPECT_TRUE(roofAtPocketTop);
	for (int k = 0; k < 7; ++k)
	{
		const Box3 beam = BoxOf(m, PartOf(t, E::CeilingBeam, S::Front, k));
		EXPECT_EQ(beam.lo[2], crown.hi[2]) << k;
		EXPECT_EQ(beam.hi[2], pocketTop) << k;
	}
	for (int k = 0; k < 8; ++k)
	{
		const Box3 cornice = BoxOf(m, PartOf(t, E::PorchCornice, S::Front, k));
		const Box3 band = BoxOf(m, PartOf(t, E::PorchBand, S::Front, k));
		const Box3 slab = BoxOf(m, PartOf(t, E::CeilingSlab, S::Front, k));
		EXPECT_EQ(cornice.lo[2], crown.hi[2]) << k;
		EXPECT_EQ(band.lo[2], cornice.hi[2]) << k;
		EXPECT_EQ(slab.lo[2], band.hi[2]) << k;
		EXPECT_EQ(slab.hi[2], pocketTop) << k;
	}
	EXPECT_EQ(BoxOf(m, PartOf(t, E::PorchCornice, S::Back)).lo[2], westCrown.hi[2]);
	EXPECT_EQ(BoxOf(m, PartOf(t, E::PorchBand, S::Back)).hi[2], static_cast<float>(t.layout.eastCeiling->westPocket.zTop));
}

// PL7 : refus nommes, a la mise en place du temple. Plafond est : les gardes 1 a 9 de LayoutGreekEastCeiling (la 10e
// en PL9) ; temple : raffinement, sans prolongement, poche est contre un pan (beamHeight de 0,6 m : dessus a 8,165 sur
// le plan du massif, ou le pan est a 8,147), admise sans fronton ; plafonds des flancs : raffinement, sans plafond
// est, puis sans mise en place.
TEST(TEST_cgarchi_classical_ceiling, a_wrong_east_ceiling_is_refused_by_name)
{
	const TempleSettings good = CeilingSettings();
	ASSERT_EQ(RefusalOf([&] { ValidateTempleSettings(good); }), "");
	const auto refusal = [&](const std::function<void (EastCeilingSettings &)> &edit) {
		TempleSettings s = good;
		edit(*s.eastCeiling);
		return RefusalOf([&] { ValidateTempleSettings(s); });
	};
	const std::string head = "Plafond est : 'eastCeiling.";
	const std::vector<std::tuple<const char *, std::function<void (EastCeilingSettings &)>, const char *>> cases = {
		{"beamWidth", [](EastCeilingSettings &e) { e.beamWidth = -1.0; }, "n'est pas une longueur finie"},
		{"cofferFrame", [](EastCeilingSettings &e) { e.coffers.frame = std::numeric_limits<double>::infinity(); },
		 "n'est pas une longueur finie"},
		{"beamCount", [](EastCeilingSettings &e) { e.beamCount = 0; }, "doit valoir au moins 1"},
		{"cofferRows", [](EastCeilingSettings &e) { e.cofferRows = 0; }, "doit valoir au moins 1"},
		{"beamSpacing", [](EastCeilingSettings &e) { e.beamSpacing = 0.5; }, "ne depasse pas la largeur"},
		{"beamSpacing", [](EastCeilingSettings &e) { e.beamSpacing = 1.75; }, "porte la face exterieure"},
		{"endMassifWidth", [](EastCeilingSettings &e) { e.beamCount = 1; e.endMassifWidth = 1.9; }, "porte le plan du massif"},
		{"beamWestBearing", [](EastCeilingSettings &e) { e.beamWestBearing = 1.2; }, "depasse l'epaisseur"},
		{"corniceDepth", [](EastCeilingSettings &e) { e.cornice.depth = 1.2; }, "depasse l'epaisseur"},
		{"beamHeight", [](EastCeilingSettings &e) { e.beamHeight = 0.3; }, "ne depasse pas la corniche"},
		{"cofferDepth", [](EastCeilingSettings &e) { e.coffers.depth = 0.21; }, "atteint l'epaisseur"},
		{"cofferOpening", [](EastCeilingSettings &e) { e.coffers.opening = 0.43; }, "atteint le pas"},
		{"cofferFrame", [](EastCeilingSettings &e) { e.coffers.frame = 0.13; }, "deux fois"},
		{"cofferRows", [](EastCeilingSettings &e) { e.cofferRows = 3; }, "ne tient pas"},
	};
	for (const auto &[field, edit, rule] : cases)
	{
		SCOPED_TRACE(field);
		const std::string what = refusal(edit);
		EXPECT_TRUE(Starts(what, head + field + "' " + rule)) << what;
	}

	TempleSettings refined = MonumentTempleSettings(HephaistosMonument());
	refined.refinement = TempleRefinementSettings{};
	refined.eastCeiling = LiteralEastCeiling();
	EXPECT_TRUE(Starts(RefusalOf([&] { ValidateTempleSettings(refined); }), "Temple : 'eastCeiling' et 'refinement'"));
	TempleSettings bare = CrownSettings();
	bare.porchExtension.reset();
	bare.eastCeiling = LiteralEastCeiling();
	EXPECT_TRUE(Starts(RefusalOf([&] { ValidateTempleSettings(bare); }), "Temple : 'eastCeiling' demande le prolongement est"));
	TempleSettings high = good;
	high.eastCeiling->beamHeight = 0.6;
	EXPECT_EQ(RefusalOf([&] { ValidateTempleSettings(high); }),
	          "Temple : 'eastCeiling.beamHeight' met le dessus des pieces au contact ou au-dessus d'un pan du comble");
	high.pediment.reset();
	EXPECT_EQ(RefusalOf([&] { ValidateTempleSettings(high); }), "");

	TempleSettings flankRefined = MonumentTempleSettings(HephaistosMonument());
	flankRefined.refinement = TempleRefinementSettings{};
	flankRefined.flankCeilings = FlankCeilingSettings{};
	EXPECT_TRUE(Starts(RefusalOf([&] { ValidateTempleSettings(flankRefined); }), "Temple : 'flankCeilings' et 'refinement'"));
	TempleSettings flankAlone = CrownSettings();
	flankAlone.flankCeilings = FlankCeilingSettings{};
	EXPECT_TRUE(Starts(RefusalOf([&] { ValidateTempleSettings(flankAlone); }), "Temple : 'flankCeilings' demande le plafond est"));
	TempleSettings flankEmpty = good;
	flankEmpty.flankCeilings = FlankCeilingSettings{};
	EXPECT_TRUE(Starts(RefusalOf([&] { ValidateTempleSettings(flankEmpty); }),
	                   "Plafonds des flancs : 'flankCeilings.beamSpacing' n'est pas une longueur finie"));
}

// PL8 : recouvrements en liste fermee (comme K8) sur un sous-assemblage : anneau, tranches du comble, epicranitis,
// couronnement et prolongement est, toutes les poutres, corniches et bandeaux, deux dalles est (de rive et
// courante) et deux dalles par flanc. Chaque recouvrement est une paire de faces dans un meme plan d'axe, de deux
// elements d'une paire listee ; chaque contact liste est present ; aucune traversee ; aucun contact entre les pieces
// des flancs et l'anneau (jeu de 0,170 m, CE-42).
TEST(TEST_cgarchi_classical_ceiling, the_only_overlaps_of_the_east_ceiling_are_its_listed_contacts)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	using E = TempleElement;
	const std::set<E> crown = {E::EpicranitisCourse, E::PorchCrown, E::CrownMoulding, E::ExtensionArchitrave,
	                           E::ExtensionFrieze, E::ExtensionCrown, E::ExtensionMoulding};
	std::vector<std::size_t> chosen;
	for (std::size_t k = 0; k < t.parts.size(); ++k)
	{
		const TempleAddress &a = t.parts[k].address;
		const bool slab = a.element == E::CeilingSlab && (a.index == 0 || a.index == 1);
		if (a.element == E::Entablature || a.element == E::Roof || crown.count(a.element) ||
		    (IsCeiling(a.element) && a.element != E::CeilingSlab) || slab)
			chosen.push_back(k);
	}
	std::vector<float> vertices;
	std::vector<unsigned> faces;
	std::vector<E> owner;
	std::vector<bool> flankOwner;
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
			flankOwner.push_back(IsCeiling(part.address.element) &&
			                     (part.address.side == PeristyleSide::Left || part.address.side == PeristyleSide::Right));
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
	// Contacts du plafond est, par paire d'elements (ordre de l'enumeration) et axe du plan commun.
	const std::map<std::tuple<E, E, int>, const char *> listed = {
		{{E::Entablature, E::CeilingBeam, 1}, "bout est des poutres sur la face interieure de l'anneau"},
		{{E::Roof, E::CeilingBeam, 0}, "bouts des poutres des flancs aux parois de leurs poches"},
		{{E::Roof, E::CeilingBeam, 1}, "bouts des poutres aux parois de la poche est"},
		{{E::EpicranitisCourse, E::CeilingBeam, 2}, "poutres des flancs sur l'epicranitis"},
		{{E::Roof, E::CeilingBeam, 2}, "dessus des poutres au plafond de la poche est"},
		{{E::Roof, E::PorchCornice, 0}, "corniche de rive au plan du massif, paroi de la poche"},
		{{E::Roof, E::PorchCornice, 1}, "corniches aux parois des poches"},
		{{E::Roof, E::PorchBand, 0}, "bandeaux aux parois des poches"},
		{{E::Roof, E::PorchBand, 1}, "bandeaux aux parois des poches"},
		{{E::Roof, E::PorchBand, 2}, "bandeau ouest au plafond de sa poche"},
		{{E::Roof, E::CeilingSlab, 0}, "dalle de rive au plan du massif"},
		{{E::Roof, E::CeilingSlab, 1}, "bouts des dalles aux parois de la poche est"},
		{{E::Roof, E::CeilingSlab, 2}, "dessus des dalles au plafond de la poche est"},
		{{E::PorchCrown, E::CeilingBeam, 2}, "poutres sur le couronnement de la frise"},
		{{E::PorchCrown, E::PorchCornice, 2}, "corniches sur le couronnement de la frise"},
		{{E::CrownMoulding, E::CeilingBeam, 2}, "poutres sur le listel"},
		{{E::CrownMoulding, E::PorchCornice, 2}, "corniches sur le listel"},
		{{E::ExtensionCrown, E::CeilingBeam, 2}, "poutres extremes sur le prolongement"},
		{{E::ExtensionCrown, E::PorchCornice, 2}, "corniches de rive sur le prolongement"},
		{{E::ExtensionMoulding, E::CeilingBeam, 2}, "poutres extremes sur le listel du prolongement"},
		{{E::ExtensionMoulding, E::PorchCornice, 2}, "corniches de rive sur le listel du prolongement"},
		{{E::CeilingBeam, E::PorchCornice, 0}, "corniches contre les faces des poutres"},
		{{E::CeilingBeam, E::PorchBand, 0}, "bandeaux contre les faces des poutres"},
		{{E::CeilingBeam, E::CeilingSlab, 0}, "dalles est contre les faces des poutres"},
		{{E::CeilingBeam, E::CeilingSlab, 1}, "dalles des flancs contre les faces des poutres"},
		{{E::PorchCornice, E::PorchBand, 2}, "bandeaux sur les corniches"},
		{{E::PorchBand, E::CeilingSlab, 2}, "dalles sur les bandeaux"},
	};
	std::map<std::tuple<E, E, int>, std::size_t> seen, outsideKeys;
	std::size_t records = 0, outside = 0, flankRing = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection && rec.kind != MeshDefect::DuplicateFace)
			continue;
		ASSERT_EQ(rec.faces.size(), 2u);
		E a = owner[rec.faces[0]], b = owner[rec.faces[1]];
		if (!IsCeiling(a) && !IsCeiling(b))
			continue;
		if ((flankOwner[rec.faces[0]] && b == E::Entablature) || (flankOwner[rec.faces[1]] && a == E::Entablature))
			++flankRing;
		++records;
		if (static_cast<int>(a) > static_cast<int>(b))
			std::swap(a, b);
		int axis = coplanarAxis(rec.faces[0], rec.faces[1]);
		if (axis < 0 && !mitre(rec.faces[0], rec.faces[1]))
			axis = -2;
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
	EXPECT_EQ(outside, 0u);
	EXPECT_EQ(flankRing, 0u);
	for (const auto &[key, name] : listed)
		EXPECT_GE(seen[key], 1u) << name << " absent : elements " << static_cast<int>(std::get<0>(key)) << " et "
		                         << static_cast<int>(std::get<1>(key)) << ", axe " << std::get<2>(key);
	std::printf("PL8 : sous-assemblage de %u faces, %zu recouvrements des plafonds, tous listes : %s\n", sub.GetNFaces(),
	            records, outside == 0 ? "oui" : "non");
}

// PL9 : gardes atteintes directement. Face exterieure de la poutre extreme au plan du massif en float seulement (le
// double passe), refusee ; sans ce refus, la travee de rive, vide en float, ferait lever GridSolidMesh. Course trop
// courte pour un caisson ('cofferPitch'), que la mise en place du temple n'atteint pas sur le temple du README.
TEST(TEST_cgarchi_classical_ceiling, the_east_ceiling_layout_refuses_its_limit_cases_in_float)
{
	const BuiltTemple &t = CeilingTemple();
	const EpicranitisLayout &crown = *t.layout.epicranitis;
	const EastCeilingSettings s = LiteralEastCeiling();
	const double depth = -t.layout.eastCeiling->eastEnd;
	const double face = t.layout.eastCeiling->beamAxes.back() + s.beamWidth / 2.0;
	double ring = face + s.endMassifWidth;
	while (!(ring - s.endMassifWidth > face))
		ring = std::nextafter(ring, 100.0);
	ASSERT_GT(ring - s.endMassifWidth, face);
	ASSERT_EQ(static_cast<float>(ring - s.endMassifWidth), static_cast<float>(face));
	const std::string what = RefusalOf([&] { LayoutGreekEastCeiling(s, crown, ring, depth); });
	EXPECT_TRUE(Starts(what, "Plafond est : 'eastCeiling.beamSpacing' porte la face exterieure")) << what;

	const double shortDepth = -crown.porch.front + 0.3;
	EXPECT_TRUE(Starts(RefusalOf([&] { LayoutGreekEastCeiling(s, crown, t.layout.eastCeiling->massif + s.endMassifWidth, shortDepth); }),
	                   "Plafond est : 'eastCeiling.cofferPitch' ne laisse aucun caisson"));
	EXPECT_EQ(RefusalOf([&] { LayoutGreekEastCeiling(s, crown, t.layout.eastCeiling->massif + s.endMassifWidth, depth); }), "");

	// Grille des caissons et couches de la dalle, valides en double, confondues en float ; saillie de la corniche au-dela
	// de la longueur libre ; poche ouest contre un pan (corniche de 0,9 m, dessus du bandeau a 8,554 contre un pan a
	// 8,50 au droit des antes), refusee avant la poche est.
	const auto refusal = [&](const std::function<void (EastCeilingSettings &)> &change) {
		TempleSettings settings = CeilingSettings();
		change(*settings.eastCeiling);
		return RefusalOf([&] { ValidateTempleSettings(settings); });
	};
	EXPECT_TRUE(Starts(refusal([](EastCeilingSettings &e) { e.coffers.frame = 1e-9; }),
	                   "Plafond est : 'eastCeiling.cofferFrame' confond deux lignes de la grille"));
	EXPECT_TRUE(Starts(refusal([](EastCeilingSettings &e) { e.coffers.depth = 0.204 - 1e-9; }),
	                   "Plafond est : 'eastCeiling.cofferDepth' atteint l'epaisseur de la dalle"));
	EXPECT_TRUE(Starts(refusal([](EastCeilingSettings &e) { e.cornice.projection = 5.0; }),
	                   "Plafond est : 'eastCeiling.corniceProjection' atteint la longueur libre"));
	EXPECT_EQ(refusal([](EastCeilingSettings &e) { e.cornice.height = 0.9; e.beamHeight = 1.2; }),
	          "Temple : 'eastCeiling.bandHeight' met le dessus de la corniche et du bandeau du porche ouest au contact ou "
	          "au-dessus d'un pan du comble");
}

// PL10 : GridSolidMesh. Une cellule : boite de 8 sommets et 12 triangles ; anneau 3 x 3 (centre vide) : genre 1,
// F = 2V ; cellule a cuvette (genre 0) : F = 2V - 4 ; chaque solide ferme, oriente vers l'exterieur (volume des
// cellules occupees). Refus : contact par une arete, par un sommet (deux cellules occupees, ou deux cellules vides
// d'un bloc 2 x 2 x 2 dont les six autres sont occupees), lignes confondues en float, non croissantes.
TEST(TEST_cgarchi_classical_ceiling, the_grid_solid_is_closed_outward_and_refuses_bad_grids)
{
	using detail::GridSolidMesh;
	const auto all = [](std::size_t, std::size_t, std::size_t) { return true; };
	const std::unique_ptr<Mesh> cell = GridSolidMesh({0.0, 1.0}, {0.0, 2.0}, {0.0, 3.0}, all);
	EXPECT_EQ(cell->GetNVertices(), 8);
	EXPECT_EQ(cell->GetNFaces(), 12);
	const MeshValidationReport rc = ValidateMesh(*cell);
	EXPECT_TRUE(rc.IsValid());
	EXPECT_NEAR(rc.signedVolume, 6.0, 1e-12);

	const std::vector<double> l3 = {0.0, 1.0, 2.0, 3.0};
	const std::unique_ptr<Mesh> ring = GridSolidMesh(l3, l3, {0.0, 1.0}, [](std::size_t i, std::size_t j, std::size_t) {
		return !(i == 1 && j == 1);
	});
	const MeshValidationReport rr = ValidateMesh(*ring);
	EXPECT_TRUE(rr.IsValid());
	EXPECT_EQ(ring->GetNFaces(), 2 * ring->GetNVertices());
	EXPECT_NEAR(rr.signedVolume, 8.0, 1e-12);

	const std::unique_ptr<Mesh> cup = GridSolidMesh(l3, l3, {0.0, 1.0, 2.0}, [](std::size_t i, std::size_t j, std::size_t k) {
		return !(k == 0 && i == 1 && j == 1);
	});
	const MeshValidationReport ru = ValidateMesh(*cup);
	EXPECT_TRUE(ru.IsValid());
	EXPECT_EQ(cup->GetNFaces(), 2 * cup->GetNVertices() - 4);
	EXPECT_NEAR(ru.signedVolume, 17.0, 1e-12);

	const std::vector<double> l2 = {0.0, 1.0, 2.0};
	EXPECT_THROW(GridSolidMesh(l2, l2, {0.0, 1.0}, [](std::size_t i, std::size_t j, std::size_t) { return i == j; }),
	             std::logic_error);
	EXPECT_THROW(GridSolidMesh(l2, l2, l2, [](std::size_t i, std::size_t j, std::size_t k) { return i == j && j == k; }),
	             std::logic_error);
	EXPECT_THROW(GridSolidMesh(l2, l2, l2, [](std::size_t i, std::size_t j, std::size_t k) { return !(i == j && j == k); }),
	             std::logic_error);
	ASSERT_EQ(static_cast<float>(1.0 + 1e-9), 1.0f);
	EXPECT_THROW(GridSolidMesh({0.0, 1.0, 1.0 + 1e-9}, {0.0, 1.0}, {0.0, 1.0}, all), std::logic_error);
	EXPECT_THROW(GridSolidMesh({0.0, 2.0, 1.0}, {0.0, 1.0}, {0.0, 1.0}, all), std::logic_error);
}

// ===========================================================================
//  Plafond est dans le document (lot C4d, monument/v14)
// ===========================================================================

// PL3 : prefixe. Le document sans plafond est (MonumentWithoutPart, qui retire aussi les flancs) donne 805 834 sommets
// et 1 598 168 faces. Avec les plafonds : pieces identiques jusqu'au comble (sommets et faces,
// au memcmp) ; le comble remplace par ses 9 tranches ; pieces de la cella au prolongement identiques au bit pres
// // (sommets), leurs faces decalees des sommets ajoutes au comble ; les 33 + 58 pieces des plafonds en fin. Le
// maillage du document est celui du temple des reglages litteraux du README (CeilingTemple), sommets et faces au
// memcmp : la conversion rend ces valeurs au bit pres.
TEST(TEST_cgarchi_classical_ceiling, the_mesh_with_ceilings_extends_the_mesh_without_them)
{
	const MonumentData &heph = HephaistosMonument();
	const MonumentBuild before = BuildMonumentMesh(MonumentWithoutPart(heph, MonumentPart::EastCeiling));
	const MonumentBuild after = BuildMonumentMesh(heph);
	ASSERT_TRUE(before.mesh && after.mesh);
	EXPECT_EQ(before.mesh->GetNVertices(), 805834);
	EXPECT_EQ(before.mesh->GetNFaces(), 1598168);
	EXPECT_EQ(after.mesh->GetNVertices(), 826574);
	EXPECT_EQ(after.mesh->GetNFaces(), 1639140);
	EXPECT_EQ(before.report[static_cast<std::size_t>(MonumentPart::EastCeiling)].state, MonumentPartState::Absent);
	EXPECT_EQ(after.report[static_cast<std::size_t>(MonumentPart::EastCeiling)].state, MonumentPartState::Built);

	std::size_t roof = 0;
	while (roof < before.parts.size() && before.parts[roof].address.element != TempleElement::Roof)
		++roof;
	ASSERT_LT(roof, before.parts.size());
	ASSERT_EQ(after.parts[roof].address.element, TempleElement::Roof);
	const TempleMeshPart &oldRoof = before.parts[roof];
	EXPECT_EQ(std::memcmp(before.mesh->GetVertices().data(), after.mesh->GetVertices().data(),
	                      3u * oldRoof.firstVertex * sizeof(float)), 0);
	bool samePrefix = true;
	for (unsigned f = 0; samePrefix && f < oldRoof.firstFace; ++f)
		for (unsigned j = 0; j < 3; ++j)
			samePrefix = samePrefix && before.mesh->GetFaceVertex(f, j) == after.mesh->GetFaceVertex(f, j);
	EXPECT_TRUE(samePrefix);
	std::size_t slices = 0;
	while (roof + slices < after.parts.size() && after.parts[roof + slices].address.element == TempleElement::Roof)
		++slices;
	EXPECT_EQ(slices, 9u);
	ASSERT_EQ(after.parts.size(), before.parts.size() + (slices - 1) + 33u + 58u);
	bool sameTail = true;
	for (std::size_t k = roof + 1; k < before.parts.size(); ++k)
	{
		const TempleMeshPart &a = before.parts[k], &b = after.parts[k + slices - 1];
		sameTail = sameTail && a.address.element == b.address.element && a.address.side == b.address.side &&
		           a.address.index == b.address.index && a.vertexCount == b.vertexCount && a.faceCount == b.faceCount &&
		           std::memcmp(before.mesh->GetVertices().data() + 3u * a.firstVertex,
		                       after.mesh->GetVertices().data() + 3u * b.firstVertex, 3u * a.vertexCount * sizeof(float)) == 0;
		for (unsigned f = 0; sameTail && f < a.faceCount; ++f)
			for (unsigned j = 0; j < 3; ++j)
				sameTail = sameTail && static_cast<unsigned>(before.mesh->GetFaceVertex(a.firstFace + f, j)) - a.firstVertex ==
				                           static_cast<unsigned>(after.mesh->GetFaceVertex(b.firstFace + f, j)) - b.firstVertex;
	}
	EXPECT_TRUE(sameTail);
	for (std::size_t k = after.parts.size() - 33u - 58u; k < after.parts.size(); ++k)
		EXPECT_TRUE(IsCeiling(after.parts[k].address.element)) << k;

	const BuiltTemple &t = CeilingTemple();
	ASSERT_EQ(after.mesh->GetNVertices(), t.mesh->GetNVertices());
	ASSERT_EQ(after.mesh->GetNFaces(), t.mesh->GetNFaces());
	EXPECT_EQ(std::memcmp(after.mesh->GetVertices().data(), t.mesh->GetVertices().data(),
	                      t.mesh->GetVertices().size() * sizeof(float)), 0);
	bool sameFaces = true;
	for (unsigned f = 0; sameFaces && f < static_cast<unsigned>(t.mesh->GetNFaces()); ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && after.mesh->GetFaceVertex(f, j) == t.mesh->GetFaceVertex(f, j);
	EXPECT_TRUE(sameFaces);
}

// PL11 : MO-P2 et cascade. MonumentExterior retire le plafond est et ses controles (Absent, non Blocked) ;
// MonumentWithoutPart (EastCeiling) retire aussi les plafonds des flancs, que le document porte ; des plafonds des flancs
// sans plafond est sont
// refuses par le rapport. Les douze parties ont leur table de dependances (MO-P3).
TEST(TEST_cgarchi_classical_ceiling, the_ceiling_blocks_follow_the_block_dependencies)
{
	using P = MonumentPart;
	const MonumentData &heph = HephaistosMonument();
	EXPECT_TRUE(heph.present.eastCeiling && heph.present.eastCeilingControls);
	EXPECT_TRUE(heph.present.flankCeilings && heph.present.flankCeilingsControls);
	const MonumentData exterior = MonumentExterior(heph);
	EXPECT_FALSE(exterior.present.eastCeiling || exterior.present.eastCeilingControls);
	EXPECT_EQ(ReportMonumentParts(exterior)[static_cast<std::size_t>(P::EastCeiling)].state, MonumentPartState::Absent);

	const MonumentData &flanks = heph;
	const MonumentData noCeiling = MonumentWithoutPart(flanks, P::EastCeiling);
	EXPECT_FALSE(noCeiling.present.eastCeiling || noCeiling.present.eastCeilingControls);
	EXPECT_FALSE(noCeiling.present.flankCeilings || noCeiling.present.flankCeilingsControls);
	EXPECT_TRUE(noCeiling.present.porchExtension);
	MonumentData orphan = noCeiling;
	orphan.present.flankCeilings = true;
	EXPECT_EQ(RefusalOf([&] { ReportMonumentParts(orphan); }), "Monument : 'flankCeilings' demande le bloc 'eastCeiling'");

	EXPECT_EQ(MonumentPartDependencies(P::EastCeiling),
	          (std::vector<P>{P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension}));
	EXPECT_EQ(MonumentPartDependencies(P::FlankCeilings),
	          (std::vector<P>{P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension,
	                          P::EastCeiling}));
	EXPECT_EQ(kMonumentPartCount, 12u);
}

// ===========================================================================
//  Plafonds des pterons nord et sud (lot C4e, option C, registre CE-42)
// ===========================================================================

// PL2 (flancs) : controles du README (section 7.11, option C, regle R13) au maillage, chacun lu dans le document
// (flankCeilingsControls) et confronte a sa demi-unite imprimee : derniers axes est et ouest, bout cote mur, bout cote
// peristyle, longueur des poutres, course et marge des caissons, la course mesuree du nu exterieur du mur long (sur le
// maillage du mur) au bout des dalles. Caissons retrouves sur une dalle : 1 rang, 4 le long de la course.
TEST(TEST_cgarchi_classical_ceiling, the_flank_ceilings_meet_the_controls_of_the_readme)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const FlankCeilingLayout &l = *t.layout.flankCeilings;
	const EastCeilingSettings &e = *t.settings.eastCeiling;
	const MonumentFlankCeilingsControls &c = HephaistosMonument().flankCeilingsControls;
	using E = TempleElement;
	using S = PeristyleSide;
	const auto d = [](float v) { return static_cast<double>(v); };
	const Box3 eastMost = BoxOf(m, PartOf(t, E::CeilingBeam, S::Right, 0));
	const Box3 westMost = BoxOf(m, PartOf(t, E::CeilingBeam, S::Right, 14));
	const double eastAxis = (d(eastMost.lo[1]) + d(eastMost.hi[1])) / 2.0;
	const double westAxis = (d(westMost.lo[1]) + d(westMost.hi[1])) / 2.0;
	EXPECT_NEAR(eastAxis, l.beamAxes.front(), kFloatTolerance);
	EXPECT_NEAR(-eastAxis, c.lastEastAxisX, kPrintedHalfUnit);
	EXPECT_NEAR(westAxis, l.beamAxes.back(), kFloatTolerance);
	EXPECT_NEAR(-westAxis, c.lastWestAxisX, kPrintedHalfUnit);
	EXPECT_NEAR(d(eastMost.lo[0]), l.wallEnd, kFloatTolerance);
	EXPECT_NEAR(d(eastMost.lo[0]), c.wallEndY, kPrintedHalfUnit);
	EXPECT_NEAR(d(eastMost.hi[0]), l.peristyleEnd, kFloatTolerance);
	EXPECT_NEAR(d(eastMost.hi[0]), c.peristyleEndY, kPrintedHalfUnit);
	EXPECT_NEAR(d(eastMost.hi[0]) - d(eastMost.lo[0]), c.beamLength, kPrintedHalfUnit);
	const Box3 left = BoxOf(m, PartOf(t, E::CeilingBeam, S::Left, 0));
	EXPECT_EQ(left.lo[0], -eastMost.hi[0]);
	EXPECT_EQ(left.hi[0], -eastMost.lo[0]);

	const TempleMeshPart &slab = PartOf(t, E::CeilingSlab, S::Right, 1);
	std::set<float> layers;
	for (unsigned v = slab.firstVertex; v < slab.firstVertex + slab.vertexCount; ++v)
	{
		float q[3];
		m.GetVertex(v, q);
		layers.insert(q[2]);
	}
	ASSERT_EQ(layers.size(), 4u);
	const float zFloor = *std::next(layers.begin(), 2);
	const std::set<float> xs = CoordinatesAt(m, slab, 0, zFloor), ys = CoordinatesAt(m, slab, 1, zFloor);
	EXPECT_EQ(xs.size(), 2u * 4u);
	EXPECT_EQ(ys.size(), 2u * 1u);
	const double wallOuter = d(BoxOf(m, PartOf(t, E::LongWall, S::Right)).hi[0]);
	EXPECT_NEAR(wallOuter, l.naosOuter, kFloatTolerance);
	const double run = d(BoxOf(m, slab).hi[0]) - wallOuter;
	EXPECT_NEAR(run, c.cofferRun, kPrintedHalfUnit);
	EXPECT_EQ(static_cast<int>(std::floor(run / e.coffers.pitch)), 4);
	const double fond = e.coffers.opening - 2.0 * e.coffers.frame;
	const double margin = d(*xs.begin()) - wallOuter - (e.coffers.pitch - fond) / 2.0;
	EXPECT_NEAR(margin, (run - 4 * e.coffers.pitch) / 2.0, 2.0 * kFloatTolerance);
	EXPECT_NEAR(margin, c.cofferMargin, kPrintedHalfUnit);
	RecordProperty("flank_coffer_margin_m", std::to_string(margin));
}

// PL5 (flancs) : coplanarites au bit pres : pied des poutres sur le dessus de l'epicranitis ; dessus des poutres au
// plafond des poches des flancs ; dessous des dalles au dessus des bandeaux du plafond est ; dessus des dalles au
// dessus des poutres ; poches des flancs au contact de la poche du naos par une face (X = +-wallEnd).
TEST(TEST_cgarchi_classical_ceiling, the_flank_ceiling_pieces_are_coplanar_to_the_bit)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const FlankCeilingLayout &l = *t.layout.flankCeilings;
	using E = TempleElement;
	using S = PeristyleSide;
	const float band = BoxOf(m, PartOf(t, E::PorchBand, S::Front, 0)).hi[2];
	for (const S side : {S::Left, S::Right})
	{
		SCOPED_TRACE(static_cast<int>(side));
		const Box3 course = BoxOf(m, PartOf(t, E::EpicranitisCourse, side));
		const float pocketTop = static_cast<float>(l.pockets[side == S::Left ? 0 : 1].zTop);
		for (int k = 0; k < 15; ++k)
		{
			const Box3 beam = BoxOf(m, PartOf(t, E::CeilingBeam, side, k));
			EXPECT_EQ(beam.lo[2], course.hi[2]) << k;
			EXPECT_EQ(beam.hi[2], pocketTop) << k;
		}
		for (int k = 0; k < 14; ++k)
		{
			const Box3 slab = BoxOf(m, PartOf(t, E::CeilingSlab, side, k));
			EXPECT_EQ(slab.lo[2], band) << k;
			EXPECT_EQ(slab.hi[2], pocketTop) << k;
		}
	}
	ASSERT_TRUE(t.layout.interior);
	EXPECT_EQ(l.pockets[1].x0, t.layout.interior->pocket.x1);
	EXPECT_EQ(l.pockets[0].x1, t.layout.interior->pocket.x0);
}

// PL7 (flancs) : refus nommes. Module : les huit gardes de LayoutGreekFlankCeilings, chacune par son motif (bout cote
// peristyle au nu exterieur des murs : jeu de 2,0 m ; aucun caisson : jeu de 1,6 m ; travee centrale, de largeur
// 2 firstBeamOffset - beamWidth, trop etroite pour un rang de caissons : 0,3 et 0,45 m ; poutres centrales au contact :
// 0,2625 m) ; temple : jeu de 0,15 m qui met le
// dessus des poutres au-dessus du pan, refus nomme 'flankCeilings.peristyleGap' a la mise en place et a la construction,
// admis sans fronton.
TEST(TEST_cgarchi_classical_ceiling, a_wrong_flank_ceiling_is_refused_by_name)
{
	const TempleSettings good = CeilingSettings();
	ASSERT_EQ(RefusalOf([&] { ValidateTempleSettings(good); }), "");
	const std::string head = "Plafonds des flancs : 'flankCeilings.";
	const std::vector<std::tuple<const char *, std::function<void (FlankCeilingSettings &)>, const char *>> cases = {
		{"beamWidth", [](FlankCeilingSettings &f) { f.beamWidth = -1.0; }, "n'est pas une longueur finie"},
		{"peristyleGap", [](FlankCeilingSettings &f) { f.peristyleGap = std::numeric_limits<double>::quiet_NaN(); },
		 "n'est pas une longueur finie"},
		{"eastBeamCount", [](FlankCeilingSettings &f) { f.frontBeamCount = 0; }, "doit valoir au moins 1"},
		{"cofferRows", [](FlankCeilingSettings &f) { f.cofferRows = 0; }, "doit valoir au moins 1"},
		{"beamSpacing", [](FlankCeilingSettings &f) { f.beamSpacing = 0.5; }, "ne depasse pas la largeur"},
		{"firstBeamOffset", [](FlankCeilingSettings &f) { f.firstBeamOffset = 0.2; }, "ne depasse pas la demi-largeur"},
		{"firstBeamOffset", [](FlankCeilingSettings &f) { f.firstBeamOffset = 0.2625; }, "ne depasse pas la demi-largeur"},
		{"cofferRows", [](FlankCeilingSettings &f) { f.firstBeamOffset = 0.3; }, "ne tient pas dans la travee la plus etroite"},
		{"cofferRows", [](FlankCeilingSettings &f) { f.firstBeamOffset = 0.45; }, "ne tient pas dans la travee la plus etroite"},
		{"eastBeamCount", [](FlankCeilingSettings &f) { f.frontBeamCount = 8; }, "porte la face de la derniere poutre est"},
		{"westBeamCount", [](FlankCeilingSettings &f) { f.backBeamCount = 9; }, "porte la face de la derniere poutre ouest"},
		{"peristyleGap", [](FlankCeilingSettings &f) { f.peristyleGap = 2.0; }, "ne laisse pas de portee"},
		{"beamHeight", [](FlankCeilingSettings &f) { f.beamHeight = 0.4; }, "met le dessus des poutres"},
		{"peristyleGap", [](FlankCeilingSettings &f) { f.peristyleGap = 1.6; }, "ne laisse aucun caisson"},
		{"cofferRows", [](FlankCeilingSettings &f) { f.cofferRows = 2; }, "ne tient pas"},
	};
	for (const auto &[field, edit, rule] : cases)
	{
		SCOPED_TRACE(field);
		TempleSettings s = good;
		edit(*s.flankCeilings);
		const std::string what = RefusalOf([&] { ValidateTempleSettings(s); });
		EXPECT_TRUE(Starts(what, head + field + "' " + rule)) << what;
	}
	TempleSettings close = good;
	close.flankCeilings->peristyleGap = 0.15;
	const std::string named = "Temple : 'flankCeilings.peristyleGap' laisse le dessus des poutres et des dalles au contact "
	                          "ou au-dessus d'un pan du comble";
	EXPECT_EQ(RefusalOf([&] { ValidateTempleSettings(close); }), named);
	// La construction rend le meme refus, avant le fronton (qui refuserait la poche en termes generiques).
	EXPECT_EQ(RefusalOf([&] { BuildTempleMesh(close); }), named);
	close.pediment.reset();
	EXPECT_EQ(RefusalOf([&] { ValidateTempleSettings(close); }), "");
}

// PL9 (flancs) : bout cote peristyle confondu en float avec le nu exterieur des murs, au-dela en double : refuse par
// la mise en place ('peristyleGap'), en float comme les sommets.
TEST(TEST_cgarchi_classical_ceiling, the_flank_ceiling_layout_refuses_its_limit_cases_in_float)
{
	const BuiltTemple &t = CeilingTemple();
	const FlankCeilingSettings s = LiteralFlankCeilings();
	const EastCeilingSettings &e = *t.settings.eastCeiling;
	const EastCeilingLayout &el = *t.layout.eastCeiling;
	const double outer = el.crown.naos.outer;
	double ring = outer + s.peristyleGap;
	while (!(ring - s.peristyleGap > outer))
		ring = std::nextafter(ring, 100.0);
	ASSERT_GT(ring - s.peristyleGap, outer);
	ASSERT_EQ(static_cast<float>(ring - s.peristyleGap), static_cast<float>(outer));
	const std::string what = RefusalOf([&] { LayoutGreekFlankCeilings(s, e, el, ring); });
	EXPECT_TRUE(Starts(what, "Plafonds des flancs : 'flankCeilings.peristyleGap' ne laisse pas de portee")) << what;
	EXPECT_EQ(RefusalOf([&] { LayoutGreekFlankCeilings(s, e, el, t.layout.flankCeilings->peristyleEnd + s.peristyleGap); }),
	          "");
}

// PL12 (option C, CE-42) : chaque sommet des pieces des flancs est sous le pan du comble, z < pan (|X|), le pan etant
// calcule en double depuis le repere du fronton : zBase = entablatureFoot + z11, xTop = pied du haut de la section
// (detail::PedimentFeet), faite = zBase + xTop / rakingRun. La marge minimale mesuree est celle du bout des pieces,
// pan (peristyleEnd) - beamTop, a 1e-6 (arrondi en float des sommets), et depasse 2 mm ; le jeu entre le bout des
// pieces et la face interieure de l'anneau vaut peristyleGap, 0,170 m, a 1e-6.
TEST(TEST_cgarchi_classical_ceiling, the_flank_ceilings_stay_under_the_roof_pans_with_their_gap)
{
	const BuiltTemple &t = CeilingTemple();
	const Mesh &m = *t.mesh;
	const FlankCeilingLayout &l = *t.layout.flankCeilings;
	const GreekEntablatureSettings &entablature = *t.settings.entablature;
	const GreekPedimentSettings &pediment = *t.settings.pediment;
	const double zBase = t.layout.entablatureFoot + GreekEntablatureBandBounds(entablature).back();
	const detail::CheckedEntablature checked = detail::CheckEntablature(entablature);
	const double xTop = detail::PedimentFeet(detail::CheckPediment(pediment, checked)).back();
	const double ridge = zBase + xTop / pediment.rakingRun;
	const auto pan = [&](double x) { return zBase + (ridge - zBase) * (xTop - std::fabs(x)) / xTop; };
	const double expected = pan(l.peristyleEnd) - l.beamTop;
	double minMargin = std::numeric_limits<double>::max();
	std::size_t vertices = 0, above = 0;
	for (const TempleMeshPart &p : t.parts)
	{
		if (!IsCeiling(p.address.element) || (p.address.side != PeristyleSide::Left && p.address.side != PeristyleSide::Right))
			continue;
		for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
		{
			float q[3];
			m.GetVertex(v, q);
			const double margin = pan(q[0]) - static_cast<double>(q[2]);
			minMargin = std::min(minMargin, margin);
			above += margin > 0.0 ? 0 : 1;
			++vertices;
		}
	}
	EXPECT_GT(vertices, 0u);
	EXPECT_EQ(above, 0u);
	EXPECT_NEAR(minMargin, expected, kFloatTolerance);
	EXPECT_GT(minMargin, 0.002);

	float ringInner = std::numeric_limits<float>::max();
	for (const TempleMeshPart &p : t.parts)
		if (p.address.element == TempleElement::Entablature && p.address.side == PeristyleSide::Front)
			for (unsigned v = p.firstVertex; v < p.firstVertex + p.vertexCount; ++v)
			{
				float q[3];
				m.GetVertex(v, q);
				if (q[0] > 0.0f)
					ringInner = std::min(ringInner, q[0]);
			}
	const Box3 beam = BoxOf(m, PartOf(t, TempleElement::CeilingBeam, PeristyleSide::Right, 7));
	const double gap = static_cast<double>(ringInner) - static_cast<double>(beam.hi[0]);
	EXPECT_NEAR(gap, t.settings.flankCeilings->peristyleGap, kFloatTolerance);
	EXPECT_NEAR(static_cast<double>(beam.hi[0]), l.peristyleEnd, kFloatTolerance);
	std::printf("PL12 : marge minimale sous le pan %.6f m (attendue %.6f) ; jeu a l'anneau %.6f m\n", minMargin, expected, gap);
	RecordProperty("flank_pan_margin_m", std::to_string(minMargin));
}
