#pragma once

#include <string>
#include <vector>

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  LE CATALOGUE DES FORMATS IMPORTABLES, DETYPE
// ---------------------------------------------------------------------------
//
// Meme contenu que sinaia/SupportedFormats.h, sans wxString ni wxChar. Le
// catalogue de sinaia est deja factorise -- il est la source unique du wildcard
// du dialogue d'ouverture ET du filtre du panneau des fichiers -- mais il est
// TYPE : le reutiliser tel quel ferait entrer wxWidgets dans peles_core.
//
// ⚠ DEUX LISTES QUI DOIVENT RESTER EGALES, ET LE MECANISME QUI LE TIENT NE
// TOURNE PAS PARTOUT. test/tu_supported_formats.cpp compare les DEUX ensembles
// d'extensions sous la meme configuration de macros, mais il n'existe QUE sous
// ENABLE_SINAIA=ON **et** ENABLE_PELES=ON -- il exige wxWidgets pour que
// sinaia/SupportedFormats.h soit incluable.
//
// ⚠ AUCUNE CONFIGURATION DE CI NE L'EXECUTE A CE JOUR : le preset ci-linux a
// ENABLE_SINAIA=OFF et ci-linux-peles n'installe pas wxWidgets. Sa seule voie
// d'execution est une construction locale sous les deux options. Hors de cette
// configuration, le test n'est pas absent en silence : il rend un GTEST_SKIP qui
// NOMME la configuration manquante.
//
// ⚠ LES GARDES CG_HAS_* SONT LA MEME CONDITION DES DEUX COTES. Quatre
// importateurs de cgmesh sont conditionnels ; compiles sans leur dependance ils
// retombent sur un stub qui rend false. Annoncer un format qu'on echouera a
// ouvrir serait trompeur, donc chaque famille concernee porte la garde de son
// importateur. Les macros sont PUBLIC sur la cible cgmesh, donc visibles ici.

// Famille d'icone d'un format.
//
// ⚠ ELLE NE DESIGNE PLUS UN EMPLACEMENT DANS UNE LISTE D'IMAGES, mais une
// CLASSE RCSS. L'ordre des valeurs n'a donc plus aucune importance -- alors
// qu'il en avait une dans sinaia, ou il devait suivre l'ordre d'ajout des
// bitmaps dans le constructeur de la frame.
enum class FileKind
{
	Obj,
	Stl,
	Generic
};

// Une famille de format importable : un libelle, une ou plusieurs extensions
// qui le partagent, et la classe d'icone.
struct Format3D
{
	std::string              label;
	std::vector<std::string> extensions; // minuscules, SANS le point
	FileKind                 kind = FileKind::Generic;
};

// L'unique endroit a editer pour ajouter ou retirer un format.
const std::vector<Format3D>& SupportedFormats ();

// Classe RCSS de la famille : "kind-obj", "kind-stl", "kind-generic".
const char* KindClass (FileKind kind);

// Classe RCSS de l'extension donnee (avec ou sans point, casse indifferente),
// ou une chaine VIDE si l'extension n'est pas au catalogue.
//
// ⚠ LA CHAINE VIDE EST LE FILTRE. C'est le pendant du -1 de
// sinaia::IconForExtension : une extension absente du catalogue ne s'affiche
// pas, elle ne s'affiche pas « avec l'icone generique ».
std::string KindClassForExtension (const std::string& extension);

bool IsSupportedExtension (const std::string& extension);

// Minuscules ASCII, SANS std::locale.
//
// ⚠ L'ABSENCE DE LOCALE EST CE QUI REND LES VIDAGES REPRODUCTIBLES. Une
// comparaison dependante de la locale ferait dependre l'ordre des lignes de
// l'environnement d'execution, et l'oracle cesserait d'etre un oracle. Les
// octets hors ASCII sont laisses tels quels : ils ne sont pas convertis a
// l'aveugle, ils sont simplement ordonnes par leur valeur.
std::string ToLowerAscii (std::string text);

// Ordre total, deterministe, insensible a la casse ASCII.
//
// ⚠ LE DEPART D'EGALITE PAR LES OCTETS BRUTS N'EST PAS DECORATIF : sans lui,
// "A.obj" et "a.obj" seraient equivalents et leur ordre relatif dependrait de
// l'algorithme de tri, donc de l'implementation de la bibliotheque standard.
//
// ⚠ UN SEUL COMPARATEUR POUR L'ARBRE ET POUR LA LISTE. Deux copies
// divergeraient a la premiere correction, et rien ne le dirait : les deux
// vidages resteraient reproductibles chacun de son cote.
bool NameLess (const std::string& left, const std::string& right);

} // namespace core
} // namespace peles
