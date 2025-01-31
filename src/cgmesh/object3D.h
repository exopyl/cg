#pragma once

#include "mesh.h"

#include <list>
#include <memory>
using namespace std;

class Object3D
{
public:
	Object3D ();
	~Object3D ();

	bool import_file (char *filename);
	bool export_file (char *filename);

	void AddMesh (Mesh *pMesh) { m_listMeshes.push_back (pMesh); };

	list<Mesh*>& GetMeshes (void) { return m_listMeshes; };

	unsigned int GetNVertices() const;
	unsigned int GetNFaces() const;
	size_t GetNMeshes() const;
	bool IsTriangleMesh() const;


protected:
	// import / export
	void clean (void);

	bool import_obj(char* filename);
	bool export_obj(char* filename);

	bool import_stl(char* filename);
	bool export_stl(char* filename);

	bool import_3ds(char* filename);
	bool export_3ds(char* filename);

private:
	list<Mesh*> m_listMeshes;
};
