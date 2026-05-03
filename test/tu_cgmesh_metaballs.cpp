#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <cmath>
#include <string>
#include <utility>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// We redefine the structures and functions here to make the test self-contained,
// as src/cgmesh/metaballs.cpp seems to be a standalone demo file with its own definitions.

namespace MetaballsTest {

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

struct Motif {
    std::vector<Vec2> trajectory = {{0.0f, 0.0f}};
    float radius = 50.0f;
    int   polygon_sides       = 0;
    float polygon_orientation = 0.0f;
    int   foil_count          = 0;
    float foil_orientation    = 0.0f;
    float cusp_sharpness      = 1.0f;
};

enum AccumulationMode {
    ACCUM_SUM,
    ACCUM_MAX,
};

struct ArchBoundary {
    bool enabled = false;
    Vec2 springline_left  = {0,0};
    Vec2 springline_right = {0,0};
};

struct FieldParams {
    AccumulationMode accumulation = ACCUM_SUM;
    float            threshold    = 1.2f;
    ArchBoundary     arch         = {};
};

static float mod_pos(float a, float b) {
    float r = std::fmod(a, b);
    return r < 0 ? r + b : r;
}

static float base_dist2(float dx, float dy, int n_sides, float orientation) {
    float r2 = dx*dx + dy*dy;
    if (n_sides < 3) return r2;
    float ang_per = 2.0f * (float)M_PI / n_sides;
    float theta = std::atan2(dy, dx) - orientation;
    float local = mod_pos(theta + ang_per * 0.5f, ang_per) - ang_per * 0.5f;
    float c = std::cos(local);
    return r2 * c * c;
}

static float foil_attenuation(float dx, float dy,
                              int foil_count,
                              float orientation,
                              float cusp)
{
    if (foil_count <= 0) return 1.0f;
    float theta = std::atan2(dy, dx) - orientation;
    float c = std::cos(foil_count * theta * 0.5f);
    float att = c * c;
    return std::pow(att, cusp);
}

static bool inside_arch(float x, float y, const ArchBoundary& a) {
    if (!a.enabled) return true;
    float base_y = std::min(a.springline_left.y, a.springline_right.y);
    if (y < base_y) return false;
    float W = std::abs(a.springline_right.x - a.springline_left.x);
    float R = W;
    float dxl = x - a.springline_left.x;
    float dyl = y - a.springline_left.y;
    if (dxl*dxl + dyl*dyl > R*R) return false;
    float dxr = x - a.springline_right.x;
    float dyr = y - a.springline_right.y;
    if (dxr*dxr + dyr*dyr > R*R) return false;
    return true;
}

static float motif_density(const Motif& m, float x, float y) {
    float best = 0.0f;
    for (const auto& C : m.trajectory) {
        float dx = x - C.x;
        float dy = y - C.y;
        float d2 = std::max(base_dist2(dx, dy, m.polygon_sides,
                                       m.polygon_orientation), 1.0f);
        float density = (m.radius * m.radius) / d2;
        density *= foil_attenuation(dx, dy, m.foil_count,
                                    m.foil_orientation, m.cusp_sharpness);
        if (density > best) best = density;
    }
    return best;
}

static float total_density(const std::vector<Motif>& motifs,
                    const FieldParams& params,
                    float x, float y)
{
    if (!inside_arch(x, y, params.arch)) return 0.0f;
    float result = 0.0f;
    if (params.accumulation == ACCUM_SUM) {
        for (const auto& m : motifs) result += motif_density(m, x, y);
    } else {
        for (const auto& m : motifs) {
            float d = motif_density(m, x, y);
            if (d > result) result = d;
        }
    }
    return result;
}

struct Mesh {
    std::vector<Vec3> verts;
    std::vector<std::array<int, 3>> tris;
    int  add_vert(Vec3 v)              { verts.push_back(v); return (int)verts.size()-1; }
    void add_tri (int a, int b, int c) { tris.push_back({a,b,c}); }
};

static const std::vector<std::vector<std::vector<int>>> CAP_POLYS = {
    {}, {{0,4,7}}, {{1,5,4}}, {{0,1,5,7}},
    {{2,6,5}}, {{0,4,7},{2,6,5}}, {{1,2,6,4}}, {{0,1,2,6,7}},
    {{3,7,6}}, {{0,4,6,3}}, {{1,5,4},{3,7,6}}, {{0,1,5,6,3}},
    {{7,3,2,5}}, {{0,4,5,2,3}}, {{4,1,2,3,7}}, {{0,1,2,3}},
};
static const std::vector<std::vector<std::pair<int,int>>> CONTOUR_SEGS = {
    {}, {{4,7}}, {{5,4}}, {{5,7}}, {{6,5}}, {{4,7},{6,5}}, {{6,4}}, {{6,7}},
    {{7,6}}, {{4,6}}, {{5,4},{7,6}}, {{5,6}}, {{7,5}}, {{4,5}}, {{7,4}}, {},
};

static Vec2 interp(Vec2 a, Vec2 b, float fa, float fb, float t) {
    if (std::abs(fa - fb) < 1e-6f) return a;
    float u = (t - fa) / (fb - fa);
    return { a.x + u*(b.x - a.x), a.y + u*(b.y - a.y) };
}

static Mesh extrude(const std::vector<Motif>& motifs,
             const FieldParams& params,
             float xmin, float ymin, float xmax, float ymax,
             int nx, int ny, float thickness)
{
    Mesh mesh;
    float dx = (xmax - xmin) / nx;
    float dy = (ymax - ymin) / ny;
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            Vec2 corners[4] = {
                {xmin +  i   *dx, ymin +  j   *dy},
                {xmin + (i+1)*dx, ymin +  j   *dy},
                {xmin + (i+1)*dx, ymin + (j+1)*dy},
                {xmin +  i   *dx, ymin + (j+1)*dy},
            };
            float f[4] = {
                total_density(motifs, params, corners[0].x, corners[0].y),
                total_density(motifs, params, corners[1].x, corners[1].y),
                total_density(motifs, params, corners[2].x, corners[2].y),
                total_density(motifs, params, corners[3].x, corners[3].y),
            };
            int code = 0;
            if (f[0] > params.threshold) code |= 1;
            if (f[1] > params.threshold) code |= 2;
            if (f[2] > params.threshold) code |= 4;
            if (f[3] > params.threshold) code |= 8;
            if (code == 0) continue;

            Vec2 edges[4] = {
                interp(corners[0], corners[1], f[0], f[1], params.threshold),
                interp(corners[1], corners[2], f[1], f[2], params.threshold),
                interp(corners[3], corners[2], f[3], f[2], params.threshold),
                interp(corners[0], corners[3], f[0], f[3], params.threshold),
            };
            auto xy_of = [&](int c) -> Vec2 {
                return c < 4 ? corners[c] : edges[c - 4];
            };

            for (const auto& poly : CAP_POLYS[code]) {
                int n = (int)poly.size();
                std::vector<int> top(n), bot(n);
                for (int k = 0; k < n; ++k) {
                    Vec2 p = xy_of(poly[k]);
                    top[k] = mesh.add_vert({p.x, p.y, thickness});
                    bot[k] = mesh.add_vert({p.x, p.y, 0.0f});
                }
                for (int k = 1; k < n - 1; ++k) mesh.add_tri(top[0], top[k], top[k+1]);
                for (int k = 1; k < n - 1; ++k) mesh.add_tri(bot[0], bot[k+1], bot[k]);
            }
            for (const auto& seg : CONTOUR_SEGS[code]) {
                Vec2 a = xy_of(seg.first);
                Vec2 b = xy_of(seg.second);
                int A_bot = mesh.add_vert({a.x, a.y, 0.0f});
                int A_top = mesh.add_vert({a.x, a.y, thickness});
                int B_bot = mesh.add_vert({b.x, b.y, 0.0f});
                int B_top = mesh.add_vert({b.x, b.y, thickness});
                mesh.add_tri(A_bot, B_bot, B_top);
                mesh.add_tri(A_bot, B_top, A_top);
            }
        }
    }
    return mesh;
}

