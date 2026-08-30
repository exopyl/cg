#include "icp_align.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/icp.h"
#include "../../../cgmesh/mesh.h"
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
		d.typeName = "mesh.align.icp";
		d.inputs.push_back ({ "source", Types ().mesh, false });
		d.inputs.push_back ({ "cible", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

IcpAlignNode::IcpAlignNode (int maxIterations, bool withScale)
{
	GetParams ().SetInt ("maxIterations", maxIterations);
	GetParams ().SetFloat ("convergenceEps", 1e-4f);
	GetParams ().SetBool ("withScale", withScale);
	GetParams ().SetFloat ("trimFraction", 0.0f);
}

const cggraph::NodeDesc &IcpAlignNode::GetDesc () const
{
	return Desc ();
}

bool IcpAlignNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> source = in[0].Share<Mesh> (Types ().mesh);
	const std::shared_ptr<const Mesh> target = in[1].Share<Mesh> (Types ().mesh);
	if (source == nullptr || target == nullptr)
		return false;
	if (source->GetNVertices () < 3 || target->GetNFaces () == 0)
		return false;

	ICPOptions options;
	options.maxIterations = GetInt (GetParams (), "maxIterations", 60);
	options.convergenceEps = GetFloat (GetParams (), "convergenceEps", 1e-4f);
	options.withScale = GetBool (GetParams (), "withScale", false);
	options.trimFraction = GetFloat (GetParams (), "trimFraction", 0.0f);

	// Copie profonde de la CIBLE : icp_align la prend par reference non const
	// (BVH::build l'exige) et le BVH garde un pointeur nu sur ses positions.
	Mesh targetCopy (*target);

	const std::vector<float> &sourcePoints = source->GetVertices ();
	const GraphContext graphContext = MakeGraphContext (ctx);
	const ICPResult result = icp_align (sourcePoints, targetCopy, options, &graphContext);

	// Copie profonde de la SOURCE : c'est elle qu'on rend, transformee. Une
	// seule copie, et c'est le maillage rendu -- pas un intermediaire.
	std::shared_ptr<Mesh> aligned = std::make_shared<Mesh> (*source);
	const unsigned int nv = aligned->GetNVertices ();
	std::vector<float> positions = aligned->GetVertices ();
	for (unsigned int i = 0; i < nv; ++i)
	{
		const float x = positions[3u * i];
		const float y = positions[3u * i + 1u];
		const float z = positions[3u * i + 2u];
		for (unsigned int k = 0; k < 3u; ++k)
			positions[3u * i + k] = result.scale * (result.R[3u * k] * x
			                                      + result.R[3u * k + 1u] * y
			                                      + result.R[3u * k + 2u] * z)
			                        + result.T[k];
	}
	aligned->SetVertices (nv, positions.data ());

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (aligned));
	return true;
}

} // namespace cggraph_nodes
