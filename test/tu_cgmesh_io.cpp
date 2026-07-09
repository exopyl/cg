#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cmath>

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

TEST(TEST_cgmesh_io, obj_cube_tex_texture)
{
    // cube-tex.obj carries a complete texture setup: mtllib -> cube.mtl
    // (map_Kd texture.png) and per-face-corner UVs (f v/vt/vn) — 8 shared
    // vertices but 14 distinct texture coordinates, so a vertex maps to
    // DIFFERENT UVs depending on the face. Verifies the OBJ importer:
    //   (1) decodes and attaches the diffuse texture image,
    //   (2) keeps all 14 texture coordinates (with the V flip),
    //   (3) preserves the PER-CORNER texcoord indices instead of collapsing
    //       them onto the vertex index — the bug that rendered the cube grey.
    Mesh* m = new Mesh();
    m->load("./test/data/obj/cube-tex.obj");

    ASSERT_EQ(m->GetNVertices(), 8u);
    ASSERT_EQ(m->GetNFaces(),    12u);

    // (1) Diffuse texture image loaded (requires PNG decoding in cgimg).
    ASSERT_EQ(m->GetNMaterials(), 1u);
    MaterialTexture* tex = dynamic_cast<MaterialTexture*>(m->GetMaterial(0));
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->GetType(), MATERIAL_TEXTURE);
    EXPECT_NE(tex->GetImage(), nullptr)
        << "texture.png was not decoded (is PNG support enabled in cgimg?)";

    // (2) 14 texture coordinates, stored as (u, 1-v). vt #1 = "0.25 1.00" ->
    // (0.25, 0.0); vt #10 = "0.75 0.50" -> (0.75, 0.50).
    EXPECT_EQ(m->m_nTextureCoordinates, 14u);
    ASSERT_EQ(m->m_pTextureCoordinates.size(), 28u);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[0],       0.25f);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[1],       0.0f);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[2*9],     0.75f);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[2*9 + 1], 0.50f);

    // (3) Per-corner texcoord indices. First face is
    //   f 3/10/1 7/6/1 8/5/1  ->  vertices {2,6,7}, texcoords {9,5,4} (0-based).
    Face* f0 = m->m_pFaces[0];
    ASSERT_TRUE(f0->m_bUseTextureCoordinates);
    ASSERT_NE(f0->m_pTextureCoordinatesIndices, nullptr);
    ASSERT_EQ(f0->GetNVertices(), 3);
    EXPECT_EQ(f0->GetVertex(0), 2);
    EXPECT_EQ(f0->GetVertex(1), 6);
    EXPECT_EQ(f0->GetVertex(2), 7);
    EXPECT_EQ(f0->m_pTextureCoordinatesIndices[0], 9u);
    EXPECT_EQ(f0->m_pTextureCoordinatesIndices[1], 5u);
    EXPECT_EQ(f0->m_pTextureCoordinatesIndices[2], 4u);

    // The texcoord index differs from the vertex index -> the mapping is truly
    // per-corner, not the per-vertex collapse that scrambled the UVs.
    EXPECT_NE(f0->m_pTextureCoordinatesIndices[0], (unsigned int)f0->GetVertex(0));

    // Every corner of every textured face must reference an in-range UV.
    for (unsigned int fi = 0; fi < m->GetNFaces(); ++fi)
    {
        Face* f = m->m_pFaces[fi];
        if (!f->m_bUseTextureCoordinates || !f->m_pTextureCoordinatesIndices)
            continue;
        for (int k = 0; k < f->GetNVertices(); ++k)
            EXPECT_LT(2u * f->m_pTextureCoordinatesIndices[k] + 1u,
                      (unsigned int)m->m_pTextureCoordinates.size());
    }

    delete m;
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

