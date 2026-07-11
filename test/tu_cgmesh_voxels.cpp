#include <gtest/gtest.h>
#include "../src/cgmesh/voxels.h"
#include "../src/cgmesh/cgmesh.h"             // Mesh (for ToMesh) + loadkvx (voxels_import_kvx.h)
#include "../src/cgmesh/voxels_import_kvx.h"

// KVX (Ken Silverman's Build-engine voxel format) import.
// test/data/kvx/duke.kvx is a 36x46x76 model holding 5458 surface voxels.
// loadkvx() parses the header, the per-column slab runs and the trailing
// 256-colour palette, activating each surface voxel and tagging it with its
// palette index (stored as the voxel "label").
TEST(TEST_cgmesh_voxels, import_kvx_duke)
{
    char path[] = "./test/data/kvx/duke.kvx";   // loadkvx takes a non-const char*
    Voxels* v = loadkvx(path);
    ASSERT_NE(v, nullptr);

    // dimensions from the KVX header
    EXPECT_EQ(v->get_nx(), 36u);
    EXPECT_EQ(v->get_ny(), 46u);
    EXPECT_EQ(v->get_nz(), 76u);

    // count activated (surface) voxels and check labels are palette bytes
    unsigned int activated = 0;
    bool labelsInByteRange = true;
    for (unsigned int x = 0; x < v->get_nx(); ++x)
        for (unsigned int y = 0; y < v->get_ny(); ++y)
            for (unsigned int z = 0; z < v->get_nz(); ++z)
                if (v->is_activated(x, y, z))
                {
                    ++activated;
                    if (v->get_label(x, y, z) > 255u) labelsInByteRange = false;
                }

    EXPECT_EQ(activated, 5458u);        // surface voxels stored in duke.kvx
    EXPECT_TRUE(labelsInByteRange);     // labels are 0..255 palette indices

    // the imported grid triangulates into a usable surface mesh
    // (regression guard: ToMesh() used to overflow on non-cubic grids)
    Mesh* m = v->ToMesh();
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(),    0u);

    // palette colours are applied: per-vertex colours are present and not all
    // left at the default grey (0.5, 0.5, 0.5)
    ASSERT_EQ(m->m_pVertexColors.size(), 3u * m->GetNVertices());
    bool anyColoured = false;
    for (size_t c = 0; c + 2 < m->m_pVertexColors.size(); c += 3)
        if (m->m_pVertexColors[c]   != 0.5f ||
            m->m_pVertexColors[c+1] != 0.5f ||
            m->m_pVertexColors[c+2] != 0.5f) { anyColoured = true; break; }
    EXPECT_TRUE(anyColoured) << "KVX palette colours were not applied to the mesh";

    delete m;
    delete v;
}

// A voxel-surface mesh must colour each FACE with its voxel's palette colour;
// colours must NOT bleed across a face. With welded grid vertices (each corner
// shared by up to 4 voxels of different colours) a face's 3 corners could carry
// 3 different colours -> the renderer interpolates -> visible colour "décalage".
// This pins the per-face-uniform colour contract.
TEST(TEST_cgmesh_voxels, kvx_colors_are_per_face)
{
    char path[] = "./test/data/kvx/laptop.kvx";
    Voxels* v = loadkvx(path);
    ASSERT_NE(v, nullptr);
    Mesh* m = v->ToMesh();
    ASSERT_NE(m, nullptr);
    ASSERT_EQ(m->m_pVertexColors.size(), 3u * m->GetNVertices());

    auto sameColor = [&](int p, int q){
        return m->m_pVertexColors[3*p]   == m->m_pVertexColors[3*q]   &&
               m->m_pVertexColors[3*p+1] == m->m_pVertexColors[3*q+1] &&
               m->m_pVertexColors[3*p+2] == m->m_pVertexColors[3*q+2];
    };
    unsigned int mixed = 0;
    for (unsigned int fidx = 0; fidx < m->GetNFaces(); ++fidx)
    {
        Face* f = m->m_pFaces[fidx];
        const int a = f->GetVertex(0), b = f->GetVertex(1), c = f->GetVertex(2);
        if (!sameColor(a, b) || !sameColor(a, c)) ++mixed;
    }
    printf("laptop: %u/%u faces have non-uniform vertex colours\n", mixed, m->GetNFaces());
    EXPECT_EQ(mixed, 0u) << "palette colours bleed across faces (vertex-weld artefact)";

    delete m;
    delete v;
}

