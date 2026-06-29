#pragma once
//
// Reconstruction — étape [8], variante : Poisson (Kazhdan).
//
// Adaptateur MINCE : implémente ISurfaceReconstructor en déballant le PointCloud
// et en appelant l'algorithme générique poisson_reconstruct() (poisson_surface.h),
// qui porte la couche PoissonRecon vendorisée. Présent uniquement sous CG_HAS_POISSON.
//
#include "recon_interfaces.h"

#ifdef CG_HAS_POISSON

class Mesh;

namespace recon {

class PoissonReconstructor : public ISurfaceReconstructor
{
public:
    int   depth = 8;            // profondeur d'octree (résolution) ; 8 = défaut PoissonRecon
    float trimQuantile = 0.f;   // 0 = pas de trim ; sinon coupe les triangles dont la densité
                                // moyenne est sous ce quantile (ex. 0.4) — supprime les
                                // « ballons » d'extrapolation basse densité (cf. SurfaceTrimmer)

    // Requiert cloud.hasNormals() (normales orientées). Renvoie nullptr sinon / si vide.
    Mesh* reconstruct(const PointCloud& cloud) override;
};

} // namespace recon

#endif // CG_HAS_POISSON
