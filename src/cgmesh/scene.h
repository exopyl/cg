#pragma once
#include <memory>
#include <vector>

#include "../cgmesh/cgmesh.h"

class Scene
{
public:
	Scene ();
	~Scene ();

	void AddObject (std::unique_ptr<Geometry> pObject);

	Geometry* GetIntersectionWithRay (const Vector3f &vOrig, const Vector3f &vDirection, Vector3f &vIntersection, Vector3f &vNormal);
	Geometry* GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, Vector3f &vIntersection, Vector3f &vNormal);

	void Dump (void);
private:
	// Interroge l'objet i, par son octree s'il en a un, par sa virtuelle sinon.
	unsigned int IntersectObject (size_t i,
	                              const Vector3f &vOrig, const Vector3f &vDirection,
	                              float *_t, Vector3f &vIntersection, Vector3f &vNormal);

	std::vector<std::unique_ptr<Geometry>> m_pObjects;

	// Accelerateur de lancer de rayon, un par objet, ALIGNE sur m_pObjects.
	//
	// `mesh` est non nul uniquement pour les objets qui sont des Mesh, dont la
	// virtuelle teste sinon tous les triangles a chaque rayon. Les primitives
	// analytiques (Sphere, Torus, Plane...) gardent la leur, n'ayant rien a
	// accelerer.
	//
	// ⚠ CONSTRUIT A L'ADOPTION, JAMAIS PENDANT UNE REQUETE : une construction
	// paresseuse dans GetIntersectionWithRay ferait d'une lecture une mutation.
	// Le contrat qui en decoule est donc : UN OBJET ADOPTE PAR LA SCENE NE CHANGE
	// PLUS DE GEOMETRIE. Scene peut le tenir parce qu'elle possede ses objets ;
	// si un appelant mute un maillage via un pointeur brut conserve, l'octree
	// devient perime en silence.
	//
	// La duree de vie exigee par mesh_raycast.h est triviale ici : Scene detient
	// le maillage ET son octree, et les detruit ensemble.
	struct Accelerator
	{
		Mesh                   *mesh = nullptr;
		std::unique_ptr<Octree> octree;
	};
	std::vector<Accelerator> m_pAccelerators;

public:
	// stats
	int m_i_GetIntersectionBboxWithRay_count;
	int m_i_GetIntersectionWithRay_count;
};
