#pragma once
//
//  Fabrique de noeuds pour la relecture d'un document.
//
// Le moteur ne sait construire aucun noeud ; c'est ici que le nom serialise
// redevient une instance. La fabrique n'est rien d'autre que le catalogue vu
// par l'interface que le moteur demande : une SECONDE liste de types serait une
// liste a maintenir en double, et elle divergerait.
//
#include "../core/serialize.h"
#include "catalog.h"

namespace cggraph_nodes
{

class CatalogFactory : public cggraph::NodeFactory
{
public:
	std::unique_ptr<cggraph::Node> Create (const std::string &typeName) const override
	{
		return MakeNode (typeName);
	}
};

} // namespace cggraph_nodes
