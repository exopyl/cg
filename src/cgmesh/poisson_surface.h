#pragma once
//
// Reconstruction de surface de Poisson (Kazhdan) — algorithme générique, autonome,
// indépendant du pipeline de reconstruction (cf. bvh.h, mesh_metrics.h, icp.h).
//
// La couche PoissonRecon (master vendorisé) est compilée DANS cgmesh derrière
// ENABLE_POISSON / CG_HAS_POISSON (in-process, pas d'exe ; les en-têtes PoissonRecon
// ne fuient pas hors du .cpp). Produit un mesh fermé depuis un nuage à NORMALES
// ORIENTÉES. L'adaptateur reconstruction est recon::PoissonReconstructor.
//
#include <cstddef>

#ifdef CG_HAS_POISSON

class Mesh;

struct PoissonParams
{
    int   depth        = 8;     // profondeur d'octree (résolution) ; 8 = défaut PoissonRecon
    float trimQuantile = 0.f;   // 0 = pas de trim ; sinon coupe les triangles dont la densité
                                // moyenne est sous ce quantile (cf. SurfaceTrimmer)
};

// Reconstruit un mesh depuis `n` points orientés (`positions` et `normals`, 3*n
// chacun en x y z). Renvoie nullptr si n<4, pointeurs nuls, ou échec du solveur.
Mesh* poisson_reconstruct(const float* positions, const float* normals, size_t n,
                          const PoissonParams& params = PoissonParams());

#endif // CG_HAS_POISSON
