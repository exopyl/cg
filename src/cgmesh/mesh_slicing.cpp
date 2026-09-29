#include "mesh_slicing.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <unordered_map>
#include <utility>

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

#include "contour_ops.h"
#include "mesh.h"

// Slicing multi-maillages : repere plateau, intersection, nettoyage des
// contours, imbrication et API haut niveau. Voir mesh_slicing.h pour la chaine
// de traitement et les limites connues, docs/mesh_slicing.md pour leur
// definition (D1-D8).

namespace cgmesh {
namespace slicing {

namespace
{
const float kFltEps = std::numeric_limits<float>::epsilon();

// Multiple de FLT_EPSILON de la marge du pre-filtre en z (voir zMargin).
constexpr double kZMarginEpsFactor = 4.0;

// Mode Winding : distance sous laquelle le premier point d'une boucle est dit
// SUR une enveloppe, pour le rattachement des faces (l'union Clipper2 arrondit
// a 1e-6 : la marge est large devant cet arrondi, petite devant un detail).
constexpr double kWindingAttachTol = 1e-4;

// Une face dans le plan ne vote (pseudo-entrees) que si sa normale est
// franchement verticale, |nz| au-dessus de ce seuil. En dessous, elle n'a pas
// de cote defini : ni haut ni bas d'un solide.
constexpr float kHorizontalVoteMinNz = 1.f - 1e-3f;

// (D8) Ecart maximal, relatif (et absolu sous 1), entre le premier et le
// dernier point d'une boucle fermee pour que le premier soit tenu pour sa
// fermeture : deux copies d'une meme jonction different au plus de quelques
// ulp FLOAT (interpolation en double arrondie en float, parfois dans l'ordre
// oppose). Quatre ulp relatifs : tres en deca de toute arete reelle, et le
// garde-fou arrete toujours les faux positifs (prolongement asymetrique,
// comblement a 0,005).
constexpr double kClosureTol = 4.0 * FLT_EPSILON;

// Nombre de threads quand hardware_concurrency () ne sait pas repondre.
[[maybe_unused]] constexpr unsigned int kFallbackThreads = 8;   // inutile sous __EMSCRIPTEN__

// Marge du pre-filtre en z. Le classement exact reste celui de classifyTriangle
// (+-FLT_EPSILON sur fl(vz - z)) ; le filtre n'a qu'a en etre un SUR-ensemble,
// d'ou quatre fois la tolerance, plus une part relative pour les grands z.
// z - marge et z + marge sont croissants en z : c'est ce qui rend le balayage
// equivalent au filtrage direct.
double zMargin (double z)
{
	return kZMarginEpsFactor * (double)kFltEps * (1.0 + std::fabs (z));
}

bool isCandidate (const PlateMesh& pm, int t, double z)
{
	const double m = zMargin (z);
	return (double)pm.zmin[t] <= z + m && (double)pm.zmax[t] >= z - m;
}

Vector3f vertexAt (const PlateMesh& pm, int v)
{
	return Vector3f (pm.vertices[3 * v + 0], pm.vertices[3 * v + 1], pm.vertices[3 * v + 2]);
}

// Normalisation : un vecteur nul reste nul (pas de NaN).
void normalizeInPlace (float& x, float& y, float& z)
{
	const float n2 = x * x + y * y + z * z;
	if (n2 > 0.f)
	{
		const float n = std::sqrt (n2);
		x /= n; y /= n; z /= n;
	}
}

// Balayage des triangles tries par zmin, pour des plans visites par z
// croissant. L'ensemble actif vaut exactement { t : isCandidate (t, z) }.
class ZSweep
{
public:
	explicit ZSweep (const PlateMesh& pm) : m_pm (pm) {}

	void advance (double z, std::vector<int>& candidates)
	{
		const double m = zMargin (z);
		const std::vector<int>& order = m_pm.byZmin;
		while (m_next < order.size () && (double)m_pm.zmin[order[m_next]] <= z + m)
			m_active.push_back (order[m_next++]);
		m_active.erase (std::remove_if (m_active.begin (), m_active.end (),
		                                [&] (int t) { return (double)m_pm.zmax[t] < z - m; }),
		                m_active.end ());
		candidates = m_active;
		// Ordre croissant d'indice : c'est lui, et non l'ordre du balayage, qui
		// fixe la sortie (ordre des segments, decision des aretes a une ou plus
		// de deux entrees).
		std::sort (candidates.begin (), candidates.end ());
	}

private:
	const PlateMesh& m_pm;
	size_t m_next = 0;
	std::vector<int> m_active;
};
} // namespace

// ---------------------------------------------------------------------------
//  Repere plateau et intersection
// ---------------------------------------------------------------------------

namespace
{
// Normale de Newell (non normee) d'une face, sur les coordonnees `v`
// (3 flottants par sommet) : robuste aux faces non planes et non convexes.
void newellNormal (const std::vector<float>& v, const std::vector<int>& idx, double n[3])
{
	n[0] = n[1] = n[2] = 0.;
	const size_t k = idx.size ();
	for (size_t i = 0; i < k; ++i)
	{
		const float* a = &v[3 * (size_t)idx[i]];
		const float* b = &v[3 * (size_t)idx[(i + 1) % k]];
		n[0] += ((double)a[1] - b[1]) * ((double)a[2] + b[2]);
		n[1] += ((double)a[2] - b[2]) * ((double)a[0] + b[0]);
		n[2] += ((double)a[0] - b[0]) * ((double)a[1] + b[1]);
	}
}

// Meme critere que la triangulation de Mesh (faceIsConvex, mesh.cpp, prive) :
// convexe une fois projetee sur sa normale de Newell.
bool isConvexFace (const std::vector<float>& v, const std::vector<int>& idx, const double n[3])
{
	const size_t k = idx.size ();
	if (k < 4) return true;
	double prevSign = 0.;
	for (size_t i = 0; i < k; ++i)
	{
		const float* a = &v[3 * (size_t)idx[(i + k - 1) % k]];
		const float* b = &v[3 * (size_t)idx[i]];
		const float* c = &v[3 * (size_t)idx[(i + 1) % k]];
		const double e1[3] = { (double)b[0] - a[0], (double)b[1] - a[1], (double)b[2] - a[2] };
		const double e2[3] = { (double)c[0] - b[0], (double)c[1] - b[1], (double)c[2] - b[2] };
		const double cx = e1[1] * e2[2] - e1[2] * e2[1];
		const double cy = e1[2] * e2[0] - e1[0] * e2[2];
		const double cz = e1[0] * e2[1] - e1[1] * e2[0];
		const double dot = cx * n[0] + cy * n[1] + cz * n[2];
		if (std::fabs (dot) < 1e-12) continue;
		const double sign = dot > 0. ? 1. : -1.;
		if (prevSign == 0.) prevSign = sign;
		else if (sign != prevSign) return false;
	}
	return true;
}

// Triangles (indices LOCAUX 0..k-1) d'une face. Triangle ou face convexe :
// eventail depuis le sommet 0, comme Mesh. Face NON convexe : la
// triangulation de Mesh elle-meme (glutess), obtenue par BuildTriangulation
// sur un Mesh temporaire reduit a cette seule face -- Mesh n'expose pas de
// triangulation par face avec la correspondance triangle -> face, et c'est la
// seule facon de l'obtenir sans reecrire un decoupage en oreilles. Chaque
// triangle rendu est remis dans le sens de la face (normale de Newell). Si
// glutess ne rend rien (face auto-intersectee), repli sur l'eventail.
std::vector<int> localTriangles (const std::vector<float>& v, const std::vector<int>& idx,
                                 const double n[3])
{
	const int k = (int)idx.size ();
	std::vector<int> local;
	if (k > 3 && !isConvexFace (v, idx, n))
	{
		std::vector<float> coords;
		coords.reserve (3 * (size_t)k);
		for (const int i : idx)
			coords.insert (coords.end (), { v[3 * (size_t)i], v[3 * (size_t)i + 1], v[3 * (size_t)i + 2] });
		std::vector<unsigned int> ring ((size_t)k);
		for (int i = 0; i < k; ++i) ring[i] = (unsigned int)i;
		Mesh one;
		one.SetVertices ((unsigned int)k, coords.data ());
		one.SetFaces (1, (unsigned int)k, ring.data ());
		const std::vector<unsigned int> tris = one.BuildTriangulation ();
		for (size_t t = 0; t + 2 < tris.size (); t += 3)
		{
			int a = (int)tris[t], b = (int)tris[t + 1], c = (int)tris[t + 2];
			if (a >= k || b >= k || c >= k) continue;
			const std::vector<int> tri = { idx[a], idx[b], idx[c] };
			double tn[3];
			newellNormal (v, tri, tn);
			if (tn[0] * n[0] + tn[1] * n[1] + tn[2] * n[2] < 0.) std::swap (b, c);
			local.insert (local.end (), { a, b, c });
		}
		if (!local.empty ()) return local;
	}
	for (int i = 1; i + 1 < k; ++i)
		local.insert (local.end (), { 0, i, i + 1 });
	return local;
}
} // namespace

PlateMesh buildPlateMesh (const Mesh& mesh, const Matrix4f& matrix)
{
	PlateMesh pm;

	double a[3][4];
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 4; ++c)
			a[r][c] = (double)matrix.at (r, c);

