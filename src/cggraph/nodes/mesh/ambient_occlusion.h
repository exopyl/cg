#pragma once
//
//  AmbientOcclusion -- occlusion ambiante par sommet, rendue en champ scalaire.
//
// Emballe MeshAlgoAmbientOcclusion (ambient_occlusion.h). Corps lu, et cinq
// choses en sortent que la declaration ne dit pas :
//
//  - Evaluate() appelle ComputeNormals() sur le maillage : Init prend un
//    `Mesh *` non const et le corps y ECRIT. D'ou la copie profonde ici ;
//  - la valeur rendue est un tableau ALLOUE PAR MALLOC, jamais libere par
//    l'algorithme. free(), pas delete[] ;
//  - le rayon d'influence est CABLE a un dixieme de la diagonale de la boite
//    englobante, et l'occluder est ignore au-dela de dix fois la racine de son
//    aire. Ce ne sont pas des reglages : aucun parametre ne les atteint ;
//  - un sommet d'aire nulle -- isole, ou entoure de faces degenerees -- garde
//    une occlusion de 0 sans que rien ne le distingue d'un sommet reellement
//    non occulte. C'est ce que le champ `defined` porte ici, et il est calcule
//    par l'adaptateur, pas par le corps ;
//  - l'octree est construit avec une profondeur maximale de 3 et 300 elements
//    par noeud, cablees elles aussi ;
//  - le rayon d'influence etait lu dans une boite englobante que le corps ne
//    calculait pas. bbox_diagonal_length () rend la derniere valeur calculee
//    sans detecter sa peremption, et BoundingBox::GetDiagonalLength () ne teste
//    pas m_bEmpty : sur un maillage jamais passe par computebbox (), elle lit
//    de la memoire NON INITIALISEE. Le corps appelle desormais computebbox ()
//    en tete, comme MeshAlgoThickness le fait deja.
//
// Le seul parametre est le nombre de PASSES : a partir de la seconde, chaque
// passe reutilise l'occlusion de la precedente comme facteur d'emission, ce qui
// simule l'inter-reflexion. Une passe est le defaut, et c'est le seul reglage
// que le corps sache tenir.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class AmbientOcclusionNode : public cggraph::Node
{
public:
	AmbientOcclusionNode (int passes = 1);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
