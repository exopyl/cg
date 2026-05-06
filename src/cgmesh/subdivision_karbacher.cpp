#include <vector>
#include <cstring>

#include "subdivision_karbacher.h"

//
// Karbacher 1->3 subdivision : insert one centroid per triangle, displaced
// along the averaged normal direction. Original vertices keep their position.
//
//             v1
//            *
//           /|\
//          / | \
//         /  |  \
//        /   * v4\
//       /  /   \  \
//      / /       \ \
//     *-------------*
//   v2               v3
//
// New vertex v4 = barycenter(v1,v2,v3) + normal-based displacement
//                 (computed by InitializePosition).
//
// Each old face (v1, v2, v3) becomes 3 new faces :
//   (v1, v2, v4), (v2, v3, v4), (v3, v1, v4)
//
bool MeshAlgoSubdivisionKarbacher::Apply (Mesh_half_edge *model)
{
	if (!model || !model->m_pMesh) return false;

	m_pModel = model;
	const int nv = (int)model->m_pMesh->m_nVertices;
	const int nf = (int)model->m_pMesh->m_nFaces;
	const float *v  = model->m_pMesh->m_pVertices.data();
	const float *vn = model->m_pMesh->m_pVertexNormals.data();
	Face **f = model->m_pMesh->m_pFaces;
	if (nv <= 0 || nf <= 0 || !v || !vn || !f) return false;

	const int nv_new = nv + nf;
	const int nf_new = 3 * nf;

	// Build new vertex array : copy originals, then append nf centroids.
	std::vector<float> v_new (3 * nv_new);
	std::memcpy (v_new.data(), v, 3 * nv * sizeof(float));

	// Build new face index list (3 triangles per old face).
	std::vector<unsigned int> faces (3 * nf_new);

	for (int i = 0; i < nf; ++i)
	{
		int iv1 = f[i]->GetVertex(0);
		int iv2 = f[i]->GetVertex(1);
		int iv3 = f[i]->GetVertex(2);
		int iv4 = nv + i;   // new centroid vertex for face i

		Vector3d V1 (v[3*iv1], v[3*iv1+1], v[3*iv1+2]);
		Vector3d V2 (v[3*iv2], v[3*iv2+1], v[3*iv2+2]);
		Vector3d V3 (v[3*iv3], v[3*iv3+1], v[3*iv3+2]);

		Vector3d N1 (vn[3*iv1], vn[3*iv1+1], vn[3*iv1+2]);
		Vector3d N2 (vn[3*iv2], vn[3*iv2+1], vn[3*iv2+2]);
		Vector3d N3 (vn[3*iv3], vn[3*iv3+1], vn[3*iv3+2]);

		Vector3d V4, NV4;
		InitializePosition (V4, NV4, V1, V2, V3, N1, N2, N3);

		v_new[3*iv4]   = V4.x;
		v_new[3*iv4+1] = V4.y;
		v_new[3*iv4+2] = V4.z;

		unsigned int *p0 = faces.data() + 3 * (3 * i + 0);
		unsigned int *p1 = faces.data() + 3 * (3 * i + 1);
		unsigned int *p2 = faces.data() + 3 * (3 * i + 2);
		p0[0] = (unsigned)iv1; p0[1] = (unsigned)iv2; p0[2] = (unsigned)iv4;
		p1[0] = (unsigned)iv2; p1[1] = (unsigned)iv3; p1[2] = (unsigned)iv4;
		p2[0] = (unsigned)iv3; p2[1] = (unsigned)iv1; p2[2] = (unsigned)iv4;
	}

	// Delete the existing per-Face objects ; SetFaces frees the outer array.
	if (model->m_pMesh->m_pFaces)
	{
		for (unsigned int i = 0; i < model->m_pMesh->m_nFaces; ++i)
			delete model->m_pMesh->m_pFaces[i];
	}

	model->m_pMesh->SetVertices ((unsigned)nv_new, v_new.data());
	model->m_pMesh->SetFaces ((unsigned)nf_new, 3, faces.data());

	// Invalidate the cached half-edge structure.
	model->create_half_edge ();

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
	float *v = m_pModel->m_pMesh->m_pVertices.data();
	Che_mesh *chePtr = m_pModel->GetCheMesh();
	int m_ne = chePtr->m_ne;

	m_ne = 3*nf;
	for (int i=0; i<m_ne; i++)
	{
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

		if (	v13 * v14 < 0.0
			&&	v23 * v24 < 0.0	)
		{
			m_pModel->edge_flip (e);
		}
	}
}
