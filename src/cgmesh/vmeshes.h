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

	// TAILLE VISEE PAR LA NORMALISATION : la plus grande dimension de la boite
	// englobante apres Normalize(). En unites monde, donc en MILLIMETRES (cf.
	// mesh_io_gltf.cpp).
	//
	// Cent, soit DIX CENTIMETRES. La valeur se lit sur la reference metrique de la
	// vue : la base de coupe de sinaia est graduee en centimetres, un carreau
	// valant 10 mm. Un modele normalise couvre donc dix graduations -- assez pour
	// que ses proportions se lisent au carreau, la ou une cible de 10 mm n'en
	// couvrait qu'un seul et une cible de 1 mm un dixieme.
	static constexpr float kNormalizedSize = 100.f;

	// Recentre sur l'origine, puis met a l'echelle pour que la plus grande
	// dimension vaille kNormalizedSize. Met a jour les bboxes des maillages.
	void Normalize();

private:
	std::vector<Mesh*> m_Meshes;
};
