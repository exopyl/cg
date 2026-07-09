#include "surface_implicit_tandem.h"

#include <stdlib.h>
#include <set>
#include <vector>

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
	plane_t *fplane;
} mc_triangulation_t;



////////////////////////////////////////////////////////////////////////////////
//
// tandem
//
typedef struct _edge_data_
{
	float m_error;
	quadric_t q;
	Vector3f m_p;
} edge_data_t;

static void edge_data_dump (Che_edge &e)
{
	edge_data_t *edata = (edge_data_t*)e.m_data;
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

	Vector3f v1, v2, v3;
	v1.Set (vertices[3*iv1], vertices[3*iv1+1], vertices[3*iv1+2]);
	v2.Set (vertices[3*iv2], vertices[3*iv2+1], vertices[3*iv2+2]);
	v3.Set (vertices[3*iv3], vertices[3*iv3+1], vertices[3*iv3+2]);
	
	pTri->farea[fi] = 2.*Vector3f::evaluate_triangle_area (v1, v2, v3);
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
	std::map<int,int>::iterator it = he_mesh->map_edges_vertex->find (vi);
	if (it == he_mesh->map_edges_vertex->end())
		return -1;

	int e_idx = it->second;
	int e_walk_idx = e_idx;
	do {
		if (e_walk_idx < 0)
			break;

		Che_edge &e_walk = he_mesh->edge(e_walk_idx);
		unsigned int f = e_walk.m_face;
/*
		printf ("face %d (%d %d %d) => %f\n", f,
			pTri->faces[3*f], pTri->faces[3*f+1], pTri->faces[3*f+2],
			pTri->farea[f]);
*/
		if (pTri->farea[f] > EPSILON)
		{
			quadric_t qf;

			plane_quadric(pTri->fplane[f], qf);
//			quadric_dump (qf);

			quadric_scale(qf, qf, pTri->farea[f]);
			quadric_add(qv, qv, qf);

			total_area += pTri->farea[f];
		}

		e_walk_idx = he_mesh->edge(e_walk.m_pair).m_he_next;
	} while (e_walk_idx != e_idx);

	if (total_area != 0.0f)
		quadric_scale(qv, qv, 1.0f / total_area);
	
	quadric_copy(pTri->q[vi], qv);
	pTri->varea[vi] = total_area;

	return 0;
}

