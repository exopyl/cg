#include "mesh_solidify.h"
#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace
{

struct V3 { double x = 0., y = 0., z = 0.; };

inline V3 sub (const V3 &a, const V3 &b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline V3 add (const V3 &a, const V3 &b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline V3 cross (const V3 &a, const V3 &b)
{
	return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline double dot (const V3 &a, const V3 &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline double len (const V3 &a) { return std::sqrt (dot (a, a)); }
inline V3 unit (const V3 &a)
{
	const double l = len (a);
	return l > 0. ? V3{a.x/l, a.y/l, a.z/l} : V3{};
}

// Cle de soudure : position quantifiee. Deux points de part et d'autre d'une
// frontiere de cellule ne se soudent pas -- sans consequence ici, les coutures
// des grilles parametriques sont des copies EXACTES (meme formule, meme u,v).
struct CellKey
{
	int64_t x, y, z;
	bool operator== (const CellKey &o) const { return x == o.x && y == o.y && z == o.z; }
};
struct CellHash
{
	size_t operator() (const CellKey &k) const
	{
		return (size_t)(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL);
	}
};

inline uint64_t edgeKey (unsigned int a, unsigned int b)
{
	return ((uint64_t)a << 32) | (uint64_t)b;
}

// Seuil d'opposition entre deux normales propres d'un meme point soude : en
// dessous, elles sont considerees de cotes opposes (couture de Mobius).
const double kSameSide = -0.5;
// Plafond du facteur d'epaisseur uniforme (cos >= 1/3) : evite qu'une arete
// tres vive projette un sommet a l'infini.
const double kMaxScale = 3.;

} // namespace

bool SolidifyMesh (const Mesh &in, float thickness, Mesh &out)
{
	const std::vector<unsigned int> tris = in.GetTriangles ();
	const unsigned int nv = in.GetNVertices ();
	if (thickness <= 0.f || tris.empty () || nv == 0)
	{
		out = in;
		return false;
	}

	std::vector<V3> p (nv);
	V3 lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
	for (unsigned int i = 0; i < nv; i++)
	{
		float v[3];
		in.GetVertex (i, v);
		p[i] = {v[0], v[1], v[2]};
		lo = {std::min (lo.x, p[i].x), std::min (lo.y, p[i].y), std::min (lo.z, p[i].z)};
		hi = {std::max (hi.x, p[i].x), std::max (hi.y, p[i].y), std::max (hi.z, p[i].z)};
	}

	// --- soudure des sommets coincidents ------------------------------------
	const double diag = len (sub (hi, lo));
	const double cell = std::max (diag * 1e-6, 1e-12);
	std::vector<unsigned int> weld (nv);
	std::vector<std::vector<unsigned int>> groups;
	{
		std::unordered_map<CellKey, unsigned int, CellHash> cells;
		cells.reserve (nv);
		for (unsigned int i = 0; i < nv; i++)
		{
			const CellKey k {(int64_t)std::llround (p[i].x / cell),
			                 (int64_t)std::llround (p[i].y / cell),
			                 (int64_t)std::llround (p[i].z / cell)};
			auto it = cells.find (k);
			if (it == cells.end ())
			{
				it = cells.emplace (k, (unsigned int)groups.size ()).first;
				groups.emplace_back ();
			}
			weld[i] = it->second;
			groups[it->second].push_back (i);
		}
	}

	// --- triangles retenus : ceux dont les 3 sommets soudes sont distincts ---
	// Les triangles degeneres des poles (deux coins confondus) n'ont ni aire ni
	// aretes utiles, et compteraient de fausses aretes de bord.
	const size_t nt = tris.size () / 3;
	std::vector<unsigned int> kept;
	kept.reserve (tris.size ());
	for (size_t t = 0; t < nt; t++)
	{
		const unsigned int a = tris[3*t], b = tris[3*t+1], c = tris[3*t+2];
		if (a >= nv || b >= nv || c >= nv) continue;
		if (weld[a] == weld[b] || weld[b] == weld[c] || weld[c] == weld[a]) continue;
		kept.push_back (a); kept.push_back (b); kept.push_back (c);
	}
	const size_t nk = kept.size () / 3;
	if (nk == 0)
	{
		out = in;
		return false;
	}

	// --- normales de face et normales propres de sommet ----------------------
	std::vector<V3> faceN (nk);                 // unitaire
	// Ponderation par l'ANGLE au coin, pas par l'aire : elle ne depend pas de la
	// triangulation. Par l'aire, un coin de cube touchant deux triangles d'une
	// face et un seul de la voisine penchait vers la premiere.
	std::vector<V3> ownN (nv);
	std::vector<std::vector<unsigned int>> incident (nv);
	for (size_t t = 0; t < nk; t++)
	{
		const unsigned int c3[3] = {kept[3*t], kept[3*t+1], kept[3*t+2]};
		faceN[t] = unit (cross (sub (p[c3[1]], p[c3[0]]), sub (p[c3[2]], p[c3[0]])));
		for (int k = 0; k < 3; k++)
		{
			const unsigned int v = c3[k];
			const V3 e1 = unit (sub (p[c3[(k+1)%3]], p[v]));
			const V3 e2 = unit (sub (p[c3[(k+2)%3]], p[v]));
			const double angle = std::acos (std::max (-1., std::min (1., dot (e1, e2))));
			ownN[v] = add (ownN[v], {faceN[t].x * angle, faceN[t].y * angle, faceN[t].z * angle});
			incident[v].push_back ((unsigned int)t);
		}
	}

	// --- normale finale + facteur d'epaisseur uniforme ------------------------
	// Un sommet cumule les normales propres des copies de son point qui sont du
	// MEME cote que lui ; le facteur vient des faces de ces memes copies, pour que
	// deux copies d'une couture recoivent exactement le meme decalage.
	std::vector<V3> offset (nv);
	const double h = 0.5 * (double)thickness;
	for (unsigned int v = 0; v < nv; v++)
	{
		const V3 nvUnit = unit (ownN[v]);
		if (len (nvUnit) == 0.) continue;       // sommet isole ou d'aire nulle
		V3 sum;
		std::vector<unsigned int> members;
		for (unsigned int w : groups[weld[v]])
			if (w == v || dot (nvUnit, unit (ownN[w])) > kSameSide)
			{
				sum = add (sum, ownN[w]);
				members.push_back (w);
			}
		const V3 n = unit (sum);
		double minCos = 1.;
		for (unsigned int w : members)
			for (unsigned int t : incident[w])
			{
				const double c = dot (n, faceN[t]);
				if (c > 0.) minCos = std::min (minCos, c);
			}
		const double s = std::min (1. / minCos, kMaxScale);
		offset[v] = {n.x * h * s, n.y * h * s, n.z * h * s};
	}

	// --- aretes de bord ------------------------------------------------------
	// Sur les indices SOUDES, une arete est interieure si elle apparait une fois
	// dans chaque sens. Sinon (bord libre, arete non manifold) chaque occurrence
	// recoit sa paroi -- sauf la couture a orientation inversee, voir plus bas.
	std::unordered_map<uint64_t, int> directed;
	directed.reserve (nk * 3);
	for (size_t t = 0; t < nk; t++)
		for (int e = 0; e < 3; e++)
		{
			const unsigned int a = kept[3*t + e], b = kept[3*t + (e+1)%3];
			directed[edgeKey (weld[a], weld[b])]++;
		}

	// --- assemblage : [0,nv) cote +normale, [nv,2nv) cote -normale ------------
	std::vector<float> verts ((size_t)6 * nv);
	for (unsigned int v = 0; v < nv; v++)
	{
		verts[3*v]           = (float)(p[v].x + offset[v].x);
		verts[3*v+1]         = (float)(p[v].y + offset[v].y);
		verts[3*v+2]         = (float)(p[v].z + offset[v].z);
		verts[3*(nv+v)]      = (float)(p[v].x - offset[v].x);
		verts[3*(nv+v)+1]    = (float)(p[v].y - offset[v].y);
		verts[3*(nv+v)+2]    = (float)(p[v].z - offset[v].z);
	}

	std::vector<unsigned int> faces;
	faces.reserve (nk * 6);
	for (size_t t = 0; t < nk; t++)
	{
		const unsigned int a = kept[3*t], b = kept[3*t+1], c = kept[3*t+2];
		faces.insert (faces.end (), {a, b, c});                  // face +normale
		faces.insert (faces.end (), {nv+a, nv+c, nv+b});         // face -normale, retournee
	}
	for (size_t t = 0; t < nk; t++)
		for (int e = 0; e < 3; e++)
		{
			const unsigned int a = kept[3*t + e], b = kept[3*t + (e+1)%3];
			const unsigned int ga = weld[a], gb = weld[b];
			const auto fwd = directed.find (edgeKey (ga, gb));
			const auto bwd = directed.find (edgeKey (gb, ga));
			const int nBwd = bwd != directed.end () ? bwd->second : 0;
			const bool interior = fwd->second == 1 && nBwd == 1;
			// Couture a orientation inversee (Mobius) : deux faces parcourent
			// l'arete dans le MEME sens. Leurs normales s'opposant, la face +
			// de l'une coincide avec la face - de l'autre -- le solide s'y referme
			// de lui-meme, et une paroi y serait une cloison interne.
			const bool flipSeam = fwd->second == 2 && nBwd == 0;
			if (interior || flipSeam) continue;
			// Quad [a+, a-, b-, b+] : normale = arete x normale, vers l'exterieur
			// du bord pour une face orientee dans le sens trigonometrique.
			faces.insert (faces.end (), {a, nv+a, nv+b});
			faces.insert (faces.end (), {a, nv+b, b});
		}

	out = Mesh ();
	out.SetVertices (2 * nv, verts.data ());
	out.SetFaces ((unsigned int)(faces.size () / 3), 3, faces.data ());
	out.ComputeNormals ();
	return true;
}
