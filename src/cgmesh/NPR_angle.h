#ifndef __NPR_CONTOUR_H__
#define __NPR_CONTOUR_H__

#include "NPRStructures.h"
#include "mesh_half_edge.h"
#include "vmeshes.h"

//
//
//
class NPR_Angle
{
public:
	NPR_Angle ();
	~NPR_Angle ();

	void SetMesh (Mesh_half_edge *hemodel);
	void SetVMeshes (VMeshes *pVMeshes);

	ListNPRSegments &ComputeSegments (void);
	ListNPRSegments &GetSegments (void);

private:
	Mesh_half_edge* m_pMesh;
	VMeshes* m_pVMeshes;
	ListNPRSegments m_listSegments;
};

#endif //  __NPR_EDGE_H__
