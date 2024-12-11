#include "NPR_silhouette.h"

//
//
//
NPR_Silhouette::NPR_Silhouette ()
{
	m_pMesh = NULL;
	m_pObject3D = NULL;
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

void NPR_Silhouette::SetObject3D (Object3D *pObject3D)
{
	m_pObject3D = pObject3D;
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
	else if (m_pObject3D)
	{
/*
		list<Mesh_half_edge*>& listMeshes = m_pObject3D->GetListMeshes ();
		for (list<Mesh_half_edge*>::const_iterator itMesh=listMeshes.begin (); itMesh!=listMeshes.end(); itMesh++)
		{
			Mesh_half_edge* pMesh = (*itMesh);

			float *pVertices = pMesh->get_vertices ();
			int nNberEdges = 3 * pMesh->nf;
			Che** pEdges = pMesh->m_edges;

			for (int i=0; i<nNberEdges; i++)
			{
				Che* pEdge = pEdges[i];
				if (!pEdge || !pEdge->m_pair)
					continue;

				int iVertex = pEdge->m_v_begin;
				Vector3f vVertex (pVertices[3*iVertex], pVertices[3*iVertex+1], pVertices[3*iVertex+2]);
				Vector3f vDir = vVertex - m_vCameraPosition;

				int f1 = pEdge->m_face;
				int f2 = pEdge->m_pair->m_face;
				
				Vector3f n1, n2;
				n1.Set (pMesh->fn[3*f1], pMesh->fn[3*f1+1], pMesh->fn[3*f1+2]);
				n2.Set (pMesh->fn[3*f2], pMesh->fn[3*f2+1], pMesh->fn[3*f2+2]);

				float fDot = (n1 * vDir) *(n2 * vDir);

				if (fDot < 0.)
				{
					NPRSegment seg;
					seg.i1 = pEdge->m_v_begin;
					seg.i2 = pEdge->m_v_end;
					seg.e1.Set (pVertices[3*pEdge->m_v_begin], pVertices[3*pEdge->m_v_begin+1], pVertices[3*pEdge->m_v_begin+2]);
					seg.e2.Set (pVertices[3*pEdge->m_v_end], pVertices[3*pEdge->m_v_end+1], pVertices[3*pEdge->m_v_end+2]);
					seg.fData = fDot;
					m_listSegments.push_back (seg);
				}
			}
		}
*/
	}

	return m_listSegments;
}

ListNPRSegments& NPR_Silhouette::GetSegments (void)
{
	return m_listSegments;
}
