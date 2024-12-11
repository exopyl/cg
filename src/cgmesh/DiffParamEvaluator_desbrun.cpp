#include "DiffParamEvaluator.h"

/**
*
* Desbrun
*
*/

/*
* 1 if (v,u,w) is obtuse
* 0 otherwise
*/
static int
is_obtuse (vec3 u, vec3 v, vec3 w)
{
	vec3 v1, v2;

	vec3_subtraction (v1, v, u);
	vec3_subtraction (v2, w, u);
	return (vec3_dot_product(v1, v2) < 0.0);
}

static int
is_triangle_obtuse (vec3 a, vec3 b, vec3 c)
{
	return (is_obtuse (a, b, c) ||
		is_obtuse (b, c, a) ||
		is_obtuse (c, a, b));
}

static float
cotan (vec3 u, vec3 v, vec3 w)
{
	vec3 v1, v2;
	float v1dotv2, denom;
	vec3_subtraction (v1, v, u);
	vec3_subtraction (v2, w, u);
	v1dotv2 = vec3_dot_product (v1, v2);
	denom = sqrt (vec3_dot_product(v1,v1)*vec3_dot_product(v2,v2) - v1dotv2*v1dotv2);
	
	return (denom == 0.0)? 0.0 : v1dotv2/denom;
}

static float
angle_from_cotan (vec3 u, vec3 v, vec3 w)
{
	vec3 v1, v2;
	float v1dotv2, denom;

	vec3_subtraction (v1, v, u);
	vec3_subtraction (v2, w, u);
	v1dotv2 = vec3_dot_product (v1, v2);
	denom = sqrt (vec3_dot_product(v1,v1)*vec3_dot_product(v2,v2) - v1dotv2*v1dotv2);
	
	return (fabs (atan2 (denom, v1dotv2)));
}

// area around a in the triangle abc
static float
region_area (vec3 a, vec3 b, vec3 c)
{
	if (vec3_triangle_area (a, b, c) == 0.0) return 0.0;
	
	if (is_triangle_obtuse (a, b, c))
    {
		if (is_obtuse (a, b, c)) return vec3_triangle_area (a, b, c) / 2.0;
		else                     return vec3_triangle_area (a, b, c) / 4.0;
    }
	else
    {
		vec3 u, v;
		vec3_subtraction (u, b, a);
		vec3_subtraction (v, c, a);
		return (cotan (b, a, c)*vec3_dot_product(v,v)+cotan (c, a, b)*vec3_dot_product(u,u))/8.0;
    }
}


