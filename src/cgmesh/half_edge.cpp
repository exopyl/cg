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

/**
 * Apparie les demi-aretes opposees, en place, dans m_pair.
 *
 * SEMANTIQUE, reproduite a l'identique de l'ancien cache Cedges_visited :
 * pour une paire de sommets NON ORIENTEE {a,b}, les demi-aretes qui la portent
 * s'apparient DEUX A DEUX dans l'ordre des indices -- la 1re avec la 2e, la 3e
 * avec la 4e --, et une derniere impaire reste sans opposee (m_pair == -1).
 * Ce n'est pas une hypothese de variete : le cache n'admettait qu'une entree a
 * la fois par cle (il l'effacait des qu'il appariait), donc c'est exactement ce
 * qu'il faisait, y compris sur un maillage non variete ou mal oriente. Et une
 * demi-arete (a,b) peut s'apparier a une autre (a,b) de MEME sens : la cle est
 * non orientee.
 *
 * REPRESENTATION : un rangement CSR, DEUX allocations et 4*(nv+1+ne) octets.
 * Les demi-aretes sont rangees dans le seau de min(v_begin, v_end) ; a
 * l'interieur d'un seau, l'ordre des indices est conserve, ce qui rend
 * l'appariement deux-a-deux directement lisible. Le cache precedent faisait
 * nv+3 allocations, pesait 3,3 fois plus, et ne liberait pas son tableau de
 * tetes.
 *
 * PRECONDITION : m_pair vaut -1 partout a l'entree. C'est ce qui sert de
 * marque « deja appariee », et create_half_edge le garantit en reconstruisant
 * chaque Che_edge avant d'appeler.
 *
 * Un sommet hors bornes n'a pas de seau : sa demi-arete reste sans opposee,
 * plutot que d'ecrire hors du tableau -- l'ancien cache indexait sans garde.
 *
 * COUT : le seau d'un sommet a la taille de son degre. L'appariement y est
 * quadratique, donc sum(d^2) au total -- de l'ordre de 36*nv sur un maillage
 * triangulaire ordinaire. Un sommet de degre pathologique le paierait ; c'est
 * le meme profil que le balayage lineaire de l'ancien cache.
 */
static void pair_half_edges (std::vector<Che_edge> &edges, int ne, int nv)
{
	if (ne <= 0 || nv <= 0)
		return;

	// Comptage decale d'un cran : head[v+1] recoit le compte de v, de sorte que
	// la somme prefixe fasse de head[v] le debut du seau de v. Le remplissage
	// s'en sert ensuite comme CURSEUR -- head[v] finit donc sur la FIN du seau
	// de v, qui est le debut de celui de v+1. Aucun troisieme tableau : c'est ce
	// decalage qui evite le vecteur de curseurs, et 4 octets par sommet avec.
	std::vector<int> head (nv + 1, 0);
	for (int i = 0; i < ne; i++)
	{
		const int a = edges[i].m_v_begin, b = edges[i].m_v_end;
		if (a < 0 || b < 0 || a >= nv || b >= nv)
			continue; // sommet hors bornes : demi-arete laissee sans opposee
		head[(a < b ? a : b) + 1]++;
	}
	for (int v = 0; v < nv; v++)
		head[v + 1] += head[v];
	const int total = head[nv];

	std::vector<int> items (total);
	for (int i = 0; i < ne; i++)
	{
		const int a = edges[i].m_v_begin, b = edges[i].m_v_end;
		if (a < 0 || b < 0 || a >= nv || b >= nv)
			continue;
		items[head[a < b ? a : b]++] = i;
	}

	for (int v = 0, begin = 0; v < nv; v++)
	{
		const int end = head[v];
		for (int p = begin; p < end; p++)
		{
			const int i = items[p];
			if (edges[i].m_pair >= 0)
				continue; // deja apparie par un tour precedent
			const int a = edges[i].m_v_begin, b = edges[i].m_v_end;
			const int other = a > b ? a : b;
			for (int q = p + 1; q < end; q++)
			{
				const int k = items[q];
				if (edges[k].m_pair >= 0)
					continue;
				const int ka = edges[k].m_v_begin, kb = edges[k].m_v_end;
				if ((ka > kb ? ka : kb) != other)
					continue;
				edges[i].m_pair = k;
				edges[k].m_pair = i;
				break;
			}
		}
		begin = end;
	}
}



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
	m_maps_built = false;
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

