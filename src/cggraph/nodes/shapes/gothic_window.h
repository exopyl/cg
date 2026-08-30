#pragma once
//
//  GothicWindow -- le descripteur ECRIT A LA MAIN, et pourquoi il l'est.
//
// La fenetre gothique est dans le meme catalogue de formes que le cube ; elle
// est pourtant le seul objet a ne PAS passer par l'adaptateur generique, et le
// motif est mesurable : elle expose 22 parametres plats, dont un « Profile » a
// cinq positions -- Flat, Chamfer, et trois barres. Or un profil de moulure est
// une COURBE : une ebrasure a sa pente, une doucine sa courbure. L'ecraser en
// cinq positions est la limite du modele plat, et l'adaptateur generique, qui
// aplatit ce qu'il recoit, la reconduirait telle quelle.
//
// Ce descripteur PROMEUT donc ce sous-objet geometrique en PORTS :
//
//   entree « profil d'ebrasement » (optionnelle) : une polyligne ouverte, qui
//     remplace la position « Chamfer » ;
//   entree « profil de barre » (optionnelle) : une section fermee, qui remplace
//     les trois positions de barre.
//
// Consequences, et elles sont le gain :
//
//  - la famille est OUVERTE. Ajouter une moulure n'ajoute plus une position a
//    une enumeration compilee dans deux fichiers : c'est un noeud de plus, et il
//    sert aussi bien au biseau du texte ou a un balayage le long d'un chemin ;
//  - le profil est mis en cache POUR LUI-MEME. Changer la largeur de la baie ne
//    refait pas la moulure, et l'inverse ;
//  - le parametre « Profile » est ABSENT de ce descripteur. Le laisser aurait
//    fait deux mecanismes pour une meme chose, dont l'un aurait silencieusement
//    perdu. Les deux ports vides redonnent la position « Flat ».
//
// Les 21 autres parametres restent PLATS, et c'est le bon niveau : ce sont des
// nombres, des booleens et des choix, ils ne produisent aucune geometrie
// reutilisable. La recursion (0..2) reste elle aussi INTERNE : la structure
// engendree depend de la geometrie locale a chaque niveau, ce n'est pas la
// repetition d'un sous-graphe.
//
#include <memory>
#include <string>

#include "../../core/node.h"

namespace cggraph_nodes
{

class GothicWindowNode : public cggraph::Node
{
public:
	GothicWindowNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
