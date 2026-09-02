#include "svg_contours.h"

#include <memory>
#include <string>
#include <vector>

#include "../../../cgmesh/import_svg.h"
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
		d.typeName = "svg.contours";
		d.inputs.push_back ({ "chemin", Types ().path, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

} // namespace

SvgContoursNode::SvgContoursNode ()
{
	// `flattenTol` s'exprime dans les unites du RESULTAT et non du document :
	// l'importeur convertit, sans quoi la finesse des courbes dependrait de
	// l'echelle du fichier source.
	GetParams ().SetFloat ("flattenTol", 0.005f);
	GetParams ().SetBool ("centerAndFit", true);
	GetParams ().SetBool ("invertY", true);
	GetParams ().SetBool ("strokeToVolume", true);
	GetParams ().SetFloat ("strokeScale", 1.0f);
	GetParams ().SetFloat ("strokeWidthFallback", 1.0f);

	// `height` n'est PAS expose : c'est la profondeur d'extrusion, elle se regle
	// sur shape.extrude. L'exposer ici donnerait un curseur sans effet, puisque
	// svg_to_contours ne le lit pas.
}

const cggraph::NodeDesc &SvgContoursNode::GetDesc () const
{
	return Desc ();
}

bool SvgContoursNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                               cggraph::ValueList &out)
{
	(void)ctx;

	const std::string *path = in[0].Get<std::string> (Types ().path);
	if (path == nullptr || path->empty ())
		return false;

	SvgExtrudeOptions options;
	options.flattenTol = GetFloat (GetParams (), "flattenTol", 0.005f);
	options.centerAndFit = GetBool (GetParams (), "centerAndFit", true);
	options.invertY = GetBool (GetParams (), "invertY", true);
	options.strokeToVolume = GetBool (GetParams (), "strokeToVolume", true);
	options.strokeScale = GetFloat (GetParams (), "strokeScale", 1.0f);
	options.strokeWidthFallback = GetFloat (GetParams (), "strokeWidthFallback", 1.0f);

	std::shared_ptr<std::vector<ExtrudeContour>> contours =
		std::make_shared<std::vector<ExtrudeContour>> ();
	if (!svg_to_contours (*path, options, *contours))
		return false;

	out[0] = cggraph::Value::Make (Types ().extrudeContours, contours);
	return true;
}

} // namespace cggraph_nodes
