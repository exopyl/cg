#include "thickness.h"
#include "mesh.h"
#include "octree.h"
#include "../cgmath/aabox.h"
#include "../cgmath/ray.h"
#include "../cgimg/color.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
	// Möller–Trumbore ray/triangle intersection WITHOUT back-face culling.
	// orig/dir define the ray (dir is assumed normalised, so the returned t
	// is the Euclidean distance). Hits at t <= tMin are rejected — this skips
	// the faces incident to the start vertex (distance ~0). Returns true and
	// writes *tOut on a valid hit.
	bool intersectRayTriangleNoCull (vec3 orig, vec3 dir,
	                                 const float *v0f, const float *v1f, const float *v2f,
	                                 float tMin, float *tOut)
	{
		vec3 v0, v1, v2, e1, e2, h, s, q;
		vec3_init (v0, v0f[0], v0f[1], v0f[2]);
		vec3_init (v1, v1f[0], v1f[1], v1f[2]);
		vec3_init (v2, v2f[0], v2f[1], v2f[2]);

		vec3_subtraction (e1, v1, v0);
		vec3_subtraction (e2, v2, v0);

		vec3_cross_product (h, dir, e2);
		float a = vec3_dot_product (e1, h);
		if (fabs (a) < 1e-12f)
			return false;                 // ray parallel to triangle plane

		float invA = 1.f / a;
		vec3_subtraction (s, orig, v0);
		float u = invA * vec3_dot_product (s, h);
		if (u < 0.f || u > 1.f)
			return false;

		vec3_cross_product (q, s, e1);
		float v = invA * vec3_dot_product (dir, q);
		if (v < 0.f || u + v > 1.f)
			return false;

		float t = invA * vec3_dot_product (e2, q);
		if (t <= tMin)
			return false;                 // behind origin / self-intersection

		*tOut = t;
		return true;
	}

	// Recursively find the nearest opposite-face hit of the ray under `node`,
	// WITHOUT back-face culling (so an inward ray on an outward-oriented mesh
	// still registers the opposite wall — which Mesh::GetIntersectionWithRay,
	// being a culling rendering primitive, would discard). Leaves test their
	// triangles; internal nodes are pruned by an exact ray/AABB slab test and
	// by the current best distance. `*best` holds the nearest distance so far
	// (-1 if none); `verts` is the shared vertex array, `nv` its vertex count.
	void octreeNearestNoCull (Octree *node, vec3 orig, vec3 dir,
	                          const float *verts, unsigned int nv,
	                          float tMin, float *best)
	{
		if (node->IsLeaf ())
		{
			const unsigned int  n    = node->GetNTriangles ();
			const unsigned int *tris = node->GetTriangles ();
			for (unsigned int i = 0; i < n; ++i)
			{
				const unsigned int a = tris[3*i], b = tris[3*i+1], c = tris[3*i+2];
				if (a >= nv || b >= nv || c >= nv)
					continue;             // corrupt index -> skip (no OOB read)
				float t;
				if (intersectRayTriangleNoCull (orig, dir,
				        &verts[3*a], &verts[3*b], &verts[3*c], tMin, &t))
				{
					if (*best < 0.f || t < *best)
						*best = t;
				}
			}
			return;
		}

		// Recursion depth is bounded by the octree's build-time maxDepth, so
		// no explicit depth counter is needed here.
		Octree **children = node->GetChildren ();
		Ray ray (orig[0], orig[1], orig[2], dir[0], dir[1], dir[2]);
		for (int i = 0; i < 8; ++i)
		{
			Octree *child = children[i];
			if (!child)
				continue;
			float mn[3], mx[3];
			child->GetMinMax (mn, mx);
			AABox box (mn[0], mn[1], mn[2]);
			box.AddVertex (mx[0], mx[1], mx[2]);
			// Prune any node farther than the nearest hit found so far.
			const float tFar = (*best >= 0.f) ? *best : 1e30f;
			if (box.intersection (ray, tMin, tFar))
				octreeNearestNoCull (child, orig, dir, verts, nv, tMin, best);
		}
	}

	// Build a triangle octree over the mesh's current geometry. Vertex
	// positions are referenced from mesh.m_pVertices (stable); the triangle
	// index list is copied into the octree, so the malloc'd temporary from
	// GetTriangles() is freed right after the build.
	void buildTriangleOctree (Mesh &mesh, Octree &octree)
	{
		unsigned int *tris = mesh.GetTriangles ();
		octree.BuildForTriangles (mesh.m_pVertices.data (), mesh.GetNVertices (),
		                          /*maxTriangles*/ 32, /*maxDepth*/ 8,
		                          tris, mesh.GetNFaces ());
		free (tris);
	}

	// Nearest opposite-face distance along (P, dir); -1 if the ray escapes.
	float castNearest (Octree &octree, vec3 P, vec3 dir,
	                   const float *verts, unsigned int nv, float tMin)
	{
		float best = -1.f;
		octreeNearestNoCull (&octree, P, dir, verts, nv, tMin, &best);
		return best;
	}

	// Orthonormal basis (t, b) spanning the plane perpendicular to unit `axis`.
	void makeBasis (vec3 axis, vec3 t, vec3 b)
	{
		vec3 up;
		if (fabs (axis[0]) < 0.9f) vec3_init (up, 1.f, 0.f, 0.f);
		else                       vec3_init (up, 0.f, 1.f, 0.f);
		vec3_cross_product (t, axis, up);
		vec3_normalize (t);
		vec3_cross_product (b, axis, t);   // unit (axis ⟂ t, both unit)
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

	// Triangle octree over the CURRENT geometry accelerates ray casting
	// (~O(V*log F) instead of the former O(V*F) brute force).
	Octree octree;
	buildTriangleOctree (mesh, octree);

	const float *verts = mesh.m_pVertices.data ();
	for (unsigned int vi = 0; vi < nv; ++vi)
	{
		vec3 P, nrm, dir;
		vec3_init (P,
		           verts[3*vi], verts[3*vi+1], verts[3*vi+2]);
		vec3_init (nrm,
		           mesh.m_pVertexNormals[3*vi], mesh.m_pVertexNormals[3*vi+1], mesh.m_pVertexNormals[3*vi+2]);

		if (vec3_length (nrm) < 1e-12f)
			continue;                     // no usable normal -> undefined

		vec3_scale (dir, nrm, -1.f);
		vec3_normalize (dir);             // normalised -> t == Euclidean distance

		const float d = castNearest (octree, P, dir, verts, nv, tMin);
		if (d >= 0.f)
		{
			outThickness[vi] = d;
			outDefined[vi]   = 1;
		}
	}

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

	Octree octree;
	buildTriangleOctree (mesh, octree);

	const float *verts = mesh.m_pVertices.data ();
	std::vector<float> dists, weights;
	dists.reserve (numRays);
	weights.reserve (numRays);

	for (unsigned int vi = 0; vi < nv; ++vi)
	{
		vec3 P, nrm, axis, tb, bb;
		vec3_init (P,   verts[3*vi], verts[3*vi+1], verts[3*vi+2]);
		vec3_init (nrm, mesh.m_pVertexNormals[3*vi], mesh.m_pVertexNormals[3*vi+1], mesh.m_pVertexNormals[3*vi+2]);
		if (vec3_length (nrm) < 1e-12f)
			continue;

		vec3_scale (axis, nrm, -1.f);     // inward
		vec3_normalize (axis);
		makeBasis (axis, tb, bb);

		dists.clear ();
		weights.clear ();
		for (int k = 0; k < numRays; ++k)
		{
			// Even cone coverage: angle from axis grows as sqrt(k) (area-fair),
			// azimuth advances by the golden angle.
			const float a  = (numRays == 1) ? 0.f
			                 : coneHalf * std::sqrt ((float)k / (float)(numRays - 1));
			const float az = GOLDEN * (float)k;
			const float ca = cos (a), sa = sin (a);
			const float caz = cos (az), saz = sin (az);

			// dir = cos(a)*axis + sin(a)*radial, with radial a unit vector in
			// the plane ⟂ axis (tb,bb orthonormal) -> dir is already unit, no
			// normalisation needed.
			vec3 dir;
			for (int c = 0; c < 3; ++c)
				dir[c] = ca * axis[c] + sa * (caz * tb[c] + saz * bb[c]);

			const float d = castNearest (octree, P, dir, verts, nv, tMin);
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
