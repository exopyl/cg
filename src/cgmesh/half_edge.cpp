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
	m_pair = NULL;
	m_face = -1;
	m_he_next = NULL;
	m_visited = false;
	m_valid = 1;
	m_data = NULL;
}

Che_edge::~Che_edge ()
{
}

void Che_edge::dump (void)
{
	printf ("edge 0x%x : ", this);
	if (m_valid)
	{
		printf ("(face %d) (vertices %d -> %d) (pair 0x%x %d -> %d) (next 0x%x %d -> %d) (data : %x)\n",
			m_face, m_v_begin, m_v_end,
			m_pair, (m_pair)?m_pair->m_v_begin:-1, (m_pair)?m_pair->m_v_end:-1,
			m_he_next, (m_he_next)?m_he_next->m_v_begin:-1, (m_he_next)?m_he_next->m_v_end:-1,
			m_data);
	}
	else
		printf ("not valid\n", m_valid);
}

Che_mesh::Che_mesh ()
{
	m_edges = NULL;
	//n_vertices = 0;
	m_edges_vertex = NULL;
	//n_faces = 0;
	m_edges_face = NULL;
	m_map_edges = new map_edges;
	map_edges_vertex = new std::map<int,Che_edge*>;
	map_edges_face = new std::map<int,Che_edge*>;
}

Che_mesh::~Che_mesh ()
{
	if (m_edges)
	{
		for (int i=0; i<m_ne; i++)
		{
			delete m_edges[i];
			m_edges[i] = NULL;
		}
		free (m_edges);
	}
}

void Che_mesh::dump (void)
{
	printf ("map_edges :\n");
	for (std::map<std::pair<int,int>,Che_edge*>::iterator it=m_map_edges->begin ();
	     it != m_map_edges->end ();
	     it++)
	{
		Che_edge *he = it->second;
		if (he)
			he->dump ();
	}
}

void Che_mesh::dump_around_vertex (unsigned int vi)
{
	std::map<int,Che_edge*>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
	{
		printf ("no edge starting from %d\n", vi);
		return;
	}
		
	Che_edge *e = it->second;
	if (!e) // isolated vertex
	{
		printf ("no edge starting from %d (isolated vertex)\n", vi);
		return;
	}

	Che_edge *e_walk = e;
	do
	{
		if (e_walk && e_walk->m_valid)
			e_walk->dump ();

		e_walk = e_walk->m_he_next->m_he_next->m_pair;
	} while (e_walk != e);
}

void Che_mesh::add_face (int fi, int v1, int v2, int v3)
{
	std::map<std::pair<int,int>,Che_edge*>::iterator it;

	Che_edge *e_v1v2 = new Che_edge ();
	Che_edge *e_v2v3 = new Che_edge ();
	Che_edge *e_v3v1 = new Che_edge ();

	e_v1v2->m_v_begin = v1;
	e_v1v2->m_v_end = v2;
	e_v1v2->m_he_next = e_v2v3;
	e_v1v2->m_face = fi;
	it = m_map_edges->find (std::make_pair(v2, v1));
	if (it != m_map_edges->end ())
	{
		e_v1v2->m_pair = it->second;
		it->second->m_pair = e_v1v2;
	}

	e_v2v3->m_v_begin = v2;
	e_v2v3->m_v_end = v3;
	e_v2v3->m_he_next = e_v3v1;
	e_v2v3->m_face = fi;
	it = m_map_edges->find (std::make_pair(v3, v2));
	if (it != m_map_edges->end ())
	{
		e_v2v3->m_pair = it->second;
		it->second->m_pair = e_v2v3;
	}

	e_v3v1->m_v_begin = v3;
	e_v3v1->m_v_end = v1;
	e_v3v1->m_he_next = e_v1v2;
	e_v3v1->m_face = fi;
	it = m_map_edges->find (std::make_pair(v1, v3));
	if (it != m_map_edges->end ())
	{
		e_v3v1->m_pair = it->second;
		it->second->m_pair = e_v3v1;
	}

	m_map_edges->insert (std::make_pair (std::make_pair(v1,v2), e_v1v2));
	m_map_edges->insert (std::make_pair (std::make_pair(v2,v3), e_v2v3));
	m_map_edges->insert (std::make_pair (std::make_pair(v3,v1), e_v3v1));

	map_edges_vertex->insert (std::make_pair (v1, e_v1v2));
	map_edges_vertex->insert (std::make_pair (v2, e_v2v3));
	map_edges_vertex->insert (std::make_pair (v3, e_v3v1));
	map_edges_face->insert (std::make_pair (fi, e_v1v2));
}

