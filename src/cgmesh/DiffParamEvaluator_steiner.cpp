#include "DiffParamEvaluator.h"


/**
*
* Steiner
*
*/
static float
length_edge_in_sphere (vec3 v1, vec3 v2, vec3 center, float radius, vec3 intersection)
{
	vec3 tmp1, tmp2, orig, dir;
	float l1, l2;
	vec3_subtraction (tmp1, v1, center);
	vec3_subtraction (tmp2, v2, center);
	l1 = vec3_length (tmp1);
	l2 = vec3_length (tmp2);
	if (l1 <= radius && l2 <= radius)
    {
	    vec3_subtraction (dir, v2, v1);
		return vec3_length (dir);
    }
	if (l1 > radius && l2 > radius)
		return 0.0;
	if (l1 <= radius && l2 > radius)
    {
	    vec3_copy (orig, v1);
	    vec3_subtraction (dir, v2, v1);
    }
	if (l1 > radius && l2 <= radius)
    {
	    vec3_copy (orig, v2);
	    vec3_subtraction (dir, v1, v2);
    }
	vec3_normalize (dir);
	
	vec3 tmp;
	vec3_subtraction (tmp, orig, center);
	float b = 2*vec3_dot_product (dir,tmp);
	float c = vec3_dot_product(tmp,tmp) - radius*radius;
	float delta = (b*b-4*c)/2;
	float t, t1, t2;
	t1 = -b-sqrt(delta);
	t2 = -b+sqrt(delta);
	
	t = (t1 >= 0.0)? t1 : t2;
	
	vec3_init (intersection, orig[0]+t*dir[0], orig[1]+t*dir[1], orig[2]+t*dir[2]);
	return t;
}

/* area of a triangle included in sphere */
static float
_area_section (vec3 v1, vec3 v2, vec3 v3, vec3 center, float radius)
{
	return 0.0;
	/* compute the normale to v1v2v3 */
	vec3 v12, v13, n;
	vec3_subtraction (v12, v2, v1);
	vec3_subtraction (v13, v3, v1);
	vec3_cross_product (n, v12, v13);
	
	/* compute radius2 */
	float h = vec3_dot_product(n,center) - vec3_dot_product(n,v1);
	float radius2 = radius*radius - h*h;
	radius2 = sqrt (radius2);
	
	/* compute center2 */
	vec3 center2;
	vec3_init (center2, center[0] - h*n[0], center[1] - h*n[1], center[2] - h*n[2]);
	
	/* compute theta */
	vec3 v0v1, v0v2;
	vec3_subtraction (v0v1, v1, center2);
	vec3_subtraction (v0v2, v2, center2);
	float theta = acos (vec3_dot_product (v0v1,v0v2));
	
	/* area of the segment */
	return radius2*radius2 * (theta - sin (theta)) / 2;
}

static float
area_triangle_in_sphere (vec3 v1, vec3 v2, vec3 v3, vec3 center, float radius)
{
	vec3 tmp1, tmp2, tmp3;
	vec3_subtraction (tmp1, v1, center);
	vec3_subtraction (tmp2, v2, center);
	vec3_subtraction (tmp3, v3, center);
	float l1, l2, l3;
	l1 = vec3_length (tmp1);
	l2 = vec3_length (tmp2);
	l3 = vec3_length (tmp3);
	
	
	vec3 i1, i2;
	if (l1 <= radius && l2 <= radius && l2 <= radius)
		return vec3_triangle_area (v1, v2, v3);
	
	if (l1 <= radius && l2 > radius && l3 > radius)
    {
		length_edge_in_sphere (v1, v2, center, radius, i1);
		length_edge_in_sphere (v1, v3, center, radius, i2);
		return vec3_triangle_area (v1, i1, i2) + _area_section (v1, v2, v3, center, radius);
    }
	if (l2 <= radius && l1 > radius && l3 > radius)
    {
		length_edge_in_sphere (v2, v1, center, radius, i1);
		length_edge_in_sphere (v2, v3, center, radius, i2);
		return vec3_triangle_area (v2, i1, i2) + _area_section (v2, i1, i2, center, radius);
    }
	if (l3 <= radius && l1 > radius && l2 > radius)
    {
		length_edge_in_sphere (v3, v1, center, radius, i1);
		length_edge_in_sphere (v3, v2, center, radius, i2);
		return vec3_triangle_area (v3, i1, i2) + _area_section (v3, i1, i2, center, radius);
    }
	
	if (l1 <= radius && l2 <= radius && l3 > radius)
    {
		length_edge_in_sphere (v1, v3, center, radius, i1);
		length_edge_in_sphere (v2, v3, center, radius, i2);
		return vec3_triangle_area (v1, i1, v2) + vec3_triangle_area (i1, v2, i2) + _area_section (v1, i1, i2, center, radius);
    }
	if (l1 <= radius && l3 <= radius && l2 > radius)
    {
		length_edge_in_sphere (v1, v2, center, radius, i1);
		length_edge_in_sphere (v3, v2, center, radius, i2);
		return vec3_triangle_area (v1, i1, v3) + vec3_triangle_area (i1, v3, i2) + _area_section (v1, i1, i2, center, radius);
    }
	if (l2 <= radius && l3 <= radius && l1 > radius)
    {
		length_edge_in_sphere (v2, v1, center, radius, i1);
		length_edge_in_sphere (v3, v1, center, radius, i2);
		return vec3_triangle_area (v2, i1, v3) + vec3_triangle_area (i1, v3, i2) + _area_section (v2, i1, i2, center, radius);
    }
	printf ("nothing\n");

	return 0.0;
}

