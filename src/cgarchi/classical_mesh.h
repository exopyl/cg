#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "mesh.h"

namespace cgarchi
{
namespace classical
{

// Maillage de triangles depuis des positions en double (stockees en float).
inline std::unique_ptr<Mesh> MakeTriangleMesh (const std::vector<std::array<double, 3>> &positions,
                                               const std::vector<std::array<unsigned, 3>> &triangles)
{
	auto mesh = std::make_unique<Mesh> (static_cast<unsigned> (positions.size ()), static_cast<unsigned> (triangles.size ()));
	for (std::size_t v = 0; v < positions.size (); ++v)
		mesh->SetVertex (static_cast<unsigned> (v), static_cast<float> (positions[v][0]), static_cast<float> (positions[v][1]),
		                 static_cast<float> (positions[v][2]));
	for (std::size_t f = 0; f < triangles.size (); ++f)
		mesh->FaceAt (static_cast<unsigned> (f))->SetTriangle (triangles[f][0], triangles[f][1], triangles[f][2]);
	return mesh;
}

// Abscisses d'un intervalle a stations : lo, les stations croissantes
// strictement dans ]lo, hi[, hi.
inline std::vector<double> SpanWithStations (double lo, double hi, const std::vector<double> &stations)
{
	std::vector<double> span;
	span.reserve (stations.size () + 2);
	span.push_back (lo);
	for (const double s : stations)
		if (lo < s && s < hi)
			span.push_back (s);
	span.push_back (hi);
	return span;
}

// Bande plane entre deux chaines de sommets croissantes en abscisse, de memes
// extremites en abscisse, `low` sous `high` : chaque triangle avance d'un
// sommet sur la chaine dont le suivant a la plus petite abscisse (`low` a
// egalite). Sens : (low[i], low[i + 1], high[j]) et (low[i], high[j + 1],
// high[j]), direct dans le plan (abscisse, hauteur) ; `reversed` retourne tous
// les triangles. Decisions par comparaisons d'abscisses seulement.
inline void AppendZipper (const std::vector<unsigned> &low, const std::vector<double> &lowAt,
                          const std::vector<unsigned> &high, const std::vector<double> &highAt, bool reversed,
                          std::vector<std::array<unsigned, 3>> &triangles)
{
	std::size_t i = 0, j = 0;
	while (i + 1 < low.size () || j + 1 < high.size ())
	{
		const bool advanceLow = j + 1 == high.size () || (i + 1 < low.size () && lowAt[i + 1] <= highAt[j + 1]);
		std::array<unsigned, 3> t = advanceLow ? std::array<unsigned, 3> {low[i], low[i + 1], high[j]}
		                                       : std::array<unsigned, 3> {low[i], high[j + 1], high[j]};
		if (reversed)
			std::swap (t[1], t[2]);
		triangles.push_back (t);
		if (advanceLow)
			++i;
		else
			++j;
	}
}

} // namespace classical
} // namespace cgarchi
