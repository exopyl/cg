#pragma once

#include <array>
#include <memory>
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

} // namespace classical
} // namespace cgarchi
