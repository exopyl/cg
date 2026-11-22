#pragma once

// Outils de test des feuilles d'acanthe partages par les tests de la feuille
// (J21') et de l'ornement (K7, K36, K40) : rayon par noeud d'une grille (theta, h),
// jour entre deux pieces, empreinte d'un prefixe du maillage.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <cgarchi/classical_acanthus_detail.h>
#include "../src/cgmesh/mesh.h"

namespace acanthus_test
{

// Grille (theta deroule autour d'un angle de reference, en degres ; h depuis le pied du chapiteau, en parties) :
// theta dans [-60, 60] au pas de 0,25, h dans [0, 32] au pas de 0,05. NaN : noeud non couvert.
struct RadiusGrid
{
	static constexpr double kTheta0 = -60.0, kDTheta = 0.25, kDH = 0.05;
	static constexpr int kNTheta = 481, kNH = 641;
	std::vector<double> r = std::vector<double> (static_cast<std::size_t> (kNTheta) * kNH, std::nan (""));
	double &At (int i, int j) { return r[static_cast<std::size_t> (j) * kNTheta + static_cast<std::size_t> (i)]; }
};

// Rayon (parties) des faces [faceBegin, faceEnd) de `mesh` aux noeuds de `grid` qu'elles couvrent, interpole dans
// la projection (theta, h) de chaque triangle ; theta mesure depuis thetaRef (rad), ramene a ]-pi, pi]. Par noeud,
// le plus grand rayon si keepMax, le plus petit sinon.
inline void Rasterize (RadiusGrid &grid, const Mesh &mesh, const cgarchi::classical::detail::CapitalEnvelope &env,
                       unsigned faceBegin, unsigned faceEnd, bool keepMax, double thetaRef = 0.0)
{
	constexpr double kPi = 3.14159265358979323846;
	const double s = env.scale;
	for (unsigned k = faceBegin; k < faceEnd; ++k)
	{
		std::array<double, 3> x {}, y {}, r {};
		for (int e = 0; e < 3; ++e)
		{
			float p[3] {};
			mesh.GetVertex (static_cast<unsigned> (mesh.GetFaceVertex (k, e)), p);
			const double theta = std::remainder (std::atan2 (static_cast<double> (p[1]), static_cast<double> (p[0])) - thetaRef, 2.0 * kPi);
			x[static_cast<std::size_t> (e)] = (theta * 180.0 / kPi - RadiusGrid::kTheta0) / RadiusGrid::kDTheta;
			y[static_cast<std::size_t> (e)] = (p[2] / s - env.foot) / RadiusGrid::kDH;
			r[static_cast<std::size_t> (e)] = std::hypot (static_cast<double> (p[0]), static_cast<double> (p[1])) / s;
		}
		const double det = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
		if (std::fabs (det) < 1e-12)
			continue;
		const int i0 = std::max (0, static_cast<int> (std::ceil (std::min ({x[0], x[1], x[2]}))));
		const int i1 = std::min (RadiusGrid::kNTheta - 1, static_cast<int> (std::floor (std::max ({x[0], x[1], x[2]}))));
		const int j0 = std::max (0, static_cast<int> (std::ceil (std::min ({y[0], y[1], y[2]}))));
		const int j1 = std::min (RadiusGrid::kNH - 1, static_cast<int> (std::floor (std::max ({y[0], y[1], y[2]}))));
		for (int j = j0; j <= j1; ++j)
			for (int i = i0; i <= i1; ++i)
			{
				const double w1 = ((i - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (j - y[0])) / det;
				const double w2 = ((x[1] - x[0]) * (j - y[0]) - (i - x[0]) * (y[1] - y[0])) / det;
				const double w0 = 1.0 - w1 - w2;
				if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0)
					continue;
				const double rv = w0 * r[0] + w1 * r[1] + w2 * r[2];
				double &cell = grid.At (i, j);
				if (std::isnan (cell) || (keepMax ? rv > cell : rv < cell))
					cell = rv;
			}
	}
}

// Distance du point p au triangle (a, b, c) : point le plus proche par les regions de Voronoi du triangle.
inline double PointTriangleDistance (const std::array<double, 3> &p, const std::array<double, 3> &a, const std::array<double, 3> &b,
                                     const std::array<double, 3> &c)
{
	using P = std::array<double, 3>;
	const auto sub = [] (const P &u, const P &v) { return P {u[0] - v[0], u[1] - v[1], u[2] - v[2]}; };
	const auto dot = [] (const P &u, const P &v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
	const auto at = [] (const P &o, const P &u, double s, const P &v, double t) {
		return P {o[0] + s * u[0] + t * v[0], o[1] + s * u[1] + t * v[1], o[2] + s * u[2] + t * v[2]};
	};
	const P ab = sub (b, a), ac = sub (c, a), ap = sub (p, a);
	const double d1 = dot (ab, ap), d2 = dot (ac, ap);
	P q;
	if (d1 <= 0.0 && d2 <= 0.0)
		q = a;
	else
	{
		const P bp = sub (p, b), cp = sub (p, c);
		const double d3 = dot (ab, bp), d4 = dot (ac, bp), d5 = dot (ab, cp), d6 = dot (ac, cp);
		const double vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
		if (d3 >= 0.0 && d4 <= d3)
			q = b;
		else if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
			q = at (a, ab, d1 / (d1 - d3), ac, 0.0);
		else if (d6 >= 0.0 && d5 <= d6)
			q = c;
		else if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
			q = at (a, ab, 0.0, ac, d2 / (d2 - d6));
		else if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
		{
			const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			q = P {b[0] + w * (c[0] - b[0]), b[1] + w * (c[1] - b[1]), b[2] + w * (c[2] - b[2])};
		}
		else
		{
			const double den = 1.0 / (va + vb + vc);
			q = at (a, ab, vb * den, ac, vc * den);
		}
	}
	return std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]);
}

