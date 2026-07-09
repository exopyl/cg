#include "thickness.h"
#include "mesh.h"
#include "bvh.h"
#include "../cgimg/color.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace
{
	// Orthonormal basis (t, b) spanning the plane perpendicular to unit `axis`.
	void makeBasis (const Vector3f &axis, Vector3f &t, Vector3f &b)
	{
		Vector3f up;
		if (fabs (axis[0]) < 0.9f) up.Set (1.f, 0.f, 0.f);
		else                       up.Set (0.f, 1.f, 0.f);
		t = axis.CrossProduct (up);
		t.Normalize ();
		b = axis.CrossProduct (t);   // unit (axis ⟂ t, both unit)
	}

	// Robust SDF aggregation: median + reject samples beyond one std dev from
	// the median, then cosine-weighted mean of the survivors. -1 if no sample.
	float robustAggregate (const std::vector<float> &dists,
	                       const std::vector<float> &weights)
	{
		const size_t n = dists.size ();
		if (n == 0) return -1.f;
		if (n == 1) return dists[0];

		std::vector<float> sorted (dists);
		std::sort (sorted.begin (), sorted.end ());
		const float median = (n & 1) ? sorted[n/2]
		                             : 0.5f * (sorted[n/2 - 1] + sorted[n/2]);

		// Population std dev, UNWEIGHTED on purpose: it only drives outlier
		// rejection (a spread gate around the median), not the final estimate.
		// The cosine weighting is applied afterwards to the survivors' mean.
		float mean = 0.f;
		for (float d : dists) mean += d;
		mean /= (float)n;
		float var = 0.f;
		for (float d : dists) var += (d - mean) * (d - mean);
		const float sigma = std::sqrt (var / (float)n);

		float dsum = 0.f, wsum = 0.f;
		for (size_t i = 0; i < n; ++i)
		{
			if (sigma == 0.f || fabs (dists[i] - median) <= sigma)
			{
				dsum += weights[i] * dists[i];
				wsum += weights[i];
			}
		}
		return (wsum > 0.f) ? dsum / wsum : median;
	}

	// Laplacian smoothing of a per-vertex scalar field over the 1-ring,
	// restricted to defined vertices. Adjacency is derived from the faces.
	void smoothField (Mesh &mesh, std::vector<float> &val,
	                  const std::vector<char> &defined, int iterations)
	{
		const unsigned int nv = mesh.GetNVertices ();
		const unsigned int nf = mesh.GetNFaces ();
		if (iterations <= 0 || nv == 0) return;

		std::vector<std::vector<unsigned int>> adj (nv);
		for (unsigned int f = 0; f < nf; ++f)
		{
			Face *face = mesh.m_pFaces[f];
			int v[3] = { face->GetVertex (0), face->GetVertex (1), face->GetVertex (2) };
			for (int e = 0; e < 3; ++e)
			{
				const int a = v[e], b = v[(e + 1) % 3];
				if (a < 0 || b < 0 || (unsigned)a >= nv || (unsigned)b >= nv) continue;
				adj[a].push_back ((unsigned)b);
				adj[b].push_back ((unsigned)a);
			}
		}
		for (unsigned int i = 0; i < nv; ++i)
		{
			std::sort (adj[i].begin (), adj[i].end ());
			adj[i].erase (std::unique (adj[i].begin (), adj[i].end ()), adj[i].end ());
		}

		for (int it = 0; it < iterations; ++it)
		{
			std::vector<float> next (val);
			for (unsigned int i = 0; i < nv; ++i)
			{
				if (!defined[i]) continue;
				float sum = val[i];
				int   cnt = 1;
				for (unsigned int nb : adj[i])
					if (defined[nb]) { sum += val[nb]; ++cnt; }
				next[i] = sum / (float)cnt;
			}
			val.swap (next);
		}
	}

	// Run worker(begin, end) over [0, n) split into contiguous chunks across
	// the hardware threads. Falls back to a single serial call below
	// `minParallel`, or when hardware concurrency is unknown/1. Each worker
	// invocation owns its own scratch, so no shared mutable state crosses
	// threads; the read-only inputs (BVH, vertices) are shared safely.
	template <class Worker>
	void parallelChunks (unsigned int n, unsigned int minParallel, Worker worker)
	{
		const unsigned int hw = std::thread::hardware_concurrency ();
		const unsigned int nThreads = (n < minParallel || hw <= 1u)
		                            ? 1u : std::min (hw, n);
		if (nThreads <= 1u) { worker (0u, n); return; }

		const unsigned int chunk = (n + nThreads - 1u) / nThreads;
		std::vector<std::thread> pool;
		pool.reserve (nThreads - 1u);
		// RAII join: guarantees every spawned thread is joined on scope exit,
		// including if the calling-thread chunk below throws (a joinable
		// std::thread destructor would otherwise call std::terminate).
		struct JoinGuard {
			std::vector<std::thread> &p;
			~JoinGuard () { for (auto &th : p) if (th.joinable ()) th.join (); }
		} guard { pool };

		for (unsigned int t = 0; t + 1u < nThreads; ++t)
		{
			const unsigned int b = t * chunk;
			const unsigned int e = std::min (n, b + chunk);
			if (b >= e) break;
			pool.emplace_back ([=]{ worker (b, e); });
		}
		// Process the last chunk on the calling thread (pool joined by guard).
		const unsigned int blast = (nThreads - 1u) * chunk;
		if (blast < n) worker (blast, n);
	}

	// Colour a per-vertex scalar field as a Moreland cool-warm heatmap with the
	// manufacturing convention THIN = warm/red, THICK = cool/blue. The field is
	// normalised over [lo, hi] (values clamped); when hi <= lo the actual
	// min/max over the defined vertices is used. Undefined vertices get a
	// neutral grey. Writes mesh.m_pVertexColors directly (the render path
	// consumes it) — does not touch texture coordinates.
	void colorizeThicknessField (Mesh &mesh, const std::vector<float> &field,
	                             const std::vector<char> &defined,
	                             float lo, float hi)
	{
		const unsigned int nv = mesh.GetNVertices ();
		mesh.m_pVertexColors.assign (3u * nv, 0.6f);   // neutral grey default
		if (nv == 0) return;
		if (field.size () < nv || defined.size () < nv) return;   // size contract

		if (!(hi > lo))                                // auto range
		{
			bool any = false; float mn = 0.f, mx = 0.f;
			for (unsigned int i = 0; i < nv; ++i)
				if (defined[i])
				{
					const float v = field[i];
					if (!any) { mn = mx = v; any = true; }
					else { if (v < mn) mn = v; if (v > mx) mx = v; }
				}
			if (!any) return;                          // nothing defined -> grey
			lo = mn; hi = mx;
		}
		const float denom = (hi > lo) ? 1.f / (hi - lo) : 0.f;

		for (unsigned int i = 0; i < nv; ++i)
		{
			if (!defined[i]) continue;
			float t = (denom > 0.f) ? (field[i] - lo) * denom : 0.5f;
			if (t < 0.f) t = 0.f;
			if (t > 1.f) t = 1.f;
			// THIN (t~0) -> warm/red: feed (1 - t) so thin maps to the warm end.
			float r, g, b;
			color_coolwarm (1.f - t, &r, &g, &b);
			mesh.m_pVertexColors[3*i]   = r;
			mesh.m_pVertexColors[3*i+1] = g;
			mesh.m_pVertexColors[3*i+2] = b;
		}
	}
}

