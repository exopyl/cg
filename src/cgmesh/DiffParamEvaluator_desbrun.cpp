#include <vector>

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
is_obtuse (const Vector3f &u, const Vector3f &v, const Vector3f &w)
{
	Vector3f v1 = v - u;
	Vector3f v2 = w - u;
	return (v1.DotProduct(v2) < 0.0);
}

static int
is_triangle_obtuse (const Vector3f &a, const Vector3f &b, const Vector3f &c)
{
	return (is_obtuse (a, b, c) ||
		is_obtuse (b, c, a) ||
		is_obtuse (c, a, b));
}

static float
cotan (const Vector3f &u, const Vector3f &v, const Vector3f &w)
{
	Vector3f v1 = v - u;
	Vector3f v2 = w - u;
	float v1dotv2 = v1.DotProduct(v2);
	float denom = sqrt (v1.DotProduct(v1)*v2.DotProduct(v2) - v1dotv2*v1dotv2);

	return (denom == 0.0)? 0.0 : v1dotv2/denom;
}

static float
angle_from_cotan (const Vector3f &u, const Vector3f &v, const Vector3f &w)
{
	Vector3f v1 = v - u;
	Vector3f v2 = w - u;
	float v1dotv2 = v1.DotProduct(v2);
	float denom = sqrt (v1.DotProduct(v1)*v2.DotProduct(v2) - v1dotv2*v1dotv2);

	return (fabs (atan2 (denom, v1dotv2)));
}

// area around a in the triangle abc
static float
region_area (const Vector3f &a, const Vector3f &b, const Vector3f &c)
{
	if (Vector3f::evaluate_triangle_area (a, b, c) == 0.0) return 0.0;

	if (is_triangle_obtuse (a, b, c))
    {
		if (is_obtuse (a, b, c)) return Vector3f::evaluate_triangle_area (a, b, c) / 2.0;
		else                     return Vector3f::evaluate_triangle_area (a, b, c) / 4.0;
    }
	else
    {
		Vector3f u = b - a;
		Vector3f v = c - a;
		return (cotan (b, a, c)*v.DotProduct(v)+cotan (c, a, b)*u.DotProduct(u))/8.0;
    }
}


