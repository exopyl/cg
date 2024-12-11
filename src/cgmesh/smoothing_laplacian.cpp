#include "smoothing_laplacian.h"

//
//
//
bool MeshAlgoSmoothingLaplacian::Apply (Mesh_half_edge *model)
{
	int nv = model->m_nVertices;
	float *v = model->m_pVertices;

	int i;
	float *vnew;

	vnew = new float[3*nv];
	if (!vnew)
		return false;
	
	for (i=0; i<nv; i++)
    {
		if (!model->m_topology_ok[i] || model->m_border[i])
		{
			vnew[3*i]   = v[3*i];
			vnew[3*i+1] = v[3*i+1];
			vnew[3*i+2] = v[3*i+2];
			continue;
		}
		
		vec3 v_mean, v_walk;
		vec3_init (v_mean, 0.0, 0.0, 0.0);
		Citerator_half_edges_vertex he_ite (model, i);
		int n_neighbours = 0;
		for (Che_edge *he_walk = he_ite.first (); he_walk && !he_ite.isLast (); he_walk = he_ite.next ())
		{
			int index = he_walk->m_v_end;
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
	
	delete (model->m_pVertices);
	model->m_pVertices = vnew;

	return true;
}
