#pragma once

#include <string>

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  LES OBJETS DE TRANSFERT VERS L'INTERFACE
// ---------------------------------------------------------------------------
//
// ⚠ MEMBRES PUBLICS, NON `const`, ET AUCUN HERITAGE. Ce sont les TROIS
// conditions que le data binding de RmlUi impose, et c'est l'UNIQUE raison
// d'etre de ces structures : il ne lie ni un membre herite, ni un membre
// `const`, ni un membre prive. Ce n'est pas un choix d'elegance -- un DTO qui
// deriverait d'une base commune, ou qui rendrait `path` constant, cesserait
// simplement d'etre liable, sans erreur de compilation et sans rien afficher.
//
// ⚠ CE SONT DES COPIES, ET L'INTERFACE NE VOIT RIEN D'AUTRE. RmlUi ne recoit
// jamais un Mesh, un Model ni un Material : ces derniers sont polymorphes et
// possedes ailleurs, donc ni liables ni sûrs a exposer.

// Une ligne de l'arbre de repertoires, dans la REPRESENTATION PLATE.
//
// ⚠ PLATE, PAS ARBORESCENTE, et ce n'est pas une simplification : RmlUi n'a pas
// de `data-for` recursif. L'arbre reel vit dans DirectoryTree ; ce qui est lie a
// l'interface est la SUITE DES LIGNES VISIBLES, recalculee par la seule
// operation qui la change -- Toggle.
struct TreeRow
{
	// Identifiant STABLE du noeud, a rendre a DirectoryTree::Toggle. Vaut -1
	// sur une ligne qui ne designe aucun noeud (ligne de debordement) : une
	// telle ligne n'est pas ouvrable.
	int id = -1;

	int depth = 0;

	// Nom du repertoire SEUL. Le chemin complet est a cote, mais il n'est pas
	// affiche : sur un arbre profond il deborde la zone bien avant d'etre lu.
	std::string label;

	// ⚠ CHAINE D'AFFICHAGE, EN UTF-8, ET RIEN D'AUTRE. Le repertoire se rouvre
	// par DirectoryTree::PathOf(id), qui rend un std::filesystem::path : une
	// conversion de chemin est destructive sous Windows (cf. core/path_text.h).
	std::string path;

	// LISTE COMPLETE DES CLASSES RCSS de la ligne, separees par des espaces :
	// profondeur (`d0`…`dN`), etat (`open`, `closed`, `error`, `overflow`),
	// `expandable` et `drive` (cf. les champs du meme nom), et selection.
	//
	// ⚠ LE CHEVRON ET L'ICONE SONT DES REGLES DE FEUILLE DE STYLE qui lisent
	// ces classes : aucun champ ne designe une image.
	//
	// ⚠ COMPLETE, ET C'EST UNE OBLIGATION : `data-attr-class` ECRASE l'attribut
	// `class` de l'element. Une classe posee en dur dans le RML a cote d'un
	// `data-attr-class` serait perdue a la premiere mise a jour du modele.
	std::string styleClass;

	bool expanded = false;

	// Faux sur une ligne en erreur et sur une ligne de debordement.
	//
	// ⚠ UN NOEUD EN ECHEC D'ACCES EST AFFICHE ET NON OUVRABLE. Le masquer
	// ferait lire « repertoire vide » la ou l'acces a ete refuse ; le traiter
	// comme une panne ferait tomber l'ouverture du parent entier, alors qu'un
	// seul de ses enfants est en cause.
	bool openable = false;
	bool error    = false;

	// Vrai quand la ligne porte un chevron : noeud PAS ENCORE ENUMERE, ou
	// enumere avec au moins une ligne a montrer dessous (enfant, entrees
	// illisibles). Faux sur un repertoire enumere vide, sur une ligne en erreur
	// et sur une ligne de debordement.
	//
	// ⚠ AVANT ENUMERATION, LE CHEVRON EST UNE PROMESSE, PAS UN CONSTAT. Savoir
	// si un repertoire a des sous-repertoires oblige a l'ouvrir : le faire pour
	// chaque ligne affichee couterait une enumeration par enfant a chaque
	// ouverture, jusqu'a kMaxChildrenPerNode, lecteurs reseau compris. Le
	// chevron disparait donc a la premiere ouverture d'un repertoire vide --
	// comportement de l'explorateur Windows.
	//
	// ⚠ DISTINCT DE `openable` : un repertoire enumere vide reste ouvrable (il
	// se selectionne en s'ouvrant), il n'a simplement rien a deplier.
	bool expandable = false;

