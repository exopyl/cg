#pragma once
//
// Reconstruction 3D à partir d'images — INTERFACES DES ÉTAPES (Phase 0).
//
// Une interface abstraite par étape du pipeline (cf. reconstruction.md §2/§3).
// Chaque interface est la « couture » d'interchangeabilité : remplacer
// l'algorithme d'une étape = fournir une autre implémentation de la même
// interface, sans toucher aux autres. Aucune logique ici (contrats purs).
//
// Tout vit dans cgmesh. Les implémentations concrètes (ONNX, COLMAP, marching
// cubes, QEM, ...) viendront en Phase 1+ comme classes dérivées.
//
#include <string>
#include <vector>

#include "recon_types.h"

class Img;   // cgimg/image.h
class Mesh;  // cgmesh/mesh.h

namespace recon {

// [1] Prétraitement : Image -> Image (in place). Peut être l'identité.
struct IPreprocessor
{
    virtual ~IPreprocessor() = default;
    virtual void process(Img& image) = 0;
};

// [2] Source de profondeur : une vue -> carte de profondeur dense.
//     (mono-depth ONNX, stéréo, MVS... derrière la même interface)
struct IDepthSource
{
    virtual ~IDepthSource() = default;
    virtual DepthMap estimate(const Img& image) = 0;
};

// [3] Poses & calibration (SfM) : images -> caméras + nuage épars.
//     Implémentation type : adaptateur COLMAP via cgmesh/Bundle::Load.
struct IPoseSource
{
    virtual ~IPoseSource() = default;
    virtual bool estimate(const std::vector<std::string>& imagePaths,
                          std::vector<CameraView>&         cameras,
                          PointCloud&                      sparse) = 0;
};

// [4] Alignement d'échelle : recale une depth relative sur l'échelle commune,
//     à partir des points épars visibles dans la vue.
struct IDepthScaler
{
    virtual ~IDepthScaler() = default;
    virtual void align(DepthMap& depth, const CameraView& cam,
                       const PointCloud& sparse) = 0;
};

// [5] Back-projection : depth + caméra -> nuage de points (repère monde).
struct IUnprojector
{
    virtual ~IUnprojector() = default;
    virtual PointCloud unproject(const DepthMap& depth, const CameraView& cam) = 0;
};

// [6] Fusion / recalage : plusieurs nuages -> un nuage unifié.
struct IFuser
{
    virtual ~IFuser() = default;
    virtual PointCloud fuse(const std::vector<PointCloud>& clouds) = 0;
};

// [7] Estimation de normales : nuage -> nuage (+normales). (optionnel selon [8])
struct INormalEstimator
{
    virtual ~INormalEstimator() = default;
    virtual void estimate(PointCloud& cloud) = 0;
};

// [8] Reconstruction de surface : nuage -> mesh. Propriété transférée à l'appelant.
struct ISurfaceReconstructor
{
    virtual ~ISurfaceReconstructor() = default;
    virtual Mesh* reconstruct(const PointCloud& cloud) = 0;
};

// [9] Post-traitement mesh : Mesh -> Mesh (in place : décimation, lissage...).
struct IMeshPostProcessor
{
    virtual ~IMeshPostProcessor() = default;
    virtual void process(Mesh& mesh) = 0;
};

// [10] Texturation : projette les images sur le mesh (UV + matériaux).
struct ITexturer
{
    virtual ~ITexturer() = default;
    virtual void texture(Mesh&                            mesh,
                         const std::vector<std::string>&  imagePaths,
                         const std::vector<CameraView>&   cameras) = 0;
};

// [11] Évaluation : mesh vs référence -> métriques.
struct IEvaluator
{
    virtual ~IEvaluator() = default;
    virtual Metrics evaluate(Mesh& mesh, Mesh& reference) = 0;
};

} // namespace recon
