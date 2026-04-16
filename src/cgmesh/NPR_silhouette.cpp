#include "NPR_silhouette.h"

//
//
//
NPR_Silhouette::NPR_Silhouette ()
{
	m_pMesh = nullptr;
	m_pVMeshes = nullptr;
}

//
//
//
NPR_Silhouette::~NPR_Silhouette ()
{
	m_listSegments.clear ();
}

//
//
//
void NPR_Silhouette::SetMesh (Mesh_half_edge *mesh_he)
{
	m_pMesh = mesh_he;
}

void NPR_Silhouette::SetVMeshes (VMeshes *pVMeshes)
{
	m_pVMeshes = pVMeshes;
}

void NPR_Silhouette::SetCameraPosition (Vector3f vCameraPosition)
{
	m_vCameraPosition.Set (vCameraPosition.x, vCameraPosition.y, vCameraPosition.z);
}

//
//
//
ListNPRSegments& NPR_Silhouette::ComputeSegments (void)
{
	m_listSegments.clear ();

	if (m_pMesh)
	{
	}
	else if (m_pVMeshes)
	{
/*
		list<Mesh*>& listMeshes = m_pVMeshes->GetMeshes ();
		for (list<Mesh*>::const_iterator itMesh=listMeshes.begin (); itMesh!=listMeshes.end(); itMesh++)
		{
			Mesh* pMesh = (*itMesh);
			// ...
		}
*/
	}

	return m_listSegments;
}

ListNPRSegments& NPR_Silhouette::GetSegments (void)
{
	return m_listSegments;
}
