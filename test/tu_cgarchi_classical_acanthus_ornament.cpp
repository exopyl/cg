#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Ornement du chapiteau corinthien, rangs F et E, tiges C : K2 a K10, K12
// ===========================================================================
//
// Une feuille representante par rang, copiee par rotation (registre AC-45) : les
// invariants de la feuille seule (J1 a J21') sont tenus par les tests de la
// feuille sur F a pi/8 et E a 0, et ceux-ci disent que chaque copie est cette
// feuille, a sa place, sans recouvrement.

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 1e-4;   // p : sommets stockes en float
constexpr double kCell = 1e-3;  // p : maille de la grille de hachage de K4
constexpr int kPerRank = 8;     // pieces par rang (registre AC-45)
constexpr unsigned kFleurons = static_cast<unsigned> (detail::kFleurons);
constexpr unsigned kPieces = 7u * kPerRank + kFleurons;   // 8 F, 8 E, 8 C, 16 D, 8 V, 8 H, 4 G (registre AC-46)
constexpr unsigned kComponents = kPieces + 2u * kPerRank + kFleurons;   // une coque par piece, plus l'oeil de chaque volute et helice et la tige de chaque fleuron

using Point = std::array<double, 3>;

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

ColumnProfile CorinthianProfile ()
{
	ColumnOptions options;
	options.order = Order::Corinthian;
	return BuildColumnProfile (ClassicalOrders ().Get (Order::Corinthian), options);
}

struct Case
{
	OrnamentDetail detail;
	double scale;
	detail::CapitalEnvelope env;
	CapitalOrnament ornament;
};

// Light et Heavy, echelles 1 et 2.
const std::vector<Case> &Cases ()
{
	static const std::vector<Case> cases = [] {
		std::vector<Case> out;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		const ColumnProfile profile = CorinthianProfile ();
		for (const OrnamentDetail detail : {OrnamentDetail::Light, OrnamentDetail::Heavy})
			for (const double scale : {1.0, 2.0})
			{
				AcanthusOptions options;
				options.detail = detail;
				options.scale = scale;
				out.push_back ({detail, scale, detail::ReadCapitalEnvelope (order, profile, scale),
				                BuildCorinthianOrnament (order, profile, options)});
			}
		return out;
	}();
	return cases;
}

// Sommet v en parties, repere du tour (z depuis le pied du chapiteau).
Point Parts (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned v)
{
	float p[3] {};
	mesh.GetVertex (v, p);
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

// Rang k de la piece i (8 F, 8 E, puis 8 C).
int RankIndex (std::size_t i)
{
	return static_cast<int> (i % static_cast<std::size_t> (kPerRank));
}

// Axe de la feuille k d'un rang, ecrit d'apres les planches (registre AC-45) et non lu dans le produit :
// F et C pi/8 + k pi/4, E k pi/4.
double PlateAxis (CapitalPieceKind kind, int k)
{
	return (kind == CapitalPieceKind::MiddleLeaf ? 0.0 : kPi / 8.0) + k * kPi / 4.0;
}

// Feuille F ou E.
bool IsLeaf (const CapitalPiece &piece)
{
	return piece.kind == CapitalPieceKind::LowerLeaf || piece.kind == CapitalPieceKind::MiddleLeaf;
}

#define CASE_TRACE(c) SCOPED_TRACE (std::string (LevelName ((c).detail)) + ", echelle " + std::to_string ((c).scale))

} // namespace

