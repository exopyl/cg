#include "surface_implicit_tandem.h"

#include <stdlib.h>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "half_edge.h"
#include "surface_implicit_triangulation.h"
#include <cgmath/cgmath.h>

namespace
{
	struct FacePlane { plane_t p; };
	struct VertexQuadric { quadric_t q; };

	// Donnees d'une arete, partagees par ses deux demi-aretes (Che_edge::m_data).
	struct edge_data_t
	{
		float m_error;
		quadric_t q;
		Vector3f m_p;
	};

	// Accumulateur du tandem : celui d'ImplicitSurface (sommets, faces), plus le maillage en
	// demi-aretes et les donnees par face et par sommet, agrandies a chaque face ajoutee
	// (tandem_add_face). edgeData possede les donnees d'aretes ; Che_edge::m_data ne fait
	// que les designer.
	struct TandemTriangulation : ImplicitTriangulation
	{
		std::unique_ptr<Che_mesh>                 he_mesh = std::make_unique<Che_mesh> ();
		std::vector<float>                        farea;    // par face
		std::vector<FacePlane>                    fplane;   // par face
		std::vector<float>                        varea;    // par sommet
		std::vector<VertexQuadric>                q;        // par sommet
		std::vector<std::unique_ptr<edge_data_t>> edgeData;

		static TandemTriangulation *FromData (void *data)
		{
			return static_cast<TandemTriangulation *> (ImplicitTriangulation::FromData (data));
		}
	};
}

typedef TandemTriangulation mc_triangulation_t;



////////////////////////////////////////////////////////////////////////////////
//
// tandem
//

static void edge_data_dump (Che_edge &e)
{
	edge_data_t *edata = (edge_data_t*)e.m_data;
	printf ("=> %f %f %f (%f)\n", edata->m_p[0], edata->m_p[1], edata->m_p[2], edata->m_error);
}

static int slice = 0;

// evaluate quadric metric at a vertex
static int tandem_update_face_info (mc_triangulation_t *pTri, int fi)
{
	const unsigned int *faces = pTri->faces.data();
	const float *vertices = pTri->vertices.data();

	unsigned int iv1 = faces[3*fi];
	unsigned int iv2 = faces[3*fi+1];
	unsigned int iv3 = faces[3*fi+2];

	Vector3f v1, v2, v3;
	v1.Set (vertices[3*iv1], vertices[3*iv1+1], vertices[3*iv1+2]);
	v2.Set (vertices[3*iv2], vertices[3*iv2+1], vertices[3*iv2+2]);
	v3.Set (vertices[3*iv3], vertices[3*iv3+1], vertices[3*iv3+2]);
	
	pTri->farea[fi] = 2.*Vector3f::evaluate_triangle_area (v1, v2, v3);
	plane_init (pTri->fplane[fi].p, v1, v2, v3);

	return 0;
}

static void tandem_add_face (mc_triangulation_t *pTri, unsigned int fi, unsigned int iv1, unsigned int iv2, unsigned int iv3)
{
	Che_mesh *he_mesh = pTri->he_mesh.get();
	he_mesh->add_face (fi, iv1, iv2, iv3);

	// donnees par face et par sommet a la taille du maillage (std::bad_alloc sur echec)
	if (fi >= pTri->farea.size ())
	{
		pTri->farea.resize (fi + 1);
		pTri->fplane.resize (fi + 1);
	}
	const unsigned int ivMax = std::max (iv1, std::max (iv2, iv3));
	if (ivMax >= pTri->varea.size ())
	{
		pTri->varea.resize (ivMax + 1);
		pTri->q.resize (ivMax + 1);
	}

	tandem_update_face_info (pTri, fi);
}

