#include "mesh_crease_normals.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "mesh.h"

namespace
{

constexpr double kPi = 3.14159265358979323846;

// Une face est degeneree quand la norme de son vecteur de Newell (deux fois
// son aire) est negligeable devant le carre de sa plus longue arete : pour un
// triangle, c'est le rapport de sa hauteur a sa base. Le seuil est ainsi
// independant de l'unite des coordonnees, et ecarte les aiguilles et les
// « bonnets » dont la normale n'est que du bruit d'arrondi.
constexpr double kDegenerateRatio = 1e-6;

constexpr unsigned int kNone = 0xFFFFFFFFu;

struct Vec3d
{
	double x = 0.0, y = 0.0, z = 0.0;
};

Vec3d Position (const std::vector<float> &p, unsigned int v)
{
	return { p[3 * (size_t)v], p[3 * (size_t)v + 1], p[3 * (size_t)v + 2] };
}

// Angle INTERIEUR du coin `cur` entre les aretes vers `prev` et vers `next`,
// en radians, oriente par la normale de la face `n` : au-dela de pi au sommet
// rentrant d'un polygone concave. Nul si une des aretes est de longueur nulle.
double CornerAngle (const Vec3d &prev, const Vec3d &cur, const Vec3d &next, const Vec3d &n)
{
	const double ax = prev.x - cur.x, ay = prev.y - cur.y, az = prev.z - cur.z;
	const double bx = next.x - cur.x, by = next.y - cur.y, bz = next.z - cur.z;
	const double cx = ay * bz - az * by;
	const double cy = az * bx - ax * bz;
	const double cz = ax * by - ay * bx;
	const double cross = std::sqrt (cx * cx + cy * cy + cz * cz);
	const double dot = ax * bx + ay * by + az * bz;
	if (cross == 0.0 && dot == 0.0)
		return 0.0;
	const double angle = std::atan2 (cross, dot);
	// Coin saillant d'un polygone oriente par n : (prev - cur) x (next - cur)
	// est oppose a n. Meme sens que n : coin rentrant.
	return (cx * n.x + cy * n.y + cz * n.z > 0.0) ? 2.0 * kPi - angle : angle;
}

// Union-find sur les coins d'UN sommet (quelques elements).
unsigned int Find (std::vector<unsigned int> &parent, unsigned int i)
{
	while (parent[i] != i)
	{
		parent[i] = parent[parent[i]];
		i = parent[i];
	}
	return i;
}

} // namespace

