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
        if (pMesh->GetNMaterials() > 0) {
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
    EXPECT_GT(meshes[0]->GetNMaterials(), 0u);

    delete pVMeshes;
}

TEST(TEST_cgmesh_io, glb_duck)
{
    // context
    VMeshes* pVMeshes = new VMeshes();

    // action
    bool res = pVMeshes->load("./test/data/Duck.glb");

    // expectations
    ASSERT_TRUE(res);
    
    unsigned int nVertices = pVMeshes->GetNVertices();
    unsigned int nFaces = pVMeshes->GetNFaces();
    
    // Standard Duck.glb values
    EXPECT_EQ(nVertices, 2399);
    EXPECT_EQ(nFaces, 4212);

    // Check materials
    auto& meshes = pVMeshes->GetMeshes();
    bool foundMaterial = false;
    bool foundTexture = false;
    bool foundTexCoords = false;
    for (auto pMesh : meshes) {
        if (pMesh->GetNMaterials() > 0) {
            foundMaterial = true;
        }
        if (pMesh->m_nTextureCoordinates > 0) {
            foundTexCoords = true;
        }
        for (unsigned int i = 0; i < pMesh->GetNMaterials(); ++i) {
            MaterialTexture* mt = dynamic_cast<MaterialTexture*>(pMesh->GetMaterial(i));
            if (mt && mt->GetImage()) {
                foundTexture = true;
            }
        }
    }
    EXPECT_TRUE(foundMaterial);
    EXPECT_TRUE(foundTexture);
    EXPECT_TRUE(foundTexCoords);

    delete pVMeshes;
}

TEST(TEST_cgmesh_io, glb_fox_non_indexed)
{
    VMeshes* pVMeshes = new VMeshes();

    bool res = pVMeshes->load("./test/data/Fox.glb");

    ASSERT_TRUE(res);
    ASSERT_EQ(pVMeshes->GetNMeshes(), 1u);

    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_EQ(meshes.size(), 1u);

    Mesh* pMesh = meshes[0];
    EXPECT_EQ(pMesh->m_nVertices, 1728u);
    EXPECT_EQ(pMesh->m_nFaces, 576u);
    EXPECT_GT(pMesh->m_nTextureCoordinates, 0u);
    EXPECT_GT(pMesh->GetNMaterials(), 0u);

    delete pVMeshes;
}

#ifdef CG_HAS_OPENNURBS

// Parameterized over every .3dm asset in test/data/3dm. Each parameter
// carries the expected per-mesh counts (vertices, indices, materials) so
// the test pins down the importer's output to the values captured the day
// the test was written. Indices = 3 * nFaces (triangulated export from
// openNURBS).

struct MeshExpect
{
    unsigned int vertices;
    unsigned int indices;
    unsigned int materials;
};

struct Rhino3dmExpectation
{
    const char* filename;
    std::vector<MeshExpect> meshes;
};

class TEST_cgmesh_io_rhino_3dm : public ::testing::TestWithParam<Rhino3dmExpectation> {};

TEST_P(TEST_cgmesh_io_rhino_3dm, load)
{
    const auto& exp = GetParam();
    const std::string filename = std::string("./test/data/3dm/") + exp.filename;

    VMeshes* pVMeshes = new VMeshes();

    ASSERT_TRUE(pVMeshes->load(filename.c_str())) << "load failed for " << filename;

    ASSERT_EQ(pVMeshes->GetNMeshes(), exp.meshes.size())
        << "nMeshes mismatch for " << exp.filename;

    for (size_t i = 0; i < exp.meshes.size(); ++i)
    {
        Mesh* m = pVMeshes->GetMeshes()[i];
        ASSERT_NE(m, nullptr) << exp.filename << " mesh[" << i << "] is null";
        EXPECT_EQ(m->GetNVertices(),     exp.meshes[i].vertices)
            << exp.filename << " mesh[" << i << "] vertices";
        EXPECT_EQ(3u * m->GetNFaces(),   exp.meshes[i].indices)
            << exp.filename << " mesh[" << i << "] indices";
        EXPECT_EQ(m->GetNMaterials(),    exp.meshes[i].materials)
            << exp.filename << " mesh[" << i << "] materials";
    }

    delete pVMeshes;
}