static Motif circle_motif(Vec2 c, float r) {
    Motif m; m.trajectory = {c}; m.radius = r; return m;
}
static Motif foil_motif(Vec2 c, float r, int n_foils,
                 float orientation = 0.0f, float cusp = 1.5f) {
    Motif m = circle_motif(c, r);
    m.foil_count = n_foils;
    m.foil_orientation = orientation;
    m.cusp_sharpness = cusp;
    return m;
}
static Motif polygon_motif(Vec2 c, float r, int sides, float orientation = 0.0f) {
    Motif m = circle_motif(c, r);
    m.polygon_sides = sides;
    m.polygon_orientation = orientation;
    return m;
}
static Motif mouchette_motif(std::vector<Vec2> path, float r) {
    Motif m; m.trajectory = path; m.radius = r; return m;
}

static std::vector<Motif> interpolate_motifs(const std::vector<Motif>& A,
                                      const std::vector<Motif>& B, float t)
{
    std::vector<Motif> result;
    int n = (int)std::min(A.size(), B.size());
    for (int i = 0; i < n; ++i) {
        Motif m = A[i];
        int npt = (int)std::min(A[i].trajectory.size(), B[i].trajectory.size());
        m.trajectory.resize(npt);
        for (int k = 0; k < npt; ++k)
            m.trajectory[k] = {
                A[i].trajectory[k].x*(1-t) + B[i].trajectory[k].x*t,
                A[i].trajectory[k].y*(1-t) + B[i].trajectory[k].y*t
            };
        m.radius             = A[i].radius             *(1-t) + B[i].radius             *t;
        m.foil_orientation   = A[i].foil_orientation   *(1-t) + B[i].foil_orientation   *t;
        m.cusp_sharpness     = A[i].cusp_sharpness     *(1-t) + B[i].cusp_sharpness     *t;
        m.polygon_orientation= A[i].polygon_orientation*(1-t) + B[i].polygon_orientation*t;
        result.push_back(m);
    }
    return result;
}

} // namespace MetaballsTest

