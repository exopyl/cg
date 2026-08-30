#pragma once
//
//  Curvature -- tenseurs de courbure par sommet, et le champ scalaire qui en
//  derive.
//
// Emballe MeshAlgoTensorEvaluator::Init / Evaluate (DiffParamEvaluator.h).
// Corps des six estimateurs lu, et six choses en sortent que la declaration ne
// dit pas :
//
//  - ApplySteiner a son corps ENTIEREMENT sous #if 0. La methode « Steiner »
//    ne calcule rien et rendait true : Init ayant pose un Tensor neuf par
//    sommet, l'appelant recevait des courbures NULLES presentees comme un
//    resultat. Le fait est deja epingle par la suite -- voir
//    test/tu_cgmesh_tensor.cpp, le cas dedie a TENSOR_STEINER. Ce noeud
//    n'expose donc PAS cette methode ;
//  - ApplyHybrid tire ses courbures de Desbrun et ses directions principales de
//    Steiner. Steiner ne modifiant aucun tenseur, l'hybride relit ce que
//    Desbrun vient d'ecrire : « Hybrid » est aujourd'hui un Desbrun. Il reste
//    expose, car il est teste et exact, mais son nom promet plus que son corps ;
//  - un sommet de BORD ou non manifold recoit un tenseur NUL, pas un tenseur
//    faux. C'est ce que porte `defined` dans le champ rendu ; confondre
//    l'absence avec une courbure de zero donne une carte fausse sur tout le
//    bord ;
//  - les estimateurs lisent GetVertexNormals () sans jamais les calculer. Sur
//    un maillage sans normales, ils lisent un tableau vide. L'adaptateur les
//    calcule avant l'appel ;
//  - les estimateurs lisent la structure demi-arete par is_manifold / is_border,
//    qui exigent create_half_edge (). L'adaptateur l'appelle ;
//  - Evaluate IGNORAIT le code de retour de la methode appelee et estampillait
//    les tenseurs valides quoi qu'il arrive. Corrige a l'occasion de
//    l'annulation : un calcul interrompu ne laisse plus derriere lui des
//    tenseurs partiels reputes a jour.
//
// Deux sorties, et c'est voulu : le maillage PORTE ses tenseurs -- c'est lui
// qu'un afficheur de directions principales consomme --, tandis que le champ
// scalaire est ce qu'un coloriage consomme. Les recalculer d'un cote pour les
// avoir de l'autre serait payer deux fois.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class CurvatureNode : public cggraph::Node
{
public:
	CurvatureNode (int method = 0, int curvature = 3);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
