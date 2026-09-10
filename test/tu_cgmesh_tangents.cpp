#include <gtest/gtest.h>

#include "../src/cgmesh/material_pbr.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"
#include "../src/cgmesh/tangents.h"
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

// ============================================================================
//  Tangentes par sommet et second jeu d'UV
// ============================================================================
//
// CATEGORIE EXCLUE, ET C'EST DELIBERE : les maillages a COIN DE COUTURE UV --
// un sommet topologique partage par deux ilots de parametrisation. Un tel
// sommet porte deux tangentes ; le stockage PAR SOMMET n'a qu'une case, et la
// moyenne qui y est ecrite est fausse pour les deux ilots. Aucune
// implementation ne peut faire mieux dans ce stockage : ce n'est pas un defaut
// a corriger mais une propriete de la representation. Le remede appartient a
// l'appelant -- Mesh::SplitVerticesByUVSeams avant generateTangents. Rien
// ci-dessous n'affirme donc quoi que ce soit sur ce cas.
//
// ============================================================================

namespace {

constexpr float kEps = 1e-4f;

// Quad plan dans XY, UV alignees sur les axes :
//
//   3(0,1)---2(1,1)      u croit avec x, v croit avec y
//     |        |         normale = +Z
//   0(0,0)---1(1,0)
//
// C'est le cas ou la reponse est connue a la main : dP/du = +X.
void MakeTexturedQuadXY (Mesh &m)
{
	m.Init (4, 1);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	m.FaceAt (0)->SetQuad (0, 1, 2, 3);

	const std::vector<float> uv = { 0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f };
	m.SetTextureCoordinates (uv, 4);
	m.ComputeNormals ();
}

// Meme quad, parametrisation MIROIR : u decroit quand x croit. La base devient
// gauchere, ce que le signe de main doit rapporter.
void MakeMirroredQuadXY (Mesh &m)
{
	m.Init (4, 1);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	m.FaceAt (0)->SetQuad (0, 1, 2, 3);

	const std::vector<float> uv = { 1.f, 0.f,  0.f, 0.f,  0.f, 1.f,  1.f, 1.f };
	m.SetTextureCoordinates (uv, 4);
	m.ComputeNormals ();
}

void MakeUntexturedTriangle (Mesh &m)
{
	m.Init (3, 1);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 0.f, 1.f, 0.f);
	m.FaceAt (0)->SetTriangle (0, 1, 2);
	m.ComputeNormals ();
}

} // namespace

// ---------------------------------------------------------------------------
//  Critere 1 : la reponse connue a la main
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, axis_aligned_quad_gives_the_x_axis)
{
	Mesh m;
	MakeTexturedQuadXY (m);

	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.GetNTangents (), 4u);

	const std::vector<float> &t = m.GetVertexTangents ();
	ASSERT_EQ (t.size (), 16u);

	for (unsigned int i = 0; i < 4; ++i)
	{
		EXPECT_NEAR (t[4*i + 0], 1.f, kEps) << "sommet " << i;
		EXPECT_NEAR (t[4*i + 1], 0.f, kEps) << "sommet " << i;
		EXPECT_NEAR (t[4*i + 2], 0.f, kEps) << "sommet " << i;
		EXPECT_NEAR (std::fabs (t[4*i + 3]), 1.f, kEps)
			<< "le signe de main vaut +1 ou -1, jamais autre chose";
	}
}

TEST (TEST_cgmesh_tangents, handedness_reports_a_mirrored_parameterization)
{
	Mesh direct, mirrored;
	MakeTexturedQuadXY (direct);
	MakeMirroredQuadXY (mirrored);

	ASSERT_TRUE (generateTangents (direct));
	ASSERT_TRUE (generateTangents (mirrored));

	// Le signe DIFFERE entre les deux parametrisations : c'est tout ce que w
	// doit garantir. Figer une valeur absolue figerait aussi la convention
	// d'orientation des faces, qui n'appartient pas a ce module.
	EXPECT_NE (direct.GetVertexTangents ()[3] < 0.f,
	           mirrored.GetVertexTangents ()[3] < 0.f);
}

TEST (TEST_cgmesh_tangents, tangents_are_orthogonal_to_the_normals)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));

	const std::vector<float> &t = m.GetVertexTangents ();
	const std::vector<float> &n = m.GetVertexNormals ();
	ASSERT_EQ (n.size (), 12u);

	for (unsigned int i = 0; i < 4; ++i)
	{
		const float d = t[4*i]*n[3*i] + t[4*i+1]*n[3*i+1] + t[4*i+2]*n[3*i+2];
		EXPECT_NEAR (d, 0.f, kEps) << "Gram-Schmidt, sommet " << i;
	}
}

// ---------------------------------------------------------------------------
//  Critere 2 : sans UV, aucune ecriture
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, a_mesh_without_uv_is_left_untouched)
{
	Mesh m;
	MakeUntexturedTriangle (m);

	const uint64_t revisionBefore = m.GetRevision ();

	EXPECT_FALSE (generateTangents (m));
	EXPECT_EQ (m.GetNTangents (), 0u);
	EXPECT_TRUE (m.GetVertexTangents ().empty ());
	EXPECT_EQ (m.GetRevision (), revisionBefore);
}

TEST (TEST_cgmesh_tangents, clear_removes_the_array)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.GetNTangents (), 4u);

	clearTangents (m);
	EXPECT_EQ (m.GetNTangents (), 0u);
	EXPECT_TRUE (m.GetVertexTangents ().empty ());
}

TEST (TEST_cgmesh_tangents, a_wrongly_sized_array_is_refused)
{
	Mesh m;
	MakeTexturedQuadXY (m);

	// 4 sommets => 16 flottants. Toute autre taille non nulle est refusee : la
	// laisser passer ferait lire hors bornes a la premiere indexation par
	// sommet.
	EXPECT_EQ (m.SetVertexTangents (std::vector<float> (12, 0.f)), -1);
	EXPECT_TRUE (m.GetVertexTangents ().empty ());
	EXPECT_EQ (m.SetVertexTangents (std::vector<float> (16, 0.f)), 0);
	EXPECT_EQ (m.GetNTangents (), 4u);
}

// ---------------------------------------------------------------------------
//  Critere 3 : rien ne se glisse dans les donnees de rendu
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, render_data_has_no_tangents_when_the_mesh_has_none)
{
	Mesh m;
	MakeTexturedQuadXY (m);

	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();

	// Un remplissage par defaut -- une base identite-X uniforme, par exemple --
	// serait indiscernable d'une vraie base pour le consommateur : il croirait
	// pouvoir echantillonner une carte de normales.
	EXPECT_TRUE (rd.tangents.empty ());
	EXPECT_TRUE (rd.texCoords1.empty ());
	EXPECT_FALSE (rd.texCoords.empty ()) << "le jeu 0, lui, est bien la";
}

TEST (TEST_cgmesh_tangents, render_data_carries_tangents_and_uv1_when_present)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.SetTextureCoordinates1 (
		std::vector<float> { 0.f, 0.f, .5f, 0.f, .5f, .5f, 0.f, .5f }), 0);

	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();
	const std::size_t nv = rd.positions.size () / 3;

	ASSERT_GT (nv, 0u);
	EXPECT_EQ (rd.tangents.size (), 4u * nv);
	EXPECT_EQ (rd.texCoords1.size (), 2u * nv);
}

TEST (TEST_cgmesh_tangents, render_data_carries_them_through_the_expansion_path)
{
	// Ombrage FRANC : chaque face est eclatee en ses propres coins, donc le
	// chemin d'expansion et non la recopie de tableaux. Les deux doivent
	// produire des tableaux paralleles aux positions.
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.SetTextureCoordinates1 (
		std::vector<float> { 0.f, 0.f, .5f, 0.f, .5f, .5f, 0.f, .5f }), 0);

	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData (true);
	const std::size_t nv = rd.positions.size () / 3;

	ASSERT_GT (nv, 0u);
	EXPECT_EQ (rd.tangents.size (), 4u * nv);
	EXPECT_EQ (rd.texCoords1.size (), 2u * nv);
	EXPECT_EQ (rd.texCoords.size (), 2u * nv);
}

// ---------------------------------------------------------------------------
//  Second jeu d'UV : contrat de taille et survie aux operations sur sommets
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, uv1_must_be_vertex_parallel)
{
	Mesh m;
	MakeTexturedQuadXY (m);

	EXPECT_EQ (m.SetTextureCoordinates1 (std::vector<float> (6, 0.f)), -1)
		<< "3 entrees pour 4 sommets : refuse, pas tronque";
	EXPECT_EQ (m.GetNTextureCoordinates1 (), 0u);

	EXPECT_EQ (m.SetTextureCoordinates1 (std::vector<float> (8, 0.f)), 0);
	EXPECT_EQ (m.GetNTextureCoordinates1 (), 4u);

	EXPECT_EQ (m.SetTextureCoordinates1 (std::vector<float> ()), 0)
		<< "le tableau vide reste toujours acceptable : c'est l'absence";
	EXPECT_EQ (m.GetNTextureCoordinates1 (), 0u);
}

