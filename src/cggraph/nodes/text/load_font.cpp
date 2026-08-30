#include "load_font.h"

#include <memory>

#include "../../../cgmath/font.h"
#include "../file_identity.h"
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
		d.typeName = "text.font.load";
		d.outputs.push_back ({ "police", Types ().font, false });
		return d;
	}();
	return desc;
}

} // namespace

LoadFontNode::LoadFontNode ()
{
	GetParams ().SetString ("name", "sans nom", cggraph::ParamRole::NonSemantic);
	// Semantic ET Internal : voir LoadMeshNode. Le hachage des octets est ce qui
	// invalide le cache, et rien de ce que l'utilisateur tape.
	GetParams ().SetString ("source.identity", HashKey (HashBuffer (nullptr, 0)),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
	GetParams ().SetInt ("fontIndex", 0);
}

const cggraph::NodeDesc &LoadFontNode::GetDesc () const
{
	return Desc ();
}

void LoadFontNode::SetBytes (std::vector<unsigned char> bytes)
{
	m_bytes = std::move (bytes);
	GetParams ().SetString ("source.identity",
	                        HashKey (HashBuffer (m_bytes.data (), m_bytes.size ())),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
}

void LoadFontNode::SetName (const std::string &name)
{
	GetParams ().SetString ("name", name, cggraph::ParamRole::NonSemantic);
}

bool LoadFontNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;

	if (m_bytes.empty ())
		return false;

	std::shared_ptr<Font> font = std::make_shared<Font> ();
	++m_parses;
	// La police recoit sa PROPRE copie des octets : elle n'en garde qu'un
	// pointeur interne, le buffer du noeud ne doit donc pas etre le sien.
	if (!font->loadFromMemory (m_bytes, GetInt (GetParams (), "fontIndex", 0)))
		return false;

	out[0] = cggraph::Value::Make (Types ().font, font);
	return true;
}

} // namespace cggraph_nodes
