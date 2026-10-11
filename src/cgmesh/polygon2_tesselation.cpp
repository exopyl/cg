#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "polygon2.h"

#include "glutess/glutess.h"

///////////////////////
//
// user data structure
//
struct glutess_state {
	// tesselation
	float *v;
	unsigned int *f;
	unsigned int fn;
	unsigned int vn;

	unsigned int vcap;   // capacite (sommets) de v ; v = 3*vcap floats
	unsigned int fcap;   // capacite (triangles) de f ; f = 3*fcap uints

	unsigned int vc;
	unsigned int tri[3];
	unsigned int face;
};

// Tampons EXTENSIBLES : les auto-intersections (callback combine) creent des
// sommets supplementaires ; sans agrandissement on ecrivait hors-borne ->
// corruption du tas -> hang (ex. rosette gothique + nombreux foils).
static int ensure_v (struct glutess_state *s, unsigned int need)
{
	if (need <= s->vcap) return 1;
	unsigned int cap = s->vcap ? s->vcap * 2 : 64;
	while (cap < need) cap *= 2;
	float *nv = (float*) realloc (s->v, 3 * (size_t)cap * sizeof(float));
	if (!nv) return 0;
	s->v = nv; s->vcap = cap; return 1;
}
static int ensure_f (struct glutess_state *s, unsigned int need)
{
	if (need <= s->fcap) return 1;
	unsigned int cap = s->fcap ? s->fcap * 2 : 64;
	while (cap < need) cap *= 2;
	unsigned int *nf = (unsigned int*) realloc (s->f, 3 * (size_t)cap * sizeof(unsigned int));
	if (!nf) return 0;
	s->f = nf; s->fcap = cap; return 1;
}
//
///////////////////////

///////////////////////
//
// Callbacks for "mesh_tesselate"
//
static void glutess_begin(int type, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;

	state->vc = 0;
	//
	// we only get triangles here because we set
	// GLU_TESS_EDGE_FLAG callback to non-nullptr
	//
	(void) type;
}

static void glutess_vertex(void *vertex_data, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	auto vi = reinterpret_cast<uintptr_t>(vertex_data);
	state->tri[state->vc] = vi;

	if (state->vc == 2)
	{
		// generate face (agrandir le tampon si besoin)
		if (!ensure_f(state, state->fn + 1)) { state->vc = 0; return; }
		unsigned int fi = state->fn++;
		if (state->face != (unsigned int)-1)
		{
			// copy of existing face
			unsigned int fp = state->face;
			for (unsigned int j = 0; j < 3; j++)
			{
				unsigned int k = state->tri[j];
				state->f[3*fi+j] = state->f[3*fp+k];
			}
		}
		else
		{
			// new face
			state->f[3*fi]   = state->tri[0];
			state->f[3*fi+1] = state->tri[1];
			state->f[3*fi+2] = state->tri[2];
		}

		state->vc = 0;
	} else
		state->vc++;
}

static void glutess_edge_flag(int unused)
{
	(void) unused;
}

static void glutess_end(void *user_data)
{
	(void) user_data;
}

static void glutess_error(int errno, void *user_data)
{
	(void) user_data;

	printf ("Tessellation Error: %d\n", errno);
}

