#pragma once

#include "gl_state_fence.h"
#include "input_event.h"
#include "ui_command.h"
#include "viewport_rect.h"
// `PivotPickStatus` est une enumeration dont les quatre valeurs sont LUES par le
// verdict de --alt-pivot : une declaration anticipee opaque suffirait au
// compilateur et ferait mentir le fichier au premier ajout d'issue.
#include "view3d/picking.h"

#include <functional>
#include <string>
#include <vector>

namespace peles
{
namespace platform
{
class RmlHost;
class Window;
}
namespace ui
{
class InputRouter;
}
namespace view3d
{
class Camera;
}

namespace app
{

// ---------------------------------------------------------------------------
//  L'INSTRUMENT, SEPARE DE L'HOTE
// ---------------------------------------------------------------------------
//
// ⚠ CE FICHIER EXISTE POUR QUE application.cpp RESTE LISIBLE. L'instrument pese
// plus que l'hote qu'il mesure -- releve d'etat, concordance de viewport, temps
// par image, lecture de pixel, vidage de mise en page, vidages d'arbre et de
// liste, remplacement de scene, repli repete. Melange a l'ordre des huit phases,
// il le noierait.
//
// ⚠ AUCUN TYPE OpenGL DANS CETTE DECLARATION, meme regle que gl_state_fence.h :
// l'en-tete doit rester incluable depuis une unite qui ne touche pas
// gl_wrapper.h.
//
// ⚠ AUCUNE DE CES FONCTIONS N'EST DERRIERE UNE OPTION PAR ELLE-MEME. C'est
// l'appelant qui decide de les appeler ; ce fichier decide seulement de ce
// qu'elles impriment et de ce qu'elles concluent.

// ---------------------------------------------------------------------------
//  --state-audit
// ---------------------------------------------------------------------------

// Ce que le mode --state-audit accumule sur une execution.
//
// ⚠ LES DEUX PHASES SOUS CLOTURE SONT ENCADREES, PAS UNE SEULE. Le contrat en
// pose deux ; n'en mesurer qu'une laisserait la clôture de la vue 3D sans
// verification, d'autant que la phase 6 renormalise a la main trois des etats
// qu'elle est censee avoir restaures.
struct FrameAudit
{
	GlStatePhaseAudit scene { "phase 5 (vue 3D, cgre)" };
	GlStatePhaseAudit ui { "phase 7 (interface, RmlUi)" };

	// ⚠ DES IMAGES RENDUES, PAS DES TOURS DE BOUCLE.
	int frames          = 0;
	int divergingFrames = 0;

	// La preuve conservee est celle de la PREMIERE image divergente.
	//
	// ⚠ Le mode de panne normal de cet instrument est « divergent a la premiere
	// image, masque ensuite » -- voir le biais intra-image dans Application::Run.
	// Imprimer le releve de la derniere image reviendrait a imprimer une preuve
	// conforme sous un verdict d'echec, exactement dans le cas ou on en a besoin.
	struct Divergence
	{
		bool            captured = false;
		int             frame    = 0;
		std::string     phase;
		std::string     states;
		GlStateSnapshot before;
		GlStateSnapshot after;
	};
	Divergence first;

	// Dernier couple effectivement mesure, imprime quand rien n'a diverge.
	GlStatePhaseAudit lastMeasured;
	int               lastMeasuredFrame = 0;

	// Remet les deux phases a « non mesurees ». A appeler en TETE d'image : un
	// releve de l'image precedente relu comme celui de celle-ci laisserait un
	// couple identique et conforme derriere une phase sautee.
	void BeginFrame ();

	void Accumulate (int frame);
};

// Rend true si le releve est conforme. Imprime la preuve de la PREMIERE image
// divergente, jamais celle de la derniere image rendue.
bool ReportStateAudit (const FrameAudit& audit);

// ---------------------------------------------------------------------------
//  Concordance du GL_VIEWPORT -- PERMANENTE, pas derriere une option
// ---------------------------------------------------------------------------

// Compare le GL_VIEWPORT effectif a la fin de la phase 5 au rectangle lu.
//
// ⚠ C'EST CE QUI ETABLIT QU'IL N'EXISTE QU'UNE SEULE GEOMETRIE. Le jour ou un
// second calcul du rectangle apparaitra en C++, les deux divergeront et cette
// comparaison le dira -- ce qu'aucune inspection visuelle ne ferait. Elle tourne
// a chaque image, pas derriere une option.
class ViewportTracker
{
public:
	// ⚠ A APPELER A L'INTERIEUR DE LA CLOTURE de la phase 5 : GL_VIEWPORT fait
	// partie de la pile d'attributs, donc le depilement l'aurait deja ramene au
	// plein cadre et la comparaison porterait sur autre chose que la vue.
	// Exige un contexte GL courant.
	void Check (const ui::GlViewport& expected);

