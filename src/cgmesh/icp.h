#pragma once
//
// Recalage ICP générique (point-to-surface) — algorithme de géométrie autonome,
// indépendant de la reconstruction (cf. bvh.h, mesh_metrics.h, thickness.h).
//
// Recale un nuage de points source sur la SURFACE d'un mesh cible (correspondances
// closest-point accélérées par BVH), en estimant à chaque itération la transfo
// optimale par la méthode quaternion de Horn (pas de réflexion parasite). Option
// ÉCHELLE -> recalage de SIMILARITÉ (7 DDL, ex. recon échelle arbitraire vs CAO en
// mm) ; sans échelle -> recalage RIGIDE (6 DDL, ex. fusion de nuages). Option
// trimming (ignore une fraction des pires paires) pour la robustesse aux
// recouvrements partiels / valeurs aberrantes.
//
#include <vector>

class Mesh;

struct ICPOptions
{
    int   maxIterations  = 60;
    float convergenceEps = 1e-4f;   // arrêt si |ΔRMS| < eps·RMS_initial
    bool  withScale      = false;   // true = similarité (7 DDL) ; false = rigide
    float trimFraction   = 0.0f;    // 0 = toutes les paires ; ex. 0.3 = ignore 30% des pires

    // Pose initiale (appliquée avant la 1re itération). Identité par défaut.
    float initR[9]   = {1,0,0, 0,1,0, 0,0,1};
    float initT[3]   = {0,0,0};
    float initScale  = 1.0f;
};

struct ICPResult
{
    float R[9]      = {1,0,0, 0,1,0, 0,0,1};  // q ≈ scale·R·p + T
    float T[3]      = {0,0,0};
    float scale     = 1.0f;
    float rmsError  = -1.f;   // RMS des distances de correspondance finales (unités cible)
    int   iterations = 0;
    bool  converged  = false;
};

// Recale `srcPts` (3*N, format x y z) sur la surface de `target`. Renvoie la
// transfo (scale, R, T) telle que scale·R·p + T rapproche p de la cible.
ICPResult icp_align(const std::vector<float>& srcPts,
                    Mesh&                      target,
                    const ICPOptions&          opt = ICPOptions());
