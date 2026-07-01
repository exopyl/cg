#include "recon_io.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

namespace recon {

static int plyTypeSize(const std::string& t)
{
    if (t=="char"||t=="uchar"||t=="int8"||t=="uint8")            return 1;
    if (t=="short"||t=="ushort"||t=="int16"||t=="uint16")        return 2;
    if (t=="int"||t=="uint"||t=="int32"||t=="uint32"||t=="float"||t=="float32") return 4;
    if (t=="double"||t=="float64")                               return 8;
    return 0;
}

bool loadPointCloudPly(const std::string& path, PointCloud& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::string line;
    std::getline(f, line);
    if (line.rfind("ply", 0) != 0) return false;

    bool   binaryLE = false;
    size_t n = 0;
    struct Prop { std::string name, type; int offset; };
    std::vector<Prop> props;
    int stride = 0;

    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream is(line);
        std::string tok; is >> tok;
        if (tok == "format")
        {
            std::string fmt; is >> fmt;
            binaryLE = (fmt == "binary_little_endian");
        }
        else if (tok == "element")
        {
            std::string e; size_t cnt; is >> e >> cnt;
            if (e == "vertex") n = cnt;
        }
        else if (tok == "property")
        {
            std::string type, name; is >> type >> name;
            if (type == "list") continue;          // faces : ignorées
            props.push_back({name, type, stride});
            stride += plyTypeSize(type);
        }
        else if (tok == "end_header")
            break;
    }
    if (!binaryLE || n == 0 || stride == 0) return false;

    // Offsets des propriétés d'intérêt.
    int ox=-1, oy=-1, oz=-1, onx=-1, ony=-1, onz=-1, ored=-1, ogrn=-1, oblu=-1;
    for (const auto& p : props)
    {
        if      (p.name=="x")  ox=p.offset;  else if (p.name=="y")  oy=p.offset;
        else if (p.name=="z")  oz=p.offset;  else if (p.name=="nx") onx=p.offset;
        else if (p.name=="ny") ony=p.offset; else if (p.name=="nz") onz=p.offset;
        else if (p.name=="red") ored=p.offset; else if (p.name=="green") ogrn=p.offset;
        else if (p.name=="blue") oblu=p.offset;
    }
    if (ox<0 || oy<0 || oz<0) return false;
    const bool hasN = (onx>=0 && ony>=0 && onz>=0);
    const bool hasC = (ored>=0 && ogrn>=0 && oblu>=0);

    out.positions.resize(3*n);
    if (hasN) out.normals.resize(3*n);
    if (hasC) out.colors.resize(3*n);

    std::vector<char> buf(stride);
    auto rf = [&](int off){ float v; std::memcpy(&v, buf.data()+off, 4); return v; };
    for (size_t i=0; i<n; ++i)
    {
        f.read(buf.data(), stride);
        if (!f) return false;
        out.positions[3*i+0]=rf(ox); out.positions[3*i+1]=rf(oy); out.positions[3*i+2]=rf(oz);
        if (hasN){ out.normals[3*i+0]=rf(onx); out.normals[3*i+1]=rf(ony); out.normals[3*i+2]=rf(onz); }
        if (hasC)
        {
            out.colors[3*i+0]=(unsigned char)buf[ored]/255.f;
            out.colors[3*i+1]=(unsigned char)buf[ogrn]/255.f;
            out.colors[3*i+2]=(unsigned char)buf[oblu]/255.f;
        }
    }
    return true;
}

bool savePointCloudPly(const std::string& path, const PointCloud& cloud)
{
    std::ofstream f(path);
    if (!f) return false;

    const size_t n  = cloud.size();
    const bool   hn = cloud.hasNormals();
    const bool   hc = cloud.hasColors();

    f << "ply\nformat ascii 1.0\nelement vertex " << n << "\n";
    f << "property float x\nproperty float y\nproperty float z\n";
    if (hn) f << "property float nx\nproperty float ny\nproperty float nz\n";
    if (hc) f << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    f << "end_header\n";

    for (size_t i = 0; i < n; ++i)
    {
        f << cloud.positions[3*i+0] << ' ' << cloud.positions[3*i+1] << ' ' << cloud.positions[3*i+2];
        if (hn) f << ' ' << cloud.normals[3*i+0] << ' ' << cloud.normals[3*i+1] << ' ' << cloud.normals[3*i+2];
        if (hc)
            f << ' ' << (int)(cloud.colors[3*i+0]*255.f + 0.5f)
              << ' ' << (int)(cloud.colors[3*i+1]*255.f + 0.5f)
              << ' ' << (int)(cloud.colors[3*i+2]*255.f + 0.5f);
        f << '\n';
    }
    return (bool)f;
}

