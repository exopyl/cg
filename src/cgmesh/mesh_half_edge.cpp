#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "mesh_half_edge.h"

static int is_visited (int edge, int *array, int n)
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
	if (!m_topology_ok.empty()) return;
	
	m_topology_ok.resize(m_pMesh->m_nVertices, 1);
	for (unsigned int i=0; i<m_pMesh->m_nVertices; i++)
	{
		int *visited_edges = NULL;
		int n_visited_edges = 0;

		int e = GetCheMesh()->m_edges_vertex[i];
		int e_walk = e;
		if (e < 0) // isolated vertex
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
			visited_edges = (int*)realloc(visited_edges, (n_visited_edges+1)*sizeof(int));
			visited_edges[n_visited_edges] = e_walk;
			n_visited_edges++;
			int n1 = GetCheMesh()->edge(e_walk).m_he_next;
			int n2 = GetCheMesh()->edge(n1).m_he_next;
			e_walk = GetCheMesh()->edge(n2).m_pair;
		} while (e_walk >= 0 && e_walk != e);
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
	if (!m_border.empty()) return;
	check_topology ();
	
	m_border.resize(m_pMesh->m_nVertices);
	for (unsigned int i=0; i<m_pMesh->m_nVertices; i++)
	{
		if (!m_topology_ok[i])
			m_border[i] = 1;
		else
		{
			int loc_e_walk = GetCheMesh()->m_edges_vertex[i];
			if (loc_e_walk < 0)
				m_border[i] = 1;
			else
			{
				do {
					int n1 = GetCheMesh()->edge(loc_e_walk).m_he_next;
					int n2 = GetCheMesh()->edge(n1).m_he_next;
					loc_e_walk = GetCheMesh()->edge(n2).m_pair;
				} while (loc_e_walk >= 0 && loc_e_walk != GetCheMesh()->m_edges_vertex[i]);
				if (loc_e_walk < 0)
					m_border[i] = 1;
				else
					m_border[i] = 0;
			}
		}
	}
}

Che_mesh* Mesh_half_edge::GetCheMesh()
{
	if (!m_pCheMesh)
	{
		m_pCheMesh = std::make_unique<Che_mesh>();
		m_pCheMesh->create_half_edge (m_pMesh->m_nVertices, m_pMesh->m_nFaces, m_pMesh->GetTriangles());
	}
	return m_pCheMesh.get();
}

bool Mesh_half_edge::is_manifold (unsigned int i)
{
	check_topology ();
	if (i >= m_topology_ok.size()) return false;
	return m_topology_ok[i] != 0;
}

bool Mesh_half_edge::is_border (unsigned int i)
{
	check_border ();
	if (i >= m_border.size()) return false;
	return m_border[i] != 0;
}

/**
* Constructor.
*/
Mesh_half_edge::Mesh_half_edge (int par_nv, float *par_v, int par_nf, unsigned int *par_f)
{
	m_pMesh = new Mesh();

	m_pMesh->SetVertices (par_nv, par_v);
	m_pMesh->SetFaces (par_nf, 3, par_f);
}

Mesh_half_edge::Mesh_half_edge (Mesh *pMesh)
{
	m_pMesh = new Mesh();

	m_pMesh->Init();

	if (!pMesh)
	{
		return;
	}

	m_pMesh->SetVertices (pMesh->m_nVertices, pMesh->m_pVertices);
	m_pMesh->SetVertexNormals(pMesh->m_nVertices, pMesh->m_pVertexNormals);
	m_pMesh->SetFaces (pMesh->m_nFaces, 3, pMesh->GetTriangles());
	m_pMesh->InitVertexColors();
}

/**
* Constructor.
*/
Mesh_half_edge::Mesh_half_edge ()
{
	m_pMesh = new Mesh();
}

Mesh_half_edge::Mesh_half_edge (const char *par_filename)
{
	m_pMesh = new Mesh();
	m_pMesh->load (par_filename);
}

void Mesh_half_edge::create_half_edge (void)
{
	m_topology_ok.clear();
	m_border.clear();
	m_pCheMesh.reset(); // Force re-creation on next GetCheMesh() call
}