TEST(TEST_cgmesh_io, obj_lines_and_points)
{
    // The tree asset is built purely from OBJ 'l' (segments) and 'p' (points)
    // elements sharing one vertex list — it has no faces. Pins down the l/p
    // parser and its export round-trip.
    Mesh* m = new Mesh();
    m->load("./test/data/obj/lines_and_points.obj");

    EXPECT_EQ(m->GetNVertices(), 42u);
    EXPECT_EQ(m->GetNFaces(),     0u);
    EXPECT_EQ(m->GetNLines(),    41u);
    EXPECT_EQ(m->GetNPoints(),   42u);

    // Every segment / point index must reference a real vertex.
    for (unsigned int i = 0; i < m->GetNLines(); ++i)
    {
        EXPECT_LT(m->m_pLines[2*i],   m->GetNVertices());
        EXPECT_LT(m->m_pLines[2*i+1], m->GetNVertices());
    }
    for (unsigned int i = 0; i < m->GetNPoints(); ++i)
        EXPECT_LT(m->m_pPoints[i], m->GetNVertices());

    // Round-trip: l/p must survive export_obj + reload.
    ASSERT_EQ(m->save("./exported_lines_and_points.obj"), 0);

    Mesh* r = new Mesh();
    r->load("./exported_lines_and_points.obj");
    EXPECT_EQ(r->GetNVertices(), 42u);
    EXPECT_EQ(r->GetNLines(),    41u);
    EXPECT_EQ(r->GetNPoints(),   42u);

    delete r;
    delete m;
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

TEST(TEST_cgmesh_io, ply_points)
{
    // context
    Mesh_half_edge* mesh = new Mesh_half_edge();

    // action
    mesh->m_pMesh->load("./test/data/ply/points_on_sphere.ply");

    // expectations
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 8000);
    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 0);
}

TEST(TEST_cgmesh_io, asc_points)
{
    // points_on_sphere.asc holds the same 8000-point cloud as the .ply, in the
    // ASC layout consumed by Mesh::import_asc : one line per point,
    // "x y z r g b nx ny nz" (r/g/b as 0-255 integers, normals synthesized as
    // the normalized position since the points sit on the unit sphere).
    Mesh* m = new Mesh();

    // action : .asc dispatches to import_asc via Mesh::load
    m->load("./test/data/ply/points_on_sphere.asc");

    // a pure point cloud : 8000 vertices, no faces
    ASSERT_EQ(m->GetNVertices(), 8000u);
    EXPECT_EQ(m->GetNFaces(), 0u);

    // positions match the first PLY row verbatim
    ASSERT_EQ(m->m_pVertices.size(), 3u * 8000u);
    EXPECT_NEAR(m->m_pVertices[0],  0.148893f, 1e-5f);
    EXPECT_NEAR(m->m_pVertices[1], -0.981710f, 1e-5f);
    EXPECT_NEAR(m->m_pVertices[2],  0.118643f, 1e-5f);

    // per-vertex colors are decoded to [0,1] : first row was "146 2 142"
    ASSERT_EQ(m->m_pVertexColors.size(), 3u * 8000u);
    EXPECT_NEAR(m->m_pVertexColors[0], 146.f / 255.f, 1e-3f);
    EXPECT_NEAR(m->m_pVertexColors[1],   2.f / 255.f, 1e-3f);
    EXPECT_NEAR(m->m_pVertexColors[2], 142.f / 255.f, 1e-3f);

    // every point lies on the unit sphere and its normal is unit-length
    ASSERT_EQ(m->m_pVertexNormals.size(), 3u * 8000u);
    for (unsigned int i = 0; i < m->GetNVertices(); ++i)
    {
        float x = m->m_pVertices[3*i], y = m->m_pVertices[3*i+1], z = m->m_pVertices[3*i+2];
        EXPECT_NEAR(sqrtf(x*x + y*y + z*z), 1.f, 1e-3f);
        float nx = m->m_pVertexNormals[3*i], ny = m->m_pVertexNormals[3*i+1], nz = m->m_pVertexNormals[3*i+2];
        EXPECT_NEAR(sqrtf(nx*nx + ny*ny + nz*nz), 1.f, 1e-3f);
    }

    delete m;
}

