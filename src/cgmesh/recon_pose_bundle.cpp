#include "recon_pose_bundle.h"

#include "bundle.h"
#include "bundle_camera.h"
#include "mesh.h"
#include "../cgimg/cgimg.h"

namespace recon {

bool BundleSource::estimate(const std::vector<std::string>& imagePaths,
                            std::vector<CameraView>&         cameras,
                            PointCloud&                      sparse)
{
    if (bundlePath.empty())
        return false;

    Bundle bundle;
    if (bundle.Load(const_cast<char*>(bundlePath.c_str())) != 0)
        return false;

    cameras.clear();
    for (unsigned int i = 0; i < bundle.cameras_n; ++i)
    {
        BundleCamera* bc = bundle.cameras[i];
        if (!bc)
            continue;

        CameraView v;
        // Dimensions image -> point principal (Bundle ne les renseigne pas).
        int w = 0, h = 0;
        if (i < imagePaths.size())
        {
            Img img;
            if (img.load(imagePaths[i].c_str()) == 0)
            {
                w = (int)img.width();
                h = (int)img.height();
            }
        }
        v.w = w; v.h = h;
        v.fx = v.fy = bc->f_pxl;          // focale en pixels (format Bundler)
        v.cx = 0.5f * w; v.cy = 0.5f * h;
        v.k1 = bc->k1; v.k2 = bc->k2;
        // Convention Bundler : X_cam = R·X + T, R = matrice monde->caméra telle
        // qu'écrite dans le .out. MAIS Bundle::Load applique SetInverse() à R
        // (bc->R = R_fichier^T). On transpose donc à la recopie pour restituer
        // la vraie R monde->caméra, attendue par le texturer (Pc = R·X+T) ET
        // l'unprojector (P_monde = R^T·(Pc−T)). Vérifié vs pose COLMAP : exact.
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                v.R[r * 3 + c] = bc->R.at(c, r);
        v.T[0] = bc->T[0]; v.T[1] = bc->T[1]; v.T[2] = bc->T[2];
        cameras.push_back(v);
    }

    // Nuage épars (les points 3D du bundle, avec couleurs si présentes).
    sparse.positions.clear();
    sparse.colors.clear();
    if (bundle.mesh)
    {
        Mesh* m = bundle.mesh;
        sparse.positions.assign(m->m_pVertices.begin(), m->m_pVertices.end());
        if (m->m_pVertexColors.size() == m->m_pVertices.size())
            sparse.colors.assign(m->m_pVertexColors.begin(), m->m_pVertexColors.end());
    }

    return !cameras.empty();
}

} // namespace recon