CreaseNormals ComputeCreaseNormals (const Mesh &mesh, float creaseAngleDeg)
{
	CreaseNormals out;

	const unsigned int nf = mesh.GetNFaces ();
	const unsigned int nv = mesh.GetNVertices ();
	const std::vector<float> &P = mesh.GetVertices ();

	// --- coins ----------------------------------------------------------------
	out.cornerOffsets.assign ((size_t)nf + 1, 0u);
	std::vector<unsigned int> cornerVertex;
	cornerVertex.reserve (3 * (size_t)nf);
	std::vector<char> faceValid (nf, 0);
	bool uniformTriangles = true;

	for (unsigned int f = 0; f < nf; ++f)
	{
		out.cornerOffsets[f] = (unsigned int)cornerVertex.size ();
		auto face = mesh.FaceAt (f);
		const int n = face ? face->GetNVertices () : 0;
		uniformTriangles = uniformTriangles && n == 3;
		bool inRange = n >= 3;
		for (int i = 0; i < n; ++i)
		{
			const int v = face->GetVertex ((unsigned int)i);
			if (v < 0 || (unsigned int)v >= nv)
				inRange = false;
			cornerVertex.push_back ((unsigned int)v);
		}
		faceValid[f] = inRange ? 1 : 0;
	}
	const size_t nc = cornerVertex.size ();
	out.cornerOffsets[nf] = (unsigned int)nc;

	// Face d'un coin : derivee des offsets, sans tableau par coin. Les faces
	// vides ont un offset egal a celui de la suivante : upper_bound rend la
	// derniere face qui commence a ou avant c, qui est celle qui le contient.
	const std::vector<unsigned int> &offsets = out.cornerOffsets;
	auto faceOf = [&] (size_t c) -> unsigned int {
		if (uniformTriangles)
			return (unsigned int)(c / 3);
		return (unsigned int)(std::upper_bound (offsets.begin (), offsets.end (), (unsigned int)c) - offsets.begin ()) - 1u;
	};

	// --- normales de face (Newell) et angles de coin ------------------------
	std::vector<float> faceNormal (3 * (size_t)nf, 0.0f);
	std::vector<float> cornerAngle (nc, 0.0f);
	for (unsigned int f = 0; f < nf; ++f)
	{
		if (!faceValid[f])
			continue;
		const unsigned int c0 = offsets[f];
		const unsigned int n = offsets[f + 1] - c0;
		Vec3d nn;
		double maxEdge2 = 0.0;
		for (unsigned int i = 0; i < n; ++i)
		{
			const Vec3d a = Position (P, cornerVertex[c0 + i]);
			const Vec3d b = Position (P, cornerVertex[c0 + (i + 1) % n]);
			nn.x += (a.y - b.y) * (a.z + b.z);
			nn.y += (a.z - b.z) * (a.x + b.x);
			nn.z += (a.x - b.x) * (a.y + b.y);
			const double ex = b.x - a.x, ey = b.y - a.y, ez = b.z - a.z;
			maxEdge2 = std::max (maxEdge2, ex * ex + ey * ey + ez * ez);
		}
		const double len = std::sqrt (nn.x * nn.x + nn.y * nn.y + nn.z * nn.z);
		if (!(len > kDegenerateRatio * maxEdge2) || !std::isfinite (len))
		{
			faceValid[f] = 0;
			continue;
		}
		const Vec3d unit { nn.x / len, nn.y / len, nn.z / len };
		faceNormal[3 * (size_t)f]     = (float)unit.x;
		faceNormal[3 * (size_t)f + 1] = (float)unit.y;
		faceNormal[3 * (size_t)f + 2] = (float)unit.z;
		for (unsigned int i = 0; i < n; ++i)
		{
			const Vec3d prev = Position (P, cornerVertex[c0 + (i + n - 1) % n]);
			const Vec3d cur  = Position (P, cornerVertex[c0 + i]);
			const Vec3d next = Position (P, cornerVertex[c0 + (i + 1) % n]);
			cornerAngle[c0 + i] = (float)CornerAngle (prev, cur, next, unit);
		}
	}

	// --- incidence sommet -> coins (tri par comptage) -----------------------
	std::vector<unsigned int> incBegin ((size_t)nv + 1, 0u);
	for (size_t c = 0; c < nc; ++c)
		if (cornerVertex[c] < nv)
			++incBegin[(size_t)cornerVertex[c] + 1];
	for (unsigned int v = 0; v < nv; ++v)
		incBegin[(size_t)v + 1] += incBegin[v];
	std::vector<unsigned int> incCorners (incBegin[nv]);
	{
		std::vector<unsigned int> fill (incBegin.begin (), incBegin.end () - 1);
		for (size_t c = 0; c < nc; ++c)
			if (cornerVertex[c] < nv)
				incCorners[fill[cornerVertex[c]]++] = (unsigned int)c;
	}

	// --- groupes par sommet, puis une normale par groupe ---------------------
	const double angle = std::min (std::max ((double)creaseAngleDeg, 0.0), 180.0);
	const float cosThreshold = (float)std::cos (angle * kPi / 180.0);

	out.cornerNormals.assign (3 * nc, 0.0f);
	auto writeNormal = [&] (unsigned int c, const Vec3d &r) {
		out.cornerNormals[3 * (size_t)c]     = (float)r.x;
		out.cornerNormals[3 * (size_t)c + 1] = (float)r.y;
		out.cornerNormals[3 * (size_t)c + 2] = (float)r.z;
	};
	auto normalized = [] (const Vec3d &s, bool &ok) -> Vec3d {
		const double len = std::sqrt (s.x * s.x + s.y * s.y + s.z * s.z);
		ok = len > 0.0 && std::isfinite (len);
		return ok ? Vec3d { s.x / len, s.y / len, s.z / len } : Vec3d {};
	};

	// Tampons par sommet, reutilises.
	std::vector<unsigned int> face, parent;
	std::vector<std::pair<unsigned int, unsigned int>> neighbours;   // (sommet voisin, coin local)
	std::vector<Vec3d> sums;

	for (unsigned int v = 0; v < nv; ++v)
	{
		const unsigned int b = incBegin[v];
		const unsigned int d = incBegin[(size_t)v + 1] - b;
		if (d == 0)
			continue;

		face.resize (d);
		parent.resize (d);
		neighbours.clear ();
		for (unsigned int k = 0; k < d; ++k)
		{
			const unsigned int c = incCorners[b + k];
			const unsigned int f = faceOf (c);
			face[k] = f;
			parent[k] = k;
			if (!faceValid[f])
				continue;
			const unsigned int c0 = offsets[f];
			const unsigned int n = offsets[f + 1] - c0;
			const unsigned int i = c - c0;
			neighbours.emplace_back (cornerVertex[c0 + (i + n - 1) % n], k);
			neighbours.emplace_back (cornerVertex[c0 + (i + 1) % n], k);
		}

		// Deux coins voisins du meme sommet w partagent l'arete (v, w).
		std::sort (neighbours.begin (), neighbours.end ());
		for (size_t s = 0; s < neighbours.size (); )
		{
			size_t e = s + 1;
			while (e < neighbours.size () && neighbours[e].first == neighbours[s].first)
				++e;
			for (size_t p = s; p < e; ++p)
				for (size_t q = p + 1; q < e; ++q)
				{
					const unsigned int kp = neighbours[p].second, kq = neighbours[q].second;
					if (kp == kq)
						continue;
					const float *np = &faceNormal[3 * (size_t)face[kp]];
					const float *nq = &faceNormal[3 * (size_t)face[kq]];
					if (np[0] * nq[0] + np[1] * nq[1] + np[2] * nq[2] > cosThreshold)
					{
						const unsigned int rp = Find (parent, kp), rq = Find (parent, kq);
						if (rp != rq)
							parent[std::max (rp, rq)] = std::min (rp, rq);
					}
				}
			s = e;
		}

		// Sommes par groupe, et somme de toutes les faces valides (repli des
		// coins de faces degenerees).
		sums.assign (d, Vec3d {});
		Vec3d all;
		for (unsigned int k = 0; k < d; ++k)
		{
			if (!faceValid[face[k]])
				continue;
			const float *nf3 = &faceNormal[3 * (size_t)face[k]];
			const double w = cornerAngle[incCorners[b + k]];
			Vec3d &s = sums[Find (parent, k)];
			s.x += w * nf3[0];
			s.y += w * nf3[1];
			s.z += w * nf3[2];
			all.x += w * nf3[0];
			all.y += w * nf3[1];
			all.z += w * nf3[2];
		}

		bool allOk = false;
		const Vec3d allN = normalized (all, allOk);
		for (unsigned int k = 0; k < d; ++k)
		{
			const unsigned int c = incCorners[b + k];
			const unsigned int f = face[k];
			bool ok = false;
			Vec3d r;
			if (faceValid[f])
			{
				r = normalized (sums[Find (parent, k)], ok);
				if (!ok)
					r = { faceNormal[3 * (size_t)f], faceNormal[3 * (size_t)f + 1], faceNormal[3 * (size_t)f + 2] };
			}
			else
				r = allOk ? allN : Vec3d { 0.0, 0.0, 1.0 };
			writeNormal (c, r);
		}
	}

	// Coins dont le sommet sort des bornes : aucune incidence, donc pas encore
	// ecrits.
	for (size_t c = 0; c < nc; ++c)
		if (cornerVertex[c] >= nv)
			out.cornerNormals[3 * c + 2] = 1.0f;

	return out;
}

Mesh BuildCreaseSplitMesh (const Mesh &mesh, float creaseAngleDeg)
{
	Mesh out (mesh);
	out.Triangulate ();
	const CreaseNormals cn = ComputeCreaseNormals (out, creaseAngleDeg);

	// SplitVerticesByUVSeams ne change ni l'ordre des faces ni celui de leurs
	// coins : les normales calculees avant restent donc indexees juste.
	out.SplitVerticesByUVSeams ();
	out.SplitVerticesByCornerNormals (cn.cornerNormals);
	return out;
}
