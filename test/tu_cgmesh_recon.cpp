#include <gtest/gtest.h>

#include <cmath>
#include <fstream>

#include "../src/cgmesh/recon_pipeline.h"
#include "../src/cgmesh/recon_unprojector.h"
#include "../src/cgmesh/recon_surface.h"
#include "../src/cgmesh/recon_surface_heightmap.h"
#include "../src/cgmesh/recon_surface_poisson.h"
#include "../src/cgmesh/recon_texture.h"
#include "../src/cgmesh/recon_postprocess.h"
#include "../src/cgmesh/recon_fuser.h"
#include "../src/cgmesh/recon_depth_scaler.h"
#include "../src/cgmesh/recon_pose_bundle.h"
#include "../src/cgmesh/recon_evaluate.h"
#include "../src/cgmesh/recon_io.h"
#include "../src/cgmesh/mesh_metrics.h"
#include "../src/cgmesh/icp.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/material.h"
#include "../src/cgimg/cgimg.h"

using namespace recon;

// --- types de données : valeurs par défaut et helpers ---
TEST(TEST_cgmesh_recon, types_defaults)
{
    DepthMap d;
    EXPECT_EQ(d.w, 0);
    EXPECT_TRUE(d.isDisparity);

    PointCloud pc;
    EXPECT_EQ(pc.size(), 0u);
    EXPECT_FALSE(pc.hasNormals());
    pc.positions = {0,0,0, 1,1,1};
    EXPECT_EQ(pc.size(), 2u);
    EXPECT_FALSE(pc.hasNormals());
    pc.normals = {0,0,1, 0,0,1};
    EXPECT_TRUE(pc.hasNormals());

    CameraView c;
    EXPECT_FLOAT_EQ(c.R[0], 1.f);  // rotation par défaut = identité
    EXPECT_FLOAT_EQ(c.R[4], 1.f);
}

// --- orchestrateur : étape requise manquante -> nullptr ---
TEST(TEST_cgmesh_recon, pipeline_missing_stages_returns_null)
{
    Pipeline p;
    Img img;
    EXPECT_EQ(p.runSingleView(img), nullptr);
}

// --- étapes factices : le câblage mono-vue produit un mesh et appelle le post ---
namespace {

struct DummyDepth : IDepthSource
{
    DepthMap estimate(const Img&) override
    {
        DepthMap d; d.w = 2; d.h = 2; d.z = {1.f,1.f,1.f,1.f};
        return d;
    }
};
struct DummyUnproject : IUnprojector
{
    PointCloud unproject(const DepthMap&, const CameraView&) override
    {
        PointCloud pc; pc.positions = {0,0,0, 1,0,0, 0,1,0};
        return pc;
    }
};
struct DummyRecon : ISurfaceReconstructor
{
    Mesh* reconstruct(const PointCloud&) override { return new Mesh(); }
};
struct CountingPost : IMeshPostProcessor
{
    int calls = 0;
    void process(Mesh&) override { ++calls; }
};

} // namespace

TEST(TEST_cgmesh_recon, pipeline_single_view_wires_stages)
{
    DummyDepth     depth;
    DummyUnproject unp;
    DummyRecon     rec;
    CountingPost   post;

    Pipeline p;
    p.depthSource          = &depth;
    p.unprojector          = &unp;
    p.surfaceReconstructor = &rec;
    p.meshPostProcessor    = &post;

    Img img;
    Mesh* m = p.runSingleView(img);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(post.calls, 1);  // post-traitement bien enchaîné
    delete m;
}

// === Étape [5] : déprojection sténopé concrète ===
TEST(TEST_cgmesh_recon, unprojector_pinhole)
{
    DepthMap d;
    d.w = 4; d.h = 4; d.isDisparity = true;
    d.z.assign(16, 0.5f);   // disparité uniforme -> tous les pixels gardés

    PinholeUnprojector up;
    up.stride = 1;          // tous les pixels
    up.dispMin = 0.f;
    PointCloud pc = up.unproject(d, CameraView{});

    EXPECT_EQ(pc.size(), 16u);                 // 4x4 points
    EXPECT_EQ(pc.positions.size(), 48u);
    for (float v : pc.positions) EXPECT_TRUE(std::isfinite(v));
}

// === Étape [8] : reconstruction de surface (PointCloudField + marching cubes) ===
// Le point critique : aucun appelant existant -> on valide ici sur une sphère.
TEST(TEST_cgmesh_recon, surface_implicit_sphere)
{
    // Nuage : sphère unité échantillonnée (fibonacci).
    PointCloud sphere;
    const int   N  = 800;
    const float PI = 3.14159265358979f;
    const float ga = PI * (3.f - std::sqrt(5.f));
    for (int i = 0; i < N; ++i)
    {
        float z  = 1.f - 2.f * (i + 0.5f) / N;
        float r  = std::sqrt(std::max(0.f, 1.f - z * z));
        float th = ga * i;
        sphere.positions.push_back(r * std::cos(th));
        sphere.positions.push_back(r * std::sin(th));
        sphere.positions.push_back(z);
    }
    ASSERT_EQ(sphere.size(), (size_t)N);

    ImplicitSurfaceReconstructor rec;
    rec.isoDistance = 0.2f;
    rec.gridCells   = 48;
    Mesh* m = rec.reconstruct(sphere);

    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(), 0u);
    delete m;
}

