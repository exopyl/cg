#pragma once
//
// Reconstruction — étape [6] : fusion de nuages.
//
#include "recon_interfaces.h"

namespace recon {

// Fusion par simple union des nuages (concaténation). Normales/couleurs reportées
// seulement si TOUS les nuages en possèdent.
//
// À venir : downsample voxel, recalage ICP, intégration TSDF (cf. reconstruction.md
// étape 6).
class ConcatFuser : public IFuser
{
public:
    PointCloud fuse(const std::vector<PointCloud>& clouds) override;
};

} // namespace recon
