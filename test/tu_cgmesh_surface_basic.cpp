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

TEST(TEST_cgmesh_surface_basic, teapot)
{
    // action
    Mesh* mesh = CreateTeapot();

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 1767);
    EXPECT_EQ(mesh->GetNFaces(), 2256);
}

TEST(TEST_cgmesh_surface_basic, klein_bottle)
{
    // action
    Mesh* mesh = CreateKleinBottle(10, 10);

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 121);
    EXPECT_EQ(mesh->GetNFaces(), 200);
}