	// Rend true si le GL_VIEWPORT a concorde sur toutes les images rendues.
	bool Report () const;

	bool Diverged () const { return m_mismatches > 0; }

	// Dernier rectangle GL RELEVE -- et non attendu. ⚠ La distinction est le
	// point : sous divergence, imprimer l'attendu a cote d'un pixel lu
	// decrirait autre chose que ce qui a servi a dessiner.
	bool HasLastActual () const { return m_hasLastActual; }
	const int* LastActual () const { return m_lastActual; }

private:
	int  m_checked    = 0;
	int  m_mismatches = 0;

	// La preuve conservee est celle de la PREMIERE divergence.
	bool m_captured   = false;
	int  m_frame      = 0;
	int  m_expected[4] = { 0, 0, 0, 0 };
	int  m_actual[4]   = { 0, 0, 0, 0 };

	bool m_hasLastActual   = false;
	int  m_lastActual[4]   = { 0, 0, 0, 0 };
};

// ---------------------------------------------------------------------------
//  --readpixel
// ---------------------------------------------------------------------------

// Lit un pixel du tampon arriere et le compare a la valeur predite pour le
// materiau par defaut sous la pose de mesure.
class PixelProbe
{
public:
	// Exige un contexte GL courant, et doit etre appelee AVANT l'echange de
	// tampons.
	void Read (int x, int y);

	bool WasRead () const { return m_read; }

