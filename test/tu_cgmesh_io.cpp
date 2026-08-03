#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/mesh_io.h"       // MeshIO::export_obj (points d entree par format)
#include "../src/cgmesh/zip_manager.h"   // ZipManager::Crc32, pour verifier l archive


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

// A single OBJ file may hold several objects ('o'/'g' directives). The importer
// flattens them into one mesh: the directives are ignored and vertex indices
// stay file-global (1-based) across objects. Guard that a two-object file loads
// with the summed counts and that the SECOND object's face resolves to the
// file-global vertices it declares (4,5,6 -> 0-based 3,4,5), not a per-object
// local numbering.
TEST(TEST_cgmesh_io, obj_multiple_objects)
{
    Mesh* m = new Mesh();
    m->load("./test/data/obj/multi_objects.obj");

    // 2 triangles (one per object) => 6 vertices, 2 faces.
    ASSERT_EQ(m->GetNVertices(), 6u);
    ASSERT_EQ(m->GetNFaces(),    2u);

    // First object's face -> vertices 0,1,2.
    Face* fA = m->m_pFaces[0];
    ASSERT_EQ(fA->GetNVertices(), 3);
    EXPECT_EQ(fA->GetVertex(0), 0);
    EXPECT_EQ(fA->GetVertex(1), 1);
    EXPECT_EQ(fA->GetVertex(2), 2);

    // Second object's face -> file-global vertices 3,4,5.
    Face* fB = m->m_pFaces[1];
    ASSERT_EQ(fB->GetNVertices(), 3);
    EXPECT_EQ(fB->GetVertex(0), 3);
    EXPECT_EQ(fB->GetVertex(1), 4);
    EXPECT_EQ(fB->GetVertex(2), 5);

    // The second triangle's first vertex is the one declared as "v 2 0 0".
    Vector3f v;
    m->GetVertex((unsigned int)fB->GetVertex(0), v);
    EXPECT_FLOAT_EQ(v.x, 2.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);

    delete m;
}

// VMeshesIO::load splits a multi-object OBJ into one Mesh per object (unlike
// Mesh::load, which flattens). Each submesh must own only the vertices its own
// faces reference, re-indexed to a local pool.
TEST(TEST_cgmesh_io, vmeshes_obj_split_objects)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/obj/multi_objects.obj"));

    // Two 'o' objects -> two distinct meshes.
    ASSERT_EQ(vm.GetNMeshes(), 2u);

    Mesh* a = vm.GetMeshes()[0];
    Mesh* b = vm.GetMeshes()[1];

    // Names carried from the 'o' directives.
    EXPECT_EQ(a->m_name, "triangle_A");
    EXPECT_EQ(b->m_name, "triangle_B");

    // Each submesh: 3 local vertices, 1 face (re-indexed, not the file-global 6).
    ASSERT_EQ(a->GetNVertices(), 3u);
    ASSERT_EQ(a->GetNFaces(),    1u);
    ASSERT_EQ(b->GetNVertices(), 3u);
    ASSERT_EQ(b->GetNFaces(),    1u);

    // Local re-indexing: object B's face references local 0,1,2 even though the
    // file used the global indices 4,5,6.
    EXPECT_EQ(b->m_pFaces[0]->GetVertex(0), 0);
    EXPECT_EQ(b->m_pFaces[0]->GetVertex(1), 1);
    EXPECT_EQ(b->m_pFaces[0]->GetVertex(2), 2);

    // ...and that local vertex 0 holds the coordinates declared as "v 2 0 0".
    Vector3f v;
    b->GetVertex(0u, v);
    EXPECT_FLOAT_EQ(v.x, 2.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);

    // Aggregate over the container still sums to the whole file.
    EXPECT_EQ(vm.GetNVertices(), 6u);
    EXPECT_EQ(vm.GetNFaces(),    2u);
}

// Each submesh must carry only the material(s) its own faces use, cloned into
// the submesh (a Mesh owns its materials), with the material name preserved.
TEST(TEST_cgmesh_io, vmeshes_obj_split_materials)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/obj/multi_objects_mtl.obj"));
    ASSERT_EQ(vm.GetNMeshes(), 2u);

    Mesh* a = vm.GetMeshes()[0];
    Mesh* b = vm.GetMeshes()[1];

    // Only the used material lands in each submesh.
    ASSERT_EQ(a->GetNMaterials(), 1u);
    ASSERT_EQ(b->GetNMaterials(), 1u);
    EXPECT_EQ(a->GetMaterial(0)->GetName(), "red");
    EXPECT_EQ(b->GetMaterial(0)->GetName(), "blue");

    // The face points at its submesh-local material id (0 here).
    EXPECT_EQ(a->m_pFaces[0]->GetMaterialId(), 0u);
    EXPECT_EQ(b->m_pFaces[0]->GetMaterialId(), 0u);
}

// When several submeshes reference the same textured material, they must SHARE
// one decoded image (shared_ptr<Img>), not each own a duplicate — and the
// texture must survive the material clone (the old copy-ctor produced a blank).
TEST(TEST_cgmesh_io, vmeshes_obj_split_shared_texture)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/obj/multi_objects_tex.obj"));
    ASSERT_EQ(vm.GetNMeshes(), 2u);

    MaterialTexture* ta = dynamic_cast<MaterialTexture*>(vm.GetMeshes()[0]->GetMaterial(0));
    MaterialTexture* tb = dynamic_cast<MaterialTexture*>(vm.GetMeshes()[1]->GetMaterial(0));
    ASSERT_NE(ta, nullptr);
    ASSERT_NE(tb, nullptr);

    // Texture preserved through the clone...
    ASSERT_NE(ta->GetImage(), nullptr) << "cloned texture lost its image";
    // ...and SHARED between the two submeshes, not duplicated.
    EXPECT_EQ(ta->GetImage(), tb->GetImage());
}

// With no 'o' in the file, the importer splits on 'g' instead.
TEST(TEST_cgmesh_io, vmeshes_obj_split_groups)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/obj/multi_groups.obj"));
    ASSERT_EQ(vm.GetNMeshes(), 2u);
    EXPECT_EQ(vm.GetMeshes()[0]->m_name, "left");
    EXPECT_EQ(vm.GetMeshes()[1]->m_name, "right");
    EXPECT_EQ(vm.GetMeshes()[0]->GetNVertices(), 3u);
    EXPECT_EQ(vm.GetMeshes()[1]->GetNVertices(), 3u);
}

// A single-object (or object-less) OBJ must load as exactly one mesh.
TEST(TEST_cgmesh_io, vmeshes_obj_single_object)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/obj/cube.obj"));
    EXPECT_EQ(vm.GetNMeshes(),   1u);
    EXPECT_EQ(vm.GetNVertices(), 8u);
    EXPECT_EQ(vm.GetNFaces(),    12u);
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

// Regression: an ASCII STL must be parsed as text, not as binary. Reading the
// 4 bytes at offset 80 of an ASCII file as a triangle count yields garbage
// (often hundreds of millions) → a bogus giant allocation → crash.
TEST(TEST_cgmesh_io, stl_ascii)
{
    const char* path = "./ascii_tri.stl";
    {
        std::ofstream o(path);
        o << "solid test\n";
        o << "facet normal 0 0 1\n outer loop\n"
             "  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\nendfacet\n";
        o << "facet normal 0 0 1\n outer loop\n"
             "  vertex 1 0 0\n  vertex 1 1 0\n  vertex 0 1 0\n endloop\nendfacet\n";
        o << "endsolid test\n";
    }

    Mesh_half_edge* mesh = new Mesh_half_edge();
    mesh->m_pMesh->load(path);

    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 2u);
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 6u);

    delete mesh;
    std::remove(path);
}

