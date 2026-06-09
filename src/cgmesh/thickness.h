#pragma once
#include <vector>

class Mesh;

//
// Wall-thickness computation on a triangle mesh.
//
// Implements step 1 (MVP) of src/cgmesh/thickness.md: the "wall thickness"
// ray method (M2) + the colour-map visualisation (V1).
//
// For each vertex, a single ray is cast along the inverted vertex normal
// (-n, i.e. into the material) and the thickness is the distance to the
// first opposite face. The ray/triangle test is done WITHOUT back-face
// culling on purpose: Mesh::GetIntersectionWithRay (the rendering primitive)
// culls back faces, so on an outward-oriented closed mesh an inward ray
// would hit the *back* of the opposite wall and be discarded — it cannot be
// reused here. Ray casting is accelerated by a triangle octree (built once
// per call), giving ~O(V*log F) instead of the O(V*F) brute force.
//
// Two methods are provided: ComputeWallThickness (single inward ray, M2) and
// ComputeShapeDiameter (robust cone-of-rays Shape Diameter Function, M1).
//
class MeshAlgoThickness
{
public:
	// Per-vertex wall thickness.
	//   outThickness[i] = distance to the opposite wall (0 where undefined).
	//   outDefined[i]   = 1 if a hit was found, 0 otherwise.
	// Requires a closed, consistently outward-oriented triangle mesh; where
	// the ray finds no opposite face (open mesh / leak / degenerate normal),
	// the value is reported undefined rather than wrong.
	// Returns false if the mesh is unusable (empty or not a triangle mesh).
	//
	// Side effects on `mesh` (hence the non-const ref): recomputes the vertex
	// normals if they are absent or of inconsistent size, and refreshes the
	// bounding box (used for the self-intersection length scale).
	static bool ComputeWallThickness (Mesh &mesh,
	                                  std::vector<float> &outThickness,
	                                  std::vector<char>  &outDefined);

	// Convenience: compute the field then colour the mesh as a heatmap with the
	// manufacturing convention THIN = red, THICK = blue (Moreland perceptual
	// cool-warm map). The raw field is also returned for export / stats.
	//
	// Colour scale: the field is normalised over [scaleMin, scaleMax]; when
	// scaleMax <= scaleMin (the default) the actual min/max over the defined
	// vertices is used. A constant field (min == max) maps to the mid (grey)
	// colour. Undefined vertices are left neutral grey.
	// Side effect (on top of ComputeWallThickness): overwrites m_pVertexColors.
	static bool ColorizeWallThickness (Mesh &mesh,
	                                   std::vector<float> &outThickness,
	                                   std::vector<char>  &outDefined,
	                                   float scaleMin = -1.f,
	                                   float scaleMax = -1.f);

	// --- Shape Diameter Function (M1): robust cone-of-rays thickness -------
	//
	// For each vertex, casts `numRays` rays inside a cone of half-angle
	// `coneHalfAngleDeg` around the inward normal, keeps the nearest opposite
	// face per ray, then aggregates robustly: median + 1-sigma outlier
	// rejection + cosine-weighted mean (rays near the axis weigh more). The
	// field is then smoothed over the 1-ring `smoothIterations` times. This is
	// far less sensitive to noise and to the "first-hit" ambiguity of the
	// single-ray wall thickness (M2). With numRays==1, coneHalfAngleDeg==0 and
	// smoothIterations==0 it reduces exactly to ComputeWallThickness.
	// Same outThickness/outDefined contract and the same mesh side effects
	// (normals recomputed if missing, bbox refreshed) as ComputeWallThickness.
	static bool ComputeShapeDiameter (Mesh &mesh,
	                                  std::vector<float> &outThickness,
	                                  std::vector<char>  &outDefined,
	                                  int   numRays          = 16,
	                                  float coneHalfAngleDeg = 60.0f,
	                                  int   smoothIterations = 1);

	// Convenience: ComputeShapeDiameter + heatmap colouring (THIN = red,
	// THICK = blue; see ColorizeWallThickness for the colour scale and side
	// effects).
	static bool ColorizeShapeDiameter (Mesh &mesh,
	                                   std::vector<float> &outThickness,
	                                   std::vector<char>  &outDefined,
	                                   int   numRays          = 16,
	                                   float coneHalfAngleDeg = 60.0f,
	                                   int   smoothIterations = 1,
	                                   float scaleMin         = -1.f,
	                                   float scaleMax         = -1.f);
};
