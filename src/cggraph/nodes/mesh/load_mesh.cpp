#include "load_mesh.h"

#include <memory>

#include "../../../cgmesh/mesh.h"
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
		d.typeName = "mesh.io.load";
		// Entree OPTIONNELLE : connectee, elle l'emporte sur le parametre `path`.
		// Optionnelle et non obligatoire pour que les documents deja ecrits -- qui
		// portent leur chemin en parametre -- se calculent a l'identique ; la
		// version du descripteur n'est donc pas incrementee.
		d.inputs.push_back ({ "chemin", Types ().path, /*optional=*/true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

LoadMeshNode::LoadMeshNode (const std::string &path)
{
	GetParams ().SetString ("path", path);
	GetParams ().SetBool ("verifyHash", false);
	// Semantic ET Internal : c'est lui qui invalide le cache quand le fichier
	// change -- son role entier --, et il n'est regle par personne, seulement
	// reecrit par RefreshExternalState.
	GetParams ().SetString ("source.identity", "absent", cggraph::ParamRole::Semantic,
	                        cggraph::ParamVisibility::Internal);
}

const cggraph::NodeDesc &LoadMeshNode::GetDesc () const
{
	return Desc ();
}

void LoadMeshNode::RefreshExternalState ()
{
	const std::string path = GetString (GetParams (), "path", std::string ());

	// La porte bon marche : elle repond « rien n'a bouge » sans ouvrir le
	// fichier. Le hash n'est demande que si l'appelant doute de l'horloge, et il
	// se paie alors sur un fichier qu'on allait relire de toute facon.
	const FileIdentity identity = StatFile (path);
	std::string key = StatKey (identity);
	if (identity.exists && GetBool (GetParams (), "verifyHash", false))
	{
		std::uint64_t hash = 0;
		if (HashFile (path, hash))
			key = HashKey (hash);
	}

	// UpdateString et non SetString : c'est la SEULE ecriture de parametre qui
	// puisse survenir pendant une evaluation, et elle ne doit toucher que le champ
	// chaine -- SetString remet la valeur entiere a zero, donc ecrit `kind`, que
	// l'evaluateur lit hors de la pre-passe pour refuser un parametre Driven.
	//
	// Le repli pose l'entree : un document ecrit a la main peut ne pas la porter,
	// et le constructeur n'a alors pas eu l'occasion de la declarer.
	if (!GetParams ().UpdateString ("source.identity", key))
		GetParams ().SetString ("source.identity", key, cggraph::ParamRole::Semantic,
		                        cggraph::ParamVisibility::Internal);
}

bool LoadMeshNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	(void)ctx;

	// L'ENTREE L'EMPORTE quand elle est connectee : un file.ref en amont est une
	// intention explicite, la ou le parametre est un etat pose de cote.
	//
	// ⚠ RefreshExternalState, lui, ne voit PAS les entrees -- elles n'existent
	// qu'au calcul. Quand le chemin vient d'un port, l'identite versee par CE
	// noeud est donc celle de son parametre, souvent vide, et c'est correct : la
	// fraicheur du fichier est portee par le `source.identity` du file.ref amont,
	// et la signature d'un noeud inclut toute sa branche.
	const std::string *incoming =
		in.empty () ? nullptr : in[0].Get<std::string> (Types ().path);
	const std::string path = (incoming != nullptr && !incoming->empty ())
		? *incoming
		: GetString (GetParams (), "path", std::string ());
	if (path.empty ())
		return false;

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh> ();
	++m_reads;
	if (mesh->load (path.c_str ()) != 0 || mesh->GetNVertices () == 0)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, mesh);
	return true;
}

} // namespace cggraph_nodes