namespace {

// Deux triangles, deux paires de sommets GEOMETRIQUEMENT confondus : (1, 3) et
// (2, 5). Le jeu 0 est uniforme, donc jamais un critere : ce que la fusion
// decide ne depend plus que du jeu 1, que l'appelant fixe.
void MakeWeldableStrip (Mesh &m, const std::vector<float> &uv1)
{
	m.Init (6, 2);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 0.f, 1.f, 0.f);
	m.SetVertex (3, 1.f, 0.f, 0.f);   // duplicat exact de 1
	m.SetVertex (4, 1.f, 1.f, 0.f);
	m.SetVertex (5, 0.f, 1.f, 0.f);   // duplicat exact de 2
	m.FaceAt (0)->SetTriangle (0, 1, 2);
	m.FaceAt (1)->SetTriangle (3, 4, 5);

	m.SetTextureCoordinates (std::vector<float> (12, 0.f), 6);
	ASSERT_EQ (m.SetTextureCoordinates1 (uv1), 0);
}

} // namespace

TEST (TEST_cgmesh_tangents, merge_vertices_keeps_uv1_vertex_parallel)
{
	Mesh m;
	MakeWeldableStrip (m, std::vector<float> (12, 0.f));

	m.MergeVertices (1e-6f);

	// Jeu 1 uniforme : il ne bloque rien, les deux paires confondues fusionnent.
	// C'est le TEMOIN du test suivant -- sans lui, un jeu 1 qui bloquerait TOUT
	// donnerait le meme resultat que le critere qu'on veut verifier.
	EXPECT_EQ (m.GetNVertices (), 4u);
	EXPECT_EQ (m.GetNTextureCoordinates1 (), m.GetNVertices ())
		<< "un jeu 1 desaccorde du nombre de sommets serait lu de travers";
	EXPECT_EQ (m.GetTextureCoordinates1 ().size (), 2u * m.GetNVertices ());
}

TEST (TEST_cgmesh_tangents, merge_vertices_treats_uv1_as_a_weld_criterion)
{
	// Sommets 1 et 3 : confondus dans l'espace, SEPARES dans le jeu 1 -- deux
	// ilots de carte de lumiere. Les souder ferait heriter l'un des deux de la
	// parametrisation de l'autre, en silence. Sommets 2 et 5 : meme jeu 1, donc
	// soudables. La fusion doit donc rendre 5 sommets et non 4.
	std::vector<float> uv1 (12, 0.f);
	uv1[2 * 1]     = 0.75f;  uv1[2 * 1 + 1] = 0.25f;   // sommet 1
	uv1[2 * 3]     = 0.10f;  uv1[2 * 3 + 1] = 0.90f;   // sommet 3

	Mesh m;
	MakeWeldableStrip (m, uv1);

	m.MergeVertices (1e-6f);

	EXPECT_EQ (m.GetNVertices (), 5u)
		<< "la paire (2,5) fusionne, la paire (1,3) est retenue par le jeu 1";
	ASSERT_EQ (m.GetTextureCoordinates1 ().size (), 2u * m.GetNVertices ());

	// Les DEUX valeurs distinctes survivent : c'est ce qui prouve qu'aucune des
	// deux n'a ete ecrasee par l'autre.
	bool seenA = false, seenB = false;
	const std::vector<float> &out = m.GetTextureCoordinates1 ();
	for (unsigned int i = 0; i < m.GetNVertices (); ++i)
	{
		if (std::fabs (out[2*i] - 0.75f) < kEps && std::fabs (out[2*i+1] - 0.25f) < kEps) seenA = true;
		if (std::fabs (out[2*i] - 0.10f) < kEps && std::fabs (out[2*i+1] - 0.90f) < kEps) seenB = true;
	}
	EXPECT_TRUE (seenA);
	EXPECT_TRUE (seenB);
}

TEST (TEST_cgmesh_tangents, split_by_uv_seams_keeps_uv1_and_tangents_parallel)
{
	// UV PAR COIN : deux faces referencent le meme sommet avec des indices d'UV
	// differents, ce qui rend le decoupage non trivial.
	Mesh m;
	m.Init (4, 2);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	m.FaceAt (0)->SetTriangle (0, 1, 2);
	m.FaceAt (1)->SetTriangle (0, 2, 3);

	// Six entrees d'UV pour quatre sommets : le tableau n'est PAS parallele aux
	// sommets, ce qui est la condition d'entree de SplitVerticesByUVSeams.
	const std::vector<float> uv = {
		0.f, 0.f,  1.f, 0.f,  1.f, 1.f,
		0.f, 0.f,  1.f, 1.f,  0.f, 1.f
	};
	m.SetTextureCoordinates (uv, 6);
	for (unsigned int f = 0; f < 2; ++f)
	{
		m.FaceAt (f)->SetUsesTextureCoordinates (true);
		m.FaceAt (f)->ActivateTextureCoordinatesIndices ();
		for (unsigned int c = 0; c < 3; ++c)
			m.FaceAt (f)->SetTexCoord (c, 3 * f + c);
	}
	m.ComputeNormals ();

	ASSERT_EQ (m.SetTextureCoordinates1 (std::vector<float> (8, 0.25f)), 0);
	ASSERT_TRUE (generateTangents (m));
	const unsigned int before = m.GetNVertices ();

	m.SplitVerticesByUVSeams ();

	// Le resultat est CALCULABLE, donc asserte a l'egalite : le decoupage cree
	// un sommet par cle (sommet, indice d'UV) distincte rencontree sur les
	// coins, soit ici (0,0) (1,1) (2,2) (0,3) (2,4) (3,5) -- six. Une inegalite
	// large passerait meme si la fonction ne faisait rien, le decoupage ne
	// pouvant de toute facon pas reduire le compte.
	EXPECT_EQ (before, 4u);
	EXPECT_EQ (m.GetNVertices (), 6u);
	EXPECT_EQ (m.GetTextureCoordinates1 ().size (), 2u * m.GetNVertices ())
		<< "le jeu 1 suit les duplicats du sommet source";
	EXPECT_EQ (m.GetVertexTangents ().size (), 4u * m.GetNVertices ())
		<< "les tangentes aussi";
	for (float v : m.GetTextureCoordinates1 ())
		EXPECT_NEAR (v, 0.25f, kEps) << "la valeur recopiee, pas un remplissage";
}

TEST (TEST_cgmesh_tangents, changing_the_vertex_count_drops_the_optional_arrays)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.SetTextureCoordinates1 (std::vector<float> (8, 0.f)), 0);

	// Un compte de sommets different rend les deux tableaux structurellement
	// invalides : ils sont VIDES, jamais conserves a l'ancienne taille.
	const std::vector<float> v = { 0.f, 0.f, 0.f,  1.f, 0.f, 0.f,  0.f, 1.f, 0.f };
	m.SetVertices (3, v.data ());

	EXPECT_EQ (m.GetNTangents (), 0u);
	EXPECT_EQ (m.GetNTextureCoordinates1 (), 0u);
}