// K2, K16 : phase des planches. Chaque piece est symetrique par son plan axial (J14 ; tige : cannelures centrees sur
// le plan radial) : l'azimut du barycentre de ses sommets est l'axe PlateAxis du rang. RankTheta, lu par le produit,
// rend ces memes axes.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_leaves_stand_on_the_phase_of_the_plates)
{
	for (const CapitalPieceKind kind : {CapitalPieceKind::LowerLeaf, CapitalPieceKind::MiddleLeaf, CapitalPieceKind::Cauliculus})
		for (int k = 0; k < kPerRank; ++k)
			EXPECT_NEAR (detail::RankTheta (kind, k), PlateAxis (kind, k), 1e-12) << k;
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		double worst = 0.0;
		for (std::size_t i = 0; i < 3u * kPerRank; ++i)
		{
			const CapitalPiece &piece = c.ornament.pieces[i];
			double sx = 0.0, sy = 0.0;
			for (unsigned v = piece.vertexBegin; v < piece.vertexEnd; ++v)
			{
				const Point p = Parts (*c.ornament.mesh, c.env, v);
				sx += p[0];
				sy += p[1];
			}
			const double gap = std::fabs (std::remainder (std::atan2 (sy, sx) - PlateAxis (piece.kind, RankIndex (i)), 2.0 * kPi));
			worst = std::max (worst, gap);
		}
		EXPECT_LE (worst, 1e-4);
		std::printf ("%s, echelle %g : K2 ecart de phase maximal %.2e rad\n", LevelName (c.detail), c.scale, worst);
	}
}

// K3 : copie = construction (feuilles F et E ; tiges : K17). La piece k est, indice par indice, la feuille que
// BuildLeafSheet pose a PlateAxis (rang, k) : a 4 ulp float pres a r ~ 30 p (8e-6 p), triangles identiques au
// decalage de la piece pres. La copie tournee et la construction directe arrondissent differemment (contraction FMA
// comprise) : comparaison en ulp, pas au bit.
TEST (TEST_cgarchi_classical_acanthus_ornament, each_copy_is_the_leaf_built_at_its_place)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, detail::LeafStyle::Olive);
		const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, c.detail);
		const Mesh &mesh = *c.ornament.mesh;
		double worst = 0.0;
		for (std::size_t i = 0; i < c.ornament.pieces.size (); ++i)
		{
			const CapitalPiece &piece = c.ornament.pieces[i];
			if (!IsLeaf (piece))
				continue;
			const double theta = PlateAxis (piece.kind, RankIndex (i));
			const detail::LeafSheet sheet = detail::BuildLeafSheet (c.env, spec, detail::DefaultLeafPlacement (piece.kind, theta), tpl);
			const Mesh &leaf = *sheet.mesh;
			ASSERT_EQ (piece.vertexEnd - piece.vertexBegin, static_cast<unsigned> (leaf.GetNVertices ())) << i;
			ASSERT_EQ (piece.faceEnd - piece.faceBegin, static_cast<unsigned> (leaf.GetNFaces ())) << i;
			for (unsigned v = 0; v < static_cast<unsigned> (leaf.GetNVertices ()); ++v)
			{
				const Point a = Parts (mesh, c.env, piece.vertexBegin + v), b = Parts (leaf, c.env, v);
				worst = std::max ({worst, std::fabs (a[0] - b[0]), std::fabs (a[1] - b[1]), std::fabs (a[2] - b[2])});
			}
			bool sameTriangles = true;
			for (unsigned f = 0; f < static_cast<unsigned> (leaf.GetNFaces ()); ++f)
				for (unsigned k = 0; k < 3; ++k)
					sameTriangles = sameTriangles && static_cast<unsigned> (mesh.GetFaceVertex (piece.faceBegin + f, k)) ==
					                                     piece.vertexBegin + static_cast<unsigned> (leaf.GetFaceVertex (f, k));
			EXPECT_TRUE (sameTriangles) << i;
		}
		EXPECT_LE (worst, 8e-6);
		std::printf ("%s, echelle %g : K3 ecart maximal copie / construction %.2e p\n", LevelName (c.detail), c.scale, worst);
	}
}

