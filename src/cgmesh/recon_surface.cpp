#include "recon_surface.h"

#include "surface_implicit_pointcloud.h"
#include "surface_implicit.h"
#include "mesh.h"

#include <algorithm>
#include <cstdlib>

namespace recon {

Mesh* ImplicitSurfaceReconstructor::reconstruct(const PointCloud& cloud)
{
    if (cloud.size() < 4)
        return nullptr;

    const int cells = (gridCells < 8) ? 8 : gridCells;

    // Étendue brute du nuage (sans padding) -> pour l'isoDistance auto. On la
    // calcule directement (GetPaddedAABB dépend de sigma, donc de isoDistance).
    float cmin[3] = { 1e30f,  1e30f,  1e30f};
    float cmax[3] = {-1e30f, -1e30f, -1e30f};
    const size_t n = cloud.size();
    for (size_t i = 0; i < n; ++i)
        for (int a = 0; a < 3; ++a)
        {
            float v = cloud.positions[3 * i + a];
            cmin[a] = std::min(cmin[a], v);
            cmax[a] = std::max(cmax[a], v);
        }
    float rawSpan = 0.f;
    for (int a = 0; a < 3; ++a) rawSpan = std::max(rawSpan, cmax[a] - cmin[a]);
    if (rawSpan <= 0.f)
        return nullptr;

    // isoDistance : explicite si > 0, sinon ~2 mailles (relatif à l'échelle).
    float d = (isoDistance > 0.f) ? isoDistance : 2.0f * rawSpan / cells;

    // Champ scalaire gaussien depuis le nuage (octree interne).
    PointCloudField field;
    field.Build(cloud.positions.data(), (int)cloud.size());
    field.SetIsoDistance(d);

    // Boîte englobante (élargie) pour la grille marching-cubes.
    float vmin[3], vmax[3];
    field.GetPaddedAABB(vmin, vmax);
    float span = 0.f;
    for (int i = 0; i < 3; ++i) span = std::max(span, vmax[i] - vmin[i]);
    if (span <= 0.f)
        return nullptr;
    int resPerUnit = (int)(cells / span);
    if (resPerUnit < 1) resPerUnit = 1;        // évite une grille de résolution nulle

    ImplicitSurface mc;
    mc.set_bbox(vmin[0], vmin[1], vmin[2], vmax[0], vmax[1], vmax[2]);
    mc.set_value(field.GetIsoLevel());
    mc.set_resolution_per_unit(resPerUnit);
    mc.set_eval_func(&PointCloudField::Eval);
    mc.set_eval_data(&field);

    // get_triangulation alloue vertices/faces (malloc) et appelle compute() en interne.
    int            nv = 0, nf = 0;
    float*         verts = nullptr;
    unsigned int*  faces = nullptr;
    mc.get_triangulation(&nv, &verts, &nf, &faces);

    Mesh* mesh = nullptr;
    if (nv > 0 && nf > 0 && verts && faces)
    {
        mesh = new Mesh();
        mesh->SetVertices((unsigned)nv, verts);   // copie
        mesh->SetFaces((unsigned)nf, 3, faces);   // copie
        mesh->ComputeNormals();
    }
    free(verts);   // tableaux malloc'd côté ImplicitSurface, propriété au caller
    free(faces);
    return mesh;
}

} // namespace recon