// ---------------------------------------------------------------------------
//  Charge de BuildPolygonRenderData
// ---------------------------------------------------------------------------
//
// La regle en vigueur (mesh.h, BuildPolygonRenderData) est que cette fonction
// n'est appelee QU'AU (RE)TELEVERSEMENT DU VBO. Ce test en mesure le prix, pour
// que la regle repose sur un chiffre et non sur une intuition.
//
// La comparaison est faite DANS LE MEME PROCESSUS, sur le MEME maillage : la
// seule difference entre les deux mesures est la presence des deux tableaux
// optionnels. C'est la seule facon d'isoler leur cout -- comparer a un chiffre
// releve sur une autre construction melangerait leur effet a celui du
// compilateur et de la machine.
//
// AUCUNE ASSERTION DE TEMPS : une borne chiffree sur une duree est instable en
// integration continue. Les volumes, eux, sont deterministes et assertes.
TEST (TEST_cgmesh_tangents, render_data_cost_of_the_two_optional_arrays)
{
	// Grille de quads : le chemin d'EXPANSION, celui que la dette technique
	// designe comme le plus lourd (chaque n-gon duplique ses coins).
	const unsigned int n = 300;                       // 300 x 300 sommets
	Mesh m;
	m.Init (n * n, (n - 1) * (n - 1));
	for (unsigned int j = 0; j < n; ++j)
		for (unsigned int i = 0; i < n; ++i)
			m.SetVertex (j * n + i, (float)i, (float)j, 0.f);
	for (unsigned int j = 0; j + 1 < n; ++j)
		for (unsigned int i = 0; i + 1 < n; ++i)
			m.FaceAt (j * (n - 1) + i)->SetQuad (
				j * n + i, j * n + i + 1, (j + 1) * n + i + 1, (j + 1) * n + i);

	std::vector<float> uv (2u * n * n);
	for (unsigned int k = 0; k < n * n; ++k)
	{
		uv[2 * k]     = (float)(k % n) / (float)(n - 1);
		uv[2 * k + 1] = (float)(k / n) / (float)(n - 1);
	}
	m.SetTextureCoordinates (uv, n * n);
	m.ComputeNormals ();

	auto bytes = [](const Mesh::PolygonRenderData &rd) {
		return (rd.positions.size () + rd.normals.size () + rd.texCoords.size ()
		        + rd.texCoords1.size () + rd.tangents.size () + rd.colors.size ())
		       * sizeof (float)
		       + rd.indices.size () * sizeof (unsigned int);
	};
	auto timeIt = [&]() {
		double best = 1e30;
		std::size_t vol = 0, verts = 0;
		for (int r = 0; r < 5; ++r)
		{
			const auto t0 = std::chrono::steady_clock::now ();
			const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData ();
			const auto t1 = std::chrono::steady_clock::now ();
			const double ms = std::chrono::duration<double, std::milli> (t1 - t0).count ();
			if (ms < best) best = ms;
			vol = bytes (rd);
			verts = rd.positions.size () / 3;
		}
		return std::make_tuple (best, vol, verts);
	};

	const auto without = timeIt ();

	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.SetTextureCoordinates1 (std::vector<float> (2u * n * n, 0.f)), 0);

	const auto with = timeIt ();

	const std::size_t verts = std::get<2> (without);
	ASSERT_EQ (std::get<2> (with), verts);

	// Volume : deterministe, donc asserte. 24 octets de plus par sommet de
	// rendu (4 flottants de tangente + 2 d'UV1).
	EXPECT_EQ (std::get<1> (with) - std::get<1> (without), 24u * verts);

	std::printf (
		"  BuildPolygonRenderData : %zu sommets de rendu\n"
		"    sans tangentes ni UV1 : %8.3f ms   %9zu octets\n"
		"    avec les deux         : %8.3f ms   %9zu octets  (+%.1f %% de volume)\n",
		verts,
		std::get<0> (without), std::get<1> (without),
		std::get<0> (with),    std::get<1> (with),
		100.0 * (double)(std::get<1> (with) - std::get<1> (without))
		      / (double)std::get<1> (without));
}

// ---------------------------------------------------------------------------
//  Critere 4 : la tangente de REPLI
// ---------------------------------------------------------------------------
//
// Elle sert aux sommets qu'aucun triangle a parametrisation valide n'atteint.
// Le repli constant (1,0,0) de vecna degenere des que la normale est proche de
// X ; celui de ce module est construit sur l'axe le MOINS dominant de la
// normale, donc orthogonal a elle quelle que soit son orientation. Les deux cas
// ci-dessous separent exactement les deux conceptions : le second echoue avec
// un repli constant, le premier non.

namespace {

// Triangle a parametrisation DEGENERE : les trois coins portent la meme UV,
// donc le determinant des derivees est nul et le triangle est ignore. Aucune
// accumulation n'atteint les sommets : tous prennent le repli.
void MakeDegenerateUvTriangle (Mesh &m, float ax, float ay, float az,
                               float bx, float by, float bz)
{
	m.Init (3, 1);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, ax, ay, az);
	m.SetVertex (2, bx, by, bz);
	m.FaceAt (0)->SetTriangle (0, 1, 2);
	m.SetTextureCoordinates (std::vector<float> (6, 0.5f), 3);
	m.ComputeNormals ();
}

} // namespace

TEST (TEST_cgmesh_tangents, the_fallback_stays_orthogonal_to_a_z_normal)
{
	Mesh m;
	MakeDegenerateUvTriangle (m, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f);   // normale +-Z

	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.GetNTangents (), 3u);

	const std::vector<float> &t = m.GetVertexTangents ();
	const std::vector<float> &n = m.GetVertexNormals ();
	ASSERT_EQ (n.size (), 9u);

	for (unsigned int i = 0; i < 3; ++i)
	{
		const float len = std::sqrt (t[4*i]*t[4*i] + t[4*i+1]*t[4*i+1] + t[4*i+2]*t[4*i+2]);
		EXPECT_NEAR (len, 1.f, kEps) << "sommet " << i << " : le repli est UNITAIRE";
		const float d = t[4*i]*n[3*i] + t[4*i+1]*n[3*i+1] + t[4*i+2]*n[3*i+2];
		EXPECT_NEAR (d, 0.f, kEps) << "sommet " << i << " : le repli est ORTHOGONAL";
		EXPECT_NEAR (std::fabs (t[4*i + 3]), 1.f, kEps);
	}
}

TEST (TEST_cgmesh_tangents, the_fallback_does_not_degenerate_on_an_x_normal)
{
	// Triangle dans le plan YZ : normale +-X. Un repli constant (1,0,0) y
	// serait COLINEAIRE a la normale -- base TBN sans rang, carte de normales
	// echantillonnee dans un repere plat. C'est le cas que l'axe le moins
	// dominant est cense couvrir.
	Mesh m;
	MakeDegenerateUvTriangle (m, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

	ASSERT_TRUE (generateTangents (m));
	const std::vector<float> &t = m.GetVertexTangents ();
	const std::vector<float> &n = m.GetVertexNormals ();
	ASSERT_EQ (t.size (), 12u);
	ASSERT_EQ (n.size (), 9u);
	ASSERT_NEAR (std::fabs (n[0]), 1.f, kEps) << "la monture doit bien donner une normale +-X";

	for (unsigned int i = 0; i < 3; ++i)
	{
		const float len = std::sqrt (t[4*i]*t[4*i] + t[4*i+1]*t[4*i+1] + t[4*i+2]*t[4*i+2]);
		EXPECT_NEAR (len, 1.f, kEps) << "sommet " << i;
		const float d = t[4*i]*n[3*i] + t[4*i+1]*n[3*i+1] + t[4*i+2]*n[3*i+2];
		EXPECT_NEAR (d, 0.f, kEps) << "sommet " << i;
	}
}

TEST (TEST_cgmesh_tangents, without_normals_the_tangent_is_normalized_and_w_is_plus_one)
{
	// Chemin SANS normales : pas d'orthogonalisation possible, la tangente
	// accumulee est simplement normalisee et le signe de main vaut +1 faute de
	// quoi que ce soit qui puisse le determiner.
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_EQ (m.SetVertexNormals (std::vector<float> ()), 0);
	ASSERT_TRUE (m.GetVertexNormals ().empty ());

	ASSERT_TRUE (generateTangents (m));
	const std::vector<float> &t = m.GetVertexTangents ();
	ASSERT_EQ (t.size (), 16u);

	for (unsigned int i = 0; i < 4; ++i)
	{
		EXPECT_NEAR (t[4*i + 0], 1.f, kEps) << "sommet " << i;
		EXPECT_NEAR (t[4*i + 1], 0.f, kEps) << "sommet " << i;
		EXPECT_NEAR (t[4*i + 2], 0.f, kEps) << "sommet " << i;
		EXPECT_NEAR (t[4*i + 3], 1.f, kEps) << "sommet " << i;
	}
}

// ---------------------------------------------------------------------------
//  Critere 5 : le JEU d'UV est un parametre, pas une constante
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, the_requested_uv_set_is_the_one_used)
{
	// Deux parametrisations ORTHOGONALES sur le meme quad :
	//   jeu 0 : u croit avec x  -> dP/du = +X
	//   jeu 1 : u croit avec y  -> dP/du = +Y
	// La tangente rendue permet donc de dire, sans ambiguite, lequel a servi.
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_EQ (m.SetTextureCoordinates1 (
		std::vector<float> { 0.f, 0.f,  0.f, 1.f,  1.f, 1.f,  1.f, 0.f }), 0);

	ASSERT_TRUE (generateTangents (m, 0));
	EXPECT_NEAR (m.GetVertexTangents ()[0], 1.f, kEps);
	EXPECT_NEAR (m.GetVertexTangents ()[1], 0.f, kEps);

	ASSERT_TRUE (generateTangents (m, 1));
	EXPECT_NEAR (m.GetVertexTangents ()[0], 0.f, kEps)
		<< "le jeu 1 demande, le jeu 0 obtenu : la base est fausse et unitaire, "
		   "donc indiscernable d'une bonne chez un lecteur conforme";
	EXPECT_NEAR (m.GetVertexTangents ()[1], 1.f, kEps);
}