// Regression: extension matching must be case-insensitive, so "*.STL" / "*.Stl"
// dispatch to the STL importer instead of silently loading nothing.
TEST(TEST_cgmesh_io, stl_uppercase_extension)
{
    const char* path = "./ascii_tri_upper.STL";
    {
        std::ofstream o(path);
        o << "solid test\n";
        o << "facet normal 0 0 1\n outer loop\n"
             "  vertex 0 0 0\n  vertex 1 0 0\n  vertex 0 1 0\n endloop\nendfacet\n";
        o << "endsolid test\n";
    }

    Mesh_half_edge* mesh = new Mesh_half_edge();
    mesh->m_pMesh->load(path);

    EXPECT_EQ(mesh->m_pMesh->GetNFaces(), 1u);
    EXPECT_EQ(mesh->m_pMesh->GetNVertices(), 3u);

    delete mesh;
    std::remove(path);
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
    VMeshesIO::load(*pVMeshes,"./test/data/sink.3ds");

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
    VMeshesIO::load(*pVMeshes,"./test/data/display.3ds");

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
    VMeshesIO::load(*pVMeshes,"./test/data/floppy.3ds");

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
    bool res = VMeshesIO::load(*pVMeshes,"./test/data/Duck.glb");

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

    bool res = VMeshesIO::load(*pVMeshes,"./test/data/Fox.glb");

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

    bool res = VMeshesIO::load(*pVMeshes,"./test/data/4pinplug.stp");
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
    const bool ok = VMeshesIO::load(*pVMeshes,filename.c_str());

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

    ASSERT_TRUE(VMeshesIO::load(*pVMeshes,"./test/data/Milled_Front_X_Carriage.iges"));

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

    ASSERT_TRUE(VMeshesIO::load(*pVMeshes,filename.c_str())) << "load failed for " << filename;

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

// ---------------------------------------------------------------------------
//  Coherence des noms de materiau entre .obj et .mtl
// ---------------------------------------------------------------------------
// export_obj ecrivait `usemtl <GetName()>` dans le .obj mais
// `newmtl material_<index>` dans le .mtl : aucun usemtl ne resolvait vers son
// newmtl des que le materiau portait un nom. Symptome trompeur -- le fichier
// s'ouvre sans erreur et le modele sort SANS COULEUR, le lecteur ne trouvant pas
// les materiaux nommes. Les deux passent desormais par objMaterialName().
TEST(TEST_cgmesh_io, obj_usemtl_names_resolve_in_the_mtl)
{
    // context : un quad par materiau, chacun nomme explicitement.
    Mesh mesh;
    float verts[] = {
        0,0,0,  1,0,0,  1,1,0,  0,1,0,
        2,0,0,  3,0,0,  3,1,0,  2,1,0,
    };
    mesh.SetVertices(8, verts);
    // Deux quads : nVerticesPerFace = 4, indices a plat.
    unsigned int faces[] = { 0,1,2,3,  4,5,6,7 };
    mesh.SetFaces(2, 4, faces);

    auto* red = new MaterialColor(255, 0, 0);
    red->SetName("rouge_vif");
    auto* blue = new MaterialColor(0, 0, 255);
    blue->SetName("bleu nuit");          // avec un ESPACE : usemtl s'arrete au blanc
    mesh.Material_Add(red);
    mesh.Material_Add(blue);
    mesh.SetFaceMaterialId(0, 0);
    mesh.SetFaceMaterialId(1, 1);

    // action
    const char* objPath = "./tu_obj_matnames.obj";
    const char* mtlPath = "./tu_obj_matnames.mtl";
    ASSERT_EQ(mesh.save(objPath), 0);

    auto readAll = [](const char* p) {
        std::ifstream f(p);
        return std::string((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    };
    const std::string obj = readAll(objPath);
    const std::string mtl = readAll(mtlPath);
    ASSERT_FALSE(obj.empty());
    ASSERT_FALSE(mtl.empty()) << "le .mtl compagnon doit etre ecrit a cote du .obj";

    // expectations : chaque `usemtl X` du .obj a son `newmtl X` dans le .mtl.
    auto collect = [](const std::string& text, const std::string& keyword) {
        std::vector<std::string> names;
        size_t pos = 0;
        while ((pos = text.find(keyword, pos)) != std::string::npos)
        {
            // uniquement en debut de ligne
            if (pos != 0 && text[pos - 1] != '\n') { pos += keyword.size(); continue; }
            size_t b = pos + keyword.size();
            size_t e = text.find_first_of("\r\n", b);
            names.push_back(text.substr(b, e - b));
            pos = b;
        }
        return names;
    };
    const std::vector<std::string> used     = collect(obj, "usemtl ");
    const std::vector<std::string> declared = collect(mtl, "newmtl ");

    EXPECT_EQ(used.size(), 2u);
    EXPECT_EQ(declared.size(), 2u);
    for (const std::string& u : used)
        EXPECT_NE(std::find(declared.begin(), declared.end(), u), declared.end())
            << "usemtl \"" << u << "\" n'a pas de newmtl correspondant";

    // Un espace dans le nom couperait le usemtl : il doit avoir ete remplace.
    for (const std::string& d : declared)
        EXPECT_EQ(d.find(' '), std::string::npos)
            << "un nom de materiau ne doit pas contenir d'espace : \"" << d << "\"";

    std::remove(objPath);
    std::remove(mtlPath);
}

// emitObjectGroups : un OBJ multi-materiaux devient separable en objets. Defaut
// false, pour ne rien changer aux appelants existants.
TEST(TEST_cgmesh_io, obj_object_groups_are_opt_in)
{
    Mesh mesh;
    float verts[] = { 0,0,0, 1,0,0, 1,1,0, 0,1,0,   2,0,0, 3,0,0, 3,1,0, 2,1,0 };
    mesh.SetVertices(8, verts);
    unsigned int faces[] = { 0,1,2,3,  4,5,6,7 };
    mesh.SetFaces(2, 4, faces);
    auto* a = new MaterialColor(255, 0, 0); a->SetName("piece_a");
    auto* b = new MaterialColor(0, 0, 255); b->SetName("piece_b");
    mesh.Material_Add(a);
    mesh.Material_Add(b);
    mesh.SetFaceMaterialId(0, 0);
    mesh.SetFaceMaterialId(1, 1);

    auto readAll = [](const char* p) {
        std::ifstream f(p);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    };
    auto countLines = [](const std::string& t, const std::string& kw) {
        size_t n = 0, pos = 0;
        while ((pos = t.find(kw, pos)) != std::string::npos) {
            if (pos == 0 || t[pos - 1] == '\n') n++;
            pos += kw.size();
        }
        return n;
    };

    // defaut : aucun groupe objet
    ASSERT_EQ(MeshIO::export_obj(mesh, "./tu_obj_nogroups.obj"), 0);
    const std::string plain = readAll("./tu_obj_nogroups.obj");
    EXPECT_EQ(countLines(plain, "o "), 0u);
    EXPECT_EQ(countLines(plain, "usemtl "), 2u);

    // demande : un `o` par changement de materiau, AVANT son usemtl
    ASSERT_EQ(MeshIO::export_obj(mesh, "./tu_obj_groups.obj", true), 0);
    const std::string grouped = readAll("./tu_obj_groups.obj");
    EXPECT_EQ(countLines(grouped, "o "), 2u);
    EXPECT_EQ(countLines(grouped, "usemtl "), 2u);
    EXPECT_NE(grouped.find("o piece_a\nusemtl piece_a\n"), std::string::npos);
    EXPECT_NE(grouped.find("o piece_b\nusemtl piece_b\n"), std::string::npos);

    for (const char* p : { "./tu_obj_nogroups.obj", "./tu_obj_nogroups.mtl",
                           "./tu_obj_groups.obj",   "./tu_obj_groups.mtl" })
        std::remove(p);
}

// Le .mtl exporte doit etre OPAQUE et sans speculaire incoherent.
//
// export_obj ecrivait `Tr 1.000000` : dans la spec MTL, `Tr` est la TRANSPARENCE
// (1 = totalement transparent) et c'est `d` qui vaut 1 pour opaque. Tout lecteur
// conforme rendait donc le modele invisible -- sauf en incidence rasante, ou
// l'accumulation des epaisseurs le fait reapparaitre. cgmesh ne le voyait pas :
// son propre import_mtl lit `d`/`Tr` puis les jette.
//
// Ecrivait aussi `Ks 1 1 1` avec `Ns 0` : exposant nul => terme speculaire
// CONSTANT sur toute la surface, donc un voile blanc uniforme.
TEST(TEST_cgmesh_io, obj_mtl_is_opaque_and_has_no_stray_specular)
{
    Mesh mesh;
    float verts[] = { 0,0,0, 1,0,0, 1,1,0, 0,1,0 };
    mesh.SetVertices(4, verts);
    unsigned int faces[] = { 0,1,2,3 };
    mesh.SetFaces(1, 4, faces);
    auto* c = new MaterialColor(200, 100, 50);
    c->SetName("terre_cuite");
    mesh.Material_Add(c);
    mesh.SetFaceMaterialId(0, 0);

    ASSERT_EQ(MeshIO::export_obj(mesh, "./tu_obj_mtlopacity.obj"), 0);
    // Bloc explicite : le flux doit etre FERME avant le std::remove de la fin, sans
    // quoi Windows refuse de supprimer le fichier encore ouvert et le .mtl reste
    // dans le repertoire de sortie. Les autres tests de ce fichier lisent via une
    // lambda readAll(), dont le flux local est detruit au retour -- meme effet.
    std::string mtl;
    {
        std::ifstream f("./tu_obj_mtlopacity.mtl");
        mtl.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    ASSERT_FALSE(mtl.empty());

    // Opacite : `d 1`, et surtout PAS `Tr 1` qui signifie l'inverse.
    EXPECT_NE(mtl.find("d 1.000000"), std::string::npos) << mtl;
    EXPECT_EQ(mtl.find("Tr 1.000000"), std::string::npos)
        << "Tr 1 = totalement TRANSPARENT : le modele disparait chez tout lecteur conforme\n" << mtl;

    // Pas de speculaire blanc a exposant nul.
    EXPECT_EQ(mtl.find("Ks 1.000000 1.000000 1.000000"), std::string::npos) << mtl;

    // La couleur, elle, doit bien y etre.
    EXPECT_NE(mtl.find("Kd 0.784314 0.392157 0.196078"), std::string::npos) << mtl;

    std::remove("./tu_obj_mtlopacity.obj");
    std::remove("./tu_obj_mtlopacity.mtl");
}

// ---------------------------------------------------------------------------
//  OBJ + MTL dans une archive ZIP
// ---------------------------------------------------------------------------
// Code au niveau OCTET (en-tetes, offsets, CRC) : il se verifie en RELISANT
// l'archive comme le ferait un vrai lecteur ZIP -- parcours du repertoire central,
// puis recalcul des CRC declares.
TEST(TEST_cgmesh_io, obj_zip_bundles_obj_and_mtl)
{
    Mesh mesh;
    float verts[] = { 0,0,0, 1,0,0, 1,1,0, 0,1,0 };
    mesh.SetVertices(4, verts);
    unsigned int faces[] = { 0,1,2,3 };
    mesh.SetFaces(1, 4, faces);
    auto* c = new MaterialColor(10, 200, 30);
    c->SetName("vert_pomme");
    mesh.Material_Add(c);
    mesh.SetFaceMaterialId(0, 0);

    const char* zipPath = "./tu_obj_bundle.zip";
    ASSERT_EQ(MeshIO::export_obj_zip(mesh, zipPath), 0);

    std::ifstream f(zipPath, std::ios::binary);
    const std::string z((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    ASSERT_GT(z.size(), 22u) << "archive trop courte pour contenir un EOCD";

    auto rd16 = [&](size_t o) { return (unsigned int)((unsigned char)z[o] | ((unsigned char)z[o+1] << 8)); };
    auto rd32 = [&](size_t o) { return (unsigned int)((unsigned char)z[o] | ((unsigned char)z[o+1] << 8)
                                                    | ((unsigned char)z[o+2] << 16) | ((unsigned char)z[o+3] << 24)); };

    // EOCD : cherche depuis la fin, comme un vrai lecteur.
    size_t eocd = std::string::npos;
    for (size_t i = z.size() - 22; i != (size_t)-1; --i)
        if (rd32(i) == 0x06054b50u) { eocd = i; break; }
    ASSERT_NE(eocd, std::string::npos) << "EOCD introuvable";

    const unsigned int nEntries = rd16(eocd + 10);
    const unsigned int cdSize   = rd32(eocd + 12);
    const unsigned int cdOff    = rd32(eocd + 16);
    EXPECT_EQ(nEntries, 2u) << "attendu : le .obj ET le .mtl";
    EXPECT_EQ(cdOff + cdSize, (unsigned int)eocd) << "le repertoire central doit finir sur l'EOCD";

    // Parcours du repertoire central + verification de chaque entree.
    std::vector<std::string> names;
    size_t p = cdOff;
    for (unsigned int k = 0; k < nEntries; ++k)
    {
        ASSERT_EQ(rd32(p), 0x02014b50u) << "signature centrale invalide, entree " << k;
        const unsigned int method = rd16(p + 10);
        const unsigned int crc    = rd32(p + 16);
        const unsigned int csize  = rd32(p + 20);
        const unsigned int usize  = rd32(p + 24);
        const unsigned int nlen   = rd16(p + 28);
        const unsigned int lho    = rd32(p + 42);
        const std::string name = z.substr(p + 46, nlen);
        names.push_back(name);

        EXPECT_EQ(method, 0u) << "entrees attendues non compressees (stored)";
        EXPECT_EQ(csize, usize) << "en stored, les deux tailles sont egales";
        ASSERT_EQ(rd32(lho), 0x04034b50u) << "signature d'en-tete local invalide";

        // Donnees : en-tete local (30 o) + nom + extra
        const size_t dataStart = lho + 30 + rd16(lho + 26) + rd16(lho + 28);
        ASSERT_LE(dataStart + usize, z.size());
        EXPECT_EQ(ZipManager::Crc32(z.data() + dataStart, usize), crc)
            << "CRC declare incorrect pour " << name;

        // Le contenu doit etre celui qu'on attend.
        const std::string data = z.substr(dataStart, usize);
        if (name.size() > 4 && name.compare(name.size()-4, 4, ".obj") == 0)
        {
            EXPECT_NE(data.find("mtllib tu_obj_bundle.mtl"), std::string::npos)
                << "la ligne mtllib doit designer l'entree .mtl de l'archive";
            EXPECT_NE(data.find("usemtl vert_pomme"), std::string::npos);
        }
        else
        {
            EXPECT_NE(data.find("newmtl vert_pomme"), std::string::npos);
            EXPECT_NE(data.find("d 1.000000"), std::string::npos);
        }
        p += 46 + nlen + rd16(p + 30) + rd16(p + 32);
    }
    // Les noms internes prennent le radical de l'archive.
    EXPECT_NE(std::find(names.begin(), names.end(), "tu_obj_bundle.obj"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "tu_obj_bundle.mtl"), names.end());

    std::remove(zipPath);
}

// Sans materiau, l'archive ne contient QUE le .obj -- pas d'entree .mtl vide.
TEST(TEST_cgmesh_io, obj_zip_omits_the_mtl_when_there_is_no_material)
{
    Mesh mesh;
    float verts[] = { 0,0,0, 1,0,0, 1,1,0 };
    mesh.SetVertices(3, verts);
    unsigned int faces[] = { 0,1,2 };
    mesh.SetFaces(1, 3, faces);

    const std::string bytes = MeshIO::export_obj_zip_bytes(mesh, "nu");
    ASSERT_FALSE(bytes.empty());
    auto rd16 = [&](size_t o) { return (unsigned int)((unsigned char)bytes[o] | ((unsigned char)bytes[o+1] << 8)); };
    auto rd32 = [&](size_t o) { return (unsigned int)((unsigned char)bytes[o] | ((unsigned char)bytes[o+1] << 8)
                                                    | ((unsigned char)bytes[o+2] << 16) | ((unsigned char)bytes[o+3] << 24)); };
    size_t eocd = std::string::npos;
    for (size_t i = bytes.size() - 22; i != (size_t)-1; --i)
        if (rd32(i) == 0x06054b50u) { eocd = i; break; }
    ASSERT_NE(eocd, std::string::npos);
    EXPECT_EQ(rd16(eocd + 10), 1u) << "une seule entree attendue : le .obj";
}

// ===========================================================================
//  Import MTL : les VALEURS
// ===========================================================================
// Les tests d'import existants (vmeshes_obj_split_materials, obj_cube_tex_texture)
// verifient le NOM du materiau et la presence de la texture, jamais un nombre. Un
// Kd lu comme gris, ou les canaux R et B permutes, passait donc inapercu -- alors
// que quatre defauts de MTL du cote EXPORT ont ete corriges recemment (Tr, Ks/Ns,
// Kd noir en branche texture, usemtl/newmtl desaccordes).
//
// Chaque champ recoit ici des valeurs DISTINCTES canal par canal ET distinctes
// d'un champ a l'autre : une permutation de canaux comme un echange de champs
// (Ka lu dans Kd) sortent tous deux du lot.

namespace
{
    // Ecrit un couple .obj/.mtl minimal. Le .mtl est nomme d'apres le .obj, car
    // import_obj resout `mtllib` relativement au repertoire du .obj.
    void writeObjWithMtl (const std::string& stem, const std::string& mtlBody,
                          const std::string& usemtlName = "probe")
    {
        {
            std::ofstream f(stem + ".mtl");
            ASSERT_TRUE(f.is_open());
            f << mtlBody;
        }
        std::ofstream f(stem + ".obj");
        ASSERT_TRUE(f.is_open());
        // basename : mtllib est resolu par rapport au .obj, pas au repertoire courant
        const size_t slash = stem.find_last_of("/\\");
        const std::string base = (slash == std::string::npos) ? stem : stem.substr(slash + 1);
        f << "mtllib " << base << ".mtl\n"
             "v 0 0 0\nv 1 0 0\nv 1 1 0\n";
        if (!usemtlName.empty())
            f << "usemtl " << usemtlName << "\n";
        f << "f 1 2 3\n";
    }

    void removeObjWithMtl (const std::string& stem)
    {
        std::remove((stem + ".obj").c_str());
        std::remove((stem + ".mtl").c_str());
    }
}

// Les six champs numeriques que import_mtl declare parser : Ka, Kd, Ks, Ke, Ns.
TEST(TEST_cgmesh_io, obj_mtl_import_reads_every_colour_channel)
{
    const std::string stem = "./tu_obj_mtlvalues";
    writeObjWithMtl(stem,
        "newmtl probe\n"
        "Ka 0.10 0.20 0.30\n"
        "Kd 0.40 0.50 0.60\n"
        "Ks 0.70 0.80 0.90\n"
        "Ke 0.11 0.22 0.33\n"
        "Ns 64\n");

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 1u);

    MaterialColorExt* m = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
    ASSERT_NE(m, nullptr) << "un MTL sans map_Kd doit donner un MaterialColorExt";
    EXPECT_EQ(m->GetName(), "probe");

    float d[4];
    m->GetDiffuse(d);
    EXPECT_NEAR(d[0], 0.40f, 1e-5f) << "Kd.r";
    EXPECT_NEAR(d[1], 0.50f, 1e-5f) << "Kd.g";
    EXPECT_NEAR(d[2], 0.60f, 1e-5f) << "Kd.b";

    EXPECT_NEAR(m->m_fAmbient[0], 0.10f, 1e-5f) << "Ka.r";
    EXPECT_NEAR(m->m_fAmbient[1], 0.20f, 1e-5f) << "Ka.g";
    EXPECT_NEAR(m->m_fAmbient[2], 0.30f, 1e-5f) << "Ka.b";

    EXPECT_NEAR(m->m_fSpecular[0], 0.70f, 1e-5f) << "Ks.r";
    EXPECT_NEAR(m->m_fSpecular[1], 0.80f, 1e-5f) << "Ks.g";
    EXPECT_NEAR(m->m_fSpecular[2], 0.90f, 1e-5f) << "Ks.b";

    EXPECT_NEAR(m->m_fEmission[0], 0.11f, 1e-5f) << "Ke.r";
    EXPECT_NEAR(m->m_fEmission[1], 0.22f, 1e-5f) << "Ke.g";
    EXPECT_NEAR(m->m_fEmission[2], 0.33f, 1e-5f) << "Ke.b";

    // Ns est un exposant de Phong OBJ (0..1000) stocke en fraction de 128, parce
    // que ActivateMaterial remultiplie par 128 (cap GL_SHININESS).
    EXPECT_NEAR(m->m_fShininess[0], 64.0f / 128.0f, 1e-5f) << "Ns";

    // La face pointe bien sur ce materiau, et non sur MATERIAL_NONE.
    ASSERT_EQ(mesh.GetNFaces(), 1u);
    EXPECT_EQ(mesh.m_pFaces[0]->GetMaterialId(), 0u);

    removeObjWithMtl(stem);
}

// Ns hors de 0..128 est ECRETE, pas mis a l'echelle depuis 1000. C'est une perte
// assumee (cf. le commentaire de import_mtl) : la figer evite qu'un futur passage
// a une remise a l'echelle 0..1000 se fasse en silence.
TEST(TEST_cgmesh_io, obj_mtl_import_clamps_the_specular_exponent)
{
    struct Case { const char* ns; float expected; const char* why; };
    const Case cases[] = {
        { "0",    0.0f,        "Ns nul" },
        { "128",  1.0f,        "Ns a la borne haute" },
        { "1000", 1.0f,        "Ns=1000, legal en OBJ, ecrete a 128" },
        { "-5",   0.0f,        "Ns negatif, ecrete a 0" },
        { "32",   32.f/128.f,  "Ns intermediaire" },
    };

    for (const Case& c : cases)
    {
        SCOPED_TRACE(c.why);
        const std::string stem = std::string("./tu_obj_mtlns_") + c.ns;
        writeObjWithMtl(stem, std::string("newmtl probe\nKd 1 1 1\nNs ") + c.ns + "\n");

        Mesh mesh;
        ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
        ASSERT_EQ(mesh.GetNMaterials(), 1u);
        MaterialColorExt* m = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
        ASSERT_NE(m, nullptr);
        EXPECT_NEAR(m->m_fShininess[0], c.expected, 1e-5f);

        removeObjWithMtl(stem);
    }
}

// import_mtl porte une branche dediee a `Ns <e> Ni <i>` sur UNE SEULE ligne, en
// plus des branches Ns et Ni separees. Aucun test ne l'exercait, et elle est
// fragile : elle precede la branche `Ns` seule, donc un ordre d'alternatives
// inverse la rendrait morte sans que rien ne le signale.
TEST(TEST_cgmesh_io, obj_mtl_import_accepts_ns_and_ni_on_one_line)
{
    const std::string stem = "./tu_obj_mtlnsni";
    writeObjWithMtl(stem, "newmtl probe\nKd 1 1 1\nNs 64 Ni 1.5\n");

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 1u);
    MaterialColorExt* m = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
    ASSERT_NE(m, nullptr);
    EXPECT_NEAR(m->m_fShininess[0], 64.0f / 128.0f, 1e-5f)
        << "la branche `Ns %f Ni %f` n'a pas pris, ou a pris la mauvaise valeur";

    removeObjWithMtl(stem);
}

// Plusieurs materiaux dans un meme .mtl : l'ordre de declaration fixe les ids, et
// chaque materiau garde SES valeurs. Attrape un etat `mat` mal reinitialise entre
// deux `newmtl`, qui ferait fuir les champs du premier vers le second.
TEST(TEST_cgmesh_io, obj_mtl_import_keeps_materials_separate)
{
    const std::string stem = "./tu_obj_mtlmulti";
    writeObjWithMtl(stem,
        "newmtl first\n"
        "Kd 1.0 0.0 0.0\n"
        "Ns 16\n"
        "newmtl second\n"
        "Kd 0.0 0.0 1.0\n",           // pas de Ns : doit rester a son defaut
        "second");

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 2u);

    MaterialColorExt* a = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
    MaterialColorExt* b = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(1));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->GetName(), "first");
    EXPECT_EQ(b->GetName(), "second");

    float da[4], db[4];
    a->GetDiffuse(da);
    b->GetDiffuse(db);
    EXPECT_NEAR(da[0], 1.0f, 1e-5f); EXPECT_NEAR(da[2], 0.0f, 1e-5f);
    EXPECT_NEAR(db[0], 0.0f, 1e-5f); EXPECT_NEAR(db[2], 1.0f, 1e-5f);

    EXPECT_NEAR(a->m_fShininess[0], 16.0f / 128.0f, 1e-5f);
    EXPECT_NEAR(b->m_fShininess[0], 0.0f, 1e-5f) << "le Ns de `first` a fuit vers `second`";

    // `usemtl second` doit resoudre vers l'id 1, pas vers le premier materiau.
    ASSERT_EQ(mesh.GetNFaces(), 1u);
    EXPECT_EQ(mesh.m_pFaces[0]->GetMaterialId(), 1u);

    removeObjWithMtl(stem);
}

// ---------------------------------------------------------------------------
//  Import MTL : robustesse
// ---------------------------------------------------------------------------
// Ces trois cas se produisent pour de vrai : un .obj distribue sans son .mtl, un
// usemtl qui ne resout pas (c'etait le defaut usemtl/newmtl corrige recemment), et
// un .mtl abime. Dans les trois cas la GEOMETRIE doit survivre : mieux vaut un
// modele sans couleur qu'un echec de chargement.

TEST(TEST_cgmesh_io, obj_survives_a_missing_mtllib)
{
    const std::string stem = "./tu_obj_nomtl";
    {
        std::ofstream f(stem + ".obj");
        ASSERT_TRUE(f.is_open());
        f << "mtllib tu_obj_nomtl_does_not_exist.mtl\n"
             "v 0 0 0\nv 1 0 0\nv 1 1 0\n"
             "usemtl whatever\n"
             "f 1 2 3\n";
    }

    Mesh mesh;
    EXPECT_EQ(mesh.load((stem + ".obj").c_str()), 0) << "le .mtl absent ne doit pas faire echouer le chargement";
    EXPECT_EQ(mesh.GetNVertices(), 3u) << "geometrie perdue";
    EXPECT_EQ(mesh.GetNFaces(), 1u);
    EXPECT_EQ(mesh.GetNMaterials(), 0u) << "aucun materiau ne peut avoir ete lu";
    ASSERT_EQ(mesh.GetNFaces(), 1u);
    EXPECT_EQ(mesh.m_pFaces[0]->GetMaterialId(), MATERIAL_NONE)
        << "la face doit rester sans materiau, pas pointer sur un id inexistant";

    std::remove((stem + ".obj").c_str());
}

// `usemtl` nommant un materiau absent du .mtl : GetMaterialId renvoie -1. La face
// ne doit PAS se retrouver avec un id hors bornes, qui ferait deborder toute
// boucle indexant m_pMaterials.
TEST(TEST_cgmesh_io, obj_usemtl_naming_an_unknown_material_leaves_the_face_unassigned)
{
    const std::string stem = "./tu_obj_badusemtl";
    writeObjWithMtl(stem, "newmtl present\nKd 1 0 0\n", "absent");

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    EXPECT_EQ(mesh.GetNVertices(), 3u);
    ASSERT_EQ(mesh.GetNMaterials(), 1u) << "le materiau declare doit quand meme etre lu";
    EXPECT_EQ(mesh.GetMaterial(0)->GetName(), "present");

    // import_obj fait `if (usemtl != -1) pFace->m_iMaterialId = usemtl;` : sur un
    // nom non resolu la face garde son defaut, MATERIAL_NONE. Sans cette garde,
    // le -1 de GetMaterialId deviendrait un id non signe enorme.
    ASSERT_EQ(mesh.GetNFaces(), 1u);
    EXPECT_EQ(mesh.m_pFaces[0]->GetMaterialId(), MATERIAL_NONE)
        << "un usemtl non resolu a tout de meme affecte un id a la face";

    removeObjWithMtl(stem);
}

// .mtl abime : directives inconnues, champ tronque, valeurs non numeriques. Le
// parseur signale sur la sortie standard et continue ; les materiaux VALIDES
// doivent survivre, et la geometrie avec.
TEST(TEST_cgmesh_io, obj_survives_a_malformed_mtl)
{
    const std::string stem = "./tu_obj_badmtl";
    writeObjWithMtl(stem,
        "newmtl probe\n"
        "Kd 0.25 0.50 0.75\n"
        "Kd_incomplete 1 2\n"          // directive inconnue
        "Ka\n"                          // champ sans valeur
        "Ks abc def ghi\n"              // valeurs non numeriques
        "@!#$\n"                        // ligne franchement invalide
        "Ns 48\n");                     // valide, APRES les lignes cassees

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    EXPECT_EQ(mesh.GetNVertices(), 3u) << "geometrie perdue";
    ASSERT_EQ(mesh.GetNMaterials(), 1u) << "le materiau valide a disparu";

    MaterialColorExt* m = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
    ASSERT_NE(m, nullptr);
    float d[4];
    m->GetDiffuse(d);
    EXPECT_NEAR(d[0], 0.25f, 1e-5f) << "le Kd valide, AVANT les lignes cassees";
    EXPECT_NEAR(d[1], 0.50f, 1e-5f);
    EXPECT_NEAR(d[2], 0.75f, 1e-5f);
    // Le parseur ne doit pas s'arreter a la premiere ligne cassee.
    EXPECT_NEAR(m->m_fShininess[0], 48.0f / 128.0f, 1e-5f)
        << "le Ns valide APRES les lignes cassees n'a pas ete lu : le parseur s'est arrete";

    removeObjWithMtl(stem);
}

// ===========================================================================
//  Import 3DS : quelle map devient la texture
// ===========================================================================
// Bar_chair_2.3ds sort d'un exportateur qui empile DEUX blocs MAT_MAPNAME dans
// un meme MAT_TEXMAP -- la diffuse, puis la normale (materiau 0) ou la reflexion
// (materiau 1) :
//
//   MAT_ENTRY "BLACKChair_C"        MAT_ENTRY "silver refle"
//     MAT_BUMPMAP -> Bar_Chair_N.     MAT_BUMPMAP -> Bar_Chair_N.
//     MAT_TEXMAP  -> Bar_Chair_C.     MAT_TEXMAP  -> Bar_Chair_C.
//                 -> Bar_Chair_N.                 -> Ref.jpg
//
// Le case CHK3DS_A_MAT_MAPNAME ecrasait strFile a chaque passage, donc le DERNIER
// nom gagnait : le fauteuil se texturait avec sa carte de normales (materiau 0) et
// avec une vignette de reflexion de 32 Ko (materiau 1) au lieu de sa diffuse de
// 401 Ko. MAT_BUMPMAP, lui, n'est pas lu du tout (son case est commente, le chunk
// tombe dans le default), donc les deux noms viennent bien du MAT_TEXMAP.
//
// Corrige : on garde le PREMIER nom, qui est la map principale.
TEST(TEST_cgmesh_io, 3ds_texmap_keeps_the_first_map_name)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/Bar Chair/Bar_chair_2.3ds"));
    ASSERT_EQ(vm.GetNMeshes(), 1u);

    Mesh* m = vm.GetMeshes()[0];
    EXPECT_EQ(m->m_name, "Bar_Chair");
    EXPECT_EQ(m->GetNVertices(), 9999u);
    EXPECT_EQ(m->GetNFaces(),    18112u);

    // TRI_MAPPINGCOORDS : une UV par sommet. Note : m_nTextureCoordinates reste a
    // 0 sur ce chemin, seul le vecteur est renseigne (import_3ds ne met pas le
    // compteur a jour).
    EXPECT_EQ(m->m_pTextureCoordinates.size(), 2u * 9999u) << "coordonnees de texture perdues";

    ASSERT_EQ(m->GetNMaterials(), 2u);
    EXPECT_EQ(m->GetMaterial(0)->GetName(), "BLACKChair_C");
    EXPECT_EQ(m->GetMaterial(1)->GetName(), "silver refle");

    // Le coeur du test : les DEUX materiaux doivent porter la diffuse, premier
    // MAT_MAPNAME de leur MAT_TEXMAP -- et non Bar_Chair_N. / Ref.jpg.
    for (unsigned int k = 0; k < 2; k++)
    {
        SCOPED_TRACE(::testing::Message() << "materiau " << k);
        MaterialTexture* tex = dynamic_cast<MaterialTexture*>(m->GetMaterial(k));
        ASSERT_NE(tex, nullptr) << "un MAT_TEXMAP doit donner un MaterialTexture";
        // Le nom stocke dans le fichier est "Bar_Chair_C." (tronque a 12
        // caracteres) ; MaterialTexture rapporte le nom COMPLETE, celui qui a
        // effectivement charge. Ce qui compte ici est la racine : C (diffuse) et
        // non N (normales) ni Ref (reflexion).
        EXPECT_EQ(tex->GetFilename(), "Bar_Chair_C.jpg")
            << "le DERNIER MAT_MAPNAME a gagne : la correction du premier-gagnant a saute";
    }
}