// K4 : symetrie diedrale D8 sans les indices. La reflexion y -> -y et la rotation de pi/4 envoient l'ensemble des
// sommets de F, E et C sur lui-meme (plus proche voisin par grille de hachage de 1e-3 p). Les D : K18, D4.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_ornament_has_the_symmetry_of_the_octagon)
{
	const auto key = [] (long long i, long long j, long long k) {
		return static_cast<std::uint64_t> (i * 73856093LL ^ j * 19349663LL ^ k * 83492791LL);
	};
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const Mesh &mesh = *c.ornament.mesh;
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		const unsigned n = c.ornament.pieces[3u * kPerRank - 1].vertexEnd;
		std::vector<Point> points (n);
		std::unordered_multimap<std::uint64_t, unsigned> grid;
		grid.reserve (n);
		const auto cellOf = [] (double x) { return static_cast<long long> (std::floor (x / kCell)); };
		for (unsigned v = 0; v < n; ++v)
		{
			points[v] = Parts (mesh, c.env, v);
			grid.emplace (key (cellOf (points[v][0]), cellOf (points[v][1]), cellOf (points[v][2])), v);
		}
		const auto nearest = [&] (const Point &q) {
			double best = std::numeric_limits<double>::infinity ();
			const long long i = cellOf (q[0]), j = cellOf (q[1]), k = cellOf (q[2]);
			for (long long di = -1; di <= 1; ++di)
				for (long long dj = -1; dj <= 1; ++dj)
					for (long long dk = -1; dk <= 1; ++dk)
					{
						const auto range = grid.equal_range (key (i + di, j + dj, k + dk));
						for (auto it = range.first; it != range.second; ++it)
						{
							const Point &p = points[it->second];
							best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
						}
					}
			return best;
		};
		const double cr = std::cos (kPi / 4.0), sr = std::sin (kPi / 4.0);
		double reflection = 0.0, rotation = 0.0;
		for (const Point &p : points)
		{
			reflection = std::max (reflection, nearest ({p[0], -p[1], p[2]}));
			rotation = std::max (rotation, nearest ({cr * p[0] - sr * p[1], sr * p[0] + cr * p[1], p[2]}));
		}
		EXPECT_LE (reflection, 1e-5);
		EXPECT_LE (rotation, 1e-5);
		std::printf ("%s, echelle %g : K4 reflexion %.2e p, rotation de pi/4 %.2e p (%u sommets)\n", LevelName (c.detail), c.scale,
		             reflection, rotation, n);
	}
}

// K5, K6 : jeu avec la corbeille et borne des feuilles, par piece (registre AC-48 : F et E contre le cone).
TEST (TEST_cgarchi_classical_acanthus_ornament, the_leaves_keep_off_the_bell_and_under_the_cone)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		// Les G (poses sur l'abaque, au-dessus des bandes de la corbeille) sont bornes par le tour (K49').
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		const double clearance = detail::CoreClearance (*c.ornament.mesh, c.env, 0u, c.ornament.pieces[kPieces - kFleurons].vertexBegin);
		double excess = -std::numeric_limits<double>::infinity ();
		for (const CapitalPiece &piece : c.ornament.pieces)
			excess = std::max (excess, detail::EnvelopeExcess (*c.ornament.mesh, c.env, piece));
		EXPECT_GE (clearance, 0.05 - kTau);
		EXPECT_LE (excess, kTau);
		std::printf ("%s, echelle %g : K5 CoreClearance %.5f p ; K6 EnvelopeExcess par piece %.5f p\n", LevelName (c.detail), c.scale,
		             clearance, excess);
	}
}

