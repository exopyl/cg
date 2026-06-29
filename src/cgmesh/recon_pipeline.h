#pragma once
//
// Reconstruction 3D à partir d'images — ORCHESTRATEUR (Phase 0).
//
// Le Pipeline enchaîne les étapes fournies (pointeurs NON possédés). Une étape
// laissée à nullptr est ignorée. Il ne contient AUCUN algorithme : uniquement le
// câblage des contrats (recon_interfaces.h) dans l'ordre du pipeline.
//
// Les implémentations concrètes des étapes sont injectées par l'appelant (Phase 1+).
//
#include <string>
#include <vector>

#include "recon_interfaces.h"

class Img;
class Mesh;

namespace recon {

class Pipeline
{
public:
    // Étapes (à fournir par l'appelant ; nullptr = ignorée). Non possédées.
    IPreprocessor*         preprocessor         = nullptr;
    IDepthSource*          depthSource          = nullptr;
    IPoseSource*           poseSource           = nullptr;
    IDepthScaler*          depthScaler          = nullptr;
    IUnprojector*          unprojector          = nullptr;
    IFuser*                fuser                = nullptr;
    INormalEstimator*      normalEstimator      = nullptr;
    ISurfaceReconstructor* surfaceReconstructor = nullptr;
    IMeshPostProcessor*    meshPostProcessor    = nullptr;
    ITexturer*             texturer             = nullptr;

    // Si non vide : exporte (PLY) le nuage de points effectivement passé à la
    // reconstruction de surface (utile pour inspecter/comparer avec le mesh).
    std::string exportCloudPath;

    // Voie mono-vue : une image -> mesh (étapes 1,2,5,[7],8,[9]).
    // Renvoie nullptr si une étape requise (depthSource, unprojector,
    // surfaceReconstructor) est absente.
    Mesh* runSingleView(Img& image);

    // Voie multi-vues : images + chemins -> mesh
    // (étapes 1,3,2,[4],5,6,[7],8,[9],[10]). imagePaths et images doivent être
    // dans le même ordre. Renvoie nullptr si une étape requise manque.
    Mesh* runMultiView(const std::vector<std::string>& imagePaths,
                       const std::vector<Img*>&         images);
};

} // namespace recon
