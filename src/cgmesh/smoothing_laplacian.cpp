#include "smoothing_laplacian.h"

//
//
//
bool MeshAlgoSmoothingLaplacian::Apply (Mesh_half_edge *model)
{
	int nv = model->m_pMesh->m_nVertices;
	float *v = model->m_pMesh->m_pVertices;

	int i;
	float *vnew;

	vnew = new float[3*nv];
	if (!vnew)
		return false;

	for (i=0; i<nv; i++)
    {
		if (!model->is_manifold(i) || model->is_border(i))
		{
			vnew[3*i]   = v[3*i];
			vnew[3*i+1] = v[3*i+1];
			vnew[3*i+2] = v[3*i+2];
			continue;
		}

		vec3 v_mean, v_walk;
		vec3_init (v_mean, 0.0, 0.0, 0.0);
		Citerator_half_edges_vertex he_ite (model->GetCheMesh(), i);
		int n_neighbours = 0;
		for (int he_walk = he_ite.first (); he_walk >= 0 && !he_ite.isLast (); he_walk = he_ite.next ())
		{
			int index = model->GetCheMesh()->edge(he_walk).m_v_end;
			vec3_init (v_walk, v[3*index], v[3*index+1], v[3*index+2]);
			vec3_addition (v_mean, v_mean, v_walk);
			n_neighbours++;
		}
		if (n_neighbours != 0)
		{
			vnew[3*i]   = v_mean[0] / n_neighbours;
			vnew[3*i+1] = v_mean[1] / n_neighbours;
			vnew[3*i+2] = v_mean[2] / n_neighbours;
		}
		else
		{
			vnew[3*i]   = v[3*i];
			vnew[3*i+1] = v[3*i+1];
			vnew[3*i+2] = v[3*i+2];
		}
    }

	delete (model->m_pMesh->m_pVertices);
	model->m_pMesh->m_pVertices = vnew;

	return true;
}