namespace {

// RANSAC : ajuste le plus grand plan du nuage. Renvoie true si un plan dominant
// existe (>= n/10 inliers), en remplissant la normale unitaire n_out[3], l'offset
// d_out (plan : n·x + d = 0) et le seuil d'inlier absolu thr_out. Utilisé à la fois
// par removeDominantPlane (filtrage) et orientSceneWithPlane (mise à niveau).
bool fitDominantPlane(const PointCloud& cloud, float distThreshRel, int iterations,
                      float n_out[3], float& d_out, float& thr_out)
{
    const std::size_t n = cloud.size();
    if (n < 3) return false;
    const float* P = cloud.positions.data();
    auto at = [&](std::size_t i, int a) { return P[3*i + a]; };

    // Seuil d'inlier absolu = fraction de la diagonale de la bbox.
    float mn[3] = { 1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f,-1e30f,-1e30f};
    for (std::size_t i = 0; i < n; ++i)
        for (int a = 0; a < 3; ++a)
        { float v = at(i,a); if (v<mn[a]) mn[a]=v; if (v>mx[a]) mx[a]=v; }
    const float diag = std::sqrt((mx[0]-mn[0])*(mx[0]-mn[0]) + (mx[1]-mn[1])*(mx[1]-mn[1]) + (mx[2]-mn[2])*(mx[2]-mn[2]));
    const float thr = distThreshRel * diag;
    if (thr <= 0.f) return false;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<std::size_t> U(0, n - 1);
    float       bn[3] = {0,0,0}, bd = 0.f;
    std::size_t best = 0;

    for (int it = 0; it < iterations; ++it)
    {
        const std::size_t a = U(rng), b = U(rng), c = U(rng);
        if (a==b || b==c || a==c) continue;
        const float e1x=at(b,0)-at(a,0), e1y=at(b,1)-at(a,1), e1z=at(b,2)-at(a,2);
        const float e2x=at(c,0)-at(a,0), e2y=at(c,1)-at(a,1), e2z=at(c,2)-at(a,2);
        float nx=e1y*e2z-e1z*e2y, ny=e1z*e2x-e1x*e2z, nz=e1x*e2y-e1y*e2x;
        const float ln=std::sqrt(nx*nx+ny*ny+nz*nz);
        if (ln < 1e-12f) continue;
        nx/=ln; ny/=ln; nz/=ln;
        const float d = -(nx*at(a,0)+ny*at(a,1)+nz*at(a,2));
        std::size_t cnt = 0;
        for (std::size_t i = 0; i < n; ++i)
            if (std::fabs(nx*at(i,0)+ny*at(i,1)+nz*at(i,2)+d) < thr) ++cnt;
        if (cnt > best) { best=cnt; bn[0]=nx; bn[1]=ny; bn[2]=nz; bd=d; }
    }
    if (best < n / 10) return false;   // pas de plan vraiment dominant
    n_out[0]=bn[0]; n_out[1]=bn[1]; n_out[2]=bn[2]; d_out=bd; thr_out=thr;
    return true;
}

} // namespace

