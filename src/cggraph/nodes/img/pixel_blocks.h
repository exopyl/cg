#pragma once
//
//  PixelBlocks -- image quantifiee et pixelisee -> blocs extrudes.
//
// Adaptateur de image_pixel_blocks.h, c'est-a-dire de la page « Blocs
// pixelises » de maker. Deux noeuds, sur le meme partage que le relief :
//
//   img.pixel_blocks        -> UN maillage, un materiau par couleur de palette.
//                              Objet d'AFFICHAGE : l'image reste lisible.
//   img.pixel_blocks.parts  -> UNE SUITE, un solide par BLOC CONNEXE. Objet de
//                              FABRICATION : ce sont les pieces separables, a
//                              imprimer, trier et assembler. C'est ce que
//                              flow.foreach parcourt.
//
// ⚠ LES DEUX ATTENDENT UNE IMAGE DEJA QUANTIFIEE **ET PIXELISEE**, telle que la
// rend img.quantize avec `pixelWidth` renseigne. Ils ne pixelisent pas.
//
// C'est le piege a connaitre de cette chaine, et il est plus vicieux que celui
// du relief : sur une image quantifiee mais NON pixelisee, la segmentation en
// composantes connexes reussit -- elle sort simplement une piece par region de
// l'image pleine resolution, soit des milliers de solides minuscules. Rien
// n'echoue ; le resultat est seulement ininterpretable. La grille se decide en
// amont, dans img.quantize.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class PixelBlocksNode : public cggraph::Node
{
public:
	PixelBlocksNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Un maillage par bloc connexe, puis la base, puis le mur -- ces deux-la
// toujours en dernier quand ils sont demandes.
//
// Chaque maillage de bloc porte un unique materiau nomme "block_NNNN_color_NN" :
// NNNN l'indice du bloc, NN l'indice de palette de sa couleur. La suite etant
// dense, ce nom est le SEUL lien fiable entre la position dans la suite et le
// bloc d'origine.
class PixelBlocksPartsNode : public cggraph::Node
{
public:
	PixelBlocksPartsNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
