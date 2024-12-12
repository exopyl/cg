#include "surface_implicit_tandem.h"

#include "half_edge.h"
#include "../cgmath/cgmath.h"


typedef struct _mc_triangulation_
{
	float *vertices;
	int nvertices;
	int nverticesmax;

	int iCurrentVertexInFace;
	unsigned int *faces;
	int nfaces;
	int nfacesmax;

	// specific for tandem
	Che_mesh *he_mesh;
	float *farea;
	float *varea;
	quadric_t *q;
	vec4 *fplane;
} mc_triangulation_t;



////////////////////////////////////////////////////////////////////////////////
//
// tandem
//
typedef struct _edge_data_
{
	float m_error;
	quadric_t q;
	vec3 m_p;
} edge_data_t;

static void edge_data_dump (Che_edge *e)
{
	edge_data_t *edata = (edge_data_t*)e->m_data;
	printf ("=> %f %f %f (%f)\n", edata->m_p[0], edata->m_p[1], edata->m_p[2], edata->m_error);
}

static int slice = 0;

// evaluate quadric metric at a vertex
static int tandem_update_face_info (mc_triangulation_t *pTri, int fi)
{
	unsigned int *faces = pTri->faces;
	float *vertices = pTri->vertices;

	unsigned int iv1 = faces[3*fi];
	unsigned int iv2 = faces[3*fi+1];
	unsigned int iv3 = faces[3*fi+2];

	vec3 v1, v2, v3;
	vec3_init (v1, vertices[3*iv1], vertices[3*iv1+1], vertices[3*iv1+2]);
	vec3_init (v2, vertices[3*iv2], vertices[3*iv2+1], vertices[3*iv2+2]);
	vec3_init (v3, vertices[3*iv3], vertices[3*iv3+1], vertices[3*iv3+2]);
	
	pTri->farea[fi] = 2.*vec3_triangle_area (v1, v2, v3);
	plane_init (pTri->fplane[fi], v1, v2, v3);

	return 0;
}

static void tandem_add_face (mc_triangulation_t *pTri, unsigned int fi, unsigned int iv1, unsigned int iv2, unsigned int iv3)
{
	Che_mesh *he_mesh = pTri->he_mesh;
	if (!he_mesh)
		return;
	
	he_mesh->add_face (fi, iv1, iv2, iv3);

	// update data
	if (fi >= pTri->nfacesmax)
	{
		pTri->farea = (float*)realloc(pTri->farea, pTri->nfacesmax*sizeof(float));
	}
	if (iv1 >= pTri->nverticesmax || iv2 >= pTri->nverticesmax || iv3 >= pTri->nverticesmax )
	{
		pTri->varea = (float*)realloc(pTri->varea, pTri->nverticesmax*sizeof(float));
		pTri->q = (quadric_t*)realloc(pTri->q, pTri->nverticesmax*sizeof(quadric_t));
	}

	tandem_update_face_info (pTri, fi);
}

// evaluate quadric metric at a vertex
static int tandem_update_vertex_quadric (mc_triangulation_t *pTri, int vi)
{
	Che_mesh *he_mesh = pTri->he_mesh;
	
	if (he_mesh->is_border (vi))
		return -1;
	float total_area = 0.0f;
	quadric_t qv;
	quadric_zero (qv);
	
	// iterate over faces adjacent to vi
	std::map<int,Che_edge*>::iterator it = he_mesh->map_edges_vertex->find (vi);
	if (it == he_mesh->map_edges_vertex->end())
		return -1;
	
	Che_edge *e = it->second;
	Che_edge *e_walk = e;
	do {
		if (!e_walk)
			break;

		unsigned int f = e_walk->m_face;
/*
		printf ("face %d (%d %d %d) => %f\n", f,
			pTri->faces[3*f], pTri->faces[3*f+1], pTri->faces[3*f+2],
			pTri->farea[f]);
*/
		if (pTri->farea[f] > EPSILON)
		{
			vec4 plane_eq;
			quadric_t qf;
			
			plane_quadric(pTri->fplane[f], qf);
//			quadric_dump (qf);

			quadric_scale(qf, qf, pTri->farea[f]);
			quadric_add(qv, qv, qf);
			
			total_area += pTri->farea[f];
		}
		
		e_walk = e_walk->m_pair->m_he_next;
	} while (e_walk != e);

	if (total_area != 0.0f)
		quadric_scale(qv, qv, 1.0f / total_area);
	
	quadric_copy(pTri->q[vi], qv);
	pTri->varea[vi] = total_area;

	return 0;
}

