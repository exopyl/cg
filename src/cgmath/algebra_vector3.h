#pragma once

#include <stdio.h>
#include <math.h>

#include "common.h"

//
//
//
typedef float vec3[3];

inline void vec3_init (vec3 v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}
inline void vec3_copy (vec3 res, vec3 u)
{
	res[0] = u[0];
	res[1] = u[1];
	res[2] = u[2];
}
inline void vec3_subtraction (vec3 res, vec3 u, vec3 v)
{
	res[0] = u[0] - v[0];
	res[1] = u[1] - v[1];
	res[2] = u[2] - v[2];
}
inline void vec3_addition (vec3 res, vec3 u, vec3 v)
{
	res[0] = u[0] + v[0];
	res[1] = u[1] + v[1];
	res[2] = u[2] + v[2];
}
inline void vec3_cross_product (vec3 res, vec3 u, vec3 v)
{
	res[0] = u[1]*v[2] - u[2]*v[1];
	res[1] = u[2]*v[0] - u[0]*v[2];
	res[2] = u[0]*v[1] - u[1]*v[0];
}
inline void vec3_scale (vec3 res, vec3 v, float s)
{
	res[0] = s * v[0];
	res[1] = s * v[1];
	res[2] = s * v[2];
}
inline float vec3_dot_product (vec3 u, vec3 v)
{
	return (u[0]*v[0] + u[1]*v[1] + u[2]*v[2]);
}
inline float vec3_length (vec3 v)
{
	return sqrt (vec3_dot_product(v, v));
}
inline float vec3_length2 (vec3 v)
{
	return vec3_dot_product(v, v);
}
extern void vec3_triangle_normal (vec3 n, vec3 v1, vec3 v2, vec3 v3);
inline void vec3_barycenter (vec3 b, vec3 v1, vec3 v2, vec3 v3)
{
	vec3_init (b,
		   (v1[0]+v2[0]+v3[0])/3.,
		   (v1[1]+v2[1]+v3[1])/3.,
		   (v1[2]+v2[2]+v3[2])/3.);
}
inline float vec3_triangle_area (vec3 v1, vec3 v2, vec3 v3)
{
	vec3 n;
	vec3_triangle_normal (n, v1, v2, v3);
	return .5*vec3_length (n);
}
inline void vec3_normalize (vec3 v)
{
	float l = vec3_length (v);
	if (l == 0.) return;
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}
inline float vec3_distance (vec3 v1, vec3 v2)
{
	vec3 v1v2;
	vec3_subtraction (v1v2, v2, v1);
	return vec3_length (v1v2);
}
inline float vec3_distance2 (vec3 v1, vec3 v2)
{
	vec3 v1v2;
	vec3_subtraction (v1v2, v2, v1);
	return vec3_length2 (v1v2);
}
inline void vec3_dump (vec3 v)
{
	printf ("vec3 : %f %f %f\n", v[0], v[1], v[2]);
}
