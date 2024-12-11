#ifndef __NPR_MANAGER_H__
#define __NPR_MANAGER_H__

#include "NPR_angle.h"
#include "NPR_border.h"
#include "NPR_silhouette.h"

#include "mesh_half_edge.h"

class NPRManager
{
public:
	NPRManager ();
	~NPRManager ();
	
	void SetMesh (Mesh_half_edge *pMesh);
	void SetObject3D (Object3D *pObject3D);
	void SetCameraPosition (Vector3f vCameraPosition);

	void ComputeSegments (void);
	ListNPRSegments& GetSegments (NPRSegmentType eType);

private:
	NPR_Angle* m_pNPRAngle;
	NPR_Border* m_pNPRBorder;
	NPR_Silhouette* m_pNPRSilhouette;
	Mesh_half_edge *m_pMesh;
};


#endif // __NPR_MANAGER_H__
