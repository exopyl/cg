#include <cmath>
#include <vector>
#include <cstring>

#include "subdivision_sqrt3.h"
#include "subdivision_common.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//
// √3 subdivision (Kobbelt 2000) :
//
//   Step 1 : compute one centroid per old face. The new vertex index for face
//            f is (nv + f).
//
//   Step 2 : compute new positions for the original vertices.
//            - smoothing off : keep them as-is ;
//            - smoothing on  : Kobbelt's α(n) mask on interior, V' = 4/27 V_L
//                              + 19/27 V + 4/27 V_R on boundary.
//
//   Step 3 : build the new face list. For each old face f with edges
//            (e0, e1, e2) and centroid M = nv + f :
//              - if edge e_i = (v_i, v_{i+1}) is interior, paired with edge
//                of face f' having centroid M' = nv + f' :
//                    emit triangle (v_i, M, M')          <- "flipped" edge
//              - if e_i is boundary :
//                    emit triangle (v_i, v_{i+1}, M)     <- kept original edge
//
//            Each old face produces exactly 3 new faces (one per edge), so
//            nf_new = 3 * nf.
//
bool MeshAlgoSubdivisionSqrt3::Apply (Mesh_half_edge *model)
{
	if (!model || !model->m_pMesh) return false;

	Che_mesh *che = model->GetCheMesh ();
	if (!che) return false;

	const int nv = (int)model->m_pMesh->m_nVertices;
	const int nf = (int)model->m_pMesh->m_nFaces;
	const float *v_old = model->m_pMesh->m_pVertices.data();
	if (nv <= 0 || nf <= 0 || !v_old) return false;

	//
	// Step 1 : centroid per old face.
	//
	std::vector<float> centroids (3 * nf);
	for (int f = 0; f < nf; ++f)
	{
		Face *F = model->m_pMesh->m_pFaces[f];
		int a = F->GetVertex(0), b = F->GetVertex(1), c = F->GetVertex(2);
		for (int k = 0; k < 3; ++k)
		{
			centroids[3*f+k] = (v_old[3*a+k] + v_old[3*b+k] + v_old[3*c+k]) / 3.0f;
		}
	}

	const int nv_new = nv + nf;
	std::vector<float> v_new (3 * nv_new, 0.0f);

	//
	// Step 2 : new positions for the original vertices.
	//
	if (!m_smoothOriginal)
	{
		std::memcpy (v_new.data(), v_old, 3 * nv * sizeof(float));
	}
	else
	{
		std::vector<int> ring;
		bool is_bnd; int bL, bR;
		for (int v = 0; v < nv; ++v)
		{
			collectVertexRing (che, v, ring, is_bnd, bL, bR);

			if (ring.empty())
			{
				v_new[3*v+0] = v_old[3*v+0];
				v_new[3*v+1] = v_old[3*v+1];
				v_new[3*v+2] = v_old[3*v+2];
				continue;
			}

			if (is_bnd)
			{
				if (bL < 0 || bR < 0)
				{
					v_new[3*v+0] = v_old[3*v+0];
					v_new[3*v+1] = v_old[3*v+1];
					v_new[3*v+2] = v_old[3*v+2];
					continue;
				}
				// V' = (4/27) V_L + (19/27) V + (4/27) V_R
				const double wL = 4.0 / 27.0;
				const double wV = 19.0 / 27.0;
				const double wR = 4.0 / 27.0;
				for (int k = 0; k < 3; ++k)
				{
					double Vk  = v_old[3*v +k];
					double VLk = v_old[3*bL+k];
					double VRk = v_old[3*bR+k];
					v_new[3*v+k] = (float)(wL * VLk + wV * Vk + wR * VRk);
				}
			}
			else
			{
				// α(n) = (4 - 2 cos(2π/n)) / 9
				int n = (int)ring.size();
				double alpha = (4.0 - 2.0 * std::cos (2.0 * M_PI / n)) / 9.0;
				double sumX = 0, sumY = 0, sumZ = 0;
				for (int j = 0; j < n; ++j)
				{
					int nb = ring[j];
					sumX += v_old[3*nb+0];
					sumY += v_old[3*nb+1];
					sumZ += v_old[3*nb+2];
				}
				double Vx = v_old[3*v+0];
				double Vy = v_old[3*v+1];
				double Vz = v_old[3*v+2];
				double w  = 1.0 - alpha;
				double s  = alpha / n;
				v_new[3*v+0] = (float)(w * Vx + s * sumX);
				v_new[3*v+1] = (float)(w * Vy + s * sumY);
				v_new[3*v+2] = (float)(w * Vz + s * sumZ);
			}
		}
	}

	// Append centroids.
	std::memcpy (v_new.data() + 3 * nv, centroids.data(), 3 * nf * sizeof(float));

	//
	// Step 3 : build the new face list.
	//
	const int nf_new = 3 * nf;
	std::vector<unsigned int> faces (3 * nf_new);

	int face_walk = 0;
	for (int f = 0; f < nf; ++f)
	{
		int e0 = che->m_edges_face[f];
		int e1 = che->edge(e0).m_he_next;
		int e2 = che->edge(e1).m_he_next;
		int es[3] = { e0, e1, e2 };

		int M = nv + f;

		for (int i = 0; i < 3; ++i)
		{
			int e = es[i];
			int v_i = che->edge(e).m_v_begin;
			int v_j = che->edge(e).m_v_end;
			int p   = che->edge(e).m_pair;

			unsigned int *p_face = faces.data() + 3 * face_walk;
			if (p < 0)
			{
				// Boundary edge : keep as triangle (v_i, v_j, M)
				p_face[0] = (unsigned)v_i;
				p_face[1] = (unsigned)v_j;
				p_face[2] = (unsigned)M;
			}
			else
			{
				// Interior edge : flipped, emit triangle (v_i, M, M_other)
				int f_other = che->edge(p).m_face;
				int M_other = nv + f_other;
				p_face[0] = (unsigned)v_i;
				p_face[1] = (unsigned)M;
				p_face[2] = (unsigned)M_other;
			}
			++face_walk;
		}
	}

	//
	// Step 4 : install new mesh data.
	//
	if (model->m_pMesh->m_pFaces)
	{
		for (unsigned int i = 0; i < model->m_pMesh->m_nFaces; ++i)
			delete model->m_pMesh->m_pFaces[i];
	}

	model->m_pMesh->SetVertices ((unsigned)nv_new, v_new.data());
	model->m_pMesh->SetFaces ((unsigned)nf_new, 3, faces.data());

	model->create_half_edge ();

	return true;
}
