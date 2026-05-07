#include <gtest/gtest.h>
#include <filesystem>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_io, obj)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/cube.obj");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 8);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 12);

    // action
    auto exported = mesh->m_pMesh->save("./exported_cube.obj");

    EXPECT_EQ(exported, 0);
}

TEST(TEST_cgmesh_io, off)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/cube.off");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 8);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 12);

    // action
    auto exported = mesh->m_pMesh->save("./exported_cube.off");

    EXPECT_EQ(exported, 0);
}

TEST(TEST_cgmesh_io, stl)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/BunnyLowPoly.stl");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 1986);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 662);

    // action : save in ASCII STL via Mesh::save dispatch
    auto exported = mesh->m_pMesh->save("./exported_BunnyLowPoly.stl");
    EXPECT_EQ(exported, 0);
    EXPECT_TRUE(std::filesystem::exists("./exported_BunnyLowPoly.stl"));

    // action : save in binary STL via the dedicated entry point
    auto exportedBin = mesh->m_pMesh->export_stl_binary("./exported_BunnyLowPoly.bin.stl");
    EXPECT_EQ(exportedBin, 0);
    EXPECT_TRUE(std::filesystem::exists("./exported_BunnyLowPoly.bin.stl"));

    // round-trip the binary file : reload it and check counts. Binary STL stores
    // 3 vertices per triangle (no shared indexing), so nVertices = 3 * nFaces.
    Mesh_half_edge* roundTrip = new Mesh_half_edge();
    roundTrip->m_pMesh->load("./exported_BunnyLowPoly.bin.stl");
    EXPECT_EQ(roundTrip->m_pMesh->GetNFaces(), 662);
    EXPECT_EQ(roundTrip->m_pMesh->GetNVertices(), 3u * 662);
    delete roundTrip;

    delete mesh;
}

TEST(TEST_cgmesh_io, ply)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/sofa.ply");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 12103);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 24104);

    // action
    auto exported = mesh->m_pMesh->save("./exported_sofa.ply");

    EXPECT_EQ(exported, 0);
}

TEST(TEST_cgmesh_io, ply_ascii)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/sofa_ascii.ply");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 12103);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 24104);
}

TEST(TEST_cgmesh_io, dae)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();
    mesh->m_pMesh->load("./test/data/BunnyLowPoly.stl");

    // action
    auto exported = mesh->m_pMesh->save("./exported_BunnyLowPoly.dae");

    // expectations
    EXPECT_EQ(exported, 0);
}

TEST(TEST_cgmesh_io, cpp)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();
    mesh->m_pMesh->load("./test/data/BunnyLowPoly.stl");

    // action
    auto exported = mesh->m_pMesh->save("./exported_BunnyLowPoly.cpp");

    // expectations
    EXPECT_EQ(exported, 0);
}


TEST(TEST_cgmesh_io, 3ds_sink)
{
    // context
    VMeshes* pVMeshes = new VMeshes();

    // action
    pVMeshes->load("./test/data/sink.3ds");

    // expectations
    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_EQ(meshes.size(), 4);
    
    // Check materials are loaded (sink.3ds has materials)
    bool foundMaterial = false;
    for (auto pMesh : meshes) {
        if (pMesh->m_nMaterials > 0) {
            foundMaterial = true;
            break;
        }
    }
    EXPECT_TRUE(foundMaterial);
}

TEST(TEST_cgmesh_io, 3ds_display)
{
    // context
    VMeshes* pVMeshes = new VMeshes();

    // action
    pVMeshes->load("./test/data/display.3ds");

    // expectations
    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_EQ(meshes.size(), 3);

    // Check materials are assigned to faces
    bool foundFaceWithMaterial = false;
    for (auto pMesh : meshes) {
        for (unsigned int i = 0; i < pMesh->m_nFaces; i++) {
            if (pMesh->m_pFaces[i]->GetMaterialId() != MATERIAL_NONE) {
                foundFaceWithMaterial = true;
                break;
            }
        }
        if (foundFaceWithMaterial) break;
    }
    EXPECT_TRUE(foundFaceWithMaterial);
}

TEST(TEST_cgmesh_io, 3ds_floppy)
{
    // context
    VMeshes* pVMeshes = new VMeshes();

    // action
    pVMeshes->load("./test/data/floppy.3ds");

    // expectations
    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_EQ(meshes.size(), 1);
    
    // Floppy also has materials
    EXPECT_GT(meshes[0]->m_nMaterials, 0u);
}