// Le nom de map que porte ce fichier est TRONQUE a la largeur du champ 8.3 d'un
// MAT_MAPNAME : "Bar_Chair_C.", point final, extension absente. Sans rattrapage,
// std::filesystem::path::extension() rend une chaine vide, aucune branche de
// format ne s'active dans ImgIO::load, et la texture est perdue alors que
// Bar_Chair_C.jpg est dans le meme repertoire que le .3ds.
//
// CompleteTruncated3dsMapName reconnait la troncature a la longueur du nom (12
// caracteres = 8 + 1 + 3, le champ rempli jusqu'au bord) et reconstruit
// l'extension .jpg.
TEST(TEST_cgmesh_io, 3ds_truncated_map_name_is_recovered_with_a_jpg_extension)
{
    // Le fichier reel, celui que le rattrapage doit retrouver.
    ASSERT_TRUE(std::filesystem::exists("./test/data/Bar Chair/Bar_Chair_C.jpg"));

    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/Bar Chair/Bar_chair_2.3ds"));
    ASSERT_EQ(vm.GetNMeshes(), 1u);
    Mesh* m = vm.GetMeshes()[0];
    ASSERT_EQ(m->GetNMaterials(), 2u);

    // Les deux materiaux pointent la meme diffuse : les deux doivent la decoder.
    for (unsigned int k = 0; k < 2; k++)
    {
        SCOPED_TRACE(::testing::Message() << "materiau " << k);
        MaterialTexture* tex = dynamic_cast<MaterialTexture*>(m->GetMaterial(k));
        ASSERT_NE(tex, nullptr);
        EXPECT_EQ(tex->GetFilename(), "Bar_Chair_C.jpg")
            << "le nom n'a pas ete complete";
        Img* img = tex->GetImage();
        ASSERT_NE(img, nullptr)
            << "image non decodee : le rattrapage de troncature a saute, ou le "
               "support JPEG de cgimg est absent";
        EXPECT_GT(img->width(), 0u);
        EXPECT_GT(img->height(), 0u);
    }
}