bool MeshAlgoThickness::ComputeWallThickness (Mesh &mesh,
                                              std::vector<float> &outThickness,
                                              std::vector<char>  &outDefined)
{
	const unsigned int nv = mesh.GetNVertices ();
	const unsigned int nf = mesh.GetNFaces ();
	if (nv == 0 || nf == 0)
		return false;
	if (!mesh.IsTriangleMesh ())
		return false;

	// Vertex normals give the surface orientation; -n points into the
	// material. (Re)compute if absent or stale-sized.
	if (mesh.m_pVertexNormals.size () < 3u * nv)
		mesh.ComputeNormals ();

	// Length scale for the self-intersection guard: ignore hits closer than a
	// tiny fraction of the bounding-box diagonal (the faces around the start
	// vertex sit at distance ~0).
	mesh.computebbox ();
	const float diag = mesh.bbox_diagonal_length ();
	const float tMin = (diag > 0.f ? diag : 1.f) * 1e-5f;

	outThickness.assign (nv, 0.f);
	outDefined.assign (nv, (char)0);

	// BVH over the CURRENT geometry accelerates ray casting (~O(V*log F)
	// instead of the former O(V*F) brute force), robust to thin/elongated meshes.
	BVH bvh;
	bvh.build (mesh);

	// Per-vertex ray casting is independent: BVH + vertices are read-only,
	// writes hit disjoint indices. Parallelise across hardware threads.
	const float *verts = mesh.m_pVertices.data ();
	parallelChunks (nv, /*minParallel*/ 2000u, [&](unsigned int begin, unsigned int end)
	{
		for (unsigned int vi = begin; vi < end; ++vi)
		{
			Vector3f P (verts[3*vi], verts[3*vi+1], verts[3*vi+2]);
			Vector3f nrm (mesh.m_pVertexNormals[3*vi], mesh.m_pVertexNormals[3*vi+1], mesh.m_pVertexNormals[3*vi+2]);

			if (nrm.getLength () < 1e-12f)
				continue;                 // no usable normal -> undefined

			Vector3f dir = nrm * (-1.f);
			dir.Normalize ();             // normalised -> t == Euclidean distance

			const float d = bvh.nearest (P, dir, tMin);
			if (d >= 0.f)
			{
				outThickness[vi] = d;
				outDefined[vi]   = 1;
			}
		}
	});

	return true;
}

