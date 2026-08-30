#include "extrude_text.h"

#include <memory>

#include "../../../cgmath/font.h"
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
		d.typeName = "text.extrude";
		d.inputs.push_back ({ "police", Types ().font, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

ExtrudeTextNode::ExtrudeTextNode (const std::string &text)
{
	GetParams ().SetString ("text", text);
	GetParams ().SetFloat ("size", 1.0f);
	GetParams ().SetFloat ("depth", 0.2f);
	GetParams ().SetFloat ("flattenTol", 0.01f);
	GetParams ().SetFloat ("lineSpacing", 1.0f);
	GetParams ().SetFloat ("letterSpacing", 0.0f);
	GetParams ().SetBool ("kerning", true);
	GetParams ().SetBool ("centerOnOrigin", false);
	GetParams ().SetBool ("unionOverlaps", false);
	// Plaque de SUPPORT : 0 = aucune, 1 = plaque pleine, 2 = bandeau, 3 = cadre.
	// C'est un contour de PLUS dans l'union 2D des glyphes, jamais un second
	// volume : le depot n'a aucun booleen 3D, et cette voie n'en demande pas.
	//
	// ⚠ Support et texte partagent la PROFONDEUR -- il n'y a qu'un champ depth,
	// et l'union 2D produit une region plane unique. Un socle plus epais que les
	// lettres n'est pas un reglage manquant : c'est une capacite absente.
	GetParams ().SetInt ("support", 0);
	GetParams ().SetFloat ("supportMargin", 0.0f);
	GetParams ().SetFloat ("supportThickness", 0.1f);
	GetParams ().SetFloat ("supportOverlap", 0.02f);
	GetParams ().SetFloat ("supportCornerRadius", 0.0f);
}

const cggraph::NodeDesc &ExtrudeTextNode::GetDesc () const
{
	return Desc ();
}

TextExtrudeStats ExtrudeTextNode::GetLastStats () const
{
	TextExtrudeStats stats;
	stats.glyphsPlaced = m_glyphsPlaced.load ();
	stats.glyphsFlattened = m_glyphsFlattened.load ();
	return stats;
}

bool ExtrudeTextNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                               cggraph::ValueList &out)
{
	const Font *font = in[0].Get<Font> (Types ().font);
	if (font == nullptr)
		return false;

	TextExtrudeOptions options;
	options.size = GetFloat (GetParams (), "size", 1.0f);
	options.depth = GetFloat (GetParams (), "depth", 0.2f);
	options.flattenTol = GetFloat (GetParams (), "flattenTol", 0.01f);
	options.lineSpacing = GetFloat (GetParams (), "lineSpacing", 1.0f);
	options.letterSpacing = GetFloat (GetParams (), "letterSpacing", 0.0f);
	options.kerning = GetBool (GetParams (), "kerning", true);
	options.centerOnOrigin = GetBool (GetParams (), "centerOnOrigin", false);
	options.unionOverlaps = GetBool (GetParams (), "unionOverlaps", false);

	// L'enumeration du corps emballe n'a que quatre valeurs : hors de [0, 3], le
	// reglage ne veut rien dire, et retomber en silence sur « aucun support »
	// masquerait une faute de frappe. On la borne, comme l'adaptateur borne
	// partout ailleurs ce que ParamSet ne sait pas borner.
	int support = GetInt (GetParams (), "support", 0);
	if (support < 0) support = 0;
	if (support > 3) support = 3;
	options.support = static_cast<TextExtrudeOptions::Support> (support);
	options.supportMargin = GetFloat (GetParams (), "supportMargin", 0.0f);
	options.supportThickness = GetFloat (GetParams (), "supportThickness", 0.1f);
	options.supportOverlap = GetFloat (GetParams (), "supportOverlap", 0.02f);
	options.supportCornerRadius = GetFloat (GetParams (), "supportCornerRadius", 0.0f);

	TextExtrudeStats stats;
	const GraphContext graphContext = MakeGraphContext (ctx);
	std::shared_ptr<Mesh> mesh (
		text_to_extruded_mesh (*font, GetString (GetParams (), "text", std::string ()), options,
		                       &stats, &graphContext));
	m_glyphsPlaced.store (stats.glyphsPlaced);
	m_glyphsFlattened.store (stats.glyphsFlattened);

	// nullptr couvre l'echec comme l'annulation : c'est l'evaluateur qui les
	// distingue, en interrogeant le contexte avant ce code de retour.
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, mesh);
	return true;
}

} // namespace cggraph_nodes
