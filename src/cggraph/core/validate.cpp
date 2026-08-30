#include "validate.h"

#include <set>

namespace cggraph
{

std::size_t NodeValidation::GetMissingRequiredCount () const
{
	std::size_t count = 0;
	for (const InputStatus &input : inputs)
		if (input.state == PortState::MissingRequired)
			++count;
	return count;
}

std::size_t BranchValidation::GetMissingRequiredCount () const
{
	std::size_t count = 0;
	for (const NodeValidation &node : nodes)
		count += node.GetMissingRequiredCount ();
	return count;
}

NodeValidation ValidateNode (const Graph &graph, NodeId id)
{
	NodeValidation validation;
	validation.node = id;

	const Node *node = graph.FindNode (id);
	if (node == nullptr)
		return validation;

	validation.readiness = NodeReadiness::Ready;

	const NodeDesc &desc = node->GetDesc ();
	for (std::size_t i = 0; i < desc.inputs.size (); ++i)
	{
		const PortDesc &port = desc.inputs[i];

		InputStatus status;
		status.port = static_cast<PortIdx> (i);
		status.name = port.name;
		status.type = port.type;

		if (graph.FindInputLink (id, status.port) != nullptr)
			status.state = PortState::Connected;
		else if (port.optional)
			status.state = PortState::MissingOptional;
		else
		{
			status.state = PortState::MissingRequired;
			validation.readiness = NodeReadiness::MissingRequiredInput;
		}

		validation.inputs.push_back (status);
	}

	return validation;
}

namespace
{

void Collect (const Graph &graph, NodeId id, std::set<NodeId> &seen, BranchValidation &out)
{
	if (!seen.insert (id).second)
		return;

	// Amont d'abord : la racine se lit en dernier, comme dans l'ordre de calcul.
	for (NodeId up : graph.GetUpstream (id))
		if (up != kInvalidNodeId)
			Collect (graph, up, seen, out);

	out.nodes.push_back (ValidateNode (graph, id));
}

} // namespace

BranchValidation ValidateBranch (const Graph &graph, NodeId id)
{
	BranchValidation validation;

	std::set<NodeId> seen;
	Collect (graph, id, seen, validation);

	validation.readiness = NodeReadiness::Ready;
	for (const NodeValidation &node : validation.nodes)
		if (node.readiness != NodeReadiness::Ready)
		{
			// Un noeud inconnu prime : la branche n'est pas « incomplete », elle
			// n'est pas lisible.
			validation.readiness = node.readiness == NodeReadiness::UnknownNode
			                           ? NodeReadiness::UnknownNode
			                           : NodeReadiness::MissingRequiredInput;
			if (validation.readiness == NodeReadiness::UnknownNode)
				break;
		}

	return validation;
}

} // namespace cggraph
