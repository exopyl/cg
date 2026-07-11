#pragma once

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

private:
	static int import_mtl (Mesh& mesh, const char *filename, const char *path);
	static int import_obj (Mesh& mesh, const char *filename);
	static int export_obj (Mesh& mesh, const char *filename);
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