static void glutess_combine(double coords[3],
			    void *vertex_data[4],
			    float weight[4],
			    void **outData,
			    void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	unsigned int *f = state->f;
	unsigned int vi, v0, v1, v2, v3;

	// generate vertex (agrandir le tampon si besoin : evite le debordement sur
	// les polygones tres auto-intersectants -> plus de corruption/hang)
	if (!ensure_v(state, state->vn + 1))
	{
		*outData = (void *) (uintptr_t) 0;   // OOM : repli sur le sommet 0
		return;
	}
	vi = state->vn++;
	if (vi == (unsigned int) -1)
	{
		printf ("tesselation error: could not allocate vertex\n");
		return;
	}

	state->v[3*vi]   = (float) coords[0];
	state->v[3*vi+1] = (float) coords[1];
	state->v[3*vi+2] = (float) coords[2];

	if (state->face != (unsigned int)-1)
	{
		printf ("case not treated yet !!!\n");
/*
		unsigned int fp = state->face;
		unsigned int fvn = mesh_face_vn(mesh, fp);

		// face is self-intersecting
		// XXX: add new vertex at end
		mesh_face_resize(mesh, fp, fvn + 1);
		mesh_face_vi(mesh, fp, fvn) = vi;

		v0 = (unsigned int) vertex_data[0];
		v1 = (unsigned int) vertex_data[1];
		v2 = (unsigned int) vertex_data[2];
		v3 = (unsigned int) vertex_data[3];

		*outData = (void *) fvn;

		printf("combine added vertex %d to face %d at %d\n", vi, fp, fvn);
		*/
	} else
		*outData = (void *) vi;
}
//
///////////////////////

///////////////////////
//
// Triangles plats laisses par le balayage
//
// glutess peut emettre un triangle A-B-C dont les trois sommets sont alignes,
// B entre A et C : typiquement un sommet pose a la meme abscisse que le cote
// d'un autre contour. La region couverte reste juste, mais le triangle est
// d'aire nulle. Avec son voisin A-C-D le long de AC, il est remplace par A-B-D
// et B-C-D : meme region, memes sommets. La bascule n'a lieu que si ces deux
// triangles sont non plats et du sens de A-C-D ; sinon les faces restent
// telles quelles. Seul le tableau de faces change.
//
// Une seule table d'aretes orientees -> face, construite seulement si un
// triangle plat existe, au depart restreinte aux aretes inverses de celles des
// triangles plats (les seules interrogees), et tenue a jour a chaque bascule. Les
// triangles plats sont traites par indice croissant ; une bascule refusee est
// retentee apres chaque bascule reussie. Chaque bascule retire au moins un
// triangle plat sans en creer : il y en a au plus autant que de triangles
// plats au depart.
//
namespace
{
// Un triangle est plat quand sa double aire ne depasse pas kFlatRatio fois le
// carre de son plus long cote, soit une hauteur sous 1e-12 fois ce cote. Le
// critere ne depend pas de l'echelle ; il retient l'aire exactement nulle et
// l'erreur d'arrondi de son calcul en double (de l'ordre de 1e-16 fois le carre
// du cote), et laisse les triangles minces.
const double kFlatRatio = 1e-12;

double twiceArea (const float *v, unsigned int a, unsigned int b, unsigned int c)
{
	return ((double) v[3*b]   - v[3*a])   * ((double) v[3*c+1] - v[3*a+1])
	     - ((double) v[3*b+1] - v[3*a+1]) * ((double) v[3*c]   - v[3*a]);
}

double squaredLength (const float *v, unsigned int a, unsigned int b)
{
	const double dx = (double) v[3*b] - v[3*a], dy = (double) v[3*b+1] - v[3*a+1];
	return dx*dx + dy*dy;
}

// Seuil de double aire du triangle a-b-c.
double flatBound (const float *v, unsigned int a, unsigned int b, unsigned int c)
{
	const double l2 = std::max (squaredLength (v, a, b),
	                            std::max (squaredLength (v, b, c), squaredLength (v, c, a)));
	return kFlatRatio * l2;
}

bool isFlat (const float *v, const unsigned int *t)
{
	return t[0] != t[1] && t[1] != t[2] && t[0] != t[2]
	    && std::fabs (twiceArea (v, t[0], t[1], t[2])) <= flatBound (v, t[0], t[1], t[2]);
}

// a-b-c est non plat et de signe `sign`.
bool isProperTriangle (const float *v, unsigned int a, unsigned int b, unsigned int c, double sign)
{
	return sign * twiceArea (v, a, b, c) > flatBound (v, a, b, c);
}

typedef std::map<std::pair<unsigned int, unsigned int>, unsigned int> EdgeTable;

void addEdges (EdgeTable &edges, const unsigned int *f, unsigned int face)
{
	for (int e = 0; e < 3; ++e)
		edges[std::make_pair (f[3*face+e], f[3*face+(e+1)%3])] = face;
}

void removeEdges (EdgeTable &edges, const unsigned int *f, unsigned int face)
{
	for (int e = 0; e < 3; ++e)
	{
		auto it = edges.find (std::make_pair (f[3*face+e], f[3*face+(e+1)%3]));
		if (it != edges.end () && it->second == face)
			edges.erase (it);
	}
}

// Bascule le triangle plat `face` avec son voisin le long de son plus long
// cote ; rend le voisin, ou -1 sans voisin, ou si A-B-D et B-C-D ne seraient
// pas tous deux non plats et du sens de A-C-D (D aligne avec A et C, B
// confondu avec A ou C, B hors de AC du cote de D).
long flipFlat (const float *v, unsigned int *f, EdgeTable &edges, unsigned int face)
{
	const unsigned int *t = f + 3*face;
	int b = 0;
	double longest = -1.0;
	for (int e = 0; e < 3; ++e)
	{
		const double len = squaredLength (v, t[(e+2)%3], t[(e+1)%3]);
		if (len > longest) { longest = len; b = e; }
	}
	const unsigned int B = t[b], C = t[(b+1)%3], A = t[(b+2)%3];

	const auto it = edges.find (std::make_pair (A, C));
	if (it == edges.end () || it->second == face)
		return -1;
	const unsigned int other = it->second;
	const unsigned int *u = f + 3*other;
	int d = -1;
	for (int e = 0; e < 3; ++e)
		if (u[e] != A && u[e] != C) d = e;
	if (d < 0)
		return -1;
	const unsigned int D = u[d];
	const double sign = (twiceArea (v, A, C, D) > 0.0) ? 1.0 : -1.0;
	if (!isProperTriangle (v, A, B, D, sign) || !isProperTriangle (v, B, C, D, sign))
		return -1;

	removeEdges (edges, f, face);
	removeEdges (edges, f, other);
	f[3*face]    = A; f[3*face+1]  = B; f[3*face+2]  = D;
	f[3*other]   = B; f[3*other+1] = C; f[3*other+2] = D;
	addEdges (edges, f, face);
	addEdges (edges, f, other);
	return (long) other;
}
} // namespace

