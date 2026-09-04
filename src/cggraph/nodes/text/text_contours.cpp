#include "text_contours.h"

#include <memory>
#include <vector>

#include "../../../cgmath/font.h"
#include "../../../cgmesh/text_extrude.h"
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
		d.typeName = "text.contours";
		d.inputs.push_back ({ "police", Types ().font, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

} // namespace

TextContoursNode::TextContoursNode (const std::string &text)
{
	GetParams ().SetString ("text", text);
	GetParams ().SetFloat ("size", 1.0f);
	// Ce que `size` mesure : 0 = corps em, 1 = hauteur de capitale. Le defaut
	// reste l'em -- c'est la semantique historique du noeud, et un document
	// existant ne doit pas changer de cote en changeant de version.
	GetParams ().SetInt ("sizeMode", 0);
	// 0 = automatique (size / 600), cf. text_extrude.h.
	GetParams ().SetFloat ("flattenTol", 0.0f);
	GetParams ().SetFloat ("lineSpacing", 1.0f);
	GetParams ().SetFloat ("letterSpacing", 0.0f);
	GetParams ().SetBool ("kerning", true);
	// 0 = gauche, 1 = centre, 2 = droite.
	GetParams ().SetInt ("align", 0);
	GetParams ().SetBool ("centerOnOrigin", false);
	// Plaque de SUPPORT : 0 = aucune, 1 = plaque pleine, 2 = bandeau, 3 = cadre.
	GetParams ().SetInt ("support", 0);
	GetParams ().SetFloat ("supportMargin", 0.0f);
	GetParams ().SetFloat ("supportThickness", 0.1f);
	GetParams ().SetFloat ("supportOverlap", 0.02f);
	GetParams ().SetFloat ("supportCornerRadius", 0.0f);

	// `unionOverlaps` N'EST PAS expose, et son absence est une affirmation :
	// l'union est desormais inconditionnelle (cf. l'en-tete). Le garder en
	// parametre laisserait croire qu'on peut la desactiver.
}

void TextContoursNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "glyphsPlaced", (double)m_glyphsPlaced.load () });
	out.push_back ({ "glyphsFlattened", (double)m_glyphsFlattened.load () });
}

const cggraph::NodeDesc &TextContoursNode::GetDesc () const
{
	return Desc ();
}

bool TextContoursNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                cggraph::ValueList &out)
{
	const Font *font = in[0].Get<Font> (Types ().font);
	if (font == nullptr)
		return false;

	TextExtrudeOptions options;
	options.size = GetFloat (GetParams (), "size", 1.0f);
	options.flattenTol = GetFloat (GetParams (), "flattenTol", 0.0f);

	// Bornage, comme pour `align` et `support` plus bas : hors intervalle le
	// reglage ne veut rien dire, et retomber en silence sur le defaut masquerait
	// une faute de frappe.
	int sizeMode = GetInt (GetParams (), "sizeMode", 0);
	if (sizeMode < 0) sizeMode = 0;
	if (sizeMode > 1) sizeMode = 1;
	options.sizeMode = static_cast<TextExtrudeOptions::SizeMode> (sizeMode);
	options.lineSpacing = GetFloat (GetParams (), "lineSpacing", 1.0f);
	options.letterSpacing = GetFloat (GetParams (), "letterSpacing", 0.0f);
	options.kerning = GetBool (GetParams (), "kerning", true);
	options.centerOnOrigin = GetBool (GetParams (), "centerOnOrigin", false);

	// Bornage, comme partout ou ParamSet ne sait pas borner : hors intervalle,
	// le reglage ne veut rien dire, et retomber en silence sur le defaut
	// masquerait une faute de frappe.
	int align = GetInt (GetParams (), "align", 0);
	if (align < 0) align = 0;
	if (align > 2) align = 2;
	options.align = static_cast<TextAlign> (align);

	int support = GetInt (GetParams (), "support", 0);
	if (support < 0) support = 0;
	if (support > 3) support = 3;
	options.support = static_cast<TextExtrudeOptions::Support> (support);
	options.supportMargin = GetFloat (GetParams (), "supportMargin", 0.0f);
	options.supportThickness = GetFloat (GetParams (), "supportThickness", 0.1f);
	options.supportOverlap = GetFloat (GetParams (), "supportOverlap", 0.02f);
	options.supportCornerRadius = GetFloat (GetParams (), "supportCornerRadius", 0.0f);

	// `depth` et `materialId` restent a leur defaut : ce sont des reglages
	// d'extrusion, et text_to_contours ne les lit pas.

	TextExtrudeStats stats;
	const GraphContext graphContext = MakeGraphContext (ctx);
	std::shared_ptr<std::vector<ExtrudeContour>> contours =
		std::make_shared<std::vector<ExtrudeContour>> ();

	if (!text_to_contours (*font, GetString (GetParams (), "text", std::string ()), options,
	                       *contours, &stats, &graphContext))
		return false;
	m_glyphsPlaced.store (static_cast<unsigned int> (stats.glyphsPlaced));
	m_glyphsFlattened.store (static_cast<unsigned int> (stats.glyphsFlattened));

	out[0] = cggraph::Value::Make (Types ().extrudeContours, contours);
	return true;
}

} // namespace cggraph_nodes
