// reconstruction-cli — pilote de la chaîne de reconstruction (Phase 1 mono-vue + Phase 2 multi-vues).
//
// Mono-vue  : reconstruction-cli <model.onnx> <image> [size]
// Multi-vues : reconstruction-cli --multiview <model.onnx> <bundle.out> <list.txt> <images_dir>
//
// Toute la logique vit dans cgmesh ; ce fichier câble les étapes concrètes via recon::Pipeline.
//
#include "cgmesh/recon_pipeline.h"
#include "cgmesh/recon_depth_onnx.h"
#include "cgmesh/recon_depth_scaler.h"
#include "cgmesh/recon_pose_bundle.h"
#include "cgmesh/recon_unprojector.h"
#include "cgmesh/recon_surface.h"
#include "cgmesh/recon_surface_heightmap.h"
#include "cgmesh/recon_surface_poisson.h"
#include "cgmesh/recon_fuser.h"
#include "cgmesh/recon_io.h"
#include "cgmesh/recon_texture.h"
#include "cgmesh/recon_postprocess.h"
#include "cgmesh/recon_evaluate.h"
#include "cgmesh/mesh_metrics.h"
#include "cgmesh/mesh.h"
#include "cgimg/image.h"

#include <cmath>
#include <cstdio>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static int run_single(const std::string& model, const std::string& imgPath, int size)
{
    Img img;
    if (img.load(imgPath.c_str()) != 0) { std::cerr << "load failed: " << imgPath << "\n"; return 1; }
    std::cout << "image: " << img.width() << "x" << img.height() << "\n";

    recon::OnnxDepthSource depth(model, size);
    if (!depth.ok()) { std::cerr << "model load failed: " << model << "\n"; return 1; }
    recon::PinholeUnprojector            unproject; unproject.organized = true;
    recon::HeightmapReconstructor        surface;   // maillage par grille (préserve le détail)
    recon::MergeVerticesPostProcessor    post;

    recon::Pipeline pipe;
    pipe.depthSource = &depth; pipe.unprojector = &unproject;
    pipe.surfaceReconstructor = &surface; pipe.meshPostProcessor = &post;

    Mesh* mesh = pipe.runSingleView(img);
    if (!mesh) { std::cerr << "reconstruction failed\n"; return 1; }
    std::cout << "mesh: " << mesh->GetNVertices() << " verts, " << mesh->GetNFaces() << " faces\n";
    mesh->save(const_cast<char*>("depth_mesh.obj"));
    std::cout << "written depth_mesh.obj\n";
    delete mesh;
    return 0;
}

