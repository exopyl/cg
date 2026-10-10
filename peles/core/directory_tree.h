#pragma once

#include "ui_dto.h"

#include <filesystem>
#include <string>
#include <vector>

namespace peles
{
namespace core
{

// PLAFOND D'ENFANTS AFFICHES PAR NOEUD.
//
// ⚠ MESURE, ET C'EST ELLE QUI FIXE LE CHIFFRE. Instrument :
// `peles --frames 120 --dump-timing --browse <dir> --browse-depth n`, qui rend
// le temps de la SEULE phase 2 (`Rml::Context::Update()`) sur la premiere image
// -- celle ou les lignes sont construites et mises en page. Le cout est
// LINEAIRE dans le nombre de lignes liees, ≈ 7,7 µs par ligne : 1000 lignes
// coutent ≈ 9 ms, 2002 lignes coutent 16,1 a 18,5 ms, soit AU-DESSUS du repere
// de 16,6 ms. Un plafond dont le pire cas depasse le budget ne borne rien
// d'utile : d'ou 1000 et non 2000.
inline constexpr int kMaxChildrenPerNode = 1000;

// PLAFOND GLOBAL DE LIGNES VISIBLES.
//
// ⚠ IL BORNE LA GRANDEUR QUE LA MESURE FAIT VARIER, ce que le plafond par noeud
// ne fait PAS : le cout est lineaire dans le TOTAL des lignes liees, et trois
// repertoires de 1000 sous-repertoires ouverts a la profondeur 1 en produisent
// 3003 sans qu'aucun noeud ne depasse son propre plafond. Meme instrument, meme
// chiffre : ≈ 9 ms a 1000 lignes.
//
// ⚠ AUCUN COMPTE N'ACCOMPAGNE LA LIGNE DE DEBORDEMENT GLOBALE, et c'est une
// consequence du plafond lui-meme : connaitre le nombre de lignes supprimees
// obligerait a construire la suite, c'est-a-dire a payer ce que le plafond
// existe pour ne pas payer.
inline constexpr int kMaxVisibleRows = 1000;

// ⚠ LES DEUX PLAFONDS ETANT EGAUX, LA LIGNE « ... N entree(s) non affichee(s) »
// DU PLAFOND PAR NOEUD EST INATTEIGNABLE, et c'est mesure. Un noeud qui depasse
// son propre plafond porte au moins 1001 enfants, donc le TOTAL franchit le
// plafond global avant que sa ligne de debordement ne soit posee : c'est la
// ligne globale qui sort. Mesure : `--dump-tree` sur un repertoire de 1200
// sous-repertoires rend 1 ligne « plafond de 1000 ligne(s) atteint » et
// 0 ligne « non affichee(s) ».
//
// Elle redevient atteignable des que le plafond par noeud passe SOUS
// kMaxVisibleRows. Le code des deux lignes est conserve pour cette raison : il
// n'est pas mort, il est masque par l'egalite des deux chiffres, et le choix de
// ces chiffres appartient au lot qui reglera les plafonds.
//
// ⚠ ELLE EST EXERCEE PAR UN TEST, PAS PAR LE PRODUIT. DirectoryTree::
// SetMaxChildrenPerNode abaisse le plafond par noeud sans toucher aux deux
// constantes mesurees ; test/tu_peles_core.cpp le pose a 3 et VOIT la ligne.
// Le binaire livre, lui, ne la produit toujours pas.

// Profondeur maximale ayant une classe d'indentation propre. Au-dela,
// l'indentation cesse de croitre : la feuille de style declare un nombre FINI
// de classes, et une ligne sans classe correspondante ne serait pas indentee du
// tout -- ce qui se lit comme une ligne de profondeur 0.
inline constexpr int kMaxDepthClass = 9;

// Libelle d'affichage d'un repertoire : son DERNIER COMPOSANT, ou le chemin
// entier quand il n'en a pas -- "C:\" n'a pas de nom de fichier.
//
// ⚠ C'EST LE LIBELLE QUI EST AFFICHE, PAS LE CHEMIN, et c'est aussi ce qui rend
// les vidages independants de la machine : un chemin absolu y ferait apparaitre
// le repertoire de l'utilisateur, donc une sortie incomparable d'une machine a
// l'autre.
//
// ⚠ RENDU EN UTF-8 par core/path_text.h : la page de codes ANSI ferait dependre
// les octets du vidage de la machine qui l'execute.
//
// ⚠ UNE SEULE SURCHARGE, ET ELLE PREND UN path. Une seconde prenant une
// std::string rendrait `DirectoryLabel("a/b")` AMBIGU -- un litteral se convertit
// aussi bien vers l'un que vers l'autre. std::string et const char* se
// convertissent deja vers std::filesystem::path : la surcharge n'apporterait
// rien qu'une erreur de compilation chez l'appelant.
std::string DirectoryLabel (const std::filesystem::path& path);

// ---------------------------------------------------------------------------
//  L'ARBRE DE REPERTOIRES
// ---------------------------------------------------------------------------
//
// Remplace le wxGenericDirCtrl de l'hote historique.
//
// ⚠ REPRESENTATION PLATE, PAS ARBORESCENTE. RmlUi n'a pas de `data-for`
// recursif : ce qui est lie a l'interface est une SUITE DE LIGNES. L'operation
// frequente du chemin reel est « ouvrir ou fermer, puis reafficher » : Toggle
// est la SEULE operation qui recalcule les lignes, et le reaffichage coute le
// nombre de lignes VISIBLES, pas la taille de l'arbre.
//
// ⚠ CHARGEMENT PARESSEUX, SUR UN SEUL NIVEAU. Un noeud n'est enumere qu'a sa
// PREMIERE ouverture. Refermer ne jette pas l'enumeration : la rouvrir ne
// touche plus le disque.
//
// ⚠ LES ERREURS D'ACCES NE SONT NI MASQUEES NI FATALES. L'enumeration passe par
// la surcharge a std::error_code -- jamais par celle qui leve -- parce qu'un
// seul enfant inaccessible ferait alors tomber l'ouverture du parent entier :
// sur C:\, System Volume Information refuse l'acces a tout le monde.
class DirectoryTree
{
public:
	// Racines = les lecteurs PRESENTS de la machine sous Windows, la racine du
	// systeme de fichiers ailleurs.
	//
	// ⚠ LES LECTEURS ABSENTS OU NON PRETS SONT OMIS, PAS RENDUS EN ERREUR. Un
	// lecteur optique vide repond a la question « es-tu pret ? » tout de suite,
	// mais son ENUMERATION coute plusieurs secondes d'attente materielle. Une
	// ligne en erreur couterait donc ce delai a l'ouverture, pour n'apprendre
	// a l'operateur rien qu'il ne sache deja.
	void SetSystemRoots ();