// evaluate quadric metric at a vertex
static int tandem_update_vertex_quadric (mc_triangulation_t *pTri, int vi)
{
	Che_mesh *he_mesh = pTri->he_mesh.get();
	
	if (he_mesh->is_border (vi))
		return -1;
	float total_area = 0.0f;
	quadric_t qv;
	quadric_zero (qv);
	
	// iterate over faces adjacent to vi
	std::map<int,int>::iterator it = he_mesh->VertexEdgeMap ()->find (vi);
	if (it == he_mesh->VertexEdgeMap ()->end())
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

			plane_quadric(pTri->fplane[f].p, qf);
//			quadric_dump (qf);

			quadric_scale(qf, qf, pTri->farea[f]);
			quadric_add(qv, qv, qf);

			total_area += pTri->farea[f];
		}

		e_walk_idx = he_mesh->edge(e_walk.m_pair).m_he_next;
	} while (e_walk_idx != e_idx);

	if (total_area != 0.0f)
		quadric_scale(qv, qv, 1.0f / total_area);
	
	quadric_copy(pTri->q[vi].q, qv);
	pTri->varea[vi] = total_area;

	return 0;
}

// evaluate quadric metric on an edge
static int tandem_update_edge_quadric (mc_triangulation_t *pTri, int he_idx, float errors_range[2])
{
	Che_mesh *he_mesh = pTri->he_mesh.get();
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
	
	quadric_scale (q0, pTri->q[v0].q, pTri->varea[v0]);
	quadric_scale (q1, pTri->q[v1].q, pTri->varea[v1]);
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
		const float *vertices = pTri->vertices.data();
		for (int i=0; i<2; i++)
		{
			int vi0 = vis[i];
			vec0.Set (vertices[3*vi0], vertices[3*vi0+1], vertices[3*vi0+2]);
			if ((vec0).getDistance (vnew) < EPSILON)
				continue;
			std::map<int,int>::iterator it = he_mesh->VertexEdgeMap ()->find (vi0);
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
		auto block = std::make_unique<edge_data_t> ();
		pTri->edgeData.push_back (std::move (block));
		he.m_data = pTri->edgeData.back ().get ();
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

	Che_mesh *he_mesh = pTri->he_mesh.get();

	unsigned int nvertices = pTri->nvertices;
	unsigned int nfaces = pTri->nfaces;
	float *vertices = pTri->vertices.data();

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
	for (std::map<std::pair<int,int>,int>::iterator it=he_mesh->EdgeMap ()->begin ();
	     it != he_mesh->EdgeMap ()->end ();
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
		// Snapshot the edge indices before contracting. edge_contract2() mutates
		// la carte des aretes (erase/insert) -- in particular it erases the key of the
		// edge being contracted, which is exactly the entry a live iterator into
		// cette carte pointerait, triggering the MSVC "cannot increment
		// value-initialized map/set iterator" crash. The indices are stable
		// handles into m_edges (contraction never grows that vector); edges that
		// a contraction invalidates are skipped by the m_valid guard below, and
		// relabelled edges keep the same index.
		std::vector<int> edge_indices;
		edge_indices.reserve (he_mesh->EdgeMap ()->size ());
		for (std::map<std::pair<int,int>,int>::iterator ite=he_mesh->EdgeMap ()->begin ();
		     ite != he_mesh->EdgeMap ()->end ();
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
				quadric_copy (pTri->q[v1].q, edata->q);

				//
				std::map<int,int>::iterator it = he_mesh->VertexEdgeMap ()->find (v1);
				if (it == he_mesh->VertexEdgeMap ()->end())
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
	}
}

static void export_tandem (mc_triangulation_t *pTri, char *filename)
{
	Che_mesh *he_mesh = pTri->he_mesh.get();
	int nvertices = pTri->nvertices;
	const float *vertices = pTri->vertices.data();

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
	for (std::map<int,int>::iterator it=he_mesh->FaceEdgeMap ()->begin ();
	     it != he_mesh->FaceEdgeMap ()->end ();
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
static void new_face_completed (void *pData)
{
	mc_triangulation_t *pTri = TandemTriangulation::FromData (pData);

	// update the half edge structure
	int findex = pTri->nfaces-1;
	tandem_add_face (pTri, findex, pTri->faces[3*findex], pTri->faces[3*findex+1], pTri->faces[3*findex+2]);
}

ImplicitSurfaceTandem::ImplicitSurfaceTandem ()
	: ImplicitSurface ()
{
	face_completed_func = new_face_completed;
	// La simplification se fait UNE fois, dans get_triangulation_post, sur le maillage
	// complet : le maillage en demi-aretes accumule toute la surface (aucune couche n'est
	// videe). Une variante en flux simplifierait ici les couches terminees.
}

ImplicitSurfaceTandem::~ImplicitSurfaceTandem ()
{
}

void ImplicitSurfaceTandem::get_triangulation_pre (void)
{
	data = static_cast<ImplicitTriangulation *> (new TandemTriangulation ());
}

bool ImplicitSurfaceTandem::get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces,
                                                    bool computed)
{
	std::unique_ptr<TandemTriangulation> tri (TandemTriangulation::FromData (data));
	data = nullptr;
	if (!computed)
		return false;

	// tandem_add_face agrandit les donnees par face et par sommet a chaque face : toute face
	// et tout sommet cite en ont. Faux, plutot qu'une lecture hors bornes, si ce n'est pas le cas.
	const std::size_t nf = static_cast<std::size_t> (tri->nfaces);
	const std::size_t nv = static_cast<std::size_t> (tri->nvertices);
	if (tri->farea.size () != nf || tri->fplane.size () != nf || tri->varea.size () != nv || tri->q.size () != nv)
		return false;

	// Simplification sur le maillage complet, puis sortie compacte rebatie depuis le
	// maillage en demi-aretes decime : les triangles survivants sont les entrees de
	// FaceEdgeMap (), les sommets contractes ne sont plus cites ; les sommets encore cites
	// sont renumerotes dans l'ordre de leur premiere citation.
	std::vector<int> remap;
	std::vector<unsigned int> outFaces;
	int newNV = 0;
	const int oldNV = tri->nvertices;
	try
	{
		tandem_simplify (tri.get ());
		Che_mesh *he_mesh = tri->he_mesh.get ();
		remap.assign (oldNV, -1);
		outFaces.reserve (3 * he_mesh->FaceEdgeMap ()->size ());
		for (std::map<int,int>::iterator it = he_mesh->FaceEdgeMap ()->begin();
		     it != he_mesh->FaceEdgeMap ()->end(); ++it)
		{
			int he_idx = it->second;
			Che_edge &he = he_mesh->edge(he_idx);
			if (!he.m_valid)
				continue;

			const int v1 = he.m_v_begin;
			const int v2 = he.m_v_end;
			const int v3 = he_mesh->edge(he.m_he_next).m_v_end;

			// triangles hors bornes ou degeneres ecartes
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
	}
	catch (const std::bad_alloc &)
	{
		return false;
	}

	// tampons d'au moins un element : free reste simple pour l'appelant
	const int outNF = (int)outFaces.size() / 3;
	float *outVerts = static_cast<float *> (AllocateImplicitOutput (3 * (newNV > 0 ? newNV : 1) * sizeof(float)));
	unsigned int *outFacesArr = static_cast<unsigned int *> (AllocateImplicitOutput (3 * (outNF > 0 ? outNF : 1) * sizeof(unsigned int)));
	if (!outVerts || !outFacesArr)
	{
		ReleaseImplicitOutput (outVerts);
		ReleaseImplicitOutput (outFacesArr);
		return false;
	}
	for (int v = 0; v < oldNV; v++)
		if (remap[v] >= 0)
		{
			outVerts[3*remap[v]]   = tri->vertices[3*v];
			outVerts[3*remap[v]+1] = tri->vertices[3*v+1];
			outVerts[3*remap[v]+2] = tri->vertices[3*v+2];
		}
	for (size_t i = 0; i < outFaces.size(); i++)
		outFacesArr[i] = outFaces[i];

	*nvertices = newNV;
	*vertices  = outVerts;
	*nfaces    = outNF;
	*faces     = outFacesArr;
	return true;
}
