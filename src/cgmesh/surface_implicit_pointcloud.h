#pragma once

//
// PointCloudField
//
// Builds a blobby (sum-of-Gaussians) scalar field from a point cloud so it can
// be meshed by ImplicitSurface (marching cubes):
//
//     f(p) = Sum_i exp( -||p - p_i||^2 / sigma^2 )
//
// The extracted surface is the iso-level { p : f(p) = T }. A single isolated
// point peaks at f = 1, so T is fixed below 1 (otherwise an isolated point
// yields no surface). We expose an intuitive "iso distance" d -- the offset
// radius of the surface around an isolated point -- and derive sigma from it:
//
//     T = exp( -d^2 / sigma^2 )  =>  sigma = d / sqrt(-ln T)
//
// The field is passed to ImplicitSurface via set_eval_func(&Eval) together
// with set_eval_data(this); the static Eval() recovers the instance from the
// user-data pointer, so the class is reentrant (no file-static globals).
//
// Neighbour queries are accelerated by an Octree: only points within a cutoff
// radius (3*sigma, beyond which exp(...) is negligible) contribute to the sum.
//

class Octree;

class PointCloudField
{
public:
	PointCloudField();
	~PointCloudField();

	// Owns a raw Octree* freed in the destructor: non-copyable (a shallow copy
	// would double-free). Move is not needed (held by value in a non-copied
	// parameterized object).
	PointCloudField(const PointCloudField&) = delete;
	PointCloudField& operator=(const PointCloudField&) = delete;

	// Copy the points (3*nPoints interleaved x,y,z floats) and build the octree.
	void Build(const float* pPoints, int nPoints);

	// Set the offset distance d of the iso-surface around an isolated point.
	// Updates the internal Gaussian width sigma and the cutoff radius.
	void SetIsoDistance(float d);

	// Iso-level T passed to ImplicitSurface::set_value().
	float GetIsoLevel() const { return m_isoLevel; }

	// Axis-aligned bounding box of the cloud, enlarged by margin*sigma so the
	// blobs are never clipped by the marching-cubes grid (=> closed surface).
	// Falls back to a small box centred on the origin for an empty cloud.
	void GetPaddedAABB(float vmin[3], float vmax[3], float margin = 3.f) const;

	int NPoints() const { return m_nPoints; }

	// Eval callback for ImplicitSurface::set_eval_func(); user_data is the
	// PointCloudField* installed via ImplicitSurface::set_eval_data().
	static float Eval(float x, float y, float z, void* user_data);

private:
	Octree* m_octree;
	int     m_nPoints;

	float   m_sigma;       // Gaussian width
	float   m_invSigma2;   // 1 / sigma^2 (precomputed for Eval)
	float   m_cutoff;      // neighbour-query radius (= 3*sigma)
	float   m_isoLevel;    // T
};