//
//
//
bool MeshAlgoTensorEvaluator::ApplyDesbrun (void)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	Face **faces = m_pModel->m_pFaces;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i,a,b,c;
	vec3 v1, v2, v3;
	Tensor *pDiffParam_walk;
	Che_edge *e, *e_walk;
	int n_neighbours;
	
	for (i=0; i<nv; i++)
    {
		if (!m_pModel->m_topology_ok[i] || m_pModel->m_border[i])
		{
			m_pDiffParams[i] = NULL;
			continue;
		}
		
		// check the number of neighbours
		e = m_edges_vertex[i];
		e_walk = e;
		n_neighbours = m_pModel->get_n_neighbours (i);
		float area = 0.0; // init the area around the vertex
		
		// init for the mean curvature normal
		vec3 mean_curvature_normal;
		vec3_init (mean_curvature_normal, 0.0, 0.0, 0.0);
		
		// init for the gaussian curvature
		float angle_sum = 0.0;
		
		e = m_edges_vertex[i];
		e_walk = e;
		do
		{
			a = -1;
			if (i == faces[e_walk->m_face]->GetVertex(0))
			{
				a = faces[e_walk->m_face]->GetVertex(0);
				b = faces[e_walk->m_face]->GetVertex(1);
				c = faces[e_walk->m_face]->GetVertex(2);
			}
			if (i == faces[e_walk->m_face]->GetVertex(1))
			{
				c = faces[e_walk->m_face]->GetVertex(0);
				a = faces[e_walk->m_face]->GetVertex(1);
				b = faces[e_walk->m_face]->GetVertex(2);
			}
			if (i == faces[e_walk->m_face]->GetVertex(2))
			{
				b = faces[e_walk->m_face]->GetVertex(0);
				c = faces[e_walk->m_face]->GetVertex(1);
				a = faces[e_walk->m_face]->GetVertex(2);
			}
			if (a == -1)
			{
				printf ("!!! state not supposed to be reached !!!\n");
				continue;
			}
			
			vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
			vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
			vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
			
			// compute the local area
			area += region_area (v1, v2, v3);
			
			//
			// compute the mean curvature normal
			//
			float temp;
			temp = cotan (v2, v1, v3);
			mean_curvature_normal[0] += temp*(v3[0]-v1[0]);
			mean_curvature_normal[1] += temp*(v3[1]-v1[1]);
			mean_curvature_normal[2] += temp*(v3[2]-v1[2]);
			
			temp = cotan (v3, v1, v2);
			mean_curvature_normal[0] += temp*(v2[0]-v1[0]);
			mean_curvature_normal[1] += temp*(v2[1]-v1[1]);
			mean_curvature_normal[2] += temp*(v2[2]-v1[2]);
			
			//
			// compute the gaussian curvature
			//
			angle_sum += angle_from_cotan (v1, v2, v3);
			
			// next
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		
		if (area > 0.0)
		{
			mean_curvature_normal[0] /= 2*area;
			mean_curvature_normal[1] /= 2*area;
			mean_curvature_normal[2] /= 2*area;
		}
		else
			printf ("!!! Pb with area nil\n");
		
		// mean curvature
		float kappa_mean = vec3_length(mean_curvature_normal)/2.0;
		
		/* add on :
		* mean_kappa is always positive according to the article
		* because the surface is not supposed to be oriented.
		* to take this fact into account, we compare the orientation
		* of the mean curvature normal and the normal traditionally
		* computed on the current vertex.
		*/
		vec3 nnn;
		vec3_init (nnn, vn[3*a], vn[3*a+1], vn[3*a+2]);
		if (vec3_dot_product (mean_curvature_normal, nnn) < 0.0)
		{
			kappa_mean *= -1.0;
			mean_curvature_normal[0] *= -1.0;
			mean_curvature_normal[1] *= -1.0;
			mean_curvature_normal[2] *= -1.0;
		}
		
		// gaussian curvature
		float kappa_gaussian = (2.0*3.14159 - angle_sum)/area;
		
		//
		// compute the principal curvatures
		//
		float temp = kappa_mean * kappa_mean - kappa_gaussian;
		if (temp < 0.0) temp = 0.0;
		temp = sqrt (temp);
		float kappa1 = kappa_mean + temp;
		float kappa2 = kappa_mean - temp;
		
		//
		// compute the principal directions
		//
		
		// construct the local basis
		vec3 basis1, basis2, basis3;
		if (vec3_length (mean_curvature_normal))
			vec3_copy (basis3, mean_curvature_normal);
		else
		{
			vec3_init (basis3, 0.0, 0.0, 0.0);
			e = m_edges_vertex[i];
			e_walk = e;
			do
			{
				a = faces[e_walk->m_face]->GetVertex(0);
				b = faces[e_walk->m_face]->GetVertex(1);
				c = faces[e_walk->m_face]->GetVertex(2);
				
				vec3 n_walk;
				vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
				vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
				vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
				vec3_triangle_normal (n_walk, v1, v2, v3);
				vec3_normalize (n_walk);
				vec3_addition (basis3, basis3, n_walk);
				
				// next
				e_walk = e_walk->m_he_next->m_he_next->m_pair;
			} while (e_walk && e_walk != e);
		}
		vec3_normalize (basis3);
		
		vec3_init (basis1, 0.0, 0.0, 0.0);
		if (fabs(basis3[0]) > fabs(basis3[1]))
			basis1[1] = 1;
		else
			basis1[0] = 1;
		vec3_cross_product (basis2,  basis3, basis1);
		vec3_normalize (basis2);
		vec3_cross_product (basis1,  basis2, basis3);
		vec3_normalize (basis1);
		
		// init the linear system in relation with the minimization
		float m3[9];
		m3[0]=m3[1]=m3[2]=m3[3]=m3[4]=m3[5]=m3[6]=m3[7]=m3[8]=0.0;
		vec3 right;
		vec3_init (right, 0.0, 0.0, 0.0);
		
		float *weights = (float*)malloc(n_neighbours*sizeof(float));
		float *kappas  = (float*)malloc(n_neighbours*sizeof(float));
		float *d1s     = (float*)malloc(n_neighbours*sizeof(float));
		float *d2s     = (float*)malloc(n_neighbours*sizeof(float));
		int n_neighbour_walk = 0;
		
		e = m_edges_vertex[i];
		e_walk = e;
		do
		{
			a = -1;
			if (i == faces[e_walk->m_face]->GetVertex(0))
			{
				a = faces[e_walk->m_face]->GetVertex(0);
				b = faces[e_walk->m_face]->GetVertex(1);
				c = faces[e_walk->m_face]->GetVertex(2);
			}
			if (i == faces[e_walk->m_face]->GetVertex(1))
			{
				c = faces[e_walk->m_face]->GetVertex(0);
				a = faces[e_walk->m_face]->GetVertex(1);
				b = faces[e_walk->m_face]->GetVertex(2);
			}
			if (i == faces[e_walk->m_face]->GetVertex(2))
			{
				b = faces[e_walk->m_face]->GetVertex(0);
				c = faces[e_walk->m_face]->GetVertex(1);
				a = faces[e_walk->m_face]->GetVertex(2);
			}
			if (a == -1)
			{
				printf ("!!! state not supposed to be reached !!!\n");
				continue;
			}
			
			
			vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
			vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
			vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
			
			// current edge
			vec3 edge;
			vec3_subtraction (edge, v2, v1);
			
			// curvature along the edge
			float kappa_n = 2 * (vec3_dot_product(edge,basis3) / vec3_dot_product(edge,edge));
			
			// weight
			float weight = 0.0;
			if (!is_triangle_obtuse (v1, v2, v3))
			{
				weight += vec3_dot_product(edge,edge) * cotan (v3, v1, v2) / 8.0;
			}
			else
			{
				if (is_obtuse (v1, v2, v3))
					weight += vec3_dot_product(edge,edge) * region_area (v1, v2, v3) / 4.0;
				else
					weight += vec3_dot_product(edge,edge) * region_area (v1, v2, v3) / 8.0;
			}
			
			// adjacent face
			vec3 vv3;
			int index = e_walk->m_pair->m_he_next->m_v_end;
			vec3_init (vv3, v[3*index], v[3*index+1], v[3*index+2]);
			if (!is_triangle_obtuse (v1, v2, vv3))
			{
				weight += vec3_dot_product(edge,edge) * cotan (vv3, v1, v2) / 8.0;
			}
			else
			{
				if (is_obtuse (v1, v2, vv3))
					weight += vec3_dot_product(edge,edge) * region_area (v1, v2, vv3) / 4.0;
				else
					weight += vec3_dot_product(edge,edge) * region_area (v1, v2, vv3) / 8.0;
			}
			//weight = 1.0;
			
			// projection of the edge on the tangent plane
			vec3 ve_tmp, ve_proj;
			float vedotn = vec3_dot_product (edge, basis3);
			vec3_init (ve_tmp, basis3[0]*vedotn, basis3[1]*vedotn, basis3[2]*vedotn);
			vec3_subtraction (ve_proj, edge, ve_tmp);
			vec3_normalize (ve_proj);
			
			// move d to 2D basis
			float d1, d2;
			d1 = vec3_dot_product (ve_proj, basis1);
			d2 = vec3_dot_product (ve_proj, basis2);
			
			weights[n_neighbour_walk] = weight;
			kappas[n_neighbour_walk]  = kappa_n;
			d1s[n_neighbour_walk]     = d1;
			d2s[n_neighbour_walk]     = d2;
			n_neighbour_walk++;
			
			// update the linear system
			m3[0] += weight * d1 * d1 * d1 * d1;
			m3[1] += 2 * weight * d1 * d1 * d1 * d2;
			m3[2] += weight * d1 *d1 *d2 *d2;
			right[0] += weight * d1 * d1 * kappa_n;
			
			m3[3] += weight * d1 * d1 * d1 * d2;
			m3[4] += 2 * weight * d1 * d1 * d2 * d2;
			m3[5] += weight * d1 *d2 *d2 *d2;
			right[1] += weight * d1 * d2 * kappa_n;
			
			// next
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		
		// complete the linear system with  a+c = 2*kappa_mean
		m3[6] = 1; m3[7] = 0; m3[8] = 1;
		right[2] = 2*kappa_mean;
		
		// solve the linearsystem
		vec3 result;
		float a, b, c;
		mat3 ls;
		mat3_init_array (ls, m3);
		if (mat3_solve_linearsystem (ls, right, result))
		{      
			a = result[0];
			b = result[1];
			c = result[2];
		}
		else
		{
			pDiffParam_walk = new Tensor ();
			pDiffParam_walk->SetNormal (basis3);   // normale
			pDiffParam_walk->SetKappaMax (kappa1); // maximal curvature
			pDiffParam_walk->SetKappaMin (kappa2); // minimal curvature
			pDiffParam_walk->SetDirectionMax (basis1[0], basis1[1], basis1[2]);
			pDiffParam_walk->SetDirectionMin (basis2[0], basis2[1], basis2[2]);
			m_pDiffParams[i] = pDiffParam_walk;
		}
		
		// solve the eigensystem
		mat2 m2;
		mat2_init (m2, a, b, b, c);
		vec2 evector1, evector2, evalues;
		//m2.SolveEigensystem (evector1, evector2, evalues);
		
		vec2 evector;
		if (1 || evalues[0] > evalues[1])
			vec2_init (evector, evector1[0], evector1[1]);
		else
			vec2_init (evector, evector2[0], evector2[1]);
		
#if 0
		float err1 = 0.0;
		float err2 = 0.0;
		/* loop through the values previsouly saved */
		for (n_neighbour_walk = 0; n_neighbour_walk<n_neighbours; n_neighbour_walk++)
		{
			float weight = weights[n_neighbour_walk];
			float kappa  = kappas[n_neighbour_walk];
			float d1     = d1s[n_neighbour_walk];
			float d2     = d2s[n_neighbour_walk];
			float temp1 = fabs (evector[0]*d1 + evector[1]*d2);
			temp1 *= temp1;
			float temp2 = fabs (evector[1]*d1 - evector[0]*d2);
			temp2 *= temp2;
			float temp3;
			
			/* err1 is for kappa1 associated to e1 */
			temp3 = kappa1*temp1 + kappa2*temp2 - kappa;
			err1 += weight*temp3*temp3;
			
			/* err2 is for kappa1 associated to e2 */
			temp3 = kappa2*temp1 + kappa1*temp2 - kappa;
			err2 += weight*temp3*temp3;
		}
		
		free (weights);
		free (kappas);
		free (d1s);
		free (d2s);
		
		/* rotate evector by a right angle if that would decrease the error */
		if (err2 <= err1)
		{
			float temp =  evector[0];
			evector[0]   =  evector[1];
			evector[1]   = -temp;
		}
#endif    
		
		vec3 principal_direction1, principal_direction2;
		vec3_init (principal_direction1,
			   evector[0] * basis1[0] + evector[1] * basis2[0],
			   evector[0] * basis1[1] + evector[1] * basis2[1],
			   evector[0] * basis1[2] + evector[1] * basis2[2]);
		vec3_normalize (principal_direction1);
		vec3_cross_product (principal_direction2, basis3, principal_direction1);
		
		// fill the tensor
		pDiffParam_walk = new Tensor ();
		pDiffParam_walk->SetNormal (basis3);   // normale
		pDiffParam_walk->SetKappaMax (kappa1); // maximal curvature
		pDiffParam_walk->SetKappaMin (kappa2); // minimal curvature
		pDiffParam_walk->SetDirectionMax (principal_direction1[0], principal_direction1[1], principal_direction1[2]);
		pDiffParam_walk->SetDirectionMin (principal_direction2[0], principal_direction2[1], principal_direction2[2]);
		m_pDiffParams[i] = pDiffParam_walk;
    }

	return true;
}
