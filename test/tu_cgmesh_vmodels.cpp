#include <gtest/gtest.h>

#include "../src/cgmesh/vmodels.h"
#include "../src/cgmesh/mesh.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>
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

// La bbox d'un modèle suit la géométrie : chaque maillage met sa boîte en cache
// contre sa révision (Mesh::CurrentBBox), et toute écriture des positions
// l'invalide.
TEST(TEST_cgmesh_vmodels, bbox_follows_geometry_edits)
{
    VModels vm;
    Model* m = vm.Add("cube");
    Mesh* cube = makeCube();
    m->m_meshes.AddMesh(cube);

    float mn[3], mx[3];
    m->ComputeBBox().GetMinMax(mn, mx);
    EXPECT_EQ(mx[0], 0.5f);

    cube->translate(5.f, 0.f, 0.f);
    m->ComputeBBox().GetMinMax(mn, mx);
    EXPECT_EQ(mn[0], 4.5f) << "translate";
    EXPECT_EQ(mx[0], 5.5f) << "translate";

    cube->SetVertex(0, -20.f, 0.f, 0.f);
    EXPECT_EQ(vm.AggregateBBox(true).GetLargestLength(), 25.5f) << "SetVertex";
}

// Init remet la révision à zéro : un maillage reconstruit par la même suite
// d'écritures revient à la même révision avec d'autres positions. L'estampille
// de la boîte est invalidée par Init, sinon la boîte périmée passerait pour à
// jour.
TEST(TEST_cgmesh_vmodels, bbox_after_reinit_to_same_revision)
{
    auto fill = [](Mesh& mesh, float offset)
    {
        mesh.Init(3, 1);
        mesh.SetVertex(0, offset + 0.f, 0.f, 0.f);
        mesh.SetVertex(1, offset + 1.f, 0.f, 0.f);
        mesh.SetVertex(2, offset + 0.f, 1.f, 0.f);
        mesh.SetFace(0, 0, 1, 2);
    };

    Mesh mesh;
    fill(mesh, 0.f);
    const uint64_t revision = mesh.GetRevision();
    float mn[3], mx[3];
    mesh.CurrentBBox().GetMinMax(mn, mx);
    EXPECT_EQ(mx[0], 1.f);

    fill(mesh, 100.f);
    ASSERT_EQ(mesh.GetRevision(), revision) << "le cas suppose une revision identique";
    mesh.CurrentBBox().GetMinMax(mn, mx);
    EXPECT_EQ(mn[0], 100.f);
    EXPECT_EQ(mx[0], 101.f);
}

