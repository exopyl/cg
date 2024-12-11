#include "NPRManager.h"

NPRManager::NPRManager ()
{
	m_pNPRAngle = new NPR_Angle ();
	m_pNPRBorder = new NPR_Border ();
	m_pNPRSilhouette = new NPR_Silhouette ();
}

NPRManager::~NPRManager ()
{
	if (m_pNPRAngle) delete m_pNPRAngle;
	if (m_pNPRBorder) delete m_pNPRBorder;
	if (m_pNPRSilhouette) delete m_pNPRSilhouette;
}

void NPRManager::SetMesh (Mesh_half_edge *pMesh)
{
	if (m_pNPRAngle)
		m_pNPRAngle->SetMesh (pMesh);

	if (m_pNPRBorder)
		m_pNPRBorder->SetMesh (pMesh);

	if (m_pNPRSilhouette)
		m_pNPRSilhouette->SetMesh (pMesh);
}

void NPRManager::SetObject3D (Object3D *pObject3D)
{
	if (m_pNPRAngle)
		m_pNPRAngle->SetObject3D (pObject3D);

	if (m_pNPRBorder)
		m_pNPRBorder->SetObject3D (pObject3D);

	if (m_pNPRSilhouette)
		m_pNPRSilhouette->SetObject3D (pObject3D);
}

void NPRManager::SetCameraPosition (Vector3f vCameraPosition)
{
	if (m_pNPRSilhouette)
		m_pNPRSilhouette->SetCameraPosition (vCameraPosition);
}

void NPRManager::ComputeSegments (void)
{
	if (m_pNPRAngle)
		m_pNPRAngle->ComputeSegments ();

	if (m_pNPRBorder)
		m_pNPRBorder->ComputeSegments ();

	if (m_pNPRSilhouette)
		m_pNPRSilhouette->ComputeSegments ();
}

ListNPRSegments& NPRManager::GetSegments (NPRSegmentType eType)
{
	if (eType == NPR_SEGMENT_ANGLE && m_pNPRAngle)
		return m_pNPRAngle->GetSegments ();

	if (eType == NPR_SEGMENT_BORDER && m_pNPRBorder)
		return m_pNPRBorder->GetSegments ();

	if (eType == NPR_SEGMENT_SILHOUETTE && m_pNPRSilhouette)
		return m_pNPRSilhouette->GetSegments ();
}
