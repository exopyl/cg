#pragma once
//
// Reconstruction — étape [2] : source de profondeur via ONNX Runtime.
//
// La couche ONNX vit dans cgmesh derrière ENABLE_ONNX (cf. reconstruction.md §9).
// La classe n'existe QUE si CG_HAS_ONNX est défini (sinon cgmesh ne dépend pas
// d'ONNX Runtime). Le header n'inclut PAS les en-têtes ORT (pimpl) : il reste
// includable même quand ONNX est désactivé.
//
#include "recon_interfaces.h"

#ifdef CG_HAS_ONNX

#include <string>

namespace recon {

// Inférence de profondeur monoculaire (ex. Depth Anything V2) via un modèle .onnx.
// Sortie : DepthMap (disparité relative). Pré-traitement ImageNet + NCHW.
class OnnxDepthSource : public IDepthSource
{
public:
    // inputSize : côté carré du réseau, ramené au multiple de 14 inférieur.
    OnnxDepthSource(const std::string& modelPath, int inputSize = 518);
    ~OnnxDepthSource() override;

    bool     ok() const;             // false si le modèle n'a pas pu être chargé
    DepthMap estimate(const Img& image) override;

private:
    struct Impl;                     // contient Ort::Env / Ort::Session (cf. .cpp)
    Impl* m_impl = nullptr;
    int   m_inputSize = 518;
};

} // namespace recon

#endif // CG_HAS_ONNX