int Che_mesh::is_border (int vi)
{
	std::map<int,Che_edge*>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
		return -1;
		
	Che_edge *e = it->second;
	if (!e) // isolated vertex
		return 1;

	Che_edge *e_walk = e;
	do
	{
		if (e_walk == NULL || !e_walk->m_valid)
			return 1;
		e_walk = e_walk->m_he_next->m_he_next->m_pair;
	} while (e_walk != e);

 	return 0;
}

int Che_mesh::vertex_is_near_border (int vi)
{
	std::map<int,Che_edge*>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end ())
		return -1;

	Che_edge *e = it->second;
	Che_edge *e_walk = e;
	do
	{
		if (is_border (e_walk->m_v_end))
			return 1;
		
		e_walk = e_walk->m_he_next->m_he_next->m_pair;
	} while (e_walk != e);

	return 0;
}

// search
Che_edge* Che_mesh::get_edge (int v1, int v2)
{
	std::map<std::pair<int,int>,Che_edge*>::iterator it = m_map_edges->find (std::make_pair(v1, v2));
	return (it == m_map_edges->end())? NULL : it->second;
}

Che_edge* Che_mesh::get_edge_from_vertex (int vi)
{
	std::map<int,Che_edge*>::iterator it = map_edges_vertex->find (vi);
	if (it == map_edges_vertex->end())
		return NULL;
	return it->second;
}

Che_edge* Che_mesh::get_edge_from_face (int fi)
{
	std::map<int,Che_edge*>::iterator it = map_edges_face->find (fi);
	if (it == map_edges_vertex->end())
		return NULL;
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

	// init the half edge structure
	m_edges_vertex = (Che_edge**)malloc(nVertices*sizeof(Che_edge*));
	m_edges_face   = (Che_edge**)malloc(nFaces*sizeof(Che_edge*));
	m_ne           = 3*nFaces;
	m_edges        = (Che_edge**)malloc(m_ne*sizeof(Che_edge*));
	for (i=0; i<nVertices; i++)
		m_edges_vertex[i] = NULL;
	for (i=0; i<nFaces; i++)
	{
		int loc_a = pFaces[3*i];
		int loc_b = pFaces[3*i+1];
		int loc_c = pFaces[3*i+2];

		add_face (i, loc_a, loc_b, loc_c); // fill m_map_edges
		
		m_edges[3*i] = new Che_edge ();
		m_edges[3*i]->m_v_begin = loc_a;
		m_edges[3*i]->m_v_end   = loc_b;
		m_edges[3*i]->m_face    = i;
		m_edges_vertex[loc_a] = m_edges[3*i];
		
		m_edges[3*i+1] = new Che_edge ();
		m_edges[3*i+1]->m_v_begin = loc_b;
		m_edges[3*i+1]->m_v_end   = loc_c;
		m_edges[3*i+1]->m_face    = i;
		m_edges_vertex[loc_b] = m_edges[3*i+1];
		
		m_edges[3*i+2] = new Che_edge ();
		m_edges[3*i+2]->m_v_begin = loc_c;
		m_edges[3*i+2]->m_v_end   = loc_a;
		m_edges[3*i+2]->m_face    = i;
		m_edges_vertex[loc_c] = m_edges[3*i+2];
		
		m_edges[3*i]->m_he_next   = m_edges[3*i+1];
		m_edges[3*i+1]->m_he_next = m_edges[3*i+2];
		m_edges[3*i+2]->m_he_next = m_edges[3*i];
		
		m_edges_face[i] = m_edges[3*i+2];
	}
	
	// build the half edge structure
	Cedges_visited *loc_ev = new Cedges_visited (nVertices);
	for (i=0; i<3*nFaces; i++)
	{
		int loc_a,loc_b,index;
		loc_a = m_edges[i]->m_v_begin;
		loc_b = m_edges[i]->m_v_end;
		
		index = loc_ev->is_edge_visited (loc_a,loc_b);
		if (index != -1)
		{
			m_edges[i]->m_pair	= m_edges[index];
			m_edges[index]->m_pair	= m_edges[i];
			loc_ev->delete_edge (loc_a,loc_b);
		}
		else
			loc_ev->add_edge (loc_a,loc_b,i);
	}
	delete loc_ev;
}


