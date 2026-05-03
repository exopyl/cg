// metaballs_full.cpp
//
// Compilation : g++ -std=c++17 -O2 metaballs_full.cpp -o metaballs_full
// Exécution   : ./metaballs_full
// Sortie      : plusieurs fichiers .obj (un par étape) + une heightmap .pgm
//
// =====================================================================
// IMPLÉMENTATION C++ DES 10 ÉTAPES DU PLAN TAKAYAMA 2024
// https://dl.acm.org/doi/10.1145/3681756.3697912
//
//   0-1-2  Setup, metaball unique, somme classique
//   3      "Stepwise" = équivalent CPU via accumulation MAX (au lieu de SUM)
//   4      Foils par atténuation angulaire cos²(Nθ/2)^k
//   5      Forme de base polygonale (au lieu du cercle)
//   6      Cusps via le paramètre cusp_sharpness (k > 1)
//   7      Mouchettes : metaball avec une trajectoire (≥ 2 points)
//   8      Composition dans un arc en tiers-point équilatéral
//   9      Interpolation entre deux jeux de motifs
//   10     Export heightmap PGM (le champ de densité brut)
//
// La fonction de meshing (marching squares + extrusion) reste la même
// pour toutes les étapes ; seul le champ de densité change.
// =====================================================================

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

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

// =====================================================================
// MOTIF — un élément décoratif paramétrable
// =====================================================================
struct Motif {
    // Étapes 1-2 / 7
    std::vector<Vec2> trajectory = {{0.0f, 0.0f}};
    // 1 point  : metaball statique
    // ≥2 points: mouchette / dague (métaball le long du chemin)

    float radius = 50.0f;             // taille de base

    // Étape 5 : forme de base
    int   polygon_sides       = 0;    // 0 = cercle ; 3+ = polygone régulier
    float polygon_orientation = 0.0f; // rotation du polygone (radians)

    // Étapes 4 et 6 : foils + cusps
    int   foil_count          = 0;    // 0 = pas d'atténuation ; 3 = trèfle ; ...
    float foil_orientation    = 0.0f; // rotation du motif foil (radians)
    float cusp_sharpness      = 1.0f; // exposant ; >1 = cusps plus marqués
};

// =====================================================================
// PARAMÈTRES GLOBAUX DU CHAMP
// =====================================================================
enum AccumulationMode {
    ACCUM_SUM,  // f = Σ fᵢ  (étapes 1-2, fusion lisse)
    ACCUM_MAX,  // f = max fᵢ (étape 3, lobes individuels préservés)
};

struct ArchBoundary {
    bool enabled = false;
    Vec2 springline_left  = {0,0};   // naissance gauche
    Vec2 springline_right = {0,0};   // naissance droite
    // Apex calculé : milieu des naissances + (largeur·√3/2) en y (équilatéral)
};

struct FieldParams {
    AccumulationMode accumulation = ACCUM_SUM;
    float            threshold    = 1.2f;
    ArchBoundary     arch         = {};
};

// =====================================================================
// MATH HELPERS
// =====================================================================
static float mod_pos(float a, float b) {
    float r = std::fmod(a, b);
    return r < 0 ? r + b : r;
}

// Étape 5 : "distance au carré" généralisée (cercle ou polygone régulier)
// Pour un polygone à n côtés inscrit autour d'un cercle de rayon r,
// les iso-courbes de cette fonction matchent la forme du polygone.
static float base_dist2(float dx, float dy, int n_sides, float orientation) {
    float r2 = dx*dx + dy*dy;
    if (n_sides < 3) return r2;
    float ang_per = 2.0f * (float)M_PI / n_sides;
    float theta = std::atan2(dy, dx) - orientation;
    float local = mod_pos(theta + ang_per * 0.5f, ang_per) - ang_per * 0.5f;
    float c = std::cos(local);
    return r2 * c * c;
}

// Étape 4 : atténuation angulaire = cos²(N·θ/2)^cusp
// Donne N lobes répartis uniformément sur [0, 2π], continue en 2π pour tout N.
static float foil_attenuation(float dx, float dy,
                              int foil_count,
                              float orientation,
                              float cusp)
{
    if (foil_count <= 0) return 1.0f;
    float theta = std::atan2(dy, dx) - orientation;
    float c = std::cos(foil_count * theta * 0.5f);
    float att = c * c;            // ∈ [0, 1], toujours positif
    return std::pow(att, cusp);
}

// Étape 8 : test "à l'intérieur d'un arc en tiers-point équilatéral"
//   - au-dessus de la naissance
//   - dans les deux arcs de cercle (rayon = largeur)
static bool inside_arch(float x, float y, const ArchBoundary& a) {
    if (!a.enabled) return true;
    float base_y = std::min(a.springline_left.y, a.springline_right.y);
    if (y < base_y) return false;
    float W = std::abs(a.springline_right.x - a.springline_left.x);
    float R = W; // équilatéral : R = largeur
    float dxl = x - a.springline_left.x;
    float dyl = y - a.springline_left.y;
    if (dxl*dxl + dyl*dyl > R*R) return false;
    float dxr = x - a.springline_right.x;
    float dyr = y - a.springline_right.y;
    if (dxr*dxr + dyr*dyr > R*R) return false;
    return true;
}

// =====================================================================
// CHAMP DE DENSITÉ
// =====================================================================
static float motif_density(const Motif& m, float x, float y) {
    // Sur la trajectoire : on prend le MAX (union des metaballs ponctuelles)
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

float total_density(const std::vector<Motif>& motifs,
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

// =====================================================================
// MESH + MARCHING SQUARES + EXTRUSION (identique au fichier précédent)
// =====================================================================
struct Mesh {
    std::vector<Vec3> verts;
    std::vector<std::array<int, 3>> tris;
    int  add_vert(Vec3 v)              { verts.push_back(v); return (int)verts.size()-1; }
    void add_tri (int a, int b, int c) { tris.push_back({a,b,c}); }
};

// Tables MS — codes 0-3 = coins (p00, p10, p11, p01), 4-7 = arêtes (bot, right, top, left)
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

Mesh extrude(const std::vector<Motif>& motifs,
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


// =====================================================================
// CONSTRUCTEURS DE MOTIFS (helpers pour le main)
// =====================================================================
Motif circle_motif(Vec2 c, float r) {
    Motif m; m.trajectory = {c}; m.radius = r; return m;
}
Motif foil_motif(Vec2 c, float r, int n_foils,
                 float orientation = 0.0f, float cusp = 1.5f) {
    Motif m = circle_motif(c, r);
    m.foil_count = n_foils;
    m.foil_orientation = orientation;
    m.cusp_sharpness = cusp;
    return m;
}
Motif polygon_motif(Vec2 c, float r, int sides, float orientation = 0.0f) {
    Motif m = circle_motif(c, r);
    m.polygon_sides = sides;
    m.polygon_orientation = orientation;
    return m;
}
Motif mouchette_motif(std::vector<Vec2> path, float r) {
    Motif m; m.trajectory = path; m.radius = r; return m;
}

// Étape 9 : interpolation linéaire entre deux jeux de motifs
// (suppose même nombre et même structure de trajectoire ; foil_count / polygon_sides
// pris depuis A par défaut — peuvent être ajustés manuellement frame par frame)
std::vector<Motif> interpolate_motifs(const std::vector<Motif>& A,
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
