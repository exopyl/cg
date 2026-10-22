#pragma once
//
//  Catalogue REUNI des formes parametriques sans ressource.
//
// Deux listes de fabriques, tenues par deux proprietaires : celle de cgmesh
// (ParametricShapes, parametric_catalog.h) et celle des formes gothiques
// (GothicParametricShapes, gothic_parametric_catalog.h). La reunion est faite
// ICI, chez le consommateur, et nulle part ailleurs : ni enregistrement
// statique, ni registre mutable. L'adaptateur generique et les pages de maker
// lisent cette liste-ci.
//
// Ordre stable : les formes de cgmesh dans leur ordre, puis les formes
// gothiques dans le leur. Les noms sont uniques sur la reunion (verifie par
// test) ; c'est la cle de recherche.
//
#include <memory>
#include <string>
#include <vector>

#include "../../../cgmesh/parametric_catalog.h"   // ParametricShapeEntry

namespace cggraph_nodes
{

// Adresses stables pour la duree du programme.
const std::vector<ParametricShapeEntry> &CatalogShapes ();

// Nul si le nom est inconnu. Ne CONSTRUIT rien : le constructeur de chaque
// forme appelle Regenerate ().
const ParametricShapeEntry *FindCatalogShape (const std::string &name);

// Nul si le nom est inconnu.
std::unique_ptr<IParameterized> MakeCatalogShape (const std::string &name);

} // namespace cggraph_nodes
