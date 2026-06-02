#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdio>

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

TEST(TEST_cgmesh_io, obj_texcoords_per_face)
{
    // Faces with texture coordinates must keep a valid, in-range texcoord
    // index for EVERY corner. Regression guard for a bug where the per-face
    // texcoord arrays were reallocated once per corner, wiping all but the
    // last corner and leaving out-of-range garbage that crashed the textured
    // render path.
    const char* path = "./tc_face.obj";
    {
        std::ofstream o(path);
        o << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
        o << "vt 0 0\nvt 1 0\nvt 0 1\n";
        o << "f 1/1 2/2 3/3\n";
    }

    Mesh* m = new Mesh();
    m->load(path);
    ASSERT_EQ(m->GetNFaces(), 1u);

    Face* f = m->m_pFaces[0];
    ASSERT_TRUE(f->m_bUseTextureCoordinates);
    ASSERT_NE(f->m_pTextureCoordinatesIndices, nullptr);
    for (int k = 0; k < f->GetNVertices(); k++)
    {
        unsigned int ti = f->m_pTextureCoordinatesIndices[k];
        EXPECT_LT(2u*ti + 1u, (unsigned)m->m_pTextureCoordinates.size());
        EXPECT_EQ(ti, (unsigned)k); // 1/1 2/2 3/3 -> 0,1,2
    }

    delete m;
    std::remove(path);
}

