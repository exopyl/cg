#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "mesh_half_edge.h"

static int is_visited (Che_edge *edge, Che_edge **array, int n)
{
	for (int i=0; i<n; i++)
		if (array[i] == edge)
			return 1;
		return 0;
}

/**
* Check the topology around the vertices.
*
* Fill the array m_topology_ok with 1 if the vertex has a correct topology, 0 otherwise.
*/
void
Mesh_half_edge::check_topology (void)
{
	m_topology_ok = (char*)malloc(m_nVertices*sizeof(char));
	for (int i=0; i<m_nVertices; i++)
	{
		m_topology_ok[i] = 1;
		
		Che_edge **visited_edges = NULL;
		int n_visited_edges = 0;
		
		Che_edge *e = m_edges_vertex[i];
		Che_edge *e_walk = e;
		if (!e) // isolated vertex
		{
			m_topology_ok[i] = 0;
			continue;
		}
		do
		{
			if (is_visited (e_walk, visited_edges, n_visited_edges))
			{
				printf ("not correct topology on vertex %d\n", i);
				m_topology_ok[i] = 0;
				break;
			}
			visited_edges = (Che_edge**)realloc(visited_edges, (n_visited_edges+1)*sizeof(Che_edge*));
			visited_edges[n_visited_edges] = e_walk;
			n_visited_edges++;
			e_walk = e_walk->m_he_next->m_he_next->m_pair;
		} while (e_walk && e_walk != e);
		free (visited_edges);
    }
}

/**
* Check the border.
*
* Fill the array border with 1 if the vertex belongs to the border, 0 otherwise.
*/
void
Mesh_half_edge::check_border (void)
{
	if (!m_topology_ok) check_topology ();
	m_border = (char*)malloc(m_nVertices*sizeof(char));
	for (int i=0; i<m_nVertices; i++)
	{
		if (!m_topology_ok[i])
			m_border[i] = 1;
		else
		{
			Che_edge *loc_e_walk = m_edges_vertex[i];
			if (!loc_e_walk)
				m_border[i] = 1;
			else
			{
				do {
					loc_e_walk = loc_e_walk->m_he_next->m_he_next->m_pair;
				} while (loc_e_walk && loc_e_walk != m_edges_vertex[i]);
				if (!loc_e_walk)
					m_border[i] = 1;
				else
					m_border[i] = 0;
			}
		}
	}
}

/**
* Constructor.
*/
Mesh_half_edge::Mesh_half_edge (int par_nv, float *par_v, int par_nf, unsigned int *par_f)
{
	SetVertices (par_nv, par_v);
	SetFaces (par_nf, 3, par_f);

	m_topology_ok = NULL;
	m_border = NULL;

	Che_mesh::create_half_edge (m_nVertices, m_nFaces, GetTriangles());
	check_border ();
}

Mesh_half_edge::Mesh_half_edge (Mesh *pMesh)
{
	if (!pMesh)
		return;

	SetVertices (pMesh->m_nVertices, pMesh->m_pVertices);
	SetFaces (pMesh->m_nFaces, 3, pMesh->GetTriangles());

	m_topology_ok = NULL;
	m_border = NULL;

	Che_mesh::create_half_edge (m_nVertices, m_nFaces, GetTriangles());
	check_border ();
}

/**
* Constructor.
*/
Mesh_half_edge::Mesh_half_edge ()
: Mesh ()
{
	m_topology_ok = NULL;
	m_border = NULL;

	Che_mesh::create_half_edge (m_nVertices, m_nFaces, GetTriangles());
	check_border ();
}

Mesh_half_edge::Mesh_half_edge (const char *par_filename)
{
	load (par_filename);

	m_topology_ok = NULL;
	m_border = NULL;

	Che_mesh::create_half_edge (m_nVertices, m_nFaces, GetTriangles());
	check_topology ();
	check_border ();
}

void Mesh_half_edge::create_half_edge (void)
{
	m_topology_ok = NULL;
	m_border = NULL;

	Che_mesh::create_half_edge (m_nVertices, m_nFaces, GetTriangles());
	check_topology ();
	check_border ();
}

