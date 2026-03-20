#ifndef __NPR_BORDER_H__
#define __NPR_BORDER_H__

#include "NPRStructures.h"
#include "mesh_half_edge.h"
#include "vmeshes.h"

//
//
//
class NPR_Border
{
public:
	NPR_Border ();
	~NPR_Border ();

	void SetMesh (Mesh_half_edge *hemodel);
	void SetVMeshes (VMeshes *pVMeshes);

	ListNPRSegments& ComputeSegments (void);
	ListNPRSegments& GetSegments (void);

private:
	Mesh_half_edge* m_pMesh;
	VMeshes *m_pVMeshes;
	ListNPRSegments m_listSegments;
};

#endif //  __NPR_CONTOUR_H__
