#include "recon_postprocess.h"

#include "mesh.h"

namespace recon {

void MergeVerticesPostProcessor::process(Mesh& mesh)
{
    mesh.MergeVertices(tolerance);
}

} // namespace recon
