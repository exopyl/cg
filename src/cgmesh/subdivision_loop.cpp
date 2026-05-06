#include <assert.h>
#include <cmath>
#include <vector>

#include "subdivision_loop.h"

//
// Walk the 1-ring of vertex `v`, returning the (unique) neighbor vertices and
// flagging boundary topology.
//
// For an interior manifold vertex : `neighbors` lists the n vertices around v
// in walk order ; `is_boundary` is false ; `bnd_left`/`bnd_right` are -1.
//
// For a boundary manifold vertex : `neighbors` lists all neighbors ; `is_boundary`
// is true ; `bnd_left` and `bnd_right` are the two neighbors connected to v by
// boundary half-edges (i.e. the endpoints of v's two incident boundary edges).
//
// For an isolated vertex (no incident edge) : `neighbors` is empty.
//
static void
collectVertexRing (Che_mesh *che, int v,
                   std::vector<int> &neighbors,
                   bool &is_boundary,
                   int &bnd_left, int &bnd_right)
{
	neighbors.clear ();
	is_boundary = false;
	bnd_left = -1;
	bnd_right = -1;

	int e0 = che->m_edges_vertex[v];
	if (e0 < 0) return;

	// Forward walk : e -> next.next.pair
	int e = e0;
	bool hit_forward = false;
	int forward_third = -1;
	do
	{
		neighbors.push_back (che->edge(e).m_v_end);
		int n1 = che->edge(e).m_he_next;
		int n2 = che->edge(n1).m_he_next;   // n2 ends at v
		int p  = che->edge(n2).m_pair;
		if (p < 0)
		{
			hit_forward = true;
			// The third vertex of this triangle (= n1.v_end = n2.v_begin) is also a
			// neighbor of v, and is the boundary endpoint on the forward side.
			forward_third = che->edge(n2).m_v_begin;
			break;
		}
		e = p;
	} while (e != e0);

	if (!hit_forward)
	{
		// Closed ring : interior vertex, all neighbors collected.
		return;
	}

	is_boundary = true;
	neighbors.push_back (forward_third);
	bnd_right = forward_third;

	// Backward walk : e -> pair(e).next
	int e_back = e0;
	while (true)
	{
		int p_back = che->edge(e_back).m_pair;
		if (p_back < 0)
		{
			// e_back is itself a boundary outgoing edge ; the neighbor at its v_end
			// is the boundary endpoint on the backward side.
			bnd_left = che->edge(e_back).m_v_end;
			break;
		}
		int prev_e = che->edge(p_back).m_he_next;
		// prev_e starts at v (it's a previous outgoing edge in the fan)
		int prev_neighbor = che->edge(prev_e).m_v_end;
		neighbors.insert (neighbors.begin(), prev_neighbor);
		e_back = prev_e;
	}
}

