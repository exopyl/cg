#include "extrude_profiled.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "contour_ops.h"
#include "mesh.h"
#include "polygon2.h"
#include "profile2d.h"

namespace
{

// --- construction de maillage ------------------------------------------------
//
// Meme idiome que extrudeProfiledToMesh (profile2d.cpp) : deux tampons plats,
// puis un seul SetVertices / SetFaces. Un Mesh ne se construit pas face a face.
struct MeshBuild
{
	std::vector<float>        verts;   // 3 flottants par sommet
	std::vector<unsigned int> faces;   // 3 index par triangle

	unsigned int Add (float x, float y, float z)
	{
		const unsigned int id = (unsigned int)(verts.size() / 3);
		verts.push_back (x);
		verts.push_back (y);
		verts.push_back (z);
		return id;
	}

	void Tri (unsigned int a, unsigned int b, unsigned int c)
	{
		faces.push_back (a);
		faces.push_back (b);
		faces.push_back (c);
	}
};

Polygon2 toPolygon (const std::vector<ExtrudeContour>& contours)
{
	Polygon2 poly;
	poly.alloc_contours ((int)contours.size());
	// Les points sont recopies dans un tampon PAR contour qui doit survivre a
	// add_contour ; d'ou le stockage local, et non un pointeur sur la boucle.
	std::vector<std::vector<float>> packed (contours.size());
	for (std::size_t c = 0; c < contours.size(); ++c)
	{
		const std::vector<Vector2f>& pts = contours[c].pts;
		packed[c].resize (pts.size() * 2);
		for (std::size_t i = 0; i < pts.size(); ++i)
		{
			packed[c][2 * i]     = pts[i].x;
			packed[c][2 * i + 1] = pts[i].y;
		}
		poly.add_contour ((unsigned int)c, (unsigned int)pts.size(), packed[c].data());
	}
	return poly;
}

// Tessellation d'une region (NonZero, cf. polygon2_tesselation.cpp), rendue en
// sommets 2D et triangles. Rend false quand rien n'a pu etre tessele.
bool tessellate (const std::vector<ExtrudeContour>& contours,
                 std::vector<Vector2f>& outVerts,
                 std::vector<unsigned int>& outTris)
{
	outVerts.clear();
	outTris.clear();
	if (contours.empty()) return false;

	Polygon2 poly = toPolygon (contours);
	float *pV = nullptr; unsigned int nV = 0, *pF = nullptr, nF = 0;
	poly.tesselate (&pV, &nV, &pF, &nF);
	const bool ok = (nV > 0 && nF > 0 && pV != nullptr && pF != nullptr);
	if (ok)
	{
		outVerts.reserve (nV);
		for (unsigned int i = 0; i < nV; ++i)
			outVerts.push_back (Vector2f (pV[3 * i], pV[3 * i + 1]));
		outTris.assign (pF, pF + 3 * nF);
	}
	if (pV) free (pV);
	if (pF) free (pF);
	return ok;
}

// --- classification des sommets d'une couronne -------------------------------
//
// Une couronne est `differenceContours(grand, petit)`, et les deux anneaux ne se
// croisent pas : chaque sommet du resultat provient donc de l'un ou de l'autre,
// et il suffit de dire duquel pour lui donner sa cote.
//
// ⚠ PAS de correspondance par coordonnee EXACTE, et la lecon a ete payee : les
// deux valeurs ne sont egales qu'a la precision de Clipper2 (six decimales), si
// bien qu'une cle quantifiee les separe des qu'elles tombent de part et d'autre
// d'une frontiere d'arrondi. Le symptome etait une peau trouee sur quatre coins
// d'un contour, la ou la distance au sommet valait pourtant zero.
//
// D'ou une GRILLE : les sommets des deux anneaux sont ranges par cellule, et un
// point de couronne cherche le plus proche dans les neuf cellules voisines. La
// cellule vaut cent fois la tolerance, de sorte qu'un voisin a portee soit
// toujours dans ce voisinage.
namespace ring_grid
{

// Tolerance de reconnaissance : 1 µm a l'echelle du millimetre -- trois ordres
// de grandeur au-dessus du bruit de Clipper2, et deux en dessous du plus petit
// detail d'un glyphe.
const float kTol = 1e-3f;
const float kCell = 0.1f;

struct Cell
{
	int x, y;
	bool operator== (const Cell& o) const { return x == o.x && y == o.y; }
};

struct CellHash
{
	std::size_t operator() (const Cell& c) const
	{
		// Le produit se calcule en NON SIGNE, ou le debordement est defini
		// (modulo 2^32) et constitue le comportement voulu d'un hachage. En
		// `int` il serait indefini : avec kCell = 0.1, un indice de cellule
		// depasse INT_MAX / 73856093 des |coordonnee| >= 2,9 mm.
		const std::uint32_t hx = (std::uint32_t)c.x * 73856093u;
		const std::uint32_t hy = (std::uint32_t)c.y * 19349663u;
		return (std::size_t)(hx ^ hy);
	}
};

struct Tagged
{
	Vector2f p;
	char     tag;
};

typedef std::unordered_map<Cell, std::vector<Tagged>, CellHash> Grid;

Cell cellOf (const Vector2f& p)
{
	return Cell { (int)std::floor (p.x / kCell), (int)std::floor (p.y / kCell) };
}

void insert (Grid& grid, const std::vector<ExtrudeContour>& ring, char tag)
{
	for (const ExtrudeContour& c : ring)
		for (const Vector2f& p : c.pts)
			grid[cellOf (p)].push_back (Tagged { p, tag });
}

// Rend le tag du sommet le plus proche dans la tolerance, ou 0 si aucun.
char lookup (const Grid& grid, const Vector2f& p)
{
	const Cell c = cellOf (p);
	float best = kTol * kTol;
	char found = 0;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx)
		{
			const auto it = grid.find (Cell { c.x + dx, c.y + dy });
			if (it == grid.end()) continue;
			for (const Tagged& t : it->second)
			{
				const float ex = t.p.x - p.x, ey = t.p.y - p.y;
				const float d = ex * ex + ey * ey;
				if (d <= best) { best = d; found = t.tag; }
			}
		}
	return found;
}

} // namespace ring_grid