	// Rend true si le pixel lu est dans la tolerance de la valeur predite.
	//
	// `modelCount` : nombre de modeles du document, imprime et EXIGE NON NUL --
	// une scene vide rend la couleur d'effacement, et rien d'autre ne le dirait.
	// `viewport` : rectangle GL RELEVE de la vue, ou nullptr s'il n'y en a pas.
	bool Report (int x, int y, int modelCount, const int* viewport) const;

private:
	bool          m_read     = false;
	unsigned char m_pixel[3] = { 0, 0, 0 };
};

// ---------------------------------------------------------------------------
//  --dump-layout et --dump-timing
// ---------------------------------------------------------------------------

// Imprime le rectangle resolu de `elementId` pour la taille demandee et rend le
// code de sortie du processus. Aucune image n'est rendue : c'est la GEOMETRIE
// qui est mesuree.
int DumpLayout (platform::RmlHost& rml, const char* elementId, int width, int height);

// ---------------------------------------------------------------------------
//  --dump-pick : LA CONVERSION DE REPERE, ET RIEN D'AUTRE
// ---------------------------------------------------------------------------
//
// Imprime le rayon monde du pixel FENETRE demande, puis celui du pixel CENTRAL
// du rectangle de la vue, et compare le second a l'axe de vue.
//
// ⚠ C'EST LE SEUL DETECTEUR DU LOT QUI VOIE UN DECALAGE DE REPERE. Un picking
// decale de la largeur du panneau gauche ne change AUCUN pixel dessine :
// --readpixel, --dump-layout et le detecteur de concordance du GL_VIEWPORT
// restent tous les trois verts.
//
// ⚠ LE RAYON CENTRAL EST CALCULE DEPUIS UN PIXEL FENETRE, LUI AUSSI. Le
// calculer directement en coordonnees de vue contournerait la soustraction que
// le critere mesure, et le detecteur ne pourrait plus echouer.
//
// ⚠ L'INSTRUMENT NE POSE AUCUNE DIMENSION, ET C'EST CE QUI FAIT DE LUI UNE
// MESURE DU PRODUIT. `rect` et `camera` sont ceux qu'une image REELLEMENT
// RENDUE a laisses : le rectangle vient de la phase 3, les dimensions de la
// phase 5. Un `SetViewSize` ecrit ici injecterait le cas negatif du critere 2
// dans l'INSTRUMENT, et le produit pourrait alors poser une hauteur fausse --
// celle du CADRE au lieu de celle du rectangle -- en laissant les six criteres
// verts, pour un rapport d'aspect errone et un picking decale sur tout pixel
// hors du centre.
//
// Deux verdicts, et aucun ne suffit seul :
//   1) les dimensions POSEES PAR LE PRODUIT sont celles du rectangle lu ;
//   2) le produit vectoriel du rayon central avec l'axe de vue est nul a 1e-5
//      pres -- ils sont colineaires PAR CONSTRUCTION, le NDC du centre valant
//      (0,0).
int DumpPick (const ui::ViewportRect& rect, view3d::Camera& camera, int frameWidth,
              int frameHeight, int pixelX, int pixelY);

// ---------------------------------------------------------------------------
//  --input-script : L'ARBITRAGE, SUR UNE SEQUENCE INJECTEE
// ---------------------------------------------------------------------------
//
// Deroule une sequence d'evenements souris SANS SOURIS REELLE et imprime, pour
// chacun, l'element survole par RmlUi et le proprietaire retenu.
//
// ⚠ LA SEQUENCE EST A DEUX VERSANTS, ET C'EST CE QUI LA REND FALSIFIABLE DANS
// LES DEUX SENS. Le premier versant -- enfoncement sur la vue, deplacement puis
// relachement SUR UN PANNEAU -- doit rendre « vue » pour les TROIS : c'est la
// regle de capture. Le second -- les memes gestes entierement sur le panneau --
// doit rendre « interface » : sans lui, un arbitre qui rendrait toujours « vue »
// passerait le premier versant.
int DumpInputScript (platform::RmlHost& rml, ui::InputRouter& router,
                     const char* viewportElementId, int width, int height);

// ---------------------------------------------------------------------------
//  --alt-pivot : LE PIVOT, COMPARE AU BIT PRES AVANT ET APRES LE GESTE
// ---------------------------------------------------------------------------
//
// Rend true si le pivot a fait ce que le geste demande : POSE sur la surface
// quand il y en avait une sous le curseur, INCHANGE AU BIT PRES quand il n'y en
// avait pas.
//
// ⚠ LE VERDICT EST A DEUX VERSANTS, ET C'EST CE QUI L'EMPECHE D'ETRE VIDE. Un
// geste qui ne poserait JAMAIS le pivot passerait un critere reduit au cas sans
// surface ; un geste qui le poserait TOUJOURS -- a l'origine du monde, par
// exemple -- echouerait sur ce meme cas mais passerait l'autre. Les deux
// executions ensemble ferment les deux portes.
//
// ⚠ L'EGALITE EST CELLE DES OCTETS, PAS CELLE DES FLOTTANTS IMPRIMES. Un pivot
// repose sur une valeur recalculee peut differer au dernier bit tout en
// s'imprimant a l'identique sous %f : la comparaison porte sur la
// representation, et la sortie imprime les octets.
//
// ⚠ ET IL EXIGE D'ABORD QUE LE GESTE SOIT ARRIVE. `reachedViewport` dit si
// l'arbitre a bien attribue l'enfoncement a la vue, `status` dit ce que le geste
// y a fait. Sans ces deux entrees, « dans la vue, aucune surface » -- qui est
// BON -- ne se distingue pas de « jamais arrive dans la vue » : un pixel dans un
// panneau, un gel du drapeau Alt et la panne totale de l'arbitrage laissent tous
// les trois le pivot inchange, c'est-a-dire la signature du cas CONFORME.
//
// Les quatre issues de `PivotPickStatus` sont TOUTES lues : `Ok` exige un pivot
// MODIFIE, `NoHit` exige un pivot inchange, `OutOfViewport` et `NoView` rendent
// la mesure NON CONCLUANTE.
bool ReportAltPivot (int windowX, int windowY, bool reachedViewport,
                     view3d::PivotPickStatus status, const float before[3],
                     const float after[3]);

// ---------------------------------------------------------------------------
//  --drag : L'ORBITE, MESUREE SUR L'AXE DE VUE
// ---------------------------------------------------------------------------
//
// Rend true si le glisser a TOURNE la vue sans DEPLACER le pivot.
//
// ⚠ C'EST LA VERSION PRODUIT DE LA REGLE DE CAPTURE, et elle est plus severe
// que celle de --input-script. Celle-ci mesure le proprietaire retenu par
// l'arbitre ; celle-la mesure ce que la CAMERA en a fait. Un glisser dont
// l'arrivee est sur un panneau doit faire tourner la vue exactement comme un
// glisser qui reste dedans -- si la capture tombe, l'angle vaut zero.
//
// ⚠ ET LE PIVOT DOIT RESTER INCHANGE AU BIT PRES : une orbite tourne AUTOUR du
// centre, elle ne le deplace pas. Sans cette seconde condition, un geste qui
// recadrerait la scene passerait pour une rotation.
//
// ⚠ `arrivalInView` EST LU, PAS SEULEMENT IMPRIME. Un glisser qui ARRIVE DANS
// LA VUE ne prouve rien de la regle de capture : il tournerait aussi bien sans
// elle. Seul un glisser SORTANT en est la version produit, et le verdict le dit
// -- sinon le versant « produit » de la capture reposait sur le choix de
// coordonnees par l'operateur, hors de toute trace.
bool ReportDrag (int fromX, int fromY, int toX, int toY, bool arrivalInView,
                 const float axisBefore[3], const float axisAfter[3], const float pivotBefore[3],
                 const float pivotAfter[3]);

// ---------------------------------------------------------------------------
//  --drag-bump : LE GESTE A DEUX BOUTONS
// ---------------------------------------------------------------------------

// Bouton qui OUVRE le geste -- donc celui, et le seul, que la camera doit
// suivre.
enum class GestureOwner
{
	None, // --drag-bump non demande
	Left,
	Right
};

// Rend true si le second bouton, enfonce PUIS RELACHE au milieu du geste, n'a
// rien change a ce que le premier fait.
//
// ⚠ DEUX VERSANTS, ET AUCUN NE COUVRE L'AUTRE.
//   Left  : le glisser gauche doit TOURNER malgre le clic droit intercale. Un
//           relachement de trop pose `tb_state = RELEASED` et la rotation vaut
//           alors exactement zero -- deux ordres de grandeur sous le seuil.
//   Right : le clic gauche intercale, IMMOBILE, ne doit rien selectionner -- son
//           deplacement nul passe sous le seuil clic/glisser -- et l'oeil doit
//           quand meme avoir bouge, sans quoi le dolly n'aurait pas eu lieu et
//           « rien de selectionne » serait vrai pour la mauvaise raison.
bool ReportDragBump (GestureOwner owner, int fromX, int fromY, int toX, int toY,
                     double rotationDeg, bool selectionChanged, const float eyeBefore[3],
                     const float eyeAfter[3]);

// ---------------------------------------------------------------------------
//  --focus-loss : UN APPUI IMMOBILE INTERROMPU PAR UNE PERTE DE FOCUS
// ---------------------------------------------------------------------------
//
// ⚠ CE QUI EST MESURE N'EST PAS « LA CAPTURE NE RESTE PAS COINCEE » : glfw
// synthetise un relachement par bouton enfonce a la perte du focus, donc aucune
// capture ne reste coincee, et un verdict bati la-dessus serait vert sans rien
// mesurer. Ce qui est mesure est que le geste est ANNULE et non TERMINE : ce
// relachement synthetique porte la DERNIERE position du curseur, donc un
// deplacement nul, donc un clic -- et un Alt+Tab pendant un appui immobile
// SELECTIONNAIT le modele sous le curseur.
//
// ⚠ LE TEMOIN EST LA MOITIE UTILE DU DETECTEUR. « Rien n'a ete selectionne »
// est vrai aussi quand le pixel ne designe aucun modele, quand l'arbitre n'a
// pas rendu le geste a la vue, ou quand le picking est casse. L'instrument
// exerce donc d'abord la SEQUENCE NORMALE sur le meme pixel : elle DOIT
// selectionner. Sans ce temoin, le verdict passerait sur une application ou
// plus rien ne fonctionne.
bool ReportFocusLoss (int windowX, int windowY, bool ownerWasViewport, bool controlSelected,
                      bool cancelSelected, bool stillHolding);

// ---------------------------------------------------------------------------
//  --click / --double-click : LE GENRE ET L'EFFET, PAS LE NOMBRE
// ---------------------------------------------------------------------------

// Ce que le clic doit produire.
//
// ⚠ `Any` N'EST PAS UN VERDICT VIDE POUR AUTANT : le double-clic exige toujours
// une OpenFile et un modele de plus, le clic simple exige toujours l'ABSENCE
// d'OpenFile. `Any` ne relache que le genre de la commande de designation.
enum class ClickExpectation
{
	Any,
	Select, // une SelectFile, et rien d'autre
	Toggle, // une ToggleDirectory
	Open    // une OpenFile (implique par --double-click)
};

// Rend true si la chaine complete a fait ce qui etait attendu.
//
// ⚠ COMPTER LES COMMANDES NE MESURE RIEN. Un `data-event-dblclick` absent du
// document -- qui s'edite sans recompiler -- laisse `--double-click` produire
// deux `SelectFile`, AUCUNE ouverture et AUCUN modele dans la scene : un simple
// compte de commandes conclurait « cablage vivant ». Le nombre de modeles AVANT
// et APRES est donc lu au meme titre que le genre des commandes -- une commande
// produite et jamais honoree se presente a l'ecran comme une commande jamais
// produite.
bool ReportClick (bool doubleClick, ClickExpectation expected,
                  const std::vector<ui::UiCommand>& commands, int modelsBefore, int modelsAfter);

// ---------------------------------------------------------------------------
//  --dump-scroll : LA DERNIERE LIGNE D'UN PANNEAU EST-ELLE ATTEIGNABLE ?
// ---------------------------------------------------------------------------
//
// Injecte des crans de molette au-dessus de `containerId` et mesure si la
// DERNIERE ligne de `contentId` entre dans la bande visible.
//
// ⚠ DEUX CONDITIONS, ET AUCUNE NE SUFFIT SEULE. Une barre sans routage de
// molette ne sert a rien ; un routage sans barre laisse l'operateur sans prise.
// L'instrument passe donc par l'arbitre -- le meme que la souris -- et rend
// compte des deux : presence de l'element de barre, et position atteinte.
//
// ⚠ UN CONTENU QUI NE DEBORDE PAS REND LE VERDICT NON CONCLUANT, PAS BON : il
// n'y aurait rien a atteindre, et « la derniere ligne est visible » serait vrai
// sans que le defilement existe.
int DumpScroll (platform::RmlHost& rml, ui::InputRouter& router, const char* viewportElementId,
                int width, int height, const char* containerId, const char* contentId);

// ---------------------------------------------------------------------------
//  --dump-copy : LA LIGNE « Path » EST-ELLE SELECTIONNABLE ET COPIABLE ?
// ---------------------------------------------------------------------------
//
// Clique sur l'element designe par `selector`, injecte Ctrl+A puis Ctrl+C, et
// relit le PRESSE-PAPIER DU SYSTEME.
//
// ⚠ TROIS MECANISMES SONT MESURES ENSEMBLE, ET AUCUN AUTRE VERDICT DU LOT N'EN
// COUVRE UN SEUL : que le clic donne le FOCUS au controle, que les touches
// soient ROUTEES jusqu'a lui, et que `SystemInterface::SetClipboardText` soit
// surchargee vers le presse-papier du SYSTEME. Sans la troisieme, Ctrl+C
// n'ecrit que dans une chaine membre de RmlUi et rien ne le dit.
//
// ⚠ UNE SENTINELLE EST SEMEE AVANT LE GESTE, ET C'EST INDISPENSABLE. Sans elle,
// un presse-papier qui contiendrait DEJA le texte attendu -- une execution
// precedente, une copie faite a la main -- rendrait le verdict vert sans que le
// geste ait rien fait.
//
// ⚠ LA SENTINELLE EST SEMEE ET RELUE PAR LA FENETRE, PAS PAR L'INTERFACE DE
// RmlUi. Passer par celle qu'on mesure rendrait la mesure tautologique.
// ⚠ LES TOUCHES SONT REMISES PAR L'APPELANT, ET CE N'EST PAS UNE COMMODITE. Le
// routage clavier est fait de DEUX maillons -- le drainage de la file par
// l'hote, puis `InputRouter::RouteKey` -- et un instrument qui appellerait
// l'arbitre directement sauterait le premier : MESURE, avec le drainage de
// l'hote neutralise, le verdict restait VERT. `sendKey` est donc le chemin du
// produit, pose par `Application`, et non un raccourci vers l'arbitre.
using KeySink = std::function<void (const ui::KeyEvent&)>;

int DumpCopy (platform::RmlHost& rml, platform::Window& window, ui::InputRouter& router,
              const KeySink& sendKey, const char* viewportElementId, int width, int height,
              const char* selector);

// Deux series : le temps d'une image ENTIERE, et le temps de la seule phase 2,
// Rml::Context::Update(). `treeRows` est le nombre de lignes liees a l'arbre au
// moment de la mesure.
//
// ⚠ LA PHASE 2 EST MESUREE SEPAREMENT PARCE QUE C'EST ELLE QUE LA QUESTION
// CONCERNE : le cout d'un `data-for` de N lignes est un cout de mise a jour et
// de mise en page, pas un cout de dessin. Une mediane d'image entiere le
// noierait dans le rendu de la vue 3D.
//
// `excluded` compte les images rendues mais absentes des deux series : celles
// qui ont contenu une image du rappel de rafraichissement.
void ReportTiming (const std::vector<double>& frameSeconds,
                   const std::vector<double>& updateSeconds, int treeRows, int excluded);

// ---------------------------------------------------------------------------
//  --dump-tree et --dump-files
// ---------------------------------------------------------------------------
//
// ⚠ CES DEUX MODES N'OUVRENT AUCUN CONTEXTE GL ET NE LISENT AUCUN ASSET : ils
// n'appellent que peles_core. Ils sont donc executables sur une machine sans
// interface graphique, et main les traite AVANT toute creation.
//
// ⚠ LEUR SORTIE EST UN ORACLE, ce qui impose trois proprietes : elle ne
// contient AUCUN CHEMIN ABSOLU (elle serait propre a une machine), elle est
// triee par un ordre qui ne depend d'aucune locale, et elle est ecrite sans
// traduction de fin de ligne (elle differerait d'un octet par ligne entre
// Windows et Linux).
//
// Rendent le code de sortie du processus : 0 en cas de succes, 2 si le
// repertoire n'est pas lisible.
int DumpTree (const std::string& directory);
int DumpFiles (const std::string& directory);

// ---------------------------------------------------------------------------
//  --dump-info : l'oracle TEXTUEL des panneaux de droite
// ---------------------------------------------------------------------------
//
// Imprime les QUATORZE lignes de la table de proprietes, puis l'arbre des
// maillages et l'arbre des materiaux, tous deux aplatis et entierement deplies.
//
// ⚠ IL APPELLE EXACTEMENT CE QUE L'INTERFACE APPELLE -- core::BuildModelInfo,
// BuildMeshTree, BuildMaterialTree -- et c'est la seule chose qui en fasse un
// oracle. Une seconde mise en forme ecrite pour la console decrirait une autre
// sortie que celle de l'ecran.
//
// ⚠ `path` VIDE N'EST PAS UNE ERREUR : c'est le cas « aucun fichier charge »,
// et il doit rendre les quatorze memes lignes, valeurs vides. Un fichier
// illisible rend lui aussi les quatorze lignes, mais avec le code 2 et un
// message : les deux situations se ressemblent a l'ecran et une seule est une
// faute.
int DumpInfo (const std::string& path);

// ---------------------------------------------------------------------------
//  --reselect : la memoire, sur des selections successives du MEME fichier
// ---------------------------------------------------------------------------
//
// Empreinte memoire PRIVEE du processus, en kibioctets. Rend false si la
// plateforme ne sait pas la donner -- auquel cas le verdict devient NON
// CONCLUANT, jamais bon.
bool ReadProcessFootprintKb (long long& kilobytes);

// NOM DE LA GRANDEUR QUE ReadProcessFootprintKb A LUE.
//
// ⚠ ELLE N'EST PAS LA MEME SUR LES DEUX PLATEFORMES, et la taire ferait lire
// deux mesures differentes sous une seule tolerance. La ligne de sortie la
// nomme, pour qu'une execution Linux soit interpretable sans relire ce fichier.
const char* ProcessFootprintName ();

// Ce qu'une selection laisse derriere elle : les registres exacts, et
// l'empreinte du processus.
//
// ⚠ LES ENTIERS SONT EXACTS, L'EMPREINTE EST UNE TENDANCE. Le verdict porte sur
// les entiers ; l'empreinte est imprimee parce qu'elle est la grandeur que le
// critere nomme, mais elle ne peut pas le trancher : elle mesure ce que
// l'ALLOCATEUR a demande au systeme, pas ce que le programme retient, et elle
// varie de plusieurs centaines de kibioctets d'une execution a l'autre sur la
// meme ligne de commande.
struct SelectionSample
{
	// Cles vivantes dans le cache de donnees derivees de cgmesh -- demi-aretes,
	// triangulation, compteurs topologiques. C'EST L'EQUIVALENT EXACT DE LA
	// wxImageList QUE L'HOTE HISTORIQUE DEVAIT PURGER A LA MAIN.
	int cacheEntries = 0;

