#pragma once
//
// Reconstruction — étape [8] : surface par champ implicite + marching cubes.
//
#include "recon_interfaces.h"

class Mesh;

namespace recon {

// Reconstruit un mesh depuis un nuage via PointCloudField (champ gaussien) +
// ImplicitSurface (marching cubes). Ne requiert PAS de normales orientées.
// Renvoie un Mesh* (propriété transférée) ou nullptr.
//
// Renvoie nullptr si le nuage est degenere ou si la triangulation echoue : grille au-dela
// de ImplicitSurface::kMaxLayerNodes noeuds par couche, ou memoire insuffisante. La
// resolution suit gridCells le long du plus grand axe : garder gridCells modere sur des
// nuages denses.
class ImplicitSurfaceReconstructor : public ISurfaceReconstructor
{
public:
    float isoDistance = 0.f;    // rayon d'iso-surface ; 0 = auto (relatif au nuage)
    int   gridCells   = 48;     // résolution marching-cubes le long du plus grand axe

    Mesh* reconstruct(const PointCloud& cloud) override;
};

} // namespace recon