// Les QUATRE formes de coupure a 12 caracteres, chacune completee par ce qui
// manque de ".jpg" :
//
//     "abcdefghijkl"  -> + ".jpg"       "abcdefghijk."  -> + "jpg"
//     "abcdefghij.j"  -> + "pg"         "abcdefghi.jp"  -> + "g"
//
// Le test a besoin d'un JPEG a chaque nom cible pour que le chargement reussisse,
// et cgimg n'a pas d'encodeur JPEG (stb ne fournit que le decodeur). On recopie
// donc Ref.jpg -- le plus petit du jeu de donnees, 32 Ko -- sous les noms voulus,
// dans le repertoire de travail du test. Aucun actif ajoute au depot.
TEST(TEST_cgmesh_io, truncation_recovery_completes_every_cut_position)
{
    const std::filesystem::path src = "./test/data/Bar Chair/Ref.jpg";
    ASSERT_TRUE(std::filesystem::exists(src));

    struct Case { const char* truncated; const char* completed; const char* what; };
    const Case cases[] = {
        { "abcdefghijkl", "abcdefghijkl.jpg", "coupure avant le point" },
        { "abcdefghijk.", "abcdefghijk.jpg",  "coupure juste apres le point" },
        { "abcdefghij.j", "abcdefghij.jpg",   "un caractere d'extension conserve" },
        { "abcdefghi.jp", "abcdefghi.jpg",    "deux caracteres d'extension conserves" },
    };

    for (const Case& c : cases)
    {
        SCOPED_TRACE(::testing::Message() << c.what << " : \"" << c.truncated << "\"");
        ASSERT_EQ(std::string(c.truncated).size(), 12u) << "le cas doit tenir la largeur du champ";

        std::error_code ec;
        std::filesystem::copy_file(src, c.completed,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << "copie impossible : " << ec.message();

        MaterialTexture tex(c.truncated, nullptr);
        EXPECT_EQ(tex.GetFilename(), c.completed) << "nom non complete";
        Img* img = tex.GetImage();
        EXPECT_NE(img, nullptr) << "image non decodee apres completion";

        std::filesystem::remove(c.completed);
    }
}

// Contre-epreuve : le rattrapage ne doit pas se declencher a tort.
TEST(TEST_cgmesh_io, truncation_recovery_leaves_other_names_alone)
{
    struct Case { const char* name; const char* why; };
    const Case cases[] = {
        { "Bar_Chai.jpg", "12 caracteres mais extension DEJA complete" },
        { "Ref.",         "hors de la largeur du champ (4 caracteres)" },
        { "abcdefghij.x", "12 caracteres, mais \".x\" n'est pas un prefixe de \"jpg\"" },
        { "abcdefghi.xy", "12 caracteres, mais \".xy\" n'est pas un prefixe de \"jpg\"" },
    };

    for (const Case& c : cases)
    {
        SCOPED_TRACE(::testing::Message() << c.why);
        MaterialTexture tex(c.name, "./test/data/Bar Chair");
        EXPECT_EQ(tex.GetFilename(), c.name) << "nom reecrit alors qu'il ne fallait pas";
        EXPECT_EQ(tex.GetImage(), nullptr) << "aucun de ces fichiers n'existe";
    }
}

// Un materiau 3DS TEXTURE : la map est la couleur diffuse, et un fichier sans
// MAT_SHININESS n'a pas de reflet speculaire.
//
// Avant ce correctif, l'import reportait le Kd/Ka du fichier dans GL_DIFFUSE et
// GL_AMBIENT. L'environnement de texture etant GL_MODULATE, le texel etait
// MULTIPLIE par ce Kd : le materiau « BLACKChair_C » de Bar_chair_2.3ds porte
// Kd = 5/255 = 0.0196 et texture le dessous en bois de l'assise, qui sortait donc
// a 2 % de son intensite -- noir a l'ecran.
//
// Et comme le fichier n'a aucun chunk MAT_SHININESS, Power restait a 0 tandis que
// Ks valait (255,255,255) : pow(N.H, 0) == 1 rendait le speculaire BLANC ENTIER
// partout ou N.H > 0, module par le texel (mode GL_SINGLE_COLOR par defaut), et
// absent des que N.H <= 0. C'est ce speculaire qui rendait la texture visible sous
// certains angles, et sa coupure qui la faisait disparaitre sous les autres.
//
// Les deux vont de pair : couper le speculaire SEUL aurait rendu le bois noir a
// tous les angles, faute de diffus pour le remplacer.
TEST(TEST_cgmesh_io, 3ds_textured_material_uses_the_map_as_diffuse_and_kills_a_zero_exponent_specular)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/Bar Chair/Bar_chair_2.3ds"));
    ASSERT_EQ(vm.GetNMeshes(), 1u);
    Mesh* m = vm.GetMeshes()[0];
    ASSERT_EQ(m->GetNMaterials(), 2u);

    for (unsigned int k = 0; k < 2; k++)
    {
        SCOPED_TRACE(::testing::Message() << "materiau " << k << " '"
                     << m->GetMaterial(k)->GetName() << "'");
        MaterialTexture* tex = dynamic_cast<MaterialTexture*>(m->GetMaterial(k));
        ASSERT_NE(tex, nullptr);

        // La map porte la couleur : diffus et ambiant neutres.
        const float* d = tex->GetDiffuse();
        const float* a = tex->GetAmbient();
        for (int c = 0; c < 3; c++)
        {
            EXPECT_NEAR(d[c], 1.f, 1e-5f)
                << "canal " << c << " : le Kd du fichier module le texel et l'eteint";
            EXPECT_NEAR(a[c], 1.f, 1e-5f) << "canal " << c;
        }

        // Aucun MAT_SHININESS dans ce fichier -> pas de reflet.
        const float* s = tex->GetSpecular();
        for (int c = 0; c < 3; c++)
            EXPECT_NEAR(s[c], 0.f, 1e-5f)
                << "canal " << c << " : speculaire blanc a exposant nul, il sature "
                   "partout ou N.H > 0 et disparait ailleurs";
        EXPECT_NEAR(tex->GetShininess(), 0.f, 1e-5f);
    }
}

