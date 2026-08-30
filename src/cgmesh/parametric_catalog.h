#pragma once

// ============================================================================
//  Catalogue des formes parametriques -- une seule liste de fabriques
// ============================================================================
//
// Les formes de parameterized_shapes.h qui se construisent SANS RESSOURCE : un
// constructeur par defaut, des parametres, un maillage. Ce sont exactement
// celles qu'un adaptateur generique sait emballer, et c'est pourquoi la liste
// vit ici plutot que chez l'un de ses consommateurs -- les pages de maker et la
// couche nodale la lisent toutes les deux, et deux listes auraient diverge au
// premier ajout.
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
// Le NOM est celui que rend GetName() : c'est la cle du catalogue, et les deux
// consommateurs s'en servent pour retrouver une forme.
//
#include <memory>
#include <string>
#include <vector>

#include "parameterized.h"

struct ParametricShapeEntry
{
	// == GetName() de l'objet construit. Verifie par test.
	const char *name = nullptr;

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
