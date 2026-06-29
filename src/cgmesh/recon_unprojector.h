#pragma once
//
// Reconstruction — étape [5] : déprojection sténopé (implémentation concrète).
//
#include "recon_interfaces.h"

namespace recon {

// Déprojette une carte de profondeur en nuage de points. Applique la pose de la
// caméra (R,T) pour sortir en repère monde — `P = R^T·(Pc - T)` (convention
// Bundler) ; une CameraView par défaut (R=identité, T=0) donne le repère caméra
// (mono-vue). Gère la profondeur en disparité relative (Depth Anything) :
// Z = 1/(disparité_normalisée + eps), ou métrique si `isDisparity == false`.
class PinholeUnprojector : public IUnprojector
{
public:
    float fovDeg  = 55.f;  // utilisé seulement si cam.fx == 0 (mono-vue non calibrée)
    float eps     = 0.1f;  // évite Z -> inf au fond (disparité ~ 0)
    float dispMin = 0.f;   // seuil premier-plan dans [0,1] (0 = tout garder)
    int   stride  = 2;     // sous-échantillonnage pixel (1 = tous les pixels)
    bool  organized = false; // true -> nuage organisé (grille + validité) pour heightmap
    bool  withNormals = false; // true -> estime des normales orientées (pour Poisson) ;
                               // ignoré si organized (cf. HeightmapReconstructor)

    PointCloud unproject(const DepthMap& depth, const CameraView& cam) override;
};

} // namespace recon
