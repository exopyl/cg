#pragma once
//
//  Quantize -- image -> image quantifiee, pretes a vectoriser.
//
// C'est le TRONC COMMUN des deux chaines « image -> regions extrudees », celle
// du relief colore et celle des blocs pixelises. Il vit ici en un seul noeud
// pour la meme raison qu'il vit dans un seul fichier cote cgmesh
// (image_region_pipeline.h) : l'ordre de ses etapes porte des decisions non
// evidentes -- lisser AVANT de quantifier, raffiner AVANT l'anti-mouchetis,
// pixeliser APRES la quantification -- et deux copies divergeraient au premier
// reglage.
//
// En faire un noeud a une consequence visible dans l'editeur : la difference
// entre les deux chaines cesse d'etre un choix binaire entre deux boites
// opaques. « Blocs pixelises », c'est CE noeud avec pixelWidth renseigne, suivi
// d'un extrudeur different. Le graphe le montre.
//
// Sa sortie est le raster quantifie, non palettise -- la quantification travaille
// sur le tampon RGBA. C'est ce que les quatre noeuds d'extrusion attendent.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class QuantizeImageNode : public cggraph::Node
{
public:
	QuantizeImageNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
