#include "subdivision_karbacher.h"

bool MeshAlgoSubdivisionKarbacher::Apply (Mesh_half_edge *model)
{
	m_pModel = model;
	int nv = model->m_pMesh->m_nVertices;
	int nf = model->m_pMesh->m_nFaces;
	float *v = model->m_pMesh->m_pVertices;
	Face **f = model->m_pMesh->m_pFaces;
	Che_mesh *chePtr = model->GetCheMesh();
	int m_ne = chePtr->m_ne;
	float *vn = model->m_pMesh->m_pVertexNormals;

	int i;

	// vertices
	int nv_new = nv+nf;
	float *v_new = (float*) malloc (3*nv_new*sizeof(float));
	v_new = (float*) memcpy ((void*)v_new, (const void*)v, 3*nv*sizeof(float));
	int nv_new_walk = nv;

	// faces
	int nf_new = 3*nf;
	Face **f_new = (Face**) malloc (3*nf_new*sizeof(Face*));

	// edges: copy existing edges and grow the vector for new ones
	m_ne = 3*nf;
	int ne_new = 3*nf_new;
	// We'll work directly with the chePtr->m_edges vector.
	// First, reserve space for the new edges.
	chePtr->m_edges.reserve(ne_new);
	int ne_new_walk = m_ne;

	chePtr->m_edges_face.resize(nf_new, -1);
	chePtr->m_edges_vertex.resize(nv_new, -1);


	// create the new triangles and link the new edges
	for (i=0; i<nf; i++)
	{
		//
		//             v1
		//            *
		//           /|\
		//          / | \
		//         /  |  \
		//     e1 /   |   \ e3
		//       /    * v4 \
		//      /   /   \   \
		//     / /         \ \
		//    *---------------*
		//  v2       e2        v3
		//

		int e1 = chePtr->m_edges_face[i];
		int e2 = chePtr->edge(e1).m_he_next;
		int e3 = chePtr->edge(e2).m_he_next;

		// new half edges - push 6 new edges
		int ie14_idx = ne_new_walk++;
		int ie41_idx = ne_new_walk++;
		int ie24_idx = ne_new_walk++;
		int ie42_idx = ne_new_walk++;
		int ie34_idx = ne_new_walk++;
		int ie43_idx = ne_new_walk++;

		chePtr->m_edges.resize(ne_new_walk);

		// initialize the new vertex
		int iv1 = f[chePtr->edge(e1).m_face]->GetVertex(0);
		int iv2 = f[chePtr->edge(e1).m_face]->GetVertex(1);
		int iv3 = f[chePtr->edge(e1).m_face]->GetVertex(2);
		int iv4 = nv_new_walk;
		Vector3d v1 (v[3*iv1], v[3*iv1+1], v[3*iv1+2]);
		Vector3d v2 (v[3*iv2], v[3*iv2+1], v[3*iv2+2]);
		Vector3d v3 (v[3*iv3], v[3*iv3+1], v[3*iv3+2]);

		Vector3d n1 (vn[3*iv1], vn[3*iv1+1], vn[3*iv1+2]);
		Vector3d n2 (vn[3*iv2], vn[3*iv2+1], vn[3*iv2+2]);
		Vector3d n3 (vn[3*iv3], vn[3*iv3+1], vn[3*iv3+2]);

		Vector3d v4, nv4;
		InitializePosition (v4, nv4, v1, v2, v3, n1, n2, n3);

		v_new[3*iv4]   = v4.x;
		v_new[3*iv4+1] = v4.y;
		v_new[3*iv4+2] = v4.z;
		nv_new_walk++;

		// update the links
		chePtr->edge(e1).m_he_next  = ie24_idx;
		chePtr->edge(ie24_idx).m_he_next = ie41_idx;
		chePtr->edge(ie41_idx).m_he_next = e1;

		chePtr->edge(e2).m_he_next  = ie34_idx;
		chePtr->edge(ie34_idx).m_he_next = ie42_idx;
		chePtr->edge(ie42_idx).m_he_next = e2;

		chePtr->edge(e3).m_he_next  = ie14_idx;
		chePtr->edge(ie14_idx).m_he_next = ie43_idx;
		chePtr->edge(ie43_idx).m_he_next = e3;

		chePtr->edge(ie14_idx).m_pair = ie41_idx;
		chePtr->edge(ie41_idx).m_pair = ie14_idx;
		chePtr->edge(ie24_idx).m_pair = ie42_idx;
		chePtr->edge(ie42_idx).m_pair = ie24_idx;
		chePtr->edge(ie34_idx).m_pair = ie43_idx;
		chePtr->edge(ie43_idx).m_pair = ie34_idx;

		chePtr->edge(ie14_idx).m_v_begin = iv1;	chePtr->edge(ie14_idx).m_v_end = iv4;
		chePtr->edge(ie41_idx).m_v_begin = iv4;	chePtr->edge(ie41_idx).m_v_end = iv1;
		chePtr->edge(ie24_idx).m_v_begin = iv2;	chePtr->edge(ie24_idx).m_v_end = iv4;
		chePtr->edge(ie42_idx).m_v_begin = iv4;	chePtr->edge(ie42_idx).m_v_end = iv2;
		chePtr->edge(ie34_idx).m_v_begin = iv3;	chePtr->edge(ie34_idx).m_v_end = iv4;
		chePtr->edge(ie43_idx).m_v_begin = iv4;	chePtr->edge(ie43_idx).m_v_end = iv3;

		// update the faces
		f_new[3*i] = new Face ();
		f_new[3*i]->SetTriangle (iv1, iv2, iv4);
		f_new[3*i+1] = new Face ();
		f_new[3*i+1]->SetTriangle (iv2, iv3, iv4);
		f_new[3*i+2] = new Face ();
		f_new[3*i+2]->SetTriangle (iv3, iv1, iv4);

		chePtr->edge(e1).m_face = 3*i;
		chePtr->edge(e2).m_face = 3*i+1;
		chePtr->edge(e3).m_face = 3*i+2;

		// edges_face
		chePtr->m_edges_face[3*i]   = e1;
		chePtr->m_edges_face[3*i+1] = e2;
		chePtr->m_edges_face[3*i+2] = e3;

		// edges vertex
		chePtr->m_edges_vertex[iv1] = e1;
		chePtr->m_edges_vertex[iv2] = e2;
		chePtr->m_edges_vertex[iv3] = e3;
		chePtr->m_edges_vertex[iv4] = ie41_idx;
	}

	model->m_pMesh->m_nFaces = nf_new;
	model->m_pMesh->m_nVertices = nv_new;
	model->m_pMesh->m_pVertices = v_new;
	model->m_pMesh->m_pFaces = f_new;

	//DeleteAngles ();

	return true;
}

