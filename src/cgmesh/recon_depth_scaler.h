#pragma once
//
// Reconstruction — étape [4] : alignement d'échelle de la profondeur.
//
#include "recon_interfaces.h"

namespace recon {

// Convertit une carte de DISPARITÉ relative (mono-depth) en PROFONDEUR MÉTRIQUE,
// par ajustement affine en espace inverse-profondeur : on projette les points
// épars (de poses connues) dans la vue, on apparie (disparité prédite ↔ 1/Z vrai),
// et on fitte 1/Z = a·disp + b (moindres carrés). La depth devient alors métrique
// et cohérente entre vues (condition d'une fusion correcte).
//
// Sans assez de points appariés (>=2) ou caméra non calibrée, la depth est laissée
// inchangée.
class AffineDepthScaler : public IDepthScaler
{
public:
    void align(DepthMap& depth, const CameraView& cam, const PointCloud& sparse) override;
};

} // namespace recon
