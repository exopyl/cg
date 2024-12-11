#include "NPR_angle.h"
#include "mesh_half_edge.h"
#include "object3D.h"

//
//
//
NPR_Angle::NPR_Angle ()
{
	m_pMesh = NULL;
	m_pObject3D = NULL;
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

void NPR_Angle::SetObject3D (Object3D *pObject3D)
{
	m_pObject3D = pObject3D;
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
		float *pVertices = m_pMesh->m_pVertices;
		int nNberEdges = 3 * m_pMesh->m_nFaces;
		Face **f = m_pMesh->m_pFaces;
		Che_edge** pEdges = m_pMesh->m_edges;

		for (int i=0; i<nNberEdges; i++)
		{
			Che_edge* pEdge = pEdges[i];
			if (!pEdge || !pEdge->m_pair)
				continue;

			int f1 = pEdge->m_face;
			int f2 = pEdge->m_pair->m_face;
			
			Vector3f n1, n2;
			n1.Set (f[f1]->GetVertex(0), f[f1]->GetVertex(1), f[f1]->GetVertex(2));
			n2.Set (f[f2]->GetVertex(0), f[f2]->GetVertex(1), f[f2]->GetVertex(2));

			float fDot = fabs (n1 * n2);

			NPRSegment seg;
			seg.i1 = pEdge->m_v_begin;
			seg.i2 = pEdge->m_v_end;
			seg.e1.Set (pVertices[3*pEdge->m_v_begin], pVertices[3*pEdge->m_v_begin+1], pVertices[3*pEdge->m_v_begin+2]);
			seg.e2.Set (pVertices[3*pEdge->m_v_end], pVertices[3*pEdge->m_v_end+1], pVertices[3*pEdge->m_v_end+2]);
			seg.fData = fDot;
			m_listSegments.push_back (seg);
		}
	}
	else if (m_pObject3D)
	{
/*
		list<Mesh_half_edge*>& listMeshes = m_pObject3D->GetListMeshes ();
		for (list<Mesh_half_edge*>::const_iterator itMesh=listMeshes.begin (); itMesh!=listMeshes.end(); itMesh++)
		{
			Mesh_half_edge* pMesh = (*itMesh);

			float *pVertices = pMesh->m_pVertices;
			int nNberEdges = 3 * pMesh->m_nFaces;
			Che** pEdges = pMesh->m_edges;

			for (int i=0; i<nNberEdges; i++)
			{
				Che* pEdge = pEdges[i];
				if (!pEdge || !pEdge->m_pair)
					continue;

				int f1 = pEdge->m_face;
				int f2 = pEdge->m_pair->m_face;
				
				Vector3f n1, n2;
				n1.Set (pMesh->fn[3*f1], pMesh->fn[3*f1+1], pMesh->fn[3*f1+2]);
				n2.Set (pMesh->fn[3*f2], pMesh->fn[3*f2+1], pMesh->fn[3*f2+2]);

				float fDot = fabs (n1 * n2);

				NPRSegment seg;
				seg.i1 = pEdge->m_v_begin;
				seg.i2 = pEdge->m_v_end;
				seg.e1.Set (pVertices[3*pEdge->m_v_begin], pVertices[3*pEdge->m_v_begin+1], pVertices[3*pEdge->m_v_begin+2]);
				seg.e2.Set (pVertices[3*pEdge->m_v_end], pVertices[3*pEdge->m_v_end+1], pVertices[3*pEdge->m_v_end+2]);
				seg.fData = fDot;
				m_listSegments.push_back (seg);
			}
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