// === Étape [9] : post-traitement (MergeVertices in place) ne casse pas le mesh ===
TEST(TEST_cgmesh_recon, postprocess_merge_runs)
{
    // Deux triangles partageant une arête, mais avec sommets dupliqués coïncidents.
    Mesh mesh;
    mesh.Init(6, 2);
    mesh.SetVertex(0, 0,0,0); mesh.SetVertex(1, 1,0,0); mesh.SetVertex(2, 0,1,0);
    mesh.SetVertex(3, 1,0,0); mesh.SetVertex(4, 0,1,0); mesh.SetVertex(5, 1,1,0);
    mesh.SetFace(0, 0,1,2);
    mesh.SetFace(1, 3,4,5);

    MergeVerticesPostProcessor post;
    post.process(mesh);

    EXPECT_EQ(mesh.GetNFaces(), 2u);            // faces préservées
    EXPECT_LE(mesh.GetNVertices(), 6u);         // soudure : pas plus de sommets
}

// === Phase 2 : déprojection en repère MONDE (R,T) ===
TEST(TEST_cgmesh_recon, unprojector_world_pose)
{
    // Profondeur métrique constante Z=2 ; caméra identité translatée T=(0,0,-5).
    DepthMap d;
    d.w = 4; d.h = 4; d.isDisparity = false;
    d.z.assign(16, 2.f);

    CameraView cam;          // R = identité
    cam.T[0] = 0.f; cam.T[1] = 0.f; cam.T[2] = -5.f;

    PinholeUnprojector up;
    up.stride = 1;
    PointCloud pc = up.unproject(d, cam);
    ASSERT_EQ(pc.size(), 16u);

    // Pixel principal (x=cx,y=cy) -> Pc=(0,0,-2) -> monde = (0,0,-2)-T = (0,0,3).
    float best = 1e9f;
    for (size_t i = 0; i < pc.size(); ++i)
    {
        float dx = pc.positions[3*i+0] - 0.f;
        float dy = pc.positions[3*i+1] - 0.f;
        float dz = pc.positions[3*i+2] - 3.f;
        best = std::min(best, dx*dx + dy*dy + dz*dz);
    }
    EXPECT_LT(best, 1e-4f);  // un point exactement en (0,0,3)
}

// === Phase 2 : fusion par union ===
TEST(TEST_cgmesh_recon, fuser_concat)
{
    PointCloud a; a.positions = {0,0,0, 1,0,0, 0,1,0};   // 3 points
    PointCloud b; b.positions = {0,0,1, 1,1,1};          // 2 points
    ConcatFuser fuser;
    PointCloud out = fuser.fuse({a, b});
    EXPECT_EQ(out.size(), 5u);
}

namespace {

struct DummyPose : IPoseSource
{
    bool estimate(const std::vector<std::string>&, std::vector<CameraView>& cams,
                  PointCloud&) override
    {
        CameraView a; a.w = 4; a.h = 4;                 // identité
        CameraView b; b.w = 4; b.h = 4; b.T[0] = 1.f;   // translatée
        cams = {a, b};
        return true;
    }
};
struct DummyMetricDepth : IDepthSource
{
    DepthMap estimate(const Img&) override
    {
        DepthMap d; d.w = 4; d.h = 4; d.isDisparity = false; d.z.assign(16, 2.f);
        return d;
    }
};
struct RecordingRecon : ISurfaceReconstructor
{
    size_t lastSize = 0;
    Mesh* reconstruct(const PointCloud& c) override { lastSize = c.size(); return new Mesh(); }
};

} // namespace

// === Phase 2 : orchestration multi-vues (runMultiView) ===
TEST(TEST_cgmesh_recon, pipeline_multiview_wires_stages)
{
    DummyPose         pose;
    DummyMetricDepth  depth;
    PinholeUnprojector unp; unp.stride = 1;
    ConcatFuser       fuser;
    RecordingRecon    rec;
    CountingPost      post;

    Pipeline p;
    p.poseSource          = &pose;
    p.depthSource         = &depth;
    p.unprojector         = &unp;
    p.fuser               = &fuser;
    p.surfaceReconstructor = &rec;
    p.meshPostProcessor   = &post;

    std::vector<std::string> paths = {"a.png", "b.png"};
    Img ia, ib;
    std::vector<Img*> imgs = {&ia, &ib};

    Mesh* m = p.runMultiView(paths, imgs);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(post.calls, 1);
    EXPECT_EQ(rec.lastSize, 32u);   // 2 vues x 16 px (stride 1) fusionnées
    delete m;
}