TEST (TEST_cgmesh_tangents, an_empty_requested_uv_set_writes_nothing)
{
	// Le jeu 0 est plein, le jeu 1 vide : la demande porte sur le jeu 1, donc
	// rien ne doit etre ecrit. Se rabattre en silence sur le jeu 0 rendrait une
	// base pour la mauvaise parametrisation.
	Mesh m;
	MakeTexturedQuadXY (m);

	EXPECT_FALSE (generateTangents (m, 1));
	EXPECT_EQ (m.GetNTangents (), 0u);
}

// ---------------------------------------------------------------------------
//  Critere 6 : des tangentes PERIMEES sont detectables
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, moving_the_vertices_invalidates_the_tangents)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	ASSERT_TRUE (generateTangents (m));
	EXPECT_TRUE (m.AreTangentsValid ());

	// MEME COMPTE de sommets : les tangentes SURVIVENT -- c'est la regle de
	// SetVertices, et c'est exactement le chemin de smoothing_laplacian et
	// smoothing_taubin. Elles decrivent pourtant la geometrie d'AVANT.
	const std::vector<float> moved = {
		0.f, 0.f, 0.f,   1.f, 0.f, 1.f,   1.f, 1.f, 2.f,   0.f, 1.f, 1.f
	};
	m.SetVertices (4, moved.data ());

	EXPECT_EQ (m.GetNTangents (), 4u) << "elles survivent, c'est le probleme";
	EXPECT_FALSE (m.AreTangentsValid ())
		<< "et c'est la seule chose qui permette de s'en apercevoir";

	// Regenerees sur la nouvelle geometrie : valides de nouveau.
	m.ComputeNormals ();
	ASSERT_TRUE (generateTangents (m));
	EXPECT_TRUE (m.AreTangentsValid ());
}

TEST (TEST_cgmesh_tangents, a_mesh_without_tangents_never_reports_them_valid)
{
	Mesh m;
	MakeTexturedQuadXY (m);
	EXPECT_FALSE (m.AreTangentsValid ()) << "aucune tangente : rien a valider";

	ASSERT_TRUE (generateTangents (m));
	ASSERT_TRUE (m.AreTangentsValid ());

	clearTangents (m);
	EXPECT_FALSE (m.AreTangentsValid ());
}

// ---------------------------------------------------------------------------
//  Critere 7 : l'ALLER-RETOUR GLB
// ---------------------------------------------------------------------------
//
// Le chemin d'ecriture (TANGENT, TEXCOORD_1) et le chemin de lecture ne sont
// verifiables que l'un par l'autre : aucun fichier du depot ne porte ces deux
// attributs. Les montures sont donc FABRIQUEES ici avec tinygltf -- que
// src/cgmesh/CMakeLists.txt propage en PUBLIC pour cette raison -- et validees
// avant de servir d'oracle.

namespace {

// ---------------------------------------------------------------------------
//  Fichier de travail : UN PAR CAS DE TEST
// ---------------------------------------------------------------------------
//
// gtest_discover_tests declare un test ctest PAR CAS, tous dans le meme
// repertoire de travail, et la CI lance `ctest -j $(nproc)`. Deux cas qui
// partagent un nom relatif sont donc deux PROCESSUS concurrents sur le meme
// fichier : l'ouverture en "wb" de l'un tronque la monture que l'autre est en
// train de relire, et la monture ainsi melangee peut aussi faire PASSER un test
// sur le contenu d'un autre. Le nom du cas en cours, unique par construction,
// donne a chacun son fichier ; sa destruction l'efface, y compris quand un
// ASSERT interrompt le test.
class TestFile
{
public:
	explicit TestFile (const char* suffix)
	{
		const ::testing::TestInfo* info =
			::testing::UnitTest::GetInstance ()->current_test_info ();
		m_path = std::string ("./tu_tangents_")
		       + (info ? info->name () : "orphan") + suffix;
	}
	~TestFile () { std::remove (m_path.c_str ()); }

	TestFile (const TestFile&) = delete;
	TestFile& operator= (const TestFile&) = delete;

	// Les deux formes attendues par les appelants : l'API C (std::fopen,
	// VMeshesIO::load) et celle de tinygltf.
	operator const char* () const { return m_path.c_str (); }
	operator const std::string& () const { return m_path; }

private:
	std::string m_path;
};

// Ecrit `bytes` sur disque. Rend faux plutot que d'abandonner un fichier
// tronque derriere lui.
bool WriteBytes (const char* path, const std::string& bytes)
{
	FILE* fp = std::fopen (path, "wb");
	if (!fp) return false;
	const size_t n = std::fwrite (bytes.data (), 1, bytes.size (), fp);
	std::fclose (fp);
	if (n != bytes.size ()) { std::remove (path); return false; }
	return true;
}

// Deux triangles coplanaires, UV PAR SOMMET, tangentes generees et jeu 1
// distinct sommet par sommet. Toutes les faces sont des triangles et les UV
// sont paralleles aux sommets : BuildPolygonRenderData reste sur le chemin de
// RECOPIE, donc l'ordre des sommets est preserve et la comparaison terme a
// terme est licite.
void MakeRoundTripMesh (Mesh &m)
{
	m.Init (4, 2);
	m.SetVertex (0,  0.f,  0.f, 0.f);
	m.SetVertex (1, 10.f,  0.f, 0.f);
	m.SetVertex (2, 10.f, 10.f, 0.f);
	m.SetVertex (3,  0.f, 10.f, 0.f);
	m.FaceAt (0)->SetTriangle (0, 1, 2);
	m.FaceAt (1)->SetTriangle (0, 2, 3);

	m.SetTextureCoordinates (
		std::vector<float> { 0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f }, 4);
	m.ComputeNormals ();
	ASSERT_TRUE (generateTangents (m));
	ASSERT_EQ (m.SetTextureCoordinates1 (
		std::vector<float> { .1f, .2f,  .3f, .4f,  .5f, .6f,  .7f, .8f }), 0);
}

} // namespace

TEST (TEST_cgmesh_tangents, glb_round_trip_carries_tangents_and_the_second_uv_set)
{
	const TestFile kRoundTrip (".glb");
	Mesh m;
	MakeRoundTripMesh (m);

	const std::string bytes = MeshIO::export_glb_bytes (m);
	ASSERT_FALSE (bytes.empty ()) << "l'ecriture a echoue : rien a relire";
	ASSERT_TRUE (WriteBytes (kRoundTrip, bytes));

	// VALIDATION DE LA MONTURE. Sans elle, un fichier ou les deux attributs
	// manquent ferait passer une relecture qui, elle aussi, les ignore.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadBinaryFromFile (&check, &err, &warn, kRoundTrip))
			<< err;
		ASSERT_EQ (check.meshes.size (), 1u);
		ASSERT_EQ (check.meshes[0].primitives.size (), 1u);
		const auto& attrs = check.meshes[0].primitives[0].attributes;
		ASSERT_NE (attrs.find ("TANGENT"),    attrs.end ()) << "TANGENT non ecrit";
		ASSERT_NE (attrs.find ("TEXCOORD_1"), attrs.end ()) << "TEXCOORD_1 non ecrit";
		EXPECT_EQ (check.accessors[attrs.at ("TANGENT")].type,    TINYGLTF_TYPE_VEC4);
		EXPECT_EQ (check.accessors[attrs.at ("TEXCOORD_1")].type, TINYGLTF_TYPE_VEC2);
		EXPECT_EQ (check.accessors[attrs.at ("TANGENT")].count,   4u);
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kRoundTrip));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	ASSERT_EQ (back->GetNVertices (), 4u);

	// L'echelle et la rotation de l'ecriture (0,001 et Rx(-90)) sont
	// exactement inversees a la lecture (1000 et Rx(+90)) : les positions
	// reviennent en millimetres, dans le repere de depart.
	const std::vector<float>& v = back->GetVertices ();
	ASSERT_EQ (v.size (), 12u);
	const float expected[12] = {
		 0.f,  0.f, 0.f,
		10.f,  0.f, 0.f,
		10.f, 10.f, 0.f,
		 0.f, 10.f, 0.f
	};
	for (int i = 0; i < 12; ++i)
		EXPECT_NEAR (v[i], expected[i], 1e-3f) << "position, composante " << i;

	// CONTRAT DE TAILLE des deux tableaux, cote lecture : refuses s'ils ne sont
	// pas paralleles aux sommets, donc leur seule presence l'atteste.
	ASSERT_EQ (back->GetNTangents (), 4u);
	ASSERT_EQ (back->GetNTextureCoordinates1 (), 4u);

	// LES TANGENTES DE L'AUTEUR SORTENT FRAICHES. Elles sont posees en FIN
	// d'import : posees avant l'ecriture des faces, l'estampille serait plus
	// vieille que la revision et un consommateur qui suit le contrat
	// (« si !AreTangentsValid() alors regenerer ») JETTERAIT la base de
	// l'auteur pour une reconstruction approximative.
	EXPECT_TRUE (back->AreTangentsValid ())
		<< "tangentes du fichier declarees perimees des leur lecture";

	// VALIDATION DE LA MONTURE : le maillage source porte bien +1, sans quoi le
	// litteral attendu plus bas serait le mauvais oracle.
	for (unsigned int i = 0; i < 4; ++i)
		ASSERT_NEAR (m.GetVertexTangents ()[4*i + 3], 1.0f, 1e-6f)
			<< "monture : signe de main du maillage source, sommet " << i;

	const std::vector<float>& t = back->GetVertexTangents ();
	for (unsigned int i = 0; i < 4; ++i)
	{
		EXPECT_NEAR (t[4*i + 0], 1.f, 1e-3f) << "tangente, sommet " << i;
		EXPECT_NEAR (t[4*i + 1], 0.f, 1e-3f) << "tangente, sommet " << i;
		EXPECT_NEAR (t[4*i + 2], 0.f, 1e-3f) << "tangente, sommet " << i;
		// La transformation d'aller-retour a un determinant POSITIF : elle
		// n'est pas miroir, le signe de main traverse donc inchange. La
		// valeur attendue est le LITTERAL +1, pas celle du maillage source :
		// une comparaison au source passerait si l'ecriture et la lecture
		// inversaient toutes deux `w`.
		EXPECT_NEAR (t[4*i + 3], 1.0f, 1e-6f)
			<< "signe de main, sommet " << i;
	}

	const std::vector<float>& uv1 = back->GetTextureCoordinates1 ();
	const float expectedUv1[8] = { .1f, .2f,  .3f, .4f,  .5f, .6f,  .7f, .8f };
	for (int i = 0; i < 8; ++i)
		EXPECT_NEAR (uv1[i], expectedUv1[i], 1e-6f) << "jeu 1, composante " << i;
}