static int is_already_visited (Che_edge *e, Che_edge **array, int n)
{
	for (int i=0; i<n; i++)
		if (array[i] == e ||
			(array[i]->m_v_begin == e->m_v_begin && array[i]->m_v_end == e->m_v_end) ||
			(array[i]->m_v_begin == e->m_v_end && array[i]->m_v_end == e->m_v_begin))
			return 1;
		return 0;
}

static int is_already_visited (int index, int *array, int n)
{
	for (int i=0; i<n; i++)
		if (array[i] == index)
			return 1;
		return 0;
}

void MeshAlgoTensorEvaluator::ApplySteinerAux (int index, float radius, int *_n_edges, Che_edge ***_edges)
{
	int nv = m_pModel->m_nVertices;
	int nf = m_pModel->m_nFaces;
	float *v = m_pModel->m_pVertices;
	Face **f = m_pModel->m_pFaces;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int j;
	if (radius == 0.0)
    {
		*_n_edges = 0;
		*_edges = NULL;
    }
	else
    {
		Che_edge **new_edges = (Che_edge**)malloc(3*nf*sizeof(Che_edge*));
		int n_new_edges = 0;
		
		int *new_vertices = (int*)malloc(nv*sizeof(int));
		int n_new_vertices = 0;
		new_vertices[n_new_vertices++] = index;
		
		vec3 v_current, v_walk, tmp;
		vec3_init (v_current, v[3*index], v[3*index+1], v[3*index+2]);
		for (j=0; j<n_new_vertices; j++)
		{
			vec3_init (v_walk, v[3*new_vertices[j]], v[3*new_vertices[j]+1], v[3*new_vertices[j]+2]);
			vec3_subtraction (tmp, v_walk, v_current);
			if (vec3_length (tmp) < radius)
			{
				/* add the new edges */
				Che_edge *e = m_edges_vertex[new_vertices[j]];
				Che_edge *e_walk = e;
				do
				{
					if (!is_already_visited (e_walk, new_edges, n_new_edges))
					{
						new_edges[n_new_edges++] = e_walk;
						new_vertices[n_new_vertices++] = e_walk->m_v_end;
					}
					e_walk = e_walk->m_he_next->m_he_next->m_pair;
				} while (e_walk && e_walk != e);
			}
		}
		free (new_vertices);
		
		new_edges = (Che_edge**)realloc(new_edges, n_new_edges*sizeof(Che_edge*));
		*_n_edges = n_new_edges;
		*_edges = new_edges;
    }
}

