#ifndef __SCENE_H__
#define __SCENE_H__

#include "../cgmesh/cgmesh.h"

class Scene
{
public:
	Scene ();
	~Scene ();

	void AddObject (Geometry *pObject);

	Geometry* GetIntersectionWithRay (vec3 vOrig, vec3 vDirection, vec3 vIntersection, vec3 vNormal);
	Geometry* GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, vec3 vIntersection, vec3 vNormal);

	void Dump (void);
private:
	unsigned int m_nObjects;
	Geometry *m_pObjects[1000];

public:
	// stats
	int m_i_GetIntersectionBboxWithRay_count;
	int m_i_GetIntersectionWithRay_count;
};

#endif // __SCENE_H__
