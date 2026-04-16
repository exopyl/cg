#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#ifdef linux
#include <sys/time.h>
#endif // linux
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // WIN32

#include "half_edge.h"

/*** Begin class Cedges_visited ***/
Cedges_visited::Cedges_visited (int par_nv)
{
	int i;
	m_nv = par_nv;
	m_n_connected_to    = (int*)malloc(m_nv*sizeof(int));
	m_n_connections_max = (int*)malloc(m_nv*sizeof(int));
	m_connected_to      = (int**)malloc(m_nv*sizeof(int*));
	for (i=0; i<m_nv; i++)
	{
		m_n_connections_max[i] = 9;
		m_connected_to[i] = (int*)malloc(2*m_n_connections_max[i]*sizeof(int));
		m_n_connected_to[i] = 0;
	}
}

Cedges_visited::~Cedges_visited ()
{
	for (int i=0; i<m_nv; i++)
		if (m_connected_to[i])
			free (m_connected_to[i]);
	if (m_n_connected_to)
		free (m_n_connected_to);
	if (m_n_connections_max)
		free (m_n_connections_max);
}

void Cedges_visited::add_edge (int par_a, int par_b, int par_index)
{
	// update the connections to par_a
	if (m_n_connected_to[par_a] == m_n_connections_max[par_a])
	{
		m_n_connections_max[par_a] *= 2;
		m_connected_to[par_a] = (int*)realloc ((void*)m_connected_to[par_a], 2*m_n_connections_max[par_a]*sizeof(int));
	}
	m_connected_to[par_a][2*m_n_connected_to[par_a]]   = par_b;
	m_connected_to[par_a][2*m_n_connected_to[par_a]+1] = par_index;
	m_n_connected_to[par_a]++;

	// update the connections to par_b
	if (m_n_connected_to[par_b] == m_n_connections_max[par_b])
	{
		m_n_connections_max[par_b] *= 2;
		m_connected_to[par_b] = (int*)realloc ((void*)m_connected_to[par_b], 2*m_n_connections_max[par_b]*sizeof(int));
	}
	m_connected_to[par_b][2*m_n_connected_to[par_b]]   = par_a;
	m_connected_to[par_b][2*m_n_connected_to[par_b]+1] = par_index;
	m_n_connected_to[par_b]++;
}

void Cedges_visited::delete_edge (int par_a, int par_b)
{
	int i;

	// update the connections to a
	for (i=0; i<m_n_connected_to[par_a]; i++)
	{
		if (m_connected_to[par_a][2*i] == par_b)
		{
			m_connected_to[par_a][2*i]   = m_connected_to[par_a][2*(m_n_connected_to[par_a]-1)];
			m_connected_to[par_a][2*i+1] = m_connected_to[par_a][2*(m_n_connected_to[par_a]-1)+1];
			m_n_connected_to[par_a]--;
			break;
		}
	}

	// update the connections to b
	for (i=0; i<m_n_connected_to[par_b]; i++)
	{
		if (m_connected_to[par_b][2*i] == par_a)
		{
			m_connected_to[par_b][2*i]   = m_connected_to[par_b][2*(m_n_connected_to[par_b]-1)];
			m_connected_to[par_b][2*i+1] = m_connected_to[par_b][2*(m_n_connected_to[par_b]-1)+1];
			m_n_connected_to[par_b]--;
			break;
		}
	}
}

int Cedges_visited::is_edge_visited (int par_a, int par_b)
{
	int i;
	for (i=0; i<m_n_connected_to[par_a]; i++)
		if (m_connected_to[par_a][2*i] == par_b)
			return m_connected_to[par_a][2*i+1];
		return -1;
}
/*** End class Cedges_visited ***/


/**
 * Constructor
 */
Che_edge::Che_edge ()
{
	m_v_begin = -1;
	m_v_end = -1;
	m_pair = -1;
	m_face = -1;
	m_he_next = -1;
	m_visited = false;
	m_valid = 1;
	m_data = nullptr;
}

void Che_edge::dump (int index)
{
	printf ("edge [%d] : ", index);
	if (m_valid)
	{
		printf ("(face %d) (vertices %d -> %d) (pair %d) (next %d) (data : %p)\n",
			m_face, m_v_begin, m_v_end,
			m_pair, m_he_next,
			(void*)m_data);
	}
	else
	{
		printf("not valid\n");
	}
}