/**
* Flip an edge.
*
\verbatim
       *                     *
      /|\                   / \ 
     / | \                 /   \ 
 e1 /  |  \ e4         e1 /     \ e4
   /   |   \             /  ep   \ 
  * edge    *    --->   *---------*
   \   |ep /             \ edge  /
 e2 \  |  / e3         e2 \     / e3
     \ | /                 \   /
      \|/                   \ /
       *                     *
\endverbatim
*/
void Che_mesh::edge_flip (Che_edge *par_edge)
{
	if (par_edge->m_pair == NULL)
		return;

	// get the half edges
	Che_edge *loc_e1 = par_edge->m_he_next;
	Che_edge *loc_e2 = loc_e1->m_he_next;
	Che_edge *loc_ep = par_edge->m_pair;
	Che_edge *loc_e3 = loc_ep->m_he_next;
	Che_edge *loc_e4 = loc_e3->m_he_next;

	// update the links
	par_edge->m_he_next	= loc_e2;
	loc_e2->m_he_next	= loc_e3;
	loc_e3->m_he_next	= par_edge;

	loc_ep->m_he_next	= loc_e4;
	loc_e4->m_he_next	= loc_e1;
	loc_e1->m_he_next	= loc_ep;

	// update the extremities
	par_edge->m_v_begin	= loc_e3->m_v_end;
	par_edge->m_v_end	= loc_e2->m_v_begin;
	loc_ep->m_v_begin	= loc_e1->m_v_end;
	loc_ep->m_v_end		= loc_e4->m_v_begin;

	// update the faces
	int loc_f1 = par_edge->m_face;
	int loc_f2 = loc_ep->m_face;

	loc_e1->m_face = loc_f2;
	loc_e2->m_face = loc_f1;
	loc_e3->m_face = loc_f1;
	loc_e4->m_face = loc_f2;
/*
	m_pFaces[3*loc_f1]->SetVertex (0, loc_e4->m_v_begin);
	m_pFaces[3*loc_f1]->SetVertex (1, loc_e2->m_v_begin);
	m_pFaces[3*loc_f1]->SetVertex (2, loc_e3->m_v_begin);

	m_pFaces[3*loc_f2]->SetVertex (0, loc_e2->m_v_begin);
	m_pFaces[3*loc_f2]->SetVertex (1, loc_e4->m_v_begin);
	m_pFaces[3*loc_f2]->SetVertex (2, loc_e1->m_v_begin);
*/
	// update half edge from vertex
	m_edges_vertex[loc_e1->m_v_begin] = loc_e1;
	m_edges_vertex[loc_e2->m_v_begin] = loc_e2;
	m_edges_vertex[loc_e3->m_v_begin] = loc_e3;
	m_edges_vertex[loc_e4->m_v_begin] = loc_e4;

	// update half edge from face (not necessary)
	m_edges_face[loc_f1] = par_edge;
	m_edges_face[loc_f2] = loc_ep;
}

