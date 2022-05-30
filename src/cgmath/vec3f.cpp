#include "vec3f.h"

void vec3_triangle_normal (vec3 n, vec3 v1, vec3 v2, vec3 v3)
{
	double ux, uy, uz;
	double vx, vy, vz;
	double nx, ny, nz;
	nx = ny = nz = 0.0f;

	ux = v2[0] - v1[0];
	uy = v2[1] - v1[1];
	uz = v2[2] - v1[2];
	
	vx = v3[0] - v1[0];
	vy = v3[1] - v1[1];
	vz = v3[2] - v1[2];

	nx = (uy * vz) - (uz * vy);
	ny = (uz * vx) - (ux * vz);
	nz = (ux * vy) - (uy * vx);

	vec3_init (n, (float)nx, (float)ny, (float)nz);
	
	return;
	vec3 v1v2, v1v3;
	vec3_subtraction (v1v2, v2, v1);
	vec3_subtraction (v1v3, v3, v1);
	vec3_cross_product (n, v1v2, v1v3);
}
