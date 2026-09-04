#include "boolean2d.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/contour_ops.h"
#include "../../../cgmesh/extrude_contours.h"
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
		d.typeName = "shape.boolean2d";
		// NOMMES, et non « entree 1 » / « entree 2 » : l'ordre porte un sens pour
		// la difference, et un port anonyme le cacherait.
		d.inputs.push_back ({ "matiere (A)", Types ().extrudeContours, false });
		d.inputs.push_back ({ "outil (B)", Types ().extrudeContours, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

} // namespace

Boolean2dNode::Boolean2dNode ()
{
	// 0 = reunion, 1 = difference (A moins B), 2 = intersection.
	GetParams ().SetInt ("op", 1);
}

const cggraph::NodeDesc &Boolean2dNode::GetDesc () const
{
	return Desc ();
}

bool Boolean2dNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	(void)ctx;

	const std::vector<ExtrudeContour> *a =
		in[0].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	const std::vector<ExtrudeContour> *b =
		in[1].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	if (a == nullptr || b == nullptr)
		return false;

	// Bornage, comme partout ou ParamSet ne sait pas borner : hors intervalle, le
	// reglage ne designe aucune operation, et retomber en silence sur la premiere
	// masquerait la faute.
	int op = GetInt (GetParams (), "op", 1);
	if (op < 0) op = 0;
	if (op > 2) op = 2;

	auto result = std::make_shared<std::vector<ExtrudeContour>> (
		  (op == 0) ? unionContours (*a, *b)
		: (op == 2) ? intersectionContours (*a, *b)
		            : differenceContours (*a, *b));

	// UN RESULTAT VIDE EST UN RESULTAT, pas une panne : deux formes disjointes
	// n'ont pas d'intersection, et un emporte-piece plus grand que sa matiere ne
	// laisse rien. Mais ce n'est pas une region extrudable, et le laisser passer
	// ferait echouer l'extrudeur plus loin sans pouvoir dire d'ou cela vient. On
	// refuse ICI, ou la cause est encore lisible.
	if (result->empty ())
		return false;

	out[0] = cggraph::Value::Make (Types ().extrudeContours, result);
	return true;
}

} // namespace cggraph_nodes
