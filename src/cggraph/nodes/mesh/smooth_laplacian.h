#pragma once
//
//  SmoothLaplacian -- lissage laplacien, zone optionnelle.
//
// Emballe MeshAlgoSmoothingLaplacian::Apply. Corps lu, et deux choses en
// sortent que la seule declaration ne disait pas :
//
//  - Apply fait UNE passe, pas n. Le parametre "iterations" boucle donc ICI ;
//  - Apply ne connait pas de facteur de relaxation : il remplace chaque sommet
//    manifold non frontiere par la moyenne de ses voisins. "lambda" est un
//    melange applique apres la passe, entre la position d'avant et celle
//    d'apres, ce qui redonne au parametre le sens qu'il a partout ailleurs.
//
// Le descripteur canonique de la conception declarait un troisieme parametre,
// "preserveBoundary". Il n'est PAS declare ici : Apply teste is_border() sans
// condition, si bien que la preservation du bord n'est pas un choix mais une
// propriete de l'algorithme. Un parametre sans effet est pire qu'un parametre
// absent -- il se regle, il entre dans la signature, et il ne change rien.
//
// L'entree "zone" est OPTIONNELLE : non alimentee, le lissage porte sur tout le
// maillage ; alimentee, les sommets hors zone sont ramenes a leur position
// d'origine apres chaque passe.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class SmoothLaplacianNode : public cggraph::Node
{
public:
	SmoothLaplacianNode (int iterations = 5, float lambda = 1.0f);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
