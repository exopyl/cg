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

void NPRManager::SetVMeshes (VMeshes *pVMeshes)
{
	if (m_pNPRAngle)
		m_pNPRAngle->SetVMeshes (pVMeshes);
	if (m_pNPRBorder)
		m_pNPRBorder->SetVMeshes (pVMeshes);
	if (m_pNPRSilhouette)
		m_pNPRSilhouette->SetVMeshes (pVMeshes);
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

	// Aucune des trois branches ne correspond quand le type est inconnu ou que le
	// generateur correspondant n'est pas instancie. La fonction tombait alors en
	// fin de corps EN PROMETTANT UNE REFERENCE : l'appelant en recevait une
	// invalide, et la moindre lecture partait en comportement indefini
	// (cpp:S935, NPRManager.cpp:67).
	//
	// Une liste vide a duree de vie statique donne une reference valide et une
	// semantique lisible : « aucun segment pour ce type ». La signature rend une
	// reference non const, donc un appelant peut techniquement y ecrire ; c'est le
	// prix de ne pas changer l'interface, et cela reste preferable a une reference
	// pendante.
	static ListNPRSegments emptySegments;
	return emptySegments;
}
