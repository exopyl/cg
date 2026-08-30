#pragma once
//
//  Simplify -- decimation QEM par contraction d'aretes.
//
// Emballe Mesh_half_edge::simplify. Corps lu, et trois proprietes en sortent
// qui ne se devinent pas de la declaration :
//
//  - il TRIANGULE le maillage s'il ne l'est pas deja, avant toute chose ;
//  - "targetRatio" est une fraction du nombre de FACES a conserver, bornee a
//    [0,1] par l'algorithme lui-meme ;
//  - "maxError" s'exprime en fraction de la diagonale de la boite englobante,
//    et 0 le desactive. Sous "exactError", le critere devient une vraie borne
//    de distance des SOMMETS a la surface d'origine, obtenue par un BVH ; sans
//    lui, c'est le cout QEM qui sert de PROXY -- ce n'est pas une borne de
//    Hausdorff, et le nom du parametre ne doit pas le laisser croire.
//
// Annulable : la boucle de contraction s'interrompt, la compaction finale a
// quand meme lieu, et le maillage rendu est alors moins decime que demande.
// L'adaptateur ne le publie pas -- il rend `false` --, et l'evaluateur nomme le
// refus Aborted parce qu'il interroge le contexte avant le code de retour.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class SimplifyNode : public cggraph::Node
{
public:
	SimplifyNode (float targetRatio = 0.5f);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
