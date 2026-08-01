#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_surface_basic, cube)
{
    // action
    Mesh* mesh = CreateCube(true);

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 8);
    EXPECT_EQ(mesh->GetNFaces(), 12);
}

TEST(TEST_cgmesh_surface_basic, capsule)
{
    // action
    Mesh* mesh = CreateCapsule(10, 10.f, 2.f);

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 102);
    EXPECT_EQ(mesh->GetNFaces(), 200);
}

// n IMPAIR : l'ancienne formule `2*(n-1)*2*nhalf + 2*n` allouait deux faces de
// plus que la boucle n'en remplit. InitFaces() pre-alloue un Face() par slot, donc
// ces deux-la restaient a ZERO sommet et ComputeNormals() lisait m_pVertices[0]
// hors bornes -- crash a la moindre capsule a n impair, c'est-a-dire des qu'on
// touchait au parametre « n » depuis ParameterizedCapsule (defaut 20 -> 21).
//
// L'invariant : toute face porte exactement trois sommets, et leurs indices sont
// dans les bornes. Le compte seul ne suffit pas a le prouver.
TEST(TEST_cgmesh_surface_basic, capsule_with_an_odd_slice_count)
{
    const unsigned int n = 11;      // impair
    Mesh* mesh = CreateCapsule(n, 10.f, 2.f);
    ASSERT_NE(mesh, nullptr);

    const unsigned int nhalf = n / 2;
    EXPECT_EQ(mesh->GetNVertices(), 2*n*nhalf + 2);
    EXPECT_EQ(mesh->GetNFaces(),    4*n*nhalf);

    for (unsigned int f = 0; f < mesh->GetNFaces(); ++f)
    {
        ASSERT_EQ(mesh->GetFaceNVertices(f), 3) << "face " << f << " vide ou non triangulaire";
        for (unsigned int k = 0; k < 3; ++k)
        {
            const int v = mesh->GetFaceVertex(f, k);
            ASSERT_GE(v, 0);
            ASSERT_LT((unsigned int)v, mesh->GetNVertices()) << "face " << f << ", sommet " << k;
        }
    }

    // Le chemin qui plantait.
    mesh->ComputeNormals();

    delete mesh;
}

TEST(TEST_cgmesh_surface_basic, teapot)
{
    // action
    Mesh* mesh = CreateTeapot();

    // expectations
    // 1178 = sizeof(TeapotData_Vertex)/(3*sizeof(float)). The previous 1767 came
    // from the buggy divisor sizeof(3*sizeof(float))==8 (1.5x), which over-read
    // the 1178-element vertex array by 589 entries.
    EXPECT_EQ(mesh->GetNVertices(), 1178);
    EXPECT_EQ(mesh->GetNFaces(), 2256);
}

TEST(TEST_cgmesh_surface_basic, klein_bottle)
{
    // action
    Mesh* mesh = CreateKleinBottle(10, 10);

    // expectations : exact ThetaResolution x PhiResolution grid = 10*10 = 100
    // vertices, periodically closed in both directions (Klein bottle, chi = 0 :
    // V - E + F = 100 - 300 + 200 = 0). The old count 121 came from the buggy
    // float-counter loops that over-wrote the vertex buffer (see CreateKleinBottle).
    EXPECT_EQ(mesh->GetNVertices(), 100);
    EXPECT_EQ(mesh->GetNFaces(), 200);
}
