#include "recon_fuser.h"

namespace recon {

PointCloud ConcatFuser::fuse(const std::vector<PointCloud>& clouds)
{
    PointCloud out;

    size_t total = 0;
    for (const auto& c : clouds) total += c.positions.size();
    out.positions.reserve(total);

    // Normales/couleurs reportées seulement si présentes dans TOUS les nuages.
    bool allNormals = !clouds.empty(), allColors = !clouds.empty();
    for (const auto& c : clouds)
    {
        if (!c.hasNormals()) allNormals = false;
        if (!c.hasColors())  allColors  = false;
    }

    for (const auto& c : clouds)
    {
        out.positions.insert(out.positions.end(), c.positions.begin(), c.positions.end());
        if (allNormals)
            out.normals.insert(out.normals.end(), c.normals.begin(), c.normals.end());
        if (allColors)
            out.colors.insert(out.colors.end(), c.colors.begin(), c.colors.end());
    }
    return out;
}

} // namespace recon
