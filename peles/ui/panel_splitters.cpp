#include "panel_splitters.h"

#include <RmlUi/Core/Box.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/ID.h>
#include <RmlUi/Core/Property.h>
#include <RmlUi/Core/Types.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace peles
{
namespace ui
{

namespace
{

// LES QUATRE EVENEMENTS ECOUTES SUR CHAQUE SEPARATEUR. `dragend` ne pose aucune
// etendue -- le dernier `drag` l'a deja fait -- il rend seulement le panneau a
// Enforce, qui ne borne pas un glisser en cours.
const std::array<Rml::EventId, 4> kEvents = { Rml::EventId::Dragstart, Rml::EventId::Drag,
	                                          Rml::EventId::Dragend, Rml::EventId::Dblclick };

int RoundPixels (float v)
{
	return (int)std::lround ((double)v);
}

// Etendue d'un element le long d'un axe, boite de bordure comprise -- la meme
// boite que celle que ReadViewportRect lit pour la vue.
int ExtentAlong (Rml::Element* element, PanelSplitters::Axis axis)
{
	if (element == nullptr)
		return 0;
	const Rml::Vector2f size = element->GetBox ().GetSize (Rml::BoxArea::Border);
	return RoundPixels (axis == PanelSplitters::Axis::Horizontal ? size.x : size.y);
}

} // namespace

// ---------------------------------------------------------------------------
//  L'ECOUTEUR
// ---------------------------------------------------------------------------
//
// ⚠ UN SEUL OBJET POUR LES TROIS SEPARATEURS : il retrouve le sien par l'element
// courant de l'evenement. L'etendue est posee DANS le rappel : SetProperty ne
// fait que salir le style, la mise en page est refaite par le prochain
// Update(), et aucun appel GL n'est emis -- ce qui distingue ce rappel des
// `data-event-*` de l'arbre et de la liste, dont l'effet charge un fichier.
class PanelSplitters::Listener final : public Rml::EventListener
{
public:
	explicit Listener (PanelSplitters& owner) : m_owner (owner) {}

	void ProcessEvent (Rml::Event& event) override
	{
		const Rml::Element* const current = event.GetCurrentElement ();
		for (int i = 0; i < kCount; ++i)
		{
			if (m_owner.m_slots[i].splitter != current)
				continue;

			const Axis axis    = Specs ()[i].axis;
			const int  pointer = event.GetParameter<int> (
			    axis == Axis::Horizontal ? "mouse_x" : "mouse_y", 0);

			switch (event.GetId ())
			{
			case Rml::EventId::Dragstart: m_owner.OnDragStart (i, pointer); break;
			case Rml::EventId::Drag:      m_owner.OnDrag (i, pointer); break;
			case Rml::EventId::Dragend:   m_owner.m_slots[i].dragging = false; break;
			case Rml::EventId::Dblclick:  m_owner.OnReset (i); break;
			default:                      break;
			}
			return;
		}
	}

private:
	PanelSplitters& m_owner;
};

// ---------------------------------------------------------------------------

const std::array<PanelSplitters::Spec, PanelSplitters::kCount>& PanelSplitters::Specs ()
{
	static const std::array<Spec, kCount> specs = { {
		{ "left", "split-left", "panel-left", Axis::Horizontal, core::SplitterSide::PanelBefore },
		{ "right", "split-right", "panel-right", Axis::Horizontal, core::SplitterSide::PanelAfter },
		{ "bottom", "split-bottom", "panel-bottom", Axis::Vertical,
		  core::SplitterSide::PanelAfter },
	} };
	return specs;
}

int PanelSplitters::IndexOf (const char* name)
{
	if (name == nullptr)
		return -1;
	for (int i = 0; i < kCount; ++i)
	{
		if (std::strcmp (Specs ()[i].name, name) == 0)
			return i;
	}
	return -1;
}

PanelSplitters::PanelSplitters () : m_listener (std::make_unique<Listener> (*this)) {}

PanelSplitters::~PanelSplitters ()
{
	for (Slot& slot : m_slots)
	{
		if (slot.splitter == nullptr)
			continue;
		for (const Rml::EventId id : kEvents)
			slot.splitter->RemoveEventListener (id, m_listener.get ());
	}
}

int PanelSplitters::Attach (Rml::Element* root, const char* viewportElementId)
{
	if (root == nullptr || viewportElementId == nullptr)
		return 0;

	m_viewport = root->GetElementById (viewportElementId);
	if (m_viewport == nullptr)
		return 0;

	int attached = 0;
	for (int i = 0; i < kCount; ++i)
	{
		Slot& slot = m_slots[i];
		if (slot.splitter != nullptr)
			continue;

		Rml::Element* const splitter = root->GetElementById (Specs ()[i].splitterId);
		Rml::Element* const panel    = root->GetElementById (Specs ()[i].panelId);
		if (splitter == nullptr || panel == nullptr)
			continue;

		slot.splitter = splitter;
		slot.panel    = panel;
		for (const Rml::EventId id : kEvents)
			splitter->AddEventListener (id, m_listener.get ());
		++attached;
	}
	return attached;
}

int PanelSplitters::PanelExtent (int index) const
{
	return ExtentAlong (m_slots[index].panel, Specs ()[index].axis);
}

int PanelSplitters::ViewportExtent (Axis axis) const
{
	return ExtentAlong (m_viewport, axis);
}

int PanelSplitters::Overflow (Rml::Element* container, Axis axis)
{
	if (container == nullptr)
		return 0;
	const Rml::Vector2f content  = container->GetBox ().GetSize (Rml::BoxArea::Content);
	int                 occupied = 0;
	const int           children = container->GetNumChildren ();
	for (int i = 0; i < children; ++i)
	{
		Rml::Element* const child = container->GetChild (i);
		if (child == nullptr)
			continue;
		const Rml::Vector2f size = child->GetBox ().GetSize (Rml::BoxArea::Margin);
		occupied += RoundPixels (axis == Axis::Horizontal ? size.x : size.y);
	}
	const int available = RoundPixels (axis == Axis::Horizontal ? content.x : content.y);
	// Le calcul du debordement vit dans core::SharedAxisExtent : panneaux et vue
	// a zero, il rend exactement le debordement a retrancher.
	return -core::SharedAxisExtent (0, 0, occupied, available);
}

int PanelSplitters::SharedExtent (int index) const
{
	// Les boites de marge des separateurs valent zero : leurs marges negatives
	// compensent leur etendue, ils ne comptent pas dans le debordement.
	const Axis axis = Specs ()[index].axis;
	return PanelExtent (index) + ViewportExtent (axis) -
	       Overflow (m_slots[index].panel->GetParentNode (), axis);
}

void PanelSplitters::ApplyExtent (int index, int extent)
{
	m_slots[index].panel->SetProperty (Rml::PropertyId::FlexBasis,
	                                   Rml::Property ((float)extent, Rml::Unit::PX));
}

void PanelSplitters::OnDragStart (int index, int pointer)
{
	// ⚠ LE POINTEUR DE `dragstart` EST CELUI DE L'ENFONCEMENT, PAS DU PREMIER
	// DEPLACEMENT : RmlUi y remet la position precedente
	// (`Context::UpdateHoverChain`). Le deplacement qui a declenche le glisser
	// est donc compte par le `drag` qui suit immediatement.
	Slot& slot = m_slots[index];

	// ⚠ UN GLISSER QUI SUIT UNE REMISE A ZERO DANS LE MEME LOT D'EVENEMENTS EST
	// IGNORE. Le double-clic a retire la propriete en ligne, mais la mise en
	// page n'a pas encore ete refaite : l'etendue lue ici serait celle d'AVANT
	// la remise, et le premier `drag` la reposerait -- une secousse d'un pixel
	// apres le double-clic annulerait la remise. Le geste suivant, apres le
	// prochain Update(), est traite normalement.
	if (slot.resetPending)
		return;

	slot.dragging     = true;
	slot.startPointer = pointer;
	slot.startExtent  = PanelExtent (index);
	// L'ETENDUE PARTAGEE EST FIGEE AU DEBUT DU GESTE. Plusieurs `drag` peuvent
	// arriver entre deux mises en page : relire la geometrie a chacun lirait
	// celle d'AVANT le precedent, et le panneau avancerait par saccades.
	slot.sharedExtent = SharedExtent (index);
}

void PanelSplitters::OnDrag (int index, int pointer)
{
	Slot& slot = m_slots[index];
	if (!slot.dragging)
		return;

	++m_dragEvents;
	const int wanted = core::DraggedPanelExtent (slot.startExtent, pointer - slot.startPointer,
	                                             Specs ()[index].side);
	slot.requested = core::ClampPanelExtent (wanted, slot.sharedExtent);
	ApplyExtent (index, slot.requested);
}

void PanelSplitters::OnReset (int index)
{
	Slot& slot     = m_slots[index];
	slot.requested    = -1;
	slot.dragging     = false;
	slot.resetPending = true;
	slot.panel->RemoveProperty (Rml::PropertyId::FlexBasis);
	++m_resets;
}

bool PanelSplitters::Enforce ()
{
	// La mise en page vient d'etre refaite : une remise a zero est maintenant
	// visible dans la geometrie lue.
	for (Slot& slot : m_slots)
		slot.resetPending = false;

	if (m_viewport == nullptr)
		return false;

	bool changed = false;
	for (const Axis axis : { Axis::Horizontal, Axis::Vertical })
		changed = EnforceAxis (axis) || changed;
	return changed;
}

bool PanelSplitters::EnforceAxis (Axis axis)
{
	// ⚠ TOUS LES PANNEAUX DE L'AXE EN UNE PASSE, et c'est la regle, pas une
	// optimisation. Les panneaux gauche et droit partagent la MEME vue : borner
	// chacun contre « lui-meme plus la vue » ignorerait la marge de l'autre, et
	// le premier descendrait sous son minimum pendant que le second garde tout.
	// L'arbitrage -- qui cede, dans quel ordre -- est core::ClampAxisExtents.
	std::vector<int>             indices;
	std::vector<core::AxisPanel> panels;
	bool                         adjustable = false;
	int                          sum        = 0;
	for (int i = 0; i < kCount; ++i)
	{
		const Slot& slot = m_slots[i];
		if (Specs ()[i].axis != axis || slot.panel == nullptr)
			continue;

		// Un glisser en cours se borne lui-meme ; le borner ici aussi le
		// ferait lutter contre l'etendue qu'il vient de poser.
		core::AxisPanel panel;
		panel.extent    = PanelExtent (i);
		panel.requested = slot.dragging ? -1 : slot.requested;
		adjustable      = adjustable || panel.requested >= 0;
		sum += panel.extent;
		indices.push_back (i);
		panels.push_back (panel);
	}
	if (!adjustable)
		return false;

	// Tous les panneaux d'un axe ont le meme conteneur (#row, ou le corps).
	const int shared = sum + ViewportExtent (axis) -
	                   Overflow (m_slots[indices.front ()].panel->GetParentNode (), axis);
	const std::vector<int> targets = core::ClampAxisExtents (panels, shared);

	bool changed = false;
	for (std::size_t k = 0; k < indices.size (); ++k)
	{
		if (panels[k].requested < 0 || targets[k] == panels[k].extent)
			continue;
		ApplyExtent (indices[k], targets[k]);
		changed = true;
	}
	return changed;
}

int PanelSplitters::RequestedExtent (int index) const
{
	if (index < 0 || index >= kCount)
		return -1;
	return m_slots[index].requested;
}

} // namespace ui
} // namespace peles
