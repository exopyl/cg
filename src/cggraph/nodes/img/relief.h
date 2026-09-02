#pragma once
//
//  Relief -- image quantifiee -> relief colore par extrusion de regions.
//
// Adaptateur de image_relief.h, c'est-a-dire de la page « Image to puzzle » de
// maker. Deux noeuds, parce que la brique a deux sorties de nature differente et
// qu'aucune ne se derive de l'autre a bon compte :
//
//   img.relief         -> UN maillage, un materiau par couleur. C'est l'objet
//                         d'AFFICHAGE : a l'ecran on veut lire l'image.
//   img.relief.layers  -> UNE SUITE, un solide par couleur. C'est l'objet de
//                         FABRICATION, et c'est ce que flow.foreach parcourt
//                         pour enregistrer une piece par fichier.
//
// ⚠ LES DEUX ATTENDENT UNE IMAGE DEJA QUANTIFIEE, telle que la rend img.quantize.
// Ils ne quantifient pas : brancher directement img.io.load sur leur entree
// produit un relief a autant de couleurs que l'image en compte, donc quelques
// milliers de regions. C'est un piege silencieux -- la geometrie sort, elle est
// seulement inexploitable -- et c'est le prix d'avoir sorti la quantification
// dans son propre noeud, ou elle est visible et reglable.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

// Un maillage, un materiau par couleur (plus la base et le mur s'ils sont
// demandes).
class ReliefNode : public cggraph::Node
{
public:
	ReliefNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Une suite : les couches de couleur dans l'ordre d'index de palette, puis la
// base, puis le mur -- ces deux-la toujours en dernier quand ils sont demandes.
//
// La suite est DENSE : une couche qui ne tesselle rien est OMISE, si bien que la
// position i ne designe pas la couleur i. La correspondance fiable est le nom du
// materiau de chaque maillage, "color_NN" ou NN est l'index de palette.
class ReliefLayersNode : public cggraph::Node
{
public:
	ReliefLayersNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
