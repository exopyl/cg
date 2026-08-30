#pragma once
//
//  flow.foreach -- applique le document delegue a CHAQUE element d'une suite.
//
// C'est la reponse au « port de sortie a cardinalite variable », que nodal.md
// §11.1 nomme un piege : une segmentation rend N pieces, et un port qui
// changerait de nombre selon l'entree ne serait pas un port. La suite est une
// VALEUR, et le parcours est un NOEUD.
//
// ⚠ CE NOEUD SERT LA DIVERGENCE, PAS LA REPETITION -- critere 9.5, et ce n'est
// pas une exhortation, c'est un cout mesurable. Chaque passe est une evaluation
// distincte au regard de la signature ; decomposer en ForEach un calcul qui
// memoise en INTERNE par contenu detruit cette memoisation. Le contre-exemple
// est mesure : text_extrude.cpp memoise par glyphe, si bien que « MISSISSIPPI »
// n'aplatit que quatre glyphes sur onze -- un ForEach sur les onze caracteres en
// aplatirait onze, et le graphe ne verrait meme pas ce qu'il a perdu. La regle :
// ForEach quand les elements DIVERGENT (des pieces exportees separement) ; la
// boucle reste DANS le noeud quand ils se repetent.
//
// GetPassCount() rend ce cout observable : n elements coutent n passes, et un
// test qui compte les passes distingue une divergence d'une repetition.
//
// Ports : un par frontiere du document, de type SUITE des deux cotes. Le
// document, lui, voit un ELEMENT -- c'est tout ce que le noeud fait.
//
#include "host.h"

namespace cggraph_nodes
{
namespace flow
{

class ForEachNode : public SubgraphHostNode
{
public:
	ForEachNode ();

	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

protected:
	void PublishPorts (const SubgraphInstance &instance) override;
};

} // namespace flow
} // namespace cggraph_nodes
