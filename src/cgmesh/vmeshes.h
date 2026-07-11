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

	void AddMesh (Mesh *pMesh) { m_Meshes.push_back (pMesh); };

	// Détruit tous les Mesh* possédés et vide la liste (utile avant un
	// rechargement en place depuis le fichier d'origine).
	void clean (void);

	std::vector<Mesh*>& GetMeshes (void) { return m_Meshes; };

	unsigned int GetNVertices() const;
	unsigned int GetNFaces() const;
	size_t GetNMeshes() const;
	bool IsTriangleMesh() const;

	void Normalize();

private:
	std::vector<Mesh*> m_Meshes;
};
