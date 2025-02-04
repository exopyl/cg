#include <stdlib.h>

#include "scene.h"

Scene::Scene ()
{
	m_nObjects = 0;

	// stats
	m_i_GetIntersectionBboxWithRay_count = 0;
	m_i_GetIntersectionWithRay_count = 0;
}

Scene::~Scene ()
{
	for (unsigned int i=0; i<m_nObjects; i++)
		delete m_pObjects[i];
	m_nObjects = 0;
}

void Scene::AddObject (Geometry *pObject)
{
	m_pObjects[m_nObjects++] = pObject;
}

Geometry* Scene::GetIntersectionWithRay (vec3 vOrig, vec3 vDirection, vec3 vIntersection, vec3 vNormal)
{
	float fT = 0.;
	Geometry *pIntersectedObject = NULL;
	for (unsigned int i=0; i<m_nObjects; i++)
	{
		float fTCurrent = 0.;
		float vIntersectionCurrent[3], vNormalCurrent[3];

		m_i_GetIntersectionBboxWithRay_count++;
		bool bGotBBox = m_pObjects[i]->GetIntersectionBboxWithRay (vOrig, vDirection);
		if (bGotBBox == false)
			continue;

		m_i_GetIntersectionWithRay_count++;
		unsigned int bIntersectionCurrent = m_pObjects[i]->GetIntersectionWithRay (vOrig, vDirection,
											   &fTCurrent,
											   vIntersectionCurrent, vNormalCurrent);
		if (bIntersectionCurrent == 1)
		{
			if ((!pIntersectedObject) ||
			    (pIntersectedObject && fTCurrent < fT))
			{
				pIntersectedObject = m_pObjects[i];
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

Geometry* Scene::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, vec3 vIntersection, vec3 vNormal)
{
	float fT = 0.;
	float vDirection[3];
	vDirection[0] = vEnd[0] - vStart[0];
	vDirection[1] = vEnd[1] - vStart[1];
	vDirection[2] = vEnd[2] - vStart[2];
	Geometry *pIntersectedObject = NULL;
	for (unsigned int i=0; i<m_nObjects; i++)
	{
		float fTCurrent = 0.;
		float vIntersectionCurrent[3], vNormalCurrent[3];
		unsigned int bIntersectionCurrent = m_pObjects[i]->GetIntersectionWithRay (vStart, vDirection,
											   &fTCurrent,
											   vIntersectionCurrent, vNormalCurrent);
		if (bIntersectionCurrent == 1 && fTCurrent < 1.)
		{
			if ((!pIntersectedObject) ||
			    (pIntersectedObject && fTCurrent < fT))
			{
				pIntersectedObject = m_pObjects[i];
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