// export statistics in html format
void Mesh_half_edge::export_statistics (const std::string & filename)
{
	FILE *ptr = fopen (filename.c_str(), "w");
	if (!ptr)
		return;

	fprintf (ptr, "<html>\n");
	fprintf (ptr, "<head><title></title></head>\n");
	fprintf (ptr, "<body>\n");
	fprintf (ptr, "<p>number of vertices : %d\n", m_pMesh->m_nVertices);
	fprintf (ptr, "<p>number of faces : %d\n", m_pMesh->m_nFaces);


	m_pMesh->computebbox ();
	float min[3], max[3];
	m_pMesh->m_bbox.GetMinMax(min, max);
	fprintf (ptr, "<p>bbox :\n");
	fprintf (ptr, "<br> x : %f -> %f\n", min[0], max[0]);
	fprintf (ptr, "<br> y : %f -> %f\n", min[1], max[1]);
	fprintf (ptr, "<br> z : %f -> %f\n", min[2], max[2]);
	fprintf (ptr, "<br>diagonal length : %f\n", m_pMesh->bbox_diagonal_length ());

	fprintf (ptr, "<p>center : (%.3f , %.3f , %.3f)\n", (max[0]+min[0])/2., (max[1]+min[1])/2., (max[2]+min[2])/2.);
	fprintf (ptr, "<p>dimensions : %.3f x %.3f x %.3f\n", max[0]-min[0], max[1]-min[1], max[2]-min[2]);

	//
	int nverticesinfaces = 13;
	int *verticesinfaces = (int*)malloc(nverticesinfaces*sizeof(int));
	m_pMesh->stats_vertices_in_faces (verticesinfaces, nverticesinfaces);
	printf ("vertices in faces : ");
	for (int i=3; i<nverticesinfaces; i++)
	{
		fprintf (ptr, "%d", i);
		if (i==nverticesinfaces-1)
			fprintf (ptr, "+");
		fprintf (ptr, "/%d ", verticesinfaces[i]);
	}
	fprintf (ptr, "<br>");
	free(verticesinfaces);

	// materials
	fprintf (ptr, "<p>Materials : %d", m_pMesh->m_nMaterials);
	for (unsigned int i=0; i<m_pMesh->m_nMaterials; i++)
	{
		Material *pMaterial = (Material*)m_pMesh->m_pMaterials[i];
		fprintf (ptr, "<br>material %d : %s", i, pMaterial->GetName().c_str());
	}


	fprintf (ptr, "</body>\n");
	fprintf (ptr, "</html>\n");

	fclose (ptr);
}


int Mesh_half_edge::get_edge (unsigned int v1, unsigned int v2)
{
	if (!is_manifold (v1))
		return -1;

	int he = GetCheMesh()->m_edges_vertex[v1];
	int he_walk = he;
	int bFound = 0;
	do
	{
		if (he_walk < 0)
			break;
		if (GetCheMesh()->edge(he_walk).m_v_end == (int)v2)
		{
			bFound = 1;
			break;
		}

		int n1 = GetCheMesh()->edge(he_walk).m_he_next;
		int n2 = GetCheMesh()->edge(n1).m_he_next;
		he_walk = GetCheMesh()->edge(n2).m_pair;
	} while (he_walk != he);

	return (bFound)? he_walk : -1;
}

/**
* Destructor.
*/
Mesh_half_edge::~Mesh_half_edge ()
{
	delete m_pMesh;
}

////////////////////////////////////////////////////////////////////////////////
//
// half-edge operations
//
void Mesh_half_edge::edge_flip (int par_edge)
{
	if (GetCheMesh()->edge(par_edge).m_pair < 0)
		return;

	// update the faces
	unsigned int f1 = GetCheMesh()->edge(par_edge).m_face;
	unsigned int f1_v1 = GetCheMesh()->edge(par_edge).m_v_begin;
	unsigned int f1_v2 = GetCheMesh()->edge(par_edge).m_v_end;
	unsigned int f1_v3 = GetCheMesh()->edge(GetCheMesh()->edge(par_edge).m_he_next).m_v_end;

	int pair = GetCheMesh()->edge(par_edge).m_pair;
	unsigned int f2 = GetCheMesh()->edge(pair).m_face;
	unsigned int f2_v1 = GetCheMesh()->edge(pair).m_v_begin;
	unsigned int f2_v2 = GetCheMesh()->edge(pair).m_v_end;
	unsigned int f2_v3 = GetCheMesh()->edge(GetCheMesh()->edge(pair).m_he_next).m_v_end;

	m_pMesh->m_pFaces[f1]->SetVertex (0, f2_v3);
	m_pMesh->m_pFaces[f1]->SetVertex (1, f1_v3);
	m_pMesh->m_pFaces[f1]->SetVertex (2, f1_v1);

	m_pMesh->m_pFaces[f2]->SetVertex (0, f1_v3);
	m_pMesh->m_pFaces[f2]->SetVertex (1, f2_v3);
	m_pMesh->m_pFaces[f2]->SetVertex (2, f1_v2);

	// update the half edge mesh
	GetCheMesh()->edge_flip (par_edge);
}