	// M * (x, y, z, 1), dont on garde xyz : la ligne homogene n'est pas lue,
	// la matrice est supposee affine.
	const unsigned int nv = mesh.GetNVertices ();
	const std::vector<float>& src = mesh.GetVertices ();
	pm.vertices.resize (3 * (size_t)nv);
	for (unsigned int i = 0; i < nv; ++i)
	{
		const double x = src[3 * i + 0], y = src[3 * i + 1], z = src[3 * i + 2];
		for (int r = 0; r < 3; ++r)
			pm.vertices[3 * i + r] = (float)(a[r][0] * x + a[r][1] * y + a[r][2] * z + a[r][3]);
	}

	const double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
	                 - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
	                 + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
	// (D4) Sous une symetrie, les sommets transformes gardent leur ordre mais
	// le triangle change de sens. On le retourne pour que sa normale reste
	// exterieure ; sans cela la normale du plan resterait +Z mais celle des
	// faces basculerait, et l'enroulement avec elle.
	pm.mirrored = det < 0.0;

	for (unsigned int fi = 0; fi < mesh.GetNFaces (); ++fi)
	{
		auto face = mesh.FaceAt (fi);
		if (!face) continue;
		const int n = face->GetNVertices ();
		if (n < 3) continue;
		// Un indice hors bornes lirait hors de pm.vertices : la face est
		// ignoree plutot que de corrompre la memoire.
		bool inRange = true;
		for (int k = 0; k < n && inRange; ++k)
		{
			const int vi = face->GetVertex (k);
			inRange = vi >= 0 && (unsigned int)vi < nv;
		}
		if (!inRange) continue;

		std::vector<int> idx ((size_t)n);
		for (int k = 0; k < n; ++k) idx[k] = face->GetVertex (k);

		// Triangulation sur les coordonnees OBJET (elle est invariante par
		// application affine, au sens pres, que retablit le retournement).
		double srcNormal[3];
		newellNormal (src, idx, srcNormal);
		const std::vector<int> local = localTriangles (src, idx, srcNormal);

		// Normale de la FACE en repere plateau (Newell, puis sens exterieur
		// retabli sous symetrie) : c'est elle qui donne son cote a une face
		// dans le plan. Un triangle d'eventail d'une face non convexe
		// peut etre retourne ; la face, non.
		double plateNormal[3];
		newellNormal (pm.vertices, idx, plateNormal);
		const double len = std::sqrt (plateNormal[0] * plateNormal[0] + plateNormal[1] * plateNormal[1] +
		                              plateNormal[2] * plateNormal[2]);
		float faceNz = (len > 0.) ? (float)(plateNormal[2] / len) : 0.f;
		if (pm.mirrored) faceNz = -faceNz;

		for (size_t t = 0; t + 2 < local.size (); t += 3)
		{
			const int v0 = idx[local[t]];
			int v1 = idx[local[t + 1]];
			int v2 = idx[local[t + 2]];
			if (pm.mirrored) std::swap (v1, v2);
			pm.triangles.push_back (v0);
			pm.triangles.push_back (v1);
			pm.triangles.push_back (v2);
			pm.faceOfTriangle.push_back ((int)fi);
			pm.faceNormalZ.push_back (faceNz);
		}
	}

