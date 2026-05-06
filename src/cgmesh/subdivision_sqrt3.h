#pragma once
#include "mesh_half_edge.h"

//
// √3 subdivision (Kobbelt, "√3-Subdivision", SIGGRAPH 2000).
//
// Each iteration combines :
//   (a) inserting one new vertex at the centroid of every triangle  (1 -> 3 split),
//   (b) flipping every interior original edge,
//   (c) optionally smoothing the original vertices via Kobbelt's α(n) mask.
//
// Combinatorial growth per iteration : nv -> nv + nf, nf -> 3 * nf  (same as
// Karbacher), but with the added edge flip step which gives the scheme its
// characteristic property : after TWO iterations, the original edges reappear
// at length 1/3 (= 1/√3 × 1/√3) ; hence the name "√3".
//
// Compared to Loop subdivision (1 -> 4) :
//   - finer granularity (×3 vs ×4) — more level-of-detail steps for the same
//     final triangle count ;
//   - more isotropic refinement (triangles tend toward equilateral faster) ;
//   - C^2 limit surface in the regular case (same as Loop), C^1 at extraordinary
//     vertices.
//
// Stencils (when m_smoothOriginal = true) :
//   New vertex per face (centroid) :
//     M_f = (V_a + V_b + V_c) / 3
//   Original interior vertex of valence n :
//     V' = (1 - α(n)) V + (α(n) / n) Σ N_i
//     α(n) = (4 - 2 cos(2π/n)) / 9
//   Original boundary vertex with two boundary neighbors V_L, V_R :
//     V' = (4/27) V_L + (19/27) V + (4/27) V_R
//
// Boundary edges are NOT flipped — the boundary curve evolves as a stationary
// cubic B-spline on its own, isolated from the interior subdivision.
//
class MeshAlgoSubdivisionSqrt3
{
public:
	MeshAlgoSubdivisionSqrt3 () {};
	~MeshAlgoSubdivisionSqrt3 () {};

	bool Apply (Mesh_half_edge *model);

	// When true (default), Kobbelt's smoothing mask is applied to original
	// vertices. When false, original vertices keep their position (pure
	// refinement — sharp corners preserved). The edge-flip step runs in
	// either mode.
	void SetSmoothOriginal (bool b) { m_smoothOriginal = b; }
	bool GetSmoothOriginal () const { return m_smoothOriginal; }

private:
	bool m_smoothOriginal = true;
};