	// Maillages enregistres aupres du moteur de rendu.
	int registeredMeshes = 0;

	// ---------------------------------------------------------------------
	//  LES TROIS VECTEURS LIES, ET C'EST LA GRANDEUR QUE LE CRITERE NOMME
	// ---------------------------------------------------------------------
	//
	// ⚠ LES DEUX REGISTRES CI-DESSUS SONT INDEXES PAR MAILLAGE, PAS PAR
	// MATERIAU. Un fichier a 60 materiaux sur UN maillage les laisse a « 1, 1 »
	// quel que soit le nombre de materiaux : le nombre que le critere nomme
	// n'entrait donc dans AUCUNE grandeur que le verdict lisait. Ces trois
	// compteurs sont indexes par LIGNE LIEE -- 242 pour ce fichier -- donc par
	// la grandeur qui croit avec le nombre de materiaux.
	//
	// ⚠ ET C'EST EXACTEMENT LA FORME DU DEFAUT REMPLACE : la wxImageList que
	// l'hote historique devait purger recevait UNE ENTREE PAR LIGNE. Une
	// accumulation de la meme taille -- un InfoRow par ligne, soit ~48 Kio par
	// selection sur 60 materiaux -- passait sous la tolerance a gros grain de
	// l'empreinte ET sous les deux registres par maillage.
	int infoRows     = 0;
	int meshRows     = 0;
	int materialRows = 0;

