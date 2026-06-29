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
// NB : la triangulation de ImplicitSurface est plafonnée à 100000 sommets/faces
// (surface_implicit.cpp) ; garder gridCells modéré sur des nuages denses/complexes.
class ImplicitSurfaceReconstructor : public ISurfaceReconstructor
{
public:
    float isoDistance = 0.f;    // rayon d'iso-surface ; 0 = auto (relatif au nuage)
    int   gridCells   = 48;     // résolution marching-cubes le long du plus grand axe

    Mesh* reconstruct(const PointCloud& cloud) override;
};

} // namespace recon