unsigned int flipFlatTriangles (const float *v, unsigned int *f, unsigned int nf)
{
	unsigned int flips = 0;
	std::set<unsigned int> flat;
	for (unsigned int i = 0; i < nf; ++i)
		if (isFlat (v, f + 3*i))
			flat.insert (i);
	if (flat.empty ())
		return flips;

	// Une bascule n'interroge que l'arete inverse d'une arete d'un triangle
	// plat ; la table ne garde au depart que ces aretes-la.
	std::vector<std::pair<unsigned int, unsigned int>> queried;
	queried.reserve (3 * flat.size ());
	for (const unsigned int face : flat)
		for (int e = 0; e < 3; ++e)
			queried.emplace_back (f[3*face+(e+1)%3], f[3*face+e]);
	std::sort (queried.begin (), queried.end ());
	queried.erase (std::unique (queried.begin (), queried.end ()), queried.end ());
	EdgeTable edges;
	for (unsigned int i = 0; i < nf; ++i)
		for (int e = 0; e < 3; ++e)
		{
			const std::pair<unsigned int, unsigned int> edge (f[3*i+e], f[3*i+(e+1)%3]);
			if (std::binary_search (queried.begin (), queried.end (), edge))
				edges[edge] = i;
		}

	bool progress = true;
	while (progress && !flat.empty ())
	{
		progress = false;
		for (auto it = flat.begin (); it != flat.end (); ++it)
		{
			const unsigned int face = *it;
			const long other = flipFlat (v, f, edges, face);
			if (other < 0)
				continue;
			++flips;
			progress = true;
			flat.erase (it);
			flat.erase ((unsigned int) other);
			break;
		}
	}
	return flips;
}
//
///////////////////////


