#include <gtest/gtest.h>

#include "../src/cgmesh/vmodels.h"
#include "../src/cgmesh/mesh.h"

namespace {

// Cube unité (8 sommets, 12 faces) décalé de (ox,oy,oz).
Mesh* makeCube(float ox = 0.f, float oy = 0.f, float oz = 0.f)
{
    Mesh* m = new Mesh();
    m->Init(8, 12);
    const float h = 0.5f;
    const float V[8][3] = {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
                           {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}};
    for (int i = 0; i < 8; ++i) m->SetVertex(i, V[i][0]+ox, V[i][1]+oy, V[i][2]+oz);
    const int F[12][3] = {{0,1,2},{0,2,3},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
                          {3,2,6},{3,6,7},{0,3,7},{0,7,4},{1,2,6},{1,6,5}};
    for (int i = 0; i < 12; ++i) m->SetFace(i, F[i][0], F[i][1], F[i][2]);
    return m;
}

} // namespace

// Chaque Model regroupe ses maillages ; VModels agrège nom/compteurs.
TEST(TEST_cgmesh_vmodels, add_counts_names)
{
    VModels vm;
    Model* a = vm.Add("a.obj"); a->m_meshes.AddMesh(makeCube());
    Model* b = vm.Add("b.obj"); b->m_meshes.AddMesh(makeCube(10,0,0));
                                b->m_meshes.AddMesh(makeCube(11,0,0));

    EXPECT_EQ(vm.GetNModels(), 2u);
    EXPECT_EQ(vm.GetModel(0)->m_name, "a.obj");
    EXPECT_EQ(vm.GetModel(1)->m_name, "b.obj");
    EXPECT_EQ(a->m_meshes.GetNMeshes(), 1u);
    EXPECT_EQ(b->m_meshes.GetNMeshes(), 2u);   // deux maillages dans le MÊME fichier
    EXPECT_EQ(vm.GetNFaces(), 36u);            // 3 cubes × 12
    EXPECT_EQ(vm.GetNVertices(), 24u);         // 3 cubes × 8
}

// Remove détruit le fichier (et ses maillages) et décale les suivants.
TEST(TEST_cgmesh_vmodels, remove)
{
    VModels vm;
    vm.Add("a")->m_meshes.AddMesh(makeCube());
    vm.Add("b")->m_meshes.AddMesh(makeCube());
    ASSERT_TRUE(vm.Remove(0));
    EXPECT_EQ(vm.GetNModels(), 1u);
    EXPECT_EQ(vm.GetModel(0)->m_name, "b");
    EXPECT_FALSE(vm.Remove(5));                // hors bornes
}

// La bbox agrégée n'inclut que les fichiers visibles quand visibleOnly=true.
TEST(TEST_cgmesh_vmodels, visibility_affects_bbox)
{
    VModels vm;
    vm.Add("near")->m_meshes.AddMesh(makeCube(0,0,0));
    Model* far = vm.Add("far"); far->m_meshes.AddMesh(makeCube(10,0,0));

    const float both = vm.AggregateBBox(true).GetLargestLength();
    far->m_visible = false;
    const float onlyNear = vm.AggregateBBox(true).GetLargestLength();

    EXPECT_GT(both, 9.f);       // deux cubes distants -> ~11
    EXPECT_LT(onlyNear, 2.f);   // un seul cube -> ~1
    EXPECT_GT(vm.AggregateBBox(false).GetLargestLength(), 9.f);  // visibleOnly=false reprend tout
}