// Le fichier porte bien ses affectations de materiau, dans des MSH_MAT_GROUP
// niches sous TRI_FACEL1 : 1664 faces pour BLACKChair_C, 16448 pour silver refle.
// C'est BLACKChair_C qui porte le disque de bois de l'atlas -- donc le materiau
// dont le Kd quasi noir eteignait la texture.
TEST(TEST_cgmesh_io, 3ds_material_groups_are_assigned_per_face)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/Bar Chair/Bar_chair_2.3ds"));
    Mesh* m = vm.GetMeshes()[0];
    ASSERT_EQ(m->GetNFaces(), 18112u);

    unsigned int counts[2] = { 0, 0 }, none = 0;
    for (unsigned int f = 0; f < m->GetNFaces(); f++)
    {
        const unsigned int id = m->m_pFaces[f]->GetMaterialId();
        if (id == MATERIAL_NONE) none++;
        else { ASSERT_LT(id, 2u); counts[id]++; }
    }
    EXPECT_EQ(none, 0u) << "des faces sans materiau alors que le fichier les affecte toutes";
    EXPECT_EQ(counts[0], 1664u)  << "BLACKChair_C";
    EXPECT_EQ(counts[1], 16448u) << "silver refle";
}

// ===========================================================================
//  Cartes de reflexion
// ===========================================================================
// Un materiau peut declarer une carte de reflexion en plus de sa diffuse : par
// `MAT_REFLMAP` en 3DS, par `refl` en MTL. Elle est appliquee en sphere mapping,
// donc sans UV.
//
// Bar_chair_2.mtl la declare explicitement -- `refl Ref.jpg` sur
// silver_reflection_2 -- alors que Bar_chair_2.3ds ne le fait PAS : il empile
// Ref.jpg comme second MAT_MAPNAME d'un MAT_TEXMAP, un emplacement dont la
// semantique n'est pas declaree (le meme emplacement porte une carte de NORMALES
// pour l'autre materiau). Le chemin OBJ est donc le seul qui puisse la charger
// depuis cet actif.
TEST(TEST_cgmesh_io, mtl_refl_loads_a_reflection_map)
{
    Mesh mesh;
    ASSERT_EQ(mesh.load("./test/data/Bar Chair/Bar_chair_2.obj"), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 2u);

    // BLACK : map_Kd + map_Bump, PAS de refl.
    MaterialTexture* black = dynamic_cast<MaterialTexture*>(mesh.GetMaterial(0));
    ASSERT_NE(black, nullptr);
    EXPECT_EQ(black->GetName(), "BLACK");
    EXPECT_FALSE(black->HasReflectionMap()) << "BLACK ne declare aucun refl";
    EXPECT_TRUE(black->GetReflectionFilename().empty());

    // silver_reflection_2 : refl Ref.jpg.
    MaterialTexture* silver = dynamic_cast<MaterialTexture*>(mesh.GetMaterial(1));
    ASSERT_NE(silver, nullptr);
    EXPECT_EQ(silver->GetName(), "silver_reflection_2");
    EXPECT_EQ(silver->GetReflectionFilename(), "Ref.jpg");
    ASSERT_TRUE(silver->HasReflectionMap()) << "Ref.jpg n'a pas ete decodee";

    Img* refl = silver->GetReflectionImage();
    ASSERT_NE(refl, nullptr);
    EXPECT_GT(refl->width(),  0u);
    EXPECT_GT(refl->height(), 0u);

    // La diffuse reste la diffuse : la reflexion ne l'ecrase pas.
    EXPECT_EQ(silver->GetFilename(), "Bar_Chair_C.jpg");
    EXPECT_NE(silver->GetImage(), nullptr);
    EXPECT_NE(silver->GetImage(), refl) << "diffuse et reflexion partagent la meme image";
}