// === Phase 2 : poses depuis un bundle.out (fabriqué, sans COLMAP) ===
TEST(TEST_cgmesh_recon, pose_bundle_source)
{
    // Images factices pour renseigner w/h (Bundle ne les remplit pas).
    { Img a(64, 48); a.init_color(10,20,30,255); a.save((char*)"./tb_cam0.bmp"); }
    { Img b(64, 48); b.init_color(40,50,60,255); b.save((char*)"./tb_cam1.bmp"); }

    // bundle.out minimal : 2 caméras (identité, T distincts) + 1 point.
    {
        std::ofstream f("./tb.out");
        f << "# Bundle file v0.3\n2 1\n";
        f << "500 0 0\n1 0 0\n0 1 0\n0 0 1\n0 0 -3\n";   // caméra 0
        f << "500 0 0\n1 0 0\n0 1 0\n0 0 1\n1 0 -3\n";   // caméra 1
        f << "0 0 0\n255 128 64\n1 0 0 100 100\n";       // point + couleur + visibilité
    }

    BundleSource src;
    src.bundlePath = "./tb.out";
    std::vector<CameraView> cams;
    PointCloud sparse;
    bool ok = src.estimate({"./tb_cam0.bmp", "./tb_cam1.bmp"}, cams, sparse);

    ASSERT_TRUE(ok);
    ASSERT_EQ(cams.size(), 2u);
    EXPECT_FLOAT_EQ(cams[0].fx, 500.f);
    EXPECT_EQ(cams[0].w, 64);
    EXPECT_EQ(cams[0].h, 48);
    EXPECT_FLOAT_EQ(cams[0].cx, 32.f);   // w/2
    EXPECT_FLOAT_EQ(cams[0].R[0], 1.f);  // R = identité
    EXPECT_FLOAT_EQ(cams[0].T[2], -3.f);
    EXPECT_FLOAT_EQ(cams[1].T[0], 1.f);
    EXPECT_EQ(sparse.size(), 1u);
}

// === Phase 3 : reconstruction heightmap sur nuage organisé ===
TEST(TEST_cgmesh_recon, surface_heightmap_grid)
{
    PointCloud pc; pc.width = 3; pc.height = 3;
    for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 3; ++gx)
        { pc.positions.push_back((float)gx); pc.positions.push_back((float)gy); pc.positions.push_back(0.f); }
    pc.valid.assign(9, 1);
    ASSERT_TRUE(pc.organized());

    HeightmapReconstructor rec;
    Mesh* m = rec.reconstruct(pc);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->GetNFaces(), 8u);      // 2x2 cellules × 2 triangles
    EXPECT_EQ(m->GetNVertices(), 9u);
    delete m;
}

TEST(TEST_cgmesh_recon, surface_heightmap_culls_discontinuity)
{
    // Grille plane sauf le coin (2,2) propulsé loin -> sa cellule est coupée.
    PointCloud pc; pc.width = 3; pc.height = 3;
    for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 3; ++gx)
        {
            pc.positions.push_back((float)gx); pc.positions.push_back((float)gy);
            pc.positions.push_back((gx == 2 && gy == 2) ? 100.f : 0.f);
        }
    pc.valid.assign(9, 1);

    HeightmapReconstructor rec; rec.edgeFactor = 4.f;
    Mesh* m = rec.reconstruct(pc);
    ASSERT_NE(m, nullptr);
    EXPECT_LT(m->GetNFaces(), 8u);      // la cellule du coin lointain est éliminée
    delete m;
}

// === [4] alignement d'échelle : disparité relative -> profondeur métrique ===
TEST(TEST_cgmesh_recon, depth_scaler_affine)
{
    // Caméra calibrée à la main (R=identité, T=0). Projection : u = cx + fx·Pc.x/(−Pc.z).
    CameraView cam;
    cam.w = 100; cam.h = 100;
    cam.fx = cam.fy = 100.f; cam.cx = cam.cy = 50.f;

    // 3 points épars à profondeurs connues, projetés sur des pixels distincts :
    // P=(x,0,−Z) -> u=50+100·x/Z, v=50, Z_true=Z.
    PointCloud sparse;
    sparse.positions = { 0.0f, 0.f, -2.f,    // -> (50,50), Z=2
                         0.4f, 0.f, -4.f,    // -> (60,50), Z=4
                         0.9f, 0.f, -6.f };  // -> (65,50), Z=6

    // Carte de disparité : valeur = 1/Z_true aux pixels épars (relation 1/Z = 1·disp + 0).
    DepthMap d; d.w = 100; d.h = 100; d.isDisparity = true;
    d.z.assign(100 * 100, 0.3f);
    d.z[50 * 100 + 50] = 0.5f;        // 1/2
    d.z[50 * 100 + 60] = 0.25f;       // 1/4
    d.z[50 * 100 + 65] = 1.f / 6.f;   // 1/6

    AffineDepthScaler scaler;
    scaler.align(d, cam, sparse);

    EXPECT_FALSE(d.isDisparity);                       // devenue métrique
    EXPECT_NEAR(d.z[50 * 100 + 50], 2.f, 0.05f);
    EXPECT_NEAR(d.z[50 * 100 + 60], 4.f, 0.05f);
    EXPECT_NEAR(d.z[50 * 100 + 65], 6.f, 0.1f);
}