// CHAQUE MUTATEUR DE POSITION invalide la boîte en cache : après l'appel, la
// révision a changé et CurrentBBox rend la boîte des positions courantes,
// recalculée ici par un parcours indépendant. L'entrée de chaque cas est
// choisie pour que la boîte change. Les deux Split… recopient les positions
// des sommets référencés vers des doublons : sur un maillage sans sommet
// isolé, ils ne peuvent pas changer la boîte, et seule la révision est
// attendue en mouvement, la boîte restant celle d'avant.
//
// SplitVerticesByUVSeams récrit chaque coin par FaceRef::SetVertex, qui
// incrémente lui-même la révision : son incrément propre ne s'observe que sur
// un maillage sans faces, d'où le cas « without_faces ». Ce qu'il advient
// alors des sommets n'est pas le sujet de ce test : la boîte y est seulement
// tenue de rester celle des positions courantes.
namespace {

// InitVertices est protege ; Init (n, f), sa seule voie publique, incremente
// la revision de son cote. L'acces direct isole l'increment propre a
// InitVertices.
struct MeshWithInitVertices : public Mesh
{
    using Mesh::InitVertices;
};

struct PositionMutator
{
    const char* name;
    std::function<void(Mesh&)> build;
    std::function<void(MeshWithInitVertices&)> mutate;
    enum class Box { Changes, Unchanged, Unspecified } box;
};

using Box = PositionMutator::Box;

void buildPoints(Mesh& mesh, const std::vector<float>& xyz)
{
    const unsigned int n = (unsigned int)(xyz.size() / 3);
    mesh.Init(n, 0);
    for (unsigned int i = 0; i < n; ++i)
        mesh.SetVertex(i, xyz[3*i], xyz[3*i+1], xyz[3*i+2]);
}

void buildTriangle(Mesh& mesh)
{
    buildPoints(mesh, {0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f});
}

// Quad de deux triangles qui partagent v0 et v2.
void buildQuad(Mesh& mesh)
{
    mesh.Init(4, 2);
    mesh.SetVertex(0, 0.f, 0.f, 0.f);
    mesh.SetVertex(1, 1.f, 0.f, 0.f);
    mesh.SetVertex(2, 1.f, 1.f, 0.f);
    mesh.SetVertex(3, 0.f, 1.f, 0.f);
    mesh.SetFace(0, 0, 1, 2);
    mesh.SetFace(1, 0, 2, 3);
}

// Couture d'UV en v0 : la face 0 lui donne l'UV 0, la face 1 l'UV 3.
void buildQuadWithUvSeam(Mesh& mesh)
{
    buildQuad(mesh);
    mesh.SetTextureCoordinates(std::vector<float>{0.f,0.f, 1.f,0.f, 1.f,1.f, 0.5f,0.5f, 0.f,1.f}, 5);
    const unsigned int uv[2][3] = {{0u, 1u, 2u}, {3u, 2u, 4u}};
    for (unsigned int f = 0; f < 2; ++f)
    {
        mesh.FaceAt(f)->SetUsesTextureCoordinates(true);
        mesh.FaceAt(f)->ActivateTextureCoordinatesIndices();
        for (unsigned int c = 0; c < 3; ++c)
            mesh.FaceAt(f)->SetTexCoord(c, uv[f][c]);
    }
}

int deleteRightOfHalf(float x, float, float) { return x > 0.5f ? 1 : 0; }

const PositionMutator kPositionMutators[] = {
    {"InitVertices", buildTriangle,
        [](MeshWithInitVertices& m) { m.InitVertices(2); }, Box::Changes},
    {"SetVertices", buildTriangle,
        [](MeshWithInitVertices& m) { const float v[9] = {10.f,0.f,0.f, 11.f,0.f,0.f, 10.f,1.f,0.f}; m.SetVertices(3, v); }, Box::Changes},
    {"SetVertexComponent", buildTriangle,
        [](MeshWithInitVertices& m) { m.SetVertexComponent(1, 0, 7.f); }, Box::Changes},
    {"SetVertex", buildTriangle,
        [](MeshWithInitVertices& m) { m.SetVertex(2, 0.f, 9.f, 0.f); }, Box::Changes},
    {"Append", buildTriangle,
        [](MeshWithInitVertices& m) { Mesh other; buildPoints(other, {20.f, 0.f, 0.f}); m.Append(&other); }, Box::Changes},
    {"DeleteVertices", buildTriangle,
        [](MeshWithInitVertices& m) { m.DeleteVertices(deleteRightOfHalf); }, Box::Changes},
    {"add_gaussian_noise", buildTriangle,
        [](MeshWithInitVertices& m) { m.add_gaussian_noise(0.5f); }, Box::Changes},
    {"SplitVerticesByUVSeams", buildQuadWithUvSeam,
        [](MeshWithInitVertices& m) { m.SplitVerticesByUVSeams(); ASSERT_EQ(m.GetNVertices(), 5u); }, Box::Unchanged},
    {"SplitVerticesByUVSeams_without_faces",
        [](Mesh& m)
        {
            buildPoints(m, {0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f});
            m.SetTextureCoordinates(std::vector<float>{0.f, 0.f}, 1);
        },
        [](MeshWithInitVertices& m) { m.SplitVerticesByUVSeams(); }, Box::Unspecified},
    {"SplitVerticesByCornerNormals", buildQuad,
        [](MeshWithInitVertices& m)
        {
            std::vector<float> corners;
            for (int c = 0; c < 3; ++c) corners.insert(corners.end(), {0.f, 0.f, 1.f});
            for (int c = 0; c < 3; ++c) corners.insert(corners.end(), {0.f, 1.f, 0.f});
            ASSERT_EQ(m.SplitVerticesByCornerNormals(corners), 2);
        }, Box::Unchanged},
    {"MergeVertices",
        [](Mesh& m) { buildPoints(m, {0.f,0.f,0.f, 1.f,0.f,0.f, 1.1f,0.f,0.f}); },
        [](MeshWithInitVertices& m) { m.MergeVertices(0.2f); ASSERT_EQ(m.GetNVertices(), 2u); }, Box::Changes},
    {"centerize", buildTriangle,
        [](MeshWithInitVertices& m) { m.centerize(); }, Box::Changes},
    {"scale", buildTriangle,
        [](MeshWithInitVertices& m) { m.scale(2.f); }, Box::Changes},
    {"scale_xyz", buildTriangle,
        [](MeshWithInitVertices& m) { m.scale_xyz(1.f, 3.f, 1.f); }, Box::Changes},
    {"translate", buildTriangle,
        [](MeshWithInitVertices& m) { m.translate(5.f, 0.f, 0.f); }, Box::Changes},
    {"transform_array", buildTriangle,
        [](MeshWithInitVertices& m) { float r[9] = {0.f,-1.f,0.f, 1.f,0.f,0.f, 0.f,0.f,1.f}; m.transform(r); }, Box::Changes},
    {"transform_Matrix3f", buildTriangle,
        [](MeshWithInitVertices& m) { Matrix3f s; s.SetScale(2.f, 2.f, 2.f); m.transform(s); }, Box::Changes},
};

void PrintTo(const PositionMutator& m, std::ostream* os) { *os << m.name; }

class TEST_cgmesh_bbox_cache : public ::testing::TestWithParam<PositionMutator> {};

} // namespace

