#include <stdlib.h>
#include <utility>

#include "scene.h"
#include "mesh_raycast.h"

Scene::Scene ()
{
	// stats
	m_i_GetIntersectionBboxWithRay_count = 0;
	m_i_GetIntersectionWithRay_count = 0;
}

Scene::~Scene () = default;

void Scene::AddObject (std::unique_ptr<Geometry> pObject)
{
	// L'accelerateur est bati ICI, a l'adoption -- pas au premier rayon (scene.h).
	Accelerator accel;
	if (Mesh *pMesh = dynamic_cast<Mesh*> (pObject.get ()))
	{
		if (pMesh->GetNVertices () > 0 && pMesh->GetNFaces () > 0)
		{
			accel.mesh   = pMesh;
			accel.octree = BuildRaycastOctree (*pMesh);
		}
	}

	m_pObjects.push_back (std::move (pObject));
	m_pAccelerators.push_back (std::move (accel));
}

//
// Interroge l'objet i : par son octree s'il en a un, par sa virtuelle sinon.
// Le `dynamic_cast` a eu lieu une fois pour toutes dans AddObject ; ici il n'y a
// qu'un test de pointeur, hors du chemin arithmetique.
//
unsigned int Scene::IntersectObject (size_t i,
                                     const Vector3f &vOrig, const Vector3f &vDirection,
                                     float *_t, Vector3f &vIntersection, Vector3f &vNormal)
{
	const Accelerator &accel = m_pAccelerators[i];
	// `::` obligatoire : sans lui, la recherche de nom trouve d'abord le membre
	// Scene::GetIntersectionWithRay et s'arrete la (masquage de nom).
	if (accel.mesh && accel.octree)
		return (unsigned int)::GetIntersectionWithRay (*accel.mesh, *accel.octree,
		                                               vOrig, vDirection,
		                                               _t, vIntersection, vNormal);

	return (unsigned int)m_pObjects[i]->GetIntersectionWithRay (vOrig, vDirection,
	                                                            _t, vIntersection, vNormal);
}

Geometry* Scene::GetIntersectionWithRay (const Vector3f &vOrig, const Vector3f &vDirection, Vector3f &vIntersection, Vector3f &vNormal)
{
	float fT = 0.;
	Geometry *pIntersectedObject = nullptr;
	for (size_t i=0; i<m_pObjects.size(); i++)
	{
		float fTCurrent = 0.;
		Vector3f vIntersectionCurrent, vNormalCurrent;

		m_i_GetIntersectionBboxWithRay_count++;
		bool bGotBBox = m_pObjects[i]->GetIntersectionBboxWithRay (vOrig, vDirection);
		if (bGotBBox == false)
			continue;

		m_i_GetIntersectionWithRay_count++;
		unsigned int bIntersectionCurrent = IntersectObject (i, vOrig, vDirection,
								     &fTCurrent,
								     vIntersectionCurrent, vNormalCurrent);
		if (bIntersectionCurrent == 1)
		{
			if ((!pIntersectedObject) ||
			    (pIntersectedObject && fTCurrent < fT))
			{
				pIntersectedObject = m_pObjects[i].get();
				fT = fTCurrent;
				vIntersection[0] = vIntersectionCurrent[0];
				vIntersection[1] = vIntersectionCurrent[1];
				vIntersection[2] = vIntersectionCurrent[2];
				vNormal[0] = vNormalCurrent[0];
				vNormal[1] = vNormalCurrent[1];
				vNormal[2] = vNormalCurrent[2];
			}
		}
	}

	return pIntersectedObject;
}

Geometry* Scene::GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, Vector3f &vIntersection, Vector3f &vNormal)
{
	float fT = 0.;
	Vector3f vDirection;
	vDirection[0] = vEnd[0] - vStart[0];
	vDirection[1] = vEnd[1] - vStart[1];
	vDirection[2] = vEnd[2] - vStart[2];
	Geometry *pIntersectedObject = nullptr;
	for (size_t i=0; i<m_pObjects.size(); i++)
	{
		float fTCurrent = 0.;
		Vector3f vIntersectionCurrent, vNormalCurrent;
		unsigned int bIntersectionCurrent = IntersectObject (i, vStart, vDirection,
								     &fTCurrent,
								     vIntersectionCurrent, vNormalCurrent);
		if (bIntersectionCurrent == 1 && fTCurrent < 1.)
		{
			if ((!pIntersectedObject) ||
			    (pIntersectedObject && fTCurrent < fT))
			{
				pIntersectedObject = m_pObjects[i].get();
				fT = fTCurrent;
				vIntersection[0] = vIntersectionCurrent[0];
				vIntersection[1] = vIntersectionCurrent[1];
				vIntersection[2] = vIntersectionCurrent[2];
				vNormal[0] = vNormalCurrent[0];
				vNormal[1] = vNormalCurrent[1];
				vNormal[2] = vNormalCurrent[2];
			}
		}
	}

	return pIntersectedObject;
}

void Scene::Dump (void)
{
	printf ("Scene stats :\n");
	printf ("m_i_GetIntersectionBboxWithRay_count : %d\n", m_i_GetIntersectionBboxWithRay_count);
	printf ("m_i_GetIntersectionWithRay_count : %d\n", m_i_GetIntersectionWithRay_count);
}

