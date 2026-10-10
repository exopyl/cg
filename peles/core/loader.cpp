#include "loader.h"

#include "document.h"

#include "mesh.h"
#include "vmeshes_io.h"
#include "vmodels.h"

namespace peles
{
namespace core
{

namespace
{

// Les deux separateurs sont cherches ensemble : une ligne de commande Windows
// melange les deux formes, et n'en connaitre qu'une rendrait le chemin entier
// comme nom de fichier sans que rien ne le signale.
std::string BaseName (const std::string& path)
{
	const std::size_t separator = path.find_last_of ("/\\");
	if (separator == std::string::npos)
		return path;
	return path.substr (separator + 1);
}

} // namespace

LoadResult LoadModel (Document& document, const std::string& path)
{
	LoadResult result;

	if (path.empty ())
	{
		result.error = "chemin vide";
		return result;
	}

	Model* model = document.AddModel (BaseName (path));
	if (model == nullptr)
	{
		result.error = "le document a refuse un nouveau modele";
		return result;
	}
	model->m_path = path;

	if (!VMeshesIO::load (model->m_meshes, path.c_str ()) ||
	    model->m_meshes.GetNMeshes () == 0)
	{
		document.RemoveModel (model);
		result.error = "format inconnu, fichier illisible ou sans geometrie";
		return result;
	}

	// LES NORMALES SONT RECALCULEES A L'IMPORT, quel que soit le format. Un OBJ
	// peut en porter, un STL binaire en porte toujours, un nuage de points n'en
	// a pas : sans ce passage, l'eclairage dependrait du format du fichier.
	for (Mesh* mesh : model->m_meshes.GetMeshes ())
	{
		if (mesh != nullptr)
			mesh->ComputeNormals ();
	}

	// Le BVH suppose la geometrie STATIQUE ensuite : il reference les sommets
	// du maillage, donc il se construit une fois, apres l'import.
	model->BuildBVH ();
	model->ComputeBBox ();

	// LE MODELE FRAICHEMENT CHARGE DEVIENT LA SELECTION, SANS CONDITION.
	//
	// ⚠ LES PANNEAUX DE DROITE DECRIVENT LE MODELE SELECTIONNE, et un chargement
	// qui AJOUTE a la scene doit donc reposer la selection : sans cela, ouvrir B
	// apres A laisse la table decrire A pendant que la vue montre les deux.
	document.Select (model);

	result.model = model;
	return result;
}

} // namespace core
} // namespace peles
