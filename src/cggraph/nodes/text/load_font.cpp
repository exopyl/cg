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
		// ENTREE OPTIONNELLE, et c'est ce qui permet de l'ajouter sans condamner
		// les documents deja ecrits : un document d'avant ne la connecte pas, le
		// noeud retombe sur ses octets internes, et le calcul est identique. La
		// version du descripteur n'est donc PAS incrementee -- IsVersionCompatible
		// etant une egalite stricte sans crochet de migration, un bump aurait
		// refuse tout ce qui existe.
		//
		// Un CHEMIN, pas des octets : Font sait lire par nom (loadFromFile), et
		// c'est la forme que les trois chargeurs partagent -- MeshIO n'a AUCUNE
		// entree en memoire, donc un port d'octets n'aurait pas pu les servir tous.
		d.inputs.push_back ({ "chemin", Types ().path, /*optional=*/true });
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

	// L'ENTREE L'EMPORTE quand elle est connectee : un noeud file.ref en amont est
	// une intention explicite, la ou les octets internes sont un etat pose de
	// cote. Les deux voies coexistent le temps que les documents migrent.
	const std::string *incoming =
		in.empty () ? nullptr : in[0].Get<std::string> (Types ().path);

	std::shared_ptr<Font> font = std::make_shared<Font> ();
	++m_parses;
	const int index = GetInt (GetParams (), "fontIndex", 0);

	if (incoming != nullptr && !incoming->empty ())
	{
		if (!font->loadFromFile (*incoming, index))
			return false;
	}
	else
	{
		if (m_bytes.empty ())
			return false;
		// La police recoit sa PROPRE copie des octets : elle n'en garde qu'un
		// pointeur interne, le buffer du noeud ne doit donc pas etre le sien.
		if (!font->loadFromMemory (m_bytes, index))
			return false;
	}

	out[0] = cggraph::Value::Make (Types ().font, font);
	return true;
}

} // namespace cggraph_nodes