// Distance au carre du point au sommet le plus proche de l'anneau, par balayage
// complet. Recours de DERNIER ressort : la grille l'a deja manque, donc autant
// ne rien supposer. Le compteur `steinerPoints` dit combien de fois cela arrive.
float nearestSq (const std::vector<ExtrudeContour>& ring, const Vector2f& p)
{
	float best = 1e30f;
	for (const ExtrudeContour& c : ring)
		for (const Vector2f& q : c.pts)
		{
			const float dx = q.x - p.x, dy = q.y - p.y;
			best = std::min (best, dx * dx + dy * dy);
		}
	return best;
}

} // namespace

bool extrudeProfiledContours (const std::vector<ExtrudeContour>& contours,
                              const Profile2D& profile,
                              const ProfiledExtrudeOptions& opt,
                              Mesh& out,
                              ProfiledExtrudeStats* stats)
{
	if (stats) *stats = ProfiledExtrudeStats();
	if (contours.empty()) return false;
	if (profile.points.size() < 2) return false;
	if (opt.zTop <= opt.zBottom) return false;

	// Contrat du profil, refuse plutot que corrige : un profil mal oriente
	// produirait une piece repliee que rien ne signalerait. Meme refus que
	// extrudeProfiledToMesh, pour que les deux consommateurs disent la meme
	// chose du meme type.
	if (std::fabs (profile.points.front().x) > 1e-9
	 || std::fabs (profile.points.front().y) > 1e-9)
		return false;
	for (const Vector2d& p : profile.points)
		if (p.x < 0.0 || p.y < 0.0) return false;

	const double uMax = profile.points.back().x;
	const double vMax = profile.points.back().y;
	// LARGEUR NULLE = PAROI DROITE, et c'est un cas utile plutot qu'une erreur :
	// c'est la limite du profil, et elle permet a une interface d'offrir « arete
	// vive » sans debrancher un noeud -- il lui suffit de mettre la largeur a
	// zero. Tous les anneaux valent alors le contour nominal, les couronnes sont
	// vides, et il reste le fond, la paroi et le capot : une extrusion droite.
	if (uMax >= (double)(opt.zTop - opt.zBottom)) return false;   // plus profond que la piece

	// --- les anneaux ---------------------------------------------------------
	//
	// Un par point du profil. `delta` CROIT avec k dans les deux modes, si bien
	// que l'anneau d'indice le plus eleve est toujours le plus grand : la boucle
	// des couronnes ne connait donc pas le sens choisi.
	const std::size_t levels = profile.points.size();
	std::vector<std::vector<ExtrudeContour>> rings (levels);
	std::vector<float> z (levels);
	int nominalPieces = 0, topPieces = 0;

	for (std::size_t k = 0; k < levels; ++k)
	{
		const double v = profile.points[k].y;
		const double delta = (opt.direction == ProfiledExtrudeOptions::Direction::Inward)
		                   ? (v - vMax)      // -vMax a la face du dessus, 0 au fond
		                   : v;              // 0 a la face du dessus, +vMax au fond
		z[k] = opt.zTop - (float)profile.points[k].x;

		int pieces = 0;
		rings[k] = offsetContours (contours, (float)delta, StrokeJoin::Round, 2.f, &pieces);
		if (rings[k].empty())
			return false;   // le decalage a tout consomme : rien a construire

		if (k == 0) topPieces = pieces;
		if (k + 1 == levels) nominalPieces = pieces;
	}
	if (stats)
	{
		stats->rings = levels;
		stats->vanishedPieces = (nominalPieces > topPieces)
		                      ? (std::size_t)(nominalPieces - topPieces) : 0;
	}

	const std::vector<ExtrudeContour>& base = rings[levels - 1];   // le plus grand
	const std::vector<ExtrudeContour>& top  = rings[0];            // le plus rentre

	MeshBuild mb;
	std::vector<Vector2f> tv;
	std::vector<unsigned int> tf;

	// --- capot du fond, normales vers -z ------------------------------------
	if (!tessellate (base, tv, tf)) return false;
	{
		const unsigned int b = (unsigned int)(mb.verts.size() / 3);
		for (const Vector2f& p : tv) mb.Add (p.x, p.y, opt.zBottom);
		for (std::size_t i = 0; i + 2 < tf.size() + 0; i += 3)
			mb.Tri (b + tf[i], b + tf[i + 2], b + tf[i + 1]);   // inverse -> -z
	}

	// --- paroi d'aplomb, du fond au plan ou le profil commence ---------------
	//
	// Sommets DISJOINTS de ceux des capots : Mesh::ComputeNormals moyenne les
	// normales par sommet, et partager un coin de capot avec une paroi
	// inclinerait la normale du capot (meme lecon que ExtrudedMeshBuilder).
	const float zBase = z[levels - 1];
	if (zBase > opt.zBottom + 1e-6f)
	{
		for (const ExtrudeContour& c : base)
		{
			const std::size_t n = c.pts.size();
			if (n < 3) continue;
			const unsigned int lo = (unsigned int)(mb.verts.size() / 3);
			for (const Vector2f& p : c.pts) mb.Add (p.x, p.y, opt.zBottom);
			const unsigned int hi = (unsigned int)(mb.verts.size() / 3);
			for (const Vector2f& p : c.pts) mb.Add (p.x, p.y, zBase);
			for (std::size_t i = 0; i < n; ++i)
			{
				const unsigned int j = (unsigned int)((i + 1) % n);
				// Sens : pour un contour trigonometrique, (lo_i, hi_i, hi_j) donne
				// la normale (-dy, dx) -- vers l'INTERIEUR. C'est l'autre ordre
				// qu'il faut, et le volume signe du cas de test le mesure.
				mb.Tri (lo + (unsigned int)i, hi + j,               hi + (unsigned int)i);
				mb.Tri (lo + (unsigned int)i, lo + j,               hi + j);
			}
		}
	}

	// --- les couronnes, du plus grand anneau vers le plus rentre -------------
	ring_grid::Grid grid;
	for (std::size_t k = levels - 1; k > 0; --k)
	{
		const std::vector<ExtrudeContour>& outer = rings[k];
		const std::vector<ExtrudeContour>& inner = rings[k - 1];
		if (std::fabs (z[k] - z[k - 1]) < 1e-9f
		 && std::fabs (profile.points[k].y - profile.points[k - 1].y) < 1e-9)
			continue;   // deux points identiques dans le profil : rien entre eux

		// LA COURONNE, et surtout : PAS de `differenceContours` ici. Une seconde
		// passe Clipper2 recalcule la frontiere exterieure, et elle en profite
		// pour recoller des micro-aretes que la paroi, elle, a gardees --
		// resultat, quatre a douze aretes non partagees sur un vrai glyphe, donc
		// une peau trouee. (Mesure, pas suppose : les quatre polices difficiles
		// du catalogue le montraient toutes.)
		//
		// La soustraction est donc faite par la REGLE DE REMPLISSAGE, comme
		// partout ailleurs dans ce depot : l'anneau interieur est fourni a
		// l'envers, et le NonZero du tessellateur le retire. Les sommets rendus
		// sont alors, mot pour mot, ceux des deux anneaux.
		std::vector<ExtrudeContour> band = outer;
		for (const ExtrudeContour& c : inner)
		{
			ExtrudeContour reversed = c;
			std::reverse (reversed.pts.begin(), reversed.pts.end());
			band.push_back (std::move (reversed));
		}
		if (!tessellate (band, tv, tf)) continue;

		grid.clear();
		ring_grid::insert (grid, outer, 'o');
		ring_grid::insert (grid, inner, 'i');

		const unsigned int b = (unsigned int)(mb.verts.size() / 3);
		for (const Vector2f& p : tv)
		{
			char tag = ring_grid::lookup (grid, p);
			if (tag == 0)
			{
				// Point qu'aucun des deux anneaux ne reclame a la tolerance : un
				// sommet ajoute par le callback COMBINE du tessellateur. Rattache
				// au plus proche -- par balayage complet, cette fois -- et COMPTE.
				// Si ce compteur grimpe, la reconnaissance par sommet ne suffit
				// plus et il faudra une vraie distance au SEGMENT.
				if (stats) stats->steinerPoints++;
				tag = (nearestSq (inner, p) <= nearestSq (outer, p)) ? 'i' : 'o';
			}
			mb.Add (p.x, p.y, (tag == 'i') ? z[k - 1] : z[k]);
		}
		// Meme sens que le capot du dessus : la couronne regarde vers le haut et
		// vers l'exterieur, et ComputeNormals fera le reste.
		for (std::size_t i = 0; i + 2 < tf.size() + 0; i += 3)
			mb.Tri (b + tf[i], b + tf[i + 1], b + tf[i + 2]);
		if (stats) stats->bands++;
	}

	// --- capot du dessus, normales vers +z ----------------------------------
	if (!tessellate (top, tv, tf)) return false;
	{
		const unsigned int b = (unsigned int)(mb.verts.size() / 3);
		for (const Vector2f& p : tv) mb.Add (p.x, p.y, opt.zTop);
		for (std::size_t i = 0; i + 2 < tf.size() + 0; i += 3)
			mb.Tri (b + tf[i], b + tf[i + 1], b + tf[i + 2]);
	}

	if (mb.faces.empty()) return false;

	out.SetVertices ((unsigned int)(mb.verts.size() / 3), mb.verts.data());
	out.SetFaces ((unsigned int)(mb.faces.size() / 3), 3, mb.faces.data());
	out.ComputeNormals ();
	out.IncrementRevision ();
	return true;
}
