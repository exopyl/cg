//
// Reconstruction 3D — orchestrateur (Phase 0). Câblage des étapes uniquement.
//
#include "recon_pipeline.h"

#include "mesh.h"
#include "recon_io.h"
#include "../cgimg/cgimg.h"

namespace recon {

Mesh* Pipeline::runSingleView(Img& image)
{
    // Étapes minimales requises pour la voie mono-vue.
    if (!depthSource || !unprojector || !surfaceReconstructor)
        return nullptr;

    if (preprocessor)
        preprocessor->process(image);                 // [1]

    DepthMap depth = depthSource->estimate(image);     // [2]

    // Mono-vue : une seule caméra, repère caméra (pas de pose ni d'échelle).
    CameraView cam;
    cam.w = depth.w;
    cam.h = depth.h;

    PointCloud cloud = unprojector->unproject(depth, cam);  // [5]

    if (normalEstimator)
        normalEstimator->estimate(cloud);              // [7]

    if (!exportCloudPath.empty())
        savePointCloudPly(exportCloudPath, cloud);     // export du nuage reconstruit

    Mesh* mesh = surfaceReconstructor->reconstruct(cloud);  // [8]

    if (mesh && meshPostProcessor)
        meshPostProcessor->process(*mesh);             // [9]

    return mesh;
}

Mesh* Pipeline::runMultiView(const std::vector<std::string>& imagePaths,
                             const std::vector<Img*>&         images)
{
    if (!poseSource || !depthSource || !unprojector || !fuser || !surfaceReconstructor)
        return nullptr;

    std::vector<CameraView> cameras;
    PointCloud              sparse;
    if (!poseSource->estimate(imagePaths, cameras, sparse))   // [3]
        return nullptr;

    std::vector<PointCloud> clouds;
    const std::size_t n = (images.size() < cameras.size()) ? images.size()
                                                            : cameras.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!images[i])
            continue;
        Img& img = *images[i];

        if (preprocessor)
            preprocessor->process(img);                // [1]

        DepthMap depth = depthSource->estimate(img);   // [2]

        if (depthScaler)
            depthScaler->align(depth, cameras[i], sparse);  // [4]

        clouds.push_back(unprojector->unproject(depth, cameras[i]));  // [5]
    }

    PointCloud fused = fuser->fuse(clouds);            // [6]

    if (normalEstimator)
        normalEstimator->estimate(fused);              // [7]

    if (!exportCloudPath.empty())
        savePointCloudPly(exportCloudPath, fused);     // export du nuage fusionné reconstruit

    Mesh* mesh = surfaceReconstructor->reconstruct(fused);  // [8]

    if (mesh && meshPostProcessor)
        meshPostProcessor->process(*mesh);             // [9]

    if (mesh && texturer)
        texturer->texture(*mesh, imagePaths, cameras); // [10]

    return mesh;
}

} // namespace recon