///////////////////////
//
// Bascules de Delaunay
//
// Une demi-arete est la position k (0, 1, 2) du triangle t : h = 3t + k, de
// f[h] vers f[3t + (k+1)%3]. twin[h] est la demi-arete de sens oppose sur la
// meme arete, ou kNoTwin quand l'arete n'a pas exactement deux demi-aretes de
// sens opposes (bord, arete portee par trois triangles ou plus, ou deux fois
// dans le meme sens). La table est construite une fois, par seaux du plus
// petit sommet tries chacun, puis tenue a jour localement a chaque bascule :
// six liens, sans recherche.
//
namespace
{
const std::size_t kNoTwin = static_cast<std::size_t> (-1);

// d est dans le cercle circonscrit de a-b-c (de signe `sign`) quand le
// determinant du test, calcule relativement a d, depasse kInCircleRatio fois
// le carre de la somme des carres des distances de a, b, c a d : quatre
// sommets cocirculaires, ceux d'un carre de grille par exemple, ne basculent
// pas d'une diagonale a l'autre.
const double kInCircleRatio = 1e-9;

// Borne de cout globale : au plus kMaxVisitsPerHalfEdge x 3 nFaces visites de
// la pile. La boucle termine sans elle (une bascule n'a lieu qu'au-dela du
// seuil relatif, tres au-dessus de l'erreur d'arrondi : chacune fait decroitre
// strictement la triangulation), mais Lawson demande un nombre quadratique de
// bascules sur un anneau mince dense : la borne y est atteinte (vers 10 000
// sommets sur un anneau de rayons 10 et 9,5). La triangulation rendue est
// alors valide, pas forcement de Delaunay, et l'appelant en est averti.
const std::size_t kMaxVisitsPerHalfEdge = 64;

bool strictlyInCircle (const float *v, unsigned int a, unsigned int b, unsigned int c,
                       unsigned int d, double sign)
{
	const double xd = v[3*d], yd = v[3*d+1];
	const double adx = v[3*a] - xd, ady = v[3*a+1] - yd;
	const double bdx = v[3*b] - xd, bdy = v[3*b+1] - yd;
	const double cdx = v[3*c] - xd, cdy = v[3*c+1] - yd;
	const double ad = adx*adx + ady*ady, bd = bdx*bdx + bdy*bdy, cd = cdx*cdx + cdy*cdy;
	const double det = ad * (bdx*cdy - cdx*bdy) - bd * (adx*cdy - cdx*ady) + cd * (adx*bdy - bdx*ady);
	const double scale = ad + bd + cd;
	return sign * det > kInCircleRatio * scale * scale;
}

std::vector<std::size_t> buildTwins (const unsigned int *f, unsigned int nf)
{
	const std::size_t nh = 3 * (std::size_t) nf;
	std::vector<std::size_t> twin (nh, kNoTwin);
	// Demi-aretes rangees par seau de leur plus petit sommet (dans l'ordre des
	// demi-aretes), puis, dans chaque seau, triees par (plus grand sommet,
	// demi-arete) : l'ordre du tri global par (cle de l'arete, demi-arete).
	unsigned int maxVertex = 0;
	for (std::size_t h = 0; h < nh; ++h)
		maxVertex = std::max (maxVertex, f[h]);
	std::vector<std::uint32_t> start ((std::size_t) maxVertex + 2, 0);
	for (std::size_t h = 0; h < nh; ++h)
	{
		const unsigned int a = f[h], b = f[3*(h/3) + (h%3 + 1)%3];
		if (a != b) ++start[(std::size_t) std::min (a, b) + 1];
	}
	for (std::size_t c = 0; c + 1 < start.size (); ++c)
		start[c + 1] += start[c];
	std::vector<std::uint32_t> items (start.back ());
	{
		std::vector<std::uint32_t> fill (start.begin (), start.end () - 1);
		for (std::size_t h = 0; h < nh; ++h)
		{
			const unsigned int a = f[h], b = f[3*(h/3) + (h%3 + 1)%3];
			if (a != b) items[fill[std::min (a, b)]++] = (std::uint32_t) h;
		}
	}
	std::vector<std::pair<unsigned int, std::uint32_t>> bucket;
	for (std::size_t lo = 0; lo + 1 < start.size (); ++lo)
	{
		bucket.clear ();
		for (std::uint32_t item = start[lo]; item < start[lo + 1]; ++item)
		{
			const std::size_t h = items[item];
			bucket.emplace_back (std::max (f[h], f[3*(h/3) + (h%3 + 1)%3]), (std::uint32_t) h);
		}
		std::sort (bucket.begin (), bucket.end ());
		for (std::size_t i = 0; i < bucket.size (); )
		{
			std::size_t j = i + 1;
			while (j < bucket.size () && bucket[j].first == bucket[i].first) ++j;
			if (j - i == 2)
			{
				const std::size_t h = bucket[i].second, g = bucket[i+1].second;
				if (f[h] != f[g])
				{
					twin[h] = g;
					twin[g] = h;
				}
			}
			i = j;
		}
	}
	return twin;
}

void link (std::vector<std::size_t> &twin, std::size_t h, std::size_t g)
{
	twin[h] = g;
	if (g != kNoTwin) twin[g] = h;
}
} // namespace

