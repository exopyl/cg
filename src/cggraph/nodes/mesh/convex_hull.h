#pragma once
//
//  ConvexHull -- enveloppe convexe 3D des sommets d'un maillage.
//
// Emballe Chull3D (chull.h), adaptation de l'algorithme incremental de
// O'Rourke. Corps lu, et quatre choses en sortent que la declaration ne dit
// pas :
//
//  - compute() rend VOID et IGNORE l'echec de double_triangle(), qui rend 1
//    quand tous les points sont colineaires. Sur une entree degeneree, l'objet
//    est laisse sans faces et le seul signalement est un printf. L'adaptateur
//    decide donc sur get_convex_hull(), pas sur compute() ;
//  - le constructeur RECOPIE les points dans sa propre liste chainee. C'est le
//    seul des noeuds d'analyse qui ne touche pas son entree -- il ne lui faut
//    pas de copie profonde du maillage, seulement de ses positions ;
//  - get_convex_hull() alloue par malloc et le rend a l'appelant, qui doit
//    free() -- pas delete[] ;
//  - get_vertex_index() parcourt la liste des sommets pour CHAQUE coin de
//    chaque face : la mise en forme du resultat est quadratique en le nombre de
//    sommets de l'enveloppe, alors que la construction ne l'est pas.
//
// Aucun parametre : l'enveloppe convexe d'un ensemble de points n'en a pas.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class ConvexHullNode : public cggraph::Node
{
public:
	ConvexHullNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
