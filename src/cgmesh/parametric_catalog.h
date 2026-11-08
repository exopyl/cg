#pragma once

// ============================================================================
//  Catalogue des formes parametriques de cgmesh
// ============================================================================
//
// Les formes de parameterized_shapes.h qui se construisent SANS RESSOURCE : un
// constructeur par defaut, des parametres, un maillage. Ce sont exactement
// celles qu'un adaptateur generique sait emballer.
//
// Cette liste ne contient QUE les formes de cgmesh. Les consommateurs -- les
// pages de maker et la couche nodale -- ne la lisent pas directement : ils
// lisent la reunion cggraph_nodes::CatalogShapes () (shapes/shape_catalog.h),
// qui la complete par les formes gothiques (gothic_parametric_catalog.h).
//
// ⚠ N'Y FIGURENT PAS les formes dont le constructeur prend un FICHIER --
// extrusion SVG, quantification d'image, blocs pixelises, texte 3D, surface
// implicite depuis un nuage. Ce n'est pas un oubli, c'est le critere : leur
// ressource n'apparait dans aucun GetParameters(), donc un adaptateur generique
// en ferait un objet sans entree, dont la ressource determinante echapperait a
// toute identite de contenu. Elles se construisent par une fabrique dediee, qui
// nomme le fichier.
//
// Instrument de cette frontiere, exact :
//   grep -c "explicit Parameterized.*(const std::string" parameterized_shapes.h
// plus ParameterizedText3D, dont le constructeur a deux arguments n'est pas
// `explicit`.
//
// Le NOM est celui que rend GetName() : c'est la cle de recherche, unique sur
// la reunion.
//
// L'IDENTIFIANT et la CATEGORIE de chaque forme sont ecrits dans la liste de
// son proprietaire, et nulle part ailleurs : les consommateurs les lisent ici.
//
#include <memory>
#include <string>
#include <vector>

#include "parameterized.h"

struct ParametricShapeEntry
{
	// Identifiant stable, unique sur la reunion : c'est le type du noeud de la
	// forme dans les documents cggraph (« shape.cube »). Il ne change jamais,
	// meme si le nom change : les documents deja ecrits le portent.
	const char *id = nullptr;

	// == GetName() de l'objet construit. Verifie par test.
	const char *name = nullptr;

	// Famille de formes, qui partitionne la reunion entre les pages de maker
	// (listShapes). Chaque liste proprietaire ecrit la sienne : « parametric »
	// pour ParametricShapes ().
	const char *category = nullptr;

	std::unique_ptr<IParameterized> (*make) () = nullptr;
};

// Ordre stable : celui dans lequel les pages de maker presentent les formes.
const std::vector<ParametricShapeEntry> &ParametricShapes ();

// Nul si le nom est inconnu. Ne CONSTRUIT rien : le constructeur de chaque
// forme appelle Regenerate(), donc instancier pour savoir si un nom existe
// coute un maillage jete.
const ParametricShapeEntry *FindParametricShape (const std::string &name);

// Nul si le nom est inconnu.
std::unique_ptr<IParameterized> MakeParametricShape (const std::string &name);
