#pragma once

#include "ui_command.h"
#include "ui_dto.h"

#include <functional>
#include <string>
#include <vector>

namespace Rml
{
class Context;
class DataModelHandle;
}

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  LA RACINE DE BINDING, ET IL N'Y EN A QU'UNE
// ---------------------------------------------------------------------------
//
// ⚠ LA GRANULARITE DE « SALI » DANS RmlUi EST LA VARIABLE RACINE. Decouper en
// trois modeles -- arbre, liste, informations -- serait une FAUSSE ECONOMIE :
// les trois zones se salissent ENSEMBLE (choisir un repertoire refait la liste,
// choisir un fichier touche les trois), et le cout reel est dans le parcours du
// vecteur, pas dans le nombre de variables declarees salies.
//
// ⚠ REPLI CONNU, s'il faut un jour en venir la : decoupage en trois racines. La
// mesure qui le declencherait est celle du temps de Context::Update() a nombre
// de lignes croissant.
struct ViewModel
{
	std::vector<core::TreeRow> treeRows;
	std::vector<core::FileRow> fileRows;

	// LES TROIS ZONES DE DROITE. ⚠ `infoRows` COMPTE TOUJOURS QUATORZE LIGNES,
	// y compris sans modele : un vecteur vide ferait disparaitre la table et la
	// hauteur du panneau changerait a chaque deselection.
	std::vector<core::InfoRow> infoRows;
	std::vector<core::InfoRow> meshRows;
	std::vector<core::InfoRow> materialRows;

	// Repertoire dont la liste de fichiers est affichee, pour le titre de zone.
	std::string currentDirectory;

	// Messages d'erreur, vides en regime normal. ⚠ ILS SONT AFFICHES : une
	// erreur d'acces tue ne se distingue pas d'un repertoire vide.
	std::string treeStatus;
	std::string filesStatus;

	// LIGNE DE STATUT DE L'HOTE, vide en regime normal.
	//
	// ⚠ ELLE EXISTE POUR L'IMAGE DE TRANSITION DU CHARGEMENT LONG. `VMeshesIO::
	// load` est SYNCHRONE et peut durer plusieurs secondes sur un STEP : sans un
	// mot pose AVANT l'appel et une image rendue entre les deux, la fenetre se
	// fige sans rien dire et le systeme la grise.
	std::string status;

	// Fichiers importables au-dela du plafond.
	int filesHidden = 0;
};

// ---------------------------------------------------------------------------
//  LES NOMS DES VARIABLES LIEES, DECLARES UNE SEULE FOIS
// ---------------------------------------------------------------------------
//
// ⚠ UN ECART ENTRE LE NOM PASSE A `Bind` ET CELUI PASSE A `DirtyVariable` EST
// TOTALEMENT SILENCIEUX EN RELEASE. `Rml::DataModelHandle::DirtyVariable` ne
// fait qu'un `RMLUI_ASSERTMSG` quand le nom lui est inconnu, et cette macro est
// compilee A RIEN sous NDEBUG : le panneau reste FIGE sur son contenu initial --
// sans erreur, sans journal, donc sans que le compteur de messages de l'hote ne
// voie quoi que ce soit.
//
// ⚠ ET LE CONTROLE DU JOURNAL NE RATTRAPE PAS CELLE-LA. Il voit ce que RmlUi
// REFUSE ; un nom sali qui n'existe pas n'est refuse par personne. La faute ne
// se manifesterait qu'a la DEUXIEME publication -- la premiere etant celle de la
// construction -- donc jamais sous un vidage, qui n'en fait qu'une.
//
// Ces constantes suppriment la classe de fautes au lieu de la surveiller : les
// deux cotes nomment le MEME symbole, et une faute de frappe cesse de compiler.
namespace binding
{
inline constexpr const char* kTreeRows         = "treeRows";
inline constexpr const char* kTreeStatus       = "treeStatus";
inline constexpr const char* kFileRows         = "fileRows";
inline constexpr const char* kFilesHidden      = "filesHidden";
inline constexpr const char* kFilesStatus      = "filesStatus";
inline constexpr const char* kCurrentDirectory = "currentDirectory";
inline constexpr const char* kInfoRows         = "infoRows";
inline constexpr const char* kMeshRows         = "meshRows";
inline constexpr const char* kMaterialRows     = "materialRows";
inline constexpr const char* kStatus           = "status";

// ---------------------------------------------------------------------------
//  LES NOMS DES RAPPELS D'EVENEMENT, MEME REGLE ET MEME RAISON
// ---------------------------------------------------------------------------
//
// ⚠ UN `data-event-click="nomInconnu(...)"` EST REFUSE PAR RmlUi AVEC UN
// MESSAGE, donc le compteur de journal de l'hote l'attrape. Ces constantes ne
// referment donc pas la meme panne que les precedentes -- elles referment la
// SYMETRIQUE : un nom LIE que personne n'appelle depuis le document reste muet,
// et c'est le clic qui ne fait rien.
inline constexpr const char* kOnTreeRow  = "onTreeRow";
inline constexpr const char* kOnFileRow  = "onFileRow";
inline constexpr const char* kOnFileOpen = "onFileOpen";
} // namespace binding

// CE QU'UN RAPPEL `data-event-` DEPOSE. Il est appele DEPUIS la distribution
// d'evenements de RmlUi : il doit enregistrer, pas agir.
using CommandSink = std::function<void (const UiCommand&)>;

// Enregistre les types des DTO, lie les variables du modele `modelName` au
// contexte et y attache les rappels d'evenement. Rend false si RmlUi refuse le
// modele -- nom deja pris, ou type deja enregistre -- et laisse `handle`
// inchange.
//
// ⚠ LES POINTEURS LIES DESIGNENT LES MEMBRES DE `model` : celui-ci doit
// survivre au contexte, ou etre delie avant de mourir.
//
// ⚠ ET `sink` EST CAPTURE PAR LES RAPPELS : ce qu'il designe doit survivre au
// contexte pour la meme raison.
bool BindViewModel (Rml::Context* context, const char* modelName, ViewModel& model,
                    CommandSink sink, Rml::DataModelHandle& handle);

void UnbindViewModel (Rml::Context* context, const char* modelName);

} // namespace ui
} // namespace peles
