#pragma once
//
//  IcpAlign -- recalage ICP d'un maillage source sur la surface d'un maillage
//  cible.
//
// Emballe icp_align (icp.h). Corps lu, et quatre choses en sortent que la
// declaration ne dit pas :
//
//  - la cible est prise par `Mesh&` NON const parce que BVH::build la demande
//    ainsi ; le corps ne l'ecrit pas, mais la frontiere d'immuabilite oblige
//    l'adaptateur a en faire une copie profonde ;
//  - le BVH construit sur la cible CONSERVE UN POINTEUR NU sur son tableau de
//    positions (bvh.cpp:56). La cible doit survivre a l'appel, ce que la copie
//    locale garantit ici ;
//  - seuls les SOMMETS de la source entrent dans le recalage. Sa topologie n'y
//    joue aucun role, et la sortie est la source dont les positions ont ete
//    transformees -- meme faces, memes attributs ;
//  - `converged` est rendu a false quand le budget d'iterations est epuise
//    comme quand l'annulation coupe la boucle. L'adaptateur ne le lit pas : un
//    recalage non convergent reste un recalage, et le refus d'annulation est
//    nomme par l'evaluateur.
//
// Le corps ne rend AUCUN diagnostic exploitable en cas d'entree inutilisable :
// il rend un ICPResult par defaut -- identite, rmsError = -1 -- quand la source
// a moins de trois points ou la cible aucune face. L'adaptateur teste ces deux
// conditions lui-meme plutot que de deviner l'identite d'un echec.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class IcpAlignNode : public cggraph::Node
{
public:
	IcpAlignNode (int maxIterations = 60, bool withScale = false);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
