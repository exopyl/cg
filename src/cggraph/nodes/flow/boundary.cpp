#include "boundary.h"

#include "../value_types.h"

namespace cggraph_nodes
{
namespace flow
{

const char *const kInputTypeName = "flow.in";
const char *const kOutputTypeName = "flow.out";
const char *const kDefaultPortName = "element";

namespace
{

const cggraph::NodeDesc &InputDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = kInputTypeName;
		d.outputs.push_back ({ "element", Types ().mesh, false });
		return d;
	}();
	return desc;
}

const cggraph::NodeDesc &OutputDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = kOutputTypeName;
		d.inputs.push_back ({ "element", Types ().mesh, false });
		// Une sortie qui repete son entree. Elle existe pour que l'hote TIRE ce
		// noeud comme n'importe quel autre et lise sa valeur : sans elle, il
		// faudrait tirer l'amont du noeud de sortie, c'est-a-dire connaitre la
		// topologie du document au lieu de sa frontiere.
		d.outputs.push_back ({ "element", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

GraphInputNode::GraphInputNode ()
{
	// Ordonne les frontieres entre elles, donc les ports de l'hote. Semantique :
	// echanger deux slots echange deux ports, ce qui est un autre calcul.
	GetParams ().SetInt ("slot", 0);
	GetParams ().SetString ("nom", kDefaultPortName);

	// Semantic ET Internal, exactement comme `source.identity` d'un noeud
	// source : c'est lui qui distingue les iterations dans l'index du cache, et
	// personne ne le tape au clavier.
	GetParams ().SetString ("flow.binding", std::string (), cggraph::ParamRole::Semantic,
	                        cggraph::ParamVisibility::Internal);
}

const cggraph::NodeDesc &GraphInputNode::GetDesc () const
{
	return InputDesc ();
}

void GraphInputNode::Bind (const cggraph::Value &value, const std::string &key)
{
	m_bound = value;
	GetParams ().SetString ("flow.binding", key, cggraph::ParamRole::Semantic,
	                        cggraph::ParamVisibility::Internal);
}

bool GraphInputNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                              cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;

	// Rien de lie : le document est evalue hors d'un hote, ou l'hote a moins
	// d'entrees que le document n'a de frontieres. Echec plutot que valeur vide
	// -- une valeur vide se propagerait jusqu'a un consommateur qui la
	// diagnostiquerait a sa place, et le refus nommerait alors le mauvais noeud.
	if (m_bound.IsEmpty ())
		return false;

	out[0] = m_bound;
	return true;
}

GraphOutputNode::GraphOutputNode ()
{
	GetParams ().SetInt ("slot", 0);
	GetParams ().SetString ("nom", kDefaultPortName);
}

const cggraph::NodeDesc &GraphOutputNode::GetDesc () const
{
	return OutputDesc ();
}

bool GraphOutputNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                               cggraph::ValueList &out)
{
	(void)ctx;
	out[0] = in[0];
	return true;
}

} // namespace flow
} // namespace cggraph_nodes
