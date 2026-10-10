#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace peles
{
namespace platform
{

// ---------------------------------------------------------------------------
//  L'ENCODAGE NEUTRE DES ENTREES
// ---------------------------------------------------------------------------
//
// ⚠ AUCUN TYPE glfw NE TRAVERSE CET EN-TETE, pas meme un `int` dont la valeur
// serait celle de glfw sans le dire. Les constantes ci-dessous sont celles de
// peles ; window_glfw.cpp traduit celles de glfw vers elles.
//
// ⚠ ET LEUR CORRESPONDANCE AVEC L'ENUMERATION DE ui/input_event.h EST VERIFIEE
// PAR LE COMPILATEUR, dans app/application.cpp -- la seule unite qui voie les
// deux en-tetes. peles_platform ne lie pas peles_ui et ne le liera pas : les
// deux vocabulaires sont donc necessairement distincts, et c'est l'executable
// qui porte la preuve qu'ils coincident.
//
// SEIZE `static_assert` : trois boutons, trois modificateurs, et les DIX
// touches ci-dessous. La verification porte sur les trois groupes, aucun
// excepte.
inline constexpr int kMouseButtonLeft   = 0;
inline constexpr int kMouseButtonRight  = 1;
inline constexpr int kMouseButtonMiddle = 2;

inline constexpr int kModShift   = 1 << 0;
inline constexpr int kModControl = 1 << 1;
inline constexpr int kModAlt     = 1 << 2;

// LES TOUCHES QUE LA FENETRE TRADUIT, ET SEULEMENT CELLES-LA.
//
// ⚠ LA LISTE EST COURTE PARCE QU'ELLE EST CELLE D'UN CHAMP DE TEXTE EN LECTURE
// SEULE : deplacer le curseur, etendre la selection, copier. Rien qui modifie.
// La traduction depuis glfw vit dans window_glfw.cpp ; une touche absente de
// cette liste n'est pas mise en file, donc n'atteint aucun destinataire.
//
// ⚠ LE PONT VERS `ui::Key` EST UN CAST NU (app/application_create.cpp) : ce sont
// les dix `static_assert` d'app/application.cpp, et eux seuls, qui empechent une
// renumerotation d'un cote de router Fin pour Debut.
inline constexpr int kKeyA        = 0;
inline constexpr int kKeyC        = 1;
inline constexpr int kKeyLeft     = 2;
inline constexpr int kKeyRight    = 3;
inline constexpr int kKeyUp       = 4;
inline constexpr int kKeyDown     = 5;
inline constexpr int kKeyHome     = 6;
inline constexpr int kKeyEnd      = 7;
inline constexpr int kKeyPageUp   = 8;
inline constexpr int kKeyPageDown = 9;

// CURSEURS DU SYSTEME que la fenetre sait poser. Neutres, comme les constantes
// ci-dessus : window_glfw.cpp les traduit vers les curseurs standard de glfw.
enum class CursorShape
{
	Arrow,
	ResizeHorizontal,
	ResizeVertical,
	Text,
	Hand,
	Crosshair
};

// Nombre de formes : dimensionne la table des curseurs de window_glfw.cpp, et un
// `static_assert` y verifie qu'il suit l'enumeration.
inline constexpr int kCursorShapeCount = 6;

// Rappels d'entree. Ils sont appeles DEPUIS glfwPollEvents, donc dans la phase 1
// d'une image : ce qu'ils font doit se limiter a enregistrer, jamais a dessiner.
struct InputCallbacks
{
	// Position du curseur, en pixels du TAMPON DE DESSIN, origine en haut a
	// gauche. ⚠ Ce n'est pas la position en coordonnees de fenetre que glfw rend
	// : sur un ecran a facteur d'echelle, les deux different, et c'est le tampon
	// que la geometrie de RmlUi mesure.
	std::function<void (int x, int y)> move;

	std::function<void (int button, bool pressed, int modifiers)> button;

	// Crans de molette, positifs vers l'avant.
	std::function<void (float notches, int modifiers)> wheel;

	// Le curseur a quitte le cadre.
	//
	// ⚠ CE RAPPEL NE ROMPT PAS LA CAPTURE DE L'ARBITRE, et ce n'est pas une
	// omission. Sous Win32, glfw capture le pointeur au premier bouton enfonce
	// et le relache au dernier relachement (`SetCapture` / `ReleaseCapture` dans
	// son backend Win32) : les deplacements continuent donc d'arriver HORS du
	// cadre pendant un glisser, avec des coordonnees qui sortent des bornes. Un
	// glisser qui sort de la fenetre est un glisser qui continue -- c'est
	// exactement le cas que la regle de capture existe pour couvrir -- et ce
	// rappel ne sert qu'a faire oublier son survol a RmlUi.
	//
	// ⚠ COMME LES TROIS AUTRES, IL DOIT SE CONTENTER D'ENREGISTRER. Agir ici le
	// ferait courir AVANT les evenements deja en file : un deplacement recu dans
	// le meme `glfwPollEvents` retablirait aussitot le survol qu'on vient de
	// faire oublier.
	std::function<void ()> leave;

	// Touche enfoncee ou relachee, parmi les kKey* ci-dessus.
	std::function<void (int key, bool pressed, int modifiers)> key;

	// LA FENETRE A PERDU LE FOCUS.
	//
	// ⚠ CE RAPPEL COURT AVANT LES RELACHEMENTS SYNTHETIQUES, ET C'EST CE QUI LE
	// REND UTILISABLE. glfw remet les boutons enfonces a `GLFW_RELEASE` quand la
	// fenetre perd le focus -- aucune capture ne reste donc coincee -- mais ces
	// relachements portent la DERNIERE position du curseur, donc un deplacement
	// nul, donc un CLIC : un Alt+Tab pendant un appui immobile selectionnerait
	// le modele sous le curseur. `_glfwInputWindowFocus` appelle le rappel de
	// focus AVANT la boucle de synthese, donc une annulation mise en file ici
	// precede les relachements et les rend sans destinataire.
	std::function<void ()> focusLost;
};

// Fenetre et contexte OpenGL, par glfw.
//
// ⚠ AUCUN EN-TETE glfw ICI, ET C'EST DELIBERE. L'unite qui pilote les huit
// phases inclut gl_wrapper.h, donc glad/gl.h, qui REDEFINIT CHAQUE ENTREE GL
// PAR MACRO. Laisser fuir <GLFW/glfw3.h> dans cet en-tete y ferait entrer un
// second en-tete GL par la porte de derriere -- glfw3.h en tire un par defaut.
// Tout ce dont l'hote a besoin passe par les methodes ci-dessous.
class Window
{
public:
	// Rend nullptr et renseigne `error` si la fenetre ou le contexte ne peuvent
	// pas etre obtenus. Le contexte est rendu COURANT avant le retour.
	static std::unique_ptr<Window> Create (int width, int height, const std::string& title,
	                                       std::string& error);

	~Window ();

	Window (const Window&)            = delete;
	Window& operator= (const Window&) = delete;

	bool ShouldClose () const;
	void PollEvents () const;
	void SwapBuffers () const;

	void GetFramebufferSize (int& width, int& height) const;

	// Taille de la zone cliente, en coordonnees d'ecran. Sert a l'instrument
	// --resize ; le produit ne dimensionne jamais sa fenetre lui-meme.
	void SetSize (int width, int height) const;

	// RAPPEL DE RAFRAICHISSEMENT : la fenetre demande a etre redessinee.
	// Une fonction vide le retire.
	//
	// ⚠ C'EST LE SEUL RAPPEL QUI DESSINE, ET IL EXISTE POUR CELA. Sous Win32,
	// tirer un bord de la fenetre fait entrer le systeme dans une boucle modale
	// de redimensionnement : glfwPollEvents ne rend la main qu'au relachement,
	// donc la boucle d'images est suspendue pendant tout le geste. Seuls
	// WM_SIZE et WM_PAINT sont encore distribues, et WM_PAINT aboutit ici.
	// Sans ce rappel, la fenetre montre une image etiree ou non peinte jusqu'au
	// relachement.
	//
	// ⚠ IL EST APPELE DEPUIS glfwPollEvents : la fonction remise ne doit pas
	// rappeler PollEvents, que glfw interdit depuis un rappel.
	void SetRefreshCallback (std::function<void ()> refresh);

	// ICONE DE LA FENETRE. Les pixels sont en RGBA8 entrelace, premiere ligne en
	// HAUT, et ils ne sont lus que pendant l'appel.
	//
	// ⚠ AUCUN TYPE D'IMAGE NE TRAVERSE CET EN-TETE, pas plus qu'un type glfw :
	// peles_platform ne lie ni cgimg ni cgre. Le decodage appartient a
	// l'appelant, qui les a deja.
	//
	// ⚠ PLUSIEURS TAILLES PLUTOT QU'UNE SEULE : Win32 demande une petite icone
	// pour la barre de titre et une grande pour la barre des taches, et glfw
	// choisit dans le tableau celle dont la taille est la plus proche. Avec une
	// seule image, l'une des deux est un redimensionnement.
	struct IconImage
	{
		int                  width  = 0;
		int                  height = 0;
		const unsigned char* rgba   = nullptr;
	};

	// Rend false si aucune image utilisable n'est fournie. Un echec est
	// COSMETIQUE : la fenetre garde l'icone par defaut du systeme.
	bool SetIcon (const std::vector<IconImage>& images) const;

	// Installe les rappels d'entree. Ils sont appeles depuis PollEvents.
	void SetInputCallbacks (InputCallbacks callbacks);

	// POSE LE CURSEUR QUI CORRESPOND A CE NOM RCSS -- celui que RmlUi remet a
	// SystemInterface::SetMouseCursor. Un nom inconnu, ou vide, rend le curseur
	// par defaut : RmlUi demande la chaine vide en quittant un element qui
	// portait un curseur.
	//
	// ⚠ LES CURSEURS SONT CREES A LA PREMIERE DEMANDE ET DETRUITS AVEC LA
	// FENETRE. glfw 3.3 ne connait que six formes standard ; `ew-resize` et
	// `col-resize` partagent la double fleche horizontale, `ns-resize` et
	// `row-resize` la verticale.
	void SetCursor (const std::string& cssName);

	// Forme posee par le dernier SetCursor, pour l'instrument.
	CursorShape CurrentCursor () const;

	// ---------------------------------------------------------------------
	//  LE PRESSE-PAPIER DU SYSTEME, VU DEPUIS LA FENETRE
	// ---------------------------------------------------------------------
	//
	// ⚠ CE N'EST PAS UN DOUBLON DE SystemInterface::SetClipboardText, ET LES
	// DEUX CHEMINS DOIVENT RESTER DISTINCTS. Celui-ci sert a l'instrument : il
	// SEME une sentinelle avant le geste et RELIT apres. Passer par l'interface
	// de RmlUi pour les deux rendrait la mesure tautologique -- elle serait verte
	// meme si l'interface ecrivait dans une chaine membre.
	void        SetClipboardText (const std::string& text) const;
	std::string ClipboardText () const;

	// 0 desactive la synchronisation verticale. ⚠ Une mesure de temps par image
	// faite avec la synchronisation ACTIVE ne mesure que la periode de l'ecran.
	void SetSwapInterval (int interval) const;

	// Horloge monotone de glfw, en secondes depuis l'initialisation.
	static double ElapsedSeconds ();

private:
	Window ();

	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace platform
} // namespace peles