/**
* Split an edge.
*
\verbatim

       *                     *
      / \                   /|\ 
     /   \                 / | \ 
 e4 /     \ e3         e4 /  |  \ e3
   /  ep   \             /   |   \ 
  *---------*    --->   *----*----*
   \ edge  /             \   |   /
 e1 \     / e2         e1 \  |  / e2
     \   /                 \ | /
      \ /                   \|/
       *                     *

\endverbatim
*/
void Che_mesh::edge_split (Che_edge *par_edge)
{
	if (par_edge->m_pair == NULL)
		return;
}

/**
* Contract an edge.
*
\verbatim
    *---------*
    |\       /|
    | \     / |
    |  \v2 /  |
    |   \ /   |
    |    *    |          *---------*
    |e1p/|\e4p|          |\       /|
    |  / | \  |          | \     / |
    | /e1|e4\ |          |  \   /  |
    |/   |   \|          |e1p\ /e4p|
 v3 *   e|ep  * v4--->   *----*----*
    |\   |   /|          |e2p/ \e3p|
    | \e2|e3/ |          |  /   \  |
    |  \ | /  |          | /     \ |
    |e2p\|/e3p|          |/       \|
    |    *    |          *---------*
    |   / \   |
    |  / v1\  |
    | /     \ |
    |/       \|
    *---------*
\endverbatim
*/
void Che_mesh::edge_contract (Che_edge *edge)
{
	if (edge->m_pair == NULL)
		return;

	// vertices implied in the edge
	int iv1 = edge->m_v_begin;
	int iv2 = edge->m_v_end;
	
	// faces implied int the edge
	int f1 = edge->m_face;
	int f2 = edge->m_pair->m_face;

	// init the half edges
	Che_edge *e1 = edge->m_he_next;
	Che_edge *e2 = e1->m_he_next;
	Che_edge *ep = edge->m_pair;
	Che_edge *e3 = ep->m_he_next;
	Che_edge *e4 = e3->m_he_next;

	Che_edge *e1p = e1->m_pair;
	Che_edge *e2p = e2->m_pair;
	Che_edge *e3p = e3->m_pair;
	Che_edge *e4p = e4->m_pair;

	// update the links between the remaining half edges
	if (e1p != NULL) e1p->m_pair = e2p;
	if (e2p != NULL) e2p->m_pair = e1p;
	if (e3p != NULL) e3p->m_pair = e4p;
	if (e4p != NULL) e4p->m_pair = e3p;
	if (e1p != NULL) e1p->m_v_end   = iv1;
	if (e4p != NULL) e4p->m_v_begin = iv1;

/*
	// update the vertex
	m_pVertices[3*iv1]   = (m_pVertices[3*iv1] + m_pVertices[3*iv2])*0.5;
	m_pVertices[3*iv1+1] = (m_pVertices[3*iv1+1] + m_pVertices[3*iv2+1])*0.5;
	m_pVertices[3*iv1+2] = (m_pVertices[3*iv1+2] + m_pVertices[3*iv2+2])*0.5;
*/
	m_edges_vertex[iv1] = e4p;//NULL;
	m_edges_vertex[iv2] = NULL;

	// delete e1, e2, e3, e4, edge and ep
	/*
	delete e1;		e1   = NULL;
	delete e2;		e2   = NULL;
	delete e3;		e3   = NULL;
	delete e4;		e4   = NULL;
	delete edge;	        edge = NULL;
	delete ep;		ep   = NULL;
	*/
	e1->m_valid = 0;
	e2->m_valid = 0;
	e3->m_valid = 0;
	e4->m_valid = 0;
	edge->m_valid = 0;
	ep->m_valid = 0;

/*
	// init the faces
	int f1 = par_edge->m_face;
	int f2 = ep->m_face;

	// delete f1 and f2
	m_pFaces[3*f1]->SetVertex (0, m_pFaces[3*(m_nFaces-1)]->GetVertex (0));
	m_pFaces[3*f1]->SetVertex (1, m_pFaces[3*(m_nFaces-1)]->GetVertex (1));
	m_pFaces[3*f1]->SetVertex (2, m_pFaces[3*(m_nFaces-1)]->GetVertex (2));

	m_pFaces[3*f2]->SetVertex (0, m_pFaces[3*(m_nFaces-2)]->GetVertex (0));
	m_pFaces[3*f2]->SetVertex (1, m_pFaces[3*(m_nFaces-2)]->GetVertex (1));
	m_pFaces[3*f2]->SetVertex (2, m_pFaces[3*(m_nFaces-2)]->GetVertex (2));

	m_nFaces -= 2;
*/
	m_edges_face[f1] = NULL;
       	m_edges_face[f2] = NULL;
}

