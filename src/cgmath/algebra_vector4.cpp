#include "algebra_vector4.h"

void plane_init(vec4 plane, vec3 v1, vec3 v2, vec3 v3)
{
	vec3 n;
	vec3_triangle_normal (n, v1, v2, v3);
	vec3_normalize (n);
	plane[0] = n[0];
	plane[1] = n[1];
	plane[2] = n[2];
	plane[3] = -(v1[0] * plane[0] + v1[1] * plane[1] + v1[2] * plane[2]);
}
