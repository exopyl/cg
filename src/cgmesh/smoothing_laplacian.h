#pragma once
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