// La reflexion appartient au materiau qui la declare, pas au suivant : `newmtl`
// doit oublier la texture precedente. Sans cette remise a zero, un `refl` place
// dans un materiau sans map_Kd se collerait a la texture du materiau d'avant.
TEST(TEST_cgmesh_io, mtl_refl_does_not_leak_to_the_next_material)
{
    const std::string stem = "./tu_obj_refl_leak";
    writeObjWithMtl(stem,
        "newmtl textured\n"
        "Kd 1 1 1\n"
        "map_Kd Bar_Chair_C.jpg\n"      // volontairement introuvable ici
        "newmtl plain\n"
        "Kd 0 1 0\n"
        "refl Ref.jpg\n",               // refl SANS map_Kd : ne doit rien accrocher
        "plain");

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 2u);

    MaterialTexture* first = dynamic_cast<MaterialTexture*>(mesh.GetMaterial(0));
    ASSERT_NE(first, nullptr) << "le premier materiau porte un map_Kd";
    EXPECT_FALSE(first->HasReflectionMap())
        << "le refl du SECOND materiau s'est accroche au premier";

    // Le second n'a pas de map_Kd, donc reste un materiau de couleur : le refl
    // n'a rien pour le porter et est ignore, sans faire echouer le chargement.
    EXPECT_EQ(dynamic_cast<MaterialTexture*>(mesh.GetMaterial(1)), nullptr);

    removeObjWithMtl(stem);
}