	// Racine UNIQUE : ce repertoire. Rend false si le chemin n'est pas un
	// repertoire accessible, et renseigne LastError -- CAUSE COMPRISE : un
	// chemin inexistant et un acces refuse doivent se distinguer.
	bool SetRoot (const std::filesystem::path& directory);

	// PLAFOND PAR NOEUD, INJECTABLE. Defaut : kMaxChildrenPerNode. Une valeur
	// nulle ou negative est ignoree.
	//
	// ⚠ CE POINT D'INJECTION EXISTE POUR LE TEST, PAS POUR LE REGLAGE. Le
	// produit ne l'appelle nulle part : ses deux plafonds restent ceux que
	// l'etape 4 a MESURES. C'est le seul moyen d'exercer la ligne « ... N
	// entree(s) non affichee(s) », inatteignable tant que les deux chiffres sont
	// egaux -- sans changer les chiffres pour satisfaire un instrument.
	//
	// ⚠ N'AGIT QUE SUR LES ENUMERATIONS A VENIR : un noeud deja enumere garde
	// ses enfants et son compte de masques.
	void SetMaxChildrenPerNode (int maxChildren);

	// Les lignes VISIBLES, dans l'ordre d'affichage.
	const std::vector<TreeRow>& Rows () const { return m_rows; }