// evaluate quadric metric on an edge
static int tandem_update_edge_quadric (mc_triangulation_t *pTri, Che_edge *he, float errors_range[2])
{
	Che_mesh *he_mesh = pTri->he_mesh;

	unsigned int v0 = he->m_v_begin;
	unsigned int v1 = he->m_v_end;
	vec3 vec0, vec1, vnew;
	quadric_t q0, q1, qe;
	float error;

	if (v0 == v1)
		return -1;	

	vec3_init (vec0, pTri->vertices[3*v0], pTri->vertices[3*v0+1], pTri->vertices[3*v0+2]);
	vec3_init (vec1, pTri->vertices[3*v1], pTri->vertices[3*v1+1], pTri->vertices[3*v1+2]);
	
	quadric_scale (q0, pTri->q[v0], pTri->varea[v0]);
	quadric_scale (q1, pTri->q[v1], pTri->varea[v1]);
	quadric_add (qe, q0, q1);
	quadric_scale (qe, qe, 1.0f / (pTri->varea[v0] + pTri->varea[v1]));

	// find best vertex
	if (quadric_minimize2(qe, vnew, &error, vec0, vec1) == -1)
		return -1;
	
	// clamp to small non-zero value for log()
	if (error < EPSILON)
		error = EPSILON;

	// does the edge contraction flip a triangle ?
	if (!he_mesh->is_border (v0) && !he_mesh->is_border (v1) && 
	    !he_mesh->vertex_is_near_border (v0) && !he_mesh->vertex_is_near_border (v1))
	{
		//printf ("evaluate %d -> %d\n", v0, v1);
		bool bFlip = false;
		unsigned int vis[2] = {v0, v1};
		vec3 vec0, vec1, vec2, n, nnew;
		float *vertices = pTri->vertices;
		for (int i=0; i<2; i++)
		{
			int vi0 = vis[i];
			vec3_init (vec0, vertices[3*vi0], vertices[3*vi0+1], vertices[3*vi0+2]);
			if (vec3_distance (vec0, vnew) < EPSILON)
				continue;
			std::map<int,Che_edge*>::iterator it = he_mesh->map_edges_vertex->find (vi0);
			Che_edge *e = it->second;
			Che_edge *e_walk = e;
			do
			{
				if (e_walk && e_walk->m_valid)
				{
					int vi1 = e_walk->m_v_end;
					int vi2 = e_walk->m_he_next->m_v_end;
					if (vi1 != v0 && vi2 != v0 && vi1 != v1 && vi2 != v1)
					{
						vec3_init (vec1, vertices[3*vi1], vertices[3*vi1+1], vertices[3*vi1+2]);
						vec3_init (vec2, vertices[3*vi2], vertices[3*vi2+1], vertices[3*vi2+2]);
						
						// compare (v0 v1 v2) & (vnew v1 v2)
						//printf ("%d %d %d vs %d %d %d\n", vi0, vi1, vi2, vi0, vi1, vi2);
						vec3_triangle_normal (n, vec0, vec1, vec2);
						vec3_normalize (n);
						vec3_triangle_normal (nnew, vnew, vec1, vec2);
						vec3_normalize (nnew);
						//printf ("%f %f %f\n", n[0], n[1], n[2]);
						//printf (" -> %f %f %f\n", nnew[0], nnew[1], nnew[2]);
						//printf ("dot = %f\n", vec3_dot_product (n, nnew) );
						if (vec3_length (n) < EPSILON || vec3_length (nnew) < EPSILON || vec3_dot_product (n, nnew) < 0.)
						{
							bFlip = true;
							break;
						}
					}
				}
				
				e_walk = e_walk->m_he_next->m_he_next->m_pair;
			} while (e_walk != e);
		}
		if (bFlip)
			error = INFINITY;
	}
	
	edge_data_t *edata = NULL;
	if (he->m_data == NULL)
	{
		he->m_data = (edge_data_t*)malloc(sizeof(edge_data_t));
	}
	if (he->m_pair)
		he->m_pair->m_data = he->m_data;
	edata = (edge_data_t*)he->m_data;

	// store info
	quadric_copy (edata->q, qe);
	edata->m_error = error;
	vec3_copy (edata->m_p, vnew);

	// update errors range
	if (error < errors_range[0])
		errors_range[0] = error;
	if (error > errors_range[1])
		errors_range[1] = error;

	return 0;
}

