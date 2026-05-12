#pragma once

#include "mesh.h"

#include <vector>
#include <memory>
using namespace std;

class VMeshes
{
public:
	VMeshes ();
	~VMeshes ();

	bool load(const char* filename);
	bool save(const char *filename);

	void AddMesh (Mesh *pMesh) { m_Meshes.push_back (pMesh); };

	std::vector<Mesh*>& GetMeshes (void) { return m_Meshes; };

	unsigned int GetNVertices() const;
	unsigned int GetNFaces() const;
	size_t GetNMeshes() const;
	bool IsTriangleMesh() const;


protected:
	// import / export
	void clean (void);

	bool export_obj(const char* filename);
	bool export_stl(const char* filename);          // ASCII STL : one solid block per Mesh
	bool export_stl_binary(const char* filename);   // Binary STL : single concatenated solid
	bool export_ply(const char* filename);

	bool import_3ds(const char* filename);
	bool import_3dm(const char* filename);
	bool import_gltf(const char* filename);
	bool export_3ds(const char* filename);

private:
	std::vector<Mesh*> m_Meshes;
};
