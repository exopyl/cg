#pragma once
//
// Reconstruction — étape [3] : poses depuis un fichier Bundler (.out).
//
#include <string>

#include "recon_interfaces.h"

namespace recon {

// Importe les poses via cgmesh/Bundle (format Bundler). Convention : Pc = R*P + T,
// caméra vers -Z (cohérent avec recon::PinholeUnprojector en repère monde).
//
// `imagePaths` (même ordre que les caméras) sert à renseigner w/h — donc le point
// principal cx=w/2, cy=h/2 — que Bundle ne remplit pas (cf. reconstruction.md §7).
// Le nuage épars (points + couleurs) est restitué dans `sparse`.
class BundleSource : public IPoseSource
{
public:
    std::string bundlePath;   // chemin du fichier .out

    bool estimate(const std::vector<std::string>& imagePaths,
                  std::vector<CameraView>&         cameras,
                  PointCloud&                      sparse) override;
};

} // namespace recon
