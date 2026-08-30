#pragma once
//
//  flow.subgraph -- applique UNE fois le document delegue.
//
// Le plus simple des trois, et le seul dont le comportement soit celui d'un
// noeud ordinaire : n entrees, m sorties, un calcul. Ce qu'il apporte est la
// REUTILISATION -- le meme document se delegue depuis plusieurs graphes et
// depuis plusieurs endroits d'un meme graphe, et il ne se corrige qu'une fois.
//
// Ses ports viennent du DOCUMENT : un port par noeud `flow.in`, un par
// `flow.out`, dans l'ordre de leur parametre `slot` et sous le nom que leur
// parametre `nom` porte.
//
#include "host.h"

namespace cggraph_nodes
{
namespace flow
{

class SubgraphNode : public SubgraphHostNode
{
public:
	SubgraphNode ();

	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

protected:
	void PublishPorts (const SubgraphInstance &instance) override;
};

} // namespace flow
} // namespace cggraph_nodes
