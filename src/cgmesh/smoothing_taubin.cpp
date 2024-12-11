#include "smoothing_taubin.h"

//
//
//
bool MeshAlgoSmoothingTaubin::ApplyCoefficient (Mesh_half_edge *model, float coeff)
{
	int nv = model->m_nVertices;
	float *v = model->m_pVertices;

	int i;
	float x_translate, y_translate, z_translate;
	float *vnew;
	float *tmp;
	
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
		
		int n_neighbours;
		
		x_translate = y_translate = z_translate = 0.0;
		Citerator_half_edges_vertex he_ite (model, i);
		n_neighbours = 0;
		for (Che_edge *he_walk = he_ite.first (); he_walk && !he_ite.isLast (); he_walk = he_ite.next ())
		{
			int index = he_walk->m_v_end;
			x_translate += v[3*index]   - v[3*i];
			y_translate += v[3*index+1] - v[3*i+1];
			z_translate += v[3*index+2] - v[3*i+2];
			n_neighbours++;
		}
		
		vnew[3*i]   = v[3*i]   + coeff * x_translate / n_neighbours;
		vnew[3*i+1] = v[3*i+1] + coeff * y_translate / n_neighbours;
		vnew[3*i+2] = v[3*i+2] + coeff * z_translate / n_neighbours;
    }
	
	delete (model->m_pVertices);
	model->m_pVertices = vnew;

	return true;
}

//
// Smoothing introduced by Taubin.
// According to the article, the following parameters are adviced :
// \param lambda : 0.7
// \param mu : -0.7527
//
bool MeshAlgoSmoothingTaubin::Apply (Mesh_half_edge *model, float lambda, float mu)
{
	ApplyCoefficient (model, lambda);
	ApplyCoefficient (model, mu);
	//compute_vertices_normales (NORMALE_GOURAUD);

	return true;
}