unsigned int delaunayFlips (const float *v, unsigned int *f, unsigned int nf, bool *complete)
{
	if (complete)
		*complete = true;
	if (nf < 2)
		return 0;
	std::vector<std::size_t> twin = buildTwins (f, nf);
	std::vector<std::size_t> stack;
	for (std::size_t h = 0; h < twin.size (); ++h)
		if (twin[h] != kNoTwin && h < twin[h])
			stack.push_back (h);

	unsigned int flips = 0;
	const std::size_t maxVisits = kMaxVisitsPerHalfEdge * twin.size ();
	for (std::size_t visits = 0; !stack.empty () && visits < maxVisits; ++visits)
	{
		const std::size_t h = stack.back ();
		stack.pop_back ();
		const std::size_t g = twin[h];
		if (g == kNoTwin)
			continue;
		// a-b-c porte h (a -> b), b-a-d porte g (b -> a).
		const std::size_t t1 = h / 3, t2 = g / 3;
		const std::size_t hBc = 3*t1 + (h%3 + 1)%3, hCa = 3*t1 + (h%3 + 2)%3;
		const std::size_t gAd = 3*t2 + (g%3 + 1)%3, gDb = 3*t2 + (g%3 + 2)%3;
		const unsigned int a = f[h], b = f[hBc], c = f[hCa], d = f[gDb];
		const double sign = (twiceArea (v, a, b, c) > 0.0) ? 1.0 : -1.0;
		if (!isProperTriangle (v, a, b, c, sign) || !isProperTriangle (v, b, a, d, sign))
			continue;
		if (!strictlyInCircle (v, a, b, c, d, sign))
			continue;
		if (!isProperTriangle (v, c, a, d, sign) || !isProperTriangle (v, d, b, c, sign))
			continue;

		const std::size_t xBc = twin[hBc], xCa = twin[hCa], yAd = twin[gAd], yDb = twin[gDb];
		f[3*t1] = c; f[3*t1+1] = a; f[3*t1+2] = d;
		f[3*t2] = d; f[3*t2+1] = b; f[3*t2+2] = c;
		link (twin, 3*t1,     xCa);
		link (twin, 3*t1 + 1, yAd);
		link (twin, 3*t1 + 2, 3*t2 + 2);
		link (twin, 3*t2,     yDb);
		link (twin, 3*t2 + 1, xBc);
		++flips;
		for (const std::size_t outer : {3*t1, 3*t1 + 1, 3*t2, 3*t2 + 1})
			stack.push_back (outer);
	}
	if (complete)
		*complete = stack.empty ();
	return flips;
}
//
///////////////////////