//
// Loop subdivision (1-to-4) with optional Warren smoothing masks.
//
//             v0
//            *
//           / \
//        m20   m01
//         /     \
//        *-------*
//       / \     / \
//      /   \   /   \
//   m12     m12 ... wait, three midpoints :
//     m01 (on v0-v1), m12 (on v1-v2), m20 (on v2-v0)
//
//             v0
//            *
//           / \
//          /   \
//      m20*-----*m01
//        / \   / \
//       /   \ /   \
//      *-----*-----*
//     v2    m12    v1
//
// Each old face (v0,v1,v2) becomes 4 new faces :
//   (v0,  m01, m20)
//   (m01, v1,  m12)
//   (m20, m12, v2 )
//   (m01, m12, m20)   <- center, same orientation
//
bool MeshAlgoSubdivisionLoop::Apply (Mesh_half_edge *model)
{
	if (!model || !model->m_pMesh) return false;

	Che_mesh *che = model->GetCheMesh ();
	if (!che) return false;

	const int nv = (int)model->m_pMesh->m_nVertices;
	const int nf = (int)model->m_pMesh->m_nFaces;
	const float *v_old = model->m_pMesh->m_pVertices;
	if (nv <= 0 || nf <= 0 || !v_old) return false;

	const int ne = che->m_ne;

	//
	// Step 1 : assign one midpoint vertex per undirected edge.
	//
	std::vector<int> edge_to_mid (ne, -1);
	std::vector<float> mid_pos;        // 3 floats per midpoint
	mid_pos.reserve (3 * ne);

	int next_mid_index = nv;
	for (int e = 0; e < ne; ++e)
	{
		if (edge_to_mid[e] >= 0) continue;

		int v0 = che->edge(e).m_v_begin;
		int v1 = che->edge(e).m_v_end;
		int p  = che->edge(e).m_pair;

		double mx, my, mz;
		if (p < 0 || !m_useWarrenMask)
		{
			// Boundary edge OR midpoint mode : simple edge midpoint.
			mx = (v_old[3*v0+0] + v_old[3*v1+0]) * 0.5;
			my = (v_old[3*v0+1] + v_old[3*v1+1]) * 0.5;
			mz = (v_old[3*v0+2] + v_old[3*v1+2]) * 0.5;
		}
		else
		{
			// Interior edge with Warren mask :
			//   M = (3/8)(V0 + V1) + (1/8)(V2 + V3)
			int v2 = che->edge(che->edge(e).m_he_next).m_v_end;
			int v3 = che->edge(che->edge(p).m_he_next).m_v_end;

			mx = (3.0/8.0) * (v_old[3*v0+0] + v_old[3*v1+0])
			   + (1.0/8.0) * (v_old[3*v2+0] + v_old[3*v3+0]);
			my = (3.0/8.0) * (v_old[3*v0+1] + v_old[3*v1+1])
			   + (1.0/8.0) * (v_old[3*v2+1] + v_old[3*v3+1]);
			mz = (3.0/8.0) * (v_old[3*v0+2] + v_old[3*v1+2])
			   + (1.0/8.0) * (v_old[3*v2+2] + v_old[3*v3+2]);
		}

		mid_pos.push_back ((float)mx);
		mid_pos.push_back ((float)my);
		mid_pos.push_back ((float)mz);

		int idx = next_mid_index++;
		edge_to_mid[e] = idx;
		if (p >= 0) edge_to_mid[p] = idx;
	}

	const int n_mid = next_mid_index - nv;
	const int nv_new = nv + n_mid;

	//
	// Step 2 : compute new positions for the original vertices.
	//
	// Without Warren : keep their position as-is.
	// With Warren    : Loop / Warren stencils for interior + boundary.
	//
	std::vector<float> v_new (3 * nv_new, 0.0f);

	if (!m_useWarrenMask)
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
				// Isolated vertex : keep original.
				v_new[3*v+0] = v_old[3*v+0];
				v_new[3*v+1] = v_old[3*v+1];
				v_new[3*v+2] = v_old[3*v+2];
				continue;
			}

			if (is_bnd)
			{
				// Boundary : V' = 3/4 V + 1/8 (V_L + V_R)
				if (bL < 0 || bR < 0)
				{
					// Degenerate boundary detection : keep original.
					v_new[3*v+0] = v_old[3*v+0];
					v_new[3*v+1] = v_old[3*v+1];
					v_new[3*v+2] = v_old[3*v+2];
					continue;
				}
				for (int k = 0; k < 3; ++k)
				{
					double Vk  = v_old[3*v +k];
					double VLk = v_old[3*bL+k];
					double VRk = v_old[3*bR+k];
					v_new[3*v+k] = (float)(0.75 * Vk + 0.125 * (VLk + VRk));
				}
			}
			else
			{
				// Interior :
				//   beta = 3/16 if n = 3, else 3/(8n)
				//   V' = (1 - n*beta) V + beta * sum(neighbors)
				int n = (int)ring.size();
				double beta = (n == 3) ? (3.0 / 16.0) : (3.0 / (8.0 * n));
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
				double w  = 1.0 - n * beta;
				v_new[3*v+0] = (float)(w * Vx + beta * sumX);
				v_new[3*v+1] = (float)(w * Vy + beta * sumY);
				v_new[3*v+2] = (float)(w * Vz + beta * sumZ);
			}
		}
	}

	// Append midpoint positions.
	std::memcpy (v_new.data() + 3 * nv, mid_pos.data(), 3 * n_mid * sizeof(float));

	//
	// Step 3 : build new face index list (4 triangles per old face).
	//
	const int nf_new = 4 * nf;
	std::vector<unsigned int> faces (3 * nf_new);

	for (int f = 0; f < nf; ++f)
	{
		// Pick the 3 half-edges of this face in walk order.
		int e0 = che->m_edges_face[f];
		int e1 = che->edge(e0).m_he_next;
		int e2 = che->edge(e1).m_he_next;

		int v0 = che->edge(e0).m_v_begin;
		int v1 = che->edge(e1).m_v_begin;
		int v2 = che->edge(e2).m_v_begin;

		int m01 = edge_to_mid[e0];
		int m12 = edge_to_mid[e1];
		int m20 = edge_to_mid[e2];

		unsigned int *p0 = faces.data() + 3 * (4 * f + 0);
		unsigned int *p1 = faces.data() + 3 * (4 * f + 1);
		unsigned int *p2 = faces.data() + 3 * (4 * f + 2);
		unsigned int *p3 = faces.data() + 3 * (4 * f + 3);

		p0[0] = (unsigned)v0;  p0[1] = (unsigned)m01; p0[2] = (unsigned)m20;
		p1[0] = (unsigned)m01; p1[1] = (unsigned)v1;  p1[2] = (unsigned)m12;
		p2[0] = (unsigned)m20; p2[1] = (unsigned)m12; p2[2] = (unsigned)v2;
		p3[0] = (unsigned)m01; p3[1] = (unsigned)m12; p3[2] = (unsigned)m20;
	}

	//
	// Step 4 : install new mesh data.
	//
	// Delete the existing per-Face objects (SetFaces frees the outer array
	// but leaks the inner Face* — we cleanup the inner pointers ourselves).
	if (model->m_pMesh->m_pFaces)
	{
		for (unsigned int i = 0; i < model->m_pMesh->m_nFaces; ++i)
			delete model->m_pMesh->m_pFaces[i];
	}

	model->m_pMesh->SetVertices ((unsigned)nv_new, v_new.data());
	model->m_pMesh->SetFaces ((unsigned)nf_new, 3, faces.data());

	// Invalidate the cached half-edge structure ; next caller builds it fresh.
	model->create_half_edge ();

	return true;
}
