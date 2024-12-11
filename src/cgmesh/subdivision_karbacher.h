#ifndef __SUBDIVISION_KARBACHER_H__
#define __SUBDIVISION_KARBACHER_H__

#include "cgmesh.h"

class MeshAlgoSubdivisionKarbacher
{
public:
	MeshAlgoSubdivisionKarbacher () {};
	~MeshAlgoSubdivisionKarbacher () {};

	bool Apply (Mesh_half_edge *model);

private:
	void InitializePosition (Vector3d &par_pos, Vector3d &par_npos,
				 Vector3d par_v1, Vector3d par_v2, Vector3d par_v3,
				 Vector3d par_n1, Vector3d par_n2, Vector3d par_n3);
	void DeleteAngles (void);

	Mesh_half_edge *m_pModel;
};

#endif // __SUBDIVISION_KARBACHER_H__