// ---------------------------------------------------------------------------
//  Critere 8 : un accessor MENSONGER ne fait pas lire hors du tampon
// ---------------------------------------------------------------------------
//
// tinygltf ne confronte JAMAIS un accessor a la taille du tampon qui le porte
// (ParseAccessor lit `count` et `bufferView` tels quels). Un .glb est une
// entree non fiable -- c'est l'ouverture de fichier de sinaia -- donc c'est au
// lecteur d'attributs de le faire. CWE-125.

namespace {

// Trois sommets, un TEXCOORD_1 dont l'accessor ment. `count` et `bufferView`
// sont les deux mensonges possibles : une portee qui deborde le tampon, et un
// indice de vue qui n'existe pas.
bool WriteGlbWithLyingUv (const char* path, size_t uvCount, int uvBufferView)
{
	const float positions[9] = {
		0.f, 0.f, 0.f,
		1.f, 0.f, 0.f,
		0.f, 1.f, 0.f
	};
	const float uv[2] = { 0.f, 0.f };            // UNE SEULE coordonnee presente

	tinygltf::Model model;
	model.asset.version = "2.0";
	model.asset.generator = "TU lying accessor fixture";

	model.buffers.resize (1);
	std::vector<unsigned char>& bin = model.buffers[0].data;
	bin.resize (sizeof (positions) + sizeof (uv));
	std::memcpy (bin.data (), positions, sizeof (positions));
	std::memcpy (bin.data () + sizeof (positions), uv, sizeof (uv));

	tinygltf::BufferView posView;
	posView.buffer     = 0;
	posView.byteOffset = 0;
	posView.byteLength = sizeof (positions);
	posView.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
	model.bufferViews.push_back (posView);

	tinygltf::BufferView uvView;
	uvView.buffer     = 0;
	uvView.byteOffset = sizeof (positions);
	uvView.byteLength = sizeof (uv);
	uvView.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
	model.bufferViews.push_back (uvView);

	tinygltf::Accessor pos;
	pos.bufferView    = 0;
	pos.byteOffset    = 0;
	pos.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	pos.count         = 3;
	pos.type          = TINYGLTF_TYPE_VEC3;
	pos.minValues     = { 0.0, 0.0, 0.0 };
	pos.maxValues     = { 1.0, 1.0, 0.0 };
	model.accessors.push_back (pos);

	tinygltf::Accessor uvAcc;
	uvAcc.bufferView    = uvBufferView;
	uvAcc.byteOffset    = 0;
	uvAcc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	uvAcc.count         = uvCount;
	uvAcc.type          = TINYGLTF_TYPE_VEC2;
	model.accessors.push_back (uvAcc);

	tinygltf::Primitive prim;
	prim.mode = TINYGLTF_MODE_TRIANGLES;
	prim.attributes["POSITION"]   = 0;
	prim.attributes["TEXCOORD_1"] = 1;
	prim.indices = -1;

	tinygltf::Mesh gltfMesh;
	gltfMesh.name = "lying";
	gltfMesh.primitives.push_back (prim);
	model.meshes.push_back (gltfMesh);

	tinygltf::Node node;
	node.mesh = 0;
	model.nodes.push_back (node);

	tinygltf::Scene scene;
	scene.nodes.push_back (0);
	model.scenes.push_back (scene);
	model.defaultScene = 0;

	tinygltf::TinyGLTF writer;
	return writer.WriteGltfSceneToFile (&model, path,
	                                    /*embedImages=*/true, /*embedBuffers=*/true,
	                                    /*prettyPrint=*/false, /*writeBinary=*/true);
}

// L'import doit RENDRE LA MAIN sans planter, sur un maillage complet et sans
// second jeu d'UV : l'accessor est refuse, pas tronque.
void ExpectLyingUvIsRefused (const char* path, const char* what)
{
	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, path)) << what;
	ASSERT_EQ (vm.GetNMeshes (), 1u) << what;
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr) << what;
	EXPECT_EQ (back->GetNVertices (), 3u) << what;
	EXPECT_EQ (back->GetNTextureCoordinates1 (), 0u)
		<< what << " : un jeu 1 issu d'un accessor refuse serait du bruit";
}

} // namespace

TEST (TEST_cgmesh_tangents, an_accessor_count_beyond_the_buffer_is_refused)
{
	const TestFile kLyingUv (".glb");
	// count = 10^9 pour 8 octets de donnees. Sans controle prealable, le
	// dimensionnement du tableau de sortie reserve 8 Go AVANT toute lecture,
	// puis la boucle lit 8 Go au-dela d'un tampon de 44 octets.
	ASSERT_TRUE (WriteGlbWithLyingUv (kLyingUv, 1000000000u, 1));

	// VALIDATION DE LA MONTURE : si tinygltf assainissait `count`, le test
	// passerait sans rien prouver.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadBinaryFromFile (&check, &err, &warn, kLyingUv)) << err;
		ASSERT_EQ (check.accessors.size (), 2u);
		EXPECT_EQ (check.accessors[1].count, 1000000000u)
			<< "tinygltf a corrige le compte : la monture ne vaut plus rien";
		ASSERT_EQ (check.bufferViews.size (), 2u);
		EXPECT_LT (check.bufferViews[1].byteLength, 16u);
	}

	ExpectLyingUvIsRefused (kLyingUv, "count hors tampon");
}

TEST (TEST_cgmesh_tangents, an_accessor_buffer_view_index_out_of_range_is_refused)
{
	const TestFile kLyingUv (".glb");
	// bufferView = 7 pour deux vues declarees : l'indexation non bornee
	// dereferencait une BufferView arbitraire, puis un Buffer arbitraire.
	ASSERT_TRUE (WriteGlbWithLyingUv (kLyingUv, 3u, 7));

	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadBinaryFromFile (&check, &err, &warn, kLyingUv)) << err;
		ASSERT_EQ (check.accessors.size (), 2u);
		EXPECT_EQ (check.accessors[1].bufferView, 7);
		EXPECT_EQ (check.bufferViews.size (), 2u);
	}

	ExpectLyingUvIsRefused (kLyingUv, "bufferView hors bornes");
}

