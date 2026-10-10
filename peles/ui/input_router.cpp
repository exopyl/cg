#include "input_router.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Types.h>

#include <cmath>

namespace peles
{
namespace ui
{

namespace
{

// Les trois modificateurs, traduits vers l'enumeration de RmlUi. La table est
// ici parce que c'est le seul endroit qui parle les deux langues.
int ToRmlModifiers (int modifiers)
{
	int state = 0;
	if (modifiers & modifier::kShift)
		state |= Rml::Input::KM_SHIFT;
	if (modifiers & modifier::kControl)
		state |= Rml::Input::KM_CTRL;
	if (modifiers & modifier::kAlt)
		state |= Rml::Input::KM_ALT;
	return state;
}

int ButtonBit (MouseButton button)
{
	return 1 << (int)button;
}

// LES TOUCHES ROUTEES, TRADUITES VERS L'ENUMERATION DE RmlUi.
//
// La table est exhaustive sur `ui::Key`, et le `switch` n'a pas de `default` :
// une entree ajoutee a l'enumeration sans traduction tombe sur le
// `return KI_UNKNOWN` qui suit, donc sur une touche que RmlUi ignore -- jamais
// sur une touche arbitraire.
//
// ⛔ MAIS AUCUN COMPILATEUR NE LE VERIFIE DANS CE DEPOT, et c'est a savoir avant
// de s'y fier. Le diagnostic qui le ferait -- `C4062` chez MSVC, `-Wswitch` chez
// GCC et Clang -- est de NIVEAU 4 chez l'un et attache a `-Wall` chez les
// autres ; la racine ne pose ni `/W4` ni `-Wall` hors `ENABLE_COVERAGE`
// (CMakeLists.txt:254). Ajouter une entree a `ui::Key` sans l'ajouter ici est
// donc SILENCIEUX : la touche est mise en file par la fenetre, traversee
// jusqu'ici, et perdue sans un mot.
Rml::Input::KeyIdentifier ToRmlKey (Key key)
{
	switch (key)
	{
	case Key::A:        return Rml::Input::KI_A;
	case Key::C:        return Rml::Input::KI_C;
	case Key::Left:     return Rml::Input::KI_LEFT;
	case Key::Right:    return Rml::Input::KI_RIGHT;
	case Key::Up:       return Rml::Input::KI_UP;
	case Key::Down:     return Rml::Input::KI_DOWN;
	case Key::Home:     return Rml::Input::KI_HOME;
	case Key::End:      return Rml::Input::KI_END;
	case Key::PageUp:   return Rml::Input::KI_PRIOR;
	case Key::PageDown: return Rml::Input::KI_NEXT;
	}
	return Rml::Input::KI_UNKNOWN;
}

} // namespace

InputRouter::InputRouter (Rml::Context* context, const char* viewportElementId)
    : m_context (context), m_viewportElementId (viewportElementId != nullptr ? viewportElementId
                                                                            : "")
{
}

void InputRouter::SetViewportRect (const ViewportRect& rect)
{
	m_rect    = rect;
	m_hasRect = true;
}

InputRouter::Owner InputRouter::OwnerFromHover () const
{
	if (m_context == nullptr)
		return Owner::Viewport;

	const Rml::Element* element = m_context->GetHoverElement ();
	if (element == nullptr)
		return Owner::None;

	for (; element != nullptr; element = element->GetParentNode ())
	{
		if (element->GetId () == m_viewportElementId)
			return Owner::Viewport;
	}
	return Owner::Ui;
}

std::string InputRouter::HoverName () const
{
	if (m_context == nullptr)
		return "(aucun contexte)";

	Rml::Element* element = m_context->GetHoverElement ();
	if (element == nullptr)
		return "(aucun)";

	// L'identifiant quand il y en a un, le nom de balise sinon : une ligne
	// engendree par `data-for` n'a pas d'identifiant, et « (sans id) » ne dirait
	// pas dans quel panneau le curseur se trouve.
	const Rml::String& id = element->GetId ();
	if (!id.empty ())
		return std::string ("#") + std::string (id);
	return std::string (element->GetTagName ());
}

InputRouter::Decision InputRouter::Route (const MouseEvent& event)
{
	Decision decision;

	if (event.kind == MouseEvent::Kind::Leave)
	{
		// ⚠ LA CAPTURE N'EST PAS ROMPUE. Un glisser qui sort du cadre est un
		// glisser qui continue : c'est exactement le cas que la regle de capture
		// existe pour couvrir. Seul le survol de RmlUi est oublie, sans quoi le
		// dernier element survole le resterait pour toujours.
		if (m_context != nullptr)
		{
			m_context->ProcessMouseLeave ();
			decision.deliveredToUi = true;
		}
		decision.owner = m_heldButtons != 0 ? m_owner : Owner::None;
		return decision;
	}

	if (event.kind == MouseEvent::Kind::Cancel)
	{
		// LE PROPRIETAIRE EST RENDU A L'APPELANT AVANT D'ETRE OUBLIE : c'est lui
		// qui doit annuler le geste de la vue, et seule cette valeur le lui dit.
		decision.owner = m_heldButtons != 0 ? m_owner : Owner::None;

		// ⚠ L'INTERFACE, ELLE, N'A PAS D'ANNULATION : RmlUi n'expose aucun
		// « le bouton n'est plus enfonce, oubliez le clic ». Un enfoncement
		// laisse sans relachement y laisse un element actif a jamais, donc on le
		// lui referme -- c'est le moins mauvais des deux etats.
		if (decision.owner == Owner::Ui && m_context != nullptr)
		{
			for (int button = 0; button < 3; ++button)
			{
				if ((m_heldButtons & (1 << button)) != 0)
					m_context->ProcessMouseButtonUp (button, ToRmlModifiers (event.modifiers));
			}
			decision.deliveredToUi = true;
		}

		m_cancelledButtons = m_heldButtons;
		m_heldButtons      = 0;
		m_owner            = Owner::None;
		return decision;
	}

	// AUCUNE COORDONNEE DE VUE SANS RECTANGLE. Voir la declaration.
	if (!m_hasRect)
		return decision;

	// ⚠ PAR LA FONCTION PARTAGEE, ET NON PAR UNE SOUSTRACTION ECRITE ICI. Le
	// vidage de rayon passe par la meme : un seul site porte la conversion, donc
	// un seul site suffit a la neutraliser pour eprouver le detecteur.
	float viewX = 0.f;
	float viewY = 0.f;
	WindowToView (m_rect, (float)event.windowX, (float)event.windowY, viewX, viewY);
	decision.viewX = (int)std::lround (viewX);
	decision.viewY = (int)std::lround (viewY);

	const int modifiers = ToRmlModifiers (event.modifiers);

	switch (event.kind)
	{
	case MouseEvent::Kind::Move:
	{
		// LE DEPLACEMENT VA AUX DEUX, TOUJOURS.
		//
		// ⚠ RmlUi LE RECOIT MEME SOUS CAPTURE DE LA VUE, et c'est voulu : il ne
		// peut rien en faire -- il n'a recu aucun enfoncement, donc il n'a ni
		// element actif ni glisser en cours -- mais son survol reste a jour.
		// Sans cela, GetHoverElement() se figerait sur le dernier element
		// touche et l'arbitrage deviendrait collant des la fin du glisser.
		//
		// ⚠ ET C'EST AUSSI CE QUI REND LA REGLE DE CAPTURE FALSIFIABLE : la
		// garde d'inutilite de --input-script exige que le survol de RmlUi ait
		// QUITTE la vue au deplacement sur le panneau. Sans remise, « vue pour
		// les trois » serait vrai sans que la capture ait rien eu a tenir.
		//
		// ⚠ DEFAUT PRODUIT ASSUME, ET IL EST VISIBLE : les regles `:hover` des
		// panneaux (peles.rcss) allument les lignes traversees PENDANT une
		// orbite. L'arbitrage entre ce defaut cosmetique et la perte du seul
		// detecteur d'arbitrage du lot est tranche en faveur du detecteur.
		if (m_context != nullptr)
		{
			m_context->ProcessMouseMove (event.windowX, event.windowY, modifiers);
			decision.deliveredToUi = true;
		}

		decision.owner = m_heldButtons != 0 ? m_owner : OwnerFromHover ();
		return decision;
	}

	case MouseEvent::Kind::Press:
	{
		// LE PROPRIETAIRE SE DECIDE AU PREMIER ENFONCEMENT, ET A LUI SEUL.
		//
		// ⚠ UN SECOND BOUTON ENFONCE PENDANT UN GLISSER NE REARBITRE RIEN. Le
		// laisser rearbitrer produirait un etat mixte -- un bouton a la vue, un
		// autre a l'interface -- dont le relachement irait au mauvais
		// destinataire, et l'un des deux garderait un bouton enfonce a jamais.
		if (m_heldButtons == 0)
		{
			m_owner = OwnerFromHover ();
			// LE BOUTON QUI OUVRE LE GESTE, MEMORISE ICI ET NULLE PART AILLEURS.
			// Voir GestureButton() : c'est lui qui ferme l'etat mixte du cote de
			// la camera, dont le trackball n'a qu'un seul slot.
			m_gestureButton = event.button;
		}

		m_heldButtons |= ButtonBit (event.button);
		decision.owner = m_owner;

		// ⚠ UN ENFONCEMENT SANS SURVOL CONNU VA A L'INTERFACE. C'est le cas d'un
		// clic recu avant tout deplacement : rien ne permet d'affirmer qu'il
		// visait la vue, et le donner a la camera ferait sauter l'orientation
		// depuis un point que personne n'a designe.
		if (decision.owner != Owner::Viewport && m_context != nullptr)
		{
			m_context->ProcessMouseButtonDown ((int)event.button, modifiers);
			decision.deliveredToUi = true;
		}
		return decision;
	}

	case MouseEvent::Kind::Release:
	{
		// LE RELACHEMENT D'UN GESTE DEJA ANNULE N'EST REMIS A PERSONNE.
		if ((m_cancelledButtons & ButtonBit (event.button)) != 0)
		{
			m_cancelledButtons &= ~ButtonBit (event.button);
			return decision;
		}

		decision.owner = m_owner;

		if (decision.owner != Owner::Viewport && m_context != nullptr)
		{
			m_context->ProcessMouseButtonUp ((int)event.button, modifiers);
			decision.deliveredToUi = true;
		}

		m_heldButtons &= ~ButtonBit (event.button);
		if (m_heldButtons == 0)
			m_owner = Owner::None;
		return decision;
	}

	case MouseEvent::Kind::Wheel:
	{
		decision.owner = m_heldButtons != 0 ? m_owner : OwnerFromHover ();

		if (decision.owner != Owner::Viewport && m_context != nullptr)
		{
			// ⚠ SIGNE INVERSE. Un cran positif est ici un cran vers l'avant,
			// qui RAPPROCHE l'oeil ; le `wheel_delta` de RmlUi positif fait
			// DESCENDRE le document. C'est le seul endroit du projet ou les deux
			// conventions se rencontrent.
			//
			// ⚠ ET LES CRANS SONT BORNES ICI AUSSI. La borne appartient a
			// l'evenement, pas au destinataire : voir ui/input_event.h.
			m_context->ProcessMouseWheel (Rml::Vector2f (0.f, -ClampNotches (event.notches)),
			                              modifiers);
			decision.deliveredToUi = true;
		}
		return decision;
	}

	// Traites avant ce switch : ils n'ont ni coordonnee a convertir ni
	// proprietaire a arbitrer au survol.
	case MouseEvent::Kind::Leave:
	case MouseEvent::Kind::Cancel: break;
	}

	return decision;
}

bool InputRouter::RouteKey (const KeyEvent& event)
{
	if (m_context == nullptr)
		return false;

	const Rml::Input::KeyIdentifier key = ToRmlKey (event.key);
	if (key == Rml::Input::KI_UNKNOWN)
		return false;

	// ⚠ LE `bool` RENDU PAR ProcessKey* N'EST PAS UTILISE, pour la meme raison
	// que celui de ProcessMouse* : « non consomme » et « aucun destinataire » y
	// sont le meme booleen.
	const int modifiers = ToRmlModifiers (event.modifiers);
	if (event.pressed)
		m_context->ProcessKeyDown (key, modifiers);
	else
		m_context->ProcessKeyUp (key, modifiers);
	return true;
}

} // namespace ui
} // namespace peles