bool MeshAlgoThickness::ComputeShapeDiameter (Mesh &mesh,
                                              std::vector<float> &outThickness,
                                              std::vector<char>  &outDefined,
                                              int   numRays,
                                              float coneHalfAngleDeg,
                                              int   smoothIterations)
{
	const unsigned int nv = mesh.GetNVertices ();
	const unsigned int nf = mesh.GetNFaces ();
	if (nv == 0 || nf == 0)
		return false;
	if (!mesh.IsTriangleMesh ())
		return false;

	if (mesh.m_pVertexNormals.size () < 3u * nv)
		mesh.ComputeNormals ();

	mesh.computebbox ();
	const float diag = mesh.bbox_diagonal_length ();
	const float tMin = (diag > 0.f ? diag : 1.f) * 1e-5f;

	// Clamp parameters to sane ranges.
	if (numRays < 1)   numRays = 1;
	if (numRays > 256) numRays = 256;
	const float PI = 3.14159265358979323846f;
	float coneHalf = coneHalfAngleDeg * (PI / 180.f);
	if (coneHalf < 0.f)            coneHalf = 0.f;
	if (coneHalf > 80.f * PI/180.f) coneHalf = 80.f * PI/180.f;  // keep rays forward
	const float GOLDEN = 2.39996323f;   // golden angle (rad) for even azimuths

	outThickness.assign (nv, 0.f);
	outDefined.assign (nv, (char)0);

	BVH bvh;
	bvh.build (mesh);

	// Per-vertex cone casting is independent. Parallelise across hardware
	// threads; each thread keeps its own dists/weights scratch (the former
	// single shared buffer would otherwise be a data race). Lower threshold
	// than the single-ray method since each vertex does numRays× the work.
	const float *verts = mesh.m_pVertices.data ();
	parallelChunks (nv, /*minParallel*/ 500u, [&](unsigned int begin, unsigned int end)
	{
		std::vector<float> dists, weights;     // per-thread scratch
		dists.reserve (numRays);
		weights.reserve (numRays);

		for (unsigned int vi = begin; vi < end; ++vi)
		{
			Vector3f P (verts[3*vi], verts[3*vi+1], verts[3*vi+2]);
			Vector3f nrm (mesh.m_pVertexNormals[3*vi], mesh.m_pVertexNormals[3*vi+1], mesh.m_pVertexNormals[3*vi+2]);
			if (nrm.getLength () < 1e-12f)
				continue;

			Vector3f axis = nrm * (-1.f);     // inward
			axis.Normalize ();
			Vector3f tb, bb;
			makeBasis (axis, tb, bb);

			dists.clear ();
			weights.clear ();
			for (int k = 0; k < numRays; ++k)
			{
				// Even cone coverage: angle from axis grows as sqrt(k)
				// (area-fair), azimuth advances by the golden angle.
				const float a  = (numRays == 1) ? 0.f
				                 : coneHalf * std::sqrt ((float)k / (float)(numRays - 1));
				const float az = GOLDEN * (float)k;
				const float ca = cos (a), sa = sin (a);
				const float caz = cos (az), saz = sin (az);

				// dir = cos(a)*axis + sin(a)*radial, with radial a unit vector
				// in the plane ⟂ axis (tb,bb orthonormal) -> already unit.
				Vector3f dir;
				for (int c = 0; c < 3; ++c)
					dir[c] = ca * axis[c] + sa * (caz * tb[c] + saz * bb[c]);

				const float d = bvh.nearest (P, dir, tMin);
				if (d >= 0.f)
				{
					dists.push_back (d);
					weights.push_back (ca);   // cosine weight: near-axis rays count more
				}
			}

			const float val = robustAggregate (dists, weights);
			if (val >= 0.f)
			{
				outThickness[vi] = val;
				outDefined[vi]   = 1;
			}
		}
	});

	smoothField (mesh, outThickness, outDefined, smoothIterations);
	return true;
}

bool MeshAlgoThickness::ColorizeWallThickness (Mesh &mesh,
                                               std::vector<float> &outThickness,
                                               std::vector<char>  &outDefined,
                                               float scaleMin, float scaleMax)
{
	if (!ComputeWallThickness (mesh, outThickness, outDefined))
		return false;

	colorizeThicknessField (mesh, outThickness, outDefined, scaleMin, scaleMax);
	return true;
}

bool MeshAlgoThickness::ColorizeShapeDiameter (Mesh &mesh,
                                               std::vector<float> &outThickness,
                                               std::vector<char>  &outDefined,
                                               int   numRays,
                                               float coneHalfAngleDeg,
                                               int   smoothIterations,
                                               float scaleMin, float scaleMax)
{
	if (!ComputeShapeDiameter (mesh, outThickness, outDefined,
	                           numRays, coneHalfAngleDeg, smoothIterations))
		return false;

	colorizeThicknessField (mesh, outThickness, outDefined, scaleMin, scaleMax);
	return true;
}