	long long footprintKb = 0;
};

// Rend true si l'arbre des materiaux est COMPLET et si rien ne s'est accumule
// d'une selection a l'autre.
//
// ⚠ CINQ CONDITIONS D'ENTIERS, PLUS DEUX GARDES, ET CHACUNE PEUT FAIRE ECHOUER
// SEULE :
//   -- `performed` doit valoir `requested` : une execution qui n'a pas fait ce
//      qui etait demande ne prouve rien, exactement comme pour --churn-tree ;
//   -- l'arbre doit etre complet -- `truncated` faux et `materials` non nul --
//      sans quoi la mesure porterait sur un arbre que le plafond a ecourte ;
//   -- le nombre d'entrees de cache doit etre IDENTIQUE d'une selection a
//      l'autre, sans tolerance ;
//   -- le nombre de maillages enregistres aussi ;
//   -- les TROIS nombres de lignes liees aussi.
//
// ⚠ LA PREMIERE SELECTION EST LA REFERENCE, ET NON L'ETAT D'AVANT. Les
// allocations d'amorcage -- tampons GL, caches de textures, arenes de
// l'allocateur -- sont payees a la premiere, pas aux suivantes : partir de
// l'etat initial ferait compter ce cout unique comme une croissance.
bool ReportSelectionMemory (int requested, int performed, int materials, bool truncated,
                            const std::vector<SelectionSample>& samples);

// ---------------------------------------------------------------------------
//  --reopen : le contrat de destruction, mis a l'epreuve
// ---------------------------------------------------------------------------
//
// Rend true si le remplacement de scene a rendu au moteur TOUTES les cles.
//
// ⚠ `registeredBefore` NUL REND LE VERDICT NON CONCLUANT, PAS BON. Si aucun
// maillage n'etait enregistre, la destruction n'avait rien a rendre : le
// compteur a zero des deux cotes ressemblerait a un succes alors que la branche
// utile n'a pas couru. C'est la meme distinction que SKIP contre PASS.
//
// ⚠ `opened` EST LE PREMIER TERME DU VERDICT, ET IL NE PEUT PAS ETRE OMIS. Le
// chargement VIDE LA SCENE AVANT de lire, et ne la restaure pas en cas d'echec :
// un fichier de remplacement illisible rend donc « avant = 1, apres = 0 » --
// c'est-a-dire la signature EXACTE d'un contrat tenu. Sans ce terme, le verdict
// est vert sur une execution qui n'a rien remplace.
bool ReportSceneReplacement (bool opened, int registeredBefore, int registeredAfter);

// ---------------------------------------------------------------------------
//  --open-second : LES PANNEAUX DECRIVENT-ILS LE DERNIER FICHIER OUVERT ?
// ---------------------------------------------------------------------------
//
// Rend true si la ligne « File » du modele lie nomme le SECOND fichier.
//
// ⚠ TROIS GARDES, ET CHACUNE EVITE UN VERDICT VERT QUI NE MESURE RIEN :
//   -- le second fichier doit avoir ete OUVERT ;
//   -- la scene doit en porter DEUX. Avec un seul modele, la selection ne peut
//      pas designer le mauvais : le critere serait vrai sans etre eprouve ;
//   -- les deux noms doivent DIFFERER. Ouvrir deux fois le meme fichier rendrait
//      la comparaison vraie quelle que soit la selection.
bool ReportSecondFile (bool opened, const std::string& fileLine, int modelCount,
                       const std::string& expectedSecond, const std::string& firstFile);

// ---------------------------------------------------------------------------
//  --churn-tree : le vecteur lie, retreci et regrandi
// ---------------------------------------------------------------------------
//
// RmlUi declare la suppression d'un element engendre par `data-for` comme un
// comportement indefini. Ce que fait l'interface, elle, n'est pas de supprimer
// un element : c'est de faire CHANGER DE TAILLE le vecteur source. Cet
// instrument repete l'operation pour que la distinction soit exercee au lieu
// d'etre supposee.
//
// ⚠ UN NOMBRE DE LIGNES QUI N'A PAS VARIE REND LE VERDICT NON CONCLUANT : le
// vecteur n'a alors ni retreci ni regrandi, et l'absence de panne ne dit rien.
//
// ⚠ `requested` EST COMPARE A `toggles`, ET C'EST LA SECONDE CONDITION DU
// VERDICT. Un basculement par image RENDUE : `--frames 1 --churn-tree 100`
// n'en produit qu'UN seul, et sans cette comparaison il rendait « REPLI OK »
// sur une execution qui n'a pas fait le centieme de ce qui etait demande.
bool ReportTreeChurn (int requested, int toggles, int minRows, int maxRows);

// ---------------------------------------------------------------------------
//  --dump-status : CE QUE LA ZONE DE STATUT DIT D'UN CHARGEMENT
// ---------------------------------------------------------------------------
//
// ⚠ LE STATUT EST UNE CHAINE DU MODELE LIE, DONC INVISIBLE DE LA CONSOLE. Sans
// cet instrument, « le statut nomme le fichier et la raison » ne se verifie
// qu'a l'oeil, sur une capture d'ecran que le lot ne produit pas -- et une
// application qui n'ecrirait plus que sur `stderr` resterait verte.
//
// ⚠ LE NOM *ET* LA RAISON SONT EXIGES, SEPAREMENT. Un statut reduit au nom du
// fichier laisse l'utilisateur sans cause ; reduit a la raison, sans coupable.
// Les deux sous-chaines sont cherchees une a une, et le verdict nomme celle qui
// manque.
//
// ⚠ AUCUN FICHIER DEMANDE REND LE VERDICT NON CONCLUANT, PAS BON : il n'y
// aurait alors aucun chargement dont le statut puisse rendre compte.
//
// ⚠ ET LE CAS QUI REUSSIT EST VERIFIE AUSSI : le statut doit etre VIDE. Sans
// cette moitie, un statut qui porterait un message d'erreur sur un chargement
// REUSSI passerait inapercu.
bool ReportLoadStatus (const std::string& requested, bool loaded, const std::string& failureName,
                       const std::string& failureReason, const std::string& status);

} // namespace app
} // namespace peles
