#pragma once
//
// Reconstruction — étape [11] : évaluation mesh reconstruit vs référence.
//
// Adaptateur MINCE : implémente IEvaluator en s'appuyant sur le métrique générique
// mesh_hausdorff (mesh_metrics.h). NB : suppose les deux mesh DÉJÀ recalés (même
// repère / échelle) — le recalage (similarité ICP, cf. icp.h) est un pré-traitement
// amont, hors de cette étape.
//
#include "recon_interfaces.h"

class Mesh;

namespace recon {

class HausdorffEvaluator : public IEvaluator
{
public:
    Metrics evaluate(Mesh& mesh, Mesh& reference) override;
};

} // namespace recon