// Jour entre deux pieces de l'ornement (parties) : plus petite distance d'un sommet de l'une a un triangle de
// l'autre, dans les deux sens, pour les sommets a moins de `near` p de la boite de l'autre piece ; `near` si aucun
// sommet n'en est aussi proche. Le croisement de deux aretes entre leurs sommets n'est pas vu (celui de deux faces,
// si : un sommet traverse alors un triangle). `where`, s'il est donne : le sommet le plus proche (parties).
inline double PieceGap (const Mesh &mesh, const cgarchi::classical::detail::CapitalEnvelope &env, const cgarchi::classical::CapitalPiece &a,
                        const cgarchi::classical::CapitalPiece &b, double near, std::array<double, 3> *where = nullptr)
{
	using P = std::array<double, 3>;
	const auto parts = [&] (unsigned v) {
		float q[3] {};
		mesh.GetVertex (v, q);
		return P {q[0] / env.scale, q[1] / env.scale, q[2] / env.scale - env.foot};
	};
	double best = near;
	for (int pass = 0; pass < 2; ++pass)
	{
		const cgarchi::classical::CapitalPiece &from = pass == 0 ? a : b, &to = pass == 0 ? b : a;
		struct Tri
		{
			P a, b, c, lo, hi;
		};
		std::vector<Tri> tris;
		P lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
		for (unsigned f = to.faceBegin; f < to.faceEnd; ++f)
		{
			Tri t {parts (static_cast<unsigned> (mesh.GetFaceVertex (f, 0))), parts (static_cast<unsigned> (mesh.GetFaceVertex (f, 1))),
			       parts (static_cast<unsigned> (mesh.GetFaceVertex (f, 2))), {}, {}};
			for (int k = 0; k < 3; ++k)
			{
				t.lo[k] = std::min ({t.a[k], t.b[k], t.c[k]});
				t.hi[k] = std::max ({t.a[k], t.b[k], t.c[k]});
				lo[k] = std::min (lo[k], t.lo[k]);
				hi[k] = std::max (hi[k], t.hi[k]);
			}
			tris.push_back (t);
		}
		for (unsigned v = from.vertexBegin; v < from.vertexEnd; ++v)
		{
			const P p = parts (v);
			if (p[0] < lo[0] - near || p[0] > hi[0] + near || p[1] < lo[1] - near || p[1] > hi[1] + near || p[2] < lo[2] - near ||
			    p[2] > hi[2] + near)
				continue;
			for (const Tri &t : tris)
			{
				if (p[0] < t.lo[0] - best || p[0] > t.hi[0] + best || p[1] < t.lo[1] - best || p[1] > t.hi[1] + best ||
				    p[2] < t.lo[2] - best || p[2] > t.hi[2] + best)
					continue;
				const double d = PointTriangleDistance (p, t.a, t.b, t.c);
				if (d < best)
				{
					best = d;
					if (where)
						*where = p;
				}
			}
		}
	}
	return best;
}

