#include "mesh_metrics.h"
#include "mesh.h"
#include "bvh.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	// sup over samples of `from` of the distance to the surface indexed by `to`.
	// Samples = vertices + triangle barycentres (deterministic). Si `out` non nul,
	// y empile toutes les distances échantillon->surface (pour mean/rms/percentile).
	float one_sided (Mesh &from, const BVH &to, std::vector<float> *out = nullptr)
	{
		float worst2 = 0.f;

		auto sample = [&](float x, float y, float z)
		{
			Vector3f p (x, y, z);
			float d2 = to.closest_distance2 (p);
			if (d2 < 0.f) d2 = 0.f;
			if (d2 > worst2) worst2 = d2;
			if (out) out->push_back (sqrtf (d2));
		};

		for (unsigned int i = 0; i < from.m_nVertices; i++)
			sample (from.m_pVertices[3*i], from.m_pVertices[3*i+1], from.m_pVertices[3*i+2]);

		for (unsigned int f = 0; f < from.m_nFaces; f++)
		{
			Face *face = from.m_pFaces[f];
			if (!face || face->m_nVertices < 3) continue;
			int a = face->m_pVertices[0], b = face->m_pVertices[1], c = face->m_pVertices[2];
			sample ((from.m_pVertices[3*a]   + from.m_pVertices[3*b]   + from.m_pVertices[3*c])   / 3.f,
			        (from.m_pVertices[3*a+1] + from.m_pVertices[3*b+1] + from.m_pVertices[3*c+1]) / 3.f,
			        (from.m_pVertices[3*a+2] + from.m_pVertices[3*b+2] + from.m_pVertices[3*c+2]) / 3.f);
		}

		return sqrtf (worst2);
	}
}

HausdorffResult mesh_hausdorff (Mesh &a, Mesh &b)
{
	BVH bvhA, bvhB;
	bvhA.build (a);
	bvhB.build (b);

	std::vector<float> da, db;
	HausdorffResult r {};
	r.a_to_b = one_sided (a, bvhB, &da);
	r.b_to_a = one_sided (b, bvhA, &db);
	r.symmetric = (r.a_to_b > r.b_to_a) ? r.a_to_b : r.b_to_a;

	// moyenne du sens A->B
	if (!da.empty ())
	{
		double s = 0; for (float v : da) s += v;
		r.mean_a_to_b = (float)(s / da.size ());
	}

	// statistiques symétriques sur l'union des échantillons des deux sens
	std::vector<float> all;
	all.reserve (da.size () + db.size ());
	all.insert (all.end (), da.begin (), da.end ());
	all.insert (all.end (), db.begin (), db.end ());
	if (!all.empty ())
	{
		double sm = 0, sq = 0;
		for (float v : all) { sm += v; sq += (double)v * v; }
		r.mean_symmetric = (float)(sm / all.size ());
		r.rms_symmetric  = (float)std::sqrt (sq / all.size ());
		std::sort (all.begin (), all.end ());
		r.p95_symmetric  = all[(size_t)(0.95 * (all.size () - 1))];
	}
	return r;
}

float mesh_hausdorff_relative (Mesh &a, Mesh &b)
{
	HausdorffResult r = mesh_hausdorff (a, b);
	a.computebbox ();
	float diag = a.bbox_diagonal_length ();
	return (diag > 0.f) ? r.symmetric / diag : 0.f;
}

std::vector<float> mesh_pointwise_distance (Mesh &from, Mesh &to)
{
	BVH bvh;
	bvh.build (to);
	std::vector<float> d (from.m_nVertices, 0.f);
	for (unsigned int i = 0; i < from.m_nVertices; i++)
	{
		Vector3f p (from.m_pVertices[3*i], from.m_pVertices[3*i+1], from.m_pVertices[3*i+2]);
		float d2 = bvh.closest_distance2 (p);
		d[i] = (d2 > 0.f) ? sqrtf (d2) : 0.f;
	}
	return d;
}
