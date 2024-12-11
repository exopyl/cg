#ifndef __NPR_BORDER_H__
#define __NPR_BORDER_H__

#include "NPRStructures.h"
#include "mesh_half_edge.h"
#include "object3D.h"

//
//
//
class NPR_Border
{
public:
	NPR_Border ();
	~NPR_Border ();

	void SetMesh (Mesh_half_edge *hemodel);
	void SetObject3D (Object3D *pObject3D);

	ListNPRSegments& ComputeSegments (void);
	ListNPRSegments& GetSegments (void);

private:
	Mesh_half_edge* m_pMesh;
	Object3D *m_pObject3D;
	ListNPRSegments m_listSegments;
};

#endif //  __NPR_CONTOUR_H__