///////////////////////
//
// Bord d'une region (GLU_TESS_BOUNDARY_ONLY)
//
namespace
{
int gluWinding (WindingRule rule)
{
	switch (rule)
	{
	case WindingRule::Odd:       return GLU_TESS_WINDING_ODD;
	case WindingRule::NonZero:   return GLU_TESS_WINDING_NONZERO;
	case WindingRule::Positive:  return GLU_TESS_WINDING_POSITIVE;
	case WindingRule::Negative:  return GLU_TESS_WINDING_NEGATIVE;
	case WindingRule::AbsGeqTwo: return GLU_TESS_WINDING_ABS_GEQ_TWO;
	}
	return GLU_TESS_WINDING_NONZERO;
}

struct BoundaryState
{
	std::vector<float>                      positions;   // x, y par sommet : entree puis croisements
	std::vector<std::vector<unsigned int>>  loops;
	bool                                    failed = false;
};

void boundaryBegin (int type, void *user)
{
	(void) type;
	static_cast<BoundaryState *> (user)->loops.emplace_back ();
}

void boundaryVertex (void *data, void *user)
{
	static_cast<BoundaryState *> (user)->loops.back ().push_back (
		static_cast<unsigned int> (reinterpret_cast<std::uintptr_t> (data)));
}

void boundaryEnd (void *user)
{
	(void) user;
}

void boundaryError (int code, void *user)
{
	(void) code;
	static_cast<BoundaryState *> (user)->failed = true;
}

void boundaryCombine (double coords[3], void *data[4], float weight[4], void **out, void *user)
{
	(void) data; (void) weight;
	BoundaryState *s = static_cast<BoundaryState *> (user);
	const std::size_t id = s->positions.size () / 2;
	s->positions.push_back ((float) coords[0]);
	s->positions.push_back ((float) coords[1]);
	*out = reinterpret_cast<void *> (static_cast<std::uintptr_t> (id));
}
} // namespace

int Polygon2::boundary (WindingRule rule, Polygon2 &out) const
{
	out = Polygon2 ();
	const unsigned int n = (unsigned int) get_n_points ();
	BoundaryState state;
	state.positions.reserve (2 * (std::size_t) n);
	// glutess garde les adresses des coordonnees jusqu'a la fin du polygone :
	// le tableau est dimensionne une fois pour toutes.
	std::vector<double> coords (3 * (std::size_t) n, 0.0);

	GLUtesselator *tess = gluNewTess ();
	if (tess == nullptr)
		return -1;
	gluTessCallback (tess, GLU_TESS_BEGIN_DATA,   (void (*)()) &boundaryBegin);
	gluTessCallback (tess, GLU_TESS_VERTEX_DATA,  (void (*)()) &boundaryVertex);
	gluTessCallback (tess, GLU_TESS_END_DATA,     (void (*)()) &boundaryEnd);
	gluTessCallback (tess, GLU_TESS_ERROR_DATA,   (void (*)()) &boundaryError);
	gluTessCallback (tess, GLU_TESS_COMBINE_DATA, (void (*)()) &boundaryCombine);
	gluTessNormal (tess, 0, 0, 1);
	gluTessProperty (tess, GLU_TESS_WINDING_RULE, gluWinding (rule));
	gluTessProperty (tess, GLU_TESS_BOUNDARY_ONLY, 1);

	gluTessBeginPolygon (tess, &state);
	std::size_t k = 0;
	for (int j = 0; j < get_n_contours (); j++)
	{
		gluTessBeginContour (tess);
		const float *pts = get_points ((unsigned int) j);
		for (int i = 0; i < get_n_points ((unsigned int) j); i++, k++)
		{
			coords[3*k]   = pts[2*i];
			coords[3*k+1] = pts[2*i+1];
			state.positions.push_back (pts[2*i]);
			state.positions.push_back (pts[2*i+1]);
			gluTessVertex (tess, &coords[3*k], reinterpret_cast<void *> (static_cast<std::uintptr_t> (k)));
		}
		gluTessEndContour (tess);
	}
	gluTessEndPolygon (tess);
	gluDeleteTess (tess);
	if (state.failed)
		return -1;

	out.alloc_contours ((int) state.loops.size ());
	std::vector<float> packed;
	for (std::size_t l = 0; l < state.loops.size (); ++l)
	{
		packed.clear ();
		for (const unsigned int id : state.loops[l])
		{
			packed.push_back (state.positions[2*id]);
			packed.push_back (state.positions[2*id+1]);
		}
		out.add_contour ((unsigned int) l, (unsigned int) state.loops[l].size (), packed.data ());
	}
	return (int) state.loops.size ();
}
//
///////////////////////