// export statistics in html format
void Mesh_half_edge::export_statistics (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return;

	fprintf (ptr, "<html>\n");
	fprintf (ptr, "<head><title></title></head>\n");
	fprintf (ptr, "<body>\n");
	fprintf (ptr, "<p>number of vertices : %d\n", m_nVertices);
	fprintf (ptr, "<p>number of faces : %d\n", m_nFaces);


	computebbox ();
	float min[3], max[3];
	m_bbox.GetMinMax(min, max);
	fprintf (ptr, "<p>bbox :\n");
	fprintf (ptr, "<br> x : %f -> %f\n", min[0], max[0]);
	fprintf (ptr, "<br> y : %f -> %f\n", min[1], max[1]);
	fprintf (ptr, "<br> z : %f -> %f\n", min[2], max[2]);
	fprintf (ptr, "<br>diagonal length : %f\n", bbox_diagonal_length ());

	fprintf (ptr, "<p>center : (%.3f , %.3f , %.3f)\n", (max[0]+min[0])/2., (max[1]+min[1])/2., (max[2]+min[2])/2.);
	fprintf (ptr, "<p>dimensions : %.3f x %.3f x %.3f\n", max[0]-min[0], max[1]-min[1], max[2]-min[2]);

	//
	int nverticesinfaces = 13;
	int *verticesinfaces = (int*)malloc(nverticesinfaces*sizeof(int));
	stats_vertices_in_faces (verticesinfaces, nverticesinfaces);
	printf ("vertices in faces : ");
	for (int i=3; i<nverticesinfaces; i++)
	{
		fprintf (ptr, "%d", i);
		if (i==nverticesinfaces-1)
			fprintf (ptr, "+");
		fprintf (ptr, "/%d ", verticesinfaces[i]);
	}
	fprintf (ptr, "<br>");

	// materials
	fprintf (ptr, "<p>Materials : %d", m_nMaterials);
	for (int i=0; i<m_nMaterials; i++)
	{
		Material *pMaterial = (Material*)m_pMaterials[i];
		fprintf (ptr, "<br>material %d : %s", i, pMaterial->GetName());
	}


	fprintf (ptr, "</body>\n");
	fprintf (ptr, "</html>\n");

	fclose (ptr);
}


Che_edge *Mesh_half_edge::get_edge (unsigned int v1, unsigned int v2)
{
	if (!is_manifold (v1))
		return NULL;

	Che_edge *he = m_edges_vertex[v1];
	Che_edge *he_walk = he;
	int bFound = 0;
	do
	{
		if (he_walk == NULL)
			break;
		if (he_walk->m_v_end == v2)
		{
			bFound = 1;
			break;
		}
		
		he_walk = he_walk->m_he_next->m_he_next->m_pair;
	} while (he_walk != he);
	
	return (bFound)? he_walk : NULL;
}

/**
* Destructor.
*/
Mesh_half_edge::~Mesh_half_edge ()
{
}

////////////////////////////////////////////////////////////////////////////////
//
// methods inherited from Che_mesh
//
void Mesh_half_edge::edge_flip (Che_edge *par_edge)
{
	if (par_edge->m_pair == NULL)
		return;

	// update the faces
	unsigned int f1 = par_edge->m_face;
	unsigned int f1_v1 = par_edge->m_v_begin;
	unsigned int f1_v2 = par_edge->m_v_end;
	unsigned int f1_v3 = par_edge->m_he_next->m_v_end;

	unsigned int f2 = par_edge->m_pair->m_face;
	unsigned int f2_v1 = par_edge->m_pair->m_v_begin;
	unsigned int f2_v2 = par_edge->m_pair->m_v_end;
	unsigned int f2_v3 = par_edge->m_pair->m_he_next->m_v_end;

	m_pFaces[f1]->SetVertex (0, f2_v3);
	m_pFaces[f1]->SetVertex (1, f1_v3);
	m_pFaces[f1]->SetVertex (2, f1_v1);

	m_pFaces[f2]->SetVertex (0, f1_v3);
	m_pFaces[f2]->SetVertex (1, f2_v3);
	m_pFaces[f2]->SetVertex (2, f1_v2);

	// update the half edge mesh
	Che_mesh::edge_flip (par_edge);
}

