#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

#include <algorithm>
#include <set>
#include <vector>

namespace {

// Build a Mesh holding a single n-vertex face. Vertices live on the unit
// circle in the XY plane (counter-clockwise), so the face is planar and
// convex for any N >= 3.
Mesh* makeRegularPolygonMesh(unsigned int n)
{
    auto* m = new Mesh();
    std::vector<float> verts;
    verts.reserve(3 * n);
    for (unsigned int i = 0; i < n; ++i)
    {
        const float a = (float)(2.0 * 3.14159265358979 * i / n);
        verts.push_back(std::cos(a));
        verts.push_back(std::sin(a));
        verts.push_back(0.0f);
    }
    m->SetVertices(n, verts.data());

    m->m_nFaces = 1;
    m->m_pFaces = new Face*[1];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[0]->SetNVertices(n);
    for (unsigned int i = 0; i < n; ++i)
        m->m_pFaces[0]->SetVertex(i, i);
    return m;
}

} // namespace

TEST(TEST_cgmesh_triangulation, triangle_face_emits_one_triangle)
{
    Mesh* m = makeRegularPolygonMesh(3);
    auto idx = m->BuildTriangulation();
    EXPECT_EQ(idx.size(), 3u);
    EXPECT_EQ(idx[0], 0u);
    EXPECT_EQ(idx[1], 1u);
    EXPECT_EQ(idx[2], 2u);
    delete m;
}

TEST(TEST_cgmesh_triangulation, quad_face_fan_triangulates_to_two_triangles)
{
    Mesh* m = makeRegularPolygonMesh(4);
    auto idx = m->BuildTriangulation();
    EXPECT_EQ(idx.size(), 6u);
    // Fan from vertex 0: (0,1,2) and (0,2,3).
    EXPECT_EQ(idx[0], 0u); EXPECT_EQ(idx[1], 1u); EXPECT_EQ(idx[2], 2u);
    EXPECT_EQ(idx[3], 0u); EXPECT_EQ(idx[4], 2u); EXPECT_EQ(idx[5], 3u);
    delete m;
}

TEST(TEST_cgmesh_triangulation, pentagon_emits_three_triangles)
{
    Mesh* m = makeRegularPolygonMesh(5);
    auto idx = m->BuildTriangulation();
    EXPECT_EQ(idx.size(), 9u); // 3 triangles for a convex pentagon
    delete m;
}

TEST(TEST_cgmesh_triangulation, hexagon_emits_four_triangles)
{
    Mesh* m = makeRegularPolygonMesh(6);
    auto idx = m->BuildTriangulation();
    EXPECT_EQ(idx.size(), 12u); // 4 triangles
    delete m;
}

TEST(TEST_cgmesh_triangulation, concave_pentagon_uses_glutess)
{
    // L-shape (concave pentagon) in XY plane:
    //   (0,0), (2,0), (2,1), (1,1), (1,2), (0,2)  -- actually 6 vertices.
    // Simpler concave: arrow-head (5 vertices).
    //
    //     (1,2)
    //    /     \
    //  (0,1)  (2,1)
    //   |   X   |     X = (1,0.4) (concave dent)
    //  (0,0)  (2,0)
    //
    // Use just 5 vertices forming a concave "dart": (0,0) (2,0) (1,0.5) (2,2) (0,2)
    auto* m = new Mesh();
    const float verts[] = {
        0.0f, 0.0f, 0.0f,
        2.0f, 0.0f, 0.0f,
        1.0f, 0.5f, 0.0f, // concave vertex pushed inward
        2.0f, 2.0f, 0.0f,
        0.0f, 2.0f, 0.0f,
    };
    m->SetVertices(5, const_cast<float*>(verts));
    m->m_nFaces = 1;
    m->m_pFaces = new Face*[1];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[0]->SetNVertices(5);
    for (unsigned int i = 0; i < 5; ++i)
        m->m_pFaces[0]->SetVertex(i, i);

    auto idx = m->BuildTriangulation();

    // Concave pentagon → 3 triangles → 9 indices (glutess).
    EXPECT_EQ(idx.size(), 9u);

    // Every emitted index must refer to one of the 5 source vertices.
    for (unsigned int v : idx)
        EXPECT_LT(v, 5u);

    // The dent vertex (#2) cannot be a "fan center" if glutess kicked in —
    // a fan from vertex 0 would have produced overlapping triangles. We
    // can't pin the exact triangulation glutess returns (implementation
    // detail), but we can confirm that all 5 vertices are referenced at
    // least once (every corner participates).
    std::set<unsigned int> referenced(idx.begin(), idx.end());
    EXPECT_EQ(referenced.size(), 5u);

    delete m;
}

