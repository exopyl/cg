#include "surface_implicit_pointcloud.h"

#include <math.h>
#include <stdlib.h>

#include "octree.h"

// Octree build parameters: stop splitting at <= 64 points per leaf or depth 10.
static const unsigned int OCTREE_MAX_POINTS = 64;
static const unsigned int OCTREE_MAX_DEPTH  = 10;

PointCloudField::PointCloudField()
	: m_octree(nullptr)
	, m_nPoints(0)
	, m_sigma(1.f)
	, m_invSigma2(1.f)
	, m_cutoff(3.f)
	, m_isoLevel(0.5f) // T < 1 so an isolated point yields a finite-radius surface
{
}

PointCloudField::~PointCloudField()
{
	delete m_octree;
}

void PointCloudField::Build(const float* pPoints, int nPoints)
{
	delete m_octree;
	m_octree = nullptr;
	m_nPoints = 0;

	if (!pPoints || nPoints <= 0)
		return;

	m_nPoints = nPoints;
	m_octree = new Octree();
	// Octree::Build copies the points internally and only reads pPoints, so the
	// const_cast is safe and avoids an extra copy here.
	m_octree->Build(const_cast<float*>(pPoints), nPoints,
	                OCTREE_MAX_POINTS, OCTREE_MAX_DEPTH);
}

void PointCloudField::SetIsoDistance(float d)
{
	if (d <= 0.f)
		d = 1e-4f;

	// T = exp(-d^2 / sigma^2)  =>  sigma = d / sqrt(-ln T)
	const float k = sqrtf(-logf(m_isoLevel)); // ~0.8326 for T = 0.5
	m_sigma     = d / k;
	m_invSigma2 = 1.f / (m_sigma * m_sigma);
	m_cutoff    = 3.f * m_sigma; // exp(-9) ~ 1.2e-4, negligible beyond this
}

void PointCloudField::GetPaddedAABB(float vmin[3], float vmax[3], float margin) const
{
	if (!m_octree || m_nPoints == 0)
	{
		for (int i = 0; i < 3; i++) { vmin[i] = -1.f; vmax[i] = 1.f; }
		return;
	}

	m_octree->GetMinMax(vmin, vmax);
	const float pad = margin * m_sigma;
	for (int i = 0; i < 3; i++)
	{
		vmin[i] -= pad;
		vmax[i] += pad;
	}
}

// Recursively accumulate the Gaussian contributions of every cloud point
// within m_cutoff of (px,py,pz), pruning octree subtrees whose bounding box is
// entirely farther than the cutoff. Uses only the octree's public accessors.
static void accumulate(Octree* node,
                       float px, float py, float pz,
                       float cutoff2, float invSigma2,
                       double& sum)
{
	if (!node)
		return;

	// Prune: squared distance from the query point to the node's AABB.
	float vmin[3], vmax[3];
	node->GetMinMax(vmin, vmax);
	const float p[3] = { px, py, pz };
	float d2box = 0.f;
	for (int k = 0; k < 3; k++)
	{
		if (p[k] < vmin[k]) { const float dd = vmin[k] - p[k]; d2box += dd * dd; }
		else if (p[k] > vmax[k]) { const float dd = p[k] - vmax[k]; d2box += dd * dd; }
	}
	if (d2box > cutoff2)
		return;

	// Leaf: sum the points it holds (internal nodes have m_nPoints == 0).
	unsigned int n = node->GetNPoints();
	if (n)
	{
		const float* pts = node->GetPoints();
		for (unsigned int i = 0; i < n; i++)
		{
			const float dx = px - pts[3 * i];
			const float dy = py - pts[3 * i + 1];
			const float dz = pz - pts[3 * i + 2];
			const float d2 = dx * dx + dy * dy + dz * dz;
			if (d2 <= cutoff2)
				sum += exp(-d2 * invSigma2);
		}
	}

	Octree** children = node->GetChildren();
	for (int i = 0; i < 8; i++)
		accumulate(children[i], px, py, pz, cutoff2, invSigma2, sum);
}

float PointCloudField::Eval(float x, float y, float z, void* user_data)
{
	PointCloudField* f = static_cast<PointCloudField*>(user_data);
	if (!f || !f->m_octree || f->m_nPoints == 0)
		return 0.f;

	double sum = 0.0;
	accumulate(f->m_octree, x, y, z, f->m_cutoff * f->m_cutoff, f->m_invSigma2, sum);
	return (float)sum;
}
