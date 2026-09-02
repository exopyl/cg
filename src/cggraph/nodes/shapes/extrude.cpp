#include "extrude.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/extrude_contours.h"
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
		d.typeName = "shape.extrude";
		d.inputs.push_back ({ "contours", Types ().extrudeContours, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

ExtrudeNode::ExtrudeNode ()
{
	GetParams ().SetFloat ("depth", 0.2f);
}

const cggraph::NodeDesc &ExtrudeNode::GetDesc () const
{
	return Desc ();
}

bool ExtrudeNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                           cggraph::ValueList &out)
{
	(void)ctx;

	const std::vector<ExtrudeContour> *contours =
		in[0].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	if (contours == nullptr || contours->empty ())
		return false;

	ExtrudeAppendOptions options;
	options.zBottom = 0.0f;
	options.zTop = GetFloat (GetParams (), "depth", 0.2f);

	// NonZero, et PAS de renormalisation d'orientation. Les deux producteurs de
	// contours du catalogue -- texte et SVG -- rendent des regions sorties de
	// Clipper2, qui oriente deja exterieurs et contre-formes en sens opposes.
	// Reorienter d'apres l'aire signee reboucherait les contre-formes : les
	// creux d'un « o » et d'un « 8 » se rempliraient.
	options.winding = ExtrudeWinding::NonZero;
	options.normalizeOrientation = false;

	ExtrudedMeshBuilder builder;
	builder.Append (*contours, options);
	if (builder.Empty ())
		return false;

	std::shared_ptr<Mesh> mesh (builder.Build ());
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, mesh);
	return true;
}

} // namespace cggraph_nodes
