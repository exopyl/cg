#include <assert.h>
#include <vector>

#include "subdivision_loop.h"

/**
* Subdivision of a mesh by Loop's method.
*
\verbatim
           *                     *
          / \                   / \
         /   \                 /   \
        /     \               /     \
       /       \     ->      *-------*
      /         \           / \     / \
     /           \  	   /   \   /   \
    /             \ 	  /     \ /     \
   *---------------*	 *-------*-------*

\endverbatim
*/
bool MeshAlgoSubdivisionLoop::Apply (Mesh_half_edge *model)
{
	int nf = model->m_pMesh->m_nFaces;
	int nv = model->m_pMesh->m_nVertices;
	float *v = model->m_pMesh->m_pVertices;

	int i, ne = 3*nf;

	int nv_new_walk = nv;
	int nv_new = nv+ne/2; // approximation of the new number of vertices
	int nf_new = 4*nf;
	Face **f_new = (Face**)malloc(3*nf_new*sizeof(Face*));
	float *v_new = (float*)malloc(3*nv_new*sizeof(float));
	v_new = (float*)memcpy ((void*)v_new, (const void*)v, 3*nv*sizeof(float));

	// Build a local vector of new edges. All m_pair and m_he_next
	// fields are indices into this vector.
	std::vector<Che_edge> edges;
	edges.reserve(12 * nf);

	// For each old edge that has been processed, store the index (into
	// the local 'edges' vector) of its first replacement new edge.
	// The edge at old_edge_to_new[oe] replaces the first half, and
	// old_edge_to_new[oe]+1 replaces the second half.
	// -1 means not yet processed.
	Che_mesh *chePtr = model->GetCheMesh();
	std::vector<int> old_edge_to_new(chePtr->m_edges.size(), -1);

	// create the new triangles and link the new edges
	for (i=0; i<nf; i++)
	{
		//
		//             v1
		//            *
		//           / \
		//       e1 /   \ e6
		//         / e26 \
		//     v2 *-------* v6
		//       / \     / \
		//   e2 /   \   /   \ e5
		//     /  e42\ /e64  \
		//    *-------*-------*
		//  v3   e3  v4   e4   v5
		//

		// Allocate 12 new edges in the local vector
		int base = (int)edges.size();
		edges.resize(base + 12);

		// Indices into the local edges vector
		int ie1  = base + 0;
		int ie2  = base + 1;
		int ie3  = base + 2;
		int ie4  = base + 3;
		int ie5  = base + 4;
		int ie6  = base + 5;
		int ie26 = base + 6;
		int ie62 = base + 7;
		int ie42 = base + 8;
		int ie24 = base + 9;
		int ie64 = base + 10;
		int ie46 = base + 11;

		// internal links between the new half edges
		edges[ie1].m_he_next  = ie26;
		edges[ie26].m_he_next = ie6;
		edges[ie6].m_he_next  = ie1;

		edges[ie3].m_he_next  = ie42;
		edges[ie42].m_he_next = ie2;
		edges[ie2].m_he_next  = ie3;

		edges[ie5].m_he_next  = ie64;
		edges[ie64].m_he_next = ie4;
		edges[ie4].m_he_next  = ie5;

		edges[ie62].m_he_next = ie24;
		edges[ie24].m_he_next = ie46;
		edges[ie46].m_he_next = ie62;

		edges[ie26].m_pair = ie62;
		edges[ie62].m_pair = ie26;
		edges[ie42].m_pair = ie24;
		edges[ie24].m_pair = ie42;
		edges[ie64].m_pair = ie46;
		edges[ie46].m_pair = ie64;

		// for the algorithm
		edges[ie1].m_visited = true;
		edges[ie2].m_visited = true;
		edges[ie3].m_visited = true;
		edges[ie4].m_visited = true;
		edges[ie5].m_visited = true;
		edges[ie6].m_visited = true;

		// old half edges (indices into model's m_edges)
		int oe12 = chePtr->m_edges_face[i];
		int oe23 = chePtr->edge(oe12).m_he_next;
		int oe31 = chePtr->edge(oe23).m_he_next;
		assert (chePtr->edge(oe31).m_he_next == oe12);

		// current vertices
		int v1 = chePtr->edge(oe12).m_v_begin;
		int v2 = chePtr->edge(oe23).m_v_begin;
		int v3 = chePtr->edge(oe31).m_v_begin;

		//
		// half edge e12
		//
		{
			int oe12_pair = chePtr->edge(oe12).m_pair;
			if ( oe12_pair < 0 || old_edge_to_new[oe12_pair] < 0 )
			{
				// pair not yet processed: create a new midpoint vertex
				v_new[3*nv_new_walk]   = (v[3*v1]+v[3*v2])/2.0;
				v_new[3*nv_new_walk+1] = (v[3*v1+1]+v[3*v2+1])/2.0;
				v_new[3*nv_new_walk+2] = (v[3*v1+2]+v[3*v2+2])/2.0;

				edges[ie1].m_v_begin = v1;
				edges[ie1].m_v_end   = nv_new_walk;
				edges[ie2].m_v_begin = nv_new_walk;
				edges[ie2].m_v_end   = v2;

				nv_new_walk++;

				if (oe12_pair < 0)
				{
					edges[ie1].m_pair = -1;
					edges[ie2].m_pair = -1;
				}
				else // pair exists but not yet processed
				{
					// will be linked when pair face is processed
					edges[ie1].m_pair = -1;
					edges[ie2].m_pair = -1;
				}
			}
			else // pair was already processed
			{
				int pair_first = old_edge_to_new[oe12_pair]; // pair's first replacement
				// The pair's edge splits are in reverse order relative to ours.
				// pair_first is the half closest to v2, pair_first+1 is closest to v1.
				// So: ie1 pairs with pair_first (which goes v2->mid),
				//     ie2 pairs with pair_first+1 (which goes mid->v1)
				// Wait - need to think about orientation.
				// Old edge oe12 goes v1->v2. Its pair oe12_pair goes v2->v1.
				// When oe12_pair was split: pair_first goes v2->mid, pair_first+1 goes mid->v1.
				// Our ie1 goes v1->mid, ie2 goes mid->v2.
				// So ie1 (v1->mid) pairs with pair_first+1 (mid->v1): correct opposing directions.
				// And ie2 (mid->v2) pairs with pair_first (v2->mid): correct opposing directions.
				edges[ie1].m_pair = pair_first + 1;
				edges[ie2].m_pair = pair_first;
				edges[pair_first + 1].m_pair = ie1;
				edges[pair_first].m_pair = ie2;

				edges[ie1].m_v_begin = v1;
				edges[ie1].m_v_end   = edges[pair_first + 1].m_v_begin;
				edges[ie2].m_v_begin = edges[pair_first + 1].m_v_begin;
				edges[ie2].m_v_end   = v2;
			}
			old_edge_to_new[oe12] = ie1;
		}

		//
		// half edge e23
		//
		{
			int oe23_pair = chePtr->edge(oe23).m_pair;
			if ( oe23_pair < 0 || old_edge_to_new[oe23_pair] < 0 )
			{
				v_new[3*nv_new_walk]   = (v[3*v2]+v[3*v3])/2.0;
				v_new[3*nv_new_walk+1] = (v[3*v2+1]+v[3*v3+1])/2.0;
				v_new[3*nv_new_walk+2] = (v[3*v2+2]+v[3*v3+2])/2.0;

				edges[ie3].m_v_begin = v2;
				edges[ie3].m_v_end   = nv_new_walk;
				edges[ie4].m_v_begin = nv_new_walk;
				edges[ie4].m_v_end   = v3;

				nv_new_walk++;

				if (oe23_pair < 0)
				{
					edges[ie3].m_pair = -1;
					edges[ie4].m_pair = -1;
				}
				else
				{
					edges[ie3].m_pair = -1;
					edges[ie4].m_pair = -1;
				}
			}
			else
			{
				int pair_first = old_edge_to_new[oe23_pair];
				edges[ie3].m_pair = pair_first + 1;
				edges[ie4].m_pair = pair_first;
				edges[pair_first + 1].m_pair = ie3;
				edges[pair_first].m_pair = ie4;

				edges[ie3].m_v_begin = v2;
				edges[ie3].m_v_end   = edges[pair_first + 1].m_v_begin;
				edges[ie4].m_v_begin = edges[pair_first + 1].m_v_begin;
				edges[ie4].m_v_end   = v3;
			}
			old_edge_to_new[oe23] = ie3;
		}

		//
		// half edge e31
		//
		{
			int oe31_pair = chePtr->edge(oe31).m_pair;
			if ( oe31_pair < 0 || old_edge_to_new[oe31_pair] < 0 )
			{
				v_new[3*nv_new_walk]   = (v[3*v3]+v[3*v1])/2.0;
				v_new[3*nv_new_walk+1] = (v[3*v3+1]+v[3*v1+1])/2.0;
				v_new[3*nv_new_walk+2] = (v[3*v3+2]+v[3*v1+2])/2.0;

				edges[ie5].m_v_begin = v3;
				edges[ie5].m_v_end   = nv_new_walk;
				edges[ie6].m_v_begin = nv_new_walk;
				edges[ie6].m_v_end   = v1;

				nv_new_walk++;

				if (oe31_pair < 0)
				{
					edges[ie5].m_pair = -1;
					edges[ie6].m_pair = -1;
				}
				else
				{
					edges[ie5].m_pair = -1;
					edges[ie6].m_pair = -1;
				}
			}
			else
			{
				int pair_first = old_edge_to_new[oe31_pair];
				edges[ie5].m_pair = pair_first + 1;
				edges[ie6].m_pair = pair_first;
				edges[pair_first + 1].m_pair = ie5;
				edges[pair_first].m_pair = ie6;

				edges[ie5].m_v_begin = v3;
				edges[ie5].m_v_end   = edges[pair_first + 1].m_v_begin;
				edges[ie6].m_v_begin = edges[pair_first + 1].m_v_begin;
				edges[ie6].m_v_end   = v1;
			}
			old_edge_to_new[oe31] = ie5;
		}

		// create the new triangles
		f_new[4*i] = new Face ();
		f_new[4*i]->SetTriangle (edges[ie1].m_v_begin, edges[ie2].m_v_begin, edges[ie6].m_v_begin);
		f_new[4*i+1] = new Face ();
		f_new[4*i+1]->SetTriangle (edges[ie2].m_v_begin, edges[ie3].m_v_begin, edges[ie4].m_v_begin);
		f_new[4*i+2] = new Face ();
		f_new[4*i+2]->SetTriangle (edges[ie4].m_v_begin, edges[ie5].m_v_begin, edges[ie6].m_v_begin);
		f_new[4*i+3] = new Face ();
		f_new[4*i+3]->SetTriangle (edges[ie2].m_v_begin, edges[ie4].m_v_begin, edges[ie6].m_v_begin);

		// connect the vertices to the half edges
		edges[ie42].m_v_begin = edges[ie4].m_v_begin;
		edges[ie42].m_v_end   = edges[ie2].m_v_begin;

		edges[ie24].m_v_begin = edges[ie2].m_v_begin;
		edges[ie24].m_v_end   = edges[ie4].m_v_begin;

		edges[ie64].m_v_begin = edges[ie6].m_v_begin;
		edges[ie64].m_v_end   = edges[ie4].m_v_begin;

		edges[ie46].m_v_begin = edges[ie4].m_v_begin;
		edges[ie46].m_v_end   = edges[ie6].m_v_begin;

		edges[ie26].m_v_begin = edges[ie2].m_v_begin;
		edges[ie26].m_v_end   = edges[ie6].m_v_begin;

		edges[ie62].m_v_begin = edges[ie6].m_v_begin;
		edges[ie62].m_v_end   = edges[ie2].m_v_begin;
	}

	nv_new = nv_new_walk;


	model->m_pMesh->m_nVertices = nv_new;
	model->m_pMesh->m_nFaces = nf_new;

	model->m_pMesh->m_pVertices = v_new;
	model->m_pMesh->m_pFaces = f_new;

	return true;
}