// Empreinte des `vertices` premiers sommets et `faces` premieres faces d'un maillage. `topology` : FNV-1a des
// comptes, puis de l'arite et des indices de chaque face (entiers, identique sur toute plateforme) ; `bits` : FNV-1a
// des comptes et des sommets float, a l'octet (une autre bibliotheque mathematique ou la contraction en FMA d'un autre
// compilateur peut changer un dernier bit : a comparer a une valeur relevee avec le meme compilateur) ; `weighted` :
// somme de i |v_i|^2 et boite, a comparer a une tolerance.
struct PrefixPrint
{
	std::uint64_t topology = 0, bits = 0;
	double weighted = 0.0;
	std::array<double, 3> lo {}, hi {};
};

inline PrefixPrint MeasurePrefix (Mesh &mesh, unsigned vertices, unsigned faces)
{
	PrefixPrint out;
	const auto fnv = [] (std::uint64_t &h, const void *data, std::size_t n) {
		const unsigned char *b = static_cast<const unsigned char *> (data);
		for (std::size_t i = 0; i < n; ++i)
			h = (h ^ b[i]) * 1099511628211ULL;
	};
	out.topology = out.bits = 1469598103934665603ULL;
	for (std::uint64_t *h : {&out.topology, &out.bits})
	{
		fnv (*h, &vertices, sizeof vertices);
		fnv (*h, &faces, sizeof faces);
	}
	out.lo = {1e300, 1e300, 1e300};
	out.hi = {-1e300, -1e300, -1e300};
	for (unsigned v = 0; v < vertices; ++v)
	{
		float p[3];
		mesh.GetVertex (v, p);
		fnv (out.bits, p, sizeof p);
		out.weighted += static_cast<double> (v) * (static_cast<double> (p[0]) * p[0] + static_cast<double> (p[1]) * p[1] +
		                                           static_cast<double> (p[2]) * p[2]);
		for (int k = 0; k < 3; ++k)
		{
			out.lo[static_cast<std::size_t> (k)] = std::min (out.lo[static_cast<std::size_t> (k)], static_cast<double> (p[k]));
			out.hi[static_cast<std::size_t> (k)] = std::max (out.hi[static_cast<std::size_t> (k)], static_cast<double> (p[k]));
		}
	}
	for (unsigned f = 0; f < faces; ++f)
	{
		const unsigned n = static_cast<unsigned> (mesh.GetFaceNVertices (f));
		fnv (out.bits, &n, sizeof n);
		fnv (out.topology, &n, sizeof n);
		for (unsigned j = 0; j < n; ++j)
		{
			const int i = mesh.GetFaceVertex (f, j);
			fnv (out.bits, &i, sizeof i);
			fnv (out.topology, &i, sizeof i);
		}
	}
	return out;
}

} // namespace acanthus_test
