#include "mesh_metrics.h"
#include "mesh.h"
#include "bvh.h"

#include <cmath>

namespace
{
	// sup over samples of `from` of the distance to the surface indexed by `to`.
	// Samples = vertices + triangle barycentres (deterministic).
	float one_sided (Mesh &from, const BVH &to)
	{
		float worst2 = 0.f;

		for (unsigned int i = 0; i < from.m_nVertices; i++)
		{
			vec3 p;
			vec3_init (p, from.m_pVertices[3*i], from.m_pVertices[3*i+1], from.m_pVertices[3*i+2]);
			float d2 = to.closest_distance2 (p);
			if (d2 > worst2) worst2 = d2;
		}

		for (unsigned int f = 0; f < from.m_nFaces; f++)
		{
			Face *face = from.m_pFaces[f];
			if (!face || face->m_nVertices < 3) continue;
			int a = face->m_pVertices[0], b = face->m_pVertices[1], c = face->m_pVertices[2];
			vec3 p;
			vec3_init (p,
			           (from.m_pVertices[3*a]   + from.m_pVertices[3*b]   + from.m_pVertices[3*c])   / 3.f,
			           (from.m_pVertices[3*a+1] + from.m_pVertices[3*b+1] + from.m_pVertices[3*c+1]) / 3.f,
			           (from.m_pVertices[3*a+2] + from.m_pVertices[3*b+2] + from.m_pVertices[3*c+2]) / 3.f);
			float d2 = to.closest_distance2 (p);
			if (d2 > worst2) worst2 = d2;
		}

		return sqrtf (worst2);
	}
}

HausdorffResult mesh_hausdorff (Mesh &a, Mesh &b)
{
	BVH bvhA, bvhB;
	bvhA.build (a);
	bvhB.build (b);

	HausdorffResult r;
	r.a_to_b = one_sided (a, bvhB);
	r.b_to_a = one_sided (b, bvhA);
	r.symmetric = (r.a_to_b > r.b_to_a) ? r.a_to_b : r.b_to_a;
	return r;
}

float mesh_hausdorff_relative (Mesh &a, Mesh &b)
{
	HausdorffResult r = mesh_hausdorff (a, b);
	a.computebbox ();
	float diag = a.bbox_diagonal_length ();
	return (diag > 0.f) ? r.symmetric / diag : 0.f;
}
