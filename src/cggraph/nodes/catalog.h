#pragma once
//
//  Catalogue -- la liste des types de noeuds que cette couche publie.
//
// Une entree par type de noeud, et rien de plus : ce que la palette, la
// serialisation et l'autocompletion liront plus tard se derive d'ici, pas d'une
// enumeration recopiee ailleurs.
//
// Ce fichier est le SEUL de la couche qui connaisse tous les domaines a la
// fois. C'est sa fonction -- agreger --, et c'est aussi pourquoi il vit a la
// racine : si mesh/ ou text/ tenait la liste, l'autre en deviendrait le client
// et le rangement par domaine cesserait d'etre un simple rangement.
//
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../core/node.h"

namespace cggraph_nodes
{

struct CatalogEntry
{
	const char *typeName = nullptr;
	const char *label = nullptr;
	const char *category = nullptr;
	// std::function et non pointeur de fonction : l'adaptateur generique fabrique
	// 26 types de noeuds a partir d'UNE classe, si bien que sa fabrique doit
	// porter la forme qu'elle emballe. Un pointeur de fonction nu ne peut rien
	// porter, et il aurait fallu 26 fonctions triviales pour dire cela.
	std::function<std::unique_ptr<cggraph::Node> ()> make;

	// Ce que le CORPS emballe fait et que sa declaration ne dit pas : code de
	// retour trompeur, garantie plus faible que son nom, degradation muette.
	// Nul quand la lecture du corps n'a rien trouve a signaler -- l'absence est
	// donc une affirmation, pas un oubli.
	const char *caveat = nullptr;
};

const std::vector<CatalogEntry> &Catalog ();

// Nul si le type est inconnu. L'identifiant est celui du descripteur, celui qui
// se serialise.
std::unique_ptr<cggraph::Node> MakeNode (const std::string &typeName);

} // namespace cggraph_nodes
