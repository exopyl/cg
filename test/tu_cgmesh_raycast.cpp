#include <gtest/gtest.h>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_raycast.h"
#include "../src/cgmesh/octree.h"

// ============================================================================
//  Lancer de rayon sur un maillage (mesh_raycast.h)
// ============================================================================
//
// Le seul autre test qui traverse ce chemin, TEST_cgmath_raytracer.scene3,
// n'affirme que la largeur et la hauteur de l'image produite : le rendu peut y
// devenir entierement faux sans qu'il bronche. D'ou ce fichier.
//
// Trois proprietes, dans cet ordre d'importance :
//
//   1. INTERROGER UN MAILLAGE NE LE MUTE PAS -- revision, nombres de sommets et
//      de faces inchanges ;
//   2. le chemin accelere et le chemin brut donnent le MEME resultat, n-gons
//      compris ;
//   3. l'elimination des faces arriere est en place.
//
// Comme tu_cgmesh_mesh.cpp, ce fichier ne passe que par l'API publique.

namespace {

// Un quad unite dans le plan z=0, oriente CCW vu depuis +z (sa normale est donc
// +z, et un rayon descendant le long de -z le voit de face).
//
//   (0,1) 3---------2 (1,1)
//         |       / |
//         |  B  /   |        A = eventail (0,1,2)
//         |   /  A  |        B = eventail (0,2,3)
//   (0,0) 0---------1 (1,0)
//
// LE POINT DE CE MAILLAGE : le coin haut-gauche n'appartient qu'au triangle B, et
// ce quad seul suffit donc a departager deux erreurs faciles a commettre.
//
//   - un chemin qui ne lirait que les TROIS PREMIERS coins d'une face manquerait
//     tout rayon tombant dans B ;
//   - un octree construit sur GetNFaces() au lieu de tris.size()/3 le manquerait
//     aussi : le quad donne 2 triangles pour 1 seule face, donc B n'entrerait
//     jamais dans l'arbre.
//
// Un maillage tout-triangles ne detecte ni l'une ni l'autre.
void MakeQuad (Mesh &m)
{
	m.Init (4, 1);

	float v[12] = {
		0.f, 0.f, 0.f,   // 0
		1.f, 0.f, 0.f,   // 1
		1.f, 1.f, 0.f,   // 2
		0.f, 1.f, 0.f,   // 3
	};
	m.SetVertices (4, v);
	m.SetFace (0, 0, 1, 2, 3);
}

// Deux triangles a des hauteurs differentes, l'un au-dessus de l'autre, pour
// verifier que c'est bien l'intersection LA PLUS PROCHE qui est rendue.
void MakeTwoStackedTriangles (Mesh &m)
{
	m.Init (6, 2);

	float v[18] = {
		0.f, 0.f, 0.f,   // 0   triangle bas, z=0
		1.f, 0.f, 0.f,   // 1
		0.f, 1.f, 0.f,   // 2

		0.f, 0.f, 2.f,   // 3   triangle haut, z=2
		1.f, 0.f, 2.f,   // 4
		0.f, 1.f, 2.f,   // 5
	};
	m.SetVertices (6, v);
	m.SetFace (0, 0, 1, 2);
	m.SetFace (1, 3, 4, 5);
}

// Un rayon vertical descendant depuis z=+1, aux coordonnees (x, y).
struct DownRay
{
	DownRay (float x, float y) : o (x, y, 1.f), d (0.f, 0.f, -1.f) {}
	Vector3f o, d;
};

} // namespace

// ============================================================================
//  1. Un lancer de rayon ne mute pas le maillage
// ============================================================================
//
// La propriete la plus importante de ce fichier. Une construction d'octree glissee
// dans la requete la violerait, et rien d'autre ne le detecterait : le compteur de
// revision ne bougeant pas non plus, la mutation serait doublement invisible.

TEST (TEST_cgmesh_raycast, brute_force_ne_touche_pas_le_maillage)
{
	Mesh m;
	MakeQuad (m);

	const uint64_t revisionAvant = m.GetRevision ();
	const unsigned int nVerticesAvant = m.GetNVertices ();
	const unsigned int nFacesAvant    = m.GetNFaces ();

	DownRay ray (0.5f, 0.25f);
	float t = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &t, hit, nrm), 1);

	EXPECT_EQ (m.GetRevision (),   revisionAvant)  << "un lancer de rayon est une LECTURE";
	EXPECT_EQ (m.GetNVertices (), nVerticesAvant);
	EXPECT_EQ (m.GetNFaces (),    nFacesAvant);
}