int Che_mesh::is_edge_contract2_valid (Che_edge *edge)
{
	if (!edge->m_valid)
	{
		printf ("unvalid edge : 0x%x : %d -> %d\n", edge, edge->m_v_begin, edge->m_v_end);
		return 0;
	}

	if (edge->m_pair == NULL)
		return 0;
	// vertices implied in the edge
	int iv1 = edge->m_v_begin;
	int iv2 = edge->m_v_end;
	
	int iv3 = edge->m_he_next->m_v_end;
	int iv4 = edge->m_pair->m_he_next->m_v_end;
	
	Che_edge *e1 = get_edge_from_vertex (iv1);
	Che_edge *e2 = get_edge_from_vertex (iv2);
	if (!e1 || !e2)
		return 0;

	Che_edge *walk1 = e1;
	do {
		int vwalk1 = walk1->m_v_end;
		if (vwalk1 == iv1 || vwalk1 == iv3 || vwalk1 == iv4)
			continue;

		Che_edge *walk2 = e2;
		do {
			int vwalk2 = walk1->m_v_end;
			if (vwalk1 == vwalk2)
				return 0;

			walk2 = walk2->m_pair;
		} while (walk2 != e2);

		walk1 = walk1->m_pair;
	} while (walk1 != e1);

	return 1;
}

