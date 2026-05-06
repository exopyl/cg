#pragma once
#include "mesh_half_edge.h"

//
// Loop subdivision : 1-to-4 triangle split.
//
// Two flavors :
//   - Polyhedral (default, m_useWarrenMask = false) : new midpoints are at the
//     edge midpoints, original vertices keep their position. Pure refinement,
//     no smoothing — preserves all sharp corners exactly. Surface area
//     unchanged ; vertex/face count grow as expected for a 1->4 split.
//
//   - Loop / Warren (m_useWarrenMask = true) : the proper Loop subdivision
//     using Warren's stencils. Original vertices and new midpoints are
//     repositioned to converge to a C^2 limit surface (C^1 at extraordinary
//     vertices). Boundary edges and boundary vertices use the Loop boundary
//     stencils (cubic B-spline along the boundary curve).
//
// Stencils (Warren) :
//   New edge midpoint, interior edge V0-V1 with opposite vertices V2, V3 :
//     M = (3/8)(V0 + V1) + (1/8)(V2 + V3)
//   New edge midpoint, boundary edge V0-V1 :
//     M = (1/2)(V0 + V1)
//   Original vertex V, interior, valence n, neighbors N_1...N_n :
//     V' = (1 - n*beta) V + beta * sum(N_i)
//     beta = 3/16 if n = 3, else 3/(8n)
//   Original vertex V, boundary, with two boundary neighbors V_L, V_R :
//     V' = (3/4) V + (1/8) (V_L + V_R)
//
class MeshAlgoSubdivisionLoop
{
public:
	MeshAlgoSubdivisionLoop () {};
	~MeshAlgoSubdivisionLoop () {};

	bool Apply (Mesh_half_edge *model);

	void SetUseWarrenMask (bool b) { m_useWarrenMask = b; }
	bool GetUseWarrenMask () const { return m_useWarrenMask; }

private:
	bool m_useWarrenMask = false;
};
