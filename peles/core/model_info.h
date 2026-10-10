#pragma once

#include "topology_stats.h"
#include "ui_dto.h"

#include <vector>

class Model;

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  LES PANNEAUX D'INFORMATIONS, COMME FONCTIONS PURES
// ---------------------------------------------------------------------------
//
// ⚠ AUCUN ETAT, AUCUN GL, AUCUN RmlUi. Ces fonctions prennent un modele et
// rendent les lignes a afficher, rien d'autre. C'est cette forme qui donne au
// panneau un ORACLE TEXTUEL : --dump-info appelle exactement ce que l'interface
// appelle, donc ce que la console imprime est ce que l'ecran montre.
//
// ⚠ LE FORMAT DES NOMBRES EST CELUI DE L'HOTE HISTORIQUE, A LA LETTRE :
// « [ %.4g , %.4g ] » pour une etendue de boite englobante, « %.4g » pour une
// taille, « %.2f » pour une composante de couleur, « %.3f » pour un facteur.
// Sans cette identite, les deux sorties ne seraient comparables qu'a l'oeil.

// PLAFOND DE LIGNES D'UN ARBRE D'INFORMATIONS.
//
// ⚠ MEME GRANDEUR ET MEME VALEUR QUE kMaxVisibleRows : le cout mesure d'une
// ligne liee est de l'ordre de 7,7 µs quelle que soit la zone, et un fichier a
// plusieurs centaines de materiaux produit plusieurs milliers de lignes de
// detail. Le plafond borne le TOTAL, seule grandeur dont le cout depende.
inline constexpr int kMaxInfoRows = 1000;

// PROFONDEUR MAXIMALE AYANT UNE CLASSE RCSS PROPRE. Au-dela, la ligne reprend
// la derniere classe : elle ne perd pas son style, elle cesse seulement de
// s'indenter davantage.
inline constexpr int kMaxInfoDepthClass = 4;

// ---------------------------------------------------------------------------
//  LES QUATORZE LIGNES, NOMMEES
// ---------------------------------------------------------------------------
//
// ⚠ L'ORDRE EST TENU A DEUX ENDROITS -- BuildEmptyModelInfo qui construit le
// vecteur, BuildModelInfo qui y ecrit -- et ce sont ces noms qui les couplent.
// Des indices nus ne les rapprochaient que par des nombres : une ligne inseree
// au milieu deplacait les suivantes d'un cote sans l'autre, et le test de garde
// n'aurait rien vu -- il compare les LIBELLES, donc il verrait une permutation,
// pas une valeur posee sur la mauvaise ligne.
enum class InfoLine
{
	File = 0,
	Vertices,
	Faces,
	Meshes,
	TriangleMesh,
	Borders,
	NonManifoldEdges,
	BoundingBoxX,
	BoundingBoxY,
	BoundingBoxZ,
	SizeX,
	SizeY,
	SizeZ,
	Path,

	count
};

// LES QUATORZE LIGNES DE LA TABLE DE PROPRIETES, VALEURS VIDES.
//
// ⚠ UN MODELE ABSENT N'EST PAS UN VECTEUR VIDE. Un vecteur vide ferait
// disparaitre la table, donc changer la hauteur du panneau a chaque
// deselection : la disposition sauterait. Les quatorze lignes sont toujours la,
// et ce sont leurs VALEURS qui s'effacent -- c'est aussi ce que fait l'hote
// historique, qui remet chaque propriete a la chaine vide ou a zero.
std::vector<InfoRow> BuildEmptyModelInfo ();

// LES QUATORZE MEMES LIGNES, RENSEIGNEES.
//
// ⚠ `model` EST PRIS PAR REFERENCE NON CONSTANTE : la boite englobante est
// RECALCULEE ici, comme l'hote historique le fait, plutot que relue d'un champ
// qu'un traitement aurait pu perimer.
std::vector<InfoRow> BuildModelInfo (Model& model, const TopologyStats& topology);

// L'ARBRE DES MAILLAGES, APLATI : racine, un noeud par maillage, deux feuilles.
//
// ⚠ TRONQUE A kMaxInfoRows, ET LA TRONCATURE EST DITE. Un arbre ecourte sans
// ligne qui l'annonce se lit comme un modele qui n'a pas plus de maillages,
// pendant que la ligne « Meshes » de la table affiche le vrai total : deux
// nombres contradictoires dans le meme panneau. La derniere ligne porte alors
// la classe `overflow` et le nombre de maillages non affiches.
//
// ⚠ `model` EST PRIS PAR REFERENCE NON CONSTANTE POUR UNE RAISON HERITEE :
// `VMeshes::GetMeshes()` n'a pas de surcharge `const`, donc il n'existe aucun
// moyen d'enumerer les maillages d'un `const Model&`. Ce n'est pas un oubli de
// `const`-correction ici, c'est la contrainte de l'interface de `cgmesh`.
std::vector<InfoRow> BuildMeshTree (Model& model);

// L'ARBRE DES MATERIAUX, APLATI : racine, maillage, materiau, details.
//
// ⚠ C'EST ICI QUE LE dynamic_cast VIT, ET IL N'EN SORT PAS. Les quatre branches
// de type -- MaterialColor, MaterialTexture, MaterialColorExt, MaterialPbr --
// sont resolues ici ; ce qui en sort est une suite de lignes de texte et de
// couleurs, c'est-a-dire ce que le liage de donnees sait porter.
//
// ⚠ MEME TRONCATURE PARLANTE QUE BuildMeshTree, ET MEME CONTRAINTE `const`
// heritee de `VMeshes::GetMeshes()`.
std::vector<InfoRow> BuildMaterialTree (Model& model);

// Nombre de materiaux de tous les maillages du modele. Sert au verdict de
// completude de l'arbre des materiaux : un arbre tronque par le plafond se
// distingue ainsi d'un modele qui n'a pas de materiaux.
//
// ⚠ MEME CONTRAINTE `const` heritee de `VMeshes::GetMeshes()` : compter ne
// modifie rien, mais enumerer exige une reference non constante.
int CountMaterials (Model& model);

} // namespace core
} // namespace peles
