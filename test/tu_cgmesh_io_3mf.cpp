#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"
#include "../src/cgmesh/mesh.h"

// ===========================================================================
//  Import 3MF (lib3mf)
// ===========================================================================
//
// Les fichiers de test sont produits a la main (un 3MF est une archive OPC) et
// portent donc une geometrie EXACTEMENT connue : un cube unite [0,1]^3, 8
// sommets, 12 triangles, oriente vers l'exterieur. Volume signe attendu = +1.
//
// Les oracles sont geometriques, pas de simples comptages : le volume signe
// valide l'orientation ET l'echelle, la boite englobante valide le placement.
// C'est ce qui distingue un import correct d'un import qui « ne plante pas ».
//
// Sans CG_HAS_LIB3MF, VMeshesIO::import_3mf retombe sur un stub renvoyant
// false : les tests verifient alors ce contrat plutot que d'etre desactives.

namespace {

// Volume signe par le theoreme de la divergence. Pour un maillage ferme et
// oriente vers l'exterieur il vaut le volume geometrique.
double signedVolume (Mesh& m)
{
	double v = 0.0;
	for (unsigned int f = 0; f < m.GetNFaces(); f++)
	{
		if (m.GetFaceNVertices (f) != 3) continue;
		float p[3][3];
		for (int k = 0; k < 3; k++)
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, k), p[k]);
		v += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
		    - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
		    + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;
	}
	return v;
}

void bbox (Mesh& m, float lo[3], float hi[3])
{
	lo[0] = lo[1] = lo[2] =  1e30f;
	hi[0] = hi[1] = hi[2] = -1e30f;
	for (unsigned int i = 0; i < m.GetNVertices(); i++)
	{
		float p[3];
		m.GetVertex (i, p);
		for (int k = 0; k < 3; k++)
		{
			if (p[k] < lo[k]) lo[k] = p[k];
			if (p[k] > hi[k]) hi[k] = p[k];
		}
	}
}

void expectUnitCubeAt (Mesh& m, float ox, float oy, float oz, const char* what)
{
	SCOPED_TRACE (what);
	EXPECT_EQ (m.GetNVertices(), 8u);
	EXPECT_EQ (m.GetNFaces(),   12u);
	EXPECT_NEAR (std::fabs (signedVolume (m)), 1.0, 1e-5) << "volume du cube unite";

	float lo[3], hi[3];
	bbox (m, lo, hi);
	EXPECT_NEAR (lo[0], ox,        1e-5f);
	EXPECT_NEAR (lo[1], oy,        1e-5f);
	EXPECT_NEAR (lo[2], oz,        1e-5f);
	EXPECT_NEAR (hi[0], ox + 1.0f, 1e-5f);
	EXPECT_NEAR (hi[1], oy + 1.0f, 1e-5f);
	EXPECT_NEAR (hi[2], oz + 1.0f, 1e-5f);
}

}  // namespace

#ifdef CG_HAS_LIB3MF

// Temoin : un 3MF mono-objet sans transformation. Sans lui, un test qui
// passerait pour de mauvaises raisons ne se distinguerait pas.
TEST (TEST_cgmesh_io_3mf, single_object)
{
	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, "./test/data/3mf/cube.3mf"));
	ASSERT_EQ (vm.GetMeshes().size(), 1u);
	expectUnitCubeAt (*vm.GetMeshes()[0], 0.f, 0.f, 0.f, "cube a l'origine");
}

// Le MEME objet reference par deux build items : verifie qu'un objet partage
// produit bien deux Mesh distincts, et que la translation du second est
// appliquee.
TEST (TEST_cgmesh_io_3mf, two_build_items_are_placed_independently)
{
	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, "./test/data/3mf/two_items.3mf"));
	ASSERT_EQ (vm.GetMeshes().size(), 2u);
	expectUnitCubeAt (*vm.GetMeshes()[0],  0.f, 0.f, 0.f, "item 1, sans transformation");
	expectUnitCubeAt (*vm.GetMeshes()[1], 10.f, 0.f, 0.f, "item 2, translate de x+10");
}

// Le cas qui valide reellement la composition : l'objet est atteint via un
// <component> translate de (0,20,0), lui-meme place par un build item translate
// de (0,0,5). Les deux doivent se cumuler.
TEST (TEST_cgmesh_io_3mf, component_and_item_transforms_compose)
{
	VMeshes vm;
	ASSERT_TRUE (VMeshesIO::load (vm, "./test/data/3mf/components.3mf"));
	ASSERT_EQ (vm.GetMeshes().size(), 1u);
	expectUnitCubeAt (*vm.GetMeshes()[0], 0.f, 20.f, 5.f, "composant (0,20,0) + item (0,0,5)");
}

// Un fichier absent doit echouer proprement, pas laisser echapper l'exception
// que lib3mf leve : ni sinaia ni sulina n'ont de try/catch.
TEST (TEST_cgmesh_io_3mf, missing_file_fails_without_throwing)
{
	VMeshes vm;
	EXPECT_NO_THROW ({
		EXPECT_FALSE (VMeshesIO::load (vm, "./test/data/3mf/__does_not_exist__.3mf"));
	});
	EXPECT_TRUE (vm.GetMeshes().empty());
}

// Une archive valide mais qui n'est pas un 3MF : meme exigence.
TEST (TEST_cgmesh_io_3mf, corrupt_file_fails_without_throwing)
{
	VMeshes vm;
	EXPECT_NO_THROW ({
		// .obj existant, renomme en .3mf par le dispatch : lib3mf doit rejeter.
		EXPECT_FALSE (VMeshesIO::load (vm, "./test/data/3mf/not_a_3mf.3mf"));
	});
	EXPECT_TRUE (vm.GetMeshes().empty());
}

#else  // !CG_HAS_LIB3MF

// Contrat du stub : echec propre et silencieux cote appelant, pas de crash.
TEST (TEST_cgmesh_io_3mf, stub_returns_false_when_lib3mf_is_absent)
{
	VMeshes vm;
	EXPECT_NO_THROW ({
		EXPECT_FALSE (VMeshesIO::load (vm, "./test/data/3mf/cube.3mf"));
	});
	EXPECT_TRUE (vm.GetMeshes().empty());
}

#endif  // CG_HAS_LIB3MF
