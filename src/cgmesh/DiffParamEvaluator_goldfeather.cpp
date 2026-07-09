#include "DiffParamEvaluator.h"
#include "../cgmath/cgmath.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyGoldfeather (void)
{
	int nv = m_pModel->m_pMesh->m_nVertices;
	float *v = m_pModel->m_pMesh->m_pVertices.data();
	float *vn = m_pModel->m_pMesh->m_pVertexNormals.data();
	int i,j,k,l;

	for (i=0; i<nv; i++)
    {
		if (!m_pModel->is_manifold(i) || m_pModel->is_border(i))
		{
			Tensors ()[i] = nullptr;
			continue;
		}

		Vector3f v_current (v[3*i], v[3*i+1], v[3*i+2]);
		Vector3f n (vn[3*i], vn[3*i+1], vn[3*i+2]);
		n.Normalize ();
		float D = - v_current.DotProduct (n);

		// local basis
		n.Set (vn[3*i], vn[3*i+1], vn[3*i+2]);
		n.Normalize ();

		Vector3f b1, b2;
		if (n[0])
			b1.Set (-(n[1]+n[2])/n[0], 1, 1);
		else
		{
			if (n[1])
				b1.Set (1, -(n[0]+n[2])/n[1], 1);
			else
				b1.Set (1, 1, -(n[0]+n[1])/n[2]);
		}
		b1.Normalize ();
		b2 = n.CrossProduct (b1);

		// compute the rotation we will apply on the normales
		Vector3f axis (n[1], -n[0], 0.0);
		//axis.normalize ();
		float theta = acos (n[2]);
		Quaternionf rot (axis, theta);

		// allocate memory for the matrices
		int n_neighbours = m_pModel->get_n_neighbours (i);
		float *A = (float*)malloc(n_neighbours*7*3*sizeof(float));
		for (j=0; j<n_neighbours*7*3; j++) A[j] = 0.0;
		float *B = (float*)malloc(n_neighbours*3*sizeof(float));
		for (j=0; j<n_neighbours*3; j++) B[j] = 0.0;

		int iwalk = 0;
		int e = m_pModel->GetCheMesh()->m_edges_vertex[i];
		int e_walk = e;
		do
		{
			Che_edge &ew = m_pModel->GetCheMesh()->edge(e_walk);
			int index = ew.m_v_end;
			if (m_pModel->m_border[index]) break;
			
			Vector3f v_walk (v[3*index], v[3*index+1], v[3*index+2]);

			float d_walk = v_walk.DotProduct (n) + D;

			Vector3f v_proj (v_walk[0]-d_walk*n[0], v_walk[1]-d_walk*n[1], v_walk[2]-d_walk*n[2]);
			Vector3f v_local = v_proj - v_current;
			float ui = v_local.DotProduct (b1);
			float vi = v_local.DotProduct (b2);

			float xi  = ui;
			float yi  = vi;
			float zi  = d_walk;

			Vector3f ni (vn[3*index], vn[3*index+1], vn[3*index+2]);
			Vector3f nrot;
			rot.rotate (nrot, ni);
			float nxi = nrot.x;
			float nyi = nrot.y;
			float nzi = nrot.z;
			
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
			int he_next = m_pModel->GetCheMesh()->edge(e_walk).m_he_next;
			e_walk = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(he_next).m_he_next).m_pair;
		} while (e_walk >= 0 && e_walk != e);
		
		if (iwalk != n_neighbours)
		{
			Tensors ()[i] = nullptr;
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
				Tensors ()[i] = nullptr;
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
			if (std::fabs(temp) < 10.f * std::numeric_limits<float>::epsilon())
			{
				temp = 0.f;
			}
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
			Matrix2f m (a, b, b, c);
			Vector2f evector1, evector2, evalues;
			m.SolveEigensystem (evector1, evector2, evalues);
			Vector3f d1, d2;
			if (evalues[0] < evalues[1])
			{
				//kappa1 = evalues[1];
				//kappa2 = evalues[0];
				d1.Set (evector2[0]*b1[0]+evector2[1]*b2[0],
					evector2[0]*b1[1]+evector2[1]*b2[1],
					evector2[0]*b1[2]+evector2[1]*b2[2]);
				d2.Set (evector1[0]*b1[0]+evector1[1]*b2[0],
					evector1[0]*b1[1]+evector1[1]*b2[1],
					evector1[0]*b1[2]+evector1[1]*b2[2]);
			}
			else
			{
				//kappa1 = evalues[0];
				//kappa2 = evalues[1];
				d1.Set (evector1[0]*b1[0]+evector1[1]*b2[0],
					evector1[0]*b1[1]+evector1[1]*b2[1],
					evector1[0]*b1[2]+evector1[1]*b2[2]);
				d2.Set (evector2[0]*b1[0]+evector2[1]*b2[0],
					evector2[0]*b1[1]+evector2[1]*b2[1],
					evector2[0]*b1[2]+evector2[1]*b2[2]);
			}
			d1.Normalize ();
			d2.Normalize ();
			
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
			
			Tensors ()[i].reset (pDiffParam_walk);

			if (kappa1 > 1.0)
				Tensors ()[i] = nullptr;
    }

	return true;
}
