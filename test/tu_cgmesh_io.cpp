#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_io, stl)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->load("./test/data/BunnyLowPoly.stl");

    // expectations
    EXPECT_EQ(mesh->GetNVertices(), 1986);
    EXPECT_EQ(mesh->GetNFaces(), 662);

    // export
    return;

    mesh->export_statistics("stats.html");
}