bool MeshAlgoTensorEvaluator::ApplySteiner (void)
{
#if 0
	int nv = m_pModel->nv;
	float *v = m_pModel->v;
	int *f = m_pModel->f;
	float *vn = m_pModel->vn;
	float *fn = m_pModel->fn;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i,j,k;
	CDiffParam *pDiffParamWalk;
		
	// initialize the radius of B
	float xup = v[0];
	float yup = v[1];
	float zup = v[2];
	float xdown = xup;
	float ydown = yup;
	float zdown = zup;
	for (i=0; i<nv; i++)
    {
		if (v[3*i] > xup)     xup   = v[3*i];
		if (v[3*i] < xdown)   xdown = v[3*i];
		if (v[3*i+1] > yup)   yup   = v[3*i+1];
		if (v[3*i+1] < ydown) ydown = v[3*i+1];
		if (v[3*i+2] > zup)   zup   = v[3*i+2];
		if (v[3*i+2] < zdown) zdown = v[3*i+2];
    }
	float radius = sqrt ((xup-xdown)*(xup-xdown) + (yup-ydown)*(yup-ydown) + (zup-zdown)*(zup-zdown));
	radius /= 100.0;
	//printf ("radius = %f\n", radius);
	
	printf ("average edges length = %f\n", m_pModel->get_average_edges_length ());
	printf ("shortest edge length = %f\n", m_pModel->get_shortest_edge_length ());
	printf ("radius = %f\n", radius);
	//radius = get_smallest_edge_length ();
	printf ("radius = %f\n", radius);
	
	int n_edges;
	Che_edge **edges;
	for (i=0; i<nv; i++)
    {
		if (!m_pModel->m_topology_ok[i] || m_pModel->m_border[i])
		{
			m_pDiffParams[i] = NULL;
			continue;
		}
		
		ApplySteinerAux (i, radius, &n_edges, &edges);
		
		// current vertex
		float vx = v[3*i];
		float vy = v[3*i+1];
		float vz = v[3*i+2];
		vec3 v_current (v[3*i], v[3*i+1], v[3*i+2]);
		
		float m[9];
		m[0] = m[1] = m[2] = m[3] = m[4] = m[5] = m[6] = m[7] = m[8] = 0.0;
		
		float area = 0.0;
		int *visited_faces = (int*)malloc(n_edges*sizeof(int));
		int n_visited_faces = 0;
		int relevant = 1;
		for (j=0; j<n_edges; j++)
		{
			if (!edges[j]->m_pair)
			{
				relevant = 0;
				break;
			}
			
			int face_index = edges[j]->m_face;
			
			// area
			if (!is_already_visited (face_index, visited_faces, n_visited_faces))
			{
				vec3 v1, v2, v3;
				int i1, i2, i3;
				i1 = f[3*face_index];
				i2 = f[3*face_index+1];
				i3 = f[3*face_index+2];
				v1.Set (v[3*i1], v[3*i1+1], v[3*i1+2]);
				v2.Set (v[3*i2], v[3*i2+1], v[3*i2+2]);
				v3.Set (v[3*i3], v[3*i3+1], v[3*i3+2]);
				//area += v3d_area_triangle (v1, v2, v3);
				area += area_triangle_in_sphere (v1, v2, v3, v_current, radius);
				
				visited_faces[n_visited_faces++] = face_index;
			}
			
			// length of the edge in the ball
			vec3 v1,v2,ee;
			v1.Set (v[3*edges[j]->m_v_begin], v[3*edges[j]->m_v_begin+1], v[3*edges[j]->m_v_begin+2]);
			v2.Set (v[3*edges[j]->m_v_end], v[3*edges[j]->m_v_end+1], v[3*edges[j]->m_v_end+2]);
			ee = v2 - v1;
			//float length_intersection = ee.length ();
			vec3 intersection;
			float length_intersection = length_edge_in_sphere (v1, v2, v_current, radius, intersection);
			//printf ("%f -> %f (%f)\n", v3d_length (ee), length_intersection, radius);
			
			// angle between the normales
			vec3 fn1, fn2;
			fn1.Set (fn[3*edges[j]->m_face], fn[3*edges[j]->m_face+1], fn[3*edges[j]->m_face+2]);
			fn2.Set (fn[3*edges[j]->m_pair->m_face], fn[3*edges[j]->m_pair->m_face+1], fn[3*edges[j]->m_pair->m_face+2]);
			float fn1dotfn2 = fn1*fn2;
			if (fn1dotfn2 >= 1.0) fn1dotfn2 = 1.0; // fucking numerical instability !!!
			float angle = acos (fn1dotfn2);
			
			// sign
			float sign = 1.0;
			ee.Normalize ();
			vec3 cross = fn1 ^ fn2;
			if ((cross*ee) < 0.0)
				sign = -1.0;
			
			float weight = sign * angle * length_intersection;
			m[0] += weight*ee[0]*ee[0];  m[1] += weight*ee[0]*ee[1];  m[2] += weight*ee[0]*ee[2];
			m[3] += weight*ee[1]*ee[0];  m[4] += weight*ee[1]*ee[1];  m[5] += weight*ee[1]*ee[2];
			m[6] += weight*ee[2]*ee[0];  m[7] += weight*ee[2]*ee[1];  m[8] += weight*ee[2]*ee[2];
		}
		
		if (edges)         free (edges);
		if (visited_faces) free (visited_faces);
		
		if (!relevant)
		{
			m_pDiffParams[i] = NULL;
			continue;
		}
		
		assert (area);
		m[0] /= area; m[1] /= area; m[2] /= area;
		m[3] /= area; m[4] /= area; m[5] /= area;
		m[6] /= area; m[7] /= area; m[8] /= area;
		
		
		float eigenvectors[3][3];
		float eigenvalues[3];
		
		Matrix3 es (m);
		vec3 evector1, evector2, evector3, evalues;
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
		
		// search the eigenvalue 0
		float zero = fabs(eigenvalues[0]);
		int i_zero = 0, i_min, i_max;
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
			pDiffParamWalk = new CDiffParam ();
			pDiffParamWalk->SetNormale (eigenvectors[0][i_zero], eigenvectors[1][i_zero], eigenvectors[2][i_zero]);
			pDiffParamWalk->SetKappaMax (eigenvalues[i_min]);
			pDiffParamWalk->SetKappaMin (eigenvalues[i_max]);
			pDiffParamWalk->SetDirectionMax (eigenvectors[0][i_max], eigenvectors[1][i_max], eigenvectors[2][i_max]);
			pDiffParamWalk->SetDirectionMin (eigenvectors[0][i_min], eigenvectors[1][i_min], eigenvectors[2][i_min]);
			m_pDiffParams[i] = pDiffParamWalk;
    }
#endif
	return true;
}
