#include "recon_unprojector.h"

#include <algorithm>
#include <cmath>

namespace recon {

PointCloud PinholeUnprojector::unproject(const DepthMap& depth, const CameraView& cam)
{
    PointCloud pc;
    const int w = depth.w, h = depth.h;
    if (w <= 0 || h <= 0 || (int)depth.z.size() < w * h)
        return pc;

    // Normalisation de la disparité sur la plage observée.
    float mn = depth.z[0], mx = depth.z[0];
    for (float v : depth.z) { mn = std::min(mn, v); mx = std::max(mx, v); }
    const float range = (mx > mn) ? (mx - mn) : 1.f;

    // Intrinsèques : celles de la caméra si fournies, sinon depuis le FOV.
    const float PI = 3.14159265358979f;
    float fx = cam.fx, fy = cam.fy, cx = cam.cx, cy = cam.cy;
    if (fx <= 0.f)
    {
        fx = fy = 0.5f * w / std::tan(0.5f * fovDeg * PI / 180.f);
        cx = 0.5f * w;
        cy = 0.5f * h;
    }

    const int s = (stride < 1) ? 1 : stride;

    // Point monde pour un pixel ; false si invalide (fond / hors plage).
    auto worldAt = [&](int x, int y, float out[3]) -> bool {
        const float val = depth.z[(size_t)y * w + x];
        float Z;
        if (depth.isDisparity)
        {
            const float disp = (val - mn) / range;   // [0,1], 1 = proche
            if (disp < dispMin) return false;
            Z = 1.f / (disp + eps);
        }
        else
        {
            Z = val;                                  // profondeur métrique
        }
        if (Z <= 0.f) return false;
        // Repère caméra (caméra vers -Z) puis monde : P = R^T·(Pc - T).
        const float Xc = (x - cx) / fx * Z;
        const float Yc = -((y - cy) / fy) * Z;
        const float Zc = -Z;
        const float t0 = Xc - cam.T[0], t1 = Yc - cam.T[1], t2 = Zc - cam.T[2];
        out[0] = cam.R[0] * t0 + cam.R[3] * t1 + cam.R[6] * t2;
        out[1] = cam.R[1] * t0 + cam.R[4] * t1 + cam.R[7] * t2;
        out[2] = cam.R[2] * t0 + cam.R[5] * t1 + cam.R[8] * t2;
        return true;
    };

    if (organized)
    {
        const int gw = (w - 1) / s + 1, gh = (h - 1) / s + 1;
        pc.width = gw; pc.height = gh;
        pc.positions.assign((size_t)gw * gh * 3, 0.f);
        pc.valid.assign((size_t)gw * gh, 0);
        for (int gy = 0; gy < gh; ++gy)
            for (int gx = 0; gx < gw; ++gx)
            {
                float p[3];
                if (worldAt(gx * s, gy * s, p))
                {
                    const size_t idx = (size_t)gy * gw + gx;
                    pc.positions[3 * idx + 0] = p[0];
                    pc.positions[3 * idx + 1] = p[1];
                    pc.positions[3 * idx + 2] = p[2];
                    pc.valid[idx] = 1;
                }
            }
    }
    else
    {
        pc.positions.reserve((size_t)((w / s) * (h / s)) * 3);

        if (!withNormals)
        {
            for (int y = 0; y < h; y += s)
                for (int x = 0; x < w; x += s)
                {
                    float p[3];
                    if (worldAt(x, y, p))
                    {
                        pc.positions.push_back(p[0]);
                        pc.positions.push_back(p[1]);
                        pc.positions.push_back(p[2]);
                    }
                }
            return pc;
        }

        // --- avec normales orientées (estimées depuis la grille de profondeur) ---
        pc.normals.reserve((size_t)((w / s) * (h / s)) * 3);
        // Centre caméra (monde) C = -R^T·T, pour orienter les normales vers la caméra.
        float Cx, Cy, Cz;
        {
            const float t0 = -cam.T[0], t1 = -cam.T[1], t2 = -cam.T[2];
            Cx = cam.R[0]*t0 + cam.R[3]*t1 + cam.R[6]*t2;
            Cy = cam.R[1]*t0 + cam.R[4]*t1 + cam.R[7]*t2;
            Cz = cam.R[2]*t0 + cam.R[5]*t1 + cam.R[8]*t2;
        }
        for (int y = 0; y < h; y += s)
            for (int x = 0; x < w; x += s)
            {
                float p[3];
                if (!worldAt(x, y, p)) continue;

                float r[3], l[3], d[3], u[3];
                const bool hr = (x + s <  w) && worldAt(x + s, y, r);
                const bool hl = (x - s >= 0) && worldAt(x - s, y, l);
                const bool hd = (y + s <  h) && worldAt(x, y + s, d);
                const bool hu = (y - s >= 0) && worldAt(x, y - s, u);

                float tu[3], tv[3];
                bool oku = true, okv = true;
                if      (hr && hl) for (int k=0;k<3;++k) tu[k] = r[k] - l[k];
                else if (hr)       for (int k=0;k<3;++k) tu[k] = r[k] - p[k];
                else if (hl)       for (int k=0;k<3;++k) tu[k] = p[k] - l[k];
                else oku = false;
                if      (hd && hu) for (int k=0;k<3;++k) tv[k] = d[k] - u[k];
                else if (hd)       for (int k=0;k<3;++k) tv[k] = d[k] - p[k];
                else if (hu)       for (int k=0;k<3;++k) tv[k] = p[k] - u[k];
                else okv = false;

                float n[3];
                if (oku && okv)
                {
                    n[0] = tu[1]*tv[2] - tu[2]*tv[1];
                    n[1] = tu[2]*tv[0] - tu[0]*tv[2];
                    n[2] = tu[0]*tv[1] - tu[1]*tv[0];
                }
                else { n[0] = Cx - p[0]; n[1] = Cy - p[1]; n[2] = Cz - p[2]; }

                float len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
                if (len < 1e-12f) { n[0]=0; n[1]=0; n[2]=1; len=1; }
                n[0]/=len; n[1]/=len; n[2]/=len;
                // orienter vers la caméra
                if (n[0]*(Cx-p[0]) + n[1]*(Cy-p[1]) + n[2]*(Cz-p[2]) < 0.f)
                { n[0]=-n[0]; n[1]=-n[1]; n[2]=-n[2]; }

                pc.positions.push_back(p[0]); pc.positions.push_back(p[1]); pc.positions.push_back(p[2]);
                pc.normals.push_back(n[0]);   pc.normals.push_back(n[1]);   pc.normals.push_back(n[2]);
            }
    }
    return pc;
}

} // namespace recon