INSTANTIATE_TEST_SUITE_P(
    AllFiles,
    TEST_cgmesh_io_rhino_3dm,
    ::testing::Values(
        Rhino3dmExpectation{"balls.3dm", {
            {6612, 37968, 1}, {6612, 37968, 1}, {6612, 37968, 1},
            {6612, 37968, 1}, {6612, 37968, 1}, {4, 6, 1},
        }},
        Rhino3dmExpectation{"Epicycles.3dm", {
            {3390, 10608, 1}, {357, 1050, 1}, {2676, 8016, 1},
        }},
        Rhino3dmExpectation{"flying.3dm", {
            {25, 96, 1}, {25, 96, 1},
        }},
        Rhino3dmExpectation{"gemstones.3dm", {
            {390, 390, 1},   {300, 300, 1},   {444, 444, 1},   {282, 282, 1},   {210, 210, 1},
            {804, 804, 1},   {384, 384, 1},   {558, 558, 1},   {426, 426, 1},   {426, 426, 1},
            {672, 672, 1},   {432, 432, 1},   {474, 474, 1},   {462, 462, 1},   {426, 426, 1},
            {426, 426, 1},   {486, 486, 1},   {504, 504, 1},   {684, 684, 1},   {378, 378, 1},
            {648, 648, 1},   {648, 648, 1},   {2820, 2820, 1}, {3744, 3744, 1}, {216, 216, 1},
            {378, 378, 1},   {282, 282, 1},   {294, 294, 1},   {216, 216, 1},   {1440, 1440, 1},
            {312, 312, 1},   {252, 252, 1},   {576, 576, 1},   {576, 576, 1},   {366, 366, 1},
            {258, 258, 1},   {204, 204, 1},   {150, 150, 1},   {558, 558, 1},   {960, 960, 1},
            {240, 240, 1},   {324, 324, 1},   {540, 540, 1},   {132, 132, 1},   {330, 330, 1},
            {180, 180, 1},   {378, 378, 1},   {378, 378, 1},   {372, 372, 1},   {216, 216, 1},
            {180, 180, 1},   {234, 234, 1},   {258, 258, 1},   {138, 138, 1},   {258, 258, 1},
            {210, 210, 1},   {102, 102, 1},   {420, 420, 1},   {156, 156, 1},   {186, 186, 1},
            {162, 162, 1},   {174, 174, 1},   {402, 402, 1},   {60, 60, 1},     {60, 60, 1},
            {372, 372, 1},   {156, 156, 1},   {276, 276, 1},   {210, 210, 1},   {174, 174, 1},
            {156, 156, 1},   {222, 222, 1},   {210, 210, 1},   {282, 282, 1},   {288, 288, 1},
            {318, 318, 1},   {354, 354, 1},   {210, 210, 1},   {246, 246, 1},   {528, 528, 1},
            {354, 354, 1},   {210, 210, 1},
        }},
        Rhino3dmExpectation{"Pistons.3dm", {
            {194, 564, 1},   {794, 2358, 1},  {466, 1422, 1},  {794, 2358, 1},
            {194, 564, 1},   {466, 1422, 1},  {194, 564, 1},   {466, 1422, 1},
            {794, 2358, 1},  {556, 1686, 1},  {794, 2358, 1},  {2210, 6804, 1},
            {194, 564, 1},
        }},
        Rhino3dmExpectation{"UniJoint.3dm", {
            {1164, 4452, 1}, {1653, 7620, 1}, {1204, 4638, 1},
        }}
    ),
    [](const ::testing::TestParamInfo<Rhino3dmExpectation>& info) {
        // gtest-friendly suffix: strip the .3dm extension, sanitize separators
        std::string s = info.param.filename;
        const size_t dot = s.find_last_of('.');
        if (dot != std::string::npos) s.resize(dot);
        for (char& c : s) if (c == '.' || c == '-' || c == ' ') c = '_';
        return s;
    }
);
#endif
