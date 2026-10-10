#pragma once

#include "document.h"
#include "dump_modes.h"
#include "input_event.h"
#include "input_router.h"
#include "ui_controller.h"
// ⚠ AUCUN EN-TETE GL DERRIERE CELUI-CI : view3d/picking.h ne nomme que des
// flottants et des declarations anticipees. Il est inclus -- et non remplace par
// une declaration anticipee -- parce que `PivotPickStatus` est une enumeration
// dont la valeur est STOCKEE ici : c'est l'issue exacte du dernier Alt + clic,
// et le verdict de --alt-pivot en lit les quatre cas.
#include "view3d/picking.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace peles
{
namespace platform
{
class RmlHost;
class Window;
}
namespace view3d
{
class Camera;
class SceneRenderer;
}
namespace ui
{
class PanelSplitters;
}
namespace app
{

struct Options
{
	// Repertoire d'assets DEJA RESOLU. Application ne resout rien : la
	// resolution echoue avant qu'une fenetre ne soit ouverte, sans quoi une
	// erreur de deploiement se presenterait comme un cadre vide.
	std::string assetsDir;

	// Fichier a charger, vide si aucun.
	std::string modelPath;

	// DOCUMENT RML CHARGE, dans le repertoire d'assets. Vide : celui du produit.
	//
	// ⚠ IL EXISTE POUR UNE SEULE RAISON, ET C'EST UN ANGLE MORT DE MESURE. Le
	// document du produit colle la vue au bord HAUT : `rect.y` vaut 0, donc
	// neutraliser le terme Y de `ui::WindowToView` laisse les six criteres
	// verts. `layout_probe.rml` place la vue sous une barre de 140 px, ce qui
	// rend les DEUX termes de la conversion non nuls et le detecteur du
	// critere 1 complet.
	std::string documentFile;

	// Repertoire ouvert dans l'arbre au demarrage. Vide : le repertoire du
	// fichier charge, ou les lecteurs de la machine si aucun fichier n'est
	// donne.
	std::string browseDir;

	// Profondeur ouverte dans l'arbre au demarrage. 0 : la racine seule est
	// ouverte, ce qui est le comportement d'un explorateur.
	int browseDepth = 0;

	// ⚠ CES DEUX MODES SONT TRAITES PAR main AVANT TOUTE CREATION, et ils ne
	// sont jamais lus par Application : ils n'ouvrent ni fenetre, ni contexte
	// GL, ni document RML. C'est ce qui les rend executables sur une machine
	// sans interface graphique.
	std::string dumpTreeDir;
	std::string dumpFilesDir;

	// ⚠ TROISIEME MODE DE LA MEME FAMILLE, ET IL N'A PAS DE VALEUR PROPRE : le
	// fichier dont il imprime les informations est l'ARGUMENT POSITIONNEL de la
	// ligne de commande, celui que l'application ouvrirait. C'est ce qui rend
	// executable le cas que le critere exige -- « meme sans fichier charge » --
	// qu'un `--dump-info <fichier>` obligatoire rendrait inatteignable, la
	// valeur vide etant refusee par l'analyse.
	bool dumpInfo = false;

	// SECOND FICHIER, OUVERT APRES LA PREMIERE IMAGE RENDUE.
	//
	// ⚠ C'EST L'INSTRUMENT DU CONTRAT DE DESTRUCTION, et il n'a pas
	// d'equivalent : remplacer la scene APRES une image rendue est le seul
	// chemin ou Document::Clear court avec un maillage REELLEMENT enregistre
	// aupres du moteur de rendu. A la fermeture, la vue meurt avant le document
	// et nettoie elle-meme -- la branche utile n'est jamais exercee.
	std::string reopenPath;

	// Nombre de basculements du noeud racine, un par image rendue. Exerce le
	// RETRECISSEMENT du vecteur lie, que RmlUi ne documente que par la negative.
	int churnTree = 0;

	// Nombre de SELECTIONS SUCCESSIVES DU MEME FICHIER, une par image rendue.
	//
	// ⚠ C'EST L'INSTRUMENT QUI REMPLACE LA PURGE DE LA wxImageList. L'hote
	// historique devait retirer a la main les pastilles de la construction
	// precedente, faute de quoi la liste d'images grossissait a chaque
	// chargement -- « sur un modele a 98 materiaux, ca se voit vite ». Ici il
	// n'y a plus de liste a purger : ce qui le VERIFIE est le releve pris apres
	// chaque selection -- CINQ entiers exacts, plus l'empreinte du processus a
	// titre indicatif.
	//
	// ⚠ DEUX DES CINQ SONT INDEXES PAR MAILLAGE, TROIS PAR LIGNE LIEE, et c'est
	// cette seconde famille qui porte la grandeur nommee par le critere : la
	// wxImageList recevait UNE ENTREE PAR LIGNE, pas une par maillage. Un
	// fichier a 60 materiaux sur UN maillage laisse les deux premiers a « 1, 1 »
	// quel que soit le nombre de materiaux.
	int reselect = 0;

	int windowWidth  = 1280;
	int windowHeight = 800;

	// 0 : jusqu'a la fermeture de la fenetre.
	int  frames     = 0;
	bool dumpTiming = false;
	bool stateAudit = false;
	bool debugger   = false;

	// Imprime le rectangle resolu de la vue pour un cadre de cette taille, puis
	// sort. Aucune image n'est rendue : c'est la GEOMETRIE qui est mesuree.
	bool dumpLayout    = false;
	int  layoutWidth   = 0;
	int  layoutHeight  = 0;

	// ELEMENT DONT LA GEOMETRIE EST IMPRIMEE. Vide : celui de la vue 3D.
	//
	// EN L'ABSENCE DE TOUTE CAPTURE, C'EST LA SEULE PREUVE FALSIFIABLE QU'UN
	// PANNEAU A DU CONTENU MIS EN PAGE. Une table dont les deux colonnes se
	// seraient effondrees, un arbre de hauteur nulle, un `data-for` qui
	// n'engendre rien : les trois se voient a l'oeil et par rien d'autre --
	// sauf par le rectangle resolu de l'element.
	std::string layoutElement;

	// Lit un pixel du tampon arriere, en coordonnees GL (origine EN BAS A
	// GAUCHE), et le compare a la valeur predite pour le materiau par defaut.
	bool readPixel = false;
	int  pixelX    = 0;
	int  pixelY    = 0;

	// IMPRIME LE RAYON MONDE D'UN PIXEL DE LA FENETRE, puis sort.
	//
	// ⚠ C'EST LE SEUL INSTRUMENT DU LOT QUI VOIE LA CONVERSION FENETRE -> VUE.
	// Un decalage de la largeur du panneau gauche ne change aucun pixel dessine
	// : ni --readpixel, ni --dump-layout, ni le detecteur de concordance du
	// GL_VIEWPORT ne le verraient.
	bool dumpPick   = false;
	int  pickWidth  = 0;
	int  pickHeight = 0;
	int  pickX      = 0;
	int  pickY      = 0;

	// DEROULE UNE SEQUENCE D'EVENEMENTS SOURIS INJECTES et imprime, pour chacun,
	// l'element survole et le proprietaire retenu. Puis sort.
	bool inputScript = false;

	// INJECTE UN Alt + CLIC GAUCHE sur un pixel de la FENETRE et compare le pivot
	// avant et apres, au bit pres. Puis sort.
	//
	// ⚠ LA COMPARAISON EST DANS L'INSTRUMENT, ET NON LAISSEE A L'OPERATEUR. Deux
	// executions d'un vidage de camera, relues a l'oeil, ne distinguent pas deux
	// flottants qui different au dernier bit : ce serait un critere qui ne peut
	// pas echouer sur le mode de panne le plus probable.
	bool altPivot  = false;
	int  altPivotX = 0;
	int  altPivotY = 0;

	// INJECTE UN CLIC -- OU UN DOUBLE-CLIC -- SUR UN PIXEL DE LA FENETRE et
	// imprime la chaine complete : survol, commande produite, effet.
	//
	// ⚠ C'EST LE SEUL INSTRUMENT QUI ETABLISSE QUE LES `data-event-` SONT VIVANTS.
	// Un attribut mal nomme, un rappel non lie, une commande produite mais jamais
	// appliquee : les trois se presentent a l'ecran comme « ca ne fait rien
	// quand je clique », et aucun autre verdict du lot ne les distingue.
	bool clickDouble = false;
	bool click       = false;
	int  clickX      = 0;
	int  clickY      = 0;

	// CE QUE LE CLIC DOIT PRODUIRE.
	//
	// ⚠ SANS ELLE, --click N'EXAMINE NI LE GENRE NI L'EFFET : il compte les
	// commandes, et deux rappels `data-event-click` intervertis dans le document
	// passent alors pour conformes -- les valeurs attendues sont IMPRIMEES, et
	// ce qui n'est pas COMPARE n'est pas mesure.
	ClickExpectation clickExpect = ClickExpectation::Any;

	// INJECTE UN GLISSER GAUCHE d'un pixel FENETRE a un autre et mesure la
	// rotation obtenue.
	//
	// ⚠ L'ARRIVEE PEUT ETRE SUR UN PANNEAU, ET C'EST LE CAS INTERESSANT : c'est
	// la version PRODUIT de la regle de capture. L'arbitre peut rendre « vue »
	// sans que la camera tourne ; seule cette mesure-ci le dit.
	bool drag      = false;
	int  dragFromX = 0;
	int  dragFromY = 0;
	int  dragToX   = 0;
	int  dragToY   = 0;

	// UN SECOND BOUTON ENFONCE *ET RELACHE* AU MILIEU DU GLISSER.
	//
	// ⚠ C'EST LE SEUL INSTRUMENT DU LOT QUI EXERCE UN GESTE A DEUX BOUTONS, et
	// aucun autre ne peut le remplacer : l'arbitre gele bien le proprietaire au
	// premier enfoncement -- --input-script le mesure -- mais le gel ne dit RIEN
	// de ce que la camera fait des enfoncements suivants. Le trackball n'a qu'un
	// slot d'etat, donc un relachement de trop tue le geste en cours.
	//
	// Deux versants, et ils mesurent deux pannes differentes :
	//   Left  : le glisser gauche doit TOURNER malgre le clic droit intercale ;
	//   Right : le clic gauche intercale, immobile, ne doit RIEN selectionner --
	//           son deplacement nul passerait sous le seuil clic/glisser.
	GestureOwner dragBump = GestureOwner::None;

	// FAIT DEFILER UN PANNEAU A LA MOLETTE, par evenements injectes, et mesure
	// si sa DERNIERE ligne devient atteignable. Puis sort.
	bool        dumpScroll = false;
	std::string scrollContainer;
	std::string scrollContent;

	// CLIQUE SUR LA LIGNE COPIABLE, INJECTE Ctrl+A PUIS Ctrl+C, et relit le
	// presse-papier DU SYSTEME. Puis sort.
	//
	// ⚠ C'EST LE SEUL INSTRUMENT DU LOT QUI VOIE LE CLAVIER. Trois mecanismes
	// distincts s'y rencontrent -- focus au clic, routage des touches,
	// presse-papier du systeme -- et leur panne commune se presente comme
	// « Ctrl+C ne colle rien », qu'aucun autre verdict ne distingue.
	bool        dumpCopy = false;
	std::string copySelector;

	// INJECTE UN APPUI IMMOBILE INTERROMPU PAR UNE PERTE DE FOCUS, sur un pixel
	// de la FENETRE, et compare a la sequence normale sur le meme pixel.
	//
	// ⚠ LE TEMOIN FAIT PARTIE DE L'INSTRUMENT. « Rien n'a ete selectionne » est
	// vrai aussi d'un pixel qui ne designe aucun modele : sans la sequence
	// normale exercee d'abord, le verdict serait vert sur une application ou le
	// picking ne fonctionne plus du tout.
	bool focusLoss  = false;
	int  focusLossX = 0;
	int  focusLossY = 0;

	// IMPRIME LA ZONE DE STATUT A LA FIN DE LA BOUCLE, et verifie qu'elle rend
	// compte du chargement demande.
	//
	// ⚠ ADDITIF, ET NON « VIDER ET SORTIR » : ce qu'il mesure est l'etat dans
	// lequel la BOUCLE laisse le statut. Un mode qui sortirait avant elle ne
	// verrait jamais un echec survenu pendant.
	bool dumpStatus = false;

	// SECOND FICHIER, OUVERT APRES LA PREMIERE IMAGE RENDUE, PAR LE CHEMIN DE
	// L'INTERFACE -- celui qui AJOUTE a la scene au lieu de la remplacer.
	//
	// ⚠ A NE PAS CONFONDRE AVEC --reopen, qui vide la scene avant de charger et
	// mesure le contrat de destruction. Ici la scene GRANDIT : c'est la seule
	// forme sous laquelle la selection du modele decrit par les panneaux peut
	// etre fausse.
	std::string openSecondPath;

	// --resize : taille demandee a la fenetre APRES la premiere image rendue.
	//
	// ⚠ CE N'EST PAS UN REDIMENSIONNEMENT AU BORD. glfwSetWindowSize ne fait
	// pas entrer Win32 dans sa boucle modale ; ce qu'il exerce, c'est le meme
	// WM_SIZE puis le meme WM_PAINT que chaque pas de cette boucle distribue.
	bool resize       = false;
	int  resizeWidth  = 0;
	int  resizeHeight = 0;

	// --drag-splitter : TIRE UN SEPARATEUR DE PANNEAU de `splitterDelta` pixels,
	// par evenements injectes, puis reduit et rend la fenetre, puis double-clique
	// le separateur. Puis sort.
	//
	// ⚠ LE GLISSER TRAVERSE LA VUE 3D QUAND LE PANNEAU S'AGRANDIT, et c'est le
	// cas qui compte : la camera ne doit pas tourner. Un glisser commence DANS
	// la vue et qui traverse le separateur est joue d'abord ; il DOIT tourner,
	// sans quoi « la camera n'a pas tourne » ne prouverait rien.
	bool        dragSplitter  = false;
	std::string splitterName;
	int         splitterDelta = 0;
};

// Echec d'initialisation : le message ET le code de sortie qui lui correspond.
//
// ⚠ LE CODE FAIT PARTIE DU DIAGNOSTIC. Une faute de CONFIGURATION -- assets
// introuvables, element de vue absent du document -- sort en 3, une panne
// d'execution en 1. Les confondre reviendrait a demander a l'operateur de lire
// le message pour savoir quoi corriger, ce qu'un script ne fait pas.
struct CreateError
{
	std::string message;
	int         exitCode = 1;
};

// L'hote : il tient la fenetre, RmlUi, le document, la vue 3D, et l'ordre des
// huit phases.
class Application
{
public:
	static std::unique_ptr<Application> Create (const Options& options, CreateError& error);

	~Application ();

	Application (const Application&)            = delete;
	Application& operator= (const Application&) = delete;

	// Code de sortie du processus : 0 si tout ce qui a ete demande a reussi.
	int Run ();

private:
	Application ();

	// ⚠ LES TROIS ETAPES DE LA CREATION, SEPAREES PARCE QU'ELLES NE PEUVENT PAS
	// ETRE REORDONNEES. La fenetre porte le contexte GL ; RmlUi exige un contexte
	// COURANT a sa creation ; la vue 3D et le document exigent les deux. Chacune
	// rend false et renseigne `error` sans rien laisser derriere elle.
	bool CreateWindowAndGl (const Options& options, CreateError& error);

	// POSE L'ICONE DE LA FENETRE DEPUIS LES ASSETS.
	//
	// ⚠ NE PEUT PAS ECHOUER, ET C'EST UNE DECISION : une icone absente est un
	// defaut d'apparence, pas une panne. Elle est NOMMEE sur stderr -- sans
	// quoi un deploiement incomplet ne se verrait que sur la barre des taches --
	// mais elle ne change pas le code de sortie, contrairement au document RML
	// ou a la police, sans lesquels il n'y a rien a l'ecran.
	void LoadWindowIcon (const Options& options);
	bool CreateUi (const Options& options, CreateError& error);

	// ⚠ NE PEUT PAS ECHOUER, ET C'EST UNE DECISION. Un fichier illisible laisse le
	// document vide et l'application utilisable ; le message nomme le fichier.
	void CreateSceneAndDocument (const Options& options);

	// UN CHARGEMENT ECHOUE, DIT SUR LES DEUX CANAUX.
	//
	// ⚠ ELLE EXISTE POUR QUE LES TROIS CHEMINS DE CHARGEMENT DISENT LA MEME
	// CHOSE. Ils sont trois -- fichier de la ligne de commande, remplacement de
	// scene, ajout depuis la liste -- et deux d'entre eux n'ecrivaient que sur
	// `stderr` : dans l'application, l'echec etait alors INVISIBLE.
	void ReportLoadFailure (const std::string& name, const std::string& reason);

	// Peuple l'arbre et la liste de fichiers. Ne peut pas echouer non plus : un
	// repertoire inaccessible laisse son message dans le modele, donc a l'ecran.
	void PopulateBrowser (const Options& options);

	// Remplace la scene par ce fichier.
	//
	// ⚠ PRECONDITION : LE CONTEXTE GL DE LA FENETRE EST COURANT. Document::Clear
	// annonce chaque maillage avant de le detruire, et SceneRenderer rend alors
	// au moteur les cles correspondantes -- ce qui LIBERE DE VRAIS OBJETS GL.
	// Appelee sans contexte courant, la liberation serait silencieusement perdue.
	// Cette fonction n'est donc appelee que depuis la boucle d'images, jamais
	// depuis la creation ni depuis la destruction.
	bool OpenModel (const std::string& path);

	// AJOUTE un modele a la scene, sans rien detruire.
	//
	// ⚠ C'EST LE CHEMIN DE L'INTERFACE, ET IL DIFFERE D'OpenModel SUR LE POINT
	// QUI COMPTE : rien n'est vide, donc la selection precedente survivrait a la
	// nouvelle si le chargeur ne la reposait pas. Les panneaux de droite
	// decriraient alors le PREMIER fichier pendant que la vue montre les deux.
	bool AddModel (const std::filesystem::path& path);

	// ---------------------------------------------------------------------
	//  LES ENTREES
	// ---------------------------------------------------------------------

	// Draine la file d'evenements remplie par les rappels de la fenetre, arbitre
	// chacun et applique les gestes de la vue 3D. Phase 1 d'une image.
	void ProcessInput ();

	// Remet les touches a RmlUi. ⚠ Aucun arbitrage : le destinataire est
	// l'element qui a le FOCUS, pas celui que le curseur survole.
	void RouteKeys (const std::vector<ui::KeyEvent>& keys);

	void ApplyViewportGesture (const ui::MouseEvent& event, int viewX, int viewY);

	// ⚠ LES DEUX EXTREMITES DU GESTE SONT SEPAREES PARCE QUE LEUR REGLE COMMUNE
	// DOIT SE VOIR : ni l'une ni l'autre n'agit sur un bouton qui n'a pas ouvert
	// le geste. Fondu dans un switch, le couplage bouton <-> trackball ne se
	// lit pas, et c'est lui qui decide si un geste a deux boutons survit.
	void BeginViewportGesture (const ui::MouseEvent& event, int viewX, int viewY);
	void EndViewportGesture (const ui::MouseEvent& event, int viewX, int viewY);

	// ANNULE LE GESTE DE LA VUE AU LIEU DE LE TERMINER.
	//
	// ⚠ LA DIFFERENCE AVEC EndViewportGesture EST LE SEUL POINT QUI COMPTE :
	// celle-ci ne compare AUCUN deplacement au seuil clic/glisser, donc elle ne
	// selectionne pas. Un geste annule par une perte de focus porte la derniere
	// position du curseur -- deplacement nul -- et serait sinon conclu comme un
	// clic.
	void CancelViewportGesture ();

	void SelectFromPixel (int viewX, int viewY);
	void HoverFromPixel (int viewX, int viewY);
	void SetHovered (::Model* model);

	// Execute ce que les clics ont demande. Appelee HORS de la distribution
	// d'evenements de RmlUi -- voir ui/ui_command.h.
	void ApplyCommand (const ui::UiCommand& command);
	void ExecuteUiCommands ();

	// POSE LE STATUT ET DIFFERE LE CHARGEMENT D'UNE IMAGE.
	//
	// ⚠ L'IMAGE DE TRANSITION N'EST PAS UN CONFORT. `VMeshesIO::load` est
	// SYNCHRONE et peut durer plusieurs secondes : charger dans la foulee du clic
	// ne laisserait jamais dessiner le message, et le systeme griserait une
	// fenetre qui ne dit rien.
	void RequestOpen (const std::filesystem::path& path);

	// Charge ce qu'une image de transition a annonce, et rend true si le
	// chargement a reussi. Appelee APRES une image REELLEMENT rendue, sans quoi
	// le message n'a pas ete dessine.
	bool FinishPendingOpen ();

	// --alt-pivot : rend le code de sortie du processus.
	//
	// ⚠ IL PASSE PAR LA FILE D'EVENEMENTS ET PAR L'ARBITRE, exactement comme une
	// souris. Appeler SetPivotFromPixel directement mesurerait le calcul, pas le
	// geste -- or c'est le geste qui porte le drapeau Alt, l'arbitrage et la
	// conversion de repere.
	int RunAltPivot ();

	// --click / --double-click : met en file la sequence d'evenements du geste.
	void QueueClickSequence ();

	// --click : rend le code de sortie du processus.
	int RunClick ();

	// --drag : rend le code de sortie du processus.
	int RunDrag ();

	// --drag-bump : le geste a DEUX boutons. Rend le code de sortie du
	// processus.
	int RunDragBump ();

	// --drag-splitter : rend le code de sortie du processus. Ses quatre temps
	// sont portes par SplitterProbe, classe imbriquee definie dans
	// app/application_splitters.cpp.
	int RunDragSplitter ();
	class SplitterProbe;

	// --dump-copy : rend le code de sortie du processus.
	int RunDumpCopy ();

	// --focus-loss : rend le code de sortie du processus.
	int RunFocusLoss ();

	// --dump-pick : rend le code de sortie du processus.
	//
	// ⚠ IL PASSE PAR UNE IMAGE REELLEMENT RENDUE, et c'est ce qui fait de lui
	// une mesure du PRODUIT. L'instrument ne pose plus AUCUNE dimension : il
	// relit celles que la phase 5 a posees et les compare au rectangle de la
	// phase 3. Les poser ici reviendrait a injecter le cas negatif du critere 2
	// dans l'INSTRUMENT au lieu du PRODUIT : le produit pourrait alors etre faux
	// sans qu'aucun des six criteres ne rougisse.
	int RunDumpPick ();

	// LE CHEMIN QUI SORT APRES UN VIDAGE, SEPARE DE LA BOUCLE D'IMAGES.
	//
	// Rend true et renseigne `exitCode` quand l'un des modes « vider et sortir »
	// a ete demande ; rend false quand Run doit entrer dans la boucle.
	bool RunDumpMode (int& exitCode);

	// La boucle d'images, et le verdict qui la suit.
	int RunLoop ();

	// Rend true SEULEMENT si les huit phases ont couru sur cette image.
	//
	// ⚠ LA DISTINCTION EST LE COEUR DE L'INSTRUMENT. Une image sans surface de
	// dessin, ou dont la vue 3D n'a rien a dessiner, produit deux releves
	// vierges -- donc identiques -- et un temps d'image quasi nul. Comptee comme
	// une image auditee, elle rendrait un verdict PASS sans que rien n'ait ete
	// mesure.
	// `updateSeconds`, s'il n'est pas nul, recoit le temps de la SEULE phase 2 --
	// Rml::Context::Update(). C'est la grandeur que la question du cout d'un
	// `data-for` de N lignes concerne ; le temps d'image entiere la noierait
	// dans le rendu de la vue 3D.
	bool RenderFrame (FrameAudit* audit, double* updateSeconds);

	// Relit la taille du tampon et la remet a RmlUi si elle a change.
	void SyncFramebufferSize (int& framebufferWidth, int& framebufferHeight);

	// PHASES 2 A 8, sur un tampon de taille non nulle. Rend true si la vue 3D a
	// ete dessinee.
	bool DrawFrame (int framebufferWidth, int framebufferHeight, FrameAudit* audit,
	                double* updateSeconds, bool readPixel);

	// L'IMAGE DEMANDEE PAR LE SYSTEME, depuis le rappel de rafraichissement de
	// la fenetre -- voir Window::SetRefreshCallback. C'est la seule image rendue
	// pendant un redimensionnement au bord sous Win32.
	void RenderRefreshFrame ();

	// CE QUE LA BOUCLE A PRODUIT, ET QUE LE VERDICT LIT. Le regrouper est ce qui
	// permet de separer les deux : sans cet agregat, chaque compteur ajoute par
	// un instrument rallonge la boucle ET la liste d'arguments du verdict.
	struct LoopOutcome
	{
		int  renderedFrames    = 0;
		bool replacementFailed = false;

		int churnToggles = 0;
		int churnMinRows = 0;
		int churnMaxRows = 0;

		int  selections        = 0;
		int  selectionMaterials = 0;
		bool selectionTruncated = false;

		// --open-second : ce que les panneaux decrivent apres la seconde
		// ouverture, et combien de modeles la scene porte alors.
		bool        secondOpened     = false;
		bool        secondAttempted  = false;
		std::string secondExpected;
		std::string secondFileLine;
		int         secondModelCount = 0;

		// ⚠ UN RELEVE PAR SELECTION, ET AUCUN AVANT LA PREMIERE. Le cout
		// d'amorcage -- tampons GL, caches de textures, arenes de l'allocateur
		// -- est paye a la premiere selection : le compter comme une croissance
		// ferait echouer le critere sur un programme qui ne fuit pas.
		std::vector<SelectionSample> selectionSamples;

		std::vector<double> frameSeconds;
		std::vector<double> updateSeconds;

		// Images rendues mais ecartees de la mesure de temps, parce qu'elles
		// ont contenu une image du rappel de rafraichissement.
		int timingExcluded = 0;
	};

	// Les instruments qui agissent DEPUIS la boucle, sur une image rendue.
	void ChurnTreeOnce (LoopOutcome& outcome);
	void ReopenIfRequested (LoopOutcome& outcome);
	void OpenSecondIfRequested (LoopOutcome& outcome);
	void RecordSecondFile (LoopOutcome& outcome);
	void ReselectOnce (LoopOutcome& outcome);

	// --resize : dimensionne la fenetre apres une image rendue, puis releve
	// l'image rendue suivante. RecordRefreshFrame est appelee par chaque image
	// du rappel de rafraichissement.
	void ResizeIfRequested ();
	void RecordResizeFollowUp ();
	void RecordRefreshFrame (int framebufferWidth, int framebufferHeight);
	bool ReportResize () const;

	// RELIT LE JOURNAL DE RmlUi ET REND 3 S'IL PORTE UN MESSAGE, 0 SINON.
	//
	// ⚠ REGLE : AUCUN CHEMIN DE SORTIE DE Run() NE REND UN CODE SANS AVOIR
	// APPELE CETTE FONCTION. Les deux chemins sont la boucle d'images et
	// --dump-layout ; ce dernier appelle Update(), donc il emet les memes
	// messages, et il est le seul instrument qui etablisse qu'un panneau a du
	// contenu mis en page. Les trois autres vidages -- --dump-tree,
	// --dump-files, --dump-info -- ne sont PAS concernes : main les traite avant
	// toute creation d'Application, ils n'ouvrent aucun contexte RmlUi et il n'y
	// a donc pas de journal a relire.
	int ReportRmlJournal () const;

	// Imprime tous les verdicts et rend le code de sortie du processus.
	int Report (const FrameAudit& audit, const LoopOutcome& outcome);

	Options m_options;

	// NOMBRE DE SELECTIONS DEMANDEES A L'ORIGINE.
	//
	// ⚠ CONSERVE A PART PARCE QUE L'INSTRUMENT ABAISSE `m_options.reselect`
	// quand une ouverture echoue : sans cette copie, le verdict comparerait le
	// nombre effectue a un nombre qu'on vient d'aligner dessus, et serait vert
	// sur une execution qui n'a pas fait ce qui etait demande.
	int m_requestedReselections = 0;

	// ⚠ DECLARE AVANT LA FENETRE, DONC DETRUIT APRES ELLE. Le document detruit
	// la geometrie, et SceneRenderer rend au moteur les cles correspondantes --
	// ce qui libere des tampons GL et exige donc un contexte COURANT. L'ordre
	// des membres est ce qui garantit que la vue meurt avant la fenetre et le
	// document apres tout le monde.
	core::Document m_document;

	std::unique_ptr<platform::Window>       m_window;
	std::unique_ptr<platform::RmlHost>      m_rml;

	// ⚠ DECLARE APRES m_rml, DONC DETRUIT AVANT LUI. Le modele lie designe des
	// membres de ce controleur, et le contexte de RmlUi le lit a chaque
	// Update() : l'ordre inverse laisserait un contexte vivant pointer sur un
	// modele mort.
	std::unique_ptr<ui::UiController>      m_ui;

	std::unique_ptr<view3d::Camera>         m_camera;
	std::unique_ptr<view3d::SceneRenderer>  m_scene;

	// ⚠ DECLARE APRES m_rml : il tient le contexte de RmlUi, qui meurt avec
	// l'hote.
	std::unique_ptr<ui::InputRouter> m_input;

	// ⚠ DECLARE APRES m_rml, DONC DETRUIT AVANT LUI : ses ecouteurs sont
	// attaches aux elements du document, et son destructeur les en detache. Dans
	// l'ordre inverse, la destruction du document les appellerait liberes.
	std::unique_ptr<ui::PanelSplitters> m_splitters;

	// ⚠ LES EVENEMENTS SONT MIS EN FILE, PAS TRAITES DANS LE RAPPEL. Un rappel de
	// glfw court a l'interieur de glfwPollEvents : y charger un fichier ferait
	// tourner une boucle de messages du systeme pendant que la scene est a moitie
	// detruite.
	std::vector<ui::MouseEvent> m_pendingEvents;

	// ⚠ FILE SEPAREE, ET NON FUSIONNEE AVEC CELLE DE LA SOURIS. Les deux n'ont
	// pas le meme destinataire : la souris est ARBITREE entre la vue et
	// l'interface, le clavier va toujours a l'element qui a le focus. Les fondre
	// obligerait chaque evenement a porter les champs de l'autre, et ferait
	// passer le clavier par une conversion de repere qui n'a pas de sens pour
	// lui.
	std::vector<ui::KeyEvent> m_pendingKeys;

	// Derniere position connue du curseur, en pixels du tampon de dessin. glfw ne
	// la livre ni avec un bouton ni avec la molette.
	int m_cursorX = 0;
	int m_cursorY = 0;

	// DERNIER RECTANGLE LU, celui que la phase 3 de l'image PRECEDENTE a mesure.
	//
	// ⚠ LA PHASE 1 ARBITRE AVEC LA GEOMETRIE DE L'IMAGE PRECEDENTE, et c'est une
	// CONSEQUENCE DE L'ORDRE CHOISI, pas une contrainte qui s'impose a lui :
	// arbitrer apres l'Update() supposerait de connaitre le survol avant d'avoir
	// remis l'evenement a RmlUi, ce qui est circulaire. Le retard vaut donc UNE
	// image sur le seul picking, et il ne porte QUE sur le rectangle -- les
	// dimensions du tampon, elles, sont posees AVANT l'arbitrage (phase 1),
	// parce que ce bloc-la ne depend d'aucun evenement.
	bool             m_hasViewRect = false;
	ui::ViewportRect m_viewRect;

	// ⚠ ALT + CLIC GAUCHE TIENT JUSQU'AU RELACHEMENT, et c'est la raison d'etre
	// de l'etat. Le trackball n'est PAS engage par ce geste : sans ce drapeau, le
	// moindre tremblement entre l'enfoncement et le relachement ferait tourner la
	// vue au moment meme ou l'operateur vise, et le relachement selectionnerait.
	bool m_altPivotGesture = false;

	// ISSUE EXACTE DU DERNIER Alt + CLIC, ET NON UN BOOLEEN.
	//
	// ⚠ RELEVE PAR LE GESTE, et non recalcule par l'instrument : un second appel
	// a SetPivotFromPixel mesurerait la fonction, pas ce que le geste en a fait.
	//
	// ⚠ LES QUATRE ISSUES SONT DISTINGUEES PARCE QUE LE VERDICT DOIT LES
	// DISTINGUER. « dans la vue, aucune surface » laisse le pivot inchange et
	// c'est BON ; « hors du rectangle » ou « la vue n'a pas de dimensions » le
	// laissent inchange aussi, et ce sont des mesures NON CONCLUANTES. Reduites
	// a un booleen, les trois rendent le meme verdict, donc BON.
	view3d::PivotPickStatus m_altPivotStatus = view3d::PivotPickStatus::NoView;

	// PROPRIETAIRE RETENU POUR LE DERNIER ENFONCEMENT ARBITRE.
	//
	// ⚠ SANS LUI, --alt-pivot NE DISTINGUE PAS « le geste a vise dans le vide »
	// DE « le geste n'a jamais atteint la vue ». Un pixel dans un panneau, un
	// gel du drapeau Alt et une panne totale de l'arbitrage produisent tous les
	// trois un pivot inchange -- c'est-a-dire la signature du cas CONFORME.
	ui::InputRouter::Owner m_lastPressOwner = ui::InputRouter::Owner::None;

	// Position de l'enfoncement, dans le repere de la VUE, pour distinguer le
	// clic du glisser.
	int m_pressX = 0;
	int m_pressY = 0;

	// DERNIERE POSITION DU GESTE EN COURS, dans le repere de la VUE.
	//
	// ⚠ ELLE EXISTE POUR L'ANNULATION, QUI NE PORTE AUCUNE COORDONNEE. Une perte
	// de focus n'en donne pas -- pas plus qu'une sortie de cadre -- et le
	// trackball attend malgre tout une position a son relachement.
	int m_gestureViewX = 0;
	int m_gestureViewY = 0;

	// Fichier annonce par une image de transition et pas encore charge.
	std::filesystem::path m_pendingOpenPath;

	// DERNIER ECHEC DE CHARGEMENT, tel que ReportLoadFailure l'a pose.
	//
	// ⚠ CONSERVE PARCE QUE LE VERDICT DE --dump-status CHERCHE CES DEUX
	// SOUS-CHAINES DANS LE STATUT. Les recalculer au moment du verdict
	// comparerait le statut a une chaine fabriquee par l'instrument, et non a ce
	// que le chargeur a reellement dit.
	std::string m_loadFailureName;
	std::string m_loadFailureReason;
	bool        m_modelLoaded = false;

	int m_framebufferWidth  = 0;
	int m_framebufferHeight = 0;

	// Garde de reentrance du rappel de rafraichissement.
	bool m_inRefreshFrame = false;

	// TAILLE DE LA DERNIERE IMAGE EFFECTIVEMENT DESSINEE, posee apres l'echange.
	// ⚠ Distincte de m_framebufferWidth/Height, que SyncFramebufferSize met a
	// jour AVANT de savoir si une image sera dessinee.
	int m_drawnWidth  = 0;
	int m_drawnHeight = 0;

	// Images rendues par le rappel de rafraichissement, sur toute l'execution.
	int m_refreshFrames = 0;

	// CE QUE --resize RELEVE.
	//
	// ⚠ LE RECTANGLE DE L'IMAGE DU RAPPEL EST COMPARE A CELUI DE L'IMAGE
	// SUIVANTE : pendant un redimensionnement au bord, l'image du rappel est la
	// seule que l'operateur voit, donc elle doit deja etre celle de la nouvelle
	// taille, pas une image intermediaire.
	struct ResizeProbe
	{
		bool             issued        = false;
		bool             followed      = false;
		// La demande egale la taille courante : rien a redimensionner, donc
		// rien a mesurer.
		bool             sameAsCurrent = false;
		ui::ViewportRect before;
		// Images du rappel posterieures a la demande et anterieures a l'image
		// suivante, QUELLE QUE SOIT LEUR TAILLE, et la taille de la derniere.
		int              refreshAfterRequest = 0;
		int              refreshWidth        = 0;
		int              refreshHeight       = 0;
		ui::ViewportRect refreshRect;
		ui::ViewportRect after;
		int              framebufferWidth  = 0;
		int              framebufferHeight = 0;
		int              contextWidth      = 0;
		int              contextHeight     = 0;
	};
	ResizeProbe m_resize;

	// L'element de la vue a ete trouve a l'initialisation ; sa disparition en
	// cours d'execution se journalise une fois, pas a chaque image.
	bool m_viewportLossReported = false;

	// L'INSTRUMENT, et il vit dans app/dump_modes.h. ⚠ Seul m_viewportTracker
	// tourne sans option : la concordance du GL_VIEWPORT est permanente.
	ViewportTracker m_viewportTracker;
	PixelProbe      m_pixelProbe;
};

} // namespace app
} // namespace peles
