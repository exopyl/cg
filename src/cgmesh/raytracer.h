#ifndef __RAYTRACER_H__
#define __RAYTRACER_H__

#include "../cgimg/cgimg.h"
#include "../cgmesh/cgmesh.h"
#include "scene.h"
#include "camera.h"

// Shadows
static unsigned int RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL = 1; // 600;
static float        RAYTRACER_SHADOWS_LIGHT_POSITION_OFFSET = 20.;

//
// raytracer
//
class Raytracer
{
public:
	Raytracer ();
	~Raytracer ();

	// set up methods
	inline Camera* GetCamera (void) { return m_pCamera; };
	inline Scene* GetScene (void) { return m_pScene; };
	inline Light* GetLight (unsigned int i) { return m_pLights[i]; };
	unsigned int AddLight (Light* pLight);

	// ray tracing process
	Img* Trace (unsigned int iWidth, unsigned int iHeight);

	// dump
	void Dump (void);
	void export_statistics (char *filename);

private:
	int GetColorWithRay (float vOrig[3], float vDirection[3], float color[3]);

private:
	Camera *m_pCamera;      // camera
	unsigned int m_nLights; // number of lights
	Light *m_pLights[8];    // description of the lights in the scene
	Scene *m_pScene;        // contains the description of the geometric objects

	// benchmarks
	double m_time_GetColorWithRay;
	double m_time_GetIntersectionWithRay;
};

#endif // __RAYTRACER_H__
