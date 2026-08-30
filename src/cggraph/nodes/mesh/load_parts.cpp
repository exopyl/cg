#include "load_parts.h"

#include <memory>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/vmeshes.h"
#include "../../../cgmesh/vmeshes_io.h"
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
		d.typeName = "mesh.io.load_parts";
		d.outputs.push_back ({ "pieces", Types ().meshArray, false });
		return d;
	}();
	return desc;
}

} // namespace

LoadPartsNode::LoadPartsNode (const std::string &path)
{
	GetParams ().SetString ("path", path);
	GetParams ().SetString ("source.identity", "absent", cggraph::ParamRole::Semantic,
	                        cggraph::ParamVisibility::Internal);
}

const cggraph::NodeDesc &LoadPartsNode::GetDesc () const
{
	return Desc ();
}

void LoadPartsNode::RefreshExternalState ()
{
	const std::string path = GetString (GetParams (), "path", std::string ());
	const std::string key = StatKey (StatFile (path));
	if (!GetParams ().UpdateString ("source.identity", key))
		GetParams ().SetString ("source.identity", key, cggraph::ParamRole::Semantic,
		                        cggraph::ParamVisibility::Internal);
}

bool LoadPartsNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;

	const std::string path = GetString (GetParams (), "path", std::string ());
	if (path.empty ())
		return false;

	VMeshes container;
	++m_reads;
	if (!VMeshesIO::load (container, path.c_str ()))
		return false;

	std::shared_ptr<MeshArray> pieces = std::make_shared<MeshArray> ();

	// CESSION, et non copie : VMeshes possede les Mesh* et son destructeur les
	// detruit. Les emballer dans des shared_ptr puis VIDER le conteneur transfere
	// la propriete sans une seule copie profonde -- la meme regle que celle de
	// node_support.h pour une enveloppe de travail, appliquee a un conteneur.
	// Vider est OBLIGATOIRE : sans cela chaque piece serait detruite deux fois.
	std::vector<Mesh *> &meshes = container.GetMeshes ();
	for (std::size_t i = 0; i < meshes.size (); ++i)
	{
		Mesh *piece = meshes[i];
		// Detache AVANT d'emballer : si l'emballage echouait, la piece fuirait,
		// mais elle ne serait jamais detruite deux fois.
		meshes[i] = nullptr;
		if (piece != nullptr)
			pieces->items.push_back (std::shared_ptr<const Mesh> (piece));
	}
	meshes.clear ();

	if (pieces->items.empty ())
		return false;

	out[0] = cggraph::Value::Make (Types ().meshArray, pieces);
	return true;
}

} // namespace cggraph_nodes