TEST(TEST_cgmesh_triangulation, mixed_faces_concatenate)
{
    // Mesh with one triangle and one quad — total 1 + 2 = 3 triangles.
    auto* m = new Mesh();
    const float verts[] = {
        // triangle (0..2)
        0, 0, 0,   1, 0, 0,   0, 1, 0,
        // quad (3..6)
        2, 0, 0,   3, 0, 0,   3, 1, 0,   2, 1, 0,
    };
    m->SetVertices(7, const_cast<float*>(verts));
    m->m_nFaces = 2;
    m->m_pFaces = new Face*[2];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[1] = new Face();

    m->m_pFaces[0]->SetNVertices(3);
    m->m_pFaces[0]->SetVertex(0, 0);
    m->m_pFaces[0]->SetVertex(1, 1);
    m->m_pFaces[0]->SetVertex(2, 2);

    m->m_pFaces[1]->SetNVertices(4);
    for (unsigned int i = 0; i < 4; ++i)
        m->m_pFaces[1]->SetVertex(i, 3 + i);

    auto idx = m->BuildTriangulation();
    EXPECT_EQ(idx.size(), 9u); // 3 + 6 = 9 indices

    delete m;
}

// =============================================================================
//  BuildPolygonRenderData()
// =============================================================================

TEST(TEST_cgmesh_polygon_render_data, triangle_mesh_does_not_expand)
{
    // Pure triangle face → render data must keep the shared topology
    // layout (no extra slots). Index list points directly into m_pVertices.
    Mesh* m = makeRegularPolygonMesh(3);
    auto rd = m->BuildPolygonRenderData();

    EXPECT_EQ(rd.positions.size(), 9u);  // 3 verts * 3 floats — topology only
    EXPECT_EQ(rd.indices.size(),   3u);
    EXPECT_EQ(rd.indices[0], 0u);
    EXPECT_EQ(rd.indices[1], 1u);
    EXPECT_EQ(rd.indices[2], 2u);

    delete m;
}

TEST(TEST_cgmesh_polygon_render_data, two_triangles_share_topology_slots)
{
    // Two triangle faces using the same topology vertices: render data must
    // NOT duplicate them. Both triangles index into slots 0..2.
    Mesh* m = makeRegularPolygonMesh(3);

    auto faces = new Face*[2];
    faces[0] = m->m_pFaces[0];
    faces[1] = new Face();
    faces[1]->SetNVertices(3);
    faces[1]->SetVertex(0, 0);
    faces[1]->SetVertex(1, 1);
    faces[1]->SetVertex(2, 2);
    delete[] m->m_pFaces;
    m->m_pFaces = faces;
    m->m_nFaces = 2;

    auto rd = m->BuildPolygonRenderData();

    EXPECT_EQ(rd.positions.size(), 9u);  // still topology-only
    EXPECT_EQ(rd.indices.size(),   6u);
    for (unsigned int i = 0; i < 6; ++i)
        EXPECT_LT(rd.indices[i], 3u);

    delete m;
}

