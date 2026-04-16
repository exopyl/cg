#pragma once
#include <map>
#include <vector>

//
// Half edge structure
//
// m_pair and m_he_next are indices into the Che_mesh::m_edges vector.
// -1 means "no edge" (replaces nullptr pointers).
//
class Che_edge
{
public:
	Che_edge ();

public:
	int m_v_begin;	      //!< vertex at the beginning of the half-edge
	int m_v_end;	      //!< vertex at the end of the half-edge
	int m_pair;           //!< index of opposite half-edge (-1 if none)
	int m_face;           //!< index of the face
	int m_he_next;        //!< index of next half-edge around the face (-1 if none)
	bool m_visited;	      //!< flag
	int m_flag;
	void *m_data;

	void dump (int index);

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

	// inline helper: access edge by index
	inline Che_edge& edge (int i) { return m_edges[i]; }
	inline const Che_edge& edge (int i) const { return m_edges[i]; }

	void dump (void);
	void dump_around_vertex (unsigned int vi);

	void create_half_edge (unsigned int nVertices, unsigned int nFaces, unsigned int *pFaces);
	void add_face (int fi, int v1, int v2, int v3);

	int is_border (int v1);
	int vertex_is_near_border (int vi);

	// get (return edge index, -1 if not found)
	int get_edge (int v1, int v2);
	int get_edge_from_vertex (int vi);
	int get_edge_from_face (int fi);

	// basic operations (take edge index)
	void edge_flip     (int e);
	void edge_split    (int e);
	void edge_contract (int e);

	int is_edge_contract2_valid (int edge);
	int edge_contract2 (int e);

public:
	int m_ne;
	std::vector<Che_edge> m_edges;
	std::vector<int> m_edges_vertex; // per-vertex: index of one outgoing edge (-1 if none)
	std::vector<int> m_edges_face;   // per-face: index of one edge (-1 if none)

	std::map<int,int> *map_edges_vertex;
	std::map<int,int> *map_edges_face;
	typedef std::map<std::pair<int,int>,int> map_edges;
	map_edges *m_map_edges;
};

//
// Half edge iterator
//
// This iterator is used to visit the vertices around a vertex identified by the index par_ivertex.
// All methods return edge indices (-1 if none).
//
class Citerator_half_edges_vertex
{
public:

	Citerator_half_edges_vertex (Che_mesh *par_model, int par_ivertex)
	{
		m_mesh = par_model;
		m_he_first = par_model->m_edges_vertex[par_ivertex];
		m_he_current = m_he_first;
		m_is_last = false;
	};

	int first (void) { return m_he_first; };
	bool isLast (void) { return m_is_last; };

	int prev (void) {
		int pair = m_mesh->edge(m_he_current).m_pair;
		m_he_current = m_mesh->edge(pair).m_he_next;
		return m_he_current;
	};

	int next (void) {
		int n1 = m_mesh->edge(m_he_current).m_he_next;
		int n2 = m_mesh->edge(n1).m_he_next;
		m_he_current = m_mesh->edge(n2).m_pair;
		if (m_he_current == m_he_first) m_is_last = true;
		return m_he_current;
	};

	int current (void) { return m_he_current; };

private:
	Che_mesh *m_mesh;
	int m_he_first;
	int m_he_current;
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