// tesselation
int Polygon2::tesselate (float **_pVertices, unsigned int *_nVertices,
			  unsigned int **_pFaces, unsigned int *_nFaces, WindingRule rule) const
{
	GLUtesselator *tess;
	double *coords;
	unsigned int nVertices;
	struct glutess_state state;

	// compute the number of vertices
	nVertices = get_n_points();
	coords = (double*)malloc(3*sizeof(double)*nVertices);
	if (coords == nullptr)
		return 0;

	// allocate memory for the result. 
	// The number of vertices can increase if there are self-intersections.
	// We allocate a bit more than needed.
	state.vcap = nVertices + 100;
	state.fcap = (nVertices + 100) * 2;
	state.v = (float*)malloc(3 * (size_t)state.vcap * sizeof(float));
	state.vn = nVertices;
	state.f = (unsigned int*)malloc(3 * (size_t)state.fcap * sizeof(unsigned int));
	state.fn = 0;
	state.face = (unsigned int)-1;

	tess = gluNewTess();
	
	gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void(*)()) &glutess_begin);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(*)()) &glutess_vertex);
	gluTessCallback(tess, GLU_TESS_END_DATA, (void(*)()) &glutess_end);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(*)()) &glutess_error);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(*)()) &glutess_combine);

	// this will force GL_TRIANGLES rendering
	gluTessCallback(tess, GLU_TESS_EDGE_FLAG, (void(*)()) &glutess_edge_flag);


	gluTessNormal(tess, 0, 0, 1);

	gluTessProperty(tess, GLU_TESS_WINDING_RULE, gluWinding(rule));

	gluTessBeginPolygon(tess, &state);
	unsigned int iVertex = 0;
	for (unsigned int j=0; j<get_n_contours(); j++)
	{
		gluTessBeginContour(tess);
		const float *pts = get_points(j);
		for (unsigned int i=0; i<get_n_points(j); i++)
		{
			coords[3 * iVertex + 0] = (double) pts[2*i];
			coords[3 * iVertex + 1] = (double) pts[2*i+1];
			coords[3 * iVertex + 2] = (double) 0.;
			
			state.v[3*iVertex+0] = (float) pts[2*i];
			state.v[3*iVertex+1] = (float) pts[2*i+1];
			state.v[3*iVertex+2] = (float) 0.;
			
			gluTessVertex(tess, &coords[3 * iVertex], (void *) iVertex);
			iVertex++;
		}
		gluTessEndContour(tess);
	}
	gluTessEndPolygon(tess);

	gluDeleteTess(tess);

	flipFlatTriangles (state.v, state.f, state.fn);

	*_pVertices = state.v;
	*_nVertices = state.vn;
	*_pFaces = state.f;
	*_nFaces = state.fn;

	free(coords);

	return 0;
}
