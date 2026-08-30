#include "subgraph.h"

#include "../value_types.h"

namespace cggraph_nodes
{
namespace flow
{

SubgraphNode::SubgraphNode ()
	: SubgraphHostNode ("flow.subgraph")
{
}

void SubgraphNode::PublishPorts (const SubgraphInstance &instance)
{
	m_desc.inputs.clear ();
	m_desc.outputs.clear ();
	for (const Boundary &boundary : instance.inputs)
		m_desc.inputs.push_back ({ boundary.name, Types ().mesh, false });
	for (const Boundary &boundary : instance.outputs)
		m_desc.outputs.push_back ({ boundary.name, Types ().mesh, false });
}

bool SubgraphNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	LoadStatus status = LoadStatus::Ok;
	std::string detail;
	const std::unique_ptr<SubgraphInstance> instance = Open (status, detail);
	if (instance == nullptr)
	{
		NoteResult (cggraph::EvalStatus::ComputeFailed, std::string (ToString (status)) + " " + detail);
		return false;
	}

	// SON graphe, SON evaluateur, sur la pile. L'evaluateur exterieur est occupe
	// -- c'est lui qui nous appelle --, et le reutiliser rendrait Busy.
	cggraph::Evaluator evaluator (instance->graph);

	std::vector<cggraph::Value> produced;
	const PassResult pass =
		RunPass (*instance, evaluator, ctx, "0", in, produced, SinksRequested ());
	NoteResult (pass.status, pass.detail);
	NotePass ();
	NoteWritten (CollectWritten (*instance));
	if (!pass.IsOk ())
		return false;

	for (std::size_t i = 0; i < out.size () && i < produced.size (); ++i)
		out[i] = produced[i];
	return true;
}

} // namespace flow
} // namespace cggraph_nodes