// === [7] normales orientées estimées à la déprojection (plan frontal) ===
TEST(TEST_cgmesh_recon, unprojector_with_normals_frontal_plane)
{
    DepthMap d; d.w = 8; d.h = 8; d.isDisparity = false;
    d.z.assign(64, 2.f);                               // plan frontal métrique Z=2

    PinholeUnprojector up; up.stride = 1; up.withNormals = true;
    PointCloud pc = up.unproject(d, CameraView{});     // caméra identité à l'origine

    ASSERT_EQ(pc.size(), 64u);
    ASSERT_TRUE(pc.hasNormals());
    // Plan frontal -> toutes les normales unitaires et ~ (0,0,1) (orientées vers la caméra).
    float minNz = 1e9f;
    for (size_t i = 0; i < pc.size(); ++i)
    {
        const float nx = pc.normals[3*i], ny = pc.normals[3*i+1], nz = pc.normals[3*i+2];
        EXPECT_NEAR(std::sqrt(nx*nx + ny*ny + nz*nz), 1.f, 1e-3f);
        minNz = std::min(minNz, nz);
    }
    EXPECT_GT(minNz, 0.99f);
}

#ifdef CG_HAS_POISSON
// === Poisson (master vendorisé) sur sphère orientée synthétique ===
TEST(TEST_cgmesh_recon, surface_poisson_sphere)
{
    PointCloud sphere;
    const int   N  = 500;   // échantillonnage réduit : coût Poisson maîtrisé en Debug+couverture
    const float PI = 3.14159265358979f;
    const float ga = PI * (3.f - std::sqrt(5.f));
    for (int i = 0; i < N; ++i)
    {
        float z  = 1.f - 2.f * (i + 0.5f) / N;
        float r  = std::sqrt(std::max(0.f, 1.f - z * z));
        float th = ga * i;
        float x = r * std::cos(th), y = r * std::sin(th);
        sphere.positions.push_back(x); sphere.positions.push_back(y); sphere.positions.push_back(z);
        sphere.normals.push_back(x);   sphere.normals.push_back(y);   sphere.normals.push_back(z);
    }
    ASSERT_TRUE(sphere.hasNormals());

    PoissonReconstructor rec; rec.depth = 4;   // faible profondeur : test rapide (Debug+couverture)
    Mesh* m = rec.reconstruct(sphere);
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(), 0u);
    delete m;
}

// === Poisson : garde des préconditions — normales orientées obligatoires ===
// Poisson intègre un champ de normales : sans normales, l'adaptateur doit refuser
// (nullptr) au lieu de tenter une reconstruction. On vérifie aussi le seuil n<4.
TEST(TEST_cgmesh_recon, surface_poisson_requires_normals)
{
    // Nuage de positions valides (>= 4 points) MAIS sans normales -> refus.
    PointCloud noNormals;
    noNormals.positions = { 0,0,0, 1,0,0, 0,1,0, 0,0,1 };
    ASSERT_EQ(noNormals.size(), 4u);
    ASSERT_FALSE(noNormals.hasNormals());

    PoissonReconstructor rec;
    EXPECT_EQ(rec.reconstruct(noNormals), nullptr);

    // Trop peu de points (n < 4), même avec des normales -> refus aussi.
    PointCloud tooFew;
    tooFew.positions = { 0,0,0, 1,0,0, 0,1,0 };
    tooFew.normals   = { 0,0,1, 0,0,1, 0,0,1 };
    ASSERT_EQ(tooFew.size(), 3u);
    ASSERT_TRUE(tooFew.hasNormals());
    EXPECT_EQ(rec.reconstruct(tooFew), nullptr);
}
#endif

