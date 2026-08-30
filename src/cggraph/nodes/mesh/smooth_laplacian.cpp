#include "smooth_laplacian.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/mesh_half_edge.h"
#include "../../../cgmesh/smoothing_laplacian.h"
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
		d.typeName = "mesh.smooth.laplacian";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.inputs.push_back ({ "zone", Types ().selection, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

SmoothLaplacianNode::SmoothLaplacianNode (int iterations, float lambda)
{
	GetParams ().SetInt ("iterations", iterations);
	GetParams ().SetFloat ("lambda", lambda);
}

const cggraph::NodeDesc &SmoothLaplacianNode::GetDesc () const
{
	return Desc ();
}

bool SmoothLaplacianNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                   cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	// L'entree optionnelle non alimentee arrive VIDE : c'est le noeud qui decide
	// ce qu'une zone absente veut dire, et ici elle veut dire « tout le
	// maillage ».
	const Selection *zone = in.size () > 1 ? in[1].Get<Selection> (Types ().selection) : nullptr;

	const int iterations = GetInt (GetParams (), "iterations", 1);
	const float lambda = GetFloat (GetParams (), "lambda", 1.0f);

	// Le maillage enveloppe est une COPIE profonde du notre : l'entree est lue,
	// jamais ecrite, et aucun handle mutable n'en est tire.
	Mesh_half_edge model (input.get ());
	Mesh *work = model.m_pMesh;
	const unsigned int nv = work->GetNVertices ();
	if (nv == 0)
		return false;

	std::vector<char> smoothed (nv, zone == nullptr ? 1 : 0);
	if (zone != nullptr)
		for (unsigned int index : zone->vertices)
			if (index < nv)
				smoothed[index] = 1;

	const GraphContext graphContext = MakeGraphContext (ctx);
	MeshAlgoSmoothingLaplacian algo;

	for (int iteration = 0; iteration < iterations; ++iteration)
	{
		const std::vector<float> before = work->GetVertices ();
		// Echec ou annulation, c'est le meme code de retour : un calcul qui n'a
		// pas produit ce qu'on lui demandait rend `false`, et c'est l'evaluateur
		// qui nomme le refus -- il interroge le contexte AVANT ce code.
		if (!algo.Apply (&model, &graphContext))
			return false;

		std::vector<float> blended = work->GetVertices ();
		for (unsigned int i = 0; i < nv; ++i)
		{
			const float weight = smoothed[i] != 0 ? lambda : 0.0f;
			for (unsigned int k = 0; k < 3; ++k)
			{
				const std::size_t at = 3u * static_cast<std::size_t> (i) + k;
				blended[at] = before[at] + weight * (blended[at] - before[at]);
			}
		}
		work->SetVertices (nv, blended.data ());

		ctx.Progress (static_cast<float> (iteration + 1) / static_cast<float> (iterations),
		              "mesh.smooth.laplacian");
	}

	// CESSION, pas copie : `work` est le maillage de travail de l'enveloppe, deja
	// une copie profonde de l'entree, et l'enveloppe s'apprete a le detruire.
	// `std::make_shared<Mesh> (*work)` en ferait une SECONDE copie profonde --
	// 88 Mio sur 2 M de triangles -- pour rien. Cf. la regle de node_support.h.
	//
	// ⚠ Apres release (), `work` et `model` ne designent plus le maillage rendu.
	// Cette ligne doit rester la DERNIERE a les toucher.
	out[0] = cggraph::Value::Make (Types ().mesh, std::shared_ptr<Mesh> (model.release ()));
	return true;
}

} // namespace cggraph_nodes