void Mesh_half_edge::edge_split (Che_edge *par_edge)
{
}

void Mesh_half_edge::edge_contract (Che_edge *edge)
{
	if (edge->m_pair == NULL)
		return;

	// update the position of the vertex
	int iv1 = edge->m_v_begin;
	int iv2 = edge->m_v_end;

	m_pVertices[3*iv1]   = (m_pVertices[3*iv1] + m_pVertices[3*iv2])*0.5;
	m_pVertices[3*iv1+1] = (m_pVertices[3*iv1+1] + m_pVertices[3*iv2+1])*0.5;
	m_pVertices[3*iv1+2] = (m_pVertices[3*iv1+2] + m_pVertices[3*iv2+2])*0.5;

	// move all the vertices iv2 into iv1
	Che_edge *he = m_edges_vertex[iv2];
	Che_edge *he_walk = he;
	do
	{
		int f_walk = he_walk->m_face;
		for (int j=0; j<m_pFaces[f_walk]->m_nVertices; j++)
		{
			if (m_pFaces[f_walk]->m_pVertices[j] == iv2)
				m_pFaces[f_walk]->m_pVertices[j] = iv1;
		}
		he_walk = he_walk->m_he_next->m_he_next->m_pair;
	} while (he_walk != he);

	// delete the faces
	int f1 = edge->m_face;
	int f2 = edge->m_pair->m_face;

	delete m_pFaces[f1];
	m_pFaces[f1] = NULL;
	delete m_pFaces[f2];
	m_pFaces[f2] = NULL;

	// update the half edge mesh
	Che_mesh::edge_contract (edge);
}

//
////////////////////////////////////////////////////////////////////////////////


/**
 * Return the length of an edge.
 *
 * \param par_edge : an edge.
 *
 * \return the length of the edge.
 */
float Mesh_half_edge::edge_length (Che_edge *par_edge)
{
	if (par_edge == NULL)
		return -1.0;

	int iv1 = par_edge->m_v_begin;
	int iv2 = par_edge->m_v_end;
	vec3 loc_v1, loc_v2, loc_v1v2;
	vec3_init (loc_v1, m_pVertices[3*iv1], m_pVertices[3*iv1+1], m_pVertices[3*iv1+2]);
	vec3_init (loc_v2, m_pVertices[3*iv2], m_pVertices[3*iv2+1], m_pVertices[3*iv2+2]);
	vec3_subtraction (loc_v1v2, loc_v2, loc_v1);
	return vec3_length (loc_v1v2);
}

/**
*
* Get the average edges length.
*
*/
float Mesh_half_edge::get_average_edges_length (void)
{
	int loc_i;
	float loc_length = 0.0;
	for (loc_i=0; loc_i<m_ne; loc_i++)
	{
		Che_edge *loc_e_walk = m_edges[loc_i];
		Vector3d loc_v1 (m_pVertices[3*loc_e_walk->m_v_begin], m_pVertices[3*loc_e_walk->m_v_begin+1], m_pVertices[3*loc_e_walk->m_v_begin+2]);
		Vector3d loc_v2 (m_pVertices[3*loc_e_walk->m_v_end], m_pVertices[3*loc_e_walk->m_v_end+1], m_pVertices[3*loc_e_walk->m_v_end+2]);
		loc_v1 = loc_v2 - loc_v1;
		loc_length += loc_v1.getLength ();
	}
	return loc_length / m_ne;
}