	const int nt = pm.triangleCount ();
	pm.zmin.resize (nt);
	pm.zmax.resize (nt);
	for (int t = 0; t < nt; ++t)
	{
		const float z0 = pm.vertices[3 * pm.triangles[3 * t + 0] + 2];
		const float z1 = pm.vertices[3 * pm.triangles[3 * t + 1] + 2];
		const float z2 = pm.vertices[3 * pm.triangles[3 * t + 2] + 2];
		pm.zmin[t] = std::min (z0, std::min (z1, z2));
		pm.zmax[t] = std::max (z0, std::max (z1, z2));
	}
	pm.byZmin.resize (nt);
	std::iota (pm.byZmin.begin (), pm.byZmin.end (), 0);
	std::stable_sort (pm.byZmin.begin (), pm.byZmin.end (),
	                  [&] (int l, int r) { return pm.zmin[l] < pm.zmin[r]; });
	return pm;
}

namespace
{
// Cle d'arete independante de l'ordre, comme celle du linker.
uint64_t inPlaneEdgeKey (int a, int b)
{
	const auto lo = static_cast<uint32_t> (std::min (a, b));
	const auto hi = static_cast<uint32_t> (std::max (a, b));
	return (static_cast<uint64_t> (lo) << 32) | hi;
}

// Une arete CONTENUE dans le plan, vue depuis une face qui la borde.
//   side = +1 : la face est "dessus" -- troisieme sommet au-dessus (nOn == 2),
//               ou face dans le plan de normale montante (nOn == 3) ;
//   side = -1 : la face est "dessous".
// Convention SOLIDE FERME : une face horizontale de normale +Z est le haut
// d'un solide, la matiere est sous elle ; elle compte donc du cote oppose a un
// mur qui descend depuis la meme arete, et l'arete (un bord de la section) est
// gardee. Deux faces du meme cote (arete "posee" sur le plan, ou diagonale
// d'une face horizontale) ne bordent pas la section : l'arete est retiree.
struct InPlaneEntry
{
	uint64_t key = 0;
	int side = 0;
	int slot = -1;      // indice du segment reel candidat (nOn == 2), -1 sinon
};

// Classement d'UN triangle. Les segments nOn == 0 / 1 sont definitifs ; un
// segment nOn == 2 est un CANDIDAT (slot) que la decision par arete gardera
// ou non ; un triangle nOn == 3 n'emet rien, mais depose une pseudo-entree
// par arete.
void classifyTriangle (const PlateMesh& pm, int t, float z,
                       std::vector<SliceSegment>& segs, std::vector<bool>& isCandidateSlot,
                       std::vector<InPlaneEntry>& entries)
{
	const int f[3] = { pm.triangles[3 * t + 0], pm.triangles[3 * t + 1], pm.triangles[3 * t + 2] };
	const Vector3f v[3] = { vertexAt (pm, f[0]), vertexAt (pm, f[1]), vertexAt (pm, f[2]) };

	// Normale du triangle, en float (charge utile du segment)
	const float e1x = v[1].x - v[0].x, e1y = v[1].y - v[0].y, e1z = v[1].z - v[0].z;
	const float e2x = v[2].x - v[0].x, e2y = v[2].y - v[0].y, e2z = v[2].z - v[0].z;
	float nx = e1y * e2z - e1z * e2y;
	float ny = e1z * e2x - e1x * e2z;
	float nz = e1x * e2y - e1y * e2x;
	normalizeInPlace (nx, ny, nz);

	// (D3) Aucune face n'est rejetee sur sa normale : une face HORIZONTALE est
	// reconnue par nOn == 3, et une face QUASI horizontale qui traverse le plan
	// rend son segment.

	// Plan de normale +Z, d = -z : la distance signee vaut fl(vz - z).
	const float d = -z;
	float distances[3] = { 0.f, 0.f, 0.f };
	int nOver = 0, nUnder = 0, nOn = 0;
	for (int i = 0; i < 3; i++)
	{
		distances[i] = v[i].z + d;
		// (D1) trois etats a +-FLT_EPSILON en absolu
		if (distances[i] > kFltEps) nOver++;
		else if (distances[i] < -kFltEps) nUnder++;
		else nOn++;
	}

	if (nOn == 3)
	{
		// Face dans le plan : pseudo-entrees, cote donne par la normale de la
		// FACE dont le triangle est issu (Newell), et non par celle du triangle -- un triangle
		// d'eventail d'une face non convexe peut etre retourne. Une face dont
		// la normale n'est pas franchement verticale ne vote pas.
		// PlateMesh rempli a la main sans faceNormalZ : pas de vote.
		if ((size_t)t >= pm.faceNormalZ.size ()) return;
		const float faceNz = pm.faceNormalZ[t];
		if (std::fabs (faceNz) <= kHorizontalVoteMinNz) return;
		const int side = (faceNz > 0.f) ? +1 : -1;
		for (int i = 0; i < 3; ++i)
			entries.push_back ({ inPlaneEdgeKey (f[i], f[(i + 1) % 3]), side, -1 });
		return;
	}

	bool found = false;
	int nFound = 0;
	SliceSegment seg;
	seg.faceId = pm.faceOfTriangle[t];

	// Interpolation sur l'arete (i1, i2) ; en double, arrondie en float.
	const auto interpolate = [&] (int i1, int i2) {
		const double tt = (double)distances[i1] / ((double)distances[i1] - (double)distances[i2]);
		return Vector3f ((float)(v[i1].x + tt * ((double)v[i2].x - v[i1].x)),
		                 (float)(v[i1].y + tt * ((double)v[i2].y - v[i1].y)),
		                 (float)(v[i1].z + tt * ((double)v[i2].z - v[i1].z)));
	};

	if (nOn == 0)   // cas general
	{
		const auto addCase0 = [&] (int i1, int i2) {
			if (nFound < 2)
			{
				seg.points[nFound] = interpolate (i1, i2);
				seg.edges[nFound][0] = f[i1];
				seg.edges[nFound][1] = f[i2];
				nFound++;
			}
		};
		const auto crosses = [&] (int i1, int i2) {
			return (distances[i1] < 0.f && distances[i2] > 0.f) ||
			       (distances[i1] > 0.f && distances[i2] < 0.f);
		};
		if (crosses (0, 1)) addCase0 (0, 1);
		if (crosses (1, 2)) addCase0 (1, 2);
		if (crosses (0, 2)) addCase0 (0, 2);
		found = (nFound == 2);
	}
	else if (nOn == 1)
	{
		if (nUnder == 1 && nOver == 1)
		{
			for (int i = 0; i < 3; i++)
			{
				if (std::fabs (distances[i]) <= kFltEps)
				{
					const int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
					seg.points[0] = interpolate (i1, i2);
					seg.edges[0][0] = f[i1];
					seg.edges[0][1] = f[i2];
					seg.points[1] = v[i];
					seg.edges[1][0] = f[i];
					seg.edges[1][1] = f[i];
					break;
				}
			}
			found = true;
		}
	}
	else   // nOn == 2 : une arete dans le plan
	{
		for (int i = 0; i < 3; i++)
		{
			if (nFound < 2 && std::fabs (distances[i]) <= kFltEps)
			{
				seg.points[nFound] = v[i];
				seg.edges[nFound][0] = f[i];
				seg.edges[nFound][1] = f[i];
				nFound++;
			}
		}
		found = (nFound == 2);
	}

	if (!found)
		return;

	// Normale gardee comme charge utile (et pour le recoupement des tests) :
	// elle ne decide pas de l'orientation.
	seg.faceNormal = Vector3f (nx, ny, nz);

	// (D6) ORIENTATION COMBINATOIRE. Une regle geometrique -- normale de face
	// a gauche, (n x p) . z <= 0 -- laisserait au hasard un segment de
	// longueur nulle (p normalise en NaN) ou une face degeneree (n nulle). La
	// regle est donc tiree du seul ENROULEMENT du triangle (repere plateau,
	// remis dans le sens exterieur sous symetrie) et du classement de ses
	// sommets :
	//
	//   le segment va du point ou l'enroulement MONTE (arete dessous -> dessus)
	//   au point ou il DESCEND (arete dessus -> dessous).
	//
	// Un sommet dans le plan compte, selon le cas, pour l'une ou l'autre :
	//   nOn == 0 : de l'arete montante vers l'arete descendante ;
	//   nOn == 1 : sommet V dans le plan, X et Y les suivants dans l'ordre
	//              d'enroulement ; X dessous -> du croisement de XY vers V,
	//              X dessus -> de V vers le croisement ;
	//   nOn == 2 : arete P -> Q dans l'ordre d'enroulement, troisieme sommet R
	//              (ordre R, P, Q) ; R dessous -> de P vers Q, R dessus -> de Q
	//              vers P.
	// Pour un triangle non degenere, c'est exactement dir = + n x z, soit la
	// regle geometrique ci-dessus : boucle brute HORAIRE (vue de +Z) pour un
	// maillage oriente vers l'exterieur. Un recoupement le verifie en test.
	int startVertex = -1;               // nOn == 1 / 2 : le depart est un sommet
	int startEdgeA = -1, startEdgeB = -1;   // nOn == 0 / 1 : le depart est un croisement
	if (nOn == 0)
	{
		for (int i = 0; i < 3; ++i)
		{
			const int j = (i + 1) % 3;
			if (distances[i] < 0.f && distances[j] > 0.f) { startEdgeA = f[i]; startEdgeB = f[j]; }
		}
	}
	else if (nOn == 1)
	{
		int iv = 0;
		while (std::fabs (distances[iv]) > kFltEps) ++iv;
		const int ix = (iv + 1) % 3, iy = (iv + 2) % 3;
		if (distances[ix] < 0.f) { startEdgeA = f[ix]; startEdgeB = f[iy]; }
		else startVertex = f[iv];
	}
	else   // nOn == 2
	{
		int ir = 0;
		while (std::fabs (distances[ir]) <= kFltEps) ++ir;
		const int ip = (ir + 1) % 3, iq = (ir + 2) % 3;
		startVertex = (distances[ir] < 0.f) ? f[ip] : f[iq];
	}
	const auto isStart = [&] (int k) {
		if (startVertex >= 0)
			return seg.edges[k][0] == startVertex && seg.edges[k][1] == startVertex;
		return (seg.edges[k][0] == startEdgeA && seg.edges[k][1] == startEdgeB) ||
		       (seg.edges[k][0] == startEdgeB && seg.edges[k][1] == startEdgeA);
	};
	if (!isStart (0) && isStart (1))
		seg.inverse ();

	if (nOn == 2)
	{
		entries.push_back ({ inPlaneEdgeKey (seg.edges[0][0], seg.edges[1][0]),
		                     (nOver == 1) ? +1 : -1, (int)segs.size () });
		isCandidateSlot.push_back (true);
	}
	else
	{
		isCandidateSlot.push_back (false);
	}
	segs.push_back (seg);
}
} // namespace

void intersectTriangles (const PlateMesh& pm, const std::vector<int>& triangles, float z,
                         std::vector<SliceSegment>& out)
{
	std::vector<SliceSegment> segs;
	std::vector<bool> isCandidateSlot;
	std::vector<InPlaneEntry> entries;
	for (const int t : triangles)
		classifyTriangle (pm, t, z, segs, isCandidateSlot, entries);

	// Aucune arete dans le plan : rien a decider.
	if (entries.empty ())
	{
		out.insert (out.end (), segs.begin (), segs.end ());
		return;
	}

	// Decision par arete, independante de l'ordre des triangles quand l'arete
	// a exactement deux entrees (cas d'une surface fermee et variete).
	std::unordered_map<uint64_t, std::vector<int>> byEdge;   // arete -> entrees, dans l'ordre
	byEdge.reserve (entries.size ());
	for (int e = 0; e < (int)entries.size (); ++e)
		byEdge[entries[e].key].push_back (e);

	std::vector<bool> keep (segs.size ());
	for (size_t i = 0; i < segs.size (); ++i)
		keep[i] = !isCandidateSlot[i];   // nOn == 0 / 1 : toujours gardes

	for (const auto& kv : byEdge)
	{
		const std::vector<int>& ids = kv.second;
		if (ids.size () == 2)
		{
			// Cotes opposes : l'arete borde la section, gardee UNE fois, par un
			// segment reel (une face non horizontale, orientee par la regle
			// combinatoire). Meme cote : retiree.
			const InPlaneEntry& a = entries[ids[0]];
			const InPlaneEntry& b = entries[ids[1]];
			if (a.side == b.side) continue;
			// (D8) Deux entrees REELLES : on garde celle de la face "dessous",
			// choix invariant par permutation des faces. Les deux segments ont
			// les memes extremites (sommets du maillage) et le meme sens ; seul
			// le faceId retenu en depend.
			int slot = -1;
			if (a.slot >= 0 && b.slot >= 0) slot = (a.side < 0) ? a.slot : b.slot;
			else slot = (a.slot >= 0) ? a.slot : b.slot;
			if (slot >= 0) keep[slot] = true;
			continue;
		}
		// Une entree (bord du maillage) ou plus de deux (arete non-variete) :
		// seules les entrees REELLES comptent, a la cle exacte. La premiere est
		// emise ; si une entree reelle posterieure est du meme cote, elle est
		// emise INVALIDE (valid = false) : presente pour le linker, jamais point
		// de depart d'un chemin. C'est ce qui referme par exemple un tore a face
		// dupliquee (T13 a). Les pseudo-entrees ne votent pas ici. Ce cas reste
		// dependant de l'ordre des faces (D8).
		int first = -1;
		bool sameSideLater = false;
		for (const int e : ids)
		{
			if (entries[e].slot < 0) continue;
			if (first < 0) { first = e; continue; }
			if (entries[e].side == entries[first].side) sameSideLater = true;
		}
		if (first >= 0)
		{
			keep[entries[first].slot] = true;
			segs[entries[first].slot].valid = !sameSideLater;
		}
	}

	// Ordre de sortie : celui des triangles (le segment d'une arete garde prend
	// la place de sa premiere entree reelle).
	for (size_t i = 0; i < segs.size (); ++i)
		if (keep[i]) out.push_back (segs[i]);
}

void intersectAtZ (const PlateMesh& pm, float z, std::vector<SliceSegment>& out)
{
	std::vector<int> candidates;
	for (int t = 0; t < pm.triangleCount (); ++t)
		if (isCandidate (pm, t, (double)z))
			candidates.push_back (t);
	intersectTriangles (pm, candidates, z, out);
}

// ---------------------------------------------------------------------------
//  Contours 2D : aire, orientation, nettoyage, decoupe, point dans polygone
// ---------------------------------------------------------------------------

double contourSignedAreaD (const Contour2d& c)
{
	const size_t n = c.size ();
	if (n < 3) return 0.;
	double area2 = 0.;
	for (size_t i = 0; i < n; i++)
	{
		const Vector2d& p = c[i];
		const Vector2d& q = c[(i + 1) % n];
		area2 += p.x * q.y - q.x * p.y;
	}
	return 0.5 * area2;
}

bool isClockwise (const Contour2d& c)
{
	const size_t n = c.size ();
	if (n < 3) return false;

	// sommet le plus bas, puis le plus a droite
	double xRef = c[0].x, yRef = c[0].y;
	size_t idx = 0;
	for (size_t i = 1; i < n; i++)
	{
		const double x = c[i].x, y = c[i].y;
		if (y < yRef || (y == yRef && x > xRef))
		{
			yRef = y; xRef = x; idx = i;
		}
	}
	const Vector2d& before = c[(idx + n - 1) % n];
	const Vector2d& after = c[(idx + 1) % n];
	const double cross = (after.x - xRef) * (before.y - yRef) - (after.y - yRef) * (before.x - xRef);
	return cross < 0.;
}

void removeClosePoints (Contour2d& c, double eps)
{
	const size_t n = c.size ();
	if (n == 0) return;

	const auto almostEqual = [eps] (double x, double y) { return std::fabs (x - y) < eps; };

	Contour2d cleaned;
	cleaned.push_back (c[0]);
	const double xFirst = c[0].x, yFirst = c[0].y;
	double xLast = xFirst, yLast = yFirst;

	for (size_t i = 1; i < n; i++)
	{
		const double x = c[i].x, y = c[i].y;

		// le premier point revient (et ce n'est pas une simple repetition du
		// premier) : on s'arrete s'il n'y a plus rien de neuf ensuite
		if (almostEqual (x, xFirst) && almostEqual (y, yFirst) && cleaned.size () != 1)
		{
			bool furtherPoint = false;
			for (size_t j = i + 1; j < n; j++)
			{
				if (!almostEqual (c[j].x, xLast) || !almostEqual (c[j].y, yLast))
				{
					furtherPoint = true;
					break;
				}
			}
			if (!furtherPoint)
				break;
		}

		if (!almostEqual (x, xLast) || !almostEqual (y, yLast))
		{
			cleaned.push_back (c[i]);
			xLast = x;
			yLast = y;
		}
	}
	c = std::move (cleaned);
}

namespace
{
// Angle non signe entre deux vecteurs, dans [0, pi] : acos du cosinus borne.
// Un vecteur nul rend NaN, et le sommet est alors retire par removeFlatAngle
// (NaN > seuil est faux).
double vectorAngle (double ax, double ay, double bx, double by)
{
	const double na = std::sqrt (ax * ax + ay * ay);
	const double nb = std::sqrt (bx * bx + by * by);
	double c = (ax * bx + ay * by) / (na * nb);
	if (c > 1.) c = 1.;
	if (c < -1.) c = -1.;
	return std::acos (c);
}
} // namespace

void removeFlatAngle (Contour2d& c, double threshold)
{
	const auto angleAt = [] (const Vector2d& p1, const Vector2d& p2, const Vector2d& p3) {
		return vectorAngle (p2.x - p1.x, p2.y - p1.y, p3.x - p2.x, p3.y - p2.y);
	};

	const size_t n = c.size ();
	Contour2d cleaned;

	// premier point a garder
	size_t iFirst = 0;
	for (iFirst = 0; iFirst < n; iFirst++)
	{
		const double angle = angleAt (c[(iFirst - 1 + n) % n], c[iFirst], c[(iFirst + 1) % n]);
		if (std::fabs (angle) > threshold)
		{
			cleaned.push_back (c[iFirst]);
			break;
		}
	}

	for (size_t i = iFirst + 1; i < n; i++)
	{
		const Vector2d& p3 = c[(i == n - 1) ? iFirst : i + 1];
		const double angle = angleAt (cleaned.back (), c[i], p3);
		if (std::fabs (angle) > threshold)
			cleaned.push_back (c[i]);
	}
	c = std::move (cleaned);
}

void cleanContour (Contour2d& c, double closeEps, double angleEps)
{
	removeClosePoints (c, closeEps);
	removeFlatAngle (c, angleEps);
}

bool splitAtRepeatedPoints (const Contour2d& c, std::vector<Contour2d>& out, double eps)
{
	out.clear ();
	const size_t n = c.size ();
	if (n < 3) return false;

	const auto almostEqual = [eps] (double x, double y) { return std::fabs (x - y) < eps; };

	for (size_t i = 0; i + 2 < n; i++)
	{
		for (size_t j = i + 2; j < n; j++)
		{
			if (almostEqual (c[i].x, c[j].x) && almostEqual (c[i].y, c[j].y))
			{
				Contour2d first (c.begin () + i, c.begin () + j);
				std::vector<Contour2d> sub;
				if (splitAtRepeatedPoints (first, sub, eps))
					out.insert (out.end (), sub.begin (), sub.end ());
				else
					out.push_back (std::move (first));

				Contour2d second (c.begin () + j, c.end ());
				second.insert (second.end (), c.begin (), c.begin () + i);
				if (splitAtRepeatedPoints (second, sub, eps))
					out.insert (out.end (), sub.begin (), sub.end ());
				else
					out.push_back (std::move (second));

				return !out.empty ();
			}
		}
	}
	return !out.empty ();
}

bool isPointInside (const Contour2d& c, double x, double y, double& minSquaredDistance)
{
	minSquaredDistance = std::numeric_limits<double>::max ();
	int crossings = 0;
	const size_t n = c.size ();
	for (size_t i = 0; i < n; i++)
	{
		const double x1 = c[i].x, y1 = c[i].y;
		const double x2 = c[(i + 1) % n].x, y2 = c[(i + 1) % n].y;
		const double d = (y - y1) * (x2 - x1) - (x - x1) * (y2 - y1);

		const double sq = (x - x1) * (x - x1) + (y - y1) * (y - y1);
		if (sq < minSquaredDistance)
			minSquaredDistance = sq;

		if ((y1 >= y) != (y2 >= y))
			crossings += (y2 - y1 >= 0) ? (d >= 0) : (d <= 0);

		// Point sur le bord : |d| <= DBL_EPSILON et dans l'emprise de l'arete.
		if (std::fabs (d) <= std::numeric_limits<double>::epsilon () &&
		    std::min (x1, x2) <= x && x <= std::max (x1, x2) &&
		    std::min (y1, y2) <= y && y <= std::max (y1, y2))
			return true;
	}
	return (crossings & 0x01) != 0;
}

// ---------------------------------------------------------------------------
//  Imbrication par contenance
// ---------------------------------------------------------------------------

namespace
{
// Emprise en FLOAT : le pre-filtre de contenance travaille a la precision du
// stockage.
struct BoxF
{
	float x0 = std::numeric_limits<float>::max (), y0 = std::numeric_limits<float>::max ();
	float x1 = std::numeric_limits<float>::lowest (), y1 = std::numeric_limits<float>::lowest ();
};

BoxF boxOf (const Contour2d& c)
{
	BoxF b;
	for (const Vector2d& p : c)
	{
		b.x0 = std::min (b.x0, (float)p.x); b.x1 = std::max (b.x1, (float)p.x);
		b.y0 = std::min (b.y0, (float)p.y); b.y1 = std::max (b.y1, (float)p.y);
	}
	return b;
}

// Inclusion LARGE des emprises, bords compris.
bool boxInside (const BoxF& inner, const BoxF& outer)
{
	return inner.x0 >= outer.x0 && inner.x1 <= outer.x1 &&
	       inner.y0 >= outer.y0 && inner.y1 <= outer.y1;
}

// `inner` est-il dans `container` ? Tous ses points doivent l'etre, sauf UN au
// plus, s'il est a moins de 0,0012 (mm) du sommet le plus proche : une
// tolerance empirique, qui absorbe un point pose sur le bord a l'arrondi pres
// sans pretendre a la robustesse (un point tres proche d'une ARETE mais loin
// de tout sommet n'en beneficie pas).
bool containsWithTolerance (const Contour2d& container, const Contour2d& inner,
                            const BoxF& containerBox, const BoxF& innerBox)
{
	if (!boxInside (innerBox, containerBox))
		return false;

	const double tol = 0.0012;
	const double tol2 = tol * tol;
	const size_t maxOutside = 1;
	size_t nOutside = 0;
	for (const Vector2d& p : inner)
	{
		double minSq = std::numeric_limits<double>::max ();
		if (!isPointInside (container, p.x, p.y, minSq))
		{
			if (minSq < tol2)
			{
				nOutside++;
				if (nOutside > maxOutside)
					return false;
			}
			else
			{
				return false;
			}
		}
	}
	return true;
}
} // namespace

std::vector<NestedContour> nestContours (const std::vector<Contour2d>& soup)
{
	const int n = (int)soup.size ();
	std::vector<NestedContour> result;
	if (n < 2)
	{
		for (int i = 0; i < n; ++i)
			result.push_back (NestedContour { i, {} });
		return result;
	}

	// noeud = un contour de la soupe, avec ses trous (indices de noeuds)
	std::vector<std::vector<int>> inners (n);

	// tri par aire croissante (aire non signee, sans trous a ce stade)
	std::vector<double> areas (n);
	for (int i = 0; i < n; ++i)
		areas[i] = std::fabs (contourSignedAreaD (soup[i]));
	std::vector<int> slots (n);
	std::iota (slots.begin (), slots.end (), 0);
	std::stable_sort (slots.begin (), slots.end (),
	                  [&] (int l, int r) { return areas[l] < areas[r]; });

	std::vector<BoxF> boxes (slots.size ());
	for (size_t i = 0; i < slots.size (); ++i)
		boxes[i] = boxOf (soup[slots[i]]);

	// Garde contre les cycles de contenance entre contours quasi identiques
	// (chacun "dans" l'autre a la tolerance pres) : un contour ne peut etre
	// rendu a la soupe plus de fois qu'il n'y a de contours.
	const size_t maxDumps = slots.size ();
	std::map<int, size_t> dumpCounts;

	for (size_t i = 0; i + 1 < slots.size (); i++)
	{
		const int a = slots[i];
		if (a < 0 || soup[a].empty ())
			continue;

		for (size_t j = i + 1; j < slots.size (); j++)
		{
			const int b = slots[j];
			if (b < 0 || soup[b].empty ())
				continue;

			if (containsWithTolerance (soup[b], soup[a], boxes[j], boxes[i]))
			{
				// `a` a deja des trous (profondeur 1). Il devient trou a son
				// tour, et ses trous retournent a la soupe (deux niveaux).
				if (!inners[a].empty ())
				{
					const std::vector<int> dumped = inners[a];
					for (const int inner : dumped)
					{
						if (++dumpCounts[inner] > maxDumps)
							continue;
						slots.push_back (inner);
						boxes.push_back (boxOf (soup[inner]));
					}
					inners[a].clear ();
				}
				inners[b].push_back (a);
				slots[i] = -1;
				break;
			}
		}
	}

	for (const int s : slots)
	{
		if (s < 0) continue;
		result.push_back (NestedContour { s, inners[s] });
	}
	return result;
}

// ---------------------------------------------------------------------------
//  Boucles d'un plan et regions
// ---------------------------------------------------------------------------

namespace
{
void loopsFromCandidates (const PlateMesh& pm, float z, const std::vector<int>& candidates,
                          std::vector<SliceLoop>& loops, bool fromHollowing)
{
	std::vector<SliceSegment> segments;
	intersectTriangles (pm, candidates, z, segments);

	// Les points sont deja en repere plateau : pas de retour par M.
	SliceLinker linker (segments);
	linker.execute ();

	std::vector<std::vector<Vector3f>> paths;
	std::vector<std::vector<int>> faces;
	std::vector<bool> closed;
	traceChains (segments, paths, faces, &closed);

	loops.reserve (loops.size () + paths.size ());
	for (size_t i = 0; i < paths.size (); i++)
	{
		SliceLoop loop;
		loop.pts.reserve (paths[i].size ());
		for (const Vector3f& p : paths[i])
			loop.pts.push_back (Vector2d ((double)p.x, (double)p.y));   // z abandonne
		loop.faceIds = std::move (faces[i]);
		loop.fromHollowing = fromHollowing;
		loop.closed = closed[i];
		loops.push_back (std::move (loop));
	}
}

std::vector<Vector2f> toFloat (const Contour2d& c)
{
	std::vector<Vector2f> out;
	out.reserve (c.size ());
	for (const Vector2d& p : c)
		out.push_back (Vector2f ((float)p.x, (float)p.y));
	return out;
}

// Contour oriente selon son role (convention O4) : enveloppe positive, trou
// negatif. Un contour degenere (aire nulle) est laisse tel quel.
ExtrudeContour orientedContour (const Contour2d& c, bool hole)
{
	ExtrudeContour ec;
	ec.pts = toFloat (c);
	ec.isHole = hole;
	const double a = contourSignedAreaD (c);
	if ((hole && a > 0.) || (!hole && a < 0.))
		std::reverse (ec.pts.begin (), ec.pts.end ());
	return ec;
}

// Distance du point au bord d'un contour ferme.
double distanceToContour (const std::vector<Vector2f>& c, double x, double y)
{
	double best = std::numeric_limits<double>::max ();
	const size_t n = c.size ();
	for (size_t i = 0; i < n; ++i)
	{
		const double ax = c[i].x, ay = c[i].y;
		const double bx = c[(i + 1) % n].x, by = c[(i + 1) % n].y;
		const double dx = bx - ax, dy = by - ay;
		const double l2 = dx * dx + dy * dy;
		double t = (l2 > 0.) ? ((x - ax) * dx + (y - ay) * dy) / l2 : 0.;
		t = std::max (0., std::min (1., t));
		const double px = ax + t * dx - x, py = ay + t * dy - y;
		best = std::min (best, std::sqrt (px * px + py * py));
	}
	return best;
}

// `hullD` : l'enveloppe `hull` deja convertie en double (une fois par region).
bool insideOrOn (const std::vector<Vector2f>& hull, const Contour2d& hullD,
                 double x, double y, double tol)
{
	double unused = 0.;
	return isPointInside (hullD, x, y, unused) || distanceToContour (hull, x, y) <= tol;
}
} // namespace

void sliceLoopsAtZ (const PlateMesh& pm, float z, std::vector<SliceLoop>& loops, bool fromHollowing)
{
	std::vector<int> candidates;
	for (int t = 0; t < pm.triangleCount (); ++t)
		if (isCandidate (pm, t, (double)z))
			candidates.push_back (t);
	loopsFromCandidates (pm, z, candidates, loops, fromHollowing);
}

namespace
{
// (D8) Ordre lexicographique EXACT des points (x, puis y).
bool pointLess (const Vector2d& a, const Vector2d& b)
{
	return a.x < b.x || (a.x == b.x && a.y < b.y);
}

// (D8) Fait demarrer une boucle FERMEE (sans point de fermeture repete)
// sur son point lexicographiquement minimal. A egalite (points confondus,
// pincement), on departage en comparant les sequences de points qui suivent,
// dans le sens de la boucle. Seul cas residuel : une boucle PERIODIQUE, dont
// plusieurs rotations minimales sont identiques -- elles rendent alors la
// meme suite de points, donc la meme sortie. Le sens de parcours (topologique)
// n'est pas touche.
void rotateToCanonicalStart (Contour2d& c)
{
	const size_t n = c.size ();
	if (n < 2) return;
	size_t best = 0;
	for (size_t i = 1; i < n; ++i)
	{
		if (pointLess (c[i], c[best])) { best = i; continue; }
		if (pointLess (c[best], c[i])) continue;
		// egalite : la suite la plus petite l'emporte
		for (size_t k = 1; k < n; ++k)
		{
			const Vector2d& a = c[(i + k) % n];
			const Vector2d& b = c[(best + k) % n];
			if (pointLess (a, b)) { best = i; break; }
			if (pointLess (b, a)) break;
		}
	}
	std::rotate (c.begin (), c.begin () + (std::ptrdiff_t)best, c.end ());
}

// (D8) Ordre canonique des boucles d'une couche, avant l'imbrication :
// l'ordre de sortie de traceChains suit celui des triangles, et
// l'imbrication (tri stable par aire) comme l'union en heritent.
bool loopLess (const SliceLoop& a, const SliceLoop& b)
{
	if (std::lexicographical_compare (a.pts.begin (), a.pts.end (), b.pts.begin (), b.pts.end (), pointLess))
		return true;
	if (std::lexicographical_compare (b.pts.begin (), b.pts.end (), a.pts.begin (), a.pts.end (), pointLess))
		return false;
	return a.fromHollowing < b.fromHollowing;
}

// Ancrage des boucles fermees, nettoyage (cleanContour (0.01, 0.001 rad)),
// decoupe des boucles auto-intersectees a leurs points doubles, retrait des
// contours de moins de trois points, puis tri canonique. Les boucles sont
// consommees.
std::vector<SliceLoop> cleanAndSplit (std::vector<SliceLoop>& loops, Nesting nesting)
{
	// (D8) Ancrage canonique des boucles FERMEES, avant le nettoyage -- dont le
	// resultat depend du point de depart (removeClosePoints, removeFlatAngle).
	// Le premier point d'un chemin ferme (copie calculee par le segment de
	// depart) est retire : chaque jonction garde ainsi la copie calculee par le
	// segment qui y ARRIVE, quel que soit le depart. Puis rotation vers le
	// point canonique, et fermeture remise (cleanContour l'attend et la retire). Les
	// chemins OUVERTS (loop.closed faux) ne sont pas touches : leurs
	// extremites ont un sens.
	for (SliceLoop& loop : loops)
	{
		if (!loop.closed || loop.pts.size () < 3) continue;
		// Garde-fou : on ne retire le premier point que s'il est bien la
		// fermeture, a kClosureTol pres du dernier. Sinon la boucle n'est pas
		// ancree (elle reste dependante de l'ordre, mais aucun sommet n'est perdu).
		const Vector2d& a = loop.pts.front ();
		const Vector2d& b = loop.pts.back ();
		const double scale = std::max (1.0, std::max (std::fabs (a.x), std::fabs (a.y)));
		if (std::fabs (a.x - b.x) > kClosureTol * scale || std::fabs (a.y - b.y) > kClosureTol * scale)
			continue;
		loop.pts.erase (loop.pts.begin ());
		rotateToCanonicalStart (loop.pts);
		loop.pts.push_back (loop.pts.front ());
	}

	for (SliceLoop& loop : loops)
		cleanContour (loop.pts, 0.01, 0.001);

	std::vector<SliceLoop> items;
	items.reserve (loops.size ());
	for (SliceLoop& loop : loops)
	{
		std::vector<Contour2d> pieces;
		if (splitAtRepeatedPoints (loop.pts, pieces, std::numeric_limits<double>::epsilon ()))
		{
			for (Contour2d& piece : pieces)
			{
				// Chaque morceau perd ses faces sources. En mode Containment il est
				// mis dans le sens horaire -- sans effet sur la sortie, dont
				// l'orientation suit le role donne par l'imbrication ; en mode
				// Winding, ou l'orientation fait foi, il garde son sens de parcours.
				if (nesting == Nesting::Containment && !isClockwise (piece))
					std::reverse (piece.begin (), piece.end ());
				SliceLoop item;
				item.pts = std::move (piece);
				item.fromHollowing = loop.fromHollowing;
				items.push_back (std::move (item));
			}
		}
		else
		{
			items.push_back (std::move (loop));
		}
	}
	loops.clear ();

	// Contours DEGENERES ecartes, dans les deux modes : moins de trois points
	// apres nettoyage et decoupe -- bouts d'une chaine ouverte, contour que le
	// nettoyage (0.01) a vide (juste sous une pointe), reste d'une decoupe.
	// Passes a l'imbrication, ils donneraient des regions a contour vide. Ils
	// emportent leurs faceIds / hollowFaceIds. Pas de filtre d'AIRE : un contour
	// d'au moins trois points alignes reste (limite documentee dans l'en-tete).
	items.erase (std::remove_if (items.begin (), items.end (),
	                             [] (const SliceLoop& item) { return item.pts.size () < 3; }),
	             items.end ());

	// (D8) Ordre canonique : ni l'imbrication (tri stable par aire) ni l'union
	// n'heritent de l'ordre des triangles.
	std::stable_sort (items.begin (), items.end (), loopLess);
	return items;
}

// Les faces d'une boucle vont dans la liste de SON maillage : principal ou
// evidement.
void appendFaces (SliceRegion& region, const SliceLoop& item)
{
	std::vector<int>& dst = item.fromHollowing ? region.hollowFaceIds : region.faceIds;
	dst.insert (dst.end (), item.faceIds.begin (), item.faceIds.end ());
}

// (D8) faceIds et hollowFaceIds tries : l'ordre de parcours depend du point de
// depart de chaque boucle. Les doublons (faces N > 3) sont gardes.
void sortFaceIds (SliceLayer& layer)
{
	for (SliceRegion& r : layer)
	{
		std::sort (r.faceIds.begin (), r.faceIds.end ());
		std::sort (r.hollowFaceIds.begin (), r.hollowFaceIds.end ());
	}
}

SliceRegion emptyRegion (int sourceMesh, bool isSupport, bool isHole)
{
	SliceRegion region;
	region.sourceMesh = sourceMesh;
	region.isSupport = isSupport;
	region.isHole = isHole;
	return region;
}

// Imbrication par contenance, orientation ignoree. La
// sortie O4 suit le ROLE donne par l'imbrication : un evidement de meme sens
// que le principal (T3) devient ainsi un trou negatif, ce que NonZero
// comprend. Les contours de moins de trois points ont deja ete ecartes.
SliceLayer buildRegionsContainment (const std::vector<SliceLoop>& items, int sourceMesh,
                                    bool isSupport, bool isHole)
{
	std::vector<Contour2d> soup;
	soup.reserve (items.size ());
	for (const SliceLoop& item : items)
		soup.push_back (item.pts);

	SliceLayer layer;
	for (const NestedContour& nc : nestContours (soup))
	{
		SliceRegion region = emptyRegion (sourceMesh, isSupport, isHole);
		region.contours.push_back (orientedContour (soup[nc.outer], false));
		appendFaces (region, items[nc.outer]);
		for (const int h : nc.holes)
		{
			region.contours.push_back (orientedContour (soup[h], true));
			appendFaces (region, items[h]);
		}
		layer.push_back (std::move (region));
	}
	sortFaceIds (layer);
	return layer;
}

// Imbrication par enroulement : union Clipper2 NonZero puis PolyTree.
// Enveloppe positive (le sens naturel est horaire, on retourne), sauf pour
// l'evidement, laisse horaire pour qu'il RETIRE de la matiere.
SliceLayer buildRegionsWinding (const std::vector<SliceLoop>& items, int sourceMesh,
                                bool isSupport, bool isHole)
{
	std::vector<ExtrudeContour> contours;
	contours.reserve (items.size ());
	for (const SliceLoop& item : items)
	{
		ExtrudeContour ec;
		ec.pts = toFloat (item.pts);
		if (!item.fromHollowing)
			std::reverse (ec.pts.begin (), ec.pts.end ());
		contours.push_back (std::move (ec));
	}

	SliceLayer layer;
	for (std::vector<ExtrudeContour>& rc : contourRegions (contours, false))
	{
		SliceRegion region = emptyRegion (sourceMesh, isSupport, isHole);
		region.contours = std::move (rc);
		layer.push_back (std::move (region));
	}

	// Le lien boucle -> faces est perdu par l'union : on le reconstruit en
	// rattachant chaque boucle a la plus petite enveloppe qui contient (ou
	// touche, a kWindingAttachTol pres) son premier point. Chaque enveloppe
	// est convertie et mesuree UNE fois.
	std::vector<Contour2d> hullsD (layer.size ());
	std::vector<double> hullAreas (layer.size ());
	for (size_t r = 0; r < layer.size (); ++r)
	{
		const std::vector<Vector2f>& hull = layer[r].contours[0].pts;
		hullsD[r].reserve (hull.size ());
		for (const Vector2f& p : hull)
			hullsD[r].push_back (Vector2d (p.x, p.y));
		hullAreas[r] = std::fabs (contourSignedAreaD (hullsD[r]));
	}
	for (const SliceLoop& item : items)
	{
		if (item.faceIds.empty () || item.pts.empty ()) continue;
		int best = -1;
		double bestArea = std::numeric_limits<double>::max ();
		for (size_t r = 0; r < layer.size (); ++r)
		{
			if (hullAreas[r] >= bestArea) continue;
			if (!insideOrOn (layer[r].contours[0].pts, hullsD[r], item.pts[0].x, item.pts[0].y,
			                 kWindingAttachTol))
				continue;
			bestArea = hullAreas[r];
			best = (int)r;
		}
		if (best >= 0)
			appendFaces (layer[best], item);
	}
	sortFaceIds (layer);
	return layer;
}
} // namespace

SliceLayer buildRegions (std::vector<SliceLoop>& loops, int sourceMesh,
                         bool isSupport, bool isHole, Nesting nesting)
{
	const std::vector<SliceLoop> items = cleanAndSplit (loops, nesting);
	return (nesting == Nesting::Containment)
		? buildRegionsContainment (items, sourceMesh, isSupport, isHole)
		: buildRegionsWinding (items, sourceMesh, isSupport, isHole);
}

// ---------------------------------------------------------------------------
//  API haut niveau
// ---------------------------------------------------------------------------

namespace
{
#ifndef __EMSCRIPTEN__
// Joint, a la sortie de portee, tout thread encore joignable. Si le lancement
// d'un thread leve (emplace_back), ceux deja partis sont ainsi attendus avant
// que l'exception ne remonte : un std::thread joignable detruit appellerait
// std::terminate, et les threads lisent des donnees de la pile appelante.
struct JoinAll
{
	std::vector<std::thread>& threads;
	~JoinAll ()
	{
		for (std::thread& t : threads)
			if (t.joinable ()) t.join ();
	}
};
#endif

// Execute work sur [0, n), decoupe en plages contigues d'au plus
// ceil (n / threads) elements. Le nombre de threads vaut
// `requested` (0 = hardware_concurrency, kFallbackThreads si inconnu), borne
// par n et par kMaxSliceThreads. Sous __EMSCRIPTEN__ : toujours sequentiel.
// La premiere exception d'un thread est relancee apres la jointure de tous.
void runBatches (size_t n, unsigned int requested,
                 const std::function<void (size_t, size_t)>& work)
{
	if (n == 0) return;
#ifdef __EMSCRIPTEN__
	(void)requested;
	work (0, n);
#else
	size_t nThreads = requested;
	if (nThreads == 0)
	{
		nThreads = std::thread::hardware_concurrency ();
		if (nThreads == 0) nThreads = kFallbackThreads;
	}
	nThreads = std::min (nThreads, std::min (n, (size_t)kMaxSliceThreads));

	const size_t batch = (n + nThreads - 1) / nThreads;
	const size_t nBatches = (n + batch - 1) / batch;
	if (nBatches <= 1)
	{
		work (0, n);
		return;
	}

	std::vector<std::exception_ptr> errors (nBatches);
	{
		std::vector<std::thread> threads;
		threads.reserve (nBatches);
		const JoinAll guard { threads };
		for (size_t b = 0; b < nBatches; ++b)
		{
			const size_t start = b * batch;
			const size_t end = std::min (n, start + batch);
			threads.emplace_back ([&work, &errors, b, start, end] () {
				try { work (start, end); }
				catch (...) { errors[b] = std::current_exception (); }
			});
		}
	}   // guard : jointure de tous les threads lances
	for (const std::exception_ptr& e : errors)
		if (e) std::rethrow_exception (e);
#endif
}
} // namespace

std::vector<SliceLayer> sliceMeshes (const std::vector<SliceInput>& inputs,
                                     const std::vector<float>& zs,
                                     const SliceOptions& options,
                                     const SliceProgress& progress)
{
	// Aucun plan : rien a faire (et pas de division par zero plus bas).
	if (zs.empty ())
		return {};

	const size_t nz = zs.size ();
	const size_t nm = inputs.size ();

	// Une altitude non finie rend une couche vide et reste HORS du tri et du
	// balayage : un NaN casserait l'ordre strict du tri, et le balayage,
	// monotone en z, en corromprait les autres couches.
	size_t nFinite = 0;
	for (const float z : zs)
		if (std::isfinite (z)) nFinite++;

	// Vues plateau construites SEQUENTIELLEMENT, puis lues en parallele sans
	// verrou.
	std::vector<PlateMesh> mains (nm), hollows (nm);
	for (size_t m = 0; m < nm; ++m)
	{
		if (!inputs[m].mesh) continue;
		mains[m] = buildPlateMesh (*inputs[m].mesh, inputs[m].matrix);
		if (inputs[m].hollowing)
			hollows[m] = buildPlateMesh (*inputs[m].hollowing, inputs[m].matrix);
	}

	// Une case par (entree, plan) : chaque case n'est ecrite que par le thread
	// qui possede le plan, et l'assemblage final suit l'ordre des entrees.
	std::vector<std::vector<SliceLayer>> perInput (nm, std::vector<SliceLayer> (nz));

	const size_t total = nFinite * nm;
	const size_t step = std::max<size_t> (total / 100, 1);
	std::atomic<size_t> done (0);
	std::mutex progressMutex;
	int lastPercent = -1;
	const auto tick = [&] () {
		const size_t d = ++done;
		if (!progress || (d % step != 0 && d != total)) return;
		const std::lock_guard<std::mutex> lock (progressMutex);
		const int percent = (int)(d * 100 / total);
		if (percent > lastPercent)
		{
			lastPercent = percent;
			progress (percent);
		}
	};

	// Traite les plans [start, end) : entree par entree, et par z croissant
	// pour le balayage.
	const auto work = [&] (size_t start, size_t end) {
		std::vector<size_t> order;
		order.reserve (end - start);
		for (size_t k = start; k < end; ++k)
			if (std::isfinite (zs[k])) order.push_back (k);
		std::stable_sort (order.begin (), order.end (),
		                  [&] (size_t l, size_t r) { return zs[l] < zs[r]; });

		for (size_t m = 0; m < nm; ++m)
		{
			const SliceInput& in = inputs[m];
			if (!in.mesh) continue;
			ZSweep sweepMain (mains[m]);
			ZSweep sweepHollow (hollows[m]);
			std::vector<int> candidates;
			for (const size_t k : order)
			{
				const float z = zs[k];
				std::vector<SliceLoop> loops;
				sweepMain.advance ((double)z, candidates);
				loopsFromCandidates (mains[m], z, candidates, loops, false);
				if (in.hollowing)
				{
					sweepHollow.advance ((double)z, candidates);
					loopsFromCandidates (hollows[m], z, candidates, loops, true);
				}
				perInput[m][k] = buildRegions (loops, (int)m, in.isSupport, in.isHole,
				                               options.nesting);
				tick ();
			}
		}
	};
	runBatches (nz, options.threads, work);

	std::vector<SliceLayer> out (nz);
	for (size_t k = 0; k < nz; ++k)
		for (size_t m = 0; m < nm; ++m)
			for (SliceRegion& r : perInput[m][k])
				out[k].push_back (std::move (r));

	if (progress && lastPercent < 100)
		progress (100);
	return out;
}

} // namespace slicing
} // namespace cgmesh
