#ifndef __SUBDIVISION_LOOP_H__
#define __SUBDIVISION_LOOP_H__

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

#endif // __SUBDIVISION_LOOP_H__