/**
*
* Get the length of the shortest edge.
*
*/
float
Mesh_half_edge::get_shortest_edge_length (void)
{
	int loc_i;
	float loc_length = 0.0, loc_length_walk;
	Vector3d loc_v1 (m_pVertices[3*m_edges[0]->m_v_begin], m_pVertices[3*m_edges[0]->m_v_begin+1], m_pVertices[3*m_edges[0]->m_v_begin+2]);
	Vector3d loc_v2 (m_pVertices[3*m_edges[0]->m_v_end], m_pVertices[3*m_edges[0]->m_v_end+1], m_pVertices[3*m_edges[0]->m_v_end+2]);
	loc_v1 = loc_v2 - loc_v1;
	loc_length += loc_v1.getLength ();
	
	for (loc_i=0; loc_i<m_ne; loc_i++)
	{
		Che_edge *loc_e_walk = m_edges[loc_i];
		loc_v1.Set (m_pVertices[3*loc_e_walk->m_v_begin], m_pVertices[3*loc_e_walk->m_v_begin+1], m_pVertices[3*loc_e_walk->m_v_begin+2]);
		loc_v2.Set (m_pVertices[3*loc_e_walk->m_v_end], m_pVertices[3*loc_e_walk->m_v_end+1], m_pVertices[3*loc_e_walk->m_v_end+2]);
		loc_v1 = loc_v2 - loc_v1;
		loc_length_walk = loc_v1.getLength ();
		loc_length = (loc_length_walk < loc_length)? loc_length_walk : loc_length;
	}
	return loc_length;
}

/**
*
* Get the number of neighbours of a vertex.
*
*/
int Mesh_half_edge::get_n_neighbours (int par_ivertex)
{
	if (!m_topology_ok[par_ivertex] || m_border[par_ivertex]) return -1;
	int loc_n_neighbours = 0;
	Che_edge *loc_e = m_edges_vertex[par_ivertex];
	Che_edge *loc_e_walk = loc_e;
	do
	{
		loc_n_neighbours++;
		loc_e_walk = loc_e_walk->m_he_next->m_he_next->m_pair;
	} while (loc_e_walk && loc_e_walk != loc_e);
	return loc_n_neighbours;
}

// "Discrete differential-geometry operators for triangulated 2-manifolds"
// M Meyer, M Desbrun, P Schröder, AH Barr
// Visualization and mathematics III, 35-57
// http://www.multires.caltech.edu/pubs/diffGeoOps.pdf
//
//                      u * v
// cot(u,v) = ---------------------------
//             ||u||^2*||v||^2 - (u*v)^2
//
static double cotan (vec3 v1, vec3 v2)
{
	double v1dotv2 = vec3_dot_product (v1, v2);
	double denom = sqrt (vec3_dot_product(v1,v1)*vec3_dot_product(v2,v2) - v1dotv2*v1dotv2);

	return (denom == 0.0)? 0.0 : v1dotv2/denom;
}

//                * v1
//               / \
//              /   \
//             /     \
//            /       \
//           /         \
//          /  -edge->  \
// v_begin *-------------* v_end
//          \           /
//           \         /
//            \       /
//             \     /
//              \   /
//               \ /
//                * v2
double Mesh_half_edge::cotangent_weight_formula(Che_edge *edge)
{
	double cotangents = 0.;
	vec3 v_begin, v_end, v1, v2, tmp1, tmp2;

	assert (edge && edge->m_pair);

	GetVertex(edge->m_v_begin, v_begin);
	GetVertex(edge->m_v_end, v_end);
	GetVertex(edge->m_he_next->m_v_end, v1);
	GetVertex(edge->m_pair->m_he_next->m_v_end, v2);

	vec3_subtraction (tmp1, v_end, v1);
	vec3_subtraction (tmp2, v_begin, v1);
	cotangents += cotan(tmp1, tmp2);

	vec3_subtraction (tmp1, v_end, v2);
	vec3_subtraction (tmp2, v_begin, v2);
	cotangents += cotan(tmp1, tmp2);

	return .5*cotangents;
}


/*
*
* Dump information about the mesh.
*
*/
void Mesh_half_edge::dump (void)
{
	printf ("%d edges :\n", m_ne);
	for (int i=0; i<m_ne; i++)
		if (m_edges[i])
			m_edges[i]->dump ();

	check_topology ();
	for (int i=0; i<m_nVertices; i++)
		printf ("topology on vertex %d : %d\n", i, m_topology_ok[i]);

}