using namespace MetaballsTest;

// =====================================================================
// EXPORTS
// =====================================================================
void write_obj(const Mesh& mesh, const std::string& path) {
    std::ofstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return; }
    f << "# " << mesh.verts.size() << " v, " << mesh.tris.size() << " tri\n";
    for (const auto& v : mesh.verts)
        f << "v " << v.x << " " << v.y << " " << v.z << "\n";
    for (const auto& t : mesh.tris)
        f << "f " << (t[0] + 1) << " " << (t[1] + 1) << " " << (t[2] + 1) << "\n";
    std::cout << "  → " << path << " ("
        << mesh.verts.size() << " v, " << mesh.tris.size() << " tri)\n";
}

// Étape 10 : champ de densité brut → image niveau de gris PGM (texte, sans dépendance)
void write_heightmap_pgm(const std::vector<Motif>& motifs,
    const FieldParams& params,
    float xmin, float ymin, float xmax, float ymax,
    int nx, int ny, const std::string& path)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return; }
    float dx = (xmax - xmin) / (nx - 1);
    float dy = (ymax - ymin) / (ny - 1);
    // Première passe : trouver la valeur max pour normaliser
    float vmax = 0.0f;
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            float v = total_density(motifs, params, xmin + i * dx, ymin + j * dy);
            if (v > vmax) vmax = v;
        }
    if (vmax < 1e-6f) vmax = 1.0f;
    // Écriture (PGM : scanlines top-to-bottom, donc j de ny-1 à 0)
    f << "P2\n" << nx << " " << ny << "\n255\n";
    for (int j = ny - 1; j >= 0; --j) {
        for (int i = 0; i < nx; ++i) {
            float v = total_density(motifs, params, xmin + i * dx, ymin + j * dy);
            int g = (int)(255.0f * std::min(v / vmax, 1.0f));
            f << g << (i == nx - 1 ? '\n' : ' ');
        }
    }
    std::cout << "  → " << path << " (" << nx << "x" << ny << ")\n";
}