// K7 : J21' sur l'assemblage. Pour chaque E_k et ses voisines F_k et F_(k-1), aux noeuds (theta deroule autour de
// theta (E_k), h) couverts par la face exterieure de E et la face interieure d'une F : rayon de F - c - rayon de E
// >= 0,05 p.
TEST (TEST_cgarchi_classical_acanthus_ornament, each_middle_leaf_passes_behind_its_lower_neighbours)
{
	using acanthus_test::RadiusGrid;
	const int n = kPerRank;
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		const Mesh &mesh = *c.ornament.mesh;
		const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, detail::LeafStyle::Olive);
		const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, c.detail);
		// Faces exterieures d'une coque : [0, nf) ; interieures : [nf, 2 nf) (BuildLeafSheet).
		const unsigned fF = 2 * tpl.leafTriangles, fE = 2 * static_cast<unsigned> (tpl.triangles.size ());
		double worst = std::numeric_limits<double>::infinity (), best = -worst;
		for (int k = 0; k < n; ++k)
		{
			const CapitalPiece &e = c.ornament.pieces[static_cast<std::size_t> (n + k)];
			ASSERT_EQ (e.kind, CapitalPieceKind::MiddleLeaf) << k;
			const double thetaE = PlateAxis (CapitalPieceKind::MiddleLeaf, k);
			RadiusGrid outerE, innerF;
			acanthus_test::Rasterize (outerE, mesh, c.env, e.faceBegin, e.faceBegin + fE, true, thetaE);
			for (const int kf : {k, (k + n - 1) % n})
			{
				const CapitalPiece &f = c.ornament.pieces[static_cast<std::size_t> (kf)];
				ASSERT_EQ (f.kind, CapitalPieceKind::LowerLeaf) << kf;
				acanthus_test::Rasterize (innerF, mesh, c.env, f.faceBegin + fF, f.faceBegin + 2 * fF, false, thetaE);
			}
			double margin = std::numeric_limits<double>::infinity ();
			unsigned common = 0;
			for (int j = 0; j < RadiusGrid::kNH; ++j)
				for (int i = 0; i < RadiusGrid::kNTheta; ++i)
				{
					const double re = outerE.At (i, j), rf = innerF.At (i, j);
					if (std::isnan (re) || std::isnan (rf))
						continue;
					++common;
					margin = std::min (margin, rf - spec.clearance - re);
				}
			EXPECT_GT (common, 0u) << k;
			EXPECT_GE (margin, 0.05) << k;
			worst = std::min (worst, margin);
			best = std::max (best, margin);
		}
		std::printf ("%s, echelle %g : K7 marge de E derriere F, %d paires : de %+.4f a %+.4f p\n", LevelName (c.detail), c.scale, n,
		             worst, best);
	}
}

// K8 : couverture de la corbeille par F et E seuls (registre AC-7, seuil des seuls rangs F et E).
TEST (TEST_cgarchi_classical_acanthus_ornament, the_two_ranks_cover_most_of_the_bell)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_EQ (c.ornament.pieces.size (), kPieces);
		const std::vector<CapitalPiece> leaves (c.ornament.pieces.begin (), c.ornament.pieces.begin () + 2 * kPerRank);
		const double coverage = detail::BellCoverage (*c.ornament.mesh, leaves, c.env, 1.0, 23.0);
		EXPECT_GE (coverage, 0.60);
		EXPECT_LE (coverage, 0.80);
		std::printf ("%s, echelle %g : K8 BellCoverage [1, 23] %.4f\n", LevelName (c.detail), c.scale, coverage);
	}
}

// K9, K22, K37 : l'ornement seul est un maillage valide, une composante par piece (16 feuilles F et E, 8 tiges, 16 D, 8 V,
// 8 H) et une par oeil de volute ou d'helice, de volume
// positif.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_ornament_is_a_sound_mesh)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		MeshValidationOptions options;
		options.expectedComponents = kComponents;
		const MeshValidationReport r = ValidateMesh (*c.ornament.mesh, options);
		std::string defects;
		for (std::size_t k = 0; k < kMeshDefectCount; ++k)
			if (r.counts[k])
				defects += std::string (" ") + MeshDefectName (static_cast<MeshDefect> (k)) + "=" + std::to_string (r.counts[k]);
		EXPECT_TRUE (r.IsValid ()) << defects << (r.selfIntersectionTruncated ? " tronque" : "");
		EXPECT_FALSE (r.selfIntersectionTruncated);
		EXPECT_EQ (r.components, kComponents);
		EXPECT_GT (r.signedVolume, 0.0);
		std::printf ("%s, echelle %g : K9 valide %d, %u composantes, volume %.6g\n", LevelName (c.detail), c.scale, r.IsValid () ? 1 : 0,
		             r.components, r.signedVolume);
	}
}

