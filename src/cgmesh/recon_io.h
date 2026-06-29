#pragma once
//
// Reconstruction — E/S de nuages de points.
//
#include <string>

#include "recon_types.h"

namespace recon {

// Charge un nuage PLY **binary_little_endian** dans `out` (positions, + normales et
// couleurs si les propriétés nx/ny/nz et red/green/blue sont présentes). Conçu pour
// le `fused.ply` de COLMAP (MVS dense : x,y,z,nx,ny,nz,red,green,blue) mais lit un
// ordre de propriétés quelconque. Renvoie false si échec / format non géré.
bool loadPointCloudPly(const std::string& path, PointCloud& out);

// Écrit un nuage PLY ASCII (positions + normales/couleurs si présentes). Pour
// exporter/inspecter le nuage effectivement passé à la reconstruction.
bool savePointCloudPly(const std::string& path, const PointCloud& cloud);

// Retire le plus grand plan du nuage (RANSAC) — typiquement le plan de table sous
// un objet, qui sinon domine la reconstruction de surface. `distThreshRel` : tolérance
// d'inlier en fraction de la diagonale de la bbox. Préserve normales/couleurs des
// points conservés. Renvoie le nombre de points retirés (0 si aucun plan dominant).
std::size_t removeDominantPlane(PointCloud& cloud, float distThreshRel = 0.015f,
                                int iterations = 300);

} // namespace recon
