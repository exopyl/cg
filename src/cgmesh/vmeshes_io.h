#pragma once

class VMeshes;

// Import / export (serialization) logic for VMeshes.
//
// All methods are static and operate on a VMeshes instance through its public
// API only (GetMeshes / AddMesh). This keeps the container class (VMeshes) free
// of format-specific I/O code, which lives here and in the format-specific
// translation units (vmeshes_io.cpp, mesh_io_3dm.cpp, vmeshes_io_occt.cpp).
class VMeshesIO
{
public:
	static bool load(VMeshes& vm, const char* filename);
	static bool save(VMeshes& vm, const char* filename);

private:
	// OBJ import splitting each object ('o', or 'g' when the file has no 'o')
	// into its own Mesh. Vertices / UVs / materials are re-indexed per object
	// (OBJ indices are file-global). Implemented in vmeshes_io.cpp on top of the
	// single-mesh MeshIO::import_obj path.
	static bool import_obj(VMeshes& vm, const char* filename);
	static bool import_3ds(VMeshes& vm, const char* filename);
	static bool import_3dm(VMeshes& vm, const char* filename);
	static bool import_gltf(VMeshes& vm, const char* filename);
	// STEP / IGES import via OpenCASCADE. One Mesh per TopoDS_Face (the
	// CAD feature decomposition is preserved). When cgmesh is built
	// without CG_HAS_OCCT both fall through to a no-op stub returning
	// false. Implementations live in vmeshes_io_occt.cpp.
	static bool import_step(VMeshes& vm, const char* filename);
	static bool import_iges(VMeshes& vm, const char* filename);

	static bool export_obj(VMeshes& vm, const char* filename);
	static bool export_stl(VMeshes& vm, const char* filename);          // ASCII STL : one solid block per Mesh
	static bool export_stl_binary(VMeshes& vm, const char* filename);   // Binary STL : single concatenated solid
	static bool export_ply(VMeshes& vm, const char* filename);
	static bool export_3ds(VMeshes& vm, const char* filename);
};