// K12 : pointes en plan, controle de coherence avec le plan de la pl. XXV (la saillie est cotee par le profil, J18) :
// max r de chaque F dans 22,3 +- 0,6 p, de chaque E dans [26,4 ; 28,9] p.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_tips_lie_on_the_circles_of_the_plan)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
		for (std::size_t i = 0; i < c.ornament.pieces.size (); ++i)
		{
			const CapitalPiece &piece = c.ornament.pieces[i];
			if (!IsLeaf (piece))
				continue;
			double r = 0.0;
			for (unsigned v = piece.vertexBegin; v < piece.vertexEnd; ++v)
			{
				const Point p = Parts (*c.ornament.mesh, c.env, v);
				r = std::max (r, std::hypot (p[0], p[1]));
			}
			const int rank = piece.kind == CapitalPieceKind::LowerLeaf ? 0 : 1;
			lo[rank] = std::min (lo[rank], r);
			hi[rank] = std::max (hi[rank], r);
			if (rank == 0)
			{
				EXPECT_NEAR (r, 22.3, 0.6) << i;
			}
			else
			{
				EXPECT_GE (r, 26.4) << i;
				EXPECT_LE (r, 28.9) << i;
			}
		}
		std::printf ("%s, echelle %g : K12 pointes F de %.4f a %.4f p, E de %.4f a %.4f p\n", LevelName (c.detail), c.scale, lo[0], hi[0],
		             lo[1], hi[1]);
	}
}

// K10, K23, K38 : la colonne, tour, 16 feuilles F et E, 8 tiges, 16 feuilles D, 8 volutes et 8 helices, est un maillage
// valide (73 coques : une par piece, une par oeil, et le tour ; test d'auto-intersection
// complet), au defaut corinthien et sur le corinthien complet (piedestal, entablement, module 2), en Light et en
// Heavy. Les pieds des feuilles reposent sur l'astragale sans y entrer (registre AC-49), les tiges partent au-dessus
// de la crete de F (AC-52). « Ornament envelope » reste decoche : coche, le cone tourne a la place du noyau contient
// les feuilles.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_column_and_its_leaves_are_a_sound_mesh)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const bool complete : {false, true})
		{
			SCOPED_TRACE (std::string (LevelName (level)) + (complete ? ", complet" : ", defaut"));
			cgarchi::ClassicalColumnSettings s;
			s.order = static_cast<int> (Order::Corinthian);
			s.capitalOrnaments = static_cast<int> (level);
			s.pedestal = complete;
			s.entablature = complete;
			s.module = complete ? 2.0f : 1.0f;
			const std::unique_ptr<Mesh> mesh = cgarchi::BuildClassicalColumnMesh (s);
			MeshValidationOptions options;
			options.expectedComponents = 81u;   // tour, 16 feuilles F et E, 8 tiges, 16 feuilles D, 8 volutes, 8 helices et leurs oeils, 4 fleurons et leurs tiges
			const MeshValidationReport r = ValidateMesh (*mesh, options);
			std::string defects;
			for (std::size_t k = 0; k < kMeshDefectCount; ++k)
				if (r.counts[k])
					defects += std::string (" ") + MeshDefectName (static_cast<MeshDefect> (k)) + "=" + std::to_string (r.counts[k]);
			EXPECT_TRUE (r.IsValid ()) << defects << (r.selfIntersectionTruncated ? " tronque" : "");
			EXPECT_FALSE (r.selfIntersectionTruncated);
			std::printf ("%s, %s : K10 valide %d, %u composantes, %u triangles%s\n", LevelName (level), complete ? "complet" : "defaut",
			             r.IsValid () ? 1 : 0, r.components, mesh->GetNFaces (), defects.c_str ());
		}
}

