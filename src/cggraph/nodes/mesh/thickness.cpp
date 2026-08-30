#include "thickness.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/thickness.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "mesh.thickness";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "champ", Types ().scalarField, false });
		return d;
	}();
	return desc;
}

} // namespace

ThicknessNode::ThicknessNode (int numRays, float coneHalfAngleDeg, int smoothIterations)
{
	GetParams ().SetInt ("numRays", numRays);
	GetParams ().SetFloat ("coneHalfAngleDeg", coneHalfAngleDeg);
	GetParams ().SetInt ("smoothIterations", smoothIterations);
}

const cggraph::NodeDesc &ThicknessNode::GetDesc () const
{
	return Desc ();
}

bool ThicknessNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	// Copie profonde : le corps recalcule les normales et la boite englobante
	// du maillage qu'on lui donne. Le maillage de travail ne sort pas d'ici --
	// la sortie est un champ --, il vit donc en pile.
	Mesh work (*input);

	std::shared_ptr<ScalarField> field = std::make_shared<ScalarField> ();
	const GraphContext graphContext = MakeGraphContext (ctx);

	// Echec ou annulation, meme code de retour : l'evaluateur nomme le refus.
	if (!MeshAlgoThickness::ComputeShapeDiameter (
	        work, field->values, field->defined,
	        GetInt (GetParams (), "numRays", 16),
	        GetFloat (GetParams (), "coneHalfAngleDeg", 60.0f),
	        GetInt (GetParams (), "smoothIterations", 1),
	        &graphContext))
		return false;

	out[0] = cggraph::Value::Make (Types ().scalarField, std::move (field));
	return true;
}

} // namespace cggraph_nodes