void MeshAlgoSubdivisionKarbacher::InitializePosition (Vector3d &pos, Vector3d &npos,
					       Vector3d v1, Vector3d v2, Vector3d v3,
					       Vector3d n1, Vector3d n2, Vector3d n3)
{
	pos.Barycenter (v1, v2, v3);
	npos = n1 + n2; npos += n3;
	npos.Normalize ();

	Vector3d posv1 = v1 - pos;
	Vector3d posv2 = v2 - pos;
	Vector3d posv3 = v3 - pos;

	float cosalpha1 = npos * n1;
	float cosbeta1  = (npos * posv1) / posv1.getLength ();
	float delta1 = (npos * posv1) + posv1.getLength () * sqrt (((1 - cosbeta1 * cosbeta1) * (1 - cosalpha1)) / (1 + cosalpha1));

	float cosalpha2 = npos * n2;
	float cosbeta2  = (npos * posv2) / posv2.getLength ();
	float delta2 = (npos * posv2) + posv2.getLength () * sqrt (((1 - cosbeta2 * cosbeta2) * (1 - cosalpha2)) / (1 + cosalpha2));

	float cosalpha3 = npos * n3;
	float cosbeta3  = (npos * posv3) / posv3.getLength ();
	float delta3 = (npos * posv3) + posv3.getLength () * sqrt (((1 - cosbeta3 * cosbeta3) * (1 - cosalpha3)) / (1 + cosalpha3));

	delta1 /= 3.0;
	delta2 /= 3.0;
	delta3 /= 3.0;
	n1.x *= delta1;	n1.y *= delta1; n1.z *= delta1;
	n2.x *= delta2;	n2.y *= delta2; n2.z *= delta2;
	n3.x *= delta3;	n3.y *= delta3; n3.z *= delta3;

	pos.x += n1.x + n2.x + n3.x;
	pos.y += n1.y + n2.y + n3.y;
	pos.z += n1.z + n2.z + n3.z;
}

void MeshAlgoSubdivisionKarbacher::DeleteAngles (void)
{
	int nf = m_pModel->m_pMesh->m_nFaces;
	float *v = m_pModel->m_pMesh->m_pVertices;
	Che_mesh *chePtr = m_pModel->GetCheMesh();
	int m_ne = chePtr->m_ne;

	m_ne = 3*nf;
	for (int i=0; i<m_ne; i++)
	{
		//
		//  v1      v3
		//  *-------*
		//  |      /|
		//  |     / |
		//  |    /  |
		//  |   /   |
		//  |  /    |
		//  | /     |
		//  *-------*
		//  v4      v2
		//
		int e = i;
		if (chePtr->edge(e).m_pair < 0)
			continue;

		int e_next = chePtr->edge(e).m_he_next;
		int e_pair = chePtr->edge(e).m_pair;
		int e_pair_next = chePtr->edge(e_pair).m_he_next;

		int iv1 = chePtr->edge(e_next).m_v_end;
		int iv2 = chePtr->edge(e_pair_next).m_v_end;
		int iv3 = chePtr->edge(e).m_v_end;
		int iv4 = chePtr->edge(e).m_v_begin;

		Vector3d v1 (v[3*iv1], v[3*iv1+1], v[3*iv1+2]);
		Vector3d v2 (v[3*iv2], v[3*iv2+1], v[3*iv2+2]);
		Vector3d v3 (v[3*iv3], v[3*iv3+1], v[3*iv3+2]);
		Vector3d v4 (v[3*iv4], v[3*iv4+1], v[3*iv4+2]);
		Vector3d v13 = v3 - v1;
		Vector3d v14 = v4 - v1;
		Vector3d v23 = v3 - v2;
		Vector3d v24 = v4 - v2;

		//Vector3d n1, n2;
		//n1.normale_triangle (v4, v3, v1);
		//n2.normale_triangle (v2, v3, v4);
		if (	v13 * v14 < 0.0
			&&	v23 * v24 < 0.0	)
		{
			m_pModel->edge_flip (e);
		}
	}
}
