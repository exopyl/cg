#ifndef __NPR_SILHOUETTE_H__
#define __NPR_SILHOUETTE_H__

#include "NPRStructures.h"
#include "mesh_half_edge.h"
#include "vmeshes.h"

//
//
//
class NPR_Silhouette
{
public:
	NPR_Silhouette ();
	~NPR_Silhouette ();

	void SetMesh (Mesh_half_edge *hemodel);
	void SetVMeshes (VMeshes *pVMeshes);
	void SetCameraPosition (Vector3f vCameraPosition);

	ListNPRSegments &ComputeSegments (void);
	ListNPRSegments &GetSegments (void);

private:
	Mesh_half_edge* m_pMesh;
	VMeshes* m_pVMeshes;
	Vector3f m_vCameraPosition;
	ListNPRSegments m_listSegments;
};

#endif //  __NPR_SILHOUETTE_H__
