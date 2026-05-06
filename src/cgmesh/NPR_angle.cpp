#include "vmeshes.h"
#include "mesh_half_edge.h"
#include "NPR_angle.h"

//
//
//
NPR_Angle::NPR_Angle ()
{
	m_pMesh = nullptr;
	m_pVMeshes = nullptr;
}

//
//
//
NPR_Angle::~NPR_Angle ()
{
	m_listSegments.clear ();
}

//
//
//
void NPR_Angle::SetMesh (Mesh_half_edge *mesh_he)
{
	m_pMesh = mesh_he;
}

void NPR_Angle::SetVMeshes (VMeshes *pVMeshes)
{
	m_pVMeshes = pVMeshes;
}

bool NPRSegmentSortPredicate (const NPRSegment& lhs, const NPRSegment& rhs)
{
	return lhs.fData < rhs.fData;
}


//
//
//
ListNPRSegments& NPR_Angle::ComputeSegments (void)
{
	m_listSegments.clear ();

	if (m_pMesh)
	{
		float *pVertices = m_pMesh->m_pMesh->m_pVertices.data();
		int nNberEdges = 3 * m_pMesh->m_pMesh->m_nFaces;
		Face **f = m_pMesh->m_pMesh->m_pFaces;
		Che_mesh *cheMesh = m_pMesh->GetCheMesh();

		for (int i=0; i<nNberEdges; i++)
		{
			Che_edge &edge = cheMesh->edge(i);
			if (edge.m_pair < 0)
				continue;

			int f1 = edge.m_face;
			int f2 = cheMesh->edge(edge.m_pair).m_face;

			Vector3f n1, n2;
			n1.Set (f[f1]->GetVertex(0), f[f1]->GetVertex(1), f[f1]->GetVertex(2));
			n2.Set (f[f2]->GetVertex(0), f[f2]->GetVertex(1), f[f2]->GetVertex(2));

			float fDot = fabs (n1 * n2);

			NPRSegment seg;
			seg.i1 = edge.m_v_begin;
			seg.i2 = edge.m_v_end;
			seg.e1.Set (pVertices[3*edge.m_v_begin], pVertices[3*edge.m_v_begin+1], pVertices[3*edge.m_v_begin+2]);
			seg.e2.Set (pVertices[3*edge.m_v_end], pVertices[3*edge.m_v_end+1], pVertices[3*edge.m_v_end+2]);
			seg.fData = fDot;
			m_listSegments.push_back (seg);
		}
	}
	else if (m_pVMeshes)
	{
/*
		list<Mesh*>& listMeshes = m_pVMeshes->GetMeshes ();
		for (list<Mesh*>::const_iterator itMesh=listMeshes.begin (); itMesh!=listMeshes.end(); itMesh++)
		{
			Mesh* pMesh = (*itMesh);

			float *pVertices = pMesh->m_pVertices.data();
			int nNberEdges = 3 * pMesh->m_nFaces;
			// ... (rest of the commented out code)
		}
*/
	}

	// highest value -> lowest value
	m_listSegments.sort (NPRSegmentSortPredicate);

	return m_listSegments;
}

ListNPRSegments& NPR_Angle::GetSegments (void)
{
	return m_listSegments;
}