TEST(TEST_cgmesh_io, pset_points)
{
    // points_on_sphere.pset holds the same 8000-point cloud in the PSET layout
    // consumed by Mesh::import_pset : one line per point, "x y z nx ny nz"
    // (position + normal, no color).
    Mesh* m = new Mesh();

    // action : .pset dispatches to import_pset via Mesh::load
    m->load("./test/data/ply/points_on_sphere.pset");

    // a pure point cloud : 8000 vertices, no faces
    ASSERT_EQ(m->GetNVertices(), 8000u);
    EXPECT_EQ(m->GetNFaces(), 0u);

    // positions match the first PLY row verbatim
    ASSERT_EQ(m->m_pVertices.size(), 3u * 8000u);
    EXPECT_NEAR(m->m_pVertices[0],  0.148893f, 1e-5f);
    EXPECT_NEAR(m->m_pVertices[1], -0.981710f, 1e-5f);
    EXPECT_NEAR(m->m_pVertices[2],  0.118643f, 1e-5f);

    // every point lies on the unit sphere and its normal is unit-length
    ASSERT_EQ(m->m_pVertexNormals.size(), 3u * 8000u);
    for (unsigned int i = 0; i < m->GetNVertices(); ++i)
    {
        float x = m->m_pVertices[3*i], y = m->m_pVertices[3*i+1], z = m->m_pVertices[3*i+2];
        EXPECT_NEAR(sqrtf(x*x + y*y + z*z), 1.f, 1e-3f);
        float nx = m->m_pVertexNormals[3*i], ny = m->m_pVertexNormals[3*i+1], nz = m->m_pVertexNormals[3*i+2];
        EXPECT_NEAR(sqrtf(nx*nx + ny*ny + nz*nz), 1.f, 1e-3f);
    }

    delete m;
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

// IGES import via OpenCASCADE — WIREFRAME files.
//
// The three sample files (ex1.iges, ex2.iges, ex3.iges) are wireframe /
// 2D-drafting IGES: they hold curves (types 100/104/106/110) and drawing
// structure, but no surface (128/143/144) or solid (186) entities, so there
// is nothing to tessellate into faces.
//
// tessellateAndAppend now imports the FREE wireframe (edges that bound no
// face -> discretised polyline segments; standalone vertices -> explicit
// points), so such a file loads as a face-less line/point mesh instead of
// being refused. The contract pinned here: load succeeds, produces meshes
// with line segments and NO faces. (Exact segment counts depend on OCCT's
// discretisation and which drafting entities its IGES reader transfers to
// TopoDS, so we assert qualitatively.)
class TEST_cgmesh_io_iges_wireframe : public ::testing::TestWithParam<std::string> {};

TEST_P(TEST_cgmesh_io_iges_wireframe, imports_free_wireframe)
{
    const std::string filename = std::string("./test/data/") + GetParam();

    VMeshes* pVMeshes = new VMeshes();
    const bool ok = pVMeshes->load(filename.c_str());

    unsigned int nLines = 0, nPoints = 0, nFaces = 0;
    for (Mesh* m : pVMeshes->GetMeshes())
    {
        nLines  += m->GetNLines();
        nPoints += m->GetNPoints();
        nFaces  += m->GetNFaces();
    }
    std::cout << "[wireframe] " << GetParam() << " load=" << ok
              << " meshes=" << pVMeshes->GetMeshes().size()
              << " lines=" << nLines << " points=" << nPoints
              << " faces=" << nFaces << std::endl;

    EXPECT_TRUE(ok) << "wireframe IGES " << filename << " should now import as polylines";
    EXPECT_GT(nLines, 0u) << "expected polyline segments from the free curves";
    EXPECT_EQ(nFaces, 0u) << "these files carry no surface, so no faces";

    // Every segment / point index must reference a real vertex (line vertices
    // live in m_pVertices, same convention as the OBJ l/p path).
    for (Mesh* m : pVMeshes->GetMeshes())
    {
        for (unsigned int i = 0; i < m->GetNLines(); ++i)
        {
            EXPECT_LT(m->m_pLines[2*i],     m->GetNVertices());
            EXPECT_LT(m->m_pLines[2*i + 1], m->GetNVertices());
        }
        for (unsigned int i = 0; i < m->GetNPoints(); ++i)
            EXPECT_LT(m->m_pPoints[i], m->GetNVertices());
    }

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