// === Primitive ICP (étape 6 rigide / étape 11 similarité) ===
namespace {

// Cube unité centré sur l'origine (8 sommets, 12 triangles).
void buildUnitCube(Mesh& m)
{
    m.Init(8, 12);
    const float h = 0.5f;
    const float V[8][3] = {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
                           {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}};
    for (int i=0;i<8;++i) m.SetVertex(i, V[i][0],V[i][1],V[i][2]);
    const int F[12][3] = {{0,1,2},{0,2,3},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
                          {3,2,6},{3,6,7},{0,3,7},{0,7,4},{1,2,6},{1,6,5}};
    for (int i=0;i<12;++i) m.SetFace(i, F[i][0],F[i][1],F[i][2]);
}

// Nuage de surface dense d'un cube unité, normales orientées (res×res par face).
PointCloud denseCubeCloud(int res)
{
    PointCloud c;
    const float h=0.5f;
    auto add=[&](float x,float y,float z,float nx,float ny,float nz){
        c.positions.push_back(x);c.positions.push_back(y);c.positions.push_back(z);
        c.normals.push_back(nx); c.normals.push_back(ny); c.normals.push_back(nz); };
    for (int i=0;i<res;++i) for (int j=0;j<res;++j)
    {
        const float a=-h+(i+0.5f)/res, b=-h+(j+0.5f)/res;
        add(a,b,-h, 0,0,-1); add(a,b, h, 0,0, 1);
        add(a,-h,b, 0,-1,0); add(a, h,b, 0, 1,0);
        add(-h,a,b, -1,0,0); add( h,a,b,  1,0,0);
    }
    return c;
}

// Points sur la surface du cube : 8 coins + 6 centres de face.
std::vector<float> cubeSurfaceSamples()
{
    const float h=0.5f;
    return {-h,-h,-h, h,-h,-h, h,h,-h, -h,h,-h, -h,-h,h, h,-h,h, h,h,h, -h,h,h,
            0,0,-h, 0,0,h, 0,-h,0, 0,h,0, -h,0,0, h,0,0};
}

// Applique p' = scale·Rz(deg)·p + t à un nuage.
std::vector<float> applySim(const std::vector<float>& pts, float deg, float scale,
                            float tx, float ty, float tz)
{
    const float a=deg*3.14159265f/180.f, c=std::cos(a), s=std::sin(a);
    std::vector<float> out; out.reserve(pts.size());
    for (size_t i=0;i<pts.size()/3;++i)
    {
        const float x=pts[3*i], y=pts[3*i+1], z=pts[3*i+2];
        out.push_back(scale*(c*x - s*y) + tx);
        out.push_back(scale*(s*x + c*y) + ty);
        out.push_back(scale*z          + tz);
    }
    return out;
}

} // namespace

// Recalage RIGIDE : un nuage sur-surface transformé (rot+transl) est recalé sur
// le cube -> RMS quasi nul, convergence.
TEST(TEST_cgmesh_recon, icp_rigid_recovers_pose)
{
    Mesh cube; buildUnitCube(cube);
    std::vector<float> src = applySim(cubeSurfaceSamples(), 12.f, 1.f, 0.08f,-0.05f,0.06f);

    ICPOptions opt; opt.withScale=false;
    ICPResult r = icp_align(src, cube, opt);

    EXPECT_TRUE(r.converged);
    EXPECT_LT(r.rmsError, 1e-3f);
    EXPECT_NEAR(r.scale, 1.f, 1e-4f);
}

// Recalage SIMILARITÉ : un nuage MIS À L'ÉCHELLE (×1.5) ne peut être recalé qu'avec
// le DDL d'échelle -> similarité récupère scale≈1/1.5 et RMS faible, là où le rigide
// laisse un résidu nettement plus grand.
TEST(TEST_cgmesh_recon, icp_scale_needs_similarity)
{
    Mesh cube; buildUnitCube(cube);
    std::vector<float> src = applySim(cubeSurfaceSamples(), 12.f, 1.5f, 0.08f,-0.05f,0.06f);

    ICPOptions rigidOpt; rigidOpt.withScale=false;
    ICPResult rigid = icp_align(src, cube, rigidOpt);

    ICPOptions simOpt; simOpt.withScale=true;
    ICPResult sim = icp_align(src, cube, simOpt);

    EXPECT_NEAR(sim.scale, 1.f/1.5f, 0.05f);
    EXPECT_LT(sim.rmsError, 0.02f);
    EXPECT_GT(rigid.rmsError, sim.rmsError * 3.f);   // l'échelle est indispensable
}

