#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyTaubin (void)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i,k;
	float eigenvectors[3][3];
	float eigenvalues[3];
	float m[9];
	
	//if (!m_tensor)
	//	m_tensor = (Ctensor**)malloc(nv*sizeof(Ctensor*));
	
	for (i=0; i<nv; i++)
    {
		if (!m_pModel->m_topology_ok[i] || m_pModel->m_border[i])
		{
			m_pDiffParams[i] = NULL;
			continue;
		}
		
		Tensor *pDiffParamWalk;
		float u_x, u_y, u_z;
		float v_x, v_y, v_z;
		float n_x, n_y, n_z;
		float t_x, t_y, t_z;
		float tmp, kappa;
		int neighbour, n_neighbours = m_pModel->get_n_neighbours (i);
		int i_zero=0, i_min=0, i_max=0;
		double zero;
		
		m[0]=m[1]=m[2]=m[3]=m[4]=m[5]=m[6]=m[7]=m[8]=0.0;
		u_x = v[3*i];
		u_y = v[3*i+1];
		u_z = v[3*i+2];
		if (fabs(u_x - -0.270190001f) < 0.001f && fabs(u_y - (-0.485267997f)) < 0.001f && fabs(u_z - 0.831571996f) < 0.001f)
			int k = 0;

		//printf ("vertex %d : %f %f %f\n", i, u_x, u_y, u_z);
		n_x = vn[3*i];
		n_y = vn[3*i+1];
		n_z = vn[3*i+2];
		//printf ("n: %f %f %f\n", n_x, n_y, n_z);
		
		Che_edge *e = m_edges_vertex[i];
		Che_edge *e_walk = e;
		do
		{
			neighbour = e_walk->m_v_end;
			v_x = v[3*neighbour];
			v_y = v[3*neighbour+1];
			v_z = v[3*neighbour+2];
			//printf ("   v: %d (%f %f %f)\n", neighbour, v_x, v_y, v_z);
			
			// compute vector T
			tmp = ((v_x-u_x)*n_x) + ((v_y-u_y)*n_y) + ((v_z-u_z)*n_z);
			t_x = (v_x-u_x) - tmp*n_x;
			t_y = (v_y-u_y) - tmp*n_y;
			t_z = (v_z-u_z) - tmp*n_z;
			
			tmp = sqrt (t_x*t_x + t_y*t_y + t_z*t_z);
			t_x /= tmp;
			t_y /= tmp;
			t_z /= tmp;
			
			// compute kappa
			kappa = 2*(n_x*(v_x-u_x) + n_y*(v_y-u_y) + n_z*(v_z-u_z));
			tmp = sqrt ((v_x-u_x)*(v_x-u_x) + (v_y-u_y)*(v_y-u_y) + (v_z-u_z)*(v_z-u_z));
			kappa /= tmp*tmp;
			
			// add a weight to kappa
			kappa /= n_neighbours;
			
			/* update the matrix */
			m[0] += kappa*t_x*t_x;    m[1] += kappa*t_x*t_y;    m[2] += kappa*t_x*t_z;
			m[3] += kappa*t_y*t_x;    m[4] += kappa*t_y*t_y;    m[5] += kappa*t_y*t_z;
			m[6] += kappa*t_z*t_x;    m[7] += kappa*t_z*t_y;    m[8] += kappa*t_z*t_z;
			
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		
		// find the eigenvectors and eigenvalues of the matrix
		Matrix3 es (m);
		Vector3 evector1, evector2, evector3, evalues;
		es.SolveEigensystem (evector1, evector2, evector3, evalues);
		eigenvectors[0][0] = evector1[0];
		eigenvectors[1][0] = evector1[1];
		eigenvectors[2][0] = evector1[2];
		eigenvectors[0][1] = evector2[0];
		eigenvectors[1][1] = evector2[1];
		eigenvectors[2][1] = evector2[2];
		eigenvectors[0][2] = evector3[0];
		eigenvectors[1][2] = evector3[1];
		eigenvectors[2][2] = evector3[2];
		eigenvalues[0] = evalues[0];
		eigenvalues[1] = evalues[1];
		eigenvalues[2] = evalues[2];
		
		// fill the tensor_t struct
		pDiffParamWalk = new Tensor ();
		
		// search the eigenvalue 0
		zero = fabs(eigenvalues[0]);
		i_zero = 0;
		for (k=1; k<3; k++)
			if (fabs(eigenvalues[k]) < zero)
			{
				i_zero = k;
				zero = fabs(eigenvalues[k]);
			}
			
			// search the min and max
			switch (i_zero)
			{
			case 0:
				if (eigenvalues[1] < eigenvalues[2])
				{
					i_min = 1;
					i_max = 2;
				}
				else
				{
					i_min = 2;
					i_max = 1;
				}
				break;
			case 1:
				if (eigenvalues[0] < eigenvalues[2])
				{
					i_min = 0;
					i_max = 2;
				}
				else
				{
					i_min = 2;
					i_max = 0;
				}
				break;
			case 2:
				if (eigenvalues[0] < eigenvalues[1])
				{
					i_min = 0;
					i_max = 1;
				}
				else
				{
					i_min = 1;
					i_max = 0;
				}
				break;
			}
			
			// fill the tensor
			if (3*eigenvalues[i_min]-eigenvalues[i_max] < 3*eigenvalues[i_max]-eigenvalues[i_min])
			{
				float kappa1 = 3*eigenvalues[i_max]-eigenvalues[i_min];
				float kappa2 = 3*eigenvalues[i_min]-eigenvalues[i_max];
				pDiffParamWalk->SetKappaMax (kappa1);
				pDiffParamWalk->SetKappaMin (kappa2);
			}
			else
			{
				float kappa1 = 3*eigenvalues[i_min]-eigenvalues[i_max];
				float kappa2 = 3*eigenvalues[i_max]-eigenvalues[i_min];
				pDiffParamWalk->SetKappaMax (kappa1);
				pDiffParamWalk->SetKappaMin (kappa2);
			}
			pDiffParamWalk->SetDirectionMax (eigenvectors[0][i_max], eigenvectors[1][i_max], eigenvectors[2][i_max]);
			pDiffParamWalk->SetDirectionMin (eigenvectors[0][i_min], eigenvectors[1][i_min], eigenvectors[2][i_min]);
			m_pDiffParams[i] = pDiffParamWalk;
    }

	return true;
}
