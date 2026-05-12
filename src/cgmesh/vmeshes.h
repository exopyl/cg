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

	bool load(char* filename);
	bool save(char *filename);

	void AddMesh (Mesh *pMesh) { m_Meshes.push_back (pMesh); };

	std::vector<Mesh*>& GetMeshes (void) { return m_Meshes; };

	unsigned int GetNVertices() const;
	unsigned int GetNFaces() const;
	size_t GetNMeshes() const;
	bool IsTriangleMesh() const;


protected:
	// import / export
	void clean (void);

	bool export_obj(char* filename);
	bool export_stl(char* filename);          // ASCII STL : one solid block per Mesh
	bool export_stl_binary(char* filename);   // Binary STL : single concatenated solid
	bool export_ply(char* filename);

	bool import_3ds(char* filename);
	bool import_3dm(char* filename);
	bool import_gltf(char* filename);
	bool export_3ds(char* filename);

private:
	std::vector<Mesh*> m_Meshes;
};