int Che_mesh::edge_contract2 (Che_edge *edge)
{
	if (!is_edge_contract2_valid (edge))
		return -1;

	// vertices implied in the edge
	int iv1 = edge->m_v_begin;
	int iv2 = edge->m_v_end;
	
	int iv3 = edge->m_he_next->m_v_end;
	int iv4 = edge->m_pair->m_he_next->m_v_end;

	// faces implied int the edge
	int f1 = edge->m_face;
	int f2 = edge->m_pair->m_face;

	// init the half edges
	Che_edge *e1 = edge->m_he_next;
	Che_edge *e2 = e1->m_he_next;
	Che_edge *ep = edge->m_pair;
	Che_edge *e3 = ep->m_he_next;
	Che_edge *e4 = e3->m_he_next;

	Che_edge *e1p = e1->m_pair;
	Che_edge *e2p = e2->m_pair;
	Che_edge *e3p = e3->m_pair;
	Che_edge *e4p = e4->m_pair;

	// replace v2 by v1 all around v2
	{
		Che_edge *walk = edge->m_pair;
		do {
			std::map<std::pair<int,int>,Che_edge*>::iterator it;
			it = m_map_edges->find (std::make_pair(walk->m_v_begin, walk->m_v_end));
			if (it != m_map_edges->end ())
				m_map_edges->erase (it);


			//printf ("%x %x\n", walk, edge->m_pair);
			if (walk != e1 && walk != edge->m_pair)
			{
				walk->m_v_begin = iv1;
				m_map_edges->insert (std::make_pair(std::make_pair(walk->m_v_begin, walk->m_v_end), walk));
			}
			
			walk = walk->m_he_next->m_he_next;

			it = m_map_edges->find (std::make_pair(walk->m_v_begin, walk->m_v_end));
			if (it != m_map_edges->end ())
				m_map_edges->erase (it);

			if (walk != edge && walk != e4)
			{
				walk->m_v_end = iv1;
				m_map_edges->insert (std::make_pair(std::make_pair(walk->m_v_begin, walk->m_v_end), walk));
			}

			walk = walk->m_pair;
		} while (walk != edge->m_pair);
/*
		walk = edge;
		do {
			//printf ("%x %x\n", walk, edge);
			walk = walk->m_he_next->m_he_next;
			walk = walk->m_pair;
		} while (walk != edge);
*/
	}

	// update the links between the remaining half edges
	if (e1p) e1p->m_pair = e2p;
	if (e2p) e2p->m_pair = e1p;
	if (e3p) e3p->m_pair = e4p;
	if (e4p) e4p->m_pair = e3p;
	//if (e1p) e1p->m_v_end   = iv1;
	//if (e4p) e4p->m_v_begin = iv1;


	// discard edge, ep, e1, e2, e3 & e4
/*
	if (1) {
		Che_edge *edges_to_delete[6] = {e1, e2, e3, e4, edge, ep};
		for (int i=4; i<6; i++)
		{
			Che_edge *e = edges_to_delete[i];
			e->dump ();
			std::map<std::pair<int,int>,Che_edge*>::iterator it;
			printf ("%d %d\n", e->m_v_begin, e->m_v_end);
			it = m_map_edges->find (std::make_pair(e->m_v_begin, e->m_v_end));
			if (it != m_map_edges->end ())
			{
				delete (it->second);
				m_map_edges->erase (it);
			}
		}
	}
	else
	{
		std::map<std::pair<int,int>,Che_edge*>::iterator it;
		it = m_map_edges->find (std::make_pair(iv1, iv2));
		if (it != m_map_edges->end ())
		{
			delete (it->second);
			m_map_edges->erase (it);
		}
		it = m_map_edges->find (std::make_pair(iv2, iv1));
		if (it != m_map_edges->end ())
		{
			delete (it->second);
			m_map_edges->erase (it);
		}
	}
*/


	e1->m_valid = 0;
	e2->m_valid = 0;
	e3->m_valid = 0;
	e4->m_valid = 0;
	edge->m_valid = 0;
	ep->m_valid = 0;



/*
	// update the vertex
	m_pVertices[3*iv1]   = (m_pVertices[3*iv1] + m_pVertices[3*iv2])*0.5;
	m_pVertices[3*iv1+1] = (m_pVertices[3*iv1+1] + m_pVertices[3*iv2+1])*0.5;
	m_pVertices[3*iv1+2] = (m_pVertices[3*iv1+2] + m_pVertices[3*iv2+2])*0.5;
*/
	map_edges_vertex->erase (iv1);
	map_edges_vertex->insert (std::make_pair (iv1, e4p));
	map_edges_vertex->erase (iv2);

	map_edges_vertex->erase (iv3);
	map_edges_vertex->insert (std::make_pair (iv3, e1p));
	map_edges_vertex->erase (iv4);
	map_edges_vertex->insert (std::make_pair (iv4, e3p));

	// delete e1, e2, e3, e4, edge and ep
/*
	delete e1;		e1   = NULL;
	delete e2;		e2   = NULL;
	delete e3;		e3   = NULL;
	delete e4;		e4   = NULL;
	delete edge;	        edge = NULL;
	delete ep;		ep   = NULL;
*/

/*
	// init the faces
	int f1 = par_edge->m_face;
	int f2 = ep->m_face;

	// delete f1 and f2
	m_pFaces[3*f1]->SetVertex (0, m_pFaces[3*(m_nFaces-1)]->GetVertex (0));
	m_pFaces[3*f1]->SetVertex (1, m_pFaces[3*(m_nFaces-1)]->GetVertex (1));
	m_pFaces[3*f1]->SetVertex (2, m_pFaces[3*(m_nFaces-1)]->GetVertex (2));

	m_pFaces[3*f2]->SetVertex (0, m_pFaces[3*(m_nFaces-2)]->GetVertex (0));
	m_pFaces[3*f2]->SetVertex (1, m_pFaces[3*(m_nFaces-2)]->GetVertex (1));
	m_pFaces[3*f2]->SetVertex (2, m_pFaces[3*(m_nFaces-2)]->GetVertex (2));

	m_nFaces -= 2;
*/
	map_edges_face->erase (f1);
	map_edges_face->erase (f2);

	return 0;
}

