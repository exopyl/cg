#include "recon_surface_heightmap.h"

#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace recon {

static float dist3(const float* a, const float* b)
{
    const float dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

Mesh* HeightmapReconstructor::reconstruct(const PointCloud& cloud)
{
    if (!cloud.organized())
        return nullptr;

    const int    W = cloud.width, H = cloud.height;
    const float* P = cloud.positions.data();
    auto V     = [&](int gx, int gy) -> const float* { return P + 3 * ((size_t)gy * W + gx); };
    auto valid = [&](int gx, int gy) { return cloud.isValid((size_t)gy * W + gx); };

    // Médiane des longueurs d'arêtes voisines (paires valides) -> seuil de coupe.
    std::vector<float> edges;
    edges.reserve((size_t)W * H);
    for (int gy = 0; gy < H; ++gy)
        for (int gx = 0; gx < W; ++gx)
        {
            if (gx + 1 < W && valid(gx, gy) && valid(gx + 1, gy))
                edges.push_back(dist3(V(gx, gy), V(gx + 1, gy)));
            if (gy + 1 < H && valid(gx, gy) && valid(gx, gy + 1))
                edges.push_back(dist3(V(gx, gy), V(gx, gy + 1)));
        }
    if (edges.empty())
        return nullptr;
    std::nth_element(edges.begin(), edges.begin() + edges.size() / 2, edges.end());
    const float median = edges[edges.size() / 2];
    const float thr    = edgeFactor * (median > 0.f ? median : 1.f);

    // Sommets compactés (seuls ceux utilisés par des cellules conservées).
    std::vector<int>          remap((size_t)W * H, -1);
    std::vector<float>        verts;
    std::vector<unsigned int> faces;
    auto getIdx = [&](int gx, int gy) -> int {
        const size_t k = (size_t)gy * W + gx;
        if (remap[k] < 0)
        {
            remap[k] = (int)(verts.size() / 3);
            const float* p = V(gx, gy);
            verts.push_back(p[0]); verts.push_back(p[1]); verts.push_back(p[2]);
        }
        return remap[k];
    };
    auto cellOk = [&](int gx, int gy) -> bool {
        if (!(valid(gx, gy) && valid(gx + 1, gy) && valid(gx, gy + 1) && valid(gx + 1, gy + 1)))
            return false;
        const float* a = V(gx, gy);   const float* b = V(gx + 1, gy);
        const float* c = V(gx, gy + 1); const float* d = V(gx + 1, gy + 1);
        return !(dist3(a, b) > thr || dist3(a, c) > thr || dist3(b, d) > thr ||
                 dist3(c, d) > thr || dist3(a, d) > thr);
    };

    for (int gy = 0; gy < H - 1; ++gy)
        for (int gx = 0; gx < W - 1; ++gx)
        {
            if (!cellOk(gx, gy)) continue;
            const int v00 = getIdx(gx, gy),     v01 = getIdx(gx + 1, gy);
            const int v10 = getIdx(gx, gy + 1), v11 = getIdx(gx + 1, gy + 1);
            faces.push_back(v10); faces.push_back(v11); faces.push_back(v00);
            faces.push_back(v11); faces.push_back(v01); faces.push_back(v00);
        }

    if (verts.empty() || faces.empty())
        return nullptr;

    Mesh* mesh = new Mesh();
    mesh->SetVertices((unsigned)(verts.size() / 3), verts.data());
    mesh->SetFaces((unsigned)(faces.size() / 3), 3, faces.data());
    mesh->ComputeNormals();
    return mesh;
}

} // namespace recon
