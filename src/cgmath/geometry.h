#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include "algebra_vector3.h"
#include "ray.h"
#include "aabox.h"

//
// Geometry
//
class Geometry
{
public:
	Geometry();

	virtual bool GetIntersectionBboxWithRay (vec3 o, vec3 d);

	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n) = 0;
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n) = 0;

	virtual void* GetMaterial (void) = 0;

	AABox *m_pAABox;
};

//
// Plane
//
class Plane : public Geometry
{
public:
	Plane ();
	Plane (Vector3f normale, float distance);
	Plane (Vector3f v1, Vector3f v2, Vector3f v3);
	Plane (Vector3f pt, Vector3f normale);
	~Plane () {};
	
	float distance_point (Vector3f v);                 // distance between the plane and a point
	float distance_point (float x, float y, float z);  // distance between the plane and a point
	int  position (Vector3f v);                        // position of a point
	void projected (Vector3f v_projected, Vector3f v); // projected of a point on the plane
	void move (Vector3f res, Vector3f v, float d);     // move a point in the direction of the normale to let it at a distance d from the plane
	
	// getters - setters
	float get_distance (void);
	void get_normale (Vector3f &normale);
	
	// least square plane fitting
	//
	// Reference : Eberly "Game Engine Design",
	//             Morgan Kaufmann, ISBN 1558605932
	//             september, 2000
	//             pp 473, 474 + code
	//
	void fitting (Vector3f *array, int n);

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return NULL; };

private:
	// equation of a plane : ax + by + cz + d = 0
	Vector3f normale; // (a , b , c)
	float distance; // c
};

//
// Sphere
//
class Sphere : public Geometry
{
public:
	Sphere () {
		m_vCenter[0] = 0.;
		m_vCenter[1] = 0.;
		m_vCenter[2] = 0.;
		m_fRadius = 1.;
		m_pAABox = new AABox (Vector3 (-1., -1., -1.), Vector3 (1., 1., 1.));
		m_pAABox = NULL;
	};
	~Sphere () { if (m_pAABox) delete m_pAABox; };

	void SetCenter (float vCenter[3])
	{
		m_vCenter[0] = vCenter[0];
		m_vCenter[1] = vCenter[1];
		m_vCenter[2] = vCenter[2];
		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};
	void SetCenter (float x, float y, float z)
	{
		m_vCenter[0] = x;
		m_vCenter[1] = y;
		m_vCenter[2] = z;
		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};
	void SetRadius (float fRadius)
	{
		m_fRadius = fRadius; 
 		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return NULL; };

private:
	float m_vCenter[3];
	float m_fRadius;
};

//
// Torus
//
class Torus : public Geometry
{
public:
	Torus () {
		R = 1.; r = 0.2;
		m_pAABox = NULL;
	};
	Torus (float _R, float _r) {
		R = _R;
		r = _r;
		m_pAABox = NULL;
	};
	~Torus () { if (m_pAABox) delete m_pAABox; };

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return NULL; };

public:
	float R, r;
};

//
// triangle
//
class Triangle : public Geometry
{
public:
	Triangle () {
		for (int i=0; i<3; i++)
			vec3_init (m_v[i], 0., 0., 0.);
		m_pAABox = NULL;
	};
	~Triangle () { if (m_pAABox) delete m_pAABox; };

	inline void SetVertex (int i, float x, float y, float z)
	{
		vec3_init (m_v[i], x, y, z);
		if (m_pAABox)
			m_pAABox->AddVertex (x, y, z);
		else
			m_pAABox = new AABox (x, y, z);
	}
	void Init (float x1, float y1, float z1,
			   float x2, float y2, float z2,
			   float x3, float y3, float z3)
	{
		vec3_init (m_v[0], x1, y1, z1);
		vec3_init (m_v[1], x2, y2, z2);
		vec3_init (m_v[2], x3, y3, z3);
		if (m_pAABox)
			delete m_pAABox;
		m_pAABox = new AABox (x1, y1, z1);
		m_pAABox->AddVertex (x2, y2, z2);
		m_pAABox->AddVertex (x3, y3, z3);
	}

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return NULL; };

public:
	vec3 m_v[3];
};


#endif // __GEOMETRY_H__