//
//
//
bool MeshAlgoTensorEvaluator::ApplyDesbrun (void)
{
	int nv = m_pModel->m_pMesh->m_nVertices;
	float *v = m_pModel->m_pMesh->m_pVertices.data();
	Face **faces = m_pModel->m_pMesh->m_pFaces;
	float *vn = m_pModel->m_pMesh->m_pVertexNormals.data();
	int i,a,b,c;
	Vector3f v1, v2, v3;
	Tensor *pDiffParam_walk;
	int e, e_walk;
	int n_neighbours;

	for (i=0; i<nv; i++)
    {
		if (!m_pModel->is_manifold(i) || m_pModel->is_border(i))
		{
			Tensors ()[i] = nullptr;
			continue;
		}

		// check the number of neighbours
		e = m_pModel->GetCheMesh()->m_edges_vertex[i];
		e_walk = e;
		n_neighbours = m_pModel->get_n_neighbours (i);
		float area = 0.0; // init the area around the vertex

		// init for the mean curvature normal
		Vector3f mean_curvature_normal (0.0, 0.0, 0.0);

		// init for the gaussian curvature
		float angle_sum = 0.0;

		e = m_pModel->GetCheMesh()->m_edges_vertex[i];
		e_walk = e;
		do
		{
			Che_edge &ew = m_pModel->GetCheMesh()->edge(e_walk);
			a = -1;
			if (i == faces[ew.m_face]->GetVertex(0))
			{
				a = faces[ew.m_face]->GetVertex(0);
				b = faces[ew.m_face]->GetVertex(1);
				c = faces[ew.m_face]->GetVertex(2);
			}
			if (i == faces[ew.m_face]->GetVertex(1))
			{
				c = faces[ew.m_face]->GetVertex(0);
				a = faces[ew.m_face]->GetVertex(1);
				b = faces[ew.m_face]->GetVertex(2);
			}
			if (i == faces[ew.m_face]->GetVertex(2))
			{
				b = faces[ew.m_face]->GetVertex(0);
				c = faces[ew.m_face]->GetVertex(1);
				a = faces[ew.m_face]->GetVertex(2);
			}
			if (a == -1)
			{
				printf ("!!! state not supposed to be reached !!!\n");
				continue;
			}

			v1.Set (v[3*a], v[3*a+1], v[3*a+2]);
			v2.Set (v[3*b], v[3*b+1], v[3*b+2]);
			v3.Set (v[3*c], v[3*c+1], v[3*c+2]);

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
			int he_next = m_pModel->GetCheMesh()->edge(e_walk).m_he_next;
			e_walk = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(he_next).m_he_next).m_pair;
		} while (e_walk >= 0 && e_walk != e);

		if (area > 0.0)
		{
			mean_curvature_normal[0] /= 2*area;
			mean_curvature_normal[1] /= 2*area;
			mean_curvature_normal[2] /= 2*area;
		}
		else
			printf ("!!! Pb with area nil\n");

		// mean curvature
		float kappa_mean = mean_curvature_normal.getLength()/2.0;

		/* add on :
		* mean_kappa is always positive according to the article
		* because the surface is not supposed to be oriented.
		* to take this fact into account, we compare the orientation
		* of the mean curvature normal and the normal traditionally
		* computed on the current vertex.
		*/
		Vector3f nnn (vn[3*a], vn[3*a+1], vn[3*a+2]);
		if (mean_curvature_normal.DotProduct (nnn) < 0.0)
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
		Vector3f basis1, basis2, basis3;
		if (mean_curvature_normal.getLength ())
			basis3 = mean_curvature_normal;
		else
		{
			basis3.Set (0.0, 0.0, 0.0);
			e = m_pModel->GetCheMesh()->m_edges_vertex[i];
			e_walk = e;
			do
			{
				Che_edge &ew = m_pModel->GetCheMesh()->edge(e_walk);
				a = faces[ew.m_face]->GetVertex(0);
				b = faces[ew.m_face]->GetVertex(1);
				c = faces[ew.m_face]->GetVertex(2);

				v1.Set (v[3*a], v[3*a+1], v[3*a+2]);
				v2.Set (v[3*b], v[3*b+1], v[3*b+2]);
				v3.Set (v[3*c], v[3*c+1], v[3*c+2]);
				Vector3f n_walk = Vector3f::evaluate_triangle_normal (v1, v2, v3);
				n_walk.Normalize ();
				basis3 += n_walk;

				// next
				int he_next = m_pModel->GetCheMesh()->edge(e_walk).m_he_next;
				e_walk = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(he_next).m_he_next).m_pair;
			} while (e_walk >= 0 && e_walk != e);
		}
		basis3.Normalize ();

		basis1.Set (0.0, 0.0, 0.0);
		if (fabs(basis3[0]) > fabs(basis3[1]))
			basis1[1] = 1;
		else
			basis1[0] = 1;
		basis2 = basis3.CrossProduct (basis1);
		basis2.Normalize ();
		basis1 = basis2.CrossProduct (basis3);
		basis1.Normalize ();

		// init the linear system in relation with the minimization
		float m3[9];
		m3[0]=m3[1]=m3[2]=m3[3]=m3[4]=m3[5]=m3[6]=m3[7]=m3[8]=0.0;
		Vector3f right (0.0, 0.0, 0.0);

		// RAII: released at the end of each vertex iteration (was malloc'd and
		// only freed inside a dead #if 0 block => leaked once per vertex)
		std::vector<float> weights (n_neighbours);
		std::vector<float> kappas  (n_neighbours);
		std::vector<float> d1s     (n_neighbours);
		std::vector<float> d2s     (n_neighbours);
		int n_neighbour_walk = 0;

		e = m_pModel->GetCheMesh()->m_edges_vertex[i];
		e_walk = e;
		do
		{
			Che_edge &ew = m_pModel->GetCheMesh()->edge(e_walk);
			a = -1;
			if (i == faces[ew.m_face]->GetVertex(0))
			{
				a = faces[ew.m_face]->GetVertex(0);
				b = faces[ew.m_face]->GetVertex(1);
				c = faces[ew.m_face]->GetVertex(2);
			}
			if (i == faces[ew.m_face]->GetVertex(1))
			{
				c = faces[ew.m_face]->GetVertex(0);
				a = faces[ew.m_face]->GetVertex(1);
				b = faces[ew.m_face]->GetVertex(2);
			}
			if (i == faces[ew.m_face]->GetVertex(2))
			{
				b = faces[ew.m_face]->GetVertex(0);
				c = faces[ew.m_face]->GetVertex(1);
				a = faces[ew.m_face]->GetVertex(2);
			}
			if (a == -1)
			{
				printf ("!!! state not supposed to be reached !!!\n");
				continue;
			}


			v1.Set (v[3*a], v[3*a+1], v[3*a+2]);
			v2.Set (v[3*b], v[3*b+1], v[3*b+2]);
			v3.Set (v[3*c], v[3*c+1], v[3*c+2]);

			// current edge
			Vector3f edge = v2 - v1;

			// curvature along the edge
			float kappa_n = 2 * (edge.DotProduct(basis3) / edge.DotProduct(edge));

			// weight
			float weight = 0.0;
			if (!is_triangle_obtuse (v1, v2, v3))
			{
				weight += edge.DotProduct(edge) * cotan (v3, v1, v2) / 8.0;
			}
			else
			{
				if (is_obtuse (v1, v2, v3))
					weight += edge.DotProduct(edge) * region_area (v1, v2, v3) / 4.0;
				else
					weight += edge.DotProduct(edge) * region_area (v1, v2, v3) / 8.0;
			}

			// adjacent face
			int pair_idx = m_pModel->GetCheMesh()->edge(e_walk).m_pair;
			int index = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(pair_idx).m_he_next).m_v_end;
			Vector3f vv3 (v[3*index], v[3*index+1], v[3*index+2]);
			if (!is_triangle_obtuse (v1, v2, vv3))
			{
				weight += edge.DotProduct(edge) * cotan (vv3, v1, v2) / 8.0;
			}
			else
			{
				if (is_obtuse (v1, v2, vv3))
					weight += edge.DotProduct(edge) * region_area (v1, v2, vv3) / 4.0;
				else
					weight += edge.DotProduct(edge) * region_area (v1, v2, vv3) / 8.0;
			}
			//weight = 1.0;

			// projection of the edge on the tangent plane
			float vedotn = edge.DotProduct (basis3);
			Vector3f ve_tmp (basis3[0]*vedotn, basis3[1]*vedotn, basis3[2]*vedotn);
			Vector3f ve_proj = edge - ve_tmp;
			ve_proj.Normalize ();

			// move d to 2D basis
			float d1, d2;
			d1 = ve_proj.DotProduct (basis1);
			d2 = ve_proj.DotProduct (basis2);

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
			int he_next = m_pModel->GetCheMesh()->edge(e_walk).m_he_next;
			e_walk = m_pModel->GetCheMesh()->edge(m_pModel->GetCheMesh()->edge(he_next).m_he_next).m_pair;
		} while (e_walk >= 0 && e_walk != e);

		// complete the linear system with  a+c = 2*kappa_mean
		m3[6] = 1; m3[7] = 0; m3[8] = 1;
		right[2] = 2*kappa_mean;

		// solve the linearsystem
		Vector3f result;
		float a, b, c;
		Matrix3f ls (m3);
		if (ls.SolveLinearSystem (right, result))
		{
			a = result[0];
			b = result[1];
			c = result[2];
		}
		else
		{
			pDiffParam_walk = new Tensor ();
			pDiffParam_walk->SetNormal (basis3.x, basis3.y, basis3.z);   // normale
			pDiffParam_walk->SetKappaMax (kappa1); // maximal curvature
			pDiffParam_walk->SetKappaMin (kappa2); // minimal curvature
			pDiffParam_walk->SetDirectionMax (basis1[0], basis1[1], basis1[2]);
			pDiffParam_walk->SetDirectionMin (basis2[0], basis2[1], basis2[2]);
			Tensors ()[i].reset (pDiffParam_walk);
		}

		// solve the eigensystem
		Matrix2f m2 (a, b, b, c);
		Vector2f evector1, evector2, evalues;
		m2.SolveEigensystem (evector1, evector2, evalues);

		Vector2f evector;
		if (1 || evalues[0] > evalues[1])
			evector = evector1;
		else
			evector = evector2;

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

		/* rotate evector by a right angle if that would decrease the error */
		if (err2 <= err1)
		{
			float temp =  evector[0];
			evector[0]   =  evector[1];
			evector[1]   = -temp;
		}
#endif

		Vector3f principal_direction1 (evector[0] * basis1[0] + evector[1] * basis2[0],
					       evector[0] * basis1[1] + evector[1] * basis2[1],
					       evector[0] * basis1[2] + evector[1] * basis2[2]);
		principal_direction1.Normalize ();
		Vector3f principal_direction2 = basis3.CrossProduct (principal_direction1);

		// fill the tensor
		pDiffParam_walk = new Tensor ();
		pDiffParam_walk->SetNormal (basis3.x, basis3.y, basis3.z);   // normale
		pDiffParam_walk->SetKappaMax (kappa1); // maximal curvature
		pDiffParam_walk->SetKappaMin (kappa2); // minimal curvature
		pDiffParam_walk->SetDirectionMax (principal_direction1[0], principal_direction1[1], principal_direction1[2]);
		pDiffParam_walk->SetDirectionMin (principal_direction2[0], principal_direction2[1], principal_direction2[2]);
		Tensors ()[i].reset (pDiffParam_walk);
    }

	return true;
}
