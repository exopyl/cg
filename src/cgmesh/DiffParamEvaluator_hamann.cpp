#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHamann (void)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i;
	float mat[9];
	vec3 right, solution;
	
	for (i=0; i<nv; i++)
    {
		if (!m_pModel->m_topology_ok[i] || m_pModel->m_border[i])
		{
			m_pDiffParams[i] = NULL;
			continue;
		}

		Che_edge *e = m_edges_vertex[i];
		Che_edge *e_walk = e;
		/*
		if (!e) // isolated vertex
		{
		tensor[i] = NULL;
		continue;
		}
		
		  int n_neighbours = 0;
		  Che *e_walk = e;
		  do
		  {
		  n_neighbours++;
		  e_walk = e_walk->he_next->he_next->pair;
		  } while (e_walk && e_walk != e);
		  if (!e_walk) // is on border
		  {
		  tensor[i] = NULL;
		  continue;
		  }
		*/
		
		// init linear system
		// fitted surface : f(u,v) = (au^2 + 2buv + bv^2)/2
		mat[0]=mat[1]=mat[2]=mat[3]=mat[4]=mat[5]=mat[6]=mat[7]=mat[8]=0.0;
		vec3_init (right, 0.0, 0.0, 0.0);
		
		vec3 v_current;
		vec3_init (v_current, v[3*i], v[3*i+1], v[3*i+2]);
		vec3 n;
		vec3_init (n, vn[3*i], vn[3*i+1], vn[3*i+2]);
		vec3_normalize (n);
		vec3 tmp;
		float D = - vec3_dot_product (v_current, n);
		
		// local basis
		vec3 b1, b2;
		if (n[0])
			vec3_init (b1, -(n[1]+n[2])/n[0], 1, 1);
		else
		{
			if (n[1])
				vec3_init (b1, 1, -(n[0]+n[2])/n[1], 1);
			else
				vec3_init (b1, 1, 1, -(n[0]+n[1])/n[2]);
		}
		vec3_normalize (b1);
		vec3_cross_product (b2, n, b1);
		
		// method more stable when there are a lot of neighbours
		//int n_neighbour = 0;
		e_walk = e;
		do
		{
			//n_neighbour++;
			int index = e_walk->m_v_end;
			
			vec3 v_walk;
			vec3_init (v_walk, v[3*index], v[3*index+1], v[3*index+2]);
			float d_walk = vec3_dot_product (v_walk, n) + D;
			vec3 v_proj;
			vec3_init (v_proj,
				   v_walk[0]-d_walk*n[0],
				   v_walk[1]-d_walk*n[1],
				   v_walk[2]-d_walk*n[2]);
			vec3 v_local;
			vec3_subtraction (v_local, v_proj, v_current);
			float ui = vec3_dot_product (v_local, b1);
			float vi = vec3_dot_product (v_local, b2);
			
			// update the matrix
			// fitting with (u,v,(au2+2buv+cv2)/2)
			mat[0] +=   ui*ui*ui*ui;  mat[1] += 2*ui*ui*ui*vi;  mat[2] +=   ui*ui*vi*vi;
			mat[3] += 2*ui*ui*ui*vi;  mat[4] += 4*ui*ui*vi*vi;  mat[5] += 2*ui*vi*vi*vi;
			mat[6] +=   ui*ui*vi*vi;  mat[7] += 2*ui*vi*vi*vi;  mat[8] +=   vi*vi*vi*vi;
			
			right[0] += 2*ui*ui*d_walk;  right[1] += 2*2*ui*vi*d_walk;  right[2] += 2*vi*vi*d_walk;
			
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		/*
		if (n_neighbour <= 4)
		{
		tensor[i] = NULL;
		continue;
		}
		*/
		
		// find the parameters of the interpolated surface
		float a, b, c;
		mat3 ls;
		mat3_init_array (ls, mat);
		if (mat3_solve_linearsystem (ls, right, solution))
		{
			a = solution[0];
			b = solution[1];
			c = solution[2];
		}
		else
		{
			Tensor *pDiffParamWalk = new Tensor ();
			pDiffParamWalk->SetKappaMax (1000000.0);
			pDiffParamWalk->SetKappaMin (1000000.0);
			pDiffParamWalk->SetDirectionMax (b1[0], b1[1], b1[2]);
			pDiffParamWalk->SetDirectionMin (b2[0], b2[1], b2[2]);
			m_pDiffParams[i] = pDiffParamWalk;
			continue;
		}
		
		//
		// principal curvatures
		//
		float kappa_gaussian = a*c-b*b;
		float kappa_mean = (a+c)/2.0;
		float temp = kappa_mean * kappa_mean - kappa_gaussian;
		if (temp < 0.0)
		{
			printf ("!!! Problem : kappa_mean * kappa_mean - kappa_gaussian = %f < 0\n", temp);
			temp = 0.0;
		}
		temp = sqrt (temp);
		float kappa1 = kappa_mean + temp;
		float kappa2 = kappa_mean - temp;
		
		//
		// principal directions
		//
		mat2 m;
		mat2_init (m, a, b, b, c);
		vec2 evector1, evector2;
		vec2 evalues;
		//m.SolveEigensystem (evector1, evector2, evalues);
		vec3 d1, d2;
		if (evalues[0] < evalues[1])
		{
			//kappa1 = evalues[1];
			//kappa2 = evalues[0];
			vec3_init (d1,
				   evector2[0]*b1[0]+evector2[1]*b2[0],
				evector2[0]*b1[1]+evector2[1]*b2[1],
				evector2[0]*b1[2]+evector2[1]*b2[2]);
			vec3_init (d2,
				   evector1[0]*b1[0]+evector1[1]*b2[0],
				evector1[0]*b1[1]+evector1[1]*b2[1],
				evector1[0]*b1[2]+evector1[1]*b2[2]);
		}
		else
		{
			//kappa1 = evalues[0];
			//kappa2 = evalues[1];
			vec3_init (d1,
				   evector1[0]*b1[0]+evector1[1]*b2[0],
				evector1[0]*b1[1]+evector1[1]*b2[1],
				evector1[0]*b1[2]+evector1[1]*b2[2]);
			vec3_init (d2,
				   evector2[0]*b1[0]+evector2[1]*b2[0],
				evector2[0]*b1[1]+evector2[1]*b2[1],
				evector2[0]*b1[2]+evector2[1]*b2[2]);
		}
		vec3_normalize (d1);
		vec3_normalize (d2);
		
		Tensor *pDiffParamWalk = new Tensor ();
		pDiffParamWalk->SetKappaMax (kappa1);
		pDiffParamWalk->SetKappaMin (kappa2);
		if (evalues[0] < evalues[1])
		{
			pDiffParamWalk->SetDirectionMax (d2[0], d2[1], d2[2]);
			pDiffParamWalk->SetDirectionMin (d1[0], d1[1], d1[2]);
		}
		else
		{
			pDiffParamWalk->SetDirectionMax (d1[0], d1[1], d1[2]);
			pDiffParamWalk->SetDirectionMin (d2[0], d2[1], d2[2]);
		}
		m_pDiffParams[i] = pDiffParamWalk;
    }

	return true;
}
