#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

namespace {

Mesh* makeQuadOfTwoTriangles()
{
    // Two triangles sharing edge 1-2; 4 unique XYZ positions.
    auto* m = new Mesh();
    const float verts[] = {
        0, 0, 0,
        1, 0, 0,
        0, 1, 0,
        1, 1, 0,
    };
    m->SetVertices(4, const_cast<float*>(verts));

    m->m_nFaces = 2;
    m->m_pFaces = new Face*[2];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[1] = new Face();
    m->m_pFaces[0]->SetNVertices(3);
    m->m_pFaces[0]->SetVertex(0, 0); m->m_pFaces[0]->SetVertex(1, 1); m->m_pFaces[0]->SetVertex(2, 2);
    m->m_pFaces[1]->SetNVertices(3);
    m->m_pFaces[1]->SetVertex(0, 1); m->m_pFaces[1]->SetVertex(1, 3); m->m_pFaces[1]->SetVertex(2, 2);

    return m;
}

} // namespace

TEST(TEST_cgmesh_merge, exact_duplicates_collapse_when_no_attributes)
{
    // 4 vertices, all colocated. With no UV/normal/color attributes the
    // welding must collapse them to 1.
    auto* m = new Mesh();
    const float verts[] = {
        1, 2, 3,   1, 2, 3,   1, 2, 3,   1, 2, 3,
    };
    m->SetVertices(4, const_cast<float*>(verts));
    m->m_nFaces = 0;
    m->m_pFaces = nullptr;

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 1u);
    delete m;
}

TEST(TEST_cgmesh_merge, duplicates_with_same_uv_collapse)
{
    // 4 colocated vertices, all with the SAME UV → merge to 1.
    auto* m = makeQuadOfTwoTriangles();
    // Force every vertex to the same XYZ + same UV.
    for (unsigned int i = 0; i < 4; ++i)
    {
        m->m_pVertices[3*i + 0] = 0.0f;
        m->m_pVertices[3*i + 1] = 0.0f;
        m->m_pVertices[3*i + 2] = 0.0f;
    }
    m->m_pTextureCoordinates.assign(8, 0.5f); // (0.5, 0.5) for all 4 verts

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 1u);
    EXPECT_EQ(m->m_pTextureCoordinates.size(), 2u);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[0], 0.5f);
    EXPECT_FLOAT_EQ(m->m_pTextureCoordinates[1], 0.5f);
    delete m;
}

TEST(TEST_cgmesh_merge, uv_seam_keeps_vertices_separate)
{
    // 4 colocated vertices, two distinct UVs → must NOT merge into 1.
    // The seam-aware welder should keep one vertex per (XYZ, UV) pair.
    auto* m = makeQuadOfTwoTriangles();
    for (unsigned int i = 0; i < 4; ++i)
    {
        m->m_pVertices[3*i + 0] = 0.0f;
        m->m_pVertices[3*i + 1] = 0.0f;
        m->m_pVertices[3*i + 2] = 0.0f;
    }
    // Two pairs: verts 0/1 with UV (0,0); verts 2/3 with UV (1,1).
    m->m_pTextureCoordinates.resize(8);
    m->m_pTextureCoordinates[0] = 0.0f; m->m_pTextureCoordinates[1] = 0.0f;
    m->m_pTextureCoordinates[2] = 0.0f; m->m_pTextureCoordinates[3] = 0.0f;
    m->m_pTextureCoordinates[4] = 1.0f; m->m_pTextureCoordinates[5] = 1.0f;
    m->m_pTextureCoordinates[6] = 1.0f; m->m_pTextureCoordinates[7] = 1.0f;

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    EXPECT_EQ(m->m_pTextureCoordinates.size(), 4u);

    // The two surviving UVs must be (0,0) and (1,1) — order isn't enforced.
    const bool firstIsZero  = (m->m_pTextureCoordinates[0] == 0.0f && m->m_pTextureCoordinates[1] == 0.0f);
    const bool secondIsOne  = (m->m_pTextureCoordinates[2] == 1.0f && m->m_pTextureCoordinates[3] == 1.0f);
    const bool firstIsOne   = (m->m_pTextureCoordinates[0] == 1.0f && m->m_pTextureCoordinates[1] == 1.0f);
    const bool secondIsZero = (m->m_pTextureCoordinates[2] == 0.0f && m->m_pTextureCoordinates[3] == 0.0f);
    EXPECT_TRUE((firstIsZero && secondIsOne) || (firstIsOne && secondIsZero));

    delete m;
}

TEST(TEST_cgmesh_merge, normal_crease_keeps_vertices_separate)
{
    // 2 colocated vertices, normals 90° apart → must NOT merge.
    auto* m = new Mesh();
    const float verts[] = { 0, 0, 0,   0, 0, 0 };
    m->SetVertices(2, const_cast<float*>(verts));
    m->m_nFaces = 0;
    m->m_pFaces = nullptr;
    m->m_pVertexNormals = { 1, 0, 0,   0, 1, 0 };

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    EXPECT_EQ(m->m_pVertexNormals.size(), 6u);
    delete m;
}

TEST(TEST_cgmesh_merge, color_difference_keeps_vertices_separate)
{
    auto* m = new Mesh();
    const float verts[] = { 0, 0, 0,   0, 0, 0 };
    m->SetVertices(2, const_cast<float*>(verts));
    m->m_nFaces = 0;
    m->m_pFaces = nullptr;
    m->m_pVertexColors = { 1, 0, 0,   0, 1, 0 };

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    EXPECT_EQ(m->m_pVertexColors.size(), 6u);
    delete m;
}

TEST(TEST_cgmesh_merge, face_indices_get_remapped)
{
    // Distinct positions; merging at high tolerance collapses 0 with 1 but
    // not 2. Face must reference the remapped indices.
    auto* m = new Mesh();
    const float verts[] = {
        0, 0, 0,
        0.0001f, 0, 0,   // very close to vertex 0
        5, 5, 5,
    };
    m->SetVertices(3, const_cast<float*>(verts));
    m->m_nFaces = 1;
    m->m_pFaces = new Face*[1];
    m->m_pFaces[0] = new Face();
    m->m_pFaces[0]->SetNVertices(3);
    m->m_pFaces[0]->SetVertex(0, 0);
    m->m_pFaces[0]->SetVertex(1, 1);
    m->m_pFaces[0]->SetVertex(2, 2);

    m->MergeVertices(0.001f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    // Face[0] becomes degenerate (two vertices collapsed to same index) and
    // is dropped — degenerate triangles are filtered by MergeVertices.
    EXPECT_EQ(m->GetNFaces(), 0u);
    delete m;
}