std::size_t removeDominantPlane(PointCloud& cloud, float distThreshRel, int iterations)
{
    float bn[3], bd, thr;
    if (!fitDominantPlane(cloud, distThreshRel, iterations, bn, bd, thr))
        return 0;

    const std::size_t n = cloud.size();
    const float* P = cloud.positions.data();
    auto at = [&](std::size_t i, int a) { return P[3*i + a]; };
    const bool hn = cloud.hasNormals(), hc = cloud.hasColors();
    PointCloud out;
    out.positions.reserve(cloud.positions.size());
    if (hn) out.normals.reserve(cloud.normals.size());
    if (hc) out.colors.reserve(cloud.colors.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        if (std::fabs(bn[0]*at(i,0)+bn[1]*at(i,1)+bn[2]*at(i,2)+bd) < thr) continue; // inlier plan -> retiré
        out.positions.push_back(at(i,0)); out.positions.push_back(at(i,1)); out.positions.push_back(at(i,2));
        if (hn) { out.normals.push_back(cloud.normals[3*i]); out.normals.push_back(cloud.normals[3*i+1]); out.normals.push_back(cloud.normals[3*i+2]); }
        if (hc) { out.colors.push_back(cloud.colors[3*i]); out.colors.push_back(cloud.colors[3*i+1]); out.colors.push_back(cloud.colors[3*i+2]); }
    }
    const std::size_t removed = n - out.size();
    cloud = std::move(out);
    return removed;
}

bool orientSceneWithPlane(PointCloud& cloud, float distThreshRel, int iterations)
{
    float pn[3], pd, thr;
    if (!fitDominantPlane(cloud, distThreshRel, iterations, pn, pd, thr))
        return false;

    const std::size_t N = cloud.size();
    auto pos = [&](std::size_t i, int a) -> float& { return cloud.positions[3*i + a]; };

    // Oriente la normale du plan vers le côté « objet » (points HORS plan) pour que la
    // scène se retrouve du côté z >= 0 après transformation. Sans ça, le signe issu du
    // RANSAC est arbitraire et l'objet pourrait se retrouver sous Oxy.
    double side = 0.0;
    for (std::size_t i = 0; i < N; ++i)
    {
        const float sd = pn[0]*pos(i,0)+pn[1]*pos(i,1)+pn[2]*pos(i,2)+pd;
        if (std::fabs(sd) >= thr) side += sd;
    }
    if (side < 0.0) { pn[0]=-pn[0]; pn[1]=-pn[1]; pn[2]=-pn[2]; pd=-pd; }

    // Base orthonormée directe (u, v, pn). La rotation R = lignes(u; v; pn) envoie pn
    // sur +Z (et le plan sur un plan horizontal). u obtenu par Gram-Schmidt à partir
    // d'un axe non colinéaire à pn ; v = pn × u. det(R)=+1 (rotation propre).
    float u[3];
    if (std::fabs(pn[0]) < 0.9f) { u[0]=1.f; u[1]=0.f; u[2]=0.f; }
    else                        { u[0]=0.f; u[1]=1.f; u[2]=0.f; }
    const float un = u[0]*pn[0]+u[1]*pn[1]+u[2]*pn[2];
    u[0]-=un*pn[0]; u[1]-=un*pn[1]; u[2]-=un*pn[2];
    const float ul = std::sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);
    if (ul < 1e-12f) return false;
    u[0]/=ul; u[1]/=ul; u[2]/=ul;
    float v[3] = { pn[1]*u[2]-pn[2]*u[1], pn[2]*u[0]-pn[0]*u[2], pn[0]*u[1]-pn[1]*u[0] };

    // Rotation puis translation z = pd : après R, un point du plan a z' = pn·x = -pd
    // (car pn·x + pd = 0), donc z'' = z' + pd = 0 -> le plan tombe exactement sur Oxy ;
    // un point objet garde z'' = pn·x + pd = sa distance signée au plan (> 0).
    auto rotate = [&](float x, float y, float z, float& ox, float& oy, float& oz) {
        ox = u[0]*x + u[1]*y + u[2]*z;
        oy = v[0]*x + v[1]*y + v[2]*z;
        oz = pn[0]*x + pn[1]*y + pn[2]*z;
    };
    for (std::size_t i = 0; i < N; ++i)
    {
        float ox, oy, oz;
        rotate(pos(i,0), pos(i,1), pos(i,2), ox, oy, oz);
        pos(i,0) = ox; pos(i,1) = oy; pos(i,2) = oz + pd;
    }
    if (cloud.hasNormals())
        for (std::size_t i = 0; i < N; ++i)
        {
            float ox, oy, oz;
            rotate(cloud.normals[3*i], cloud.normals[3*i+1], cloud.normals[3*i+2], ox, oy, oz);
            cloud.normals[3*i]=ox; cloud.normals[3*i+1]=oy; cloud.normals[3*i+2]=oz;
        }
    return true;
}

} // namespace recon