void Che_mesh::EnsureMaps ()
{
	if (m_maps_built)
		return;
	// Pose AVANT le remplissage : les insertions ci-dessous n'appellent pas les
	// accesseurs, mais un futur editeur qui le ferait boucherait sinon a
	// l'infini.
	m_maps_built = true;

	m_map_edges->clear ();
	map_edges_vertex->clear ();
	map_edges_face->clear ();

	// m_ne, et non m_edges.size () : add_face pousse dans m_edges sans toucher
	// m_ne, et remplit lui-meme les cartes pour ce qu'il ajoute. La borne est
	// donc celle de create_half_edge, mot pour mot.
	for (int i = 0; i < m_ne; i++)
		m_map_edges->insert (std::make_pair (
			std::make_pair (m_edges[i].m_v_begin, m_edges[i].m_v_end), i));
	for (size_t i = 0; i < m_edges_vertex.size (); i++)
		if (m_edges_vertex[i] >= 0)
			map_edges_vertex->insert (std::make_pair ((int)i, m_edges_vertex[i]));
	for (size_t i = 0; i < m_edges_face.size (); i++)
		if (m_edges_face[i] >= 0)
			map_edges_face->insert (std::make_pair ((int)i, m_edges_face[i]));
}

Che_mesh::map_edges *Che_mesh::EdgeMap ()
{
	EnsureMaps ();
	return m_map_edges;
}

std::map<int,int> *Che_mesh::VertexEdgeMap ()
{
	EnsureMaps ();
	return map_edges_vertex;
}

std::map<int,int> *Che_mesh::FaceEdgeMap ()
{
	EnsureMaps ();
	return map_edges_face;
}

void Che_mesh::dump (void)
{
	printf ("map_edges :\n");
	map_edges *edge_map = EdgeMap ();
	for (map_edges::iterator it=edge_map->begin ();
	     it != edge_map->end ();
	     it++)
	{
		int idx = it->second;
		if (idx >= 0)
			m_edges[idx].dump (idx);
	}
}

void Che_mesh::dump_around_vertex (unsigned int vi)
{
	std::map<int,int> *vertex_map = VertexEdgeMap ();
	std::map<int,int>::iterator it = vertex_map->find (vi);
	if (it == vertex_map->end())
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
	// AVANT les push_back, et l'ordre compte : EnsureMaps () indexe m_edges sur
	// [0, m_ne[, et les trois aretes ajoutees ici sont au-dela. Les construire
	// apres les insertions les manquerait ; les construire apres les push_back
	// mais avant les insertions les indexerait deux fois.
	EnsureMaps ();

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
	std::map<int,int> *vertex_map = VertexEdgeMap ();
	std::map<int,int>::iterator it = vertex_map->find (vi);
	if (it == vertex_map->end())
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
	std::map<int,int> *vertex_map = VertexEdgeMap ();
	std::map<int,int>::iterator it = vertex_map->find (vi);
	if (it == vertex_map->end ())
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
	map_edges *edge_map = EdgeMap ();
	map_edges::iterator it = edge_map->find (std::make_pair(v1, v2));
	return (it == edge_map->end())? -1 : it->second;
}

int Che_mesh::get_edge_from_vertex (int vi)
{
	std::map<int,int> *vertex_map = VertexEdgeMap ();
	std::map<int,int>::iterator it = vertex_map->find (vi);
	if (it == vertex_map->end())
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
	pair_half_edges (m_edges, m_ne, (int)nVertices);

	// Les cartes NE SONT PAS remplies ici : elles le seront au premier acces,
	// depuis ces memes vecteurs (Che_mesh::EnsureMaps). Les vider maintenant
	// rend la memoire d'une construction precedente sur le meme objet.
	m_map_edges->clear();
	map_edges_vertex->clear();
	map_edges_face->clear();
	m_maps_built = false;
}


/**
* Flip an edge.
*/
void Che_mesh::edge_flip (int par_edge)
{
	if (m_edges[par_edge].m_pair < 0)
		return;

	// ⚠ Ce basculement ecrit dans m_edges et dans m_edges_vertex / m_edges_face,
	// mais PAS dans les cartes : elles restent telles qu'elles etaient avant.
	// C'etait deja le cas quand elles etaient construites d'office. Les
	// construire ici preserve exactement ce comportement -- sans cet appel, un
	// premier acces posterieur les batirait sur les vecteurs DEJA bascules, ce
	// qui n'est pas la meme chose. C'est le prix de l'equivalence, et il ne se
	// paie que sur les chemins qui basculent des aretes.
	EnsureMaps ();

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

	// Meme raison que dans edge_flip : cette contraction-ci ecrit dans les
	// vecteurs et laisse les cartes intactes. Les figer maintenant.
	EnsureMaps ();

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
		return 0;

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

	// Cette contraction ENTRETIENT les cartes en place (erase/insert) au lieu de
	// les refaire : il faut donc qu'elles existent. is_edge_contract2_valid les
	// a deja construites via get_edge_from_vertex, mais ne pas s'appuyer
	// la-dessus -- un futur raccourci de validation les laisserait absentes.
	EnsureMaps ();

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
