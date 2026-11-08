#pragma once

// En-tete interne de l'architecture classique grecque, hors de l'API publique :
// predicats stricts de polygone, aire, decoupe d'oreilles et correspondance
// (a, d) -> (x, y) d'un cote, partages par l'entablement et le fronton (D3-95).

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_peristyle.h>

namespace cgarchi
{
namespace classical
{
namespace detail
{

// p q - r s en evaluation stricte : chaque produit est arrondi en double avant
// la difference (lecture volatile), jamais contracte en FMA, quels que soient
// le compilateur et ses drapeaux. Les tests d'alignement exact (== 0) des
// orientations donnent donc le meme verdict sur toute cible : les flancs de
// canaux a 45 degres des deux bras d'une piece d'angle sont alignes et
// disjoints, et une orientation contractee y laisse un residu de signe
// arbitraire, lu comme une traversee.
inline double StrictCrossDifference (double p, double q, double r, double s)
{
	const volatile double left = p * q;
	const volatile double right = r * s;
	return left - right;
}

// Orientation de (a, b, c) : > 0 si le tour est direct.
inline double Orient (const RingSectionPoint &a, const RingSectionPoint &b, const RingSectionPoint &c)
{
	return StrictCrossDifference (b.u - a.u, c.z - a.z, b.z - a.z, c.u - a.u);
}

// c, colineaire a [a, b], est dans la boite du segment.
inline bool WithinBox (const RingSectionPoint &a, const RingSectionPoint &b, const RingSectionPoint &c)
{
	return std::min (a.u, b.u) <= c.u && c.u <= std::max (a.u, b.u) && std::min (a.z, b.z) <= c.z &&
	       c.z <= std::max (a.z, b.z);
}

// Deux segments fermes se touchent ou se coupent.
inline bool SegmentsMeet (const RingSectionPoint &p1, const RingSectionPoint &p2, const RingSectionPoint &p3,
                          const RingSectionPoint &p4)
{
	const double d1 = Orient (p3, p4, p1), d2 = Orient (p3, p4, p2), d3 = Orient (p1, p2, p3), d4 = Orient (p1, p2, p4);
	if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0)))
		return true;
	return (d1 == 0.0 && WithinBox (p3, p4, p1)) || (d2 == 0.0 && WithinBox (p3, p4, p2)) ||
	       (d3 == 0.0 && WithinBox (p1, p2, p3)) || (d4 == 0.0 && WithinBox (p1, p2, p4));
}

// Polygone simple : aretes non voisines disjointes, aretes voisines sans
// repli (deux aretes colineaires de sens opposes se recouvrent).
inline bool IsSimple (const std::vector<RingSectionPoint> &s)
{
	const std::size_t n = s.size ();
	for (std::size_t i = 0; i < n; ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % n], &c = s[(i + 2) % n];
		const double du0 = b.u - a.u, dz0 = b.z - a.z, du1 = c.u - b.u, dz1 = c.z - b.z;
		if (StrictCrossDifference (du0, dz1, dz0, du1) == 0.0 && du0 * du1 + dz0 * dz1 < 0.0)
			return false;
		for (std::size_t j = i + 2; j < n; ++j)
		{
			if ((j + 1) % n == i)
				continue;
			if (SegmentsMeet (a, b, s[j], s[(j + 1) % n]))
				return false;
		}
	}
	return true;
}

// Deux fois l'aire signee (formule du lacet).
inline double TwiceSignedArea (const std::vector<RingSectionPoint> &s)
{
	double sum = 0.0;
	for (std::size_t i = 0; i < s.size (); ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % s.size ()];
		sum += a.u * b.z - b.u * a.z;
	}
	return sum;
}

// (a, d) -> (x, y) d'un cote, par les expressions de l'anneau (SweepRectangularRing).
inline std::array<double, 2> PlanPoint (PeristyleSide side, double hx, double hy, double a, double d)
{
	switch (side)
	{
	case PeristyleSide::Front: return {a, -(hy + d)};
	case PeristyleSide::Right: return {hx + d, a};
	case PeristyleSide::Back:  return {a, hy + d};
	case PeristyleSide::Left:  return {-(hx + d), a};
	}
	return {};
}

inline double Cross (const std::array<double, 2> &a, const std::array<double, 2> &b, const std::array<double, 2> &c)
{
	return StrictCrossDifference (b[0] - a[0], c[1] - a[1], b[1] - a[1], c[0] - a[0]);
}

// Decoupe d'oreilles d'un polygone simple de sens direct, en double : P - 2
// triangles de sens direct (indices des sommets). Une oreille est un sommet
// strictement convexe dont le triangle ne contient, bord compris, aucun autre
// sommet restant. Vide si une etape ne trouve aucune oreille.
inline std::vector<std::array<unsigned, 3>> EarClip (const std::vector<std::array<double, 2>> &polygon)
{
	std::vector<unsigned> left;
	left.reserve (polygon.size ());
	for (std::size_t i = 0; i < polygon.size (); ++i)
		left.push_back (static_cast<unsigned> (i));
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (polygon.size ());
	while (left.size () > 3)
	{
		const std::size_t n = left.size ();
		bool clipped = false;
		for (std::size_t i = 0; i < n && !clipped; ++i)
		{
			const unsigned a = left[(i + n - 1) % n], b = left[i], c = left[(i + 1) % n];
			if (!(Cross (polygon[a], polygon[b], polygon[c]) > 0.0))
				continue;
			bool empty = true;
			for (std::size_t k = 0; k < n && empty; ++k)
			{
				const unsigned v = left[k];
				if (v == a || v == b || v == c)
					continue;
				empty = !(Cross (polygon[a], polygon[b], polygon[v]) >= 0.0 && Cross (polygon[b], polygon[c], polygon[v]) >= 0.0 &&
				          Cross (polygon[c], polygon[a], polygon[v]) >= 0.0);
			}
			if (!empty)
				continue;
			triangles.push_back ({a, b, c});
			left.erase (left.begin () + static_cast<std::ptrdiff_t> (i));
			clipped = true;
		}
		if (!clipped)
			return {};
	}
	triangles.push_back ({left[0], left[1], left[2]});
	return triangles;
}

} // namespace detail
} // namespace classical
} // namespace cgarchi