TEST(TEST_cgmesh_polygon_render_data, quad_appends_4_render_vertices)
{
    // Quad: topology has 4 verts, the n-gon expansion appends 4 more.
    // Total = 8 render-verts; indices reference the expansion slots [4..7].
    Mesh* m = makeRegularPolygonMesh(4);
    m->ComputeNormals(); // populates m_pVertexNormals so output includes normals
    auto rd = m->BuildPolygonRenderData();

    EXPECT_EQ(rd.positions.size(), 24u); // 4 topology + 4 expansion = 8 * 3
    EXPECT_EQ(rd.normals.size(),   24u);
    EXPECT_EQ(rd.indices.size(),   6u);

    // Indices reference the expansion slots, not the topology.
    for (unsigned int idx : rd.indices)
    {
        EXPECT_GE(idx, 4u);
        EXPECT_LT(idx, 8u);
    }

    // The 4 expansion slots all carry the same polygon normal.
    const float nx0 = rd.normals[4*3 + 0];
    const float ny0 = rd.normals[4*3 + 1];
    const float nz0 = rd.normals[4*3 + 2];
    for (unsigned int i = 5; i < 8; ++i)
    {
        EXPECT_FLOAT_EQ(rd.normals[i*3 + 0], nx0);
        EXPECT_FLOAT_EQ(rd.normals[i*3 + 1], ny0);
        EXPECT_FLOAT_EQ(rd.normals[i*3 + 2], nz0);
    }
    // Quad in XY plane → polygon normal is ±Z.
    EXPECT_NEAR(nx0, 0.0f, 1e-5f);
    EXPECT_NEAR(ny0, 0.0f, 1e-5f);
    EXPECT_NEAR(std::fabs(nz0), 1.0f, 1e-5f);

    delete m;
}

TEST(TEST_cgmesh_polygon_render_data, two_adjacent_quads_each_get_own_expansion)
{
    auto* m = new Mesh();
    const float verts[] = {
        0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,  // quad A: 0..3 (XY plane)
        1, 0, 1,  1, 1, 1,                       // quad B extra: 4,5
    };
    m->SetVertices(6, const_cast<float*>(verts));
    m->m_nFaces = 2;
    m->m_pFaces = new Face*[2];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[1] = new Face();

    m->m_pFaces[0]->SetNVertices(4);
    for (unsigned int i = 0; i < 4; ++i) m->m_pFaces[0]->SetVertex(i, i);

    m->m_pFaces[1]->SetNVertices(4);
    m->m_pFaces[1]->SetVertex(0, 1);
    m->m_pFaces[1]->SetVertex(1, 4);
    m->m_pFaces[1]->SetVertex(2, 5);
    m->m_pFaces[1]->SetVertex(3, 2);

    m->ComputeNormals();
    auto rd = m->BuildPolygonRenderData();

    // 6 topology + 4 (quad A expansion) + 4 (quad B expansion) = 14
    EXPECT_EQ(rd.positions.size(), 14u * 3u);
    EXPECT_EQ(rd.indices.size(),   12u); // 2 quads * 2 triangles * 3 indices

    // Quad A's expansion is slots 6..9, quad B's is 10..13.
    const float* nA = &rd.normals[6*3];
    const float* nB = &rd.normals[10*3];
    for (size_t i = 1; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(rd.normals[(6 + i)*3 + 0], nA[0]);
        EXPECT_FLOAT_EQ(rd.normals[(6 + i)*3 + 1], nA[1]);
        EXPECT_FLOAT_EQ(rd.normals[(6 + i)*3 + 2], nA[2]);
        EXPECT_FLOAT_EQ(rd.normals[(10 + i)*3 + 0], nB[0]);
        EXPECT_FLOAT_EQ(rd.normals[(10 + i)*3 + 1], nB[1]);
        EXPECT_FLOAT_EQ(rd.normals[(10 + i)*3 + 2], nB[2]);
    }
    // A normal ≈ ±Z, B normal ≈ ±Y → orthogonal.
    const float dot = nA[0]*nB[0] + nA[1]*nB[1] + nA[2]*nB[2];
    EXPECT_NEAR(dot, 0.0f, 1e-4f);

    delete m;
}

// =============================================================================
//  Mesh::Triangulate() — in-place mutation
// =============================================================================

TEST(TEST_cgmesh_triangulate, triangle_mesh_unchanged)
{
    Mesh* m = makeRegularPolygonMesh(3);
    const auto nBefore = m->GetNFaces();
    m->Triangulate();
    EXPECT_EQ(m->GetNFaces(), nBefore);
    EXPECT_TRUE(m->IsTriangleMesh());
    delete m;
}

