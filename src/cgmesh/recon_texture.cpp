#include "recon_texture.h"

#include "mesh.h"
#include "material.h"
#include "bvh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace recon {

void ProjectiveTexturer::texture(Mesh&                           mesh,
                                 const std::vector<std::string>& imagePaths,
                                 const std::vector<CameraView>&  cameras)
{
    const size_t nc = cameras.size();

    // Un matériau-texture par image (id = index caméra) + repli gris. Noms uniques
    // (sinon export_obj écrit des `newmtl` vides -> usemtl ambigus).
    std::vector<int> matId(nc, -1);
    for (size_t i = 0; i < nc; ++i)
        if (i < imagePaths.size())
        {
            MaterialTexture* mt = new MaterialTexture(imagePaths[i].c_str());
            mt->SetName("tex" + std::to_string(i));
            matId[i] = (int)mesh.Material_Add(mt);
        }
    MaterialColor* fb = new MaterialColor(200, 200, 200);
    fb->SetName("fallback");
    const int fallback = (int)mesh.Material_Add(fb);

    // Centres caméra C = -R^T·T.
    std::vector<float> camC(3 * nc);
    for (size_t i = 0; i < nc; ++i)
    {
        const CameraView& c = cameras[i];
        const float t0=-c.T[0], t1=-c.T[1], t2=-c.T[2];
        camC[3*i+0]=c.R[0]*t0+c.R[3]*t1+c.R[6]*t2;
        camC[3*i+1]=c.R[1]*t0+c.R[4]*t1+c.R[7]*t2;
        camC[3*i+2]=c.R[2]*t0+c.R[5]*t1+c.R[8]*t2;
    }

    // On ÉCLATE le mesh : chaque face -> 3 sommets propres avec UV par sommet.
    // (cgre/BuildPolygonRenderData n'exploite que des UV vertex-parallèles.)
    const float*   Vsrc = mesh.m_pVertices.data();
    const unsigned nf   = mesh.GetNFaces();

    // BVH sur la géométrie d'entrée pour le test d'occlusion (rayon d'ombre
    // barycentre->caméra). Construit AVANT la boucle (réfère m_pVertices, qui
    // n'est ré-alloué qu'après, par SetVertices). diag = échelle scène -> epsilon.
    BVH  bvh;
    bool haveBvh = false;
    float tEps = 1e-4f;
    if (occlusion && nf > 0 && Vsrc)
    {
        bvh.build(mesh);
        haveBvh = true;
        const unsigned nv = mesh.GetNVertices();
        float mn[3]={Vsrc[0],Vsrc[1],Vsrc[2]}, mx[3]={Vsrc[0],Vsrc[1],Vsrc[2]};
        for (unsigned i=1;i<nv;++i) for (int k=0;k<3;++k)
        { float x=Vsrc[3*i+k]; if(x<mn[k])mn[k]=x; if(x>mx[k])mx[k]=x; }
        const float diag=std::sqrt((mx[0]-mn[0])*(mx[0]-mn[0])+(mx[1]-mn[1])*(mx[1]-mn[1])+(mx[2]-mn[2])*(mx[2]-mn[2]));
        tEps = (diag>0.f) ? 1e-3f*diag : 1e-4f;
    }
    // true si la face (barycentre B) est occultée vue depuis la caméra ci.
    auto occluded = [&](float bx,float by,float bz,int ci)->bool {
        if (!haveBvh) return false;
        const float dx=camC[3*ci]-bx, dy=camC[3*ci+1]-by, dz=camC[3*ci+2]-bz;
        const float dist=std::sqrt(dx*dx+dy*dy+dz*dz);
        if (dist < tEps) return false;
        Vector3f o (bx, by, bz);
        Vector3f d (dx/dist, dy/dist, dz/dist);
        const float t = bvh.nearest(o, d, tEps);   // tEps saute la face d'origine
        return (t >= 0.f && t < dist - tEps);       // un hit AVANT la caméra -> occultée
    };

    struct Cand { float facing; int ci; float u[3], v[3]; };
    std::vector<Cand> cands;
    size_t nOccludedFaces = 0;   // faces dont la 1re caméra frontale était occultée
    std::vector<float>        nverts;  nverts.reserve((size_t)nf * 9);
    std::vector<float>        nuv;     nuv.reserve((size_t)nf * 6);
    std::vector<unsigned int> nfaces;  nfaces.reserve((size_t)nf * 3);
    std::vector<int>          nmat;    nmat.reserve(nf);
    std::vector<unsigned char> ntex;   ntex.reserve(nf);

    for (unsigned fi = 0; fi < nf; ++fi)
    {
        Face* f = mesh.GetFace(fi);
        if (!f || f->GetNVertices() != 3) continue;
        const int ia=f->GetVertex(0), ib=f->GetVertex(1), ic=f->GetVertex(2);
        const float vx[3]={Vsrc[3*ia],Vsrc[3*ib],Vsrc[3*ic]};
        const float vy[3]={Vsrc[3*ia+1],Vsrc[3*ib+1],Vsrc[3*ic+1]};
        const float vz[3]={Vsrc[3*ia+2],Vsrc[3*ib+2],Vsrc[3*ic+2]};

        // normale + barycentre
        float nx=(vy[1]-vy[0])*(vz[2]-vz[0])-(vz[1]-vz[0])*(vy[2]-vy[0]);
        float ny=(vz[1]-vz[0])*(vx[2]-vx[0])-(vx[1]-vx[0])*(vz[2]-vz[0]);
        float nz=(vx[1]-vx[0])*(vy[2]-vy[0])-(vy[1]-vy[0])*(vx[2]-vx[0]);
        const float nl=std::sqrt(nx*nx+ny*ny+nz*nz);
        const float Bx=(vx[0]+vx[1]+vx[2])/3, By=(vy[0]+vy[1]+vy[2])/3, Bz=(vz[0]+vz[1]+vz[2])/3;

        int   best=-1; float U[3]={0,0,0}, Wv[3]={0,0,0};
        if (nl > 1e-12f)
        {
            const float inx=nx/nl, iny=ny/nl, inz=nz/nl;
            cands.clear();
            for (size_t ci=0; ci<nc; ++ci)
            {
                if (matId[ci] < 0) continue;
                const CameraView& cam=cameras[ci];
                if (cam.fx<=0.f || cam.w<=0 || cam.h<=0) continue;
                const float dx=camC[3*ci]-Bx, dy=camC[3*ci+1]-By, dz=camC[3*ci+2]-Bz;
                const float dl=std::sqrt(dx*dx+dy*dy+dz*dz); if (dl<1e-9f) continue;
                const float facing=(inx*dx+iny*dy+inz*dz)/dl;
                if (facing <= 0.05f) continue;
                float uu[3], vv[3]; bool ok=true;
                for (int k=0;k<3;++k)
                {
                    const float Pc0=cam.R[0]*vx[k]+cam.R[1]*vy[k]+cam.R[2]*vz[k]+cam.T[0];
                    const float Pc1=cam.R[3]*vx[k]+cam.R[4]*vy[k]+cam.R[5]*vz[k]+cam.T[1];
                    const float Pc2=cam.R[6]*vx[k]+cam.R[7]*vy[k]+cam.R[8]*vz[k]+cam.T[2];
                    if (Pc2>=-1e-6f){ ok=false; break; }
                    const float u=cam.cx+cam.fx*Pc0/(-Pc2);
                    const float v=cam.cy-cam.fy*Pc1/(-Pc2);
                    if (u<0||u>=cam.w||v<0||v>=cam.h){ ok=false; break; }
                    uu[k]=u/cam.w; vv[k]=1.f - v/cam.h;
                }
                if (!ok) continue;
                Cand c; c.facing=facing; c.ci=(int)ci;
                for(int k=0;k<3;++k){ c.u[k]=uu[k]; c.v[k]=vv[k]; }
                cands.push_back(c);
            }
            // Meilleure caméra par frontalité décroissante, mais NON occultée.
            std::sort(cands.begin(), cands.end(),
                      [](const Cand&a,const Cand&b){ return a.facing>b.facing; });
            for (size_t i=0;i<cands.size();++i)
            {
                if (occlusion && occluded(Bx,By,Bz, cands[i].ci))
                { if (i==0) ++nOccludedFaces; continue; }
                best=cands[i].ci;
                for(int k=0;k<3;++k){ U[k]=cands[i].u[k]; Wv[k]=cands[i].v[k]; }
                break;
            }
        }

        for (int k=0;k<3;++k)
        {
            nverts.push_back(vx[k]); nverts.push_back(vy[k]); nverts.push_back(vz[k]);
            nuv.push_back(U[k]);     nuv.push_back(Wv[k]);
        }
        const unsigned base=(unsigned)(nverts.size()/3) - 3;
        nfaces.push_back(base); nfaces.push_back(base+1); nfaces.push_back(base+2);
        nmat.push_back(best>=0 ? matId[best] : fallback);
        ntex.push_back(best>=0 ? 1 : 0);
    }

    if (occlusion)
        std::printf("texture: occlusion ON - %zu/%u faces dont la camera la plus frontale etait occultee (repli sur une autre vue)\n",
                    nOccludedFaces, nf);

    // Reconstruit la géométrie éclatée (UV vertex-parallèles).
    mesh.SetVertices((unsigned)(nverts.size()/3), nverts.data());
    mesh.m_pTextureCoordinates = nuv;
    mesh.m_nTextureCoordinates = (unsigned)(nuv.size()/2);
    mesh.SetFaces((unsigned)(nfaces.size()/3), 3, nfaces.data());
    for (unsigned fi=0; fi<(unsigned)nmat.size(); ++fi)
    {
        mesh.SetFaceMaterialId(fi, (unsigned)nmat[fi]);
        if (ntex[fi])
        {
            Face* f=mesh.GetFace(fi);
            f->ActivateTextureCoordinatesIndices();
            for (unsigned k=0;k<3;++k) f->SetTexCoord(k, 3*fi+k);
            f->m_bUseTextureCoordinates=true;
        }
    }
    mesh.ComputeNormals();
}

} // namespace recon
