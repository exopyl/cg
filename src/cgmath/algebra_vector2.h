#pragma once

#include <stdio.h>
#include <math.h>

typedef float vec2[2];

inline void vec2_init (vec2 v, float x, float y)
{
	v[0] = x;
	v[1] = y;
}

inline void vec2_copy (vec2 res, vec2 u)
{
	res[0] = u[0];
	res[1] = u[1];
}

inline float vec2_length (vec2 v)
{
	return sqrtf (v[0]*v[0] + v[1]*v[1]);
}

inline float vec2_length2 (vec2 v)
{
	return (v[0]*v[0] + v[1]*v[1]);
}

inline void vec2_subtraction (vec2 res, vec2 v1, vec2 v2)
{
	res[0] = v1[0] - v2[0];
	res[1] = v1[1] - v2[1];
}

inline void vec2_addition (vec2 res, vec2 v1, vec2 v2)
{
	res[0] = v1[0] + v2[0];
	res[1] = v1[1] + v2[1];
}

inline void vec2_scalar (vec2 res, vec2 v1, float s)
{
	res[0] = s*v1[0];
	res[1] = s*v1[1];
}

inline float vec2_distance (vec2 v1, vec2 v2)
{
	vec2 v1v2;
	vec2_subtraction (v1v2, v2, v1);
	return vec2_length (v1v2);
}

inline float vec2_dot_product (vec2 u, vec2 v)
{
	return (u[0]*v[0] + u[1]*v[1]);
}

inline void vec2_normalize (vec2 v)
{
	float l = vec2_length (v);
	if (l == 0.) return;
	v[0] /= l;
	v[1] /= l;
}

inline void vec2_dump (vec2 v)
{
	printf ("vec2 : %f %f\n", v[0], v[1]);
}

// 2d segment
typedef struct seg2
{
	vec2 vs;
	vec2 ve;
} seg2;