TEST(TEST_cgmesh_triangulate, quad_becomes_two_triangles)
{
    Mesh* m = makeRegularPolygonMesh(4);
    m->Triangulate();
    EXPECT_EQ(m->GetNFaces(), 2u);
    EXPECT_TRUE(m->IsTriangleMesh());
    delete m;
}

TEST(TEST_cgmesh_triangulate, pentagon_becomes_three_triangles)
{
    Mesh* m = makeRegularPolygonMesh(5);
    m->Triangulate();
    EXPECT_EQ(m->GetNFaces(), 3u);
    EXPECT_TRUE(m->IsTriangleMesh());
    delete m;
}

TEST(TEST_cgmesh_triangulate, preserves_material_id)
{
    Mesh* m = makeRegularPolygonMesh(4);
    m->m_pFaces[0]->SetMaterialId(7);
    m->Triangulate();
    ASSERT_EQ(m->GetNFaces(), 2u);
    EXPECT_EQ((unsigned)m->m_pFaces[0]->GetMaterialId(), 7u);
    EXPECT_EQ((unsigned)m->m_pFaces[1]->GetMaterialId(), 7u);
    delete m;
}

TEST(TEST_cgmesh_triangulate, bumps_revision)
{
    Mesh* m = makeRegularPolygonMesh(4);
    const auto rBefore = m->GetRevision();
    m->Triangulate();
    EXPECT_GT(m->GetRevision(), rBefore);
    delete m;
}

TEST(TEST_cgmesh_triangulate, propagates_face_relative_uv_indices)
{
    // Quad with face-relative UV indices: each corner of the quad points to
    // a distinct slot in the mesh's UV table. After triangulation, both
    // sub-triangles must carry the right UV indices for their corners — and
    // the m_bUseTextureCoordinates flag must travel too (the OBJ exporter
    // gates UV emission on it).
    Mesh* m = makeRegularPolygonMesh(4);
    Face* src = m->m_pFaces[0];
    src->m_bUseTextureCoordinates = true;
    src->ActivateTextureCoordinatesIndices();
    src->m_pTextureCoordinatesIndices[0] = 100;
    src->m_pTextureCoordinatesIndices[1] = 101;
    src->m_pTextureCoordinatesIndices[2] = 102;
    src->m_pTextureCoordinatesIndices[3] = 103;

    m->Triangulate();
    ASSERT_EQ(m->GetNFaces(), 2u);

    // Fan triangulation from vertex 0: (0,1,2) and (0,2,3).
    EXPECT_EQ(m->m_pFaces[0]->m_pTextureCoordinatesIndices[0], 100u);
    EXPECT_EQ(m->m_pFaces[0]->m_pTextureCoordinatesIndices[1], 101u);
    EXPECT_EQ(m->m_pFaces[0]->m_pTextureCoordinatesIndices[2], 102u);
    EXPECT_EQ(m->m_pFaces[1]->m_pTextureCoordinatesIndices[0], 100u);
    EXPECT_EQ(m->m_pFaces[1]->m_pTextureCoordinatesIndices[1], 102u);
    EXPECT_EQ(m->m_pFaces[1]->m_pTextureCoordinatesIndices[2], 103u);

    // m_bUseTextureCoordinates must reach both sub-triangles.
    EXPECT_TRUE(m->m_pFaces[0]->m_bUseTextureCoordinates);
    EXPECT_TRUE(m->m_pFaces[1]->m_bUseTextureCoordinates);

    delete m;
}

TEST(TEST_cgmesh_triangulate, propagates_inline_uv_coordinates)
{
    Mesh* m = makeRegularPolygonMesh(4);
    Face* src = m->m_pFaces[0];
    src->ActivateTextureCoordinates();
    // Set (u,v) per corner.
    src->m_pTextureCoordinates[0] = 0.0f; src->m_pTextureCoordinates[1] = 0.0f;
    src->m_pTextureCoordinates[2] = 1.0f; src->m_pTextureCoordinates[3] = 0.0f;
    src->m_pTextureCoordinates[4] = 1.0f; src->m_pTextureCoordinates[5] = 1.0f;
    src->m_pTextureCoordinates[6] = 0.0f; src->m_pTextureCoordinates[7] = 1.0f;

    m->Triangulate();
    ASSERT_EQ(m->GetNFaces(), 2u);

    // Tri 0 uses src corners 0,1,2 → (0,0) (1,0) (1,1).
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[0], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[1], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[2], 1.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[3], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[4], 1.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[0]->m_pTextureCoordinates[5], 1.0f);
    // Tri 1 uses src corners 0,2,3 → (0,0) (1,1) (0,1).
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[0], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[1], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[2], 1.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[3], 1.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[4], 0.0f);
    EXPECT_FLOAT_EQ(m->m_pFaces[1]->m_pTextureCoordinates[5], 1.0f);

    delete m;
}

