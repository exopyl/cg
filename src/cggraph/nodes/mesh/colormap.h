#pragma once
//
//  Colormap -- coloriage d'un maillage par un champ scalaire par sommet.
//
// Emballe Mesh::InitVertexColorsFromArray (mesh.h). Corps lu, et trois choses
// en sortent que la declaration ne dit pas :
//
//  - le tableau `defined` NE PROTEGE PAS les sommets non definis. Le corps leur
//    ecrit bien du noir, puis appelle color_jet sans `else` juste apres et
//    ecrase ce noir par la couleur d'une valeur qui n'a pas de sens
//    (mesh.cpp:142-148). L'adaptateur ne corrige pas ce corps : il NEUTRALISE
//    les valeurs non definies avant l'appel, en les remplacant par le minimum
//    des valeurs definies, de sorte que le coloriage soit celui d'un champ
//    entierement defini ;
//  - `defined` n'entre PAS dans le calcul du min/max initial pour l'indice 0 :
//    le corps sème min = max = array[0] avant la boucle, quel que soit son
//    drapeau. La neutralisation ci-dessus ferme aussi ce cas ;
//  - le corps ECRIT LES COORDONNEES DE TEXTURE et active leurs indices sur
//    chaque face. Colorier un maillage change donc sa parametrisation, ce que
//    ni le nom ni la signature n'annoncent.
//
// Aucun parametre : le corps normalise sur le min/max observe et n'expose
// aucune echelle. Un parametre d'echelle serait un mensonge que la UI rendrait
// credible.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class ColormapNode : public cggraph::Node
{
public:
	ColormapNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
