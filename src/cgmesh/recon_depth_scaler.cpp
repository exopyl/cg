#include "recon_depth_scaler.h"

#include <cmath>
#include <vector>

namespace recon {

void AffineDepthScaler::align(DepthMap& depth, const CameraView& cam,
                              const PointCloud& sparse)
{
    const int w = depth.w, h = depth.h;
    if (w <= 0 || h <= 0 || (int)depth.z.size() < w * h)
        return;
    if (cam.fx <= 0.f || cam.fy <= 0.f)   // caméra non calibrée -> rien
        return;

    // Apparie (disparité prédite, 1/Z vrai) pour chaque point épars visible.
    std::vector<double> ds, iz;
    const size_t n = sparse.size();
    for (size_t j = 0; j < n; ++j)
    {
        const float P0 = sparse.positions[3 * j + 0];
        const float P1 = sparse.positions[3 * j + 1];
        const float P2 = sparse.positions[3 * j + 2];
        // Pc = R*P + T (R row-major monde->caméra) ; caméra vers -Z.
        const float Pc0 = cam.R[0]*P0 + cam.R[1]*P1 + cam.R[2]*P2 + cam.T[0];
        const float Pc1 = cam.R[3]*P0 + cam.R[4]*P1 + cam.R[5]*P2 + cam.T[1];
        const float Pc2 = cam.R[6]*P0 + cam.R[7]*P1 + cam.R[8]*P2 + cam.T[2];
        if (Pc2 >= -1e-6f) continue;            // derrière la caméra
        const float Ztrue = -Pc2;
        const float u = cam.cx + cam.fx * Pc0 / (-Pc2);
        const float v = cam.cy - cam.fy * Pc1 / (-Pc2);
        const int iu = (int)(u + 0.5f), iv = (int)(v + 0.5f);
        if (iu < 0 || iu >= w || iv < 0 || iv >= h) continue;
        ds.push_back(depth.z[(size_t)iv * w + iu]);
        iz.push_back(1.0 / Ztrue);
    }
    if (ds.size() < 2)
        return;

    // Moindres carrés : iz = a*ds + b.
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const int m = (int)ds.size();
    for (int i = 0; i < m; ++i)
    {
        sx += ds[i]; sy += iz[i]; sxx += ds[i]*ds[i]; sxy += ds[i]*iz[i];
    }
    const double denom = (double)m * sxx - sx * sx;
    if (std::fabs(denom) < 1e-12)
        return;
    const double a = ((double)m * sxy - sx * sy) / denom;
    const double b = (sy - a * sx) / m;

    // Applique : Z = 1/(a*disp + b) ; inverse-profondeur <= 0 -> point invalide (Z=0).
    DepthMap out;
    out.w = w; out.h = h; out.isDisparity = false;
    out.z.resize((size_t)w * h);
    for (size_t i = 0; i < depth.z.size(); ++i)
    {
        const double invZ = a * (double)depth.z[i] + b;
        out.z[i] = (invZ > 1e-4) ? (float)(1.0 / invZ) : 0.f;
    }
    depth = std::move(out);
}

} // namespace recon
