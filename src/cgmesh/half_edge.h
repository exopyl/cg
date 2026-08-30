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
	// m_valid AVANT m_data, et ce n'est pas cosmetique : place APRES le
	// pointeur il tirait un bourrage de fin, et l'arete pesait 40 octets sur
	// cible 64 bits au lieu de 32 (mesure : tu_cgmesh_he.cpp). Loge dans le trou
	// d'alignement qui precede le pointeur, il ne coute rien.
	//
	// ⚠ Ce gain est propre aux cibles ou alignof(void*) > alignof(int). Sur
	// wasm32 les deux valent 4 : l'ordre n'y change RIEN (28 octets dans les
	// deux dispositions, mesure par mallinfo). Ne pas en attendre un gain la-bas.
	char m_valid;
	// Charge utile OPAQUE, propriete de l'appelant. Seul surface_implicit_tandem
	// s'en sert : il y accroche un edge_data_t partage avec l'arete opposee et
	// le libere lui-meme. Che_edge ne la possede pas et ne la detruit pas.
	void *m_data;

	void dump (int index);
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
	// ⚠ Ces deux recherches CONSTRUISENT les cartes au premier appel (cf. plus
	// bas). Un parcours de 1-anneau depuis m_edges_vertex ne coute rien ; passer
	// par ici coute 275 Mio sur 2 M de triangles.
	int get_edge (int v1, int v2);
	int get_edge_from_vertex (int vi);

	// basic operations (take edge index)
	void edge_flip     (int e);
	void edge_split    (int e);
	void edge_contract (int e);

	int is_edge_contract2_valid (int edge);
	int edge_contract2 (int e);

	typedef std::map<std::pair<int,int>,int> map_edges;

	////////////////////////////////////////////////////////////////////////////
	//
	// Cartes d'indexation -- PARESSEUSES
	//
	// Elles ne portent AUCUNE information que m_edges, m_edges_vertex et
	// m_edges_face n'aient deja : create_half_edge les remplissait a partir
	// d'eux, en fin de construction. Elles pesent pourtant 275 Mio sur un
	// maillage de 2 M de triangles, soit plus que tout le reste de la structure
	// reunie -- un noeud std::map par arete, par sommet et par face.
	//
	// Elles sont donc construites au PREMIER acces. La grande majorite des
	// algorithmes qui prennent un Che_mesh se contentent de m_edges et de
	// m_edges_vertex, et ne paient rien.
	//
	// ⚠ Passer par ces trois accesseurs, JAMAIS par les membres : lire une carte
	// non construite rendrait un conteneur VIDE, donc « arete introuvable » --
	// une reponse fausse et silencieuse. C'est pourquoi les membres sont prives.
	//
	map_edges         *EdgeMap ();        //!< (v_begin, v_end) -> indice d'arete
	std::map<int,int> *VertexEdgeMap ();  //!< sommet -> une arete sortante
	std::map<int,int> *FaceEdgeMap ();    //!< face -> une de ses aretes

public:
	int m_ne;
	std::vector<Che_edge> m_edges;
	std::vector<int> m_edges_vertex; // per-vertex: index of one outgoing edge (-1 if none)
	std::vector<int> m_edges_face;   // per-face: index of one edge (-1 if none)

private:
	// Construit les trois cartes si elles ne le sont pas. Reproduit A L'IDENTIQUE
	// le remplissage que create_half_edge faisait en fin de construction, y
	// compris sa borne m_ne -- qui n'est PAS m_edges.size() des qu'un add_face
	// est passe par la.
	void EnsureMaps ();

	// Faux tant que les cartes n'ont pas ete construites depuis les vecteurs.
	// create_half_edge le remet a faux : il refait les vecteurs sous elles.
	bool m_maps_built;

	std::map<int,int> *map_edges_vertex;
	std::map<int,int> *map_edges_face;
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
