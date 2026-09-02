#include "file_ref.h"

#include <memory>

#include "../file_identity.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "file.ref";
		d.outputs.push_back ({ "chemin", Types ().path, false });
		return d;
	}();
	return desc;
}

// Dernier segment d'un chemin, pour le nom decoratif.
std::string BaseName (const std::string &path)
{
	const std::size_t cut = path.find_last_of ("/\\");
	return cut == std::string::npos ? path : path.substr (cut + 1);
}

} // namespace

FileRefNode::FileRefNode ()
{
	// SERIALISE et semantique : c'est lui qui designe la ressource, et c'est lui
	// qui rend le document reproductible.
	GetParams ().SetString ("path", std::string ());

	// Decoratif, hors signature : le renommer ne recalcule rien.
	GetParams ().SetString ("name", "sans nom", cggraph::ParamRole::NonSemantic);

	// Semantic ET Internal : voir LoadMeshNode. C'est l'identite du CONTENU qui
	// invalide le cache, jamais ce que l'utilisateur tape. Interne parce qu'elle
	// est ecrite par RefreshExternalState, donc pendant une pre-passe qui peut
	// etre celle d'un autre fil : la lire depuis Compute serait la seule lecture
	// de parametre concurrente d'une ecriture.
	GetParams ().SetString ("source.identity", StatKey (FileIdentity ()),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
}

const cggraph::NodeDesc &FileRefNode::GetDesc () const
{
	return Desc ();
}

std::string FileRefNode::GetPath () const
{
	const cggraph::ParamValue *value = GetParams ().Find ("path");
	return value != nullptr ? value->stringValue : std::string ();
}

void FileRefNode::RefreshIdentity ()
{
	GetParams ().SetString ("source.identity", StatKey (StatFile (GetPath ())),
	                        cggraph::ParamRole::Semantic, cggraph::ParamVisibility::Internal);
}

void FileRefNode::RefreshExternalState ()
{
	RefreshIdentity ();
}

void FileRefNode::SetPath (const std::string &path)
{
	GetParams ().SetString ("path", path);
	GetParams ().SetString ("name", BaseName (path), cggraph::ParamRole::NonSemantic);
	RefreshIdentity ();
}

bool FileRefNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                           cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;

	const std::string path = GetPath ();
	if (path.empty ())
		return false;

	// LE FICHIER N'EST PAS OUVERT ICI -- ce noeud designe, il ne lit pas. On
	// verifie seulement qu'il EXISTE, et c'est un service reel : sans cela
	// l'echec surviendrait un cran plus loin, dans un chargeur qui dirait
	// « police illisible » la ou le vrai probleme est un chemin faux.
	//
	// L'etat releve vient de RefreshExternalState, qui a deja state le fichier
	// pendant la pre-passe ; on ne le refait pas ici.
	const FileIdentity identity = StatFile (path);
	if (!identity.exists)
		return false;

	out[0] = cggraph::Value::Make (Types ().path, std::make_shared<std::string> (path));
	return true;
}

} // namespace cggraph_nodes