Che_mesh::Che_mesh ()
{
	m_ne = 0;
	m_map_edges = new map_edges;
	map_edges_vertex = new std::map<int,int>;
	map_edges_face = new std::map<int,int>;
}

Che_mesh::~Che_mesh ()
{
	if (m_map_edges) delete m_map_edges;
	if (map_edges_vertex) delete map_edges_vertex;
	if (map_edges_face) delete map_edges_face;
}

void Che_mesh::dump (void)
{
	printf ("map_edges :\n");
	for (map_edges::iterator it=m_map_edges->begin ();
	     it != m_map_edges->end ();
	     it++)
	{
		int idx = it->second;
		if (idx >= 0)
			m_edges[idx].dump (idx);
	}
}

void Che_mesh::dump_around_vertex (unsigned int vi)
{
	std::map<int,int>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
	{
		printf ("no edge starting from %d\n", vi);
		return;
	}

	int e = it->second;
	if (e < 0) // isolated vertex
	{
		printf ("no edge starting from %d (isolated vertex)\n", vi);
		return;
	}

	int e_walk = e;
	do
	{
		if (e_walk >= 0 && m_edges[e_walk].m_valid)
			m_edges[e_walk].dump (e_walk);

		int n1 = m_edges[e_walk].m_he_next;
		int n2 = m_edges[n1].m_he_next;
		e_walk = m_edges[n2].m_pair;
	} while (e_walk != e);
}

void Che_mesh::add_face (int fi, int v1, int v2, int v3)
{
	// Push 3 new edges into m_edges and record their indices
	int idx_v1v2 = (int)m_edges.size();
	m_edges.push_back(Che_edge());
	int idx_v2v3 = (int)m_edges.size();
	m_edges.push_back(Che_edge());
	int idx_v3v1 = (int)m_edges.size();
	m_edges.push_back(Che_edge());

	m_edges[idx_v1v2].m_v_begin = v1;
	m_edges[idx_v1v2].m_v_end = v2;
	m_edges[idx_v1v2].m_he_next = idx_v2v3;
	m_edges[idx_v1v2].m_face = fi;

	map_edges::iterator it;
	it = m_map_edges->find (std::make_pair(v2, v1));
	if (it != m_map_edges->end ())
	{
		m_edges[idx_v1v2].m_pair = it->second;
		m_edges[it->second].m_pair = idx_v1v2;
	}

	m_edges[idx_v2v3].m_v_begin = v2;
	m_edges[idx_v2v3].m_v_end = v3;
	m_edges[idx_v2v3].m_he_next = idx_v3v1;
	m_edges[idx_v2v3].m_face = fi;
	it = m_map_edges->find (std::make_pair(v3, v2));
	if (it != m_map_edges->end ())
	{
		m_edges[idx_v2v3].m_pair = it->second;
		m_edges[it->second].m_pair = idx_v2v3;
	}

	m_edges[idx_v3v1].m_v_begin = v3;
	m_edges[idx_v3v1].m_v_end = v1;
	m_edges[idx_v3v1].m_he_next = idx_v1v2;
	m_edges[idx_v3v1].m_face = fi;
	it = m_map_edges->find (std::make_pair(v1, v3));
	if (it != m_map_edges->end ())
	{
		m_edges[idx_v3v1].m_pair = it->second;
		m_edges[it->second].m_pair = idx_v3v1;
	}

	m_map_edges->insert (std::make_pair (std::make_pair(v1,v2), idx_v1v2));
	m_map_edges->insert (std::make_pair (std::make_pair(v2,v3), idx_v2v3));
	m_map_edges->insert (std::make_pair (std::make_pair(v3,v1), idx_v3v1));

	map_edges_vertex->insert (std::make_pair (v1, idx_v1v2));
	map_edges_vertex->insert (std::make_pair (v2, idx_v2v3));
	map_edges_vertex->insert (std::make_pair (v3, idx_v3v1));
	map_edges_face->insert (std::make_pair (fi, idx_v1v2));
}

int Che_mesh::is_border (int vi)
{
	std::map<int,int>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
		return -1;

	int e = it->second;
	if (e < 0) // isolated vertex
		return 1;

	int e_walk = e;
	do
	{
		if (e_walk < 0 || !m_edges[e_walk].m_valid)
			return 1;
		int n1 = m_edges[e_walk].m_he_next;
		int n2 = m_edges[n1].m_he_next;
		e_walk = m_edges[n2].m_pair;
	} while (e_walk != e);

 	return 0;
}