static void tandem_simplify (mc_triangulation_t *pTri)
{
	if (!pTri)
		return;

	Che_mesh *he_mesh = pTri->he_mesh;

	unsigned int nvertices = pTri->nvertices;
	unsigned int nfaces = pTri->nfaces;
	float *vertices = pTri->vertices;
	unsigned int *faces = pTri->faces;

	// evaluate quadric metric at each vertex
	for (int v=0; v<nvertices; v++)
		tandem_update_vertex_quadric (pTri, v);

	int bAgain = true;
	while (bAgain)
	{

	// initialize edge quadrics
	float errors_range[2] = {INFINITY, -INFINITY};
	for (std::map<std::pair<int,int>,Che_edge*>::iterator it=he_mesh->m_map_edges->begin ();
	     it != he_mesh->m_map_edges->end ();
	     it++)
	{
		Che_edge *he = it->second;
		if (he && he->m_valid)
			tandem_update_edge_quadric (pTri, he, errors_range);
	}
	//printf ("error_min = %f(%f) error_max = %f(%f)\n", errors_range[0], log(errors_range[0]), errors_range[1], log(errors_range[1]));

		bAgain = false;
		//printf ("%d edges\t %d faces\t %d vertices\n",
		//he_mesh->m_map_edges->size(), he_mesh->map_edges_face->size(), he_mesh->map_edges_vertex->size());
		//printf ("%d\n", he_mesh->m_map_edges->size());
		for (std::map<std::pair<int,int>,Che_edge*>::iterator ite=he_mesh->m_map_edges->begin ();
		     ite != he_mesh->m_map_edges->end ();
		     ite++)
		{	
			Che_edge *he = ite->second;
			if (!he || !he->m_valid)
				continue;

			if (he_mesh->is_border(he->m_v_begin) || he_mesh->is_border(he->m_v_end))
				continue;

			edge_data_t *edata = (edge_data_t*)he->m_data;
			bool bContract = true;
			
			int v1 = he->m_v_begin;
			int v2 = he->m_v_end;
			if (he_mesh->vertex_is_near_border (v1) || he_mesh->vertex_is_near_border (v2))
				bContract = false;

			if (bContract
			    && edata->m_error <= EPSILON //0.0000005
			    && he_mesh->is_edge_contract2_valid (he))
			{
				bAgain = true;
				vec3 vnew;
				vec3_copy (vnew, ((edge_data_t*)he->m_data)->m_p);

				he_mesh->edge_contract2 (he);
/*
				printf ("~~~~~~~~ after contraction (%e) : (%d / %d) %f %f %f / %f %f %f => %f %f %f\n",
					edata->m_error, v1, v2,
					vertices[3*v1], vertices[3*v1+1], vertices[3*v1+2],
					vertices[3*v2], vertices[3*v2+1], vertices[3*v2+2],
					vnew[0], vnew[1], vnew[2]);
*/				
				vertices[3*v1]   = vnew[0];
				vertices[3*v1+1] = vnew[1];
				vertices[3*v1+2] = vnew[2];
				
				pTri->varea[v1] += pTri->varea[v2];
				quadric_copy (pTri->q[v1], edata->q);
				
				//
				std::map<int,Che_edge*>::iterator it = he_mesh->map_edges_vertex->find (v1);
				if (it == he_mesh->map_edges_vertex->end())
				{
					printf (":S\n");
					continue;
				}
				
				Che_edge *e = it->second;
				
				// update info about faces around v1
				Che_edge *e_walk = e;
				do
				{
					int fi = e_walk->m_face;
					tandem_update_face_info (pTri, fi);
					
					e_walk = e_walk->m_he_next->m_he_next->m_pair;
				} while (e_walk != e);
				
				// update info about edges around v1
				e_walk = e;
				do
				{
					tandem_update_edge_quadric (pTri, e_walk, errors_range);
					//tandem_update_edge_quadric (pTri, e_walk->m_pair, errors_range);
					
					e_walk = e_walk->m_he_next->m_he_next->m_pair;
				} while (e_walk != e);
			}
		}
		//printf ("%d edges\t %d faces\t %d vertices\n",
		//	he_mesh->m_map_edges->size(), he_mesh->map_edges_face->size(), he_mesh->map_edges_vertex->size());
	}
}