	// Racine de SetSystemRoots : icone de lecteur au lieu de dossier.
	bool drive = false;

	bool selected = false;
};

// Une ligne de la liste de fichiers.
struct FileRow
{
	// Rang du fichier dans FileList, a rendre a FileList::PathOf et
	// FileList::Select. -1 : aucun.
	//
	// ⚠ C'EST LUI QUI DESIGNE LE FICHIER, PAS `path`. Sous Windows, la chaine
	// d'affichage est une conversion du chemin natif : elle sert a LIRE, jamais
	// a rouvrir (cf. core/path_text.h).
	int id = -1;

	std::string label;

	// ⚠ CHAINE D'AFFICHAGE, EN UTF-8, ET RIEN D'AUTRE. L'identite du fichier
	// vit dans FileList, sous la forme d'un std::filesystem::path.
	std::string path;

	// LISTE COMPLETE DES CLASSES RCSS de la ligne : la famille de format
	// ("kind-obj", "kind-stl", "kind-generic") et, le cas echeant, "selected".
	//
	// ⚠ LA FAMILLE REMPLACE L'INDEX D'ICONE. L'icone devient une regle de
	// feuille de style, donc editable sans recompiler ; la liste d'images, les
	// bitmaps et leur mise a l'echelle disparaissent avec elle.
	//
	// ⚠ COMPLETE, ET C'EST UNE OBLIGATION : `data-attr-class` ECRASE l'attribut
	// `class` de l'element -- meme regle que TreeRow::styleClass. La selection ne
	// peut donc pas etre portee par une classe posee ailleurs.
	std::string kind;

	bool selected = false;
};

// ---------------------------------------------------------------------------
//  UNE LIGNE DE PANNEAU D'INFORMATIONS
// ---------------------------------------------------------------------------
//
// Le MEME type sert aux trois zones de droite : la table de proprietes, l'arbre
// des maillages et l'arbre des materiaux. Les deux arbres sont APLATIS, comme
// celui des repertoires et pour la meme raison -- RmlUi n'a pas de `data-for`
// recursif -- et ils sont produits ENTIEREMENT DEPLIES, ce qui est l'etat dans
// lequel l'hote historique les laisse.
//
// ⚠ C'EST ICI QUE LE POLYMORPHISME DE `Material` S'ARRETE. Material est une
// hierarchie a quatre sous-classes, atteintes par dynamic_cast ; RmlUi ne lie
// ni un membre herite, ni un membre virtuel, ni un type polymorphe. Ce DTO
// n'est donc pas une commodite : sans lui, l'arbre des materiaux ne serait pas
// liable du tout.
struct InfoRow
{
	// Colonne GAUCHE de la table de proprietes ; texte COMPLET de la ligne dans
	// les deux arbres.
	std::string label;

	// Colonne DROITE de la table. Vide sur une ligne d'arbre.
	std::string value;

	int depth = 0;

	// LISTE COMPLETE DES CLASSES RCSS, separees par des espaces : profondeur
	// (`d0`…`dN`), `sw` quand la ligne porte une pastille, `overflow` sur la
	// ligne de debordement.
	//
	// ⚠ COMPLETE, ET C'EST UNE OBLIGATION : `data-attr-class` ECRASE l'attribut
	// `class` de l'element.
	std::string styleClass;

	// COULEUR DE LA PASTILLE, sous la forme "#rrggbb".
	//
	// ⚠ ELLE REMPLACE A ELLE SEULE MakeColorSwatch (25 lignes de GDI), la
	// wxImageList qui recevait une image par materiau, et la purge de cette
	// liste a chaque reconstruction. Zero ligne de dessin, zero liste d'images,
	// zero fuite.
	//
	// ⚠ JAMAIS VIDE : la valeur est "transparent" quand la ligne ne porte pas
	// de pastille. Le champ est consomme par `data-style-background-color`, et
	// Element::SetProperty JOURNALISE EN LT_WARNING toute valeur qu'il ne sait
	// pas analyser -- une chaine vide en est une. Le compteur d'avertissements
	// de l'hote en ferait une faute de configuration.
	std::string swatch = "transparent";

	// Vrai sur la seule ligne dont le texte doit etre selectionnable.
	//
	// ⚠ RmlUi N'A PAS D'INFOBULLE : le chemin complet, qui etait le
	// SetHelpString de la ligne « File », devient une ligne a part entiere. Un
	// texte de ligne ordinaire n'y est ni selectionnable ni copiable ; la seule
	// forme qui le soit est un controle de saisie.
	bool copyable = false;
};

} // namespace core
} // namespace peles
