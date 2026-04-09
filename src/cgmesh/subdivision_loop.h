#pragma once
#include "mesh_half_edge.h"

//
//
//
class MeshAlgoSubdivisionLoop
{
public:
	MeshAlgoSubdivisionLoop () {};
	~MeshAlgoSubdivisionLoop () {};

	bool Apply (Mesh_half_edge *model);

private:
};
