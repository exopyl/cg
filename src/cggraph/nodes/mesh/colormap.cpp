#include "colormap.h"

#include <memory>
#include <vector>

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
		d.typeName = "mesh.color.map";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.inputs.push_back ({ "champ", Types ().scalarField, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

ColormapNode::ColormapNode ()
{
}

const cggraph::NodeDesc &ColormapNode::GetDesc () const
{
	return Desc ();
}

bool ColormapNode::Compute (cggraph::EvalContext &, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;
	const ScalarField *field = in[1].Get<ScalarField> (Types ().scalarField);
	if (field == nullptr)
		return false;

	const unsigned int nv = input->GetNVertices ();
	if (nv == 0 || field->values.size () < nv)
		return false;

	// Neutralisation des sommets non definis -- voir l'en-tete. On les ramene au
	// MINIMUM des valeurs definies plutot que de compter sur le drapeau que le
	// corps ignore. Si aucun sommet n'est defini, il n'y a rien a colorier.
	std::vector<float> values (field->values.begin (), field->values.begin () + nv);
	bool any = false;
	float lo = 0.0f;
	for (unsigned int i = 0; i < nv; ++i)
		if (field->IsDefined (i))
		{
			if (!any || values[i] < lo) lo = values[i];
			any = true;
		}
	if (!any)
		return false;
	for (unsigned int i = 0; i < nv; ++i)
		if (!field->IsDefined (i))
			values[i] = lo;

	// Copie franche : le corps emballe ecrit dans le maillage -- couleurs, UV et
	// indices d'UV par face --, et l'entree est immuable.
	std::shared_ptr<Mesh> work = std::make_shared<Mesh> (*input);
	work->InitVertexColorsFromArray (values.data ());

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (work));
	return true;
}

} // namespace cggraph_nodes