// La colonne porte l'ornement tel que BuildCorinthianOrnament le construit : ses sommets sont ceux du
// tour (colonne sans ornement), puis ceux de l'ornement construit sur le meme profil a l'echelle du tour, a l'octet ;
// ses faces, celles du tour puis celles de l'ornement decalees du nombre de sommets du tour, dans le meme ordre. Deux
// sorties du produit comparees entre elles, sans calcul du test.
TEST (TEST_cgarchi_classical_acanthus_ornament, the_column_carries_the_ornament_as_built)
{
	const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const bool complete : {false, true})
		{
			SCOPED_TRACE (std::string (LevelName (level)) + (complete ? ", complet" : ", defaut"));
			cgarchi::ClassicalColumnSettings s;
			s.order = static_cast<int> (Order::Corinthian);
			s.capitalOrnaments = static_cast<int> (level);
			s.pedestal = complete;
			s.entablature = complete;
			s.module = complete ? 2.0f : 1.0f;
			ColumnProfile profile;
			const std::unique_ptr<Mesh> column = cgarchi::BuildClassicalColumnMesh (s, &profile);
			s.capitalOrnaments = static_cast<int> (OrnamentDetail::None);
			const std::unique_ptr<Mesh> lathe = cgarchi::BuildClassicalColumnMesh (s);
			AcanthusOptions o;
			o.detail = level;
			o.scale = s.module / order.partsPerModule;
			const CapitalOrnament ornament = BuildCorinthianOrnament (order, profile, o);
			Mesh &orn = *ornament.mesh;
			const unsigned lv = static_cast<unsigned> (lathe->GetNVertices ()), lf = static_cast<unsigned> (lathe->GetNFaces ());
			ASSERT_GT (orn.GetNFaces (), 0);
			ASSERT_EQ (column->GetNVertices (), lathe->GetNVertices () + orn.GetNVertices ());
			ASSERT_EQ (column->GetNFaces (), lathe->GetNFaces () + orn.GetNFaces ());
			unsigned vertexDiff = 0, faceDiff = 0;
			for (unsigned v = 0; v < static_cast<unsigned> (column->GetNVertices ()); ++v)
			{
				float a[3] {}, b[3] {};
				column->GetVertex (v, a);
				if (v < lv)
					lathe->GetVertex (v, b);
				else
					orn.GetVertex (v - lv, b);
				vertexDiff += a[0] == b[0] && a[1] == b[1] && a[2] == b[2] ? 0u : 1u;
			}
			for (unsigned f = 0; f < static_cast<unsigned> (column->GetNFaces ()); ++f)
			{
				Mesh &from = f < lf ? *lathe : orn;
				const unsigned g = f < lf ? f : f - lf;
				const int shift = f < lf ? 0 : static_cast<int> (lv);
				bool same = column->GetFaceNVertices (f) == from.GetFaceNVertices (g);
				for (unsigned j = 0; same && j < static_cast<unsigned> (column->GetFaceNVertices (f)); ++j)
					same = column->GetFaceVertex (f, j) == from.GetFaceVertex (g, j) + shift;
				faceDiff += same ? 0u : 1u;
			}
			EXPECT_EQ (vertexDiff, 0u);
			EXPECT_EQ (faceDiff, 0u);
			std::printf ("%s, %s : colonne = tour (%u sommets) + ornement (%d sommets) : %u sommets et %u faces differents\n", LevelName (level),
			             complete ? "complet" : "defaut", lv, orn.GetNVertices (), vertexDiff, faceDiff);
		}
}