// === Étape [11] : évaluation Hausdorff, auto-vérifiée sur cube synthétique ===
// Cube ANALYTIQUE = oracle exact : reconstruire un nuage de cube par Poisson puis
// mesurer l'écart à la référence donne des valeurs connues et interprétables —
// erreur quasi nulle sur les faces, concentrée aux 8 coins (arrondi Poisson).
#ifdef CG_HAS_POISSON
TEST(TEST_cgmesh_recon, evaluate_cube_poisson)
{
    PointCloud cloud = denseCubeCloud(24);
    ASSERT_TRUE(cloud.hasNormals());

    PoissonReconstructor rec; rec.depth = 4;   // profondeur réduite (coût Debug+couverture)
    Mesh* m = rec.reconstruct(cloud);
    ASSERT_NE(m, nullptr);
    ASSERT_GT(m->GetNVertices(), 0u);

    Mesh ref; buildUnitCube(ref);
    HausdorffEvaluator ev;
    Metrics mt = ev.evaluate(*m, ref);

    // Cube côté 1.0 : erreurs petites et ordonnées max > p95 > moyenne > 0.
    // Seuils calibrés pour depth=4 (profondeur basse choisie pour le coût Debug/
    // couverture) : plus lâches qu'à depth 7 car une grille d'octree plus grossière
    // arrondit davantage les arêtes — les marges restent ~1,4–1,9× (robuste CI).
    EXPECT_GT(mt.meanError, 0.f);
    EXPECT_LT(mt.meanError, 0.01f);          // faces quasi exactes
    EXPECT_LT(mt.p95Error,  0.025f);
    EXPECT_LT(mt.hausdorff, 0.07f);          // max borné par l'arrondi des coins/arêtes
    EXPECT_LT(mt.hausdorffRelative, 0.05f);
    EXPECT_GE(mt.hausdorff, mt.p95Error);    // max >= 95e percentile
    EXPECT_GE(mt.p95Error,  mt.meanError);   // concentration de l'erreur (coins)

    // Heatmap : une distance par sommet.
    std::vector<float> d = mesh_pointwise_distance(*m, ref);
    EXPECT_EQ(d.size(), (size_t)m->GetNVertices());

    delete m;
}
#endif

// === Étape [11] : évaluation Hausdorff — tests INDÉPENDANTS de Poisson ===
// L'unique autre test de HausdorffEvaluator (evaluate_cube_poisson) est sous
// #ifdef CG_HAS_POISSON : sans Poisson, recon_evaluate.cpp n'a aucune couverture.
// Ici on construit les mesh directement (cube unité), sans reconstruction, pour
// valider l'ADAPTATEUR lui-même : mapping des champs + garde de la diagonale.

// L'adaptateur recopie fidèlement les champs de mesh_hausdorff dans Metrics, et
// pose hausdorffRelative = symmetric / diagonale(mesh). On le vérifie en comparant
// à un appel direct de la métrique sous-jacente (oracle).
TEST(TEST_cgmesh_recon, evaluate_maps_metric_fields)
{
    // Deux cubes identiques, l'un translaté -> erreur non nulle, valeurs concrètes.
    Mesh a;  buildUnitCube(a);
    Mesh b;  buildUnitCube(b);  b.translate(0.3f, 0.f, 0.f);
    HausdorffResult h = mesh_hausdorff(a, b);   // oracle (déterministe, sans RNG)

    Mesh a2; buildUnitCube(a2);
    Mesh b2; buildUnitCube(b2); b2.translate(0.3f, 0.f, 0.f);
    HausdorffEvaluator ev;
    Metrics m = ev.evaluate(a2, b2);

    // Mapping champ à champ.
    EXPECT_FLOAT_EQ(m.hausdorff,  h.symmetric);
    EXPECT_FLOAT_EQ(m.meanError,  h.mean_symmetric);
    EXPECT_FLOAT_EQ(m.rmsError,   h.rms_symmetric);
    EXPECT_FLOAT_EQ(m.p95Error,   h.p95_symmetric);
    EXPECT_GT(m.hausdorff, 0.f);                       // mesh décalés -> erreur réelle

    // hausdorffRelative = symmetric / diagonale du PREMIER argument (le mesh évalué).
    a2.computebbox();
    const float diag = a2.bbox_diagonal_length();
    ASSERT_GT(diag, 0.f);
    EXPECT_FLOAT_EQ(m.hausdorffRelative, h.symmetric / diag);
}

// Mesh identiques -> toutes les erreurs nulles (samples exactement sur l'autre surface).
TEST(TEST_cgmesh_recon, evaluate_identical_meshes_zero_error)
{
    Mesh a; buildUnitCube(a);
    Mesh b; buildUnitCube(b);
    HausdorffEvaluator ev;
    Metrics m = ev.evaluate(a, b);

    EXPECT_NEAR(m.hausdorff,         0.f, 1e-5f);
    EXPECT_NEAR(m.meanError,         0.f, 1e-5f);
    EXPECT_NEAR(m.rmsError,          0.f, 1e-5f);
    EXPECT_NEAR(m.p95Error,          0.f, 1e-5f);
    EXPECT_NEAR(m.hausdorffRelative, 0.f, 1e-5f);
}

