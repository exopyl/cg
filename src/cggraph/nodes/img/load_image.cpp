#include "load_image.h"

#include <memory>

#include "../../../cgimg/image.h"
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
		d.typeName = "img.io.load";
		// Entree OPTIONNELLE, meme motif que sur les deux autres chargeurs : les
		// documents deja ecrits ne la connectent pas et se calculent a l'identique.
		d.inputs.push_back ({ "chemin", Types ().path, /*optional=*/true });
		d.outputs.push_back ({ "image", Types ().image, false });
		return d;
	}();
	return desc;
}

} // namespace

LoadImageNode::LoadImageNode ()
{
	GetParams ().SetString ("name", "sans nom", cggraph::ParamRole::NonSemantic);
	// Semantic ET Internal : voir LoadMeshNode et LoadFontNode. Le hachage des
	// octets est ce qui invalide le cache, et rien de ce que l'utilisateur tape.
	GetParams ().SetString ("source.identity", HashKey (HashBuffer (nullptr, 0)),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
}

const cggraph::NodeDesc &LoadImageNode::GetDesc () const
{
	return Desc ();
}

void LoadImageNode::SetBytes (std::vector<unsigned char> bytes)
{
	m_bytes = std::move (bytes);
	GetParams ().SetString ("source.identity",
	                        HashKey (HashBuffer (m_bytes.data (), m_bytes.size ())),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
}

void LoadImageNode::SetName (const std::string &name)
{
	GetParams ().SetString ("name", name, cggraph::ParamRole::NonSemantic);
}

bool LoadImageNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	(void)ctx;

	// L'ENTREE L'EMPORTE quand elle est connectee. Par chemin, le format est
	// reconnu a l'EXTENSION (Img::load) ; par octets, au CONTENU
	// (load_from_memory). La couverture n'est donc pas tout a fait la meme, et
	// c'est assume : un fichier porte un nom, un buffer n'en a pas.
	const std::string *incoming =
		in.empty () ? nullptr : in[0].Get<std::string> (Types ().path);

	std::shared_ptr<Img> image = std::make_shared<Img> ();
	++m_decodes;

	if (incoming != nullptr && !incoming->empty ())
	{
		if (image->load (incoming->c_str ()) != 0)
			return false;
	}
	else
	{
		if (m_bytes.empty ())
			return false;
		// L'image recoit sa PROPRE copie des pixels : load_from_memory decode dans
		// le tampon d'Img et ne garde aucun pointeur vers `m_bytes`, qui peut donc
		// continuer a vivre -- il sert au hachage, pas au contenu.
		if (image->load_from_memory (m_bytes.data (), m_bytes.size ()) != 0)
			return false;
	}

	// Une image de surface nulle n'est pas une donnee utilisable : tout l'aval
	// (quantification, vectorisation) la refuserait un cran plus loin, avec un
	// diagnostic moins clair. Le decodeur ne peut pas la produire -- il rejette
	// deja les dimensions nulles -- mais la garde est gratuite et documente
	// l'invariant du port.
	if (image->width () == 0 || image->height () == 0)
		return false;

	out[0] = cggraph::Value::Make (Types ().image, image);
	return true;
}

} // namespace cggraph_nodes
