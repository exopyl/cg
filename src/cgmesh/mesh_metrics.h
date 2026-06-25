#pragma once

class Mesh;

// Mesh comparison metrics. General-purpose (validation, QA, regression of any
// mesh-processing algorithm) — not specific to simplification.

struct HausdorffResult
{
	float a_to_b;     // sup over samples of A of the distance to B's surface
	float b_to_a;     // sup over samples of B of the distance to A's surface
	float symmetric;  // max(a_to_b, b_to_a)
};

// Approximate (one-sided and symmetric) Hausdorff distance between two triangle
// meshes, in absolute units. Each mesh is sampled deterministically (vertices +
// per-face barycentres — no RNG, so results are reproducible) and the nearest
// surface point on the other mesh is found with a BVH closest-point query.
// Sampling under-estimates on very large faces; densify the sampling if needed.
HausdorffResult mesh_hausdorff (Mesh &a, Mesh &b);

// Symmetric Hausdorff normalised by the bbox diagonal of `a` (a scale-free
// error in [0, ~1]); 0 if the diagonal is degenerate.
float mesh_hausdorff_relative (Mesh &a, Mesh &b);