int Che_mesh::vertex_is_near_border (int vi)
{
	std::map<int,int>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end ())
		return -1;

	int e = it->second;
	int e_walk = e;
	do
	{
		if (is_border (m_edges[e_walk].m_v_end))
			return 1;

		int n1 = m_edges[e_walk].m_he_next;
		int n2 = m_edges[n1].m_he_next;
		e_walk = m_edges[n2].m_pair;
	} while (e_walk != e);

	return 0;
}

// search - returns edge index, -1 if not found
int Che_mesh::get_edge (int v1, int v2)
{
	map_edges::iterator it = m_map_edges->find (std::make_pair(v1, v2));
	return (it == m_map_edges->end())? -1 : it->second;
}

int Che_mesh::get_edge_from_vertex (int vi)
{
	std::map<int,int>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
		return -1;
	return it->second;
}

int Che_mesh::get_edge_from_face (int fi)
{
	std::map<int,int>::iterator it = map_edges_face->find (fi);
	if (it == map_edges_face->end())
		return -1;
	return it->second;
}

/**
 * Create the half edge structure from the arrays containing the vertices and the faces.
 */
void Che_mesh::create_half_edge (unsigned int nVertices, unsigned int nFaces, unsigned int *pFaces)
{
	if (!m_map_edges)
		m_map_edges = new map_edges;
	int i;

	// init
	m_edges_vertex.assign(nVertices, -1);
	m_edges_face.assign(nFaces, -1);
	m_ne = 3*nFaces;
	m_edges.resize(m_ne);

	for (i=0; i<(int)nFaces; i++)
	{
		int loc_a = pFaces[3*i];
		int loc_b = pFaces[3*i+1];
		int loc_c = pFaces[3*i+2];

		// Note: add_face uses m_edges.push_back, but we already sized m_edges.
		// Instead, populate m_edges directly and fill maps manually.

		int e0 = 3*i;
		int e1 = 3*i+1;
		int e2 = 3*i+2;

		m_edges[e0] = Che_edge();
		m_edges[e0].m_v_begin = loc_a;
		m_edges[e0].m_v_end   = loc_b;
		m_edges[e0].m_face    = i;
		m_edges_vertex[loc_a] = e0;

		m_edges[e1] = Che_edge();
		m_edges[e1].m_v_begin = loc_b;
		m_edges[e1].m_v_end   = loc_c;
		m_edges[e1].m_face    = i;
		m_edges_vertex[loc_b] = e1;

		m_edges[e2] = Che_edge();
		m_edges[e2].m_v_begin = loc_c;
		m_edges[e2].m_v_end   = loc_a;
		m_edges[e2].m_face    = i;
		m_edges_vertex[loc_c] = e2;

		m_edges[e0].m_he_next = e1;
		m_edges[e1].m_he_next = e2;
		m_edges[e2].m_he_next = e0;

		m_edges_face[i] = e2;
	}

	// build the pair links
	Cedges_visited *loc_ev = new Cedges_visited (nVertices);
	for (i=0; i<3*(int)nFaces; i++)
	{
		int loc_a = m_edges[i].m_v_begin;
		int loc_b = m_edges[i].m_v_end;

		int index = loc_ev->is_edge_visited (loc_a, loc_b);
		if (index != -1)
		{
			m_edges[i].m_pair     = index;
			m_edges[index].m_pair = i;
			loc_ev->delete_edge (loc_a, loc_b);
		}
		else
			loc_ev->add_edge (loc_a, loc_b, i);
	}
	delete loc_ev;

	// populate the maps from m_edges for lookup functions
	m_map_edges->clear();
	map_edges_vertex->clear();
	map_edges_face->clear();
	for (i=0; i<m_ne; i++)
	{
		m_map_edges->insert(std::make_pair(
			std::make_pair(m_edges[i].m_v_begin, m_edges[i].m_v_end), i));
	}
	for (i=0; i<(int)nVertices; i++)
	{
		if (m_edges_vertex[i] >= 0)
			map_edges_vertex->insert(std::make_pair(i, m_edges_vertex[i]));
	}
	for (i=0; i<(int)nFaces; i++)
	{
		if (m_edges_face[i] >= 0)
			map_edges_face->insert(std::make_pair(i, m_edges_face[i]));
	}
}