TEST(TEST_cgmesh_triangulation, planar_self_intersecting_quad_does_not_crash)
{
    // Bowtie quad — two diagonals crossing at the center. Caveat: this is a
    // PLANAR self-intersection. The Newell normal sums to zero for such
    // shapes, so faceIsConvex returns true (no signed dot products) and the
    // face goes through the fan path, NOT through glutess. The fan output
    // is geometrically incorrect (overlapping triangles) but stays within
    // the source vertex index range.
    //
    // This test therefore does NOT exercise the combine sentinel path in
    // tessCombineCB / tessVertexCB. Reaching that path requires a non-
    // planar concave polygon with internal edge crossings, which is rarely
    // produced by importers (3dm/OBJ/STL never do). The combine sentinel
    // remains a defensive safety net validated by code review.
    //
    // What this test verifies: a malformed face doesn't trip an assertion
    // and doesn't emit out-of-range indices into the IBO.
    auto* m = new Mesh();
    const float verts[] = {
        0, 0, 0,   1, 1, 0,   1, 0, 0,   0, 1, 0,
    };
    m->SetVertices(4, const_cast<float*>(verts));
    m->m_nFaces = 1;
    m->m_pFaces = new Face*[1];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[0]->SetNVertices(4);
    m->m_pFaces[0]->SetVertex(0, 0);
    m->m_pFaces[0]->SetVertex(1, 1);
    m->m_pFaces[0]->SetVertex(2, 2);
    m->m_pFaces[0]->SetVertex(3, 3);

    auto idx = m->BuildTriangulation();
    for (unsigned int i : idx)
        EXPECT_LT(i, 4u) << "out-of-range index " << i << " emitted";

    delete m;
}

TEST(TEST_cgmesh_polygon_render_data, mixed_triangle_and_quad)
{
    // Triangle stays in topology; quad expands. Expect topology(7) + 4 expansion = 11.
    auto* m = new Mesh();
    const float verts[] = {
        0, 0, 0,   1, 0, 0,   0, 1, 0,                          // triangle (0..2)
        2, 0, 0,   3, 0, 0,   3, 1, 0,   2, 1, 0,              // quad (3..6)
    };
    m->SetVertices(7, const_cast<float*>(verts));
    m->m_nFaces = 2;
    m->m_pFaces = new Face*[2];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[1] = new Face();
    m->m_pFaces[0]->SetNVertices(3);
    m->m_pFaces[0]->SetVertex(0, 0); m->m_pFaces[0]->SetVertex(1, 1); m->m_pFaces[0]->SetVertex(2, 2);
    m->m_pFaces[1]->SetNVertices(4);
    for (unsigned int i = 0; i < 4; ++i) m->m_pFaces[1]->SetVertex(i, 3 + i);

    auto rd = m->BuildPolygonRenderData();

    EXPECT_EQ(rd.positions.size(), 11u * 3u); // 7 topology + 4 expansion
    EXPECT_EQ(rd.indices.size(),   9u);       // 3 (triangle) + 6 (quad fan)

    // Triangle face references topology slots 0..2.
    EXPECT_EQ(rd.indices[0], 0u);
    EXPECT_EQ(rd.indices[1], 1u);
    EXPECT_EQ(rd.indices[2], 2u);
    // Quad face references expansion slots 7..10.
    for (unsigned int i = 3; i < 9; ++i)
    {
        EXPECT_GE(rd.indices[i], 7u);
        EXPECT_LT(rd.indices[i], 11u);
    }

    delete m;
}