// Garde de la diagonale : mesh dégénéré (tous sommets confondus -> bbox plate,
// diagonale = 0). hausdorffRelative doit valoir 0 (et non +inf/NaN) même si la
// distance absolue est non nulle.
TEST(TEST_cgmesh_recon, evaluate_relative_zero_on_degenerate_bbox)
{
    Mesh degen; degen.Init(3, 1);
    degen.SetVertex(0, 0,0,0); degen.SetVertex(1, 0,0,0); degen.SetVertex(2, 0,0,0);
    degen.SetFace(0, 0,1,2);
    Mesh ref; buildUnitCube(ref);

    HausdorffEvaluator ev;
    Metrics m = ev.evaluate(degen, ref);

    EXPECT_FLOAT_EQ(m.hausdorffRelative, 0.f);   // diagonale(mesh) = 0 -> garde
    EXPECT_GT(m.hausdorff, 0.f);                 // point à l'origine vs surface du cube
}

// === recon_io : orientation de la scène sur le plan dominant (mise à niveau Oxy) ===
// Plan incliné synthétique NE passant pas par l'origine + points « objet » au-dessus.
// Après orientSceneWithPlane : le plan tombe sur z=0 (normale -> +Z) et l'objet reste
// au-dessus (z>0), à sa distance signée d'origine.
TEST(TEST_cgmesh_recon, orient_scene_with_plane)
{
    const float n0[3] = { 0.3f, 0.2f, 1.0f };
    const float nl = std::sqrt(n0[0]*n0[0]+n0[1]*n0[1]+n0[2]*n0[2]);
    const float N[3] = { n0[0]/nl, n0[1]/nl, n0[2]/nl };
    const float B[3] = { 1.f, 1.f, 3.f };                 // point du plan (hors origine)
    float e1[3] = { N[1], -N[0], 0.f };                   // base in-plane (e1 ⟂ N)
    const float e1l = std::sqrt(e1[0]*e1[0]+e1[1]*e1[1]+e1[2]*e1[2]);
    e1[0]/=e1l; e1[1]/=e1l; e1[2]/=e1l;
    const float e2[3] = { N[1]*e1[2]-N[2]*e1[1], N[2]*e1[0]-N[0]*e1[2], N[0]*e1[1]-N[1]*e1[0] };

    recon::PointCloud pc;
    const int G = 11;
    size_t nPlane = 0;
    for (int gx=-G; gx<=G; ++gx) for (int gy=-G; gy<=G; ++gy)
    {
        for (int k=0;k<3;++k) pc.positions.push_back(B[k] + gx*0.2f*e1[k] + gy*0.2f*e2[k]);
        for (int k=0;k<3;++k) pc.normals.push_back(N[k]);
        ++nPlane;
    }
    for (int i=0;i<20;++i)                                 // objet : +0.5 le long de N
    {
        const float s = -1.f + 0.1f*i;
        for (int k=0;k<3;++k) pc.positions.push_back(B[k] + 0.5f*N[k] + s*0.3f*e1[k]);
        for (int k=0;k<3;++k) pc.normals.push_back(N[k]);
    }
    ASSERT_TRUE(pc.hasNormals());

    ASSERT_TRUE(recon::orientSceneWithPlane(pc));

    for (size_t i=0;i<nPlane;++i)                          // plan -> Oxy, normale -> +Z
    {
        EXPECT_NEAR(pc.positions[3*i+2], 0.f, 0.02f);
        EXPECT_GT(pc.normals[3*i+2], 0.98f);
    }
    for (size_t i=nPlane;i<pc.size();++i)                  // objet -> au-dessus (~0.5)
        EXPECT_NEAR(pc.positions[3*i+2], 0.5f, 0.05f);
}

// === Étape [10] : texturation projective (ProjectiveTexturer) ===
// Caméra calibrée à la main (R=identité, T=0 -> centre à l'origine regardant -Z).
// Modèle sténopé du texturer : u = cx + fx·Pc.x/(−Pc.z), v = cy − fy·Pc.y/(−Pc.z),
// UV = (u/w, 1 − v/h). Une image BMP réelle est générée car MaterialTexture charge
// le fichier (la logique de texturation, elle, est purement géométrique).
namespace {

CameraView frontalCamera()
{
    CameraView cam;
    cam.w = 100; cam.h = 100;
    cam.fx = cam.fy = 100.f;
    cam.cx = cam.cy = 50.f;     // R=identité, T=0 par défaut
    return cam;
}

void writeTestTexture()
{
    Img a(64, 48); a.init_color(180, 140, 100, 255); a.save((char*)"./tt_tex0.bmp");
}

} // namespace

