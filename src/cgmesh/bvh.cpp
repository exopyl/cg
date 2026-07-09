#include "bvh.h"
#include "mesh.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>

namespace
{
	// Möller–Trumbore ray/triangle intersection WITHOUT back-face culling.
	// orig/dir define the ray (dir is assumed normalised, so the returned t
	// is the Euclidean distance). Hits at t <= tMin are rejected — this skips
	// the faces incident to the start vertex (distance ~0). Returns true and
	// writes *tOut on a valid hit.
	bool intersectRayTriangleNoCull (const Vector3f &orig, const Vector3f &dir,
	                                 const float *v0f, const float *v1f, const float *v2f,
	                                 float tMin, float *tOut)
	{
		Vector3f v0, v1, v2, e1, e2, h, s, q;
		v0.Set (v0f[0], v0f[1], v0f[2]);
		v1.Set (v1f[0], v1f[1], v1f[2]);
		v2.Set (v2f[0], v2f[1], v2f[2]);

		e1 = v1 - v0;
		e2 = v2 - v0;

		h = (dir).CrossProduct (e2);
		float a = (e1).DotProduct (h);
		if (fabs (a) < 1e-12f)
			return false;                 // ray parallel to triangle plane

		float invA = 1.f / a;
		s = orig - v0;
		float u = invA * (s).DotProduct (h);
		if (u < 0.f || u > 1.f)
			return false;

		q = (s).CrossProduct (e1);
		float v = invA * (dir).DotProduct (q);
		if (v < 0.f || u + v > 1.f)
			return false;

		float t = invA * (e2).DotProduct (q);
		if (t <= tMin)
			return false;                 // behind origin / self-intersection

		*tOut = t;
		return true;
	}
}

void BVH::build (Mesh &mesh)
{
	m_verts = mesh.m_pVertices.data ();
	m_nv    = mesh.GetNVertices ();
	const unsigned int nf = mesh.GetNFaces ();

	unsigned int *t = mesh.GetTriangles ();   // malloc'd 3*nf
	m_tri.assign (t, t + 3u * nf);
	free (t);

	m_cmid.resize (3u * nf); m_tmin.resize (3u * nf); m_tmax.resize (3u * nf);
	m_order.resize (nf);
	for (unsigned int f = 0; f < nf; ++f)
	{
		m_order[f] = f;
		const unsigned int a = m_tri[3*f], b = m_tri[3*f+1], c = m_tri[3*f+2];
		for (int d = 0; d < 3; ++d)
		{
			const float va = m_verts[3*a+d], vb = m_verts[3*b+d], vc = m_verts[3*c+d];
			const float mn = std::min (va, std::min (vb, vc));
			const float mx = std::max (va, std::max (vb, vc));
			m_tmin[3*f+d] = mn; m_tmax[3*f+d] = mx; m_cmid[3*f+d] = 0.5f * (mn + mx);
		}
	}
	m_nodes.clear ();
	m_nodes.reserve (2u * nf + 1u);
	if (nf > 0) buildRange (0u, nf);

	m_cmid.clear (); m_cmid.shrink_to_fit ();
	m_tmin.clear (); m_tmin.shrink_to_fit ();
	m_tmax.clear (); m_tmax.shrink_to_fit ();
}

int BVH::buildRange (unsigned int begin, unsigned int end)
{
	const int idx = (int)m_nodes.size ();
	m_nodes.push_back (Node{});

	float bmin[3] = { 1e30f, 1e30f, 1e30f }, bmax[3] = { -1e30f, -1e30f, -1e30f };
	float cmin[3] = { 1e30f, 1e30f, 1e30f }, cmax[3] = { -1e30f, -1e30f, -1e30f };
	for (unsigned int i = begin; i < end; ++i)
	{
		const unsigned int f = m_order[i];
		for (int d = 0; d < 3; ++d)
		{
			bmin[d] = std::min (bmin[d], m_tmin[3*f+d]);
			bmax[d] = std::max (bmax[d], m_tmax[3*f+d]);
			cmin[d] = std::min (cmin[d], m_cmid[3*f+d]);
			cmax[d] = std::max (cmax[d], m_cmid[3*f+d]);
		}
	}
	for (int d = 0; d < 3; ++d) { m_nodes[idx].bmin[d] = bmin[d]; m_nodes[idx].bmax[d] = bmax[d]; }

	const unsigned int count = end - begin;
	int axis = 0; float ext = cmax[0] - cmin[0];
	if (cmax[1] - cmin[1] > ext) { axis = 1; ext = cmax[1] - cmin[1]; }
	if (cmax[2] - cmin[2] > ext) { axis = 2; ext = cmax[2] - cmin[2]; }

	if (count <= LEAF || ext <= 0.f)          // small or degenerate -> leaf
	{
		m_nodes[idx].left = m_nodes[idx].right = -1;
		m_nodes[idx].start = (int)begin; m_nodes[idx].count = (int)count;
		return idx;
	}

	const unsigned int mid = begin + count / 2u;
	std::nth_element (m_order.begin () + begin, m_order.begin () + mid, m_order.begin () + end,
	    [&](unsigned int A, unsigned int B) { return m_cmid[3*A+axis] < m_cmid[3*B+axis]; });

	const int l = buildRange (begin, mid);
	const int r = buildRange (mid, end);
	m_nodes[idx].left = l; m_nodes[idx].right = r;
	m_nodes[idx].start = m_nodes[idx].count = 0;
	return idx;
}

