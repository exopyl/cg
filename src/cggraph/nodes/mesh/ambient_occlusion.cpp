#include "ambient_occlusion.h"

#include <cstdlib>
#include <memory>
#include <vector>

#include "../../../cgmesh/ambient_occlusion.h"
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
		d.typeName = "mesh.ambient_occlusion";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "champ", Types ().scalarField, false });
		return d;
	}();
	return desc;
}

} // namespace

AmbientOcclusionNode::AmbientOcclusionNode (int passes)
{
	GetParams ().SetInt ("passes", passes);
}

const cggraph::NodeDesc &AmbientOcclusionNode::GetDesc () const
{
	return Desc ();
}

bool AmbientOcclusionNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                    cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	const unsigned int nv = input->GetNVertices ();
	if (nv == 0 || input->GetNFaces () == 0)
		return false;

	int passes = GetInt (GetParams (), "passes", 1);
	if (passes < 1)
		passes = 1;

	// Copie profonde : Init prend un `Mesh *` non const et Evaluate y recalcule
	// les normales. Le maillage de travail ne sort pas d'ici -- la sortie est un
	// champ --, il vit donc en pile plutot qu'en shared_ptr.
	Mesh work (*input);

	const GraphContext graphContext = MakeGraphContext (ctx);
	MeshAlgoAmbientOcclusion algo;
	if (!algo.Init (&work))
		return false;

	// Echec ou annulation, meme code de retour : l'evaluateur nomme le refus.
	float *occlusion = algo.Evaluate (passes, &graphContext);
	if (occlusion == nullptr)
		return false;

	std::shared_ptr<ScalarField> field = std::make_shared<ScalarField> ();
	field->values.assign (occlusion, occlusion + nv);
	// malloc cote cgmesh, free ici : contrat de Evaluate.
	std::free (occlusion);

	// Un sommet d'aire nulle n'est pas « non occulte », il est indetermine. Le
	// corps ne fait pas cette distinction ; l'adaptateur la porte, et c'est ce
	// que `defined` sert a dire.
	field->defined.assign (nv, (char)1);
	std::vector<float> areas (nv, 0.0f);
	for (unsigned int f = 0; f < work.GetNFaces (); ++f)
	{
		auto face = work.FaceAt (f);
		if (!face)
			continue;
		const unsigned int n = face->GetNVertices ();
		if (n == 0)
			continue;
		const float share = static_cast<float> (work.GetFaceArea (f)) / static_cast<float> (n);
		for (unsigned int k = 0; k < n; ++k)
		{
			const unsigned int v = face->GetVertex (k);
			if (v < nv)
				areas[v] += share;
		}
	}
	for (unsigned int i = 0; i < nv; ++i)
		if (areas[i] < 1e-7f)
			field->defined[i] = 0;

	out[0] = cggraph::Value::Make (Types ().scalarField, std::move (field));
	return true;
}

} // namespace cggraph_nodes
