#pragma once
//
//  Document du graphe -- JSON versionne.
//
// Un graphe se sauve, se recharge et se rejoue sans interface. Trois proprietes
// dictent la forme de ce fichier :
//
//  - le moteur ne sait construire AUCUN noeud : les types de noeuds
//    appartiennent aux couches de domaine. La relecture passe donc par une
//    fabrique fournie par l'appelant, ce qui permet a ce module de ne nommer
//    aucun domaine ;
//  - l'identite d'un type est une adresse de descripteur en memoire, et une
//    adresse ne survit pas a un arret du programme. Ce qui se serialise est donc
//    le NOM du type de noeud ; les types de PORTS ne se serialisent pas du tout,
//    puisqu'ils se rededuisent du descripteur du noeud reconstruit ;
//  - le versionnement est a DEUX etages, et ils ne mesurent pas la meme chose :
//    la version du FORMAT (ce fichier) et la version de chaque TYPE de noeud
//    (NodeDesc::version). Un document ecrit sous un format plus recent est
//    refuse en entier ; un noeud ecrit sous une autre version de son type est
//    relu -- il reste visible, deplacable, relie -- et l'evaluateur le refuse.
//
// Schema, et c'est le seul endroit ou il soit ecrit :
//
//   {
//     "format": "cggraph",
//     "formatVersion": 1,
//     "nodes": [
//       {
//         "id": 1,                    entier non nul, repris tel quel
//         "type": "domaine.operation",
//         "version": 1,               version du TYPE de noeud
//         "x": 0.0, "y": 0.0,         position d'ecran, hors signature
//         "subgraph": "autre.json",   OPTIONNEL -- absent = ne delegue rien
//                                     Une CHAINE designe un fichier ; un OBJET
//                                     est le document lui-meme, embarque, et
//                                     rend le parent autonome.
//         "params": [
//           { "name": "n", "role": "semantic", "kind": "literal",
//             "type": "int", "value": 3 },
//           { "name": "w", "role": "semantic", "kind": "driven",
//             "type": "float", "expression": "largeur / 2" }
//         ]
//       }
//     ],
//     "links": [ { "from": 1, "fromPort": 0, "to": 2, "toPort": 0 } ]
//   }
//
// "subgraph" est porte des maintenant bien qu'aucun type de noeud ne
// l'interprete : ajouter un champ a un format apres coup oblige a migrer tous
// les documents deja ecrits.
//
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "graph.h"

namespace cggraph
{

// Version du FORMAT de document. Sans rapport avec NodeDesc::version, qui
// versionne un TYPE de noeud.
static const int kDocumentFormatVersion = 1;

class NodeFactory
{
public:
	virtual ~NodeFactory () = default;

	// Nul si le type est inconnu. Le refus est un cas nominal : un document peut
	// nommer un type qu'un binaire plus ancien ne publie pas.
	virtual std::unique_ptr<Node> Create (const std::string &typeName) const = 0;
};

// Toute sortie d'echec est un statut nomme, jamais un booleen : un refus pour
// « type inconnu » ne doit pas pouvoir passer pour un refus de lien.
enum class SerializeStatus
{
	Ok,
	FileNotReadable,
	FileNotWritable,
	ParseError,
	NotAGraph,
	UnsupportedFormat,
	GraphNotEmpty,
	BadNode,
	DuplicateNodeId,
	UnknownNodeType,
	BadLink
};

struct LoadResult
{
	SerializeStatus status = SerializeStatus::Ok;

	// Nomme ce qui est en cause : le type refuse, le champ manquant, le lien.
	// Un diagnostic muet ne se distingue pas d'un plantage pour qui le lit.
	std::string detail;

	// Noeuds relus dont la version de document differe de celle de leur
	// descripteur courant. Ils sont DANS le graphe -- c'est le « visible mais
	// non calculable » : l'evaluateur les refuse par un statut nomme, et une
	// re-sauvegarde conserve leur version d'origine.
	std::vector<NodeId> incompatible;

	bool IsOk () const { return status == SerializeStatus::Ok; }
};

// Le texte produit est stable : deux appels sur le meme graphe rendent les
// memes octets, et sauver un graphe relu rend le document dont il vient.
std::string SaveGraph (const Graph &graph);

// `graph` doit etre VIDE : la relecture reprend les identifiants du document,
// et fusionner deux jeux d'identifiants produirait des liens faux en silence.
LoadResult LoadGraph (const std::string &text, const NodeFactory &factory, Graph &graph);

SerializeStatus SaveGraphToFile (const Graph &graph, const std::string &path);
LoadResult LoadGraphFromFile (const std::string &path, const NodeFactory &factory, Graph &graph);

// Pour un diagnostic lisible en tete haute.
const char *ToString (SerializeStatus status);

} // namespace cggraph
