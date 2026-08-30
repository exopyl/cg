#pragma once
//
//  flow.repeat -- applique n fois le document delegue, en chainant.
//
// La boucle bornee, exprimee SANS cycle : le graphe reste un DAG, le retour
// arriere vit DANS le noeud. C'est la reponse de nodal.md §11.2, et c'est aussi
// pourquoi Graph::Connect n'a besoin d'aucune exception -- il refuse tous les
// cycles, y compris ceux qu'une boucle aurait exiges, parce qu'aucune boucle
// n'en demande.
//
// ⚠ n ENTRE DANS LA SIGNATURE, et il n'y a rien a ecrire pour cela : c'est un
// parametre Semantic, donc HashParamSet le hache comme les autres. Le critere
// 9.1 est satisfait par le mecanisme de l'etape 1, pas par un ajout de celle-ci
// -- et le test qui le verifie est ce qui distingue les deux.
//
// PORTS FIXES, UN ENTRANT ET UN SORTANT, contrairement aux deux autres. Ce n'est
// pas un raccourci : chainer exige que la sortie d'une passe soit l'entree de la
// suivante, donc une correspondance un pour un. Un descripteur derive du
// document pourrait publier trois entrees et deux sorties, forme que le corps ne
// saurait pas tenir -- c'est exactement ce que la regle du gabarit interdit
// (nodal.md §8.1bis). Le document doit donc porter UNE frontiere de chaque cote,
// et le calcul refuse s'il n'en est pas ainsi.
//
#include "host.h"

namespace cggraph_nodes
{
namespace flow
{

class RepeatNode : public SubgraphHostNode
{
public:
	RepeatNode ();

	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

protected:
	void PublishPorts (const SubgraphInstance &instance) override;
};

} // namespace flow
} // namespace cggraph_nodes
