#ifndef __EXTRACT_PLANES_H__
#define __EXTRACT_PLANES_H__

#include "../cgmath/cgmath.h"
#include "mesh_half_edge.h"
#include "geometric_primitives.h"

class Cextract_planes
{
public:
	Cextract_planes (Mesh_half_edge *model);

	void compute (float threshold, float percentage);

	void Dump (char *filename);

private:
	void add_plane (VectorizedPlane *plane);

	Mesh_half_edge *model;
	float total_area;
	int n_planes;
	VectorizedPlane **planes;
};

#endif // __EXTRACT_PLANES_H__
