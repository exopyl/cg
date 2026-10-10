#pragma once

#include "directory_tree.h"
#include "file_list.h"
#include "ui_command.h"
#include "view_model.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Model;

namespace Rml
{
class Context;
}

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  CE QUI TRADUIT L'ETAT EN LIGNES, ET RIEN D'AUTRE
// ---------------------------------------------------------------------------
//
// ⚠ IL NE DESSINE RIEN ET NE CONNAIT AUCUNE GEOMETRIE. Il tient l'arbre, la
// liste et le modele lie, et il declare sali ce qui a change. La cible peles_ui
// ne lie ni cgre ni glfw : aucun type de rendu ne peut entrer ici.
//
// ⚠ LES DTO SONT DES COPIES. Publier des pointeurs vers l'arbre ferait tenir a
// RmlUi des adresses que le prochain Toggle invaliderait, et la limite
// documentee sur la suppression d'elements engendres deviendrait un probleme
// de duree de vie au lieu d'un simple redessin.
class UiController
{
public:
	UiController ();
	~UiController ();

	UiController (const UiController&)            = delete;
	UiController& operator= (const UiController&) = delete;

	// Lie le modele au contexte. Rend false si RmlUi refuse le modele.
	bool Bind (Rml::Context* context, const char* modelName);

	// Racines = les lecteurs de la machine. La liste de fichiers est videe : on
	// n'est dans aucun repertoire.
	void ShowSystemRoots ();

	// Racine unique, OUVERTE et SELECTIONNEE, liste de fichiers peuplee. Rend
	// false si le repertoire est inaccessible -- le message est dans le modele,
	// donc a l'ecran, pas seulement dans un journal.
	bool ShowDirectory (const std::filesystem::path& directory);

	bool ToggleRow (int rowId);

	// Ouvre l'arbre jusqu'a cette profondeur. ⚠ Le nombre de lignes produites
	// n'est borne que par le plafond par noeud : c'est l'operation par laquelle
	// une interface peut se retrouver avec plusieurs milliers de lignes liees.
	void ExpandToDepth (int maxDepth);

	// Ouvre le repertoire de la ligne : il devient le repertoire courant et sa
	// liste de fichiers est peuplee.
	bool SelectDirectory (int rowId);

	// Marque comme selectionne le fichier de ce chemin, s'il est dans la liste.
	// Un chemin absent de la liste DESELECTIONNE.
	//
	// ⚠ LA SELECTION VIT DANS FileList, PAS DANS LA COPIE DTO. Portee par la
	// copie, elle serait effacee par la republication suivante -- republication
	// que declenche n'importe quelle operation touchant la liste, y compris
	// celles qui n'ont rien a voir avec la selection. C'est la meme regle que
	// DirectoryTree::m_selected.
	void SelectFile (const std::filesystem::path& path);

	// LES TROIS PANNEAUX DE DROITE : table de proprietes, arbre des maillages,
	// arbre des materiaux. `model` a nullptr les vide -- et « vider » veut dire
	// quatorze lignes de proprietes sans valeur, pas zero ligne.
	//
	// ⚠ CE CONTROLEUR NE CONNAIT NI LE DOCUMENT NI LE RENDU. Il recoit le modele
	// que l'hote lui designe et en tire des lignes ; il ne va pas le chercher,
	// et il n'existe aucun chemin d'ici vers la vue 3D.
	void ShowModelInfo (::Model* model);

	// LIGNE DE STATUT, affichee sous le titre de la zone basse. Vide l'efface.
	//
	// ⚠ ELLE EST POSEE AVANT UN CHARGEMENT LONG, ET L'HOTE REND UNE IMAGE
	// COMPLETE ENTRE LES DEUX. Posee puis suivie immediatement du chargement,
	// elle ne serait jamais dessinee : `VMeshesIO::load` est synchrone.
	void SetStatus (const std::string& status);

	// ---------------------------------------------------------------------
	//  CE QUE LES CLICS ONT DEMANDE
	// ---------------------------------------------------------------------
	//
	// ⚠ LA FILE EST VIDEE PAR LA LECTURE, et l'hote la lit HORS de la
	// distribution d'evenements de RmlUi. Executer une commande depuis le rappel
	// reviendrait a reconstruire les vecteurs lies -- donc les elements engendres
	// par `data-for` -- pendant que RmlUi parcourt son arbre.
	std::vector<UiCommand> TakeCommands ();

	// Applique une commande d'arbre : ouvre ou ferme le noeud ET en fait le
	// repertoire courant. Rend false si la ligne ne designe aucun noeud.
	//
	// ⚠ LES DEUX ENSEMBLE, ET C'EST UNE DECISION D'INTERFACE ASSUMEE. La feuille
	// de style ne dessine aucun chevron d'ouverture : une ligne qui ne ferait que
	// selectionner laisserait l'arbre inouvrable, faute de zone ou cliquer pour
	// l'ouvrir.
	bool ActivateDirectoryRow (int rowId);

	// Designe le fichier de rang `rowId`. Rend son chemin -- l'IDENTITE, pas la
	// chaine d'affichage -- ou un chemin vide si le rang ne designe rien.
	std::filesystem::path SelectFileRow (int rowId);

	const ViewModel&           Model () const { return m_model; }
	const core::DirectoryTree& Tree () const { return m_tree; }
	const core::FileList&      Files () const { return m_files; }

private:
	void PublishTree ();
	void PublishFiles ();
	void PublishInfo ();
	void PublishStatus ();

	// Pimpl : il ne contient qu'un Rml::DataModelHandle et le contexte, mais
	// l'un et l'autre imposeraient un en-tete de RmlUi ici -- or peles_ui ne
	// lie RmlUi qu'en PRIVATE, et cet en-tete est inclus par l'executable.
	struct Binding;
	std::unique_ptr<Binding> m_binding;

	core::DirectoryTree m_tree;
	core::FileList      m_files;
	ViewModel           m_model;

	std::vector<UiCommand> m_commands;
};

} // namespace ui
} // namespace peles