void Mesh_half_edge::edge_split (int par_edge)
{
}

void Mesh_half_edge::edge_contract (int ei)
{
	if (GetCheMesh()->edge(ei).m_pair < 0)
		return;

	// update the position of the vertex
	int iv1 = GetCheMesh()->edge(ei).m_v_begin;
	int iv2 = GetCheMesh()->edge(ei).m_v_end;

	m_pMesh->m_pVertices[3*iv1]   = (m_pMesh->m_pVertices[3*iv1] + m_pMesh->m_pVertices[3*iv2])*0.5;
	m_pMesh->m_pVertices[3*iv1+1] = (m_pMesh->m_pVertices[3*iv1+1] + m_pMesh->m_pVertices[3*iv2+1])*0.5;
	m_pMesh->m_pVertices[3*iv1+2] = (m_pMesh->m_pVertices[3*iv1+2] + m_pMesh->m_pVertices[3*iv2+2])*0.5;

	// move all the vertices iv2 into iv1
	int he = GetCheMesh()->m_edges_vertex[iv2];
	int he_walk = he;
	do
	{
		int f_walk = GetCheMesh()->edge(he_walk).m_face;
		for (int j=0; j<m_pMesh->m_pFaces[f_walk]->m_nVertices; j++)
		{
			if (m_pMesh->m_pFaces[f_walk]->m_pVertices[j] == iv2)
				m_pMesh->m_pFaces[f_walk]->m_pVertices[j] = iv1;
		}
		int n1 = GetCheMesh()->edge(he_walk).m_he_next;
		int n2 = GetCheMesh()->edge(n1).m_he_next;
		he_walk = GetCheMesh()->edge(n2).m_pair;
	} while (he_walk != he);

	// delete the faces
	int f1 = GetCheMesh()->edge(ei).m_face;
	int f2 = GetCheMesh()->edge(GetCheMesh()->edge(ei).m_pair).m_face;

	delete m_pMesh->m_pFaces[f1];
	m_pMesh->m_pFaces[f1] = NULL;
	delete m_pMesh->m_pFaces[f2];
	m_pMesh->m_pFaces[f2] = NULL;

	// update the half edge mesh
	GetCheMesh()->edge_contract (ei);
}

//
////////////////////////////////////////////////////////////////////////////////


/**
 * Return the length of an edge.
 */
float Mesh_half_edge::edge_length (int par_edge)
{
	if (par_edge < 0)
		return -1.0;

	int iv1 = GetCheMesh()->edge(par_edge).m_v_begin;
	int iv2 = GetCheMesh()->edge(par_edge).m_v_end;
	vec3 loc_v1, loc_v2, loc_v1v2;
	vec3_init (loc_v1, m_pMesh->m_pVertices[3*iv1], m_pMesh->m_pVertices[3*iv1+1], m_pMesh->m_pVertices[3*iv1+2]);
	vec3_init (loc_v2, m_pMesh->m_pVertices[3*iv2], m_pMesh->m_pVertices[3*iv2+1], m_pMesh->m_pVertices[3*iv2+2]);
	vec3_subtraction (loc_v1v2, loc_v2, loc_v1);
	return vec3_length (loc_v1v2);
}

/**
* Get the average edges length.
*/
float Mesh_half_edge::get_average_edges_length (void)
{
	int loc_i;
	float loc_length = 0.0;
	for (loc_i=0; loc_i<GetCheMesh()->m_ne; loc_i++)
	{
		Che_edge &e = GetCheMesh()->edge(loc_i);
		Vector3d loc_v1 (m_pMesh->m_pVertices[3*e.m_v_begin], m_pMesh->m_pVertices[3*e.m_v_begin+1], m_pMesh->m_pVertices[3*e.m_v_begin+2]);
		Vector3d loc_v2 (m_pMesh->m_pVertices[3*e.m_v_end], m_pMesh->m_pVertices[3*e.m_v_end+1], m_pMesh->m_pVertices[3*e.m_v_end+2]);
		loc_v1 = loc_v2 - loc_v1;
		loc_length += loc_v1.getLength ();
	}
	return loc_length / GetCheMesh()->m_ne;
}