// Le .3ds du fauteuil ne declare AUCUN MAT_REFLMAP : son Ref.jpg est le second
// MAT_MAPNAME d'un MAT_TEXMAP. Ce test fige ce constat, pour qu'on ne croie pas
// le support des reflexions cassé en le chargeant.
TEST(TEST_cgmesh_io, 3ds_chair_declares_no_reflection_map)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/Bar Chair/Bar_chair_2.3ds"));
    Mesh* m = vm.GetMeshes()[0];
    ASSERT_EQ(m->GetNMaterials(), 2u);
    for (unsigned int k = 0; k < 2; k++)
    {
        MaterialTexture* tex = dynamic_cast<MaterialTexture*>(m->GetMaterial(k));
        ASSERT_NE(tex, nullptr);
        EXPECT_FALSE(tex->HasReflectionMap())
            << "materiau " << k << " : une reflexion apparait, alors que le fichier "
               "n'a pas de chunk MAT_REFLMAP -- le second MAT_MAPNAME du MAT_TEXMAP "
               "serait-il pris pour une reflexion ?";
    }
}

// Un `map_Kd` neutralise les couleurs du MTL, comme sur le chemin 3DS : la map EST
// la couleur diffuse. Sans cela le Kd du MTL multipliait le texel (GL_MODULATE) --
// « BLACK » porte Kd = 0.018262 et texture le dessous en bois de l'assise, qui
// sortait donc noir -- et `Ka 0.000000` retirait tout plancher ambiant.
TEST(TEST_cgmesh_io, mtl_map_kd_neutralises_the_material_colours)
{
    Mesh mesh;
    ASSERT_EQ(mesh.load("./test/data/Bar Chair/Bar_chair_2.obj"), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 2u);

    for (unsigned int k = 0; k < 2; k++)
    {
        SCOPED_TRACE(::testing::Message() << "materiau " << k << " '"
                     << mesh.GetMaterial(k)->GetName() << "'");
        MaterialTexture* t = dynamic_cast<MaterialTexture*>(mesh.GetMaterial(k));
        ASSERT_NE(t, nullptr);
        for (int c = 0; c < 3; c++)
        {
            EXPECT_NEAR(t->GetDiffuse()[c], 1.f, 1e-5f)
                << "canal " << c << " : le Kd du MTL eteint le texel";
            EXPECT_NEAR(t->GetAmbient()[c], 1.f, 1e-5f) << "canal " << c;
        }
    }
}

