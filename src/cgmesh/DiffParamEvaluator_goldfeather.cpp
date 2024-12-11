#include "DiffParamEvaluator.h"
#include "../cgmath/cgmath.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyGoldfeather (void)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i,j,k,l;
	
	for (i=0; i<nv; i++)
    {
		if (!m_pModel->m_topology_ok[i] || m_pModel->m_border[i])
		{
			m_pDiffParams[i] = NULL;
			continue;
		}
		
		vec3 v_current, n;
		vec3_init (v_current, v[3*i], v[3*i+1], v[3*i+2]);
		vec3_init (n, vn[3*i], vn[3*i+1], vn[3*i+2]);
		vec3_normalize (n);
		float D = - vec3_dot_product (v_current, n);
		
		// local basis
		vec3_init (n, vn[3*i], vn[3*i+1], vn[3*i+2]);
		vec3_normalize (n);
		
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
		
		// compute the rotation we will apply on the normales
		vec3 axis;
		vec3_init (axis, n[1], -n[0], 0.0);
		//axis.normalize ();
		float theta = acos (n[2]);
		quaternion rot;
		quaternion_init_axis_angle (rot, axis, theta);
		
		// allocate memory for the matrices
		int n_neighbours = m_pModel->get_n_neighbours (i);
		float *A = (float*)malloc(n_neighbours*7*3*sizeof(float));
		for (j=0; j<n_neighbours*7*3; j++) A[j] = 0.0;
		float *B = (float*)malloc(n_neighbours*3*sizeof(float));
		for (j=0; j<n_neighbours*3; j++) B[j] = 0.0;
		
		int iwalk = 0;
		Che_edge *e = m_edges_vertex[i];
		Che_edge *e_walk = e;
		e_walk = e;
		do
		{
			int index = e_walk->m_v_end;
			if (m_pModel->m_border[index]) break;
			
			vec3 v_walk;
			vec3_init (v_walk, v[3*index], v[3*index+1], v[3*index+2]);
			
			float d_walk = vec3_dot_product (v_walk, n) + D;
			
			vec3 v_proj;
			vec3_init (v_proj, v_walk[0]-d_walk*n[0], v_walk[1]-d_walk*n[1], v_walk[2]-d_walk*n[2]);
			vec3 v_local;
			vec3_subtraction (v_local, v_proj, v_current);
			float ui = vec3_dot_product (v_local, b1);
			float vi = vec3_dot_product (v_local, b2);
			
			float xi  = ui;
			float yi  = vi;
			float zi  = d_walk;
			
			vec3 ni;
			vec3_init (ni, vn[3*index], vn[3*index+1], vn[3*index+2]);
			quaternion_rotate (rot, ni, ni);
			float nxi = ni[0];
			float nyi = ni[1];
			float nzi = ni[2];
			
			// fill A
			A[21*iwalk]    = xi*xi/2.0;
			A[21*iwalk+1]  = xi*yi;
			A[21*iwalk+2]  = yi*yi/2.0;
			A[21*iwalk+3]  = xi*xi*xi;
			A[21*iwalk+4]  = xi*xi*yi;
			A[21*iwalk+5]  = xi*yi*yi;
			A[21*iwalk+6]  = yi*yi*yi;
			
			A[21*iwalk+7]  = xi;
			A[21*iwalk+8]  = yi;
			A[21*iwalk+9]  = 0.0;
			A[21*iwalk+10] = 3*xi*xi;
			A[21*iwalk+11] = 2*xi*yi;
			A[21*iwalk+12] = yi*yi;
			A[21*iwalk+13] = 0.0;
			
			A[21*iwalk+14] = 0.0;
			A[21*iwalk+15] = xi;
			A[21*iwalk+16] = yi;
			A[21*iwalk+17] = 0.0;
			A[21*iwalk+18] = xi*xi;
			A[21*iwalk+19] = 2*xi*yi;
			A[21*iwalk+20] = 3*yi*yi;
			
			// fill B
			B[3*iwalk]   = zi;
			B[3*iwalk+1] = -nxi/nzi;
			B[3*iwalk+2] = -nyi/nzi;
			
			iwalk++;
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		
		if (iwalk != n_neighbours)
		{
			m_pDiffParams[i] = NULL;
			if (A) free (A);
			if (B) free (B);
			continue;
		}
		
		//
		// solve Ax=B
		//
		
		// compute m1 = A^t*A
		float *m1 = (float*)malloc(7*7*sizeof(float));
		for (j=0; j<7; j++)
			for (k=0; k<7; k++)
			{
				m1[7*j+k] = 0.0;
				for (l=0; l<3*n_neighbours; l++)
					m1[7*j+k] += A[7*l+j] * A[7*l+k];
			}
			
			// compute m2 = A^t*B
			float *m2 = (float*)malloc(7*sizeof(float));
			for (j=0; j<7; j++)
			{
				m2[j] = 0.0;
				for (k=0; k<3*n_neighbours; k++)
					m2[j] += A[7*k+j] * B[k];
			}
			
			// solve linear system
			SquareMatrixf sm (7, m1);
			//Csquare_matrix *sm = new Csquare_matrix (7, m1);
			//sm->dump ();
			float *result = (float*)malloc(7*sizeof(float));
			float a, b, c;
			if (sm.SolveLinearSystem (m2, result, SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION))
			{
				// find the parameters of the interpolated surface
				a = result[0];
				b = result[1];
				c = result[2];
			}
			else
			{
				/*
				Tensor *tensor_walk = new Tensor ();
				tensor_walk->SetKappaMax (1000000.0);
				tensor_walk->SetKappaMin (1000000.0);
				tensor_walk->SetDirectionMax (b1.x, b1.y, b1.z);
				tensor_walk->SetDirectionMin (b2.x, b2.y, b2.z);
				*/
				m_pDiffParams[i] = NULL;
				continue;
			}
			
			if (0) // check result //
			{
				printf ("check result\n");
				for (j=0; j<7; j++)
				{
					float tmp = 0.0;
					for (k=0; k<7; k++)
						tmp += m1[7*j+k] * result[k];
					printf ("%f (%f)\n", tmp, m2[j]);
				}
				printf ("\n");
				printf ("%f %f %f\n", a, b, c);
				// end check result //
			}
			
			if (m1) free (m1);
			if (m2) free (m2);
			if (A) free (A);
			if (B) free (B);
			delete sm;
			
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
			//printf ("%f %f\n\n", kappa1, kappa2);
			
			//
			// principal directions
			//
			mat2 m;
			mat2_init (m, a, b, b, c);
			vec2 evector1, evector2;
			vec2 evalues;
			mat2_solve_eigensystem (m, evector1, evector2, evalues);
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
			
			Tensor *pDiffParam_walk = new Tensor ();
			pDiffParam_walk->SetKappaMax (kappa1);
			pDiffParam_walk->SetKappaMin (kappa2);
			if (evalues[0] < evalues[1])
			{
				pDiffParam_walk->SetDirectionMax (d2[0], d2[1], d2[2]);
				pDiffParam_walk->SetDirectionMin (d1[0], d1[1], d1[2]);
			}
			else
			{
				pDiffParam_walk->SetDirectionMax (d1[0], d1[1], d1[2]);
				pDiffParam_walk->SetDirectionMin (d2[0], d2[1], d2[2]);
			}
			
			m_pDiffParams[i] = pDiffParam_walk;
			
			if (kappa1 > 1.0)
				m_pDiffParams[i] = NULL;
    }

	return true;
}
