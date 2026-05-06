#include "NPR_border.h"

//
//
//
NPR_Border::NPR_Border ()
{
	m_pMesh = nullptr;
	m_pVMeshes = nullptr;
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

void NPR_Border::SetVMeshes (VMeshes *pVMeshes)
{
	m_pVMeshes = pVMeshes;
}

//
//
//
ListNPRSegments& NPR_Border::ComputeSegments (void)
{
	m_listSegments.clear ();

	if (m_pMesh)
	{
		float *pVertices = m_pMesh->m_pMesh->m_pVertices.data();
		int nNberEdges = 3 * m_pMesh->m_pMesh->m_nFaces;
		Che_mesh *cheMesh = m_pMesh->GetCheMesh();

		for (int i=0; i<nNberEdges; i++)
		{
			Che_edge &edge = cheMesh->edge(i);
			if (edge.m_pair < 0)
			{
				NPRSegment seg;
				seg.i1 = edge.m_v_begin;
				seg.i2 = edge.m_v_end;
				seg.e1.Set (pVertices[3*edge.m_v_begin], pVertices[3*edge.m_v_begin+1], pVertices[3*edge.m_v_begin+2]);
				seg.e2.Set (pVertices[3*edge.m_v_end], pVertices[3*edge.m_v_end+1], pVertices[3*edge.m_v_end+2]);
				m_listSegments.push_back (seg);
			}
		}
	}
	else if (m_pVMeshes)
	{
/*
		list<Mesh*>& listMeshes = m_pVMeshes->GetMeshes ();
		for (list<Mesh*>::const_iterator itMesh=listMeshes.begin (); itMesh!=listMeshes.end(); itMesh++)
		{
			Mesh* pMesh = (*itMesh);
			float *pVertices = pMesh->get_vertices ();
			int nNberEdges = 3 * pMesh->nf;
			// ...
		}
*/
	}

	return m_listSegments;
}

ListNPRSegments& NPR_Border::GetSegments (void)
{
	return m_listSegments;
}
