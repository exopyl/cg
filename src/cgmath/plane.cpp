#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "geometry.h"
#include "TMatrix3.h"

//
// Constructors
//
Plane::Plane ()
{
	normale.Set(0., 0., 1.);
	distance = 0.;
	m_pAABox = NULL;
}

Plane::Plane (Vector3f _normale, float _distance)
{
  normale.Set (_normale.x, _normale.y, _normale.z);
  distance = _distance;
  m_pAABox = NULL;
}

Plane::Plane (Vector3f v1, Vector3f v2, Vector3f v3)
{
  normale.Set (v1.y*(v2.z-v3.z)+v2.y*(v3.z-v1.z)+v3.y*(v1.z-v2.z),
		v1.z*(v2.x-v3.x)+v2.z*(v3.x-v1.x)+v3.z*(v1.x-v2.x),
		v1.x*(v2.y-v3.y)+v2.x*(v3.y-v1.y)+v3.x*(v1.z-v2.y));
  normale.Normalize ();
  
  distance = -normale.x*v1.x - normale.y*v1.y - normale.z*v1.z;
  m_pAABox = NULL;
}

Plane::Plane (Vector3f _pt, Vector3f _normale)
{
  normale = _normale;
  normale.Normalize ();
  distance = - (normale * _pt);
  m_pAABox = NULL;
}

// distance between the plane and a point
float Plane::distance_point (float x, float y, float z)
{
  return normale.x*x + normale.y*y + normale.z*z + distance;
}

// distance between the plane and a point
float Plane::distance_point (Vector3f v)
{
	return distance_point (v.x, v.y, v.z);
}

// position of a point
int Plane::position (Vector3f v)
{
	float d = distance_point (v);
	if (d == 0.0)
		return 0;
	if (d > 0)
		return 1;
	else
		return -1;
}

// projection of a point on the plane
void Plane::projected (Vector3f res, Vector3f v)
{
  float d = distance_point (v);
  res.Set (v.x - d * normale.x, v.y - d * normale.y, v.z - d * normale.z);
}

// move a point in the direction of the normale to let it at a distance d from the plane
void
Plane::move (Vector3f res, Vector3f v, float d)
{
	float dist = distance_point (v);
	res.Set (v.x + (d - dist) * normale.x,
		v.y + (d - dist) * normale.y,
		v.z + (d - dist) * normale.z);
}

//
// getters
//
float Plane::get_distance (void)
{
  return distance;
}

void Plane::get_normale (Vector3f &_normale)
{
  _normale.Set (normale.x, normale.y, normale.z);
}

/* least square plane fitting */
/*
 * Reference : Eberly "Game Engine Design",
 *             Morgan Kaufmann, ISBN 1558605932
 *             september, 2000
 *             pp 473, 474 + code
 */
void Plane::fitting (Vector3f *array, int n)
{
  Vector3f p;
  Vector3f vector_walk;
  int i;
  
  float XX = 0.;
  float XY = 0.;
  float XZ = 0.;
  float YY = 0.;
  float YZ = 0.;
  float ZZ = 0.;

  // average of points
  p.Set (0.0, 0.0, 0.0);
  for (i=0; i<n; i++) p += array[i];
  p.x /= n;
  p.y /= n;
  p.z /= n;
  distance = sqrt (p*p);

  // sums of products
  vector_walk.Set (0.0, 0.0, 0.0);
  for (i=0; i<n; i++)
    {
      vector_walk = array[i] - p;
      XX += vector_walk.x * vector_walk.x;
      XY += vector_walk.x * vector_walk.y;
      XZ += vector_walk.x * vector_walk.z;
      YY += vector_walk.y * vector_walk.y;
      YZ += vector_walk.y * vector_walk.z;
      ZZ += vector_walk.z * vector_walk.z;
    }
  
  // setup the eigensolver
  Matrix3f m (XX, XY, XZ,
	      XY, YY, YZ,
	      XZ, YZ, ZZ);
  Vector3f e1, e2, e3, v;
  m.SolveEigensystem (e1, e2, e3, v);
  normale = e3;
  normale.Normalize ();

  /* the minimum energy */
  // return eigen value (2);
}












//
//
//
int Plane::GetIntersectionWithRay (vec3 vO, vec3 vD, float *_t, vec3 i, vec3 n)
{
	float a = normale[0];
	float b = normale[1];
	float c = normale[2];
	float d = distance;

	float t = - (d+a*vO[0]+b*vO[1]+c*vO[2]) / (a*vD[0]+b*vD[1]+c*vD[2]);
	if (t < 0.)
		return 0;
	*_t = t;

	i[0] = vO[0] + t*vD[0];
	i[1] = vO[1] + t*vD[1];
	i[2] = vO[2] + t*vD[2];

	n[0] = a;
	n[1] = b;
	n[2] = c;

	return 1;
}

int Plane::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	vec3 vDirection;
	vDirection[0] = vEnd[0] - vStart[0];
	vDirection[1] = vEnd[1] - vStart[1];
	vDirection[2] = vEnd[2] - vStart[2];
	float l = sqrt (vec3_dot_product (vDirection, vDirection));
	vec3_normalize (vDirection);
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res && *_t < l);
}
