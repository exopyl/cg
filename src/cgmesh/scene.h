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
	std::vector<std::unique_ptr<Geometry>> m_pObjects;

public:
	// stats
	int m_i_GetIntersectionBboxWithRay_count;
	int m_i_GetIntersectionWithRay_count;
};
