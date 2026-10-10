#pragma once

#include "input_event.h"
#include "viewport_rect.h"

#include <string>

namespace Rml
{
class Context;
}

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  L'ARBITRE : QUI, DE LA VUE 3D OU DE L'INTERFACE, RECOIT CE CLIC
// ---------------------------------------------------------------------------
//
// ⚠ RIEN DANS LA CHAINE glfw -> Context::ProcessMouse* NE DIT « CE CLIC ETAIT
// POUR LA 3D ». Tout remettre aux deux ferait tourner la camera quand on glisse
// une barre de defilement ; tout remettre a RmlUi rendrait la vue inerte.
//
// ⚠ L'ARBITRE EST Context::GetHoverElement(), COMPARE A L'ELEMENT DE LA VUE. Le
// test de survol n'est PAS reimplemente : le refaire en comparant le curseur aux
// bords du rectangle donnerait un SECOND CALCUL DE GEOMETRIE -- la duplication
// que la conception refuse -- et il se tromperait des qu'un panneau, une barre
// de defilement ou une infobulle recouvre la vue.
//
// ⚠ LE `bool` RENDU PAR ProcessMouse* N'EST PAS UTILISE. Sa documentation dit
// « true si la souris n'interagit avec aucun element » : « non consomme » et
// « aucun survol » y sont le meme booleen. Batir l'arbitrage dessus, c'est le
// batir sur une lecture incertaine.
class InputRouter
{
public:
	// `viewportElementId` est l'identifiant de l'element qui porte la vue 3D
	// dans le document. Le contexte peut etre nul : l'arbitre rend alors tout a
	// la vue, ce qui est le seul comportement utile sans interface.
	InputRouter (Rml::Context* context, const char* viewportElementId);

	// Rectangle de la vue, tel que l'image courante l'a LU apres Update().
	//
	// ⚠ IL EST REPOSE A CHAQUE IMAGE, ET C'EST INDISPENSABLE : la conversion
	// fenetre -> vue le soustrait. Un rectangle perime decalerait le picking de
	// la difference, et rien ne le signalerait.
	void SetViewportRect (const ViewportRect& rect);

	enum class Owner
	{
		// Aucun proprietaire : aucun survol connu -- avant le premier
		// deplacement, ou apres une sortie de fenetre.
		None,
		Ui,
		Viewport
	};

	struct Decision
	{
		Owner owner = Owner::None;

		// Vrai quand l'evenement a ete remis a RmlUi. ⚠ Un deplacement SANS
		// bouton est remis aux DEUX : sans cela le survol de RmlUi resterait
		// fige sur le dernier element touche, et l'arbitrage deviendrait
		// collant -- une fois entre dans la vue, on n'en sortirait plus.
		bool deliveredToUi = false;

		// Coordonnees dans le repere de la VUE. N'ont de sens que sous
		// `owner == Viewport`.
		//
		// ⚠ C'EST ICI, ET NULLE PART AILLEURS, QUE L'ORIGINE DU RECTANGLE EST
		// SOUSTRAITE. L'hote historique n'avait pas cette conversion a faire :
		// son canvas ETAIT la fenetre de rendu. L'oublier produit une orbite qui
		// saute et un picking decale de la largeur du panneau gauche, sans
		// qu'aucun pixel ne paraisse faux.
		int viewX = 0;
		int viewY = 0;
	};

	// ⚠ REND `Owner::None` TANT QUE `SetViewportRect` N'A PAS ETE APPELE, et
	// l'evenement n'est alors remis a personne. Sans cette garde, la conversion
	// fenetre -> vue soustrairait un rectangle nul : des coordonnees FENETRE
	// seraient presentees comme des coordonnees de VUE, c'est-a-dire exactement
	// le defaut que le lot mesure, et rien ne le signalerait.
	// ⚠ `Kind::Leave` ECHAPPE A LA GARDE : il ne porte aucune coordonnee, donc
	// il n'y a rien a convertir, et l'oubli de survol reste juste a tout moment.
	Decision Route (const MouseEvent& event);