/**
* Flip an edge.
*/
void Che_mesh::edge_flip (int par_edge)
{
	if (m_edges[par_edge].m_pair < 0)
		return;

	// get the half edges
	int loc_e1 = m_edges[par_edge].m_he_next;
	int loc_e2 = m_edges[loc_e1].m_he_next;
	int loc_ep = m_edges[par_edge].m_pair;
	int loc_e3 = m_edges[loc_ep].m_he_next;
	int loc_e4 = m_edges[loc_e3].m_he_next;

	// update the links
	m_edges[par_edge].m_he_next = loc_e2;
	m_edges[loc_e2].m_he_next   = loc_e3;
	m_edges[loc_e3].m_he_next   = par_edge;

	m_edges[loc_ep].m_he_next   = loc_e4;
	m_edges[loc_e4].m_he_next   = loc_e1;
	m_edges[loc_e1].m_he_next   = loc_ep;

	// update the extremities
	m_edges[par_edge].m_v_begin = m_edges[loc_e3].m_v_end;
	m_edges[par_edge].m_v_end   = m_edges[loc_e2].m_v_begin;
	m_edges[loc_ep].m_v_begin   = m_edges[loc_e1].m_v_end;
	m_edges[loc_ep].m_v_end     = m_edges[loc_e4].m_v_begin;

	// update the faces
	int loc_f1 = m_edges[par_edge].m_face;
	int loc_f2 = m_edges[loc_ep].m_face;

	m_edges[loc_e1].m_face = loc_f2;
	m_edges[loc_e2].m_face = loc_f1;
	m_edges[loc_e3].m_face = loc_f1;
	m_edges[loc_e4].m_face = loc_f2;

	// update half edge from vertex
	m_edges_vertex[m_edges[loc_e1].m_v_begin] = loc_e1;
	m_edges_vertex[m_edges[loc_e2].m_v_begin] = loc_e2;
	m_edges_vertex[m_edges[loc_e3].m_v_begin] = loc_e3;
	m_edges_vertex[m_edges[loc_e4].m_v_begin] = loc_e4;

	// update half edge from face
	m_edges_face[loc_f1] = par_edge;
	m_edges_face[loc_f2] = loc_ep;
}

void Che_mesh::edge_split (int par_edge)
{
	if (m_edges[par_edge].m_pair < 0)
		return;
}

void Che_mesh::edge_contract (int ei)
{
	if (m_edges[ei].m_pair < 0)
		return;

	// vertices implied in the edge
	int iv1 = m_edges[ei].m_v_begin;
	int iv2 = m_edges[ei].m_v_end;

	// faces implied in the edge
	int f1 = m_edges[ei].m_face;
	int f2 = m_edges[m_edges[ei].m_pair].m_face;

	// init the half edges
	int e1 = m_edges[ei].m_he_next;
	int e2 = m_edges[e1].m_he_next;
	int ep = m_edges[ei].m_pair;
	int e3 = m_edges[ep].m_he_next;
	int e4 = m_edges[e3].m_he_next;

	int e1p = m_edges[e1].m_pair;
	int e2p = m_edges[e2].m_pair;
	int e3p = m_edges[e3].m_pair;
	int e4p = m_edges[e4].m_pair;

	// update the links between the remaining half edges
	if (e1p >= 0) m_edges[e1p].m_pair = e2p;
	if (e2p >= 0) m_edges[e2p].m_pair = e1p;
	if (e3p >= 0) m_edges[e3p].m_pair = e4p;
	if (e4p >= 0) m_edges[e4p].m_pair = e3p;
	if (e1p >= 0) m_edges[e1p].m_v_end   = iv1;
	if (e4p >= 0) m_edges[e4p].m_v_begin = iv1;

	m_edges_vertex[iv1] = e4p;
	m_edges_vertex[iv2] = -1;

	// mark edges as invalid
	m_edges[e1].m_valid = 0;
	m_edges[e2].m_valid = 0;
	m_edges[e3].m_valid = 0;
	m_edges[e4].m_valid = 0;
	m_edges[ei].m_valid = 0;
	m_edges[ep].m_valid = 0;

	m_edges_face[f1] = -1;
	m_edges_face[f2] = -1;
}

