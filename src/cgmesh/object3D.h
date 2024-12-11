#ifndef __OBJECT3D_H__
#define __OBJECT3D_H__

#include "mesh.h"

#include <list>
using namespace std;

class Object3D
{
public:
	Object3D ();
	~Object3D ();

	bool import_file (char *filename);
	bool export_file (char *filename);

	void AddMesh (Mesh *pMesh) { m_listMeshes.push_back (pMesh); };

	list<Mesh*>& GetListMeshes (void) { return m_listMeshes; };

protected:
	// import / export
	void clean (void);

	bool import_obj (char *filename);
	bool export_obj (char *filename);

private:
	list<Mesh*> m_listMeshes;
};

#endif // __OBJECT3D_H__
