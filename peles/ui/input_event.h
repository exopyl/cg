#pragma once

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  L'EVENEMENT DE SOURIS, DANS UN VOCABULAIRE QUI N'EST CELUI D'AUCUN TIERS
// ---------------------------------------------------------------------------
//
// ⚠ NI glfw NI RmlUi ICI, ET C'EST UNE CONTRAINTE DE CIBLES. peles_ui ne lie
// pas glfw ; peles_platform ne lie pas peles_ui. Le seul type que les deux
// puissent partager est celui que l'executable leur traduit -- et c'est lui qui
// porte les `static_assert` reliant les deux encodages, la ou les deux en-tetes
// sont visibles ensemble.
//
// ⚠ LES COORDONNEES SONT CELLES DE LA FENETRE. La conversion vers le repere de
// la VUE est le travail de l'arbitre, qui seul connait le rectangle lu a cette
// image.

// Boutons. La valeur numerique est celle de RmlUi -- gauche 0, droit 1, milieu
// 2 -- et elle se trouve etre aussi celle de glfw et celle du trackball.
//
// ⚠ UNE SEULE DES DEUX FRONTIERES VERIFIE LA COINCIDENCE, ET C'EST A SAVOIR.
// La frontiere `platform -> ui` est tenue par les seize `static_assert` de
// app/application.cpp -- trois boutons, trois modificateurs, dix touches --
// seule unite qui voie les deux en-tetes. La frontiere
// `ui -> RmlUi`, elle, ne peut pas l'etre au meme endroit : le `(int)
// event.button` remis a Context::ProcessMouseButton* est ecrit dans
// ui/input_router.cpp, et peles_ui n'inclut aucun en-tete d'entree de RmlUi --
// Rml::Input::KeyIdentifier n'enumere pas les boutons de souris, qui ne sont
// que des entiers dans son interface. Cette seconde frontiere repose donc sur
// la documentation de RmlUi, pas sur une verification du compilateur.
enum class MouseButton
{
	Left   = 0,
	Right  = 1,
	Middle = 2
};

// Modificateurs de clavier. Seul `kAlt` porte un geste dans ce lot -- Alt +
// clic gauche pose le pivot -- mais les trois sont transportes : RmlUi les
// attend tous les trois, et n'en passer qu'un ferait diverger le comportement
// de ses controles de celui du reste du systeme.
namespace modifier
{
inline constexpr int kShift   = 1 << 0;
inline constexpr int kControl = 1 << 1;
inline constexpr int kAlt     = 1 << 2;
} // namespace modifier

struct MouseEvent
{
	// ⚠ `Leave` PASSE PAR LA MEME FILE QUE LES QUATRE AUTRES, et c'est une
	// condition de correction, pas une uniformite de facade. Traitee hors file,
	// une sortie de cadre recue dans le MEME glfwPollEvents qu'un deplacement
	// s'appliquerait AVANT lui : RmlUi oublierait son survol, puis le
	// deplacement le retablirait sur l'element interieur, et la ligne resterait
	// en surbrillance curseur hors fenetre.
	//
	// Les coordonnees ne sont pas renseignees pour ce genre : le systeme n'en
	// donne aucune avec la sortie.
	//
	// ⚠ `Cancel` N'EST PAS UN `Release`, ET LES CONFONDRE EST LE DEFAUT QU'IL
	// FERME. A la perte du focus, glfw synthetise un relachement par bouton
	// enfonce -- donc aucune capture ne reste coincee -- mais ce relachement
	// porte la DERNIERE position du curseur : le deplacement est nul, il passe
	// sous le seuil clic/glisser, et un Alt+Tab pendant un appui immobile
	// SELECTIONNE le modele sous le curseur. Un geste que l'utilisateur n'a pas
	// termine doit etre ANNULE, pas conclu.
	//
	// Comme `Leave`, il ne porte aucune coordonnee : le systeme n'en donne pas.
	enum class Kind
	{
		Move,
		Press,
		Release,
		Wheel,
		Leave,
		Cancel
	};

	Kind        kind   = Kind::Move;
	MouseButton button = MouseButton::Left;

	// Repere FENETRE, origine en haut a gauche, pixels du tampon de dessin.
	int windowX = 0;
	int windowY = 0;

	// Crans de molette, POSITIF VERS L'AVANT -- molette poussee loin de
	// l'utilisateur. C'est le signe de glfw, celui de l'hote historique et celui
	// qu'attend `Ctrackball::zoom_step`, ou un cran positif RAPPROCHE l'oeil :
	// le geste de la vue 3D se porte donc sans changement de signe.
	//
	// ⚠ RmlUi PREND L'OPPOSE : son `wheel_delta` positif fait descendre le
	// document. La negation vit dans l'arbitre, au seul endroit ou l'evenement
	// passe a RmlUi.
	float notches = 0.f;

	int modifiers = 0;
};

// BORNE DU NOMBRE DE CRANS PAR EVENEMENT DE MOLETTE.
//
// Une molette haute resolution envoie des fractions de cran, et un pilote
// exotique peut en envoyer beaucoup d'un coup : sans borne, un seul evenement
// traverserait la scene.
//
// ⚠ LA BORNE EST POSEE AVANT TOUT USAGE, DONC POUR LES DEUX DESTINATAIRES. La
// borner sur le seul chemin camera laisserait RmlUi defiler d'un coup sur le
// meme evenement -- et c'est bien AVANT tout usage que l'hote historique la
// posait (sinaia/wxOpenGLCanvas.cpp, OnMouseWheel).
inline constexpr float kMaxWheelNotches = 5.f;

inline float ClampNotches (float notches)
{
	if (notches > kMaxWheelNotches)
		return kMaxWheelNotches;
	if (notches < -kMaxWheelNotches)
		return -kMaxWheelNotches;
	return notches;
}

// ---------------------------------------------------------------------------
//  LES TOUCHES ROUTEES, ET SEULEMENT CELLES-LA
// ---------------------------------------------------------------------------
//
// ⚠ CETTE ENUMERATION EST VOLONTAIREMENT INCOMPLETE, ET C'EST CE QUI TIENT LE
// CARACTERE NON MODIFIABLE DE LA LIGNE « Path ». RmlUi 6.3 n'implemente pas
// l'attribut `readonly` -- seul `disabled` existe, et il retire le focus, donc
// la selection avec lui. La seule forme de lecture seule atteignable est donc
// de ne router QUE ce qu'un champ en lecture seule a besoin de recevoir :
// deplacement du curseur, extension de la selection, copie. Ni saisie de
// caractere -- `Context::ProcessTextInput` n'est appele nulle part -- ni
// effacement, ni coupe, ni collage.
//
// ⚠ AJOUTER UNE ENTREE ICI, C'EST RENDRE LA LIGNE MODIFIABLE. `KI_X`, `KI_V`,
// `KI_BACK` et `KI_DELETE` en sont absents pour cette raison, pas par oubli.
//
// Les valeurs sont celles de peles ; la traduction vers `Rml::Input::
// KeyIdentifier` vit dans ui/input_router.cpp, et celle depuis glfw dans
// platform/window_glfw.cpp.
enum class Key
{
	A,
	C,
	Left,
	Right,
	Up,
	Down,
	Home,
	End,
	PageUp,
	PageDown
};

struct KeyEvent
{
	Key  key       = Key::A;
	bool pressed   = true;
	int  modifiers = 0;
};

} // namespace ui
} // namespace peles
