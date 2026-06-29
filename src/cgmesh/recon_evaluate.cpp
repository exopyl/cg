#include "recon_evaluate.h"

#include "mesh.h"
#include "mesh_metrics.h"

namespace recon {

Metrics HausdorffEvaluator::evaluate(Mesh& mesh, Mesh& reference)
{
    HausdorffResult h = mesh_hausdorff(mesh, reference);

    Metrics m;
    m.hausdorff = h.symmetric;
    mesh.computebbox();
    const float diag = mesh.bbox_diagonal_length();
    m.hausdorffRelative = (diag > 0.f) ? h.symmetric / diag : 0.f;
    m.meanError = h.mean_symmetric;
    m.rmsError  = h.rms_symmetric;
    m.p95Error  = h.p95_symmetric;
    return m;
}

} // namespace recon