float BVH::nearest (const Vector3f &orig, const Vector3f &dir, float tMin) const
{
	if (m_nodes.empty ()) return -1.f;
	float best = -1.f;
	// Explicit DFS stack. Fixed capacity: a median-split BVH has depth
	// ~log2(nf), so 64 covers ~1.8e19 triangles. std::array keeps it
	// stack-allocated (zero per-query heap allocation — nearest() is on the hot
	// path), unlike std::vector.
	std::array<int, 64> stack;
	int sp = 0;
	stack[sp++] = 0;
	while (sp > 0)
	{
		const Node &nd = m_nodes[stack[--sp]];
		if (!slabHit (nd, orig, dir, tMin, (best >= 0.f) ? best : 1e30f))
			continue;
		if (nd.left < 0)
		{
			const int e = nd.start + nd.count;
			for (int i = nd.start; i < e; ++i)
			{
				const unsigned int f = m_order[i];
				const unsigned int a = m_tri[3*f], b = m_tri[3*f+1], c = m_tri[3*f+2];
				if (a >= m_nv || b >= m_nv || c >= m_nv) continue;
				float t;
				if (intersectRayTriangleNoCull (orig, dir,
				        &m_verts[3*a], &m_verts[3*b], &m_verts[3*c], tMin, &t))
					if (best < 0.f || t < best) best = t;
			}
		}
		else
		{
			// The capacity is a safety net (see above). assert() surfaces any
			// breach in debug instead of silently dropping nodes (which would
			// under-report the thickness).
			assert (sp + 2 <= (int)stack.size () && "BVH traversal stack overflow");
			if (sp + 2 <= (int)stack.size ())
			{
				stack[sp++] = nd.left;
				stack[sp++] = nd.right;
			}
		}
	}
	return best;
}

namespace
{
	// Squared distance from point p to an AABB (0 if inside). Lower bound on the
	// distance to anything in the node -> used to prune the closest-point descent.
	float aabbDist2 (const float bmin[3], const float bmax[3], const Vector3f &p)
	{
		float d2 = 0.f;
		for (int d = 0; d < 3; ++d)
		{
			const float v = p[d];
			if (v < bmin[d]) { const float e = bmin[d] - v; d2 += e * e; }
			else if (v > bmax[d]) { const float e = v - bmax[d]; d2 += e * e; }
		}
		return d2;
	}
}

float BVH::closest_distance2 (const Vector3f &p, Vector3f *closest_out) const
{
	if (m_nodes.empty ()) return -1.f;

	float best2 = 1e30f;
	Vector3f bestPt (0.f, 0.f, 0.f);

	std::array<int, 64> stack;
	int sp = 0;
	stack[sp++] = 0;
	while (sp > 0)
	{
		const Node &nd = m_nodes[stack[--sp]];
		if (aabbDist2 (nd.bmin, nd.bmax, p) >= best2)
			continue;                               // whole node is farther than best

		if (nd.left < 0)
		{
			const int e = nd.start + nd.count;
			for (int i = nd.start; i < e; ++i)
			{
				const unsigned int f = m_order[i];
				const unsigned int a = m_tri[3*f], b = m_tri[3*f+1], c = m_tri[3*f+2];
				if (a >= m_nv || b >= m_nv || c >= m_nv) continue;
				Vector3f cl;
				const float d2 = point_triangle_distance2 (p, &m_verts[3*a], &m_verts[3*b], &m_verts[3*c], cl);
				if (d2 < best2) { best2 = d2; bestPt[0]=cl[0]; bestPt[1]=cl[1]; bestPt[2]=cl[2]; }
			}
		}
		else
		{
			// Visit the nearer child first (push it last) so best2 shrinks early
			// and prunes the farther subtree harder.
			const Node &L = m_nodes[nd.left];
			const Node &R = m_nodes[nd.right];
			const float dl = aabbDist2 (L.bmin, L.bmax, p);
			const float dr = aabbDist2 (R.bmin, R.bmax, p);
			assert (sp + 2 <= (int)stack.size () && "BVH closest-point stack overflow");
			if (sp + 2 <= (int)stack.size ())
			{
				if (dl < dr) { stack[sp++] = nd.right; stack[sp++] = nd.left; }
				else         { stack[sp++] = nd.left;  stack[sp++] = nd.right; }
			}
		}
	}

	if (best2 >= 1e30f) return -1.f;
	if (closest_out) *closest_out = bestPt;
	return best2;
}

// Robust ray/AABB slab test on [t0, t1]; handles dir component == 0 (ray
// parallel to an axis) without producing a 0*inf NaN.
bool BVH::slabHit (const Node &nd, const Vector3f &o, const Vector3f &dir, float t0, float t1)
{
	for (int d = 0; d < 3; ++d)
	{
		if (dir[d] != 0.f)
		{
			const float inv = 1.f / dir[d];
			float tn = (nd.bmin[d] - o[d]) * inv;
			float tf = (nd.bmax[d] - o[d]) * inv;
			if (tn > tf) std::swap (tn, tf);
			if (tn > t0) t0 = tn;
			if (tf < t1) t1 = tf;
			if (t0 > t1) return false;
		}
		else if (o[d] < nd.bmin[d] || o[d] > nd.bmax[d])
			return false;             // parallel to axis & outside the slab
	}
	return true;
}
