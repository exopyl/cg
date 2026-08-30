#include "convex_hull.h"

#include <cstdlib>
#include <memory>
#include <vector>

#include "../../../cgmesh/chull.h"
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
		d.typeName = "mesh.hull.convex";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

ConvexHullNode::ConvexHullNode ()
{
}

const cggraph::NodeDesc &ConvexHullNode::GetDesc () const
{
	return Desc ();
}

bool ConvexHullNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                              cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	const unsigned int nv = input->GetNVertices ();
	// Quatre points non coplanaires au minimum : en dessous, double_triangle ()
	// ne peut pas amorcer et ne le dit que par un printf.
	if (nv < 4)
		return false;

	// SEULE copie de cet adaptateur, et elle ne porte que les positions : le
	// constructeur de Chull3D demande un `float *` non const, qu'il se contente
	// de lire. Copier le maillage entier n'apporterait rien -- rien d'autre que
	// les positions n'entre dans l'enveloppe.
	std::vector<float> points = input->GetVertices ();

	const GraphContext graphContext = MakeGraphContext (ctx);
	Chull3D hull (points.data (), static_cast<int> (nv));
	hull.compute (&graphContext);

	float *hullVertices = nullptr;
	int nHullVertices = 0;
	int *hullFaces = nullptr;
	int nHullFaces = 0;
	// Echec ou annulation, meme code de retour : c'est l'evaluateur qui nomme le
	// refus, il interroge le contexte AVANT ce code.
	if (hull.get_convex_hull (&hullVertices, &nHullVertices, &hullFaces, &nHullFaces) != 0)
		return false;
	if (hullVertices == nullptr || hullFaces == nullptr || nHullVertices < 4 || nHullFaces < 4)
	{
		std::free (hullVertices);
		std::free (hullFaces);
		return false;
	}

	std::vector<unsigned int> faces (3u * static_cast<std::size_t> (nHullFaces));
	for (std::size_t i = 0; i < faces.size (); ++i)
		faces[i] = static_cast<unsigned int> (hullFaces[i]);

	std::shared_ptr<Mesh> result = std::make_shared<Mesh> ();
	result->SetVertices (static_cast<unsigned int> (nHullVertices), hullVertices);
	result->SetFaces (static_cast<unsigned int> (nHullFaces), 3, faces.data ());

	// malloc cote cgmesh, free ici : c'est le contrat de get_convex_hull, et il
	// n'est ecrit nulle part ailleurs que dans son corps.
	std::free (hullVertices);
	std::free (hullFaces);

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (result));
	return true;
}

} // namespace cggraph_nodes
