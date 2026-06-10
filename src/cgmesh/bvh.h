#pragma once
#include "../cgmath/cgmath.h"   // vec3
#include <vector>

class Mesh;

//
// Median-split BVH over a triangle mesh's faces, for nearest-hit ray queries
// WITHOUT back-face culling (an inward ray must register the opposite,
// back-facing wall — so this cannot reuse Mesh::GetIntersectionWithRay, which
// culls for rendering).
//
// Robust to anisotropic (thin / elongated) meshes where a spatial-median
// octree degenerates: each triangle lands in exactly ONE leaf (no straddling
// duplication), node AABBs adapt to the geometry, build is O(F log F). Built
// once from a Mesh, then queried read-only and concurrently from many threads.
//
class BVH
{
public:
	// Build over the mesh's CURRENT geometry. Vertex positions are referenced
	// (mesh.m_pVertices must outlive the BVH and stay un-reallocated); the
	// triangle vertex indices are copied. Triangle meshes only (faces use
	// vertices 0..2).
	void build (Mesh &mesh);

	// Nearest opposite-face distance along the ray (orig, dir); -1 if the ray
	// hits nothing. dir must be normalised so the result is a Euclidean
	// distance. Hits at t <= tMin are rejected (skips the faces incident to a
	// start vertex). const + member-read-only -> safe to call concurrently.
	float nearest (vec3 orig, vec3 dir, float tMin) const;

private:
	struct Node
	{
		float bmin[3], bmax[3];
		int   left, right;     // children indices; leaf when left < 0
		int   start, count;    // leaf triangle range into m_order
	};

	int         buildRange (unsigned int begin, unsigned int end);
	static bool slabHit (const Node &nd, vec3 o, vec3 dir, float t0, float t1);

	static constexpr unsigned int LEAF = 4u;       // target triangles per leaf

	const float              *m_verts = nullptr;   // shared mesh vertices
	unsigned int              m_nv = 0;
	std::vector<unsigned int> m_tri;               // 3*nf vertex indices (owned)
	std::vector<unsigned int> m_order;             // triangle ids, reordered
	std::vector<Node>         m_nodes;

	// Per-triangle AABB + centroid, used only during build (freed afterwards).
	std::vector<float>        m_cmid, m_tmin, m_tmax;
};
