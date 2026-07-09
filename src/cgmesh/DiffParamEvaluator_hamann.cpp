#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHamann (void)
{
	int nv = m_pModel->m_pMesh->m_nVertices;
	float *v = m_pModel->m_pMesh->m_pVertices.data();
	float *vn = m_pModel->m_pMesh->m_pVertexNormals.data();
	int i;
	float mat[9];
	Vector3f right, solution;

	for (i=0; i<nv; i++)
    {
		if (!m_pModel->is_manifold(i) || m_pModel->is_border(i))
		{
			Tensors ()[i] = nullptr;
			continue;
		}

		int e = m_pModel->GetCheMesh()->m_edges_vertex[i];
		int e_walk = e;
		/*
		if (!e) // isolated vertex
		{
		tensor[i] = nullptr;
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
		  tensor[i] = nullptr;
		  continue;
		  }
		*/

		// init linear system
		// fitted surface : f(u,v) = (au^2 + 2buv + bv^2)/2
		mat[0]=mat[1]=mat[2]=mat[3]=mat[4]=mat[5]=mat[6]=mat[7]=mat[8]=0.0;
		right.Set (0.0, 0.0, 0.0);

		Vector3f v_current (v[3*i], v[3*i+1], v[3*i+2]);
		Vector3f n (vn[3*i], vn[3*i+1], vn[3*i+2]);
		n.Normalize ();
		float D = - v_current.DotProduct (n);

		// local basis
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

		// method more stable when there are a lot of neighbours
		//int n_neighbour = 0;
		e_walk = e;
		do
		{
			//n_neighbour++;
			int index = m_pModel->GetCheMesh()->edge(e_walk).m_v_end;

			Vector3f v_walk (v[3*index], v[3*index+1], v[3*index+2]);
			float d_walk = v_walk.DotProduct (n) + D;
			Vector3f v_proj (v_walk[0]-d_walk*n[0],
					 v_walk[1]-d_walk*n[1],
					 v_walk[2]-d_walk*n[2]);
			Vector3f v_local = v_proj - v_current;
			float ui = v_local.DotProduct (b1);
			float vi = v_local.DotProduct (b2);

			// update the matrix
			// fitting with (u,v,(au2+2buv+cv2)/2)
			mat[0] +=   ui*ui*ui*ui;  mat[1] += 2*ui*ui*ui*vi;  mat[2] +=   ui*ui*vi*vi;
			mat[3] += 2*ui*ui*ui*vi;  mat[4] += 4*ui*ui*vi*vi;  mat[5] += 2*ui*vi*vi*vi;
			mat[6] +=   ui*ui*vi*vi;  mat[7] += 2*ui*vi*vi*vi;  mat[8] +=   vi*vi*vi*vi;

			right[0] += 2*ui*ui*d_walk;  right[1] += 2*2*ui*vi*d_walk;  right[2] += 2*vi*vi*d_walk;

			int he_next = m_pModel->GetCheMesh()->edge(e_walk).m_he_next;
			e_walk = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(he_next).m_he_next).m_pair;
		} while (e_walk >= 0 && e_walk != e);
		/*
		if (n_neighbour <= 4)
		{
		tensor[i] = nullptr;
		continue;
		}
		*/

		// find the parameters of the interpolated surface
		float a, b, c;
		Matrix3f ls (mat);
		if (ls.SolveLinearSystem (right, solution))
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
			Tensors ()[i].reset (pDiffParamWalk);
			continue;
		}

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
		Tensors ()[i].reset (pDiffParamWalk);
    }

	return true;
}