static int run_multiview(const std::string& model, const std::string& bundlePath,
                         const std::string& listPath, const std::string& imagesDir, int size,
                         int poissonDepth)
{
    // Ordre des images = ordre des caméras dans le fichier liste Bundler.
    std::vector<std::string> names;
    {
        std::ifstream f(listPath);
        std::string line;
        while (std::getline(f, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (!line.empty()) names.push_back(line);
        }
    }
    if (names.empty()) { std::cerr << "empty list: " << listPath << "\n"; return 1; }

    std::vector<std::string> paths;
    std::vector<Img>         imgStore(names.size());
    std::vector<Img*>        imgs;
    for (size_t i = 0; i < names.size(); ++i)
    {
        std::string p = imagesDir + "/" + names[i];
        paths.push_back(p);
        if (imgStore[i].load(p.c_str()) != 0) { std::cerr << "load failed: " << p << "\n"; return 1; }
        imgs.push_back(&imgStore[i]);
    }
    std::cout << "multiview: " << names.size() << " views\n";

    recon::BundleSource    pose;    pose.bundlePath = bundlePath;
    recon::OnnxDepthSource depth(model, size);
    if (!depth.ok()) { std::cerr << "model load failed: " << model << "\n"; return 1; }
    recon::AffineDepthScaler             scaler;
    recon::PinholeUnprojector            unproject; unproject.withNormals = true;
    recon::ConcatFuser                   fuser;
    recon::MergeVerticesPostProcessor    post;

    recon::Pipeline pipe;
    pipe.poseSource = &pose;     pipe.depthSource = &depth;   pipe.depthScaler = &scaler;
    pipe.unprojector = &unproject; pipe.fuser = &fuser;
    pipe.meshPostProcessor = &post;
    pipe.exportCloudPath = "recon_multiview_cloud.ply";   // nuage fusionné utilisé

#ifdef CG_HAS_POISSON
    recon::PoissonReconstructor          surface; surface.depth = poissonDepth;
    std::cout << "surface: Poisson (depth " << surface.depth << ")\n";
#else
    recon::ImplicitSurfaceReconstructor  surface;
    std::cout << "surface: implicit (blobby)\n";
#endif
    pipe.surfaceReconstructor = &surface;

    Mesh* mesh = pipe.runMultiView(paths, imgs);
    if (!mesh) { std::cerr << "multiview reconstruction failed\n"; return 1; }
    std::cout << "mesh: " << mesh->GetNVertices() << " verts, " << mesh->GetNFaces() << " faces\n";
    mesh->save(const_cast<char*>("recon_multiview.obj"));
    std::cout << "written recon_multiview.obj\n";
    delete mesh;
    return 0;
}

#ifdef CG_HAS_POISSON
// Cube unité centré (8 sommets, 12 triangles) — référence analytique EXACTE.
static void buildUnitCube(Mesh& m)
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
static recon::PointCloud denseCubeCloud(int res)
{
    recon::PointCloud c;
    const float h = 0.5f;
    auto add=[&](float x,float y,float z,float nx,float ny,float nz){
        c.positions.push_back(x); c.positions.push_back(y); c.positions.push_back(z);
        c.normals.push_back(nx);  c.normals.push_back(ny);  c.normals.push_back(nz);
    };
    for (int i=0;i<res;++i) for (int j=0;j<res;++j)
    {
        const float a = -h + (i+0.5f)/res, b = -h + (j+0.5f)/res;
        add(a,b,-h, 0,0,-1); add(a,b, h, 0,0, 1);   // faces z
        add(a,-h,b, 0,-1,0); add(a, h,b, 0, 1,0);   // faces y
        add(-h,a,b, -1,0,0); add( h,a,b,  1,0,0);   // faces x
    }
    return c;
}
// PLY ASCII coloré (sommets + couleurs + faces) — Mesh::export_ply est vertices-only.
static void writeColoredPly(const char* path, Mesh& m, const std::vector<unsigned char>& rgb)
{
    FILE* fp = fopen(path, "w");
    if (!fp) return;
    const unsigned nv = m.GetNVertices(), nf = m.GetNFaces();
    fprintf(fp, "ply\nformat ascii 1.0\n");
    fprintf(fp, "element vertex %u\n", nv);
    fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    fprintf(fp, "property uchar red\nproperty uchar green\nproperty uchar blue\n");
    fprintf(fp, "element face %u\n", nf);
    fprintf(fp, "property list uchar int vertex_indices\nend_header\n");
    for (unsigned i=0;i<nv;++i)
        fprintf(fp, "%f %f %f %d %d %d\n",
                m.m_pVertices[3*i], m.m_pVertices[3*i+1], m.m_pVertices[3*i+2],
                rgb[3*i], rgb[3*i+1], rgb[3*i+2]);
    for (unsigned i=0;i<nf;++i)
    {
        Face* f = m.GetFace(i); if (!f) continue;
        const unsigned n = f->GetNVertices();
        fprintf(fp, "%u", n);
        for (unsigned k=0;k<n;++k) fprintf(fp, " %d", f->GetVertex(k));
        fprintf(fp, "\n");
    }
    fclose(fp);
}
#endif

int main(int argc, char** argv)
{
    // Reconstruction Poisson directe d'un nuage dense (ex. fused.ply de COLMAP MVS).
    if (argc > 1 && std::string(argv[1]) == "--poisson-ply")
    {
        if (argc < 3) { std::cerr << "usage: reconstruction-cli --poisson-ply <cloud.ply> [depth] [trimQuantile] [removePlane 0/1]\n"; return 1; }
#ifdef CG_HAS_POISSON
        const int   depth   = (argc > 3) ? std::stoi(argv[3]) : 8;
        const float trim    = (argc > 4) ? std::stof(argv[4]) : 0.f;
        const bool  rmPlane = (argc > 5) ? (std::stoi(argv[5]) != 0) : false;
        recon::PointCloud cloud;
        if (!recon::loadPointCloudPly(argv[2], cloud)) { std::cerr << "load failed: " << argv[2] << "\n"; return 1; }
        std::cout << "dense cloud: " << cloud.size() << " pts, normals=" << cloud.hasNormals() << "\n";
        if (rmPlane)
        {
            const size_t r = recon::removeDominantPlane(cloud);
            std::cout << "plan dominant retiré: " << r << " pts -> reste " << cloud.size() << "\n";
        }
        if (recon::savePointCloudPly("recon_dense_cloud.ply", cloud))
            std::cout << "exported recon_dense_cloud.ply\n";
        recon::PoissonReconstructor rec; rec.depth = depth; rec.trimQuantile = trim;
        std::cout << "Poisson depth=" << depth << " trim=" << trim << " removePlane=" << rmPlane << "\n";
        Mesh* m = rec.reconstruct(cloud);
        if (!m) { std::cerr << "Poisson failed (empty / no normals)\n"; return 1; }
        std::cout << "mesh: " << m->GetNVertices() << " verts, " << m->GetNFaces() << " faces\n";
        m->save(const_cast<char*>("recon_dense.obj"));
        std::cout << "written recon_dense.obj\n";
        delete m;
        return 0;
#else
        std::cerr << "Poisson non activé (ENABLE_POISSON)\n"; return 1;
#endif
    }

    // Texturation projective d'un mesh à partir des caméras (bundle.out) + images.
    if (argc > 1 && std::string(argv[1]) == "--texture")
    {
        if (argc < 6) { std::cerr << "usage: reconstruction-cli --texture <mesh.obj> <bundle.out> <list.txt> <images_dir> [out.obj]\n"; return 1; }
        const std::string outPath = (argc > 6) ? argv[6] : "textured.obj";
        std::vector<std::string> names;
        { std::ifstream f(argv[4]); std::string line; while (std::getline(f, line)) { while (!line.empty() && (line.back()=='\r'||line.back()==' ')) line.pop_back(); if (!line.empty()) names.push_back(line); } }
        if (names.empty()) { std::cerr << "empty list: " << argv[4] << "\n"; return 1; }
        std::vector<std::string> imagePaths;
        for (const auto& nm : names) imagePaths.push_back(std::string(argv[5]) + "/" + nm);

        Mesh mesh;
        if (mesh.load(argv[2]) != 0) { std::cerr << "load mesh failed: " << argv[2] << "\n"; return 1; }
        std::cout << "mesh: " << mesh.GetNVertices() << " v, " << mesh.GetNFaces() << " f\n";

        recon::BundleSource pose; pose.bundlePath = argv[3];
        std::vector<recon::CameraView> cams; recon::PointCloud sparse;
        if (!pose.estimate(imagePaths, cams, sparse)) { std::cerr << "pose load failed\n"; return 1; }
        std::cout << "cameras: " << cams.size() << "\n";

        recon::ProjectiveTexturer tex;
        tex.texture(mesh, imagePaths, cams);
        if (mesh.save(const_cast<char*>(outPath.c_str())) == 0) std::cout << "written " << outPath << " (+ .mtl)\n";
        else std::cerr << "save failed\n";
        return 0;
    }

    // Validation étape 11 : cube SYNTHÉTIQUE -> Poisson -> évaluation vs cube analytique.
    if (argc > 1 && std::string(argv[1]) == "--eval-cube")
    {
#ifdef CG_HAS_POISSON
        const int depth = (argc > 2) ? std::stoi(argv[2]) : 7;
        const int res   = (argc > 3) ? std::stoi(argv[3]) : 32;
        recon::PointCloud cloud = denseCubeCloud(res);
        std::cout << "cube synthetique: " << cloud.size() << " pts orientes (res " << res << "/face)\n";

        recon::PoissonReconstructor rec; rec.depth = depth;
        Mesh* m = rec.reconstruct(cloud);
        if (!m) { std::cerr << "Poisson failed\n"; return 1; }
        std::cout << "Poisson depth " << depth << " -> " << m->GetNVertices() << " v, " << m->GetNFaces() << " f\n";

        Mesh ref; buildUnitCube(ref);
        recon::HausdorffEvaluator ev;
        recon::Metrics mt = ev.evaluate(*m, ref);
        std::cout << "--- evaluation vs cube analytique (cote 1.0) ---\n";
        std::cout << "  Hausdorff (max) : " << mt.hausdorff << "\n";
        std::cout << "  moyenne         : " << mt.meanError << "\n";
        std::cout << "  RMS             : " << mt.rmsError << "\n";
        std::cout << "  p95             : " << mt.p95Error << "\n";
        std::cout << "  relatif (/diag) : " << mt.hausdorffRelative << "\n";

        // Heatmap : couleur par sommet selon la distance à la référence (plafond p95).
        std::vector<float> d = mesh_pointwise_distance(*m, ref);
        const float cap = (mt.p95Error > 1e-6f) ? mt.p95Error : 1e-6f;
        std::vector<unsigned char> rgb(3 * m->GetNVertices());
        for (unsigned i = 0; i < m->GetNVertices(); ++i)
        {
            float t = d[i] / cap; if (t < 0) t = 0; if (t > 1) t = 1;
            float r, g, b;
            if (t < 0.5f) { r = 0; g = 2*t; b = 1 - 2*t; }               // bleu -> vert
            else          { r = 2*(t-0.5f); g = 1 - 2*(t-0.5f); b = 0; }  // vert -> rouge
            rgb[3*i]   = (unsigned char)(255.f*r);
            rgb[3*i+1] = (unsigned char)(255.f*g);
            rgb[3*i+2] = (unsigned char)(255.f*b);
        }
        writeColoredPly("eval_cube_heatmap.ply", *m, rgb);
        std::cout << "written eval_cube_heatmap.ply (heatmap d'erreur, plafond p95=" << cap << ")\n";
        delete m;
        return 0;
#else
        std::cerr << "Poisson non active (ENABLE_POISSON)\n"; return 1;
#endif
    }

    if (argc > 1 && std::string(argv[1]) == "--multiview")
    {
        if (argc < 6) { std::cerr << "usage: reconstruction-cli --multiview <model.onnx> <bundle.out> <list.txt> <images_dir> [size] [poisson_depth]\n"; return 1; }
        const int size = (argc > 6) ? std::stoi(argv[6]) : 518;
        const int poissonDepth = (argc > 7) ? std::stoi(argv[7]) : 8;
        return run_multiview(argv[2], argv[3], argv[4], argv[5], size, poissonDepth);
    }

    const std::string model   = (argc > 1) ? argv[1] : "depth_anything_v2_vitb_dynamic.onnx";
    const std::string imgPath = (argc > 2) ? argv[2] : "";
    const int         size    = (argc > 3) ? std::stoi(argv[3]) : 518;
    if (imgPath.empty()) { std::cerr << "usage: reconstruction-cli <model.onnx> <image> [size]  |  --multiview ...\n"; return 1; }
    return run_single(model, imgPath, size);
}