// ---------------------------------------------------------------------------
//  Critere 8bis : un `count` qui FAIT DEBORDER size_t n'ecrit pas hors du tas
// ---------------------------------------------------------------------------
//
// Le controle de portee `base + (count-1)*stride + element <= taille` est
// franchissable : le produit s'enroule modulo 2^64. Quand il retombe sur une
// PETITE valeur, le controle PASSE. Et `resize (count * K)` s'enroule lui aussi
// -- il alloue alors une poignee d'elements pendant que la boucle, dont la
// borne ne s'enroule pas, tourne `count` fois. C'est une ECRITURE hors du tas,
// pas seulement un deni de service : CWE-787.
//
// Les trois copieurs sont eprouves separement. Les comptes ci-dessous ne sont
// pas des nombres au hasard : chacun est la solution du systeme
// « (count-1)*stride ≡ r (mod 2^64) avec base+r+element <= taille du tampon »
// et « count*K (mod 2^64) petit », pour le copieur vise.
//
// Les montures sont ecrites en .gltf TEXTE : un `count` de 2^63 doit traverser
// le JSON tel quel, ce qu'un ecrivain GLB ne garantit pas.

namespace {

// La monture ne vaut que si tinygltf laisse passer le compte : sans cela le
// test passerait sans rien prouver.
void ExpectCountSurvivedParsing (const char* path, int accessorIndex, size_t expected)
{
	tinygltf::Model check;
	tinygltf::TinyGLTF loader;
	std::string err, warn;
	ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, path)) << err;
	ASSERT_GT ((int) check.accessors.size (), accessorIndex);
	EXPECT_EQ (check.accessors[accessorIndex].count, expected)
		<< "tinygltf a corrige le compte : la monture ne vaut plus rien";
}

} // namespace