	// Ouvre ou ferme le noeud `rowId` -- l'identifiant porte par TreeRow::id, et
	// non le rang de la ligne. Rend false si l'identifiant ne designe aucun
	// noeud ouvrable.
	//
	// ⚠ SEULE OPERATION QUI RECALCULE LES LIGNES, avec Select. Tout le reste
	// lit Rows().
	bool Toggle (int rowId);

	// Ouvre tous les noeuds fermes jusqu'a `maxDepth` inclus, niveau par
	// niveau. Un noeud en echec d'acces reste ferme et devient une ligne en
	// erreur.
	//
	// ⚠ LA BORNE EST OBLIGATOIRE, PAS COMMODE : sans elle, ouvrir depuis "C:\"
	// parcourrait le disque entier. Elle est en outre le seul point ou plusieurs
	// noeuds s'ouvrent en une operation -- l'interface, elle, en ouvre un a la
	// fois.
	void ExpandToDepth (int maxDepth);

	bool IsExpanded (int rowId) const;

	// Selection de l'arbre, c'est-a-dire le repertoire courant. -1 : aucune.
	void Select (int rowId);
	int  Selected () const { return m_selected; }

	// Chemin du noeud, ou chemin VIDE si l'identifiant ne designe rien.
	//
	// ⚠ UN std::filesystem::path, PAS UNE std::string : c'est l'IDENTITE du
	// repertoire, et une conversion vers une chaine etroite est destructive sous
	// Windows (cf. core/path_text.h). La chaine de TreeRow::path est une copie
	// d'affichage, elle ne sert pas a rouvrir le repertoire.
	const std::filesystem::path& PathOf (int rowId) const;

	// Rend true si le plafond global de lignes a ete atteint au dernier
	// recalcul : les lignes visibles ne sont alors PAS toutes celles des noeuds
	// ouverts, et la derniere ligne le dit.
	bool Truncated () const { return m_truncated; }

	const std::string& LastError () const { return m_lastError; }

private:
	struct Node
	{
		std::filesystem::path path;
		std::string      label;
		int              depth      = 0;
		bool             expanded   = false;
		bool             enumerated = false;
		bool             error      = false;

		// Racine posee par SetSystemRoots : un lecteur, pas un repertoire
		// choisi. Seule l'icone en depend.
		bool             drive      = false;
		int              hidden     = 0;

		// Entrees dont le STATUT n'a pas pu etre lu. Elles ne sont ni des
		// enfants ni des absentes : elles se comptent, et le compte devient une
		// ligne.
		int              unreadable = 0;
		std::vector<int> children;
	};

	// ⚠ LES NOEUDS NE SONT JAMAIS RETIRES DU RESERVOIR, et c'est ce qui rend
	// TreeRow::id STABLE : un identifiant garde apres un repli designe encore
	// le meme repertoire apres le depli suivant. Refermer ne libere donc pas la
	// memoire des enfants -- c'est le prix de la stabilite, et il est borne par
	// ce qui a ete reellement ouvert.
	int  AddNode (const std::filesystem::path& path, const std::string& label, int depth);
	void Enumerate (int index);
	void RebuildRows ();

	// Rend false quand le plafond global a ete atteint : l'appelant cesse
	// d'ajouter des lignes au lieu de continuer un parcours dont la sortie est
	// deja tronquee.
	bool AppendRows (int index);
	void AppendTruncationRow (int depth);
	bool ValidIndex (int index) const;

	std::vector<Node>    m_nodes;
	std::vector<int>     m_roots;
	std::vector<TreeRow> m_rows;
	int                  m_selected  = -1;
	int                  m_maxChildrenPerNode = kMaxChildrenPerNode;
	bool                 m_truncated = false;
	std::string          m_lastError;
};

} // namespace core
} // namespace peles