static void export_tandem (mc_triangulation_t *pTri, char *filename)
{
	Che_mesh *he_mesh = pTri->he_mesh;
	int nvertices = pTri->nvertices;
	float *vertices = pTri->vertices;
	int nfaces = pTri->nfaces;
	unsigned int *faces = pTri->faces;

	FILE *ptr = fopen (filename, "w");
	//fprintf (ptr, "mtllib export_simplified.mtl\n");
	for (int i=0; i<nvertices; i++)
		fprintf (ptr, "v %f %f %f\n", vertices[3*i], vertices[3*i+1], vertices[3*i+2]);

/*
	float errors_range[2] = {INFINITY, -INFINITY};
	for (int i=0; i<nvertices; i++)
	{
		Che_edge *e = he_mesh->get_edge_from_vertex (i);
		if (e)
		{
			edge_data_t *edata = (edge_data_t*)e->m_data;
			if (edata->m_error < errors_range[0])
				errors_range[0] = edata->m_error;
			if (edata->m_error > errors_range[1])
				errors_range[1] = edata->m_error;
		}
	}
	for (int i=0; i<nvertices; i++)
	{
		Che_edge *e = he_mesh->get_edge_from_vertex (i);
		if (e)
		{
			edge_data_t *edata = (edge_data_t*)e->m_data;
			fprintf (ptr, "vt %f 0.\n", (edata->m_error-errors_range[0]) / (errors_range[1]-errors_range[0]));
		}
		else
			fprintf (ptr, "vt 0. 0.\n");
	}
*/
	fprintf (ptr, "usemtl material\n");
	for (std::map<int,Che_edge*>::iterator it=he_mesh->map_edges_face->begin ();
	     it != he_mesh->map_edges_face->end ();
	     it++)
	{
		Che_edge *he = it->second;
		int v1 = he->m_v_begin;
		int v2 = he->m_v_end;
		int v3 = he->m_he_next->m_v_end;
		fprintf (ptr, "f %d %d %d\n", 1+v1, 1+v2, 1+v3);
	}
	
	fclose (ptr);
}

static void tandem_end_layer (void *data)
{
	mc_triangulation_t *pTri = (mc_triangulation_t*)data;

	tandem_simplify (pTri);
	export_tandem (pTri, "export_simplified.obj");
	printf ("slice %d : %d\n", slice, pTri->he_mesh->map_edges_face->size());
	slice++;
	//return;
	if (slice == 2)
		exit(0);
}
//
// tandem
//
////////////////////////////////////////////////////////////////////////////////
void new_face_completed (void *pData)
{
	mc_triangulation_t *pTri = (mc_triangulation_t*)pData;

	// update the half edge structure
	int findex = pTri->nfaces-1;
	tandem_add_face (pTri, findex, pTri->faces[3*findex], pTri->faces[3*findex+1], pTri->faces[3*findex+2]);
}

ImplicitSurfaceTandem::ImplicitSurfaceTandem ()
	: ImplicitSurface ()
{
	face_completed_func = new_face_completed;
	layer_completed_func = tandem_end_layer;
}

ImplicitSurfaceTandem::~ImplicitSurfaceTandem ()
{
}

void ImplicitSurfaceTandem::get_triangulation_pre (void)
{
	mc_triangulation_t *tri = (mc_triangulation_t*)malloc(sizeof(mc_triangulation_t));
	tri->nverticesmax = 100000;
	tri->vertices = (float*)malloc(3*tri->nverticesmax*sizeof(float));
	tri->nvertices = 0;

	tri->nfacesmax = 100000;
	tri->faces = (unsigned int*)malloc(3*tri->nfacesmax*sizeof(unsigned int));
	tri->nfaces = 0;

	tri->iCurrentVertexInFace = 0;

	// tandem specific
	tri->farea = (float*)malloc(tri->nfacesmax*sizeof(float));
	tri->fplane = (vec4*)malloc(tri->nfacesmax*sizeof(vec4));
	tri->varea = (float*)malloc(tri->nverticesmax*sizeof(float));
	tri->q = (quadric_t*)malloc(tri->nverticesmax*sizeof(quadric_t));
	tri->he_mesh = new Che_mesh ();

	data = (void*)tri;
}

void ImplicitSurfaceTandem::get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces)
{
	mc_triangulation_t *tri = (mc_triangulation_t*)data;
	*nvertices = tri->nvertices;
	*vertices = tri->vertices;
	*nfaces = tri->nfaces;
	*faces = tri->faces;

	free (tri);
	data = NULL;
}
