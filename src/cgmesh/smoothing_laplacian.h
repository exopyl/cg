#ifndef __SMOOTHING_LAPLACIAN_H__
#define __SMOOTHING_LAPLACIAN_H__

#include "mesh_half_edge.h"

//
//
//
class MeshAlgoSmoothingLaplacian
{
public:
	MeshAlgoSmoothingLaplacian () {};
	~MeshAlgoSmoothingLaplacian () {};

	bool Apply (Mesh_half_edge *model);
};

#endif // __SMOOTHING_LAPLACIAN_H__