TEST(TEST_cgmesh_metaballs, stage_1_2_classical) {
    std::vector<Motif> motifs = {
        circle_motif({100, 100}, 50),
        circle_motif({180, 100}, 50),
        circle_motif({140, 170}, 50),
    };
    FieldParams p;
    p.accumulation = ACCUM_SUM;
    p.threshold = 1.2f;
    Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
    write_obj(m, "stage_1_2_classical.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_3_stepwise) {
    std::vector<Motif> motifs = {
        circle_motif({100, 100}, 50),
        circle_motif({180, 100}, 50),
        circle_motif({140, 170}, 50),
    };
    FieldParams p;
    p.accumulation = ACCUM_MAX;
    p.threshold = 1.2f;
    Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
    write_obj(m, "stage_3_stepwise.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_4_foils) {
    const float PI = (float)M_PI;
    for (int n : {3, 4, 5}) {
        std::vector<Motif> motifs = { foil_motif({140,140}, 80, n, PI/2, 1.5f) };
        FieldParams p; p.accumulation = ACCUM_MAX; p.threshold = 1.0f;
        Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
        write_obj(m, "stage_4_foils_" + std::to_string(n) + ".obj");

        EXPECT_GT(m.verts.size(), 0);
        EXPECT_GT(m.tris.size(), 0);
    }
}

TEST(TEST_cgmesh_metaballs, stage_5_hexagonal) {
    std::vector<Motif> motifs = { polygon_motif({140,140}, 80, 6, 0.0f) };
    FieldParams p; p.threshold = 1.2f;
    Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
    write_obj(m, "stage_5_hexagonal.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_6_cusps) {
    const float PI = (float)M_PI;
    std::vector<Motif> motifs = { foil_motif({140,140}, 90, 3, PI/2, 4.0f) };
    FieldParams p; p.threshold = 0.5f;
    Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
    write_obj(m, "stage_6_cusps.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_7_mouchette) {
    const float PI = (float)M_PI;
    std::vector<Vec2> path;
    for (int i = 0; i < 10; ++i) { // Reduced steps for faster test
        float t = (float)i / 9;
        float x = 50 + t * 180;
        float y = 100 + 70 * std::sin(t * PI);
        path.push_back({x, y});
    }
    std::vector<Motif> motifs = { mouchette_motif(path, 25) };
    FieldParams p; p.accumulation = ACCUM_MAX; p.threshold = 1.2f;
    Mesh m = extrude(motifs, p, 0,0, 280,280, 20,20, 15.0f);
    write_obj(m, "stage_7_mouchette.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_8_arch) {
    const float PI = (float)M_PI;
    std::vector<Motif> motifs = {
        foil_motif({140, 200}, 35, 3, PI/2,  2.0f),
        foil_motif({140, 130}, 30, 4, PI/4,  1.5f),
        foil_motif({ 90,  90}, 25, 3, PI/6,  1.5f),
        foil_motif({190,  90}, 25, 3, -PI/6, 1.5f),
    };
    FieldParams p;
    p.accumulation = ACCUM_MAX;
    p.threshold = 1.0f;
    p.arch.enabled = true;
    p.arch.springline_left  = { 40, 30};
    p.arch.springline_right = {240, 30};
    Mesh m = extrude(motifs, p, 0,0, 280,260, 20,20, 15.0f);
    write_obj(m, "stage_8_arch.obj");

    EXPECT_GT(m.verts.size(), 0);
    EXPECT_GT(m.tris.size(), 0);
}

TEST(TEST_cgmesh_metaballs, stage_9_interp) {
    const float PI = (float)M_PI;
    std::vector<Motif> A = { foil_motif({140,140}, 80, 3, 0.0f, 1.5f) };
    std::vector<Motif> B = { foil_motif({140,140}, 80, 5, PI/5, 2.5f) };
    for (int frame = 0; frame < 3; ++frame) {
        float t = frame / 2.0f;
        std::vector<Motif> motifs = interpolate_motifs(A, B, t);
        motifs[0].foil_count = 3 + frame;
        FieldParams p; p.threshold = 1.0f;
        Mesh m = extrude(motifs, p, 0, 0, 280, 280, 150, 150, 15.0f);
        write_obj(m, "stage_9_interp_" + std::to_string(frame) + ".obj");

        EXPECT_GT(m.verts.size(), 0);
        EXPECT_GT(m.tris.size(), 0);
    }
}

TEST(TEST_cgmesh_metaballs, stage_10_heightmap_pgm) {
    const float PI = (float)M_PI;
    std::vector<Motif> motifs = { foil_motif({140,140}, 80, 4, PI / 4, 1.5f) };
    FieldParams p; p.threshold = 1.2f;
    write_heightmap_pgm(motifs, p, 0, 0, 280, 280, 280, 280, "stage_10_heightmap.pgm");
}
