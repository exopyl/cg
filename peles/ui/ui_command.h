#pragma once

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  CE QU'UN CLIC DEMANDE, ET QUI N'EST PAS CE QU'IL FAIT
// ---------------------------------------------------------------------------
//
// ⚠ UN RAPPEL `data-event-` COURT A L'INTERIEUR DE LA DISTRIBUTION D'EVENEMENT
// DE RmlUi. Le rappel ENREGISTRE donc une intention ; l'hote l'execute apres,
// hors de toute distribution. DEUX RAISONS, ET AUCUNE N'EST LA LIMITE DE
// `data-for` :
//
//   1. L'OUVERTURE DOIT RENDRE LA MAIN. `VMeshesIO::load` est SYNCHRONE et peut
//      durer plusieurs secondes : charger dans la foulee du clic ne laisserait
//      jamais dessiner l'image de transition qui porte le statut, et le systeme
//      griserait une fenetre qui ne dit rien.
//
//   2. L'OUVERTURE EMET DE VRAIS APPELS GL. `OpenModel` annonce chaque maillage
//      detruit, `SceneRenderer` rend au moteur les cles correspondantes, et ces
//      liberations sont des appels GL. Emis depuis l'interieur de
//      `ProcessMouseButtonDown`, ils seraient HORS des deux clotures d'etat de
//      l'image et hors de tout releve --state-audit : le contrat d'etat GL
//      cesserait d'etre mesure precisement la ou il est le plus expose.
//
// ⚠ LA LIMITE §1.5 -- « modifier la structure pendant un `data-for` est
// indefini » -- NE MORD PAS ICI, et l'invoquer serait une raison fausse : un
// appel direct ne ferait que `DirtyVariable`, et la reconstruction des elements
// engendres a lieu dans `Update()`, qui n'est jamais reentre depuis un rappel
// souris.
//
// ⚠ ET C'EST AUSSI CE QUI EMPECHE LE CYCLE. Le controleur d'interface ne
// connait ni le document, ni la vue 3D, ni l'hote : il ne pourrait pas charger
// un fichier meme s'il le voulait. Ce qu'il sait faire, c'est dire ce qui a ete
// demande.
enum class UiCommandKind
{
	// Ouvrir ou fermer le repertoire de cette ligne d'arbre, et en faire le
	// repertoire courant.
	ToggleDirectory,

	// Designer ce fichier dans la liste, sans le charger.
	SelectFile,

	// Charger ce fichier et l'ajouter a la scene.
	OpenFile
};

struct UiCommand
{
	UiCommandKind kind = UiCommandKind::SelectFile;

	// Identifiant de la ligne concernee -- un identifiant de noeud pour l'arbre,
	// un rang pour la liste de fichiers.
	//
	// ⚠ UN IDENTIFIANT, PAS UN CHEMIN. Le chemin d'affichage porte par le DTO est
	// une conversion en UTF-8 : sous Windows, rouvrir un fichier a partir de lui
	// peut viser autre chose. L'identite vit dans DirectoryTree et FileList, et
	// l'hote la leur redemande.
	int rowId = -1;
};

} // namespace ui
} // namespace peles
