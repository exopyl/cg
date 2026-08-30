#include "graph.h"

namespace cggraph
{

namespace
{

const PortDesc *FindPort (const std::vector<PortDesc> &ports, PortIdx index)
{
	if (index >= ports.size ())
		return nullptr;
	return &ports[index];
}

} // namespace

NodeId Graph::AddNode (std::unique_ptr<Node> node)
{
	if (node == nullptr)
		return kInvalidNodeId;

	Slot slot;
	slot.id = m_nextId++;
	slot.node = std::move (node);
	const NodeId id = slot.id;
	m_nodes.push_back (std::move (slot));
	return id;
}

NodeId Graph::AddNodeWithId (NodeId id, std::unique_ptr<Node> node)
{
	if (node == nullptr || id == kInvalidNodeId || FindSlot (id) != nullptr)
		return kInvalidNodeId;

	Slot slot;
	slot.id = id;
	slot.node = std::move (node);
	m_nodes.push_back (std::move (slot));

	// Le compteur repart AU-DELA de l'identifiant repris : un noeud ajoute
	// apres une relecture ne doit pas recevoir un identifiant deja pose.
	if (id >= m_nextId)
		m_nextId = id + 1;
	return id;
}

RemoveStatus Graph::RemoveNode (NodeId id)
{
	std::size_t index = m_nodes.size ();
	for (std::size_t i = 0; i < m_nodes.size (); ++i)
		if (m_nodes[i].id == id)
		{
			index = i;
			break;
		}
	if (index == m_nodes.size ())
		return RemoveStatus::UnknownNode;

	// Parcours a rebours : effacer par l'arriere laisse intacts les indices qui
	// restent a examiner.
	for (std::size_t i = m_links.size (); i > 0; --i)
	{
		const Link &link = m_links[i - 1];
		if (link.from == id || link.to == id)
			m_links.erase (m_links.begin () + static_cast<std::ptrdiff_t> (i - 1));
	}

	// m_nextId n'est pas touche : la place liberee ne se redistribue pas.
	m_nodes.erase (m_nodes.begin () + static_cast<std::ptrdiff_t> (index));
	return RemoveStatus::Ok;
}

const Node *Graph::FindNode (NodeId id) const
{
	for (const Slot &slot : m_nodes)
		if (slot.id == id)
			return slot.node.get ();
	return nullptr;
}

Node *Graph::FindNode (NodeId id)
{
	const Graph *self = this;
	return const_cast<Node *> (self->FindNode (id));
}

std::vector<NodeId> Graph::GetNodeIds () const
{
	std::vector<NodeId> ids;
	ids.reserve (m_nodes.size ());
	for (const Slot &slot : m_nodes)
		ids.push_back (slot.id);
	return ids;
}

const Graph::Slot *Graph::FindSlot (NodeId id) const
{
	for (const Slot &slot : m_nodes)
		if (slot.id == id)
			return &slot;
	return nullptr;
}

bool Graph::SetNodePosition (NodeId id, float x, float y)
{
	for (Slot &slot : m_nodes)
		if (slot.id == id)
		{
			slot.x = x;
			slot.y = y;
			return true;
		}
	return false;
}

bool Graph::GetNodePosition (NodeId id, float &x, float &y) const
{
	const Slot *slot = FindSlot (id);
	if (slot == nullptr)
		return false;
	x = slot->x;
	y = slot->y;
	return true;
}

bool Graph::SetNodeSubgraph (NodeId id, const std::string &reference)
{
	for (Slot &slot : m_nodes)
		if (slot.id == id)
		{
			slot.subgraph = reference;
			// Le noeud APPREND sa reference. Le graphe ne l'interprete pas -- il
			// ne connait aucun type de noeud --, il la transmet a celui qui le
			// peut.
			if (slot.node != nullptr)
				slot.node->SetSubgraphReference (reference);
			return true;
		}
	return false;
}

const std::string &Graph::GetNodeSubgraph (NodeId id) const
{
	static const std::string empty;
	const Slot *slot = FindSlot (id);
	return slot == nullptr ? empty : slot->subgraph;
}

const Link *Graph::FindInputLink (NodeId to, PortIdx toPort) const
{
	for (const Link &link : m_links)
		if (link.to == to && link.toPort == toPort)
			return &link;
	return nullptr;
}

std::vector<NodeId> Graph::GetUpstream (NodeId id) const
{
	std::vector<NodeId> sources;
	const Node *node = FindNode (id);
	if (node == nullptr)
		return sources;

	const NodeDesc &desc = node->GetDesc ();
	sources.reserve (desc.inputs.size ());
	for (std::size_t i = 0; i < desc.inputs.size (); ++i)
	{
		const Link *link = FindInputLink (id, static_cast<PortIdx> (i));
		sources.push_back (link ? link->from : kInvalidNodeId);
	}
	return sources;
}

bool Graph::IsDownstream (NodeId origin, NodeId target) const
{
	if (origin == target)
		return true;

	std::vector<NodeId> stack (1, origin);
	std::vector<NodeId> seen;
	while (!stack.empty ())
	{
		const NodeId current = stack.back ();
		stack.pop_back ();

		bool alreadySeen = false;
		for (NodeId id : seen)
			if (id == current)
			{
				alreadySeen = true;
				break;
			}
		if (alreadySeen)
			continue;
		seen.push_back (current);

		for (const Link &link : m_links)
		{
			if (link.from != current)
				continue;
			if (link.to == target)
				return true;
			stack.push_back (link.to);
		}
	}
	return false;
}

ConnectStatus Graph::Connect (NodeId from, PortIdx fromPort, NodeId to, PortIdx toPort)
{
	const Node *source = FindNode (from);
	const Node *sink = FindNode (to);
	if (source == nullptr || sink == nullptr)
		return ConnectStatus::UnknownNode;

	const PortDesc *out = FindPort (source->GetDesc ().outputs, fromPort);
	const PortDesc *in = FindPort (sink->GetDesc ().inputs, toPort);
	if (out == nullptr || in == nullptr)
		return ConnectStatus::UnknownPort;

	// Egalite de descripteurs, donc de pointeurs : le registre refuse les
	// doublons de nom, deux descripteurs distincts sont deux types distincts.
	// Un port sans type est mal forme et ne se branche sur rien.
	if (out->type == nullptr || in->type == nullptr || out->type != in->type)
		return ConnectStatus::TypeMismatch;

	if (FindInputLink (to, toPort) != nullptr)
		return ConnectStatus::InputAlreadyConnected;

	// Le lien fermerait une boucle si l'aval remonte deja jusqu'a l'amont ; le
	// cas from == to est couvert par la meme comparaison.
	if (IsDownstream (to, from))
		return ConnectStatus::Cycle;

	Link link;
	link.from = from;
	link.fromPort = fromPort;
	link.to = to;
	link.toPort = toPort;
	m_links.push_back (link);
	return ConnectStatus::Ok;
}

bool Graph::Disconnect (NodeId to, PortIdx toPort)
{
	for (std::size_t i = 0; i < m_links.size (); ++i)
	{
		if (m_links[i].to != to || m_links[i].toPort != toPort)
			continue;
		m_links.erase (m_links.begin () + static_cast<std::ptrdiff_t> (i));
		return true;
	}
	return false;
}

} // namespace cggraph
