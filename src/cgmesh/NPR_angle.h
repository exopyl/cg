#ifndef __NPR_CONTOUR_H__
#define __NPR_CONTOUR_H__

#include "NPRStructures.h"
#include "mesh_half_edge.h"
#include "object3D.h"

//
//
//
class NPR_Angle
{
public:
	NPR_Angle ();
	~NPR_Angle ();

	void SetMesh (Mesh_half_edge *hemodel);
	void SetObject3D (Object3D *pObject3D);

	ListNPRSegments &ComputeSegments (void);
	ListNPRSegments &GetSegments (void);

private:
	Mesh_half_edge* m_pMesh;
	Object3D* m_pObject3D;
	ListNPRSegments m_listSegments;
};

#endif //  __NPR_EDGE_H__
