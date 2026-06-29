#include "icp.h"

#include "mesh.h"
#include "bvh.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

// --- valeurs propres d'une matrice symétrique 4x4 (Jacobi cyclique) ---
// Renvoie le vecteur propre (unitaire) associé à la plus grande valeur propre,
// interprété comme quaternion (w,x,y,z). Auto-suffisant (cgmath n'a qu'un
// solveur 3x3) et toujours convergent pour une petite matrice symétrique.
static void largestEigenvectorSym4(const double Ain[4][4], double q[4])
{
    double A[4][4]; for (int i=0;i<4;++i) for (int j=0;j<4;++j) A[i][j]=Ain[i][j];
    double V[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};

    for (int sweep=0; sweep<50; ++sweep)
    {
        double off=0;
        for (int p=0;p<4;++p) for (int qq=p+1;qq<4;++qq) off+=A[p][qq]*A[p][qq];
        if (off < 1e-20) break;

        for (int p=0;p<4;++p)
            for (int r=p+1;r<4;++r)
            {
                if (std::fabs(A[p][r]) < 1e-18) continue;
                const double theta=(A[r][r]-A[p][p])/(2.0*A[p][r]);
                const double t=(theta>=0?1.0:-1.0)/(std::fabs(theta)+std::sqrt(theta*theta+1.0));
                const double c=1.0/std::sqrt(t*t+1.0), s=t*c;
                // rotation de Givens (p,r)
                for (int k=0;k<4;++k)
                {
                    const double akp=A[k][p], akr=A[k][r];
                    A[k][p]=c*akp - s*akr;
                    A[k][r]=s*akp + c*akr;
                }
                for (int k=0;k<4;++k)
                {
                    const double apk=A[p][k], ark=A[r][k];
                    A[p][k]=c*apk - s*ark;
                    A[r][k]=s*apk + c*ark;
                }
                for (int k=0;k<4;++k)
                {
                    const double vkp=V[k][p], vkr=V[k][r];
                    V[k][p]=c*vkp - s*vkr;
                    V[k][r]=s*vkp + c*vkr;
                }
            }
    }

    int best=0; for (int i=1;i<4;++i) if (A[i][i]>A[best][best]) best=i;
    for (int i=0;i<4;++i) q[i]=V[i][best];
    double n=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n>1e-20) for (int i=0;i<4;++i) q[i]/=n; else { q[0]=1;q[1]=q[2]=q[3]=0; }
}

// quaternion (w,x,y,z) -> matrice de rotation 3x3 (row-major)
static void quatToR(const double q[4], double R[9])
{
    const double w=q[0],x=q[1],y=q[2],z=q[3];
    R[0]=1-2*(y*y+z*z); R[1]=2*(x*y-w*z);   R[2]=2*(x*z+w*y);
    R[3]=2*(x*y+w*z);   R[4]=1-2*(x*x+z*z); R[5]=2*(y*z-w*x);
    R[6]=2*(x*z-w*y);   R[7]=2*(y*z+w*x);   R[8]=1-2*(x*x+y*y);
}

static inline void matVec(const double R[9], double x,double y,double z, double o[3])
{
    o[0]=R[0]*x+R[1]*y+R[2]*z;
    o[1]=R[3]*x+R[4]*y+R[5]*z;
    o[2]=R[6]*x+R[7]*y+R[8]*z;
}
// C = A * B (3x3 row-major)
static void matMul(const double A[9], const double B[9], double C[9])
{
    for (int r=0;r<3;++r) for (int c=0;c<3;++c)
        C[r*3+c]=A[r*3]*B[c]+A[r*3+1]*B[3+c]+A[r*3+2]*B[6+c];
}
// ré-orthonormalise R (Gram-Schmidt sur les lignes) pour limiter la dérive
static void orthonormalize(double R[9])
{
    auto nrm=[&](int o){ double n=std::sqrt(R[o]*R[o]+R[o+1]*R[o+1]+R[o+2]*R[o+2]);
                         if(n>1e-20){R[o]/=n;R[o+1]/=n;R[o+2]/=n;} };
    nrm(0);
    double d=R[0]*R[3]+R[1]*R[4]+R[2]*R[5];
    R[3]-=d*R[0]; R[4]-=d*R[1]; R[5]-=d*R[2]; nrm(3);
    // ligne 2 = ligne0 x ligne1
    R[6]=R[1]*R[5]-R[2]*R[4];
    R[7]=R[2]*R[3]-R[0]*R[5];
    R[8]=R[0]*R[4]-R[1]*R[3];
}

} // namespace