	// ---------------------------------------------------------------------
	//  LE CLAVIER
	// ---------------------------------------------------------------------
	//
	// ⚠ IL VA TOUJOURS A L'INTERFACE, ET IL N'EST PAS ARBITRE. La vue 3D de ce
	// lot n'a aucun raccourci : lui disputer le clavier ferait dependre la
	// copie d'un texte de la position du curseur, ce qu'aucune interface ne
	// fait. Le destinataire reel est l'element qui a le FOCUS, et c'est RmlUi
	// qui le tient.
	//
	// Rend vrai si l'evenement a ete remis a RmlUi.
	bool RouteKey (const KeyEvent& event);

	// Vrai tant qu'au moins un bouton reste enfonce.
	bool Holding () const { return m_heldButtons != 0; }

	// BOUTON QUI A OUVERT LE GESTE COURANT -- celui dont l'enfoncement a fait
	// passer le masque de 0 a non nul.
	//
	// ⚠ IL EXISTE PARCE QUE LA CAMERA N'A QU'UN SEUL SLOT D'ETAT. Le trackball
	// ferme son glisser sur TOUT relachement, quel que soit le bouton : lui
	// remettre chaque enfoncement et chaque relachement ferait tuer par un
	// second bouton le geste ouvert par le premier. Le gel du proprietaire ferme
	// l'etat mixte ICI ; c'est cette valeur qui le ferme cote camera.
	//
	// ⚠ ELLE SURVIT AU DERNIER RELACHEMENT, ET C'EST INDISPENSABLE : le
	// relachement est arbitre APRES la mise a jour du masque, et c'est lui qui a
	// besoin de savoir quel bouton avait ouvert le geste. Elle n'est reecrite
	// qu'au prochain enfoncement ouvrant.
	MouseButton GestureButton () const { return m_gestureButton; }

	// Proprietaire courant. Sous capture, c'est celui qui a recu le PREMIER
	// enfoncement, et il ne change pas avant le dernier relachement.
	Owner CurrentOwner () const { return m_owner; }

	// Identifiant de l'element survole, pour le diagnostic. Chaine vide quand
	// l'element n'en porte pas ; "(aucun)" quand rien n'est survole.
	std::string HoverName () const;

private:
	// Rend Viewport quand l'element survole EST l'element de la vue ou l'un de
	// ses descendants.
	//
	// ⚠ LA REMONTEE DE LA CHAINE DE PARENTS N'EST PAS UN LUXE : RmlUi rend le
	// descendant le PLUS JEUNE sous le curseur. L'element de la vue est vide
	// aujourd'hui, mais une image de transition ou un texte de statut poses
	// dedans feraient survoler un enfant, et une comparaison directe rendrait
	// alors la vue inerte.
	Owner OwnerFromHover () const;

	Rml::Context* m_context = nullptr;
	std::string   m_viewportElementId;

	// ⚠ LE RECTANGLE A-T-IL ETE POSE ? Un rectangle par defaut est un rectangle
	// VALIDE en apparence -- origine (0,0) -- donc son absence ne se deduit pas
	// de son contenu.
	bool         m_hasRect = false;
	ViewportRect m_rect;

	// ⚠ MASQUE DE BOUTONS ENFONCES, ET C'EST LUI QUI PORTE LA REGLE DE CAPTURE.
	// Des qu'un bouton est enfonce, le proprietaire est FIGE jusqu'au
	// relachement du dernier bouton, quel que soit le survol entre-temps. Sans
	// cette regle, un orbite qui passe sous un panneau rendrait la main a
	// l'interface EN PLEIN GLISSER -- le defaut n°5 de debt_sinaia.md, que
	// l'hote historique presente faute d'appeler CaptureMouse.
	int         m_heldButtons  = 0;
	Owner       m_owner        = Owner::None;
	MouseButton m_gestureButton = MouseButton::Left;

	// ⚠ BOUTONS DONT LE GESTE A ETE ANNULE ET DONT LE RELACHEMENT RESTE A VENIR.
	// A la perte du focus, glfw appelle le rappel de focus AVANT de synthetiser
	// les relachements (`_glfwInputWindowFocus`, glfw 3.3.10) : l'annulation
	// arrive donc la premiere, et les relachements qui la suivent ne doivent
	// etre remis a personne. Sans ce masque, RmlUi recevrait un `ButtonUp` sans
	// `ButtonDown` correspondant, et la vue un relachement de geste deja clos.
	int m_cancelledButtons = 0;
};

} // namespace ui
} // namespace peles
