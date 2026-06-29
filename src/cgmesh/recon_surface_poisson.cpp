#include "recon_surface_poisson.h"

#ifdef CG_HAS_POISSON

#include "poisson_surface.h"

namespace recon {

Mesh* PoissonReconstructor::reconstruct(const PointCloud& cloud)
{
    // Requiert des normales orientées ; délègue à l'algorithme générique.
    if (cloud.size() < 4 || !cloud.hasNormals())
        return nullptr;

    PoissonParams p;
    p.depth        = depth;
    p.trimQuantile = trimQuantile;
    return poisson_reconstruct(cloud.positions.data(), cloud.normals.data(), cloud.size(), p);
}

} // namespace recon

#endif // CG_HAS_POISSON
