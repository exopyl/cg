//
//  Mesh::load / Mesh::save -- version WebAssembly, restreinte a l'OBJ.
//
// mesh_io.cpp definit ces deux membres, mais il est HORS du build Emscripten :
// il inclut mesh_io_3ds.h et appelle les importeurs PLY (rply) et U3D, soit
// plusieurs formats binaires entiers a porter pour rendre deux methodes de
// dispatch. mesh_io_obj.cpp avait deja ete extrait pour la meme raison.
//
// La restriction est donc EXPLICITE et elle est un refus, pas une degradation :
// toute extension autre que .obj rend -1, comme un format inconnu. Un maillage
// PLY glisse a la page web echoue en le disant, il ne se charge pas a moitie.
//
// La normalisation de casse suit celle de mesh_io.cpp cote load ; cote save,
// mesh_io.cpp compare l'extension par strcmp sans normaliser, si bien qu'un
// chemin en .OBJ n'est pas reconnu. Ce fichier ne corrige pas cet ecart : il le
// reproduit, pour que la cible web ne diverge pas du natif sur une regle deja
// documentee ailleurs.
//
// Le fichier entier est sous __EMSCRIPTEN__ : le build natif globe *.cpp et
// compilerait sinon une SECONDE definition de Mesh::load / Mesh::save, a cote
// de celle de mesh_io.cpp.
//
#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

#include "mesh.h"
#include "mesh_io.h"

namespace
{

std::string LowerExtension (const char *filename)
{
	std::filesystem::path p (filename);
	std::string ext = p.extension ().string ();
	std::transform (ext.begin (), ext.end (), ext.begin (),
	                [] (unsigned char c) { return (char)std::tolower (c); });
	return ext;
}

} // namespace

int Mesh::load (const char *filename)
{
	if (!filename)
		return -1;

	if (LowerExtension (filename) != ".obj")
		return -1;

	const int res = MeshIO::import_obj (*this, filename);

	// Coherence, reprise mot pour mot de MeshIO::load : sans coordonnees de
	// texture lues, aucune face ne doit pretendre en porter.
	if (GetNTextureCoordinates () == 0)
	{
		for (unsigned int iFace = 0; iFace < GetNFaces (); iFace++)
			FaceAt (iFace)->SetUsesTextureCoordinates (false);
	}

	return res;
}

int Mesh::save (const char *filename) const
{
	if (!filename)
		return -1;

	const std::size_t n = std::strlen (filename);
	if (n < 4 || std::strcmp (filename + (n - 4), ".obj") != 0)
		return -1;

	return MeshIO::export_obj (*this, filename);
}

#endif // __EMSCRIPTEN__
