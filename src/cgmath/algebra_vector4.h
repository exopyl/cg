#pragma once

#include "algebra_vector3.h"

typedef float vec4[4];

inline void vec4_init (vec4 v, float x, float y, float z, float w)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = w;
}

extern void plane_init(vec4 plane, vec3 v1, vec3 v2, vec3 v3);