int Che_mesh::is_edge_contract2_valid (int ei)
{
	if (!m_edges[ei].m_valid)
	{
		printf ("invalid edge : [%d] : %d -> %d\n", ei, m_edges[ei].m_v_begin, m_edges[ei].m_v_end);
		return 0;
	}

	if (m_edges[ei].m_pair < 0)
		return 0;

	int iv1 = m_edges[ei].m_v_begin;
	int iv2 = m_edges[ei].m_v_end;

	int iv3 = m_edges[m_edges[ei].m_he_next].m_v_end;
	int iv4 = m_edges[m_edges[m_edges[ei].m_pair].m_he_next].m_v_end;

	int e1 = get_edge_from_vertex (iv1);
	int e2 = get_edge_from_vertex (iv2);
	if (e1 < 0 || e2 < 0)
		return 0;

	int walk1 = e1;
	do {
		int vwalk1 = m_edges[walk1].m_v_end;
		if (vwalk1 == iv1 || vwalk1 == iv3 || vwalk1 == iv4)
		{
			walk1 = m_edges[walk1].m_pair;
			continue;
		}

		int walk2 = e2;
		do {
			int vwalk2 = m_edges[walk2].m_v_end;
			if (vwalk1 == vwalk2)
				return 0;

			walk2 = m_edges[walk2].m_pair;
		} while (walk2 != e2);

		walk1 = m_edges[walk1].m_pair;
	} while (walk1 != e1);

	return 1;
}

int Che_mesh::edge_contract2 (int ei)
{
	if (!is_edge_contract2_valid (ei))
		return -1;

	int iv1 = m_edges[ei].m_v_begin;
	int iv2 = m_edges[ei].m_v_end;

	int iv3 = m_edges[m_edges[ei].m_he_next].m_v_end;
	int iv4 = m_edges[m_edges[m_edges[ei].m_pair].m_he_next].m_v_end;

	int f1 = m_edges[ei].m_face;
	int f2 = m_edges[m_edges[ei].m_pair].m_face;

	int e1 = m_edges[ei].m_he_next;
	int e2 = m_edges[e1].m_he_next;
	int ep = m_edges[ei].m_pair;
	int e3 = m_edges[ep].m_he_next;
	int e4 = m_edges[e3].m_he_next;

	int e1p = m_edges[e1].m_pair;
	int e2p = m_edges[e2].m_pair;
	int e3p = m_edges[e3].m_pair;
	int e4p = m_edges[e4].m_pair;

	// replace v2 by v1 all around v2
	{
		int walk = ep;
		do {
			map_edges::iterator it;
			it = m_map_edges->find (std::make_pair(m_edges[walk].m_v_begin, m_edges[walk].m_v_end));
			if (it != m_map_edges->end ())
				m_map_edges->erase (it);

			if (walk != e1 && walk != ep)
			{
				m_edges[walk].m_v_begin = iv1;
				m_map_edges->insert (std::make_pair(std::make_pair(m_edges[walk].m_v_begin, m_edges[walk].m_v_end), walk));
			}

			int n1 = m_edges[walk].m_he_next;
			walk = m_edges[n1].m_he_next;

			it = m_map_edges->find (std::make_pair(m_edges[walk].m_v_begin, m_edges[walk].m_v_end));
			if (it != m_map_edges->end ())
				m_map_edges->erase (it);

			if (walk != ei && walk != e4)
			{
				m_edges[walk].m_v_end = iv1;
				m_map_edges->insert (std::make_pair(std::make_pair(m_edges[walk].m_v_begin, m_edges[walk].m_v_end), walk));
			}

			walk = m_edges[walk].m_pair;
		} while (walk != ep);
	}

	// update the links between the remaining half edges
	if (e1p >= 0) m_edges[e1p].m_pair = e2p;
	if (e2p >= 0) m_edges[e2p].m_pair = e1p;
	if (e3p >= 0) m_edges[e3p].m_pair = e4p;
	if (e4p >= 0) m_edges[e4p].m_pair = e3p;

	// mark as invalid
	m_edges[e1].m_valid = 0;
	m_edges[e2].m_valid = 0;
	m_edges[e3].m_valid = 0;
	m_edges[e4].m_valid = 0;
	m_edges[ei].m_valid = 0;
	m_edges[ep].m_valid = 0;

	map_edges_vertex->erase (iv1);
	map_edges_vertex->insert (std::make_pair (iv1, e4p));
	map_edges_vertex->erase (iv2);

	map_edges_vertex->erase (iv3);
	map_edges_vertex->insert (std::make_pair (iv3, e1p));
	map_edges_vertex->erase (iv4);
	map_edges_vertex->insert (std::make_pair (iv4, e3p));

	map_edges_face->erase (f1);
	map_edges_face->erase (f2);

	return 0;
}