ICPResult icp_align(const std::vector<float>& srcPts, Mesh& target, const ICPOptions& opt)
{
    ICPResult res;
    const size_t N = srcPts.size()/3;
    if (N < 3 || target.GetNFaces()==0) return res;

    BVH bvh; bvh.build(target);

    // Transfo globale accumulée (double), initialisée par la pose fournie.
    double R[9]; for (int i=0;i<9;++i) R[i]=opt.initR[i];
    double T[3]={opt.initT[0],opt.initT[1],opt.initT[2]};
    double scale=opt.initScale;

    // Points source transformés courants X = scale·R·p + T.
    std::vector<double> X(3*N);
    for (size_t i=0;i<N;++i)
    {
        double o[3]; matVec(R, srcPts[3*i],srcPts[3*i+1],srcPts[3*i+2], o);
        X[3*i]  =scale*o[0]+T[0];
        X[3*i+1]=scale*o[1]+T[1];
        X[3*i+2]=scale*o[2]+T[2];
    }

    std::vector<double> Q(3*N);   // correspondances (closest sur la cible)
    std::vector<double> d2(N);
    std::vector<unsigned char> keep(N,1);

    double prevRms = 1e30, firstRms = 1e-9;
    const float trim = std::min(std::max(opt.trimFraction,0.f),0.9f);

    for (int iter=0; iter<opt.maxIterations; ++iter)
    {
        // 1) correspondances closest-point
        for (size_t i=0;i<N;++i)
        {
            float p[3]={(float)X[3*i],(float)X[3*i+1],(float)X[3*i+2]}, cp[3];
            float dd=bvh.closest_distance2(p, cp);
            Q[3*i]=cp[0]; Q[3*i+1]=cp[1]; Q[3*i+2]=cp[2];
            d2[i]=(dd>=0.f)?dd:0.0;
        }

        // 2) trimming optionnel : ne garder que les (1-trim) meilleures paires
        size_t nkeep=N;
        if (trim>0.f)
        {
            std::vector<double> s=d2; std::sort(s.begin(),s.end());
            size_t cut=(size_t)((1.0-trim)*N); if (cut<3) cut=3; if (cut>N) cut=N;
            const double thr=s[cut-1];
            nkeep=0;
            for (size_t i=0;i<N;++i){ keep[i]=(d2[i]<=thr)?1:0; nkeep+=keep[i]; }
            if (nkeep<3){ std::fill(keep.begin(),keep.end(),1); nkeep=N; }
        }
        else std::fill(keep.begin(),keep.end(),1);

        // 3) centroïdes des paires gardées
        double cx[3]={0,0,0}, cq[3]={0,0,0};
        for (size_t i=0;i<N;++i) if (keep[i])
            for (int k=0;k<3;++k){ cx[k]+=X[3*i+k]; cq[k]+=Q[3*i+k]; }
        for (int k=0;k<3;++k){ cx[k]/=nkeep; cq[k]/=nkeep; }

        // 4) covariance croisée M = Σ x'·q'^T (x' = X-cx, q' = Q-cq)
        double M[9]={0,0,0,0,0,0,0,0,0}, sigmaX=0;
        for (size_t i=0;i<N;++i) if (keep[i])
        {
            const double xp[3]={X[3*i]-cx[0],X[3*i+1]-cx[1],X[3*i+2]-cx[2]};
            const double qp[3]={Q[3*i]-cq[0],Q[3*i+1]-cq[1],Q[3*i+2]-cq[2]};
            for (int a=0;a<3;++a) for (int b=0;b<3;++b) M[a*3+b]+=xp[a]*qp[b];
            sigmaX+=xp[0]*xp[0]+xp[1]*xp[1]+xp[2]*xp[2];
        }

        // 5) rotation optimale par quaternion de Horn (matrice N 4x4)
        const double Sxx=M[0],Sxy=M[1],Sxz=M[2],Syx=M[3],Syy=M[4],Syz=M[5],Szx=M[6],Szy=M[7],Szz=M[8];
        double Nm[4][4]={
            {Sxx+Syy+Szz, Syz-Szy,      Szx-Sxz,      Sxy-Syx},
            {Syz-Szy,     Sxx-Syy-Szz,  Sxy+Syx,      Szx+Sxz},
            {Szx-Sxz,     Sxy+Syx,     -Sxx+Syy-Szz,  Syz+Szy},
            {Sxy-Syx,     Szx+Sxz,      Syz+Szy,     -Sxx-Syy+Szz}};
        double quat[4]; largestEigenvectorSym4(Nm, quat);
        double dR[9];   quatToR(quat, dR);

        // 6) échelle optimale (similarité) : ds = Σ q'·(dR x') / Σ ||x'||²
        double ds=1.0;
        if (opt.withScale && sigmaX>1e-20)
        {
            double num=0;
            for (size_t i=0;i<N;++i) if (keep[i])
            {
                const double xp[3]={X[3*i]-cx[0],X[3*i+1]-cx[1],X[3*i+2]-cx[2]};
                double rx[3]; matVec(dR, xp[0],xp[1],xp[2], rx);
                num += (Q[3*i]-cq[0])*rx[0]+(Q[3*i+1]-cq[1])*rx[1]+(Q[3*i+2]-cq[2])*rx[2];
            }
            ds = num/sigmaX;
            if (ds<1e-6) ds=1e-6;
        }

        // 7) translation : dT = cq - ds·dR·cx
        double drcx[3]; matVec(dR, cx[0],cx[1],cx[2], drcx);
        const double dT[3]={cq[0]-ds*drcx[0], cq[1]-ds*drcx[1], cq[2]-ds*drcx[2]};

        // 8) accumulation dans la transfo globale (X = ds·dR·X + dT)
        double Rn[9]; matMul(dR,R,Rn); orthonormalize(Rn);
        for (int i=0;i<9;++i) R[i]=Rn[i];
        double drT[3]; matVec(dR, T[0],T[1],T[2], drT);
        for (int k=0;k<3;++k) T[k]=ds*drT[k]+dT[k];
        scale*=ds;

        // 9) mise à jour des points + RMS des paires gardées
        double sse=0;
        for (size_t i=0;i<N;++i)
        {
            double rx[3]; matVec(dR, X[3*i],X[3*i+1],X[3*i+2], rx);
            X[3*i]  =ds*rx[0]+dT[0];
            X[3*i+1]=ds*rx[1]+dT[1];
            X[3*i+2]=ds*rx[2]+dT[2];
        }
        for (size_t i=0;i<N;++i) if (keep[i])
        {
            float p[3]={(float)X[3*i],(float)X[3*i+1],(float)X[3*i+2]};
            float dd=bvh.closest_distance2(p, nullptr);
            sse += (dd>=0.f)?dd:0.0;
        }
        const double rms=std::sqrt(sse/nkeep);
        res.iterations=iter+1;

        // Convergence : variation de RMS faible RELATIVEMENT à l'erreur initiale
        // (référence stable, robuste quand rms -> ~0), ou RMS négligeable.
        if (iter==0) firstRms = (rms>1e-12)?rms:1e-12;
        if (rms < 1e-9 || (iter>0 && std::fabs(prevRms-rms) < opt.convergenceEps*firstRms))
        { res.converged=true; prevRms=rms; break; }
        prevRms=rms;
    }

    for (int i=0;i<9;++i) res.R[i]=(float)R[i];
    for (int k=0;k<3;++k) res.T[k]=(float)T[k];
    res.scale=(float)scale;
    res.rmsError=(float)prevRms;
    return res;
}