// L'exposant speculaire ne doit etre divise par 128 QU'UNE FOIS.
//
// import_mtl convertit deja le Ns du MTL en fraction de 128, et ActivateMaterial
// remultiplie par 128 a l'affichage. La branche map_Kd divisait une seconde fois :
// un Ns de 96 devenait un exposant de 0,75, or pow(N.H, 0.75) est quasi constant,
// donc tout le speculaire s'appliquait partout et les pieds chromes sortaient
// beaucoup plus brillants que le rendu de reference.
TEST(TEST_cgmesh_io, mtl_specular_exponent_is_not_divided_twice)
{
    Mesh mesh;
    ASSERT_EQ(mesh.load("./test/data/Bar Chair/Bar_chair_2.obj"), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 2u);

    // Valeurs Ns du .mtl, dans l'ordre de declaration.
    const float expectedNs[2] = { 17.647059f, 96.078431f };
    for (unsigned int k = 0; k < 2; k++)
    {
        SCOPED_TRACE(::testing::Message() << "materiau " << k);
        MaterialTexture* t = dynamic_cast<MaterialTexture*>(mesh.GetMaterial(k));
        ASSERT_NE(t, nullptr);
        // GetShininess() est la fraction ; ActivateMaterial pousse 128 * fraction
        // dans GL_SHININESS, ce qui doit redonner le Ns du fichier.
        EXPECT_NEAR(128.f * t->GetShininess(), expectedNs[k], 0.01f)
            << "exposant reel " << 128.f * t->GetShininess() << " au lieu de " << expectedNs[k];
    }
}

// Une directive de map non geree, et une ligne vide, doivent etre AVALEES : les
// anciens `strcmp(line, "map_Ka ")` comparaient la ligne entiere a un prefixe et ne
// pouvaient jamais correspondre, si bien que chaque `map_Bump` et chaque ligne vide
// -- il y en a une entre deux materiaux dans tout export Blender -- signalait une
// « MTL parse error ». Ce qui suit la directive doit continuer d'etre lu.
TEST(TEST_cgmesh_io, mtl_unhandled_map_directives_do_not_derail_the_parser)
{
    const std::string stem = "./tu_obj_mtlmaps";
    writeObjWithMtl(stem,
        "newmtl probe\n"
        "Kd 0.25 0.50 0.75\n"
        "\n"                            // ligne vide
        "map_Bump some_normal.jpg\n"     // non gere
        "map_Ka some_ambient.jpg\n"      // non gere
        "bump other.jpg\n"               // non gere
        "\n"
        "Ns 48\n");                      // valide, APRES les directives ignorees

    Mesh mesh;
    ASSERT_EQ(mesh.load((stem + ".obj").c_str()), 0);
    ASSERT_EQ(mesh.GetNMaterials(), 1u);

    // Pas de map_Kd : le materiau reste une couleur, et garde son Kd.
    MaterialColorExt* m = dynamic_cast<MaterialColorExt*>(mesh.GetMaterial(0));
    ASSERT_NE(m, nullptr);
    float d[4];
    m->GetDiffuse(d);
    EXPECT_NEAR(d[0], 0.25f, 1e-5f);
    EXPECT_NEAR(d[2], 0.75f, 1e-5f);
    EXPECT_NEAR(m->m_fShininess[0], 48.f / 128.f, 1e-5f)
        << "le Ns place APRES les directives ignorees n'a pas ete lu";

    removeObjWithMtl(stem);
}