TEST_P(TEST_cgmesh_bbox_cache, mutator_refreshes_current_bbox)
{
    const PositionMutator& c = GetParam();
    MeshWithInitVertices mesh;
    c.build(mesh);

    float mn0[3], mx0[3];
    mesh.CurrentBBox().GetMinMax(mn0, mx0);   // remplit le cache
    const uint64_t revision = mesh.GetRevision();

    c.mutate(mesh);
    if (::testing::Test::HasFatalFailure())
        return;
    EXPECT_NE(mesh.GetRevision(), revision);

    float mn[3], mx[3];
    mesh.CurrentBBox().GetMinMax(mn, mx);

    if (mesh.GetNVertices() == 0)
    {
        EXPECT_TRUE(mesh.CurrentBBox().IsEmpty()) << "aucun sommet";
        EXPECT_NE(c.box, Box::Unchanged);
        return;
    }
    const std::vector<float>& v = mesh.GetVertices();
    float omn[3] = { v[0], v[1], v[2] }, omx[3] = { v[0], v[1], v[2] };
    for (unsigned int i = 1; i < mesh.GetNVertices(); ++i)
        for (int a = 0; a < 3; ++a)
        {
            omn[a] = std::min(omn[a], v[3*i+a]);
            omx[a] = std::max(omx[a], v[3*i+a]);
        }

    bool changed = false;
    for (int a = 0; a < 3; ++a)
    {
        EXPECT_EQ(mn[a], omn[a]) << "min, axe " << a;
        EXPECT_EQ(mx[a], omx[a]) << "max, axe " << a;
        changed = changed || omn[a] != mn0[a] || omx[a] != mx0[a];
    }
    if (c.box != Box::Unspecified)
        EXPECT_EQ(changed, c.box == Box::Changes) << "l'entree du cas doit changer la boite, ou ne le peut pas";
}

INSTANTIATE_TEST_SUITE_P(PositionMutators, TEST_cgmesh_bbox_cache,
                         ::testing::ValuesIn(kPositionMutators),
                         [](const ::testing::TestParamInfo<PositionMutator>& info)
                         { return std::string(info.param.name); });