TEST (TEST_cgmesh_tangents, a_vec2_count_that_overflows_the_span_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// TEXCOORD_1, VEC2 FLOAT, foulee 8, base 36, tampon de 44 octets.
	// count = 2^63+1  ->  (count-1)*8 ≡ 0 (mod 2^64), donc portee = 44 : le
	// controle PASSE. count*2 ≡ 2, donc resize (2) reussit et la boucle ecrit
	// 2^63 paires de flottants dans un tampon de deux.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAA="}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":8}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5126,"count":9223372036854775809,"type":"VEC2"}],
"meshes":[{"name":"overflow","primitives":[{"mode":4,"attributes":{"POSITION":0,"TEXCOORD_1":1}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));
	ASSERT_NO_FATAL_FAILURE (ExpectCountSurvivedParsing (kOverflowGltf, 1, 9223372036854775809ull));

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_EQ (back->GetNTextureCoordinates1 (), 0u)
		<< "un jeu 1 issu d'un accessor refuse serait du bruit";
}

TEST (TEST_cgmesh_tangents, a_vec3_count_that_overflows_the_span_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// POSITION, VEC3 FLOAT, foulee 12, base 0, tampon de 16 octets.
	// count = 12297829382473034412  ->  (count-1)*12 ≡ 4 (mod 2^64), donc
	// portee = 16 : le controle PASSE. count*3 ≡ 4, donc resize (4) reussit et
	// la boucle deborde des le deuxieme tour.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":16,"uri":"data:application/octet-stream;base64,AACAPwAAAEAAAEBAAACAQA=="}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":16}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":12297829382473034412,"type":"VEC3","min":[0,0,0],"max":[1,1,1]}],
"meshes":[{"name":"overflow","primitives":[{"mode":4,"attributes":{"POSITION":0}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));
	ASSERT_NO_FATAL_FAILURE (ExpectCountSurvivedParsing (kOverflowGltf, 0, 12297829382473034412ull));

	// La primitive entiere est refusee : sans positions il n'y a pas de
	// maillage. Le contrat verifie est que l'import RENDE LA MAIN.
	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	EXPECT_EQ (vm.GetNMeshes (), 0u)
		<< "une primitive dont les positions sont refusees ne doit rien produire";
}

TEST (TEST_cgmesh_tangents, a_vec4_count_that_overflows_the_span_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// TANGENT, VEC4 FLOAT, foulee 16, base 36, tampon de 52 octets.
	// count = 2^62+1  ->  (count-1)*16 ≡ 0 (mod 2^64), donc portee = 52 : le
	// controle PASSE. count*4 ≡ 4, donc resize (4) reussit.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":52,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAAAACAPw=="}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":16}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5126,"count":4611686018427387905,"type":"VEC4"}],
"meshes":[{"name":"overflow","primitives":[{"mode":4,"attributes":{"POSITION":0,"TANGENT":1}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));
	ASSERT_NO_FATAL_FAILURE (ExpectCountSurvivedParsing (kOverflowGltf, 1, 4611686018427387905ull));

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_EQ (back->GetNTangents (), 0u)
		<< "des tangentes issues d'un accessor refuse seraient du bruit";
}

// ---------------------------------------------------------------------------
//  Critere 8ter : un indice d'accesseur hors bornes ne dereference rien
// ---------------------------------------------------------------------------
//
// `primitive.attributes` est un map<string,int> que ParsePrimitive remplit sans
// jamais le confronter a model.accessors.size(). `model.accessors[idx]` est donc
// une indexation par un entier du fichier. Meme chose pour `primitive.indices`.

TEST (TEST_cgmesh_tangents, an_attribute_accessor_index_out_of_range_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// TANGENT designe l'accesseur 4000000 pour deux declares : ~1 Go au-dela du
	// tableau. `count`, `type` et `bufferView` etaient lus sur cette memoire.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":52,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAAAACAPw=="}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":16}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5126,"count":1,"type":"VEC4"}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"attributes":{"POSITION":0,"TANGENT":4000000,"TEXCOORD_1":4000001}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer l'indice sauvage.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.accessors.size (), 2u);
		ASSERT_EQ (check.meshes.size (), 1u);
		ASSERT_EQ (check.meshes[0].primitives.size (), 1u);
		const auto& attrs = check.meshes[0].primitives[0].attributes;
		ASSERT_NE (attrs.find ("TANGENT"), attrs.end ());
		EXPECT_EQ (attrs.at ("TANGENT"), 4000000);
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_EQ (back->GetNTangents (), 0u);
	EXPECT_EQ (back->GetNTextureCoordinates1 (), 0u);
}

// L'INDICE D'ACCESSEUR DE `primitive.indices`, LUI, EST DEJA BORNE PAR
// TINYGLTF : LoadASCIIFromFile rejette le fichier entier (« primitive indices
// accessor out of bounds », tiny_gltf.h:6261) et valide aussi le `bufferView`
// de cet accesseur (`:6272`). Le controle du lecteur est donc une defense en
// profondeur non atteignable, et non testable — d'ou l'absence de test dessus.
// Ce qui n'est PAS borne a ce niveau, c'est le `buffer` de la vue.

TEST (TEST_cgmesh_tangents, an_index_buffer_view_with_a_missing_buffer_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// La vue 1 designe le tampon 5 pour un seul declare. tinygltf valide
	// l'accesseur d'indices et son `bufferView`, mais JAMAIS le `buffer` de
	// cette vue : `model.buffers[5]` dereferencait un std::vector hors du
	// tableau, puis lisait les indices a travers son pointeur de donnees.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":5,"byteOffset":0,"byteLength":6}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"indices":1,"attributes":{"POSITION":0}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer le tampon absent.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.buffers.size (), 1u);
		ASSERT_EQ (check.bufferViews.size (), 2u);
		EXPECT_EQ (check.bufferViews[1].buffer, 5)
			<< "tinygltf a borne le tampon : la monture ne vaut plus rien";
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	EXPECT_EQ (vm.GetNMeshes (), 0u)
		<< "une primitive dont le tampon d'indices n'existe pas ne doit rien produire";
}

TEST (TEST_cgmesh_tangents, an_index_count_that_overflows_the_span_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// Meme arithmetique que pour les copieurs, cote indices : count = 2^63+1
	// sur des indices de 2 octets donne count*2 ≡ 2 (mod 2^64), donc une
	// portee de 38 dans un tampon de 42 -- le controle de portee PASSE. Seul
	// le plafond `count <= taille du tampon`, pose AVANT la multiplication,
	// arrete la boucle, qui tournerait sinon count/3 fois.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5123,"count":9223372036854775809,"type":"SCALAR"}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"indices":1,"attributes":{"POSITION":0}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer le compte.
	ASSERT_NO_FATAL_FAILURE (ExpectCountSurvivedParsing (kOverflowGltf, 1, 9223372036854775809ull));

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	EXPECT_EQ (vm.GetNMeshes (), 0u)
		<< "une primitive dont le compte d'indices deborde ne doit rien produire";
}

TEST (TEST_cgmesh_tangents, a_position_accessor_index_out_of_range_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// POSITION designe l'accesseur 4 000 000 pour un seul declare.
	// `primitive.attributes` est un map<string,int> que ParsePrimitive
	// remplit sans le confronter a model.accessors.size() : indexer
	// directement, c'est indexer par un entier arbitraire de l'entree.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"attributes":{"POSITION":4000000}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer l'indice sauvage.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.accessors.size (), 1u);
		ASSERT_EQ (check.meshes.size (), 1u);
		ASSERT_EQ (check.meshes[0].primitives.size (), 1u);
		const auto& attrs = check.meshes[0].primitives[0].attributes;
		ASSERT_NE (attrs.find ("POSITION"), attrs.end ());
		EXPECT_EQ (attrs.at ("POSITION"), 4000000)
			<< "tinygltf a borne l'indice : la monture ne vaut plus rien";
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	EXPECT_EQ (vm.GetNMeshes (), 0u)
		<< "une primitive sans accesseur de positions ne doit rien produire";
}

TEST (TEST_cgmesh_tangents, a_texcoord_accessor_index_out_of_range_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// Meme defaut que ci-dessus, sur TEXCOORD_0 : la primitive reste lisible,
	// c'est le jeu d'UV qui doit sortir vide plutot que d'etre lu a travers un
	// accesseur qui n'existe pas.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"attributes":{"POSITION":0,"TEXCOORD_0":4000000}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer l'indice sauvage.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.accessors.size (), 1u);
		ASSERT_EQ (check.meshes.size (), 1u);
		ASSERT_EQ (check.meshes[0].primitives.size (), 1u);
		const auto& attrs = check.meshes[0].primitives[0].attributes;
		ASSERT_NE (attrs.find ("TEXCOORD_0"), attrs.end ());
		EXPECT_EQ (attrs.at ("TEXCOORD_0"), 4000000)
			<< "tinygltf a borne l'indice : la monture ne vaut plus rien";
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_EQ (back->GetNTextureCoordinates (), 0u);
}

TEST (TEST_cgmesh_tangents, an_accessor_byte_offset_that_wraps_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// `base = view.byteOffset + accessor.byteOffset` est une somme de deux
	// size_t que tinygltf ne valide ni l'un ni l'autre. Ici
	// accessor.byteOffset = 2^64 - 24 : la portee calculee vaut
	// base + 2*8 + 8 = 2^64, soit 0 apres bouclage, donc le controle de
	// portee PASSE -- et la boucle lit alors 24 octets AVANT le tampon, dont
	// elle fait un jeu d'UV de trois paires declare valide.
	//
	// L'oracle est le COMPTE, pas un plantage : la lecture tombe dans
	// l'en-tete de bloc de l'allocateur et ne faute pas de facon fiable.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":0,"byteOffset":18446744073709551592,"componentType":5126,"count":3,"type":"VEC2"}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"attributes":{"POSITION":0,"TEXCOORD_1":1}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer le decalage.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.accessors.size (), 2u);
		EXPECT_EQ (check.accessors[1].byteOffset, (size_t) 18446744073709551592ull)
			<< "tinygltf a assaini le decalage : la monture ne vaut plus rien";
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_EQ (back->GetNTextureCoordinates1 (), 0u)
		<< "un jeu 1 lu avant le tampon serait du bruit declare valide";
}

TEST (TEST_cgmesh_tangents, an_index_span_beyond_the_buffer_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// L'accesseur d'indices declare 30 elements pour une vue de 6 octets,
	// dans un tampon de 42. Les deux boucles indexees d'ecriture des faces
	// adressent le tampon par un pointeur brut -- sans passer par un copieur --
	// donc le controle de portee que les copieurs portent pour les attributs
	// doit exister a part pour les indices. Sans lui, la boucle lit
	// 60 octets a partir du 36e d'un tampon qui en compte 42, et alimente
	// SetVertex avec ce qu'elle y trouve.
	//
	// 30 <= 42 : le PLAFOND passe. Ce fichier isole donc le controle de
	// PORTEE, l'autre test isolant le plafond.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5123,"count":30,"type":"SCALAR"}],
"meshes":[{"name":"wild","primitives":[{"mode":4,"indices":1,"attributes":{"POSITION":0}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf laisse bien passer le compte, et la
	// vue reste trop courte pour lui. Sans ce controle, un lecteur qui
	// assainirait `count` ferait passer le test sans rien prouver.
	ASSERT_NO_FATAL_FAILURE (ExpectCountSurvivedParsing (kOverflowGltf, 1, 30u));
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.bufferViews.size (), 2u);
		EXPECT_EQ (check.bufferViews[1].byteLength, 6u);
		ASSERT_EQ (check.buffers.size (), 1u);
		EXPECT_EQ (check.buffers[0].data.size (), 42u);
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	EXPECT_EQ (vm.GetNMeshes (), 0u)
		<< "une primitive dont les indices depassent le tampon ne doit rien produire";
}

// ---------------------------------------------------------------------------
//  Critere 8quater : un componentType hors liste blanche ne fabrique pas d'UV
// ---------------------------------------------------------------------------

TEST (TEST_cgmesh_tangents, an_unlisted_uv_component_type_is_refused)
{
	const TestFile kOverflowGltf (".gltf");
	// TEXCOORD_0 en UNSIGNED_INT (5125) : ParseAccessor l'accepte -- il ne
	// valide que BYTE <= type <= DOUBLE -- et le repli du convertisseur
	// rendait 0 pour chaque composante. Un jeu d'UV TOUTES NULLES, declare
	// valide, qui aurait ensuite servi de base a la generation des tangentes.
	const std::string gltf = R"({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":60,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAABAAAAAAAAAAAAAAABAAAA"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},
               {"buffer":0,"byteOffset":36,"byteLength":24}],
"accessors":[{"bufferView":0,"byteOffset":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
             {"bufferView":1,"byteOffset":0,"componentType":5125,"count":3,"type":"VEC2"}],
"meshes":[{"name":"badtype","primitives":[{"mode":4,"attributes":{"POSITION":0,"TEXCOORD_0":1}}]}],
"nodes":[{"mesh":0}],
"scenes":[{"nodes":[0]}],
"scene":0
})";
	ASSERT_TRUE (WriteBytes (kOverflowGltf, gltf));

	// VALIDATION DE LA MONTURE : tinygltf accepte bien 5125 sur un TEXCOORD.
	{
		tinygltf::Model check;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		ASSERT_TRUE (loader.LoadASCIIFromFile (&check, &err, &warn, kOverflowGltf)) << err;
		ASSERT_EQ (check.accessors.size (), 2u);
		EXPECT_EQ (check.accessors[1].componentType, 5125);
	}

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kOverflowGltf));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	EXPECT_EQ (back->GetNVertices (), 3u);
	EXPECT_TRUE (back->GetTextureCoordinates ().empty ())
		<< "un type de composante non traite produisait un jeu d'UV nulles "
		   "declare valide";
}

// ---------------------------------------------------------------------------
//  Critere 9 : a l'import, la base est batie sur le jeu de la CARTE DE NORMALES
// ---------------------------------------------------------------------------
//
// glTF 2.0 attache le TANGENT aux « texture coordinates associated with the
// normal texture ». Une base batie sur le jeu de la couleur de base reste
// unitaire et orthogonale a la normale : elle est INDISCERNABLE d'une bonne
// pour un lecteur conforme, et l'eclairage est faux sans aucun signal.

namespace {

// PNG 1x1 RGBA, le plus petit contenu qui fasse exister une carte : un
// emplacement de MaterialPbr sans image est un emplacement VIDE
// (cf. cgpbr::TextureRef), donc sans lui aucune carte de normales n'existe et
// aucune tangente n'est generee.
const unsigned char kOnePixelPng[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
	0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x68, 0x68, 0xf8, 0xff,
	0x1f, 0x00, 0x06, 0x82, 0x02, 0xff, 0x6c, 0xe0, 0x43, 0x23, 0x00, 0x00,
	0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

// Un quad dans le plan XY de glTF, DEUX parametrisations orthogonales :
//   TEXCOORD_0 : u croit avec x   -> dP/du = +X de glTF -> +X du depot
//   TEXCOORD_1 : u croit avec y   -> dP/du = +Y de glTF -> +Z du depot
// La couleur de base echantillonne le jeu 0, la carte de normales le jeu 1.
// La tangente rendue dit donc, sans ambiguite, lequel a servi.
//
// `breakUv0` declare l'accessor de TEXCOORD_0 en VEC3 alors que sa vue porte
// des VEC2 : la lecture le REFUSE, et le maillage sort avec un jeu 0 VIDE et un
// jeu 1 plein. C'est le cas dissymetrique du rabattement des uvSet.
bool WriteTwoUvSetsGlb (const char* path, bool breakUv0 = false)
{
	const float positions[12] = {
		0.f, 0.f, 0.f,   1.f, 0.f, 0.f,   1.f, 1.f, 0.f,   0.f, 1.f, 0.f
	};
	const float uv0[8] = { 0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f };
	const float uv1[8] = { 0.f, 0.f,  0.f, 1.f,  1.f, 1.f,  1.f, 0.f };
	const uint16_t indices[6] = { 0, 1, 2,  0, 2, 3 };

	tinygltf::Model model;
	model.asset.version = "2.0";
	model.asset.generator = "TU two-uv-sets fixture";

	model.buffers.resize (1);
	std::vector<unsigned char>& bin = model.buffers[0].data;

	// Chaque vue commence sur une frontiere de 4 octets : c'est ce qu'exige la
	// specification pour un accessor de flottants, et un validateur tiers le
	// verifie.
	auto append = [&bin](const void* src, size_t bytes) -> size_t {
		while (bin.size () % 4u) bin.push_back (0);
		const size_t offset = bin.size ();
		const unsigned char* p = static_cast<const unsigned char*> (src);
		bin.insert (bin.end (), p, p + bytes);
		return offset;
	};
	auto addView = [&model](size_t offset, size_t length, int target) -> int {
		tinygltf::BufferView v;
		v.buffer     = 0;
		v.byteOffset = offset;
		v.byteLength = length;
		v.target     = target;
		model.bufferViews.push_back (v);
		return (int) model.bufferViews.size () - 1;
	};

	const int posView = addView (append (positions, sizeof (positions)),
	                             sizeof (positions), TINYGLTF_TARGET_ARRAY_BUFFER);
	const int uv0View = addView (append (uv0, sizeof (uv0)),
	                             sizeof (uv0), TINYGLTF_TARGET_ARRAY_BUFFER);
	const int uv1View = addView (append (uv1, sizeof (uv1)),
	                             sizeof (uv1), TINYGLTF_TARGET_ARRAY_BUFFER);
	const int idxView = addView (append (indices, sizeof (indices)),
	                             sizeof (indices), TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);
	const int pngView = addView (append (kOnePixelPng, sizeof (kOnePixelPng)),
	                             sizeof (kOnePixelPng), 0);

	auto addFloatAccessor = [&model](int view, size_t count, int type) -> int {
		tinygltf::Accessor a;
		a.bufferView    = view;
		a.byteOffset    = 0;
		a.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		a.count         = count;
		a.type          = type;
		model.accessors.push_back (a);
		return (int) model.accessors.size () - 1;
	};

	const int posAcc = addFloatAccessor (posView, 4, TINYGLTF_TYPE_VEC3);
	model.accessors[(size_t) posAcc].minValues = { 0.0, 0.0, 0.0 };
	model.accessors[(size_t) posAcc].maxValues = { 1.0, 1.0, 0.0 };
	const int uv0Acc = addFloatAccessor (uv0View, 4,
	                                     breakUv0 ? TINYGLTF_TYPE_VEC3 : TINYGLTF_TYPE_VEC2);
	const int uv1Acc = addFloatAccessor (uv1View, 4, TINYGLTF_TYPE_VEC2);

	tinygltf::Accessor idxAcc;
	idxAcc.bufferView    = idxView;
	idxAcc.byteOffset    = 0;
	idxAcc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	idxAcc.count         = 6;
	idxAcc.type          = TINYGLTF_TYPE_SCALAR;
	model.accessors.push_back (idxAcc);
	const int idxAccIdx = (int) model.accessors.size () - 1;

	tinygltf::Image image;
	image.bufferView = pngView;
	image.mimeType   = "image/png";
	model.images.push_back (image);

	tinygltf::Texture texture;
	texture.source = 0;
	model.textures.push_back (texture);

	tinygltf::Material material;
	material.name = "two_uv_sets";
	material.pbrMetallicRoughness.baseColorTexture.index    = 0;
	material.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
	material.normalTexture.index    = 0;
	material.normalTexture.texCoord = 1;
	model.materials.push_back (material);

	tinygltf::Primitive prim;
	prim.mode = TINYGLTF_MODE_TRIANGLES;
	prim.attributes["POSITION"]   = posAcc;
	prim.attributes["TEXCOORD_0"] = uv0Acc;
	prim.attributes["TEXCOORD_1"] = uv1Acc;
	prim.indices  = idxAccIdx;
	prim.material = 0;
	// AUCUN TANGENT : c'est la generation qu'on veut observer.

	tinygltf::Mesh gltfMesh;
	gltfMesh.name = "two_uv_sets";
	gltfMesh.primitives.push_back (prim);
	model.meshes.push_back (gltfMesh);

	tinygltf::Node node;
	node.mesh = 0;
	model.nodes.push_back (node);

	tinygltf::Scene scene;
	scene.nodes.push_back (0);
	model.scenes.push_back (scene);
	model.defaultScene = 0;

	tinygltf::TinyGLTF writer;
	return writer.WriteGltfSceneToFile (&model, path,
	                                    /*embedImages=*/true, /*embedBuffers=*/true,
	                                    /*prettyPrint=*/false, /*writeBinary=*/true);
}

} // namespace

TEST (TEST_cgmesh_tangents, import_builds_the_basis_on_the_normal_map_uv_set)
{
	const TestFile kTwoUvSets (".glb");
	ASSERT_TRUE (WriteTwoUvSetsGlb (kTwoUvSets));

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kTwoUvSets));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);
	ASSERT_EQ (back->GetNVertices (), 4u);

	// VALIDATION DE LA MONTURE : sans carte de normales lue, aucune tangente
	// n'est generee et le test passerait pour la mauvaise raison.
	const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*> (back->GetMaterial (0));
	ASSERT_NE (pbr, nullptr);
	ASSERT_TRUE (pbr->HasMap (cgpbr::MapSlot::normal));
	EXPECT_EQ ((unsigned) pbr->GetMap (cgpbr::MapSlot::base_color).uvSet, 0u);
	EXPECT_EQ ((unsigned) pbr->GetMap (cgpbr::MapSlot::normal).uvSet, 1u)
		<< "la carte de normales doit rester sur le jeu 1 : les deux jeux "
		   "existent, aucune permutation ni rabattement ne s'applique";
	ASSERT_EQ (back->GetNTextureCoordinates1 (), 4u);

	ASSERT_EQ (back->GetNTangents (), 4u) << "aucune tangente generee";
	const std::vector<float>& t = back->GetVertexTangents ();
	for (unsigned int i = 0; i < 4; ++i)
	{
		// glTF -> depot : (x,y,z) -> (1000x, -1000z, 1000y). Le +Y de glTF,
		// direction de croissance de u DANS LE JEU 1, devient le +Z du depot.
		EXPECT_NEAR (std::fabs (t[4*i + 2]), 1.f, kEps)
			<< "sommet " << i << " : base batie sur le jeu 0 (la couleur de "
			   "base) au lieu du jeu 1 (la carte de normales)";
		EXPECT_NEAR (t[4*i + 0], 0.f, kEps) << "sommet " << i;
	}
}

TEST (TEST_cgmesh_tangents, a_map_never_designates_an_empty_uv_set)
{
	const TestFile kTwoUvSets (".glb");
	// CONTRAT : apres lecture, `uvSet` designe un jeu que le maillage PORTE
	// DES QU'IL EN PORTE UN. Le contrat ne couvre pas le maillage SANS AUCUN
	// jeu : le rabattement laisse alors 0, valeur neutre qui ne designe rien
	// d'echantillonnable. Un consommateur reste donc tenu de verifier que le
	// jeu n'est pas vide ; ce que le rabattement lui epargne, c'est le cas ou
	// un jeu existe et ou `uvSet` designe l'autre.
	//
	// Ici le jeu 0 est
	// refuse et le jeu 1 accepte : la carte de couleur de base, qui designait
	// le jeu 0 dans le fichier, doit ressortir sur le jeu 1 -- le seul
	// echantillonnable.
	ASSERT_TRUE (WriteTwoUvSetsGlb (kTwoUvSets, /*breakUv0=*/true));

	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, kTwoUvSets));
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	const Mesh* back = vm.GetMeshes ()[0];
	ASSERT_NE (back, nullptr);

	// VALIDATION DE LA MONTURE : c'est bien le jeu 0 qui manque, et le 1 qui est la.
	ASSERT_TRUE (back->GetTextureCoordinates ().empty ())
		<< "l'accessor VEC3 aurait du etre refuse : la monture ne vaut plus rien";
	ASSERT_EQ (back->GetNTextureCoordinates1 (), 4u);

	const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*> (back->GetMaterial (0));
	ASSERT_NE (pbr, nullptr);
	ASSERT_TRUE (pbr->HasMap (cgpbr::MapSlot::base_color));
	EXPECT_EQ ((unsigned) pbr->GetMap (cgpbr::MapSlot::base_color).uvSet, 1u)
		<< "uvSet 0 laisse en place alors que le jeu 0 est vide : le "
		   "consommateur lirait un tableau vide en croyant le contrat";
	EXPECT_EQ ((unsigned) pbr->GetMap (cgpbr::MapSlot::normal).uvSet, 1u);
}
