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

    m->SetNFaces (2);
    m->FaceAt (0)->SetTriangle (0, 1, 2);
    m->FaceAt (1)->SetTriangle (1, 3, 2);

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
    m->SetNFaces (0);

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
        m->SetVertexComponent (i, 0, 0.0f);
        m->SetVertexComponent (i, 1, 0.0f);
        m->SetVertexComponent (i, 2, 0.0f);
    }
    m->SetTextureCoordinates (std::vector<float>(8, 0.5f), 4); // (0.5, 0.5) partout

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 1u);
    EXPECT_EQ(m->GetTextureCoordinates ().size(), 2u);
    EXPECT_FLOAT_EQ(m->GetTextureCoordinates ()[0], 0.5f);
    EXPECT_FLOAT_EQ(m->GetTextureCoordinates ()[1], 0.5f);
    delete m;
}

TEST(TEST_cgmesh_merge, uv_seam_keeps_vertices_separate)
{
    // 4 colocated vertices, two distinct UVs → must NOT merge into 1.
    // The seam-aware welder should keep one vertex per (XYZ, UV) pair.
    auto* m = makeQuadOfTwoTriangles();
    for (unsigned int i = 0; i < 4; ++i)
    {
        m->SetVertexComponent (i, 0, 0.0f);
        m->SetVertexComponent (i, 1, 0.0f);
        m->SetVertexComponent (i, 2, 0.0f);
    }
    // Two pairs: verts 0/1 with UV (0,0); verts 2/3 with UV (1,1).
    m->SetTextureCoordinates (std::vector<float>{ 0.0f, 0.0f,
                                                 0.0f, 0.0f,
                                                 1.0f, 1.0f,
                                                 1.0f, 1.0f }, 4);

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    EXPECT_EQ(m->GetTextureCoordinates ().size(), 4u);

    // The two surviving UVs must be (0,0) and (1,1) — order isn't enforced.
    const bool firstIsZero  = (m->GetTextureCoordinates ()[0] == 0.0f && m->GetTextureCoordinates ()[1] == 0.0f);
    const bool secondIsOne  = (m->GetTextureCoordinates ()[2] == 1.0f && m->GetTextureCoordinates ()[3] == 1.0f);
    const bool firstIsOne   = (m->GetTextureCoordinates ()[0] == 1.0f && m->GetTextureCoordinates ()[1] == 1.0f);
    const bool secondIsZero = (m->GetTextureCoordinates ()[2] == 0.0f && m->GetTextureCoordinates ()[3] == 0.0f);
    EXPECT_TRUE((firstIsZero && secondIsOne) || (firstIsOne && secondIsZero));

    delete m;
}

TEST(TEST_cgmesh_merge, normals_are_not_a_merge_criterion)
{
    // 2 colocated vertices, normals 90° apart. Normals are NOT a merge
    // criterion (callers recompute them afterwards), so the vertices weld.
    // This is what lets a faceted STL — whose unmerged vertices each carry
    // their own facet normal — actually merge.
    auto* m = new Mesh();
    const float verts[] = { 0, 0, 0,   0, 0, 0 };
    m->SetVertices(2, const_cast<float*>(verts));
    m->SetNFaces (0);
    m->SetVertexNormals (std::vector<float>{ 1, 0, 0,   0, 1, 0 });

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 1u);              // welded despite differing normals
    EXPECT_EQ(m->GetVertexNormals ().size(), 3u);     // normals shrunk to the merged count
    delete m;
}

TEST(TEST_cgmesh_merge, color_difference_keeps_vertices_separate)
{
    auto* m = new Mesh();
    const float verts[] = { 0, 0, 0,   0, 0, 0 };
    m->SetVertices(2, const_cast<float*>(verts));
    m->SetNFaces (0);
    m->SetVertexColors (std::vector<float>{ 1, 0, 0,   0, 1, 0 });

    m->MergeVertices(1e-6f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    EXPECT_EQ(m->GetVertexColors ().size(), 6u);
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
    m->SetNFaces (1);
    m->FaceAt (0)->SetTriangle (0, 1, 2);

    m->MergeVertices(0.001f);
    EXPECT_EQ(m->GetNVertices(), 2u);
    // Face[0] becomes degenerate (two vertices collapsed to same index) and
    // is dropped — degenerate triangles are filtered by MergeVertices.
    EXPECT_EQ(m->GetNFaces(), 0u);
    delete m;
}
