#pragma once
//
// Reconstruction — étape [9] : post-traitement mesh.
//
#include "recon_interfaces.h"

class Mesh;

namespace recon {

// Post-traitement minimal et sûr (in place) : soudure des sommets coïncidents.
//
// À venir : décimation QEM (`Mesh_half_edge::simplify`) et lissage Taubin
// (`MeshAlgoSmoothingTaubin`). Ils opèrent sur un `Mesh_half_edge` et `simplify`
// REMPLACE le mesh sous-jacent — il faudra donc recopier le résultat dans le
// `Mesh&` passé (cf. reconstruction.md étape 9).
class MergeVerticesPostProcessor : public IMeshPostProcessor
{
public:
    float tolerance = 1e-5f;

    void process(Mesh& mesh) override;
};

} // namespace recon