TEST (TEST_cgmesh_raycast, chemin_accelere_ne_touche_pas_le_maillage)
{
	Mesh m;
	MakeQuad (m);

	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);
	ASSERT_NE (octree, nullptr);

	// La construction elle-meme ne doit rien muter non plus : elle ne fait que
	// DERIVER une triangulation.
	const uint64_t revisionApresConstruction = m.GetRevision ();

	DownRay ray (0.5f, 0.25f);
	float t = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (GetIntersectionWithRay (m, *octree, ray.o, ray.d, &t, hit, nrm), 1);

	EXPECT_EQ (m.GetRevision (), revisionApresConstruction);
}

// Le maillage doit survivre a l'octree (mesh_raycast.h). Ici on verifie l'ordre
// inverse, qui est licite et doit le rester : detruire l'octree avant le maillage
// ne pose aucun probleme.
TEST (TEST_cgmesh_raycast, octree_detruit_avant_le_maillage)
{
	Mesh m;
	MakeQuad (m);

	{
		std::unique_ptr<Octree> octree = BuildRaycastOctree (m);
		float t = 0.f;
		Vector3f hit, nrm;
		DownRay ray (0.5f, 0.25f);
		EXPECT_EQ (GetIntersectionWithRay (m, *octree, ray.o, ray.d, &t, hit, nrm), 1);
	}

	// Le maillage reste utilisable apres la mort de son accelerateur.
	float t = 0.f;
	Vector3f hit, nrm;
	DownRay ray (0.5f, 0.25f);
	EXPECT_EQ (GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &t, hit, nrm), 1);
}

// ============================================================================
//  2. Les deux chemins s'accordent, n-gons compris
// ============================================================================

TEST (TEST_cgmesh_raycast, quad_second_triangle_touche_par_les_deux_chemins)
{
	Mesh m;
	MakeQuad (m);
	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);

	// (0.25, 0.75) est au-dessus de la diagonale y=x : le point n'appartient qu'au
	// triangle B de l'eventail. C'est le cas discriminant decrit sur MakeQuad.
	DownRay ray (0.25f, 0.75f);

	float tBrut = -1.f, tAccel = -1.f;
	Vector3f hitBrut, nrmBrut, hitAccel, nrmAccel;

	EXPECT_EQ (GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &tBrut, hitBrut, nrmBrut), 1)
		<< "le chemin brut doit trianguler la face en eventail, pas lire 3 coins";
	EXPECT_EQ (GetIntersectionWithRay (m, *octree, ray.o, ray.d, &tAccel, hitAccel, nrmAccel), 1)
		<< "l'octree doit indexer tris.size()/3 triangles, pas m_nFaces";

	EXPECT_NEAR (tBrut,  1.f, 1e-4f);
	EXPECT_NEAR (tAccel, 1.f, 1e-4f);
	EXPECT_NEAR (hitBrut[2], 0.f, 1e-4f);
	EXPECT_NEAR (hitAccel[2], 0.f, 1e-4f);
}

TEST (TEST_cgmesh_raycast, les_deux_chemins_s_accordent_sur_une_grille)
{
	Mesh m;
	MakeQuad (m);
	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);

	// Balayage du quad ET de ses alentours : les accords sur les manques comptent
	// autant que les accords sur les touches.
	for (int iy=-2; iy<=12; iy++)
	{
		for (int ix=-2; ix<=12; ix++)
		{
			const float x = 0.1f * (float)ix;
			const float y = 0.1f * (float)iy;
			DownRay ray (x, y);

			float tBrut = -1.f, tAccel = -1.f;
			Vector3f hitBrut, nrmBrut, hitAccel, nrmAccel;
			const int resBrut  = GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &tBrut, hitBrut, nrmBrut);
			const int resAccel = GetIntersectionWithRay (m, *octree, ray.o, ray.d, &tAccel, hitAccel, nrmAccel);

			ASSERT_EQ (resBrut, resAccel) << "desaccord en (" << x << ", " << y << ")";
			if (resBrut == 1)
				ASSERT_NEAR (tBrut, tAccel, 1e-4f) << "en (" << x << ", " << y << ")";
		}
	}
}

