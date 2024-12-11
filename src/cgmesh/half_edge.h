#ifndef __HALF_EDGE_H__
#define __HALF_EDGE_H__

#include <map>

//
// Half edge structure
//
class Che_edge
{
public:
	Che_edge ();
	~Che_edge ();

public:
	int m_v_begin;	      //!< vertex at the beginning of the half-edge
	int m_v_end;	      //!< vertex at the end of the half-edge
	Che_edge *m_pair;     //!< opposite half-edge
	int m_face;			  //!< index of the face
	Che_edge *m_he_next;  //!< next half-edge around the face
	bool m_visited;	      //!< flag
	int m_flag;
	void *m_data;

	void dump (void);

	char m_valid;
};

//
// Half edge mesh structure
//
class Che_mesh
{
	friend class Citerator_half_edges_vertex;

public:
	Che_mesh ();
	~Che_mesh ();

	void dump (void);
	void dump_around_vertex (unsigned int vi);

	void create_half_edge (unsigned int nVertices, unsigned int nFaces, unsigned int *pFaces);
	void add_face (int fi, int v1, int v2, int v3);
	
	int is_border (int v1); 
	int vertex_is_near_border (int vi);

	// get
	Che_edge* get_edge (int v1, int v2);
	Che_edge* get_edge_from_vertex (int vi);
	Che_edge* get_edge_from_face (int fi);

	// basic operations
	void edge_flip     (Che_edge *e);
	void edge_split    (Che_edge *e);
	void edge_contract (Che_edge *e);

	int is_edge_contract2_valid (Che_edge *edge);
	int edge_contract2 (Che_edge *e);

public:
	int m_ne;
	Che_edge **m_edges;
	//unsigned int n_vertices;
	Che_edge **m_edges_vertex;
	//unsigned int n_faces;
	Che_edge **m_edges_face;

	std::map<int,Che_edge*> *map_edges_vertex;
	std::map<int,Che_edge*> *map_edges_face;
	typedef std::map<std::pair<int,int>,Che_edge*> map_edges;
	map_edges *m_map_edges;
};

//
// Half edge iterator
//
// This iterator is used to visit the vertices around a vertex identified by the index par_ivertex.
//
class Citerator_half_edges_vertex
{
public:

	Citerator_half_edges_vertex (Che_mesh *par_model, int par_ivertex)
	{
		m_he_first = par_model->m_edges_vertex[par_ivertex];
		m_he_current = m_he_first;
		m_is_last = false;
	};

	Che_edge* first (void) { return m_he_first; };
	bool isLast (void) { return m_is_last; };

	Che_edge* prev (void) {
		m_he_current = m_he_current->m_pair->m_he_next;
		return m_he_current;
	};

	Che_edge* next (void) {
		m_he_current = m_he_current->m_he_next->m_he_next->m_pair;
		if (m_he_current == m_he_first) m_is_last = true;
		return m_he_current; 
	};

	Che_edge* current (void) { return m_he_current; };

private:
	Che_edge* m_he_first;
	Che_edge* m_he_current;
	bool m_is_last;
};

// cache for half edges
class Cedges_visited
{
public:
	Cedges_visited (int par_nv);
	~Cedges_visited ();

	void add_edge        (int par_a, int par_b, int par_index);
	void delete_edge     (int par_a, int par_b);
	int  is_edge_visited (int par_a, int par_b);

private:
	int **m_connected_to;
	int *m_n_connected_to;
	int *m_n_connections_max;
	int m_nv;
};

#endif // __HALF_EDGE_H__