// End-to-end: .kvx now imports through the standard VMeshes::load dispatch —
// the exact path the sinaia app uses (OnOpen -> LoadModel -> VMeshes::load).
TEST(TEST_cgmesh_voxels, import_kvx_via_vmeshes_load)
{
    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, "./test/data/kvx/duke.kvx"));
    ASSERT_EQ(vm.GetNMeshes(), 1u);

    Mesh* m = vm.GetMeshes()[0];
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(),    0u);
}

TEST(TEST_cgmesh_voxels, Initialization) {
    Voxels v(10, 20, 30);
    EXPECT_EQ(v.get_nx(), 10);
    EXPECT_EQ(v.get_ny(), 20);
    EXPECT_EQ(v.get_nz(), 30);

    for (unsigned int i = 0; i < 10; ++i) {
        for (unsigned int j = 0; j < 20; ++j) {
            for (unsigned int k = 0; k < 30; ++k) {
                EXPECT_FALSE(v.is_activated(i, j, k));
                EXPECT_EQ(v.get_data(i, j, k), 0.0f);
                EXPECT_EQ(v.get_label(i, j, k), 0);
            }
        }
    }
}

TEST(TEST_cgmesh_voxels, Activation) {
    Voxels v(5, 5, 5);
    v.activate(1, 2, 3);
    EXPECT_TRUE(v.is_activated(1, 2, 3));
    EXPECT_FALSE(v.is_activated(0, 0, 0));

    v.inverse_activation();
    EXPECT_FALSE(v.is_activated(1, 2, 3));
    EXPECT_TRUE(v.is_activated(0, 0, 0));
}

TEST(TEST_cgmesh_voxels, DataAndLabels) {
    Voxels v(5, 5, 5);
    v.set_data(1, 1, 1, 12.5f);
    EXPECT_EQ(v.get_data(1, 1, 1), 12.5f);

    v.set_label(2, 2, 2, 100);
    EXPECT_EQ(v.get_label(2, 2, 2), 100);

    v.reset_labels();
    EXPECT_EQ(v.get_label(2, 2, 2), 0);
}

TEST(TEST_cgmesh_voxels, Threshold) {
    Voxels v(5, 5, 5);
    v.activate(1, 1, 1);
    v.set_data(1, 1, 1, 5.0f);
    
    v.activate(2, 2, 2);
    v.set_data(2, 2, 2, 15.0f);

    v.threshold_data(10.0f);
    
    EXPECT_FALSE(v.is_activated(1, 1, 1));
    EXPECT_TRUE(v.is_activated(2, 2, 2));
}

TEST(TEST_cgmesh_voxels, ExtremalValues) {
    Voxels v(5, 5, 5);
    v.activate(1, 1, 1);
    v.set_data(1, 1, 1, 5.0f);
    v.activate(2, 2, 2);
    v.set_data(2, 2, 2, 15.0f);
    v.activate(3, 3, 3);
    v.set_data(3, 3, 3, -10.0f);

    float min, max;
    v.get_extremal_values(&min, &max);
    EXPECT_EQ(min, -10.0f);
    EXPECT_EQ(max, 15.0f);
}

TEST(TEST_cgmesh_voxels, Dilation) {
    Voxels v(10, 10, 10);
    v.activate(5, 5, 5);
    v.set_data(5, 5, 5, 1.0f);
    
    v.dilation();
    
    // Check 6-neighbors
    EXPECT_TRUE(v.is_activated(4, 5, 5));
    EXPECT_TRUE(v.is_activated(6, 5, 5));
    EXPECT_TRUE(v.is_activated(5, 4, 5));
    EXPECT_TRUE(v.is_activated(5, 6, 5));
    EXPECT_TRUE(v.is_activated(5, 5, 4));
    EXPECT_TRUE(v.is_activated(5, 5, 6));
    
    // Check non-neighbor
    EXPECT_FALSE(v.is_activated(0, 0, 0));
}