TEST (TEST_cgmesh_raycast, intersection_la_plus_proche)
{
	Mesh m;
	MakeTwoStackedTriangles (m);
	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);

	// Depuis z=+1 vers le bas : seul le triangle du bas (z=0) est devant le
	// rayon ; celui du haut (z=2) est derriere son origine.
	{
		DownRay ray (0.2f, 0.2f);
		float t = -1.f;
		Vector3f hit, nrm;
		ASSERT_EQ (GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &t, hit, nrm), 1);
		EXPECT_NEAR (t, 1.f, 1e-4f);
		EXPECT_NEAR (hit[2], 0.f, 1e-4f);
	}

	// Depuis z=+5 : les deux sont devant, c'est celui du HAUT qui doit sortir.
	{
		Vector3f o (0.2f, 0.2f, 5.f), d (0.f, 0.f, -1.f);
		float tBrut = -1.f, tAccel = -1.f;
		Vector3f hitBrut, nrmBrut, hitAccel, nrmAccel;
		ASSERT_EQ (GetIntersectionWithRayBruteForce (m, o, d, &tBrut, hitBrut, nrmBrut), 1);
		ASSERT_EQ (GetIntersectionWithRay (m, *octree, o, d, &tAccel, hitAccel, nrmAccel), 1);
		EXPECT_NEAR (hitBrut[2],  2.f, 1e-4f);
		EXPECT_NEAR (hitAccel[2], 2.f, 1e-4f);
		EXPECT_NEAR (tBrut, tAccel, 1e-4f);
	}
}

// ============================================================================
//  3. Elimination des faces arriere
// ============================================================================
//
// Caracterisation, non specification : ce comportement vient de
// Triangle::GetIntersectionWithRay (cgmath/geometry.cpp:662) et c'est ce qui
// distingue cette primitive de BVH, qui ne cule pas (bvh.h:8-11). Le figer ici
// evite qu'un refactoring ne le perde par inadvertance.

TEST (TEST_cgmesh_raycast, face_abordee_par_derriere_non_vue)
{
	Mesh m;
	MakeQuad (m);   // normale +z
	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);

	// Rayon montant depuis z=-1 : il aborde le quad par sa face arriere.
	Vector3f o (0.5f, 0.25f, -1.f), d (0.f, 0.f, 1.f);
	float t = 0.f;
	Vector3f hit, nrm;

	EXPECT_EQ (GetIntersectionWithRayBruteForce (m, o, d, &t, hit, nrm), 0);
	EXPECT_EQ (GetIntersectionWithRay (m, *octree, o, d, &t, hit, nrm), 0);
}

TEST (TEST_cgmesh_raycast, rayon_hors_du_maillage_rend_zero_et_t_negatif)
{
	Mesh m;
	MakeQuad (m);
	std::unique_ptr<Octree> octree = BuildRaycastOctree (m);

	DownRay ray (5.f, 5.f);   // largement a cote

	float tBrut = 0.f, tAccel = 0.f;
	Vector3f hit, nrm;
	EXPECT_EQ (GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &tBrut, hit, nrm), 0);
	EXPECT_EQ (GetIntersectionWithRay (m, *octree, ray.o, ray.d, &tAccel, hit, nrm), 0);
	EXPECT_LT (tBrut,  0.f);
	EXPECT_LT (tAccel, 0.f);
}

// ============================================================================
//  4. La virtuelle de Geometry passe par le chemin non accelere
// ============================================================================
//
// Mesh::GetIntersectionWithRay implemente une virtuelle pure de Geometry, donc
// elle ne peut pas disparaitre. Elle doit rendre exactement ce que rend le chemin
// brut, et ne rien construire.

TEST (TEST_cgmesh_raycast, methode_virtuelle_equivaut_au_chemin_brut)
{
	Mesh m;
	MakeQuad (m);

	DownRay ray (0.25f, 0.75f);

	float tMethode = -1.f, tLibre = -1.f;
	Vector3f hitMethode, nrmMethode, hitLibre, nrmLibre;

	const uint64_t revisionAvant = m.GetRevision ();
	const int resMethode = m.GetIntersectionWithRay (ray.o, ray.d, &tMethode, hitMethode, nrmMethode);
	EXPECT_EQ (m.GetRevision (), revisionAvant) << "la virtuelle ne doit rien construire";

	const int resLibre = GetIntersectionWithRayBruteForce (m, ray.o, ray.d, &tLibre, hitLibre, nrmLibre);

	EXPECT_EQ (resMethode, resLibre);
	EXPECT_NEAR (tMethode, tLibre, 1e-6f);
}
