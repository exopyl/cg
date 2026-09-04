#include "contour_ops.h"

#include <algorithm>
#include <cmath>
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

const cggraph::NodeDesc &OffsetDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "shape.contours.offset";
		d.inputs.push_back ({ "contours", Types ().extrudeContours, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

const cggraph::NodeDesc &PlateDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "shape.contours.plate";
		d.inputs.push_back ({ "contours", Types ().extrudeContours, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

} // namespace

// --- decalage ---------------------------------------------------------------

ContourOffsetNode::ContourOffsetNode ()
{
	GetParams ().SetFloat ("delta", 0.0f);
	// 0 = arrondi, 1 = onglet, 2 = chanfrein. Meme vocabulaire que Clipper2 et
	// que `stroke-linejoin` de SVG, qui expriment la meme chose.
	GetParams ().SetInt ("join", 0);
	// Sans effet hors onglet : c'est la limite au-dela de laquelle une pointe est
	// tronquee. La valeur de Clipper2 par defaut.
	GetParams ().SetFloat ("miterLimit", 2.0f);
}

void ContourOffsetNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "pieces", (double)m_pieces.load () });
}

const cggraph::NodeDesc &ContourOffsetNode::GetDesc () const
{
	return OffsetDesc ();
}

bool ContourOffsetNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                 cggraph::ValueList &out)
{
	(void)ctx;

	const std::vector<ExtrudeContour> *contours =
		in[0].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	if (contours == nullptr || contours->empty ())
		return false;

	int join = GetInt (GetParams (), "join", 0);
	if (join < 0) join = 0;
	if (join > 2) join = 2;
	const StrokeJoin jt = (join == 1) ? StrokeJoin::Miter
	                    : (join == 2) ? StrokeJoin::Bevel
	                                  : StrokeJoin::Round;

	int pieces = 0;
	std::shared_ptr<std::vector<ExtrudeContour>> result =
		std::make_shared<std::vector<ExtrudeContour>> (
			offsetContours (*contours,
			                GetFloat (GetParams (), "delta", 0.0f), jt,
			                GetFloat (GetParams (), "miterLimit", 2.0f),
			                &pieces));

	// Un retrecissement qui a tout consomme rend un jeu VIDE. Ce n'est pas une
	// panne du noeud, mais ce n'est pas une region extrudable non plus : on
	// refuse, plutot que de laisser l'extrudeur echouer sans dire pourquoi.
	if (result->empty ())
	{
		m_pieces.store (0);
		return false;
	}

	m_pieces.store ((unsigned int)(pieces > 0 ? pieces : 0));
	out[0] = cggraph::Value::Make (Types ().extrudeContours, result);
	return true;
}

// --- plaque rectangulaire ---------------------------------------------------

ContourPlateNode::ContourPlateNode ()
{
	GetParams ().SetFloat ("margin", 0.0f);
	GetParams ().SetFloat ("cornerRadius", 0.0f);
}

const cggraph::NodeDesc &ContourPlateNode::GetDesc () const
{
	return PlateDesc ();
}

bool ContourPlateNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                cggraph::ValueList &out)
{
	(void)ctx;

	const std::vector<ExtrudeContour> *contours =
		in[0].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	if (contours == nullptr || contours->empty ())
		return false;

	float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
	if (!contoursBBox (*contours, x0, y0, x1, y1))
		return false;

	const float margin = std::max (0.0f, GetFloat (GetParams (), "margin", 0.0f));
	const float radius = std::max (0.0f, GetFloat (GetParams (), "cornerRadius", 0.0f));

	std::vector<Vector2f> rect = roundedRectContour (x0 - margin, y0 - margin,
	                                                 x1 + margin, y1 + margin, radius);
	if (rect.size () < 3)
		return false;

	// Sens du plus GRAND contour d'entree : cf. la note d'orientation de
	// l'en-tete. Une plaque tracee a l'envers des lettres les soustrairait.
	float widest = 0.f;
	for (const ExtrudeContour &c : *contours)
	{
		const float a = contourSignedArea (c.pts);
		if (std::fabs (a) > std::fabs (widest)) widest = a;
	}
	const float want = (widest >= 0.f) ? 1.f : -1.f;
	if (contourSignedArea (rect) * want < 0.f)
		std::reverse (rect.begin (), rect.end ());

	auto result = std::make_shared<std::vector<ExtrudeContour>> ();
	ExtrudeContour plate;
	plate.pts = std::move (rect);
	result->push_back (std::move (plate));

	out[0] = cggraph::Value::Make (Types ().extrudeContours, result);
	return true;
}

} // namespace cggraph_nodes
