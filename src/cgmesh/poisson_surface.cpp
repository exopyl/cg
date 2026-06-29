#include "poisson_surface.h"

#ifdef CG_HAS_POISSON

#include "mesh.h"

// PoissonRecon (master vendorisé). PreProcessor.h doit précéder Reconstructors.h.
#include "PreProcessor.h"
#include "Reconstructors.h"

#include <algorithm>
#include <vector>

namespace {

using PReal = float;
static const unsigned int PDim = 3;

// Flux d'entrée : points orientés depuis des tableaux bruts (positions + normales).
struct CloudStream : public PoissonRecon::Reconstructor::InputOrientedSampleStream<PReal, PDim>
{
    const float* pos;
    const float* nor;
    size_t       n;
    size_t       cur = 0;
    CloudStream(const float* p, const float* nr, size_t nn) : pos(p), nor(nr), n(nn) {}

    void reset(void) override { cur = 0; }

    bool read(PoissonRecon::Point<PReal, PDim>& p, PoissonRecon::Point<PReal, PDim>& nv) override
    {
        if (cur >= n) return false;
        const size_t i = cur++;
        p[0]  = pos[3 * i + 0]; p[1]  = pos[3 * i + 1]; p[2]  = pos[3 * i + 2];
        nv[0] = nor[3 * i + 0]; nv[1] = nor[3 * i + 1]; nv[2] = nor[3 * i + 2];
        return true;
    }
};

// Flux de sortie : sommets du level-set -> coordonnées plates + densité (Weight).
struct VStream : public PoissonRecon::Reconstructor::OutputLevelSetVertexStream<PReal, PDim>
{
    std::vector<PReal>& v;
    std::vector<PReal>& dens;
    VStream(std::vector<PReal>& vc, std::vector<PReal>& dc) : v(vc), dens(dc) {}
    size_t size(void) const override { return v.size() / 3; }
    size_t write(const PoissonRecon::Point<PReal, PDim>& p,
                 const PoissonRecon::Point<PReal, PDim>&, const PReal& w) override
    {
        v.push_back(p[0]); v.push_back(p[1]); v.push_back(p[2]);
        dens.push_back(w);        // Weight = densité (si ep.outputDensity)
        return v.size() / 3 - 1;
    }
};

// Flux de sortie : polygones (indices).
struct FStream : public PoissonRecon::Reconstructor::OutputFaceStream<2>
{
    std::vector<std::vector<int>>& f;
    explicit FStream(std::vector<std::vector<int>>& faces) : f(faces) {}
    size_t size(void) const override { return f.size(); }
    size_t write(const std::vector<PoissonRecon::node_index_type>& poly) override
    {
        std::vector<int> q(poly.size());
        for (size_t i = 0; i < poly.size(); ++i) q[i] = (int)poly[i];
        f.push_back(q);
        return f.size() - 1;
    }
};

} // namespace

Mesh* poisson_reconstruct(const float* positions, const float* normals, size_t n,
                          const PoissonParams& params)
{
    using namespace PoissonRecon;

    if (n < 4 || !positions || !normals)
        return nullptr;

    ThreadPool::ParallelizationType = (ThreadPool::ParallelType)0;

    using ReconType = Reconstructor::Poisson;
    static const unsigned int FEMSig =
        FEMDegreeAndBType<ReconType::DefaultFEMDegree, ReconType::DefaultFEMBoundary>::Signature;
    using FEMSigs  = IsotropicUIntPack<PDim, FEMSig>;
    using Implicit = Reconstructor::Implicit<PReal, PDim, FEMSigs>;
    using Solver   = ReconType::Solver<PReal, PDim, FEMSigs>;

    ReconType::SolutionParameters<PReal> sp;
    sp.depth = (unsigned int)params.depth;

    CloudStream stream(positions, normals, n);
    Implicit* implicit = Solver::Solve(stream, sp);
    if (!implicit)
        return nullptr;

    Reconstructor::LevelSetExtractionParameters ep;
    ep.outputDensity = (params.trimQuantile > 0.f);     // densité par sommet pour le trim
    std::vector<std::vector<int>> polys;
    std::vector<PReal>            vc, dens;
    VStream vs(vc, dens);
    FStream fs(polys);
    implicit->extractLevelSet(vs, fs, ep);
    delete implicit;

    if (vc.empty() || polys.empty())
        return nullptr;

    // Triangulation en éventail des polygones (PoissonRecon peut sortir des quads).
    std::vector<unsigned int> tris;
    for (const auto& poly : polys)
        for (size_t k = 2; k < poly.size(); ++k)
        {
            tris.push_back((unsigned)poly[0]);
            tris.push_back((unsigned)poly[k - 1]);
            tris.push_back((unsigned)poly[k]);
        }
    if (tris.empty())
        return nullptr;

    // Trim par densité : retire les triangles dont la densité moyenne < quantile.
    if (params.trimQuantile > 0.f && dens.size() == vc.size() / 3)
    {
        std::vector<PReal> s = dens;
        const size_t idx = (size_t)(params.trimQuantile * (s.size() - 1));
        std::nth_element(s.begin(), s.begin() + idx, s.end());
        const PReal thr = s[idx];
        std::vector<unsigned int> kept;
        kept.reserve(tris.size());
        for (size_t t = 0; t < tris.size(); t += 3)
        {
            const PReal avg = (dens[tris[t]] + dens[tris[t+1]] + dens[tris[t+2]]) / 3.f;
            if (avg >= thr)
            { kept.push_back(tris[t]); kept.push_back(tris[t+1]); kept.push_back(tris[t+2]); }
        }
        tris.swap(kept);
        if (tris.empty()) return nullptr;
    }

    // Compactage : ne conserver que les sommets référencés par les triangles gardés.
    std::vector<int>          remap(vc.size() / 3, -1);
    std::vector<float>        nverts;
    std::vector<unsigned int> nfaces;
    nfaces.reserve(tris.size());
    for (unsigned int vi : tris)
    {
        if (remap[vi] < 0)
        {
            remap[vi] = (int)(nverts.size() / 3);
            nverts.push_back(vc[3*vi]); nverts.push_back(vc[3*vi+1]); nverts.push_back(vc[3*vi+2]);
        }
        nfaces.push_back((unsigned)remap[vi]);
    }

    Mesh* mesh = new Mesh();
    mesh->SetVertices((unsigned)(nverts.size() / 3), nverts.data());
    mesh->SetFaces((unsigned)(nfaces.size() / 3), 3, nfaces.data());
    mesh->ComputeNormals();
    return mesh;
}

#endif // CG_HAS_POISSON
