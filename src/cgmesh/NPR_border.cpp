#include "NPR_border.h"

//
//
//
NPR_Border::NPR_Border ()
{
	m_pMesh = NULL;
	m_pObject3D = NULL;
}

//
//
//
NPR_Border::~NPR_Border ()
{
	m_listSegments.clear ();
}

//
//
//
void NPR_Border::SetMesh (Mesh_half_edge *mesh_he)
{
	m_pMesh = mesh_he;
}

void NPR_Border::SetObject3D (Object3D *pObject3D)
{
	m_pObject3D = pObject3D;
}

//
//
//
ListNPRSegments& NPR_Border::ComputeSegments (void)
{
	m_listSegments.clear ();

	if (m_pMesh)
	{
		float *pVertices = m_pMesh->m_pVertices;
		int nNberEdges = 3 * m_pMesh->m_nFaces;
		Che_edge** pEdges = m_pMesh->m_edges;

		for (int i=0; i<nNberEdges; i++)
		{
			Che_edge* pEdge = pEdges[i];
			if (pEdge && !pEdge->m_pair)
			{
				NPRSegment seg;
				seg.i1 = pEdge->m_v_begin;
				seg.i2 = pEdge->m_v_end;
				seg.e1.Set (pVertices[3*pEdge->m_v_begin], pVertices[3*pEdge->m_v_begin+1], pVertices[3*pEdge->m_v_begin+2]);
				seg.e2.Set (pVertices[3*pEdge->m_v_end], pVertices[3*pEdge->m_v_end+1], pVertices[3*pEdge->m_v_end+2]);
				m_listSegments.push_back (seg);
				//listSegments.push_back (pEdge->m_v_begin);
				//listSegments.push_back (pEdge->m_v_end);
			}
		}
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
				if (pEdge && !pEdge->m_pair)
				{
					NPRSegment seg;
					seg.i1 = pEdge->m_v_begin;
					seg.i2 = pEdge->m_v_end;
					seg.e1.Set (pVertices[3*pEdge->m_v_begin], pVertices[3*pEdge->m_v_begin+1], pVertices[3*pEdge->m_v_begin+2]);
					seg.e2.Set (pVertices[3*pEdge->m_v_end], pVertices[3*pEdge->m_v_end+1], pVertices[3*pEdge->m_v_end+2]);
					m_listSegments.push_back (seg);
				}
			}
		}
*/
	}

	return m_listSegments;
}

ListNPRSegments& NPR_Border::GetSegments (void)
{
	return m_listSegments;
}