/**
* Get the length of the shortest edge.
*/
float
Mesh_half_edge::get_shortest_edge_length (void)
{
	int loc_i;
	float loc_length = 0.0, loc_length_walk;
	{
		Che_edge &e0 = GetCheMesh()->edge(0);
		Vector3d loc_v1 (m_pMesh->m_pVertices[3*e0.m_v_begin], m_pMesh->m_pVertices[3*e0.m_v_begin+1], m_pMesh->m_pVertices[3*e0.m_v_begin+2]);
		Vector3d loc_v2 (m_pMesh->m_pVertices[3*e0.m_v_end], m_pMesh->m_pVertices[3*e0.m_v_end+1], m_pMesh->m_pVertices[3*e0.m_v_end+2]);
		loc_v1 = loc_v2 - loc_v1;
		loc_length = loc_v1.getLength ();
	}

	for (loc_i=0; loc_i<GetCheMesh()->m_ne; loc_i++)
	{
		Che_edge &e = GetCheMesh()->edge(loc_i);
		Vector3d loc_v1 (m_pMesh->m_pVertices[3*e.m_v_begin], m_pMesh->m_pVertices[3*e.m_v_begin+1], m_pMesh->m_pVertices[3*e.m_v_begin+2]);
		Vector3d loc_v2 (m_pMesh->m_pVertices[3*e.m_v_end], m_pMesh->m_pVertices[3*e.m_v_end+1], m_pMesh->m_pVertices[3*e.m_v_end+2]);
		loc_v1 = loc_v2 - loc_v1;
		loc_length_walk = loc_v1.getLength ();
		loc_length = (loc_length_walk < loc_length)? loc_length_walk : loc_length;
	}
	return loc_length;
}

/**
* Get the number of neighbours of a vertex.
*/
int Mesh_half_edge::get_n_neighbours (int par_ivertex)
{
	if (!is_manifold (par_ivertex) || is_border (par_ivertex)) return -1;
	int loc_n_neighbours = 0;
	int loc_e = GetCheMesh()->m_edges_vertex[par_ivertex];
	int loc_e_walk = loc_e;
	do
	{
		loc_n_neighbours++;
		int n1 = GetCheMesh()->edge(loc_e_walk).m_he_next;
		int n2 = GetCheMesh()->edge(n1).m_he_next;
		loc_e_walk = GetCheMesh()->edge(n2).m_pair;
	} while (loc_e_walk >= 0 && loc_e_walk != loc_e);
	return loc_n_neighbours;
}

// "Discrete differential-geometry operators for triangulated 2-manifolds"
// M Meyer, M Desbrun, P Schroder, AH Barr
static double cotan (vec3 v1, vec3 v2)
{
	double v1dotv2 = vec3_dot_product (v1, v2);
	double denom = sqrt (vec3_dot_product(v1,v1)*vec3_dot_product(v2,v2) - v1dotv2*v1dotv2);

	return (denom == 0.0)? 0.0 : v1dotv2/denom;
}

double Mesh_half_edge::cotangent_weight_formula(int ei)
{
	double cotangents = 0.;
	vec3 v_begin, v_end, v1, v2, tmp1, tmp2;

	assert (ei >= 0 && GetCheMesh()->edge(ei).m_pair >= 0);

	Che_edge &e = GetCheMesh()->edge(ei);
	int pair = e.m_pair;
	Che_edge &ep = GetCheMesh()->edge(pair);

	m_pMesh->GetVertex(e.m_v_begin, v_begin);
	m_pMesh->GetVertex(e.m_v_end, v_end);
	m_pMesh->GetVertex(GetCheMesh()->edge(e.m_he_next).m_v_end, v1);
	m_pMesh->GetVertex(GetCheMesh()->edge(ep.m_he_next).m_v_end, v2);

	vec3_subtraction (tmp1, v_end, v1);
	vec3_subtraction (tmp2, v_begin, v1);
	cotangents += cotan(tmp1, tmp2);

	vec3_subtraction (tmp1, v_end, v2);
	vec3_subtraction (tmp2, v_begin, v2);
	cotangents += cotan(tmp1, tmp2);

	return .5*cotangents;
}


void Mesh_half_edge::dump (void)
{
	printf ("%d edges :\n", GetCheMesh()->m_ne);
	for (int i=0; i<GetCheMesh()->m_ne; i++)
		if (GetCheMesh()->edge(i).m_valid)
			GetCheMesh()->edge(i).dump (i);

	check_topology ();
	for (unsigned int i=0; i<m_pMesh->m_nVertices; i++)
		printf ("topology on vertex %d : %d\n", i, m_topology_ok[i]);

}