// evaluate quadric metric on an edge
static int tandem_update_edge_quadric (mc_triangulation_t *pTri, int he_idx, float errors_range[2])
{
	Che_mesh *he_mesh = pTri->he_mesh;
	Che_edge &he = he_mesh->edge(he_idx);

	unsigned int v0 = he.m_v_begin;
	unsigned int v1 = he.m_v_end;
	Vector3f vec0, vec1, vnew;
	quadric_t q0, q1, qe;
	float error;

	if (v0 == v1)
		return -1;	

	vec0.Set (pTri->vertices[3*v0], pTri->vertices[3*v0+1], pTri->vertices[3*v0+2]);
	vec1.Set (pTri->vertices[3*v1], pTri->vertices[3*v1+1], pTri->vertices[3*v1+2]);
	
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
	//
	// This test walks the one-ring of both endpoints -- O(valence^2) per edge.
	// An edge is only ever contracted while its error stays <= EPSILON, so for
	// every edge whose QEM error already exceeds the threshold (the vast
	// majority on any curved surface) the result is irrelevant: skip it. After
	// the clamp above, error <= EPSILON is exactly the "candidate" case.
	if (error <= EPSILON &&
	    !he_mesh->is_border (v0) && !he_mesh->is_border (v1) &&
	    !he_mesh->vertex_is_near_border (v0) && !he_mesh->vertex_is_near_border (v1))
	{
		//printf ("evaluate %d -> %d\n", v0, v1);
		bool bFlip = false;
		unsigned int vis[2] = {v0, v1};
		Vector3f vec0, vec1, vec2, n, nnew;
		float *vertices = pTri->vertices;
		for (int i=0; i<2; i++)
		{
			int vi0 = vis[i];
			vec0.Set (vertices[3*vi0], vertices[3*vi0+1], vertices[3*vi0+2]);
			if ((vec0).getDistance (vnew) < EPSILON)
				continue;
			std::map<int,int>::iterator it = he_mesh->map_edges_vertex->find (vi0);
			int eidx = it->second;
			int e_walk_idx = eidx;
			do
			{
				if (e_walk_idx >= 0 && he_mesh->edge(e_walk_idx).m_valid)
				{
					Che_edge &ew = he_mesh->edge(e_walk_idx);
					int vi1 = ew.m_v_end;
					int vi2 = he_mesh->edge(ew.m_he_next).m_v_end;
					if (vi1 != v0 && vi2 != v0 && vi1 != v1 && vi2 != v1)
					{
						vec1.Set (vertices[3*vi1], vertices[3*vi1+1], vertices[3*vi1+2]);
						vec2.Set (vertices[3*vi2], vertices[3*vi2+1], vertices[3*vi2+2]);

						// compare (v0 v1 v2) & (vnew v1 v2)
						//printf ("%d %d %d vs %d %d %d\n", vi0, vi1, vi2, vi0, vi1, vi2);
						n = Vector3f::evaluate_triangle_normal (vec0, vec1, vec2);
						(n).Normalize ();
						nnew = Vector3f::evaluate_triangle_normal (vnew, vec1, vec2);
						(nnew).Normalize ();
						//printf ("%f %f %f\n", n[0], n[1], n[2]);
						//printf (" -> %f %f %f\n", nnew[0], nnew[1], nnew[2]);
						//printf ("dot = %f\n", (n).DotProduct (nnew) );
						if ((n).getLength () < EPSILON || (nnew).getLength () < EPSILON || (n).DotProduct (nnew) < 0.)
						{
							bFlip = true;
							break;
						}
					}
				}

				Che_edge &ew = he_mesh->edge(e_walk_idx);
				e_walk_idx = he_mesh->edge(he_mesh->edge(ew.m_he_next).m_he_next).m_pair;
			} while (e_walk_idx != eidx);
		}
		if (bFlip)
			error = INFINITY;
	}
	
	edge_data_t *edata = nullptr;
	if (he.m_data == nullptr)
	{
		he.m_data = (edge_data_t*)malloc(sizeof(edge_data_t));
	}
	if (he.m_pair >= 0)
		he_mesh->edge(he.m_pair).m_data = he.m_data;
	edata = (edge_data_t*)he.m_data;

	// store info
	quadric_copy (edata->q, qe);
	edata->m_error = error;
	edata->m_p = vnew;

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

	// Initialize edge quadrics ONCE. Each contraction below refreshes the
	// quadrics of the edges incident to the merged vertex (the only ones whose
	// endpoints change), so re-initializing every edge on every pass is
	// redundant. errors_range is kept only because the local update helper
	// takes it; it is not used in any decision (the contraction test is the
	// absolute threshold edata->m_error <= EPSILON).
	float errors_range[2] = {INFINITY, -INFINITY};
	for (std::map<std::pair<int,int>,int>::iterator it=he_mesh->m_map_edges->begin ();
	     it != he_mesh->m_map_edges->end ();
	     it++)
	{
		int he_idx = it->second;
		Che_edge &he = he_mesh->edge(he_idx);
		if (he.m_valid)
			tandem_update_edge_quadric (pTri, he_idx, errors_range);
	}

	int bAgain = true;
	while (bAgain)
	{
		bAgain = false;
		//printf ("%d edges\t %d faces\t %d vertices\n",
		//he_mesh->m_map_edges->size(), he_mesh->map_edges_face->size(), he_mesh->map_edges_vertex->size());
		//printf ("%d\n", he_mesh->m_map_edges->size());
		// Snapshot the edge indices before contracting. edge_contract2() mutates
		// m_map_edges (erase/insert) -- in particular it erases the key of the
		// edge being contracted, which is exactly the entry a live iterator into
		// m_map_edges would point at, triggering the MSVC "cannot increment
		// value-initialized map/set iterator" crash. The indices are stable
		// handles into m_edges (contraction never grows that vector); edges that
		// a contraction invalidates are skipped by the m_valid guard below, and
		// relabelled edges keep the same index.
		std::vector<int> edge_indices;
		edge_indices.reserve (he_mesh->m_map_edges->size ());
		for (std::map<std::pair<int,int>,int>::iterator ite=he_mesh->m_map_edges->begin ();
		     ite != he_mesh->m_map_edges->end ();
		     ite++)
			edge_indices.push_back (ite->second);

		for (size_t k=0; k<edge_indices.size (); k++)
		{
			int he_idx = edge_indices[k];
			Che_edge &he = he_mesh->edge(he_idx);
			if (!he.m_valid)
				continue;

			// Cheap candidate test FIRST. Only edges with a near-zero error are
			// ever contracted, so reject the rest before the costly border /
			// near-border / validity queries (which walk the one-ring through
			// std::map). edata is null for edges the quadric pass skipped
			// (degenerate / border) -- treat those as non-candidates.
			edge_data_t *edata = (edge_data_t*)he.m_data;
			if (!edata || edata->m_error > EPSILON)
				continue;

			if (he_mesh->is_border(he.m_v_begin) || he_mesh->is_border(he.m_v_end))
				continue;

			int v1 = he.m_v_begin;
			int v2 = he.m_v_end;
			if (he_mesh->vertex_is_near_border (v1) || he_mesh->vertex_is_near_border (v2))
				continue;

			if (he_mesh->is_edge_contract2_valid (he_idx))
			{
				bAgain = true;
				Vector3f vnew;
				vnew = ((edge_data_t*)he.m_data)->m_p;

				he_mesh->edge_contract2 (he_idx);
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
				std::map<int,int>::iterator it = he_mesh->map_edges_vertex->find (v1);
				if (it == he_mesh->map_edges_vertex->end())
					continue; // v1 unexpectedly missing after contraction; skip

				int e_idx = it->second;

				// update info about faces around v1
				int ew_idx = e_idx;
				do
				{
					Che_edge &ew = he_mesh->edge(ew_idx);
					int fi = ew.m_face;
					tandem_update_face_info (pTri, fi);

					ew_idx = he_mesh->edge(he_mesh->edge(ew.m_he_next).m_he_next).m_pair;
				} while (ew_idx != e_idx);

				// update info about edges around v1
				ew_idx = e_idx;
				do
				{
					tandem_update_edge_quadric (pTri, ew_idx, errors_range);
					//tandem_update_edge_quadric (pTri, he_mesh->edge(ew_idx).m_pair, errors_range);

					Che_edge &ew = he_mesh->edge(ew_idx);
					ew_idx = he_mesh->edge(he_mesh->edge(ew.m_he_next).m_he_next).m_pair;
				} while (ew_idx != e_idx);
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
		int e_idx = he_mesh->get_edge_from_vertex (i);
		if (e_idx >= 0)
		{
			edge_data_t *edata = (edge_data_t*)he_mesh->edge(e_idx).m_data;
			if (edata->m_error < errors_range[0])
				errors_range[0] = edata->m_error;
			if (edata->m_error > errors_range[1])
				errors_range[1] = edata->m_error;
		}
	}
	for (int i=0; i<nvertices; i++)
	{
		int e_idx = he_mesh->get_edge_from_vertex (i);
		if (e_idx >= 0)
		{
			edge_data_t *edata = (edge_data_t*)he_mesh->edge(e_idx).m_data;
			fprintf (ptr, "vt %f 0.\n", (edata->m_error-errors_range[0]) / (errors_range[1]-errors_range[0]));
		}
		else
			fprintf (ptr, "vt 0. 0.\n");
	}
*/
	fprintf (ptr, "usemtl material\n");
	for (std::map<int,int>::iterator it=he_mesh->map_edges_face->begin ();
	     it != he_mesh->map_edges_face->end ();
	     it++)
	{
		int he_idx = it->second;
		Che_edge &he = he_mesh->edge(he_idx);
		int v1 = he.m_v_begin;
		int v2 = he.m_v_end;
		int v3 = he_mesh->edge(he.m_he_next).m_v_end;
		fprintf (ptr, "f %d %d %d\n", 1+v1, 1+v2, 1+v3);
	}
	
	fclose (ptr);
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
	// NOTE: simplification is run ONCE in get_triangulation_post() over the
	// complete mesh, not per Z-layer. The half-edge mesh accumulates the whole
	// surface anyway (completed layers are never flushed), so re-simplifying it
	// on every layer was redundant work scaling with the layer count. A genuine
	// streaming variant would flush+simplify finalized layers here instead.
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
	tri->fplane = (plane_t*)malloc(tri->nfacesmax*sizeof(plane_t));
	tri->varea = (float*)malloc(tri->nverticesmax*sizeof(float));
	tri->q = (quadric_t*)malloc(tri->nverticesmax*sizeof(quadric_t));
	tri->he_mesh = new Che_mesh ();

	data = (void*)tri;
}

void ImplicitSurfaceTandem::get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces)
{
	mc_triangulation_t *tri = (mc_triangulation_t*)data;
	Che_mesh *he_mesh = tri->he_mesh;

	// Simplify the whole iso-surface once, now that extraction has built the
	// complete half-edge mesh (see the constructor note on why this is not done
	// per-layer).
	tandem_simplify (tri);

	// Rebuild compact output buffers from the *simplified* half-edge mesh.
	// tandem_simplify() decimated he_mesh in place (contracting edges and
	// updating the surviving vertex positions in tri->vertices); the surviving
	// triangles are exactly the entries of map_edges_face, and contracted-away
	// vertices are simply no longer referenced. We gather the still-used
	// vertices, remap them to a dense range, and emit remapped triangles -- so
	// the public API returns the decimated mesh instead of the raw MC faces.
	const int oldNV = tri->nvertices;
	std::vector<int> remap(oldNV, -1);
	std::vector<unsigned int> outFaces;
	outFaces.reserve(3 * he_mesh->map_edges_face->size());
	int newNV = 0;

	for (std::map<int,int>::iterator it = he_mesh->map_edges_face->begin();
	     it != he_mesh->map_edges_face->end(); ++it)
	{
		int he_idx = it->second;
		Che_edge &he = he_mesh->edge(he_idx);
		if (!he.m_valid)
			continue;

		const int v1 = he.m_v_begin;
		const int v2 = he.m_v_end;
		const int v3 = he_mesh->edge(he.m_he_next).m_v_end;

		// skip out-of-range or degenerate triangles defensively
		if (v1 < 0 || v2 < 0 || v3 < 0 || v1 >= oldNV || v2 >= oldNV || v3 >= oldNV)
			continue;
		if (v1 == v2 || v2 == v3 || v1 == v3)
			continue;

		const int tv[3] = { v1, v2, v3 };
		for (int k = 0; k < 3; k++)
		{
			if (remap[tv[k]] == -1)
				remap[tv[k]] = newNV++;
			outFaces.push_back((unsigned int)remap[tv[k]]);
		}
	}

	// compacted vertices (allocate at least 1 element to keep free() simple)
	float *outVerts = (float*)malloc(3 * (newNV > 0 ? newNV : 1) * sizeof(float));
	for (int v = 0; v < oldNV; v++)
		if (remap[v] >= 0)
		{
			outVerts[3*remap[v]]   = tri->vertices[3*v];
			outVerts[3*remap[v]+1] = tri->vertices[3*v+1];
			outVerts[3*remap[v]+2] = tri->vertices[3*v+2];
		}

	const int outNF = (int)outFaces.size() / 3;
	unsigned int *outFacesArr =
		(unsigned int*)malloc(3 * (outNF > 0 ? outNF : 1) * sizeof(unsigned int));
	for (size_t i = 0; i < outFaces.size(); i++)
		outFacesArr[i] = outFaces[i];

	*nvertices = newNV;
	*vertices  = outVerts;
	*nfaces    = outNF;
	*faces     = outFacesArr;

	// release the intermediate extraction buffers and the half-edge mesh.
	// edge_data_t blocks are shared between an edge and its pair (see
	// tandem_update_edge_quadric), so free each unique pointer once.
	std::set<void*> freed;
	for (size_t e = 0; e < he_mesh->m_edges.size(); e++)
	{
		void *d = he_mesh->m_edges[e].m_data;
		if (d && freed.insert(d).second)
			free(d);
		he_mesh->m_edges[e].m_data = nullptr;
	}
	delete he_mesh;

	free(tri->vertices);
	free(tri->faces);
	free(tri->farea);
	free(tri->fplane);
	free(tri->varea);
	free(tri->q);
	free(tri);
	data = nullptr;
}