// Triangle frontal projeté entièrement dans l'image -> texturé par la vue 0,
// avec des UV connus (oracle analytique).
TEST(TEST_cgmesh_recon, texture_projective_frontal_face)
{
    writeTestTexture();

    Mesh mesh; mesh.Init(3, 1);
    mesh.SetVertex(0, -0.5f, -0.5f, -2.f);
    mesh.SetVertex(1,  0.5f, -0.5f, -2.f);
    mesh.SetVertex(2,  0.0f,  0.5f, -2.f);
    mesh.SetFace(0, 0, 1, 2);                 // normale = +Z, vers la caméra

    ProjectiveTexturer tex; tex.occlusion = false;
    tex.texture(mesh, {"./tt_tex0.bmp"}, {frontalCamera()});

    // Mesh éclaté : 1 triangle -> 1 face, 3 sommets propres.
    ASSERT_EQ(mesh.GetNFaces(), 1u);
    ASSERT_EQ(mesh.GetNVertices(), 3u);

    const int texId = mesh.GetMaterialId("tex0");
    ASSERT_GE(texId, 0);
    EXPECT_EQ(mesh.GetFaceMaterialId(0), (unsigned)texId);   // texturée par la vue 0
    EXPECT_TRUE(mesh.GetFace(0)->m_bUseTextureCoordinates);

    // UV attendus : A(0.25,0.25) B(0.75,0.25) C(0.5,0.75) (cf. projection ci-dessus).
    ASSERT_EQ(mesh.m_pTextureCoordinates.size(), 6u);
    const float expect[6] = { 0.25f,0.25f, 0.75f,0.25f, 0.5f,0.75f };
    for (int i = 0; i < 6; ++i)
        EXPECT_NEAR(mesh.m_pTextureCoordinates[i], expect[i], 1e-4f);
}

// Face dos tourné (normale opposée à la caméra) -> aucune vue valide -> repli gris,
// pas de coordonnées de texture.
TEST(TEST_cgmesh_recon, texture_projective_fallback_when_not_facing)
{
    writeTestTexture();

    Mesh mesh; mesh.Init(3, 1);
    mesh.SetVertex(0, -0.5f, -0.5f, -2.f);
    mesh.SetVertex(1,  0.5f, -0.5f, -2.f);
    mesh.SetVertex(2,  0.0f,  0.5f, -2.f);
    mesh.SetFace(0, 0, 2, 1);                 // enroulement inversé -> normale = −Z

    ProjectiveTexturer tex; tex.occlusion = false;
    tex.texture(mesh, {"./tt_tex0.bmp"}, {frontalCamera()});

    const int fb = mesh.GetMaterialId("fallback");
    ASSERT_GE(fb, 0);
    EXPECT_EQ(mesh.GetFaceMaterialId(0), (unsigned)fb);      // repli, pas la texture
    EXPECT_FALSE(mesh.GetFace(0)->m_bUseTextureCoordinates);
}

// Deux faces frontales alignées sur la ligne de visée : l'avant masque l'arrière.
// occlusion ON -> face arrière repliée ; OFF -> elle serait texturée (l'occlusion
// est bien la cause du repli, pas la géométrie de projection).
TEST(TEST_cgmesh_recon, texture_projective_occlusion_culls_hidden_face)
{
    writeTestTexture();

    auto build = [](Mesh& m) {
        m.Init(6, 2);
        // Face 0 : occulteur, z=−1, projette entièrement dans l'image, contient (0,0).
        m.SetVertex(0, -0.4f, -0.4f, -1.f);
        m.SetVertex(1,  0.4f, -0.4f, -1.f);
        m.SetVertex(2,  0.0f,  0.4f, -1.f);
        m.SetFace(0, 0, 1, 2);
        // Face 1 : occultée, z=−2, directement derrière (barycentre sur la visée).
        m.SetVertex(3, -0.5f, -0.5f, -2.f);
        m.SetVertex(4,  0.5f, -0.5f, -2.f);
        m.SetVertex(5,  0.0f,  0.5f, -2.f);
        m.SetFace(1, 3, 4, 5);
    };

    const CameraView cam = frontalCamera();

    // --- occlusion ON ---
    Mesh on; build(on);
    ProjectiveTexturer texOn;                 // occlusion = true par défaut
    texOn.texture(on, {"./tt_tex0.bmp"}, {cam});

    ASSERT_EQ(on.GetNFaces(), 2u);
    const int texOnId = on.GetMaterialId("tex0");
    const int fbOnId  = on.GetMaterialId("fallback");
    ASSERT_GE(texOnId, 0); ASSERT_GE(fbOnId, 0);
    EXPECT_EQ(on.GetFaceMaterialId(0), (unsigned)texOnId);  // avant : texturée
    EXPECT_EQ(on.GetFaceMaterialId(1), (unsigned)fbOnId);   // arrière : occultée -> repli
    EXPECT_TRUE (on.GetFace(0)->m_bUseTextureCoordinates);
    EXPECT_FALSE(on.GetFace(1)->m_bUseTextureCoordinates);

    // --- occlusion OFF : la même face arrière est désormais texturée ---
    Mesh off; build(off);
    ProjectiveTexturer texOff; texOff.occlusion = false;
    texOff.texture(off, {"./tt_tex0.bmp"}, {cam});

    EXPECT_EQ(off.GetFaceMaterialId(1), (unsigned)off.GetMaterialId("tex0"));
    EXPECT_TRUE(off.GetFace(1)->m_bUseTextureCoordinates);
}
