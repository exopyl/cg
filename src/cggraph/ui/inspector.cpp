#include "inspector.h"

namespace cggraph_ui
{

void Inspector::Clear ()
{
	m_node = cggraph::kInvalidNodeId;
	m_typeName.clear ();
	m_label.clear ();
	m_caveat = nullptr;
	m_readiness = cggraph::NodeReadiness::UnknownNode;
	m_inputs.clear ();
	m_outputs.clear ();
	m_params.clear ();
}

void Inspector::Build (const Palette &palette, cggraph::Graph &graph, cggraph::NodeId id)
{
	Clear ();

	cggraph::Node *node = graph.FindNode (id);
	if (node == nullptr)
		return;

	m_node = id;

	const cggraph::NodeDesc &desc = node->GetDesc ();
	m_typeName = desc.typeName;

	const PaletteItem *item = palette.Find (m_typeName);
	m_label = item != nullptr ? item->label : m_typeName;
	m_caveat = item != nullptr ? item->caveat : nullptr;

	// L'etat des entrees vient de la validation, jamais d'une seconde lecture
	// des liens : deux lectures divergeraient le jour ou l'une des deux change.
	const cggraph::NodeValidation validation = cggraph::ValidateNode (graph, id);
	m_readiness = validation.readiness;

	for (const cggraph::InputStatus &input : validation.inputs)
	{
		PortField field;
		field.port = input.port;
		field.name = input.name;
		field.type = input.type;
		field.state = input.state;
		m_inputs.push_back (field);
	}

	for (std::size_t i = 0; i < desc.outputs.size (); ++i)
	{
		PortField field;
		field.port = static_cast<cggraph::PortIdx> (i);
		field.name = desc.outputs[i].name;
		field.type = desc.outputs[i].type;
		m_outputs.push_back (field);
	}

	// Ordre de declaration des parametres, c'est-a-dire l'ordre dans lequel le
	// constructeur du noeud les a poses : l'auteur du noeud decide de ce que
	// l'utilisateur lit en premier.
	cggraph::ParamSet &params = node->GetParams ();
	for (const cggraph::ParamEntry &entry : params.GetEntries ())
	{
		// Un parametre interne n'est pas un parametre grise : il n'a pas de
		// champ du tout. Le publier en lecture seule laisserait croire qu'il est
		// a regler, alors que c'est le noeud qui l'ecrit.
		if (entry.visibility != cggraph::ParamVisibility::Public)
			continue;

		cggraph::ParamValue *value = params.Find (entry.name);
		if (value == nullptr)
			continue;

		ParamField field;
		field.name = entry.name;
		field.kind = entry.value.kind;
		field.type = entry.value.type;
		field.role = entry.role;
		field.value = value;
		m_params.push_back (field);
	}
}

} // namespace cggraph_ui
