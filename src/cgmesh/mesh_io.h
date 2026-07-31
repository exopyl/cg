#pragma once

#include <string>

class Mesh;

// Import / export (serialization) logic for Mesh.
//
// All methods are static and operate on a Mesh instance passed as the first
// parameter. MeshIO is a friend of Mesh (see mesh.h), so these helpers may
// touch Mesh's private/protected members and methods directly. This keeps the
// geometry class (Mesh) free of format-specific I/O code.
//
// Mesh::load / Mesh::save / Mesh::export_stl_binary remain as thin public
// delegators that forward to the matching MeshIO static below.
class MeshIO
{
public:
	static int load (Mesh& mesh, const char *filename);
	static int save (Mesh& mesh, const char *filename);
	static int export_stl_binary (Mesh& mesh, const char *filename);   // Binary STL (caller chooses format)

	// Wavefront OBJ : points d'entree PUBLICS, contrairement aux autres formats.
	//
	// Ils vivent dans mesh_io_obj.cpp, seule unite de compilation de la famille
	// mesh_io a ne dependre que de Mesh -- les autres tirent mesh_io_3ds.h ou
	// mesh_io_rply.h. Elle est donc la seule compilable pour la cible WebAssembly
	// de maker, ou load()/save() (mesh_io.cpp) sont justement absents : sans un
	// acces direct par format, l'OBJ y serait inatteignable.
	//
	// export_obj ecrit AUSSI le .mtl compagnon, a cote du .obj et avec le meme
	// radical ; c'est de ce chemin qu'il derive la ligne `mtllib`.
	//
	// emitObjectGroups : ecrit en plus une ligne `o <materiau>` a chaque changement
	// de materiau. Un OBJ multi-materiaux devient alors separable en objets par
	// tout outil qui lit les groupes (slicers, Blender), la ou `usemtl` seul ne
	// garantit pas le decoupage. Defaut false : aucun appelant existant ne voit son
	// fichier changer.
	static int import_obj (Mesh& mesh, const char *filename);
	static int export_obj (Mesh& mesh, const char *filename,
			       bool emitObjectGroups = false);

	// Le meme export, mais les DEUX fichiers dans UNE archive ZIP : un OBJ ne peut
	// pas embarquer ses materiaux, il les reference par `mtllib`, donc un export
	// colore compte forcement deux fichiers. Les livrer zippes evite a l'appelant
	// de les gerer par paire (et a un navigateur d'enchainer deux telechargements).
	//
	// Les noms INTERNES a l'archive sont derives du nom de l'archive : `foo.zip`
	// contient `foo.obj` et `foo.mtl`, et la ligne `mtllib foo.mtl` resout donc
	// telle quelle apres extraction. Pas de .mtl dans l'archive si le maillage ne
	// porte aucun materiau.
	static int export_obj_zip (Mesh& mesh, const char *filename,
				   bool emitObjectGroups = false);

	// Variante en MEMOIRE : renvoie les octets de l'archive, chaine vide en cas
	// d'echec. `basename` (sans extension) nomme les entrees internes. Utile la ou
	// il n'y a pas de fichier de destination -- typiquement le pont WebAssembly,
	// qui rend les octets au navigateur.
	static std::string export_obj_zip_bytes (Mesh& mesh, const std::string& basename,
						 bool emitObjectGroups = false);

private:
	static int import_mtl (Mesh& mesh, const char *filename, const char *path);
	static int export_3ds (Mesh& mesh, const char *filename);
	static int import_asc (Mesh& mesh, const char *filename);
	static int export_asc (Mesh& mesh, const char *filename);
	static int import_pset (Mesh& mesh, const char *filename);
	static int export_pset (Mesh& mesh, const char *filename);
	static int export_dae (Mesh& mesh, const char *filename);
	static int export_cpp (Mesh& mesh, const char *filename);
	static int export_gts (Mesh& mesh, const char *filename);
	static int import_off (Mesh& mesh, const char *filename);
	static int export_off (Mesh& mesh, const char *filename);
	static int import_pgm (Mesh& mesh, const char *filename);
	static int import_pts (Mesh& mesh, const char *filename);
	static int export_pts (Mesh& mesh, const char *filename);
	static int import_ply (Mesh& mesh, const char *filename);
	static int export_ply (Mesh& mesh, const char *filename);
	static int import_stl (Mesh& mesh, const char *filename);          // auto-detects binary vs ASCII
	static int import_stl_ascii (Mesh& mesh, const char *filename);
	static int export_stl (Mesh& mesh, const char *filename);          // ASCII STL (called by save() for .stl)
	static int import_u3d (Mesh& mesh, const char *filename);
	static int export_u3d (Mesh& mesh, const char *filename);
};