TEST(TEST_cgmesh_io, obj_negative_indices)
{
    // OBJ negative indices count back from the most recently declared vertex.
    // Interleave vertices and faces so the relative offsets only resolve
    // correctly when measured against the running count (not the file total):
    // each face's -3/-2/-1 must reference the three vertices just above it.
    const char* path = "./neg_index.obj";
    {
        std::ofstream o(path);
        o << "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
        o << "f -3 -2 -1\n";
        o << "v 5 5 5\nv 6 5 5\nv 5 6 5\n";
        o << "f -3 -2 -1\n";
    }

    Mesh* m = new Mesh();
    m->load(path);

    ASSERT_EQ(m->GetNVertices(), 6u);
    ASSERT_EQ(m->GetNFaces(), 2u);

    EXPECT_EQ(m->m_pFaces[0]->GetVertex(0), 0);
    EXPECT_EQ(m->m_pFaces[0]->GetVertex(1), 1);
    EXPECT_EQ(m->m_pFaces[0]->GetVertex(2), 2);

    EXPECT_EQ(m->m_pFaces[1]->GetVertex(0), 3);
    EXPECT_EQ(m->m_pFaces[1]->GetVertex(1), 4);
    EXPECT_EQ(m->m_pFaces[1]->GetVertex(2), 5);

    delete m;
    std::remove(path);
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

    // The three parts (base / neck / panel) form an articulated rig: they only
    // assemble correctly once the 3DS keyframer node transforms are applied on
    // import. Check the assembled bounds (engine Y-up) match the expected
    // monitor: ~420 wide (x), standing ~450 tall (y), thin in depth (z).
    BoundingBox total;
    for (auto pMesh : meshes)
    {
        pMesh->computebbox();
        total.AddBoundingBox(pMesh->bbox());
    }
    EXPECT_NEAR(total.GetMinX(), -210.f, 5.f);
    EXPECT_NEAR(total.GetMaxX(),  210.f, 5.f);
    EXPECT_NEAR(total.GetMinY(),    0.f, 5.f);
    EXPECT_NEAR(total.GetMaxY(),  450.f, 5.f);
    EXPECT_NEAR(total.GetMinZ(), -111.f, 5.f);
    EXPECT_NEAR(total.GetMaxZ(),  111.f, 5.f);

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

#ifdef CG_HAS_OCCT

// STEP import via OpenCASCADE. The 4-pin plug is a small mechanical part
// with rounded barrels and chamfers — typical mix of planar + cylindrical
// faces, plenty of TopoDS_Face entities. The importer emits one Mesh per
// face, so the mesh count is determined by the STEP topology (stable
// across OCCT versions). Per-mesh vertex / triangle counts depend on
// BRepMesh_IncrementalMesh's deflection settings and may drift slightly
// with OCCT version bumps, so we only assert non-degeneracy there.
TEST(TEST_cgmesh_io, step_4pinplug)
{
    VMeshes* pVMeshes = new VMeshes();

    bool res = pVMeshes->load("./test/data/4pinplug.stp");
    ASSERT_TRUE(res);

    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_GT(meshes.size(), 0u);

    // Aggregate sanity: the assembled scene must carry geometry.
    EXPECT_GT(pVMeshes->GetNVertices(), 0u);
    EXPECT_GT(pVMeshes->GetNFaces(),    0u);

    // No empty Mesh in the list (each TopoDS_Face had a non-trivial
    // triangulation).
    for (Mesh* m : meshes) {
        ASSERT_NE(m, nullptr);
        EXPECT_GT(m->GetNVertices(), 0u);
        EXPECT_GT(m->GetNFaces(),    0u);
    }

    // Bounding box has positive extent in at least one axis — catches
    // the silent-failure mode where the importer returns Meshes filled
    // with zeros.
    BoundingBox bbox;
    for (Mesh* m : meshes) {
        m->computebbox();
        bbox.AddBoundingBox(m->bbox());
    }
    float diag = bbox.GetDiagonalLength();
    EXPECT_GT(diag, 0.0f);

    delete pVMeshes;
}

// IGES import via OpenCASCADE.
//
// The three sample files in test/data/ (ex1.iges, ex2.iges, ex3.iges)
// turn out to be WIREFRAME IGES — they hold only type-106 copious-data
// curves plus subfigures/groups (ex1 is even labelled "INTEGRATED
// CIRCUIT SEMICUSTOM CELL", i.e. a 2D PCB layout). They contain zero
// TopoDS_Face entities after OCCT translation, so there is nothing to
// tessellate.
//
// The contract this test pins down is therefore: when given a
// wireframe-only IGES, import_iges parses successfully, finds no
// triangulatable surface, and refuses gracefully (returns false from
// VMeshes::load) instead of crashing or producing empty Meshes that
// would later surprise the renderer / bbox code.
//
// Replace these files with surface-bearing IGES (types 128/143/144)
// when one becomes available and flip the expectations to ASSERT_TRUE
// + non-zero mesh counts.
class TEST_cgmesh_io_iges_wireframe : public ::testing::TestWithParam<std::string> {};

TEST_P(TEST_cgmesh_io_iges_wireframe, load_returns_false_no_meshes)
{
    const std::string filename = std::string("./test/data/") + GetParam();

    VMeshes* pVMeshes = new VMeshes();

    // load returns false: VMeshes::import_iges parsed the file and
    // ran the IGES → TopoDS_Shape transfer, but tessellateAndAppend
    // found no TopAbs_FACE and emitted zero Meshes.
    EXPECT_FALSE(pVMeshes->load(filename.c_str()))
        << "wireframe IGES " << filename << " should not produce meshes";

    // And no leftover Meshes were pushed onto the vector before the
    // refusal — important so callers that ignore the bool return still
    // see a clean state.
    EXPECT_EQ(pVMeshes->GetMeshes().size(), 0u);

    delete pVMeshes;
}

INSTANTIATE_TEST_SUITE_P(
    Examples,
    TEST_cgmesh_io_iges_wireframe,
    ::testing::Values(std::string("ex1.iges"),
                      std::string("ex2.iges"),
                      std::string("ex3.iges")),
    [](const ::testing::TestParamInfo<std::string>& info) {
        // Strip extension so the gtest-generated name is `ex1` / `ex2` / `ex3`.
        std::string s = info.param;
        const size_t dot = s.find_last_of('.');
        if (dot != std::string::npos) s.resize(dot);
        return s;
    }
);

// Surface-bearing IGES: Milled_Front_X_Carriage.iges is a real B-Rep
// solid (entity types 128 NURBS surface, 186 manifold solid, 510 face,
// 514 shell). It should hit the same happy path as step_4pinplug —
// non-zero meshes, non-degenerate bbox.
TEST(TEST_cgmesh_io, iges_milled_front_x_carriage)
{
    VMeshes* pVMeshes = new VMeshes();

    ASSERT_TRUE(pVMeshes->load("./test/data/Milled_Front_X_Carriage.iges"));

    auto& meshes = pVMeshes->GetMeshes();
    ASSERT_GT(meshes.size(), 0u);

    EXPECT_GT(pVMeshes->GetNVertices(), 0u);
    EXPECT_GT(pVMeshes->GetNFaces(),    0u);

    for (Mesh* m : meshes) {
        ASSERT_NE(m, nullptr);
        EXPECT_GT(m->GetNVertices(), 0u);
        EXPECT_GT(m->GetNFaces(),    0u);
    }

    BoundingBox bbox;
    for (Mesh* m : meshes) {
        m->computebbox();
        bbox.AddBoundingBox(m->bbox());
    }
    EXPECT_GT(bbox.GetDiagonalLength(), 0.0f);

    delete pVMeshes;
}

#endif // CG_HAS_OCCT

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
