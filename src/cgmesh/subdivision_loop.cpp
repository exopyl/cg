#include <assert.h>

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
	int nf = model->m_nFaces;
	int nv = model->m_nVertices;
	float *v = model->m_pVertices;

	int i, ne = 3*nf;

	Che_edge **edges_new        = NULL;
	Che_edge **edges_vertex_new = NULL;
	Che_edge **edges_face_new   = NULL;

	int nv_new_walk = nv;
	int nv_new = nv+ne/2; // approximation of the new number of vertices
	int nf_new = 4*nf;
	Face **f_new = (Face**)malloc(3*nf_new*sizeof(Face*));
	float *v_new = (float*)malloc(3*nv_new*sizeof(float));
	v_new = (float*)memcpy ((void*)v_new, (const void*)v, 3*nv*sizeof(float));


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

		// new half edges_edge
		Che_edge *e1  = new Che_edge ();
		Che_edge *e2  = new Che_edge ();
		Che_edge *e3  = new Che_edge ();
		Che_edge *e4  = new Che_edge ();
		Che_edge *e5  = new Che_edge ();
		Che_edge *e6  = new Che_edge ();

		Che_edge *e26 = new Che_edge ();
		Che_edge *e62 = new Che_edge ();
		Che_edge *e42 = new Che_edge ();
		Che_edge *e24 = new Che_edge ();
		Che_edge *e64 = new Che_edge ();
		Che_edge *e46 = new Che_edge ();

		// internal links between the new half edges
		e1->m_he_next = e26;
		e26->m_he_next = e6;
		e6->m_he_next = e1;

		e3->m_he_next = e42;
		e42->m_he_next = e2;
		e2->m_he_next = e3;

		e5->m_he_next = e64;
		e64->m_he_next = e4;
		e4->m_he_next = e5;

		e62->m_he_next = e24;
		e24->m_he_next = e46;
		e46->m_he_next = e62;

		e26->m_pair = e62;
		e62->m_pair = e26;
		e42->m_pair = e24;
		e24->m_pair = e42;
		e64->m_pair = e46;
		e46->m_pair = e64;

		// for the algorithm
		e1->m_visited = true;
		e2->m_visited = true;
		e3->m_visited = true;
		e4->m_visited = true;
		e5->m_visited = true;
		e6->m_visited = true;

		// old half edges
		Che_edge *e12, *e23, *e31;
		e12 = model->m_edges_face[i];
		e23 = e12->m_he_next;
		e31 = e23->m_he_next;
		assert (e31->m_he_next == e12);

		// current vertices
		int v1, v2, v3;
		v1 = e12->m_v_begin;
		v2 = e23->m_v_begin;
		v3 = e31->m_v_begin;

		//
		// half edge e12
		//
		if ( e12->m_pair == NULL || e12->m_pair->m_visited == false )
		{
			// create a new vertex
			v_new[3*nv_new_walk]   = (v[3*v1]+v[3*v2])/2.0;
			v_new[3*nv_new_walk+1] = (v[3*v1+1]+v[3*v2+1])/2.0;
			v_new[3*nv_new_walk+2] = (v[3*v1+2]+v[3*v2+2])/2.0;
			
			e1->m_v_begin = v1;
			e1->m_v_end   = nv_new_walk;
			e2->m_v_begin = nv_new_walk;
			e2->m_v_end   = v2;
			
			nv_new_walk++;

			if (e12->m_pair == NULL)
			{
				e1->m_pair = e2->m_pair = NULL;
			}
			else // e12->pair->visited == false
			{
				e1->m_pair  = e12->m_pair;
				e2->m_pair  = e12->m_pair;
				e12->m_pair->m_pair = e1;
			}
		}
		else // e12->pair->visited == true
		{
			e1->m_pair = e12->m_pair;
			e2->m_pair = e12->m_pair->m_he_next;
			e12->m_pair = e1;
			e12->m_pair->m_he_next = e2;
			e1->m_v_begin = v1;
			e1->m_v_end   = e1->m_pair->m_v_end;
			e2->m_v_begin = e1->m_pair->m_v_end;
			e2->m_v_end   = v2;
		}

		//
		// half edge e23
		//
		if ( e23->m_pair == NULL || e23->m_pair->m_visited == false )
		{
			// create a new vertex
			v_new[3*nv_new_walk]   = (v[3*v2]+v[3*v3])/2.0;
			v_new[3*nv_new_walk+1] = (v[3*v2+1]+v[3*v3+1])/2.0;
			v_new[3*nv_new_walk+2] = (v[3*v2+2]+v[3*v3+2])/2.0;
			
			e3->m_v_begin = v2;
			e3->m_v_end   = nv_new_walk;
			e4->m_v_begin = nv_new_walk;
			e4->m_v_end   = v3;
			
			nv_new_walk++;

			if (e23->m_pair == NULL)
			{
				e3->m_pair = e4->m_pair = NULL;
			}
			else // e23->pair->visited == false
			{
				e3->m_pair  = e23->m_pair;
				e4->m_pair  = e23->m_pair;
				e23->m_pair->m_pair = e3;
			}
		}
		else // e23->pair->visited == true
		{
			e3->m_pair = e23->m_pair;
			e4->m_pair = e23->m_pair->m_he_next;
			e23->m_pair = e3;
			e23->m_pair->m_he_next = e4;
			e3->m_v_begin = v2;
			e3->m_v_end   = e3->m_pair->m_v_end;
			e4->m_v_begin = e3->m_pair->m_v_end;
			e4->m_v_end   = v3;
		}

		//
		// half edge e31
		//
		if ( e31->m_pair == NULL || e31->m_pair->m_visited == false )
		{
			// create a new vertex
			v_new[3*nv_new_walk]   = (v[3*v3]+v[3*v1])/2.0;
			v_new[3*nv_new_walk+1] = (v[3*v3+1]+v[3*v1+1])/2.0;
			v_new[3*nv_new_walk+2] = (v[3*v3+2]+v[3*v1+2])/2.0;
			
			e5->m_v_begin = v3;
			e5->m_v_end   = nv_new_walk;
			e6->m_v_begin = nv_new_walk;
			e6->m_v_end   = v1;
			
			nv_new_walk++;

			if (e31->m_pair == NULL)
			{
				e5->m_pair = e6->m_pair = NULL;
			}
			else // e31->m_pair->m_visited == false
			{
				e5->m_pair  = e31->m_pair;
				e6->m_pair  = e31->m_pair;
				e31->m_pair->m_pair = e5;
			}
		}
		else // e31->m_pair->m_visited == true
		{
			e5->m_pair = e31->m_pair;
			e6->m_pair = e31->m_pair->m_he_next;
			e31->m_pair = e5;
			e31->m_pair->m_he_next = e6;
			e5->m_v_begin = v3;
			e5->m_v_end   = e5->m_pair->m_v_end;
			e6->m_v_begin = e5->m_pair->m_v_end;
			e6->m_v_end   = v1;
		}

		// create the new triangles
		f_new[4*i] = new Face ();
		f_new[4*i]->SetTriangle (e1->m_v_begin, e2->m_v_begin, e6->m_v_begin);
		f_new[4*i+1] = new Face ();
		f_new[4*i+1]->SetTriangle (e2->m_v_begin, e3->m_v_begin, e4->m_v_begin);
		f_new[4*i+2] = new Face ();
		f_new[4*i+2]->SetTriangle (e4->m_v_begin, e5->m_v_begin, e6->m_v_begin);
		f_new[4*i+3] = new Face ();
		f_new[4*i+3]->SetTriangle (e2->m_v_begin, e4->m_v_begin, e6->m_v_begin);

		// connect the vertices to the half edges
		e42->m_v_begin = e4->m_v_begin;
		e42->m_v_end   = e2->m_v_begin;

		e24->m_v_begin = e2->m_v_begin;
		e24->m_v_end   = e4->m_v_begin;

		e64->m_v_begin = e6->m_v_begin;
		e64->m_v_end   = e4->m_v_begin;

		e46->m_v_begin = e4->m_v_begin;
		e46->m_v_end   = e6->m_v_begin;

		e26->m_v_begin = e2->m_v_begin;
		e26->m_v_end   = e6->m_v_begin;

		e62->m_v_begin = e6->m_v_begin;
		e62->m_v_end   = e2->m_v_begin;
	}

	nv_new = nv_new_walk;


	model->m_nVertices = nv_new;
	model->m_nFaces = nf_new;

	model->m_pVertices = v_new;
	model->m_pFaces = f_new;

	return true;
}
