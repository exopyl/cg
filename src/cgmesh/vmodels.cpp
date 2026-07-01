#include "vmodels.h"

#include "mesh.h"

void Model::BuildBVH()
{
	m_bvhs.clear();
	for (Mesh* m : m_meshes.GetMeshes())
	{
		if (!m || m->GetNFaces() == 0)   // nuage sans faces -> pas de BVH (repli AABB)
			continue;
		auto bvh = std::make_unique<BVH>();
		bvh->build(*m);
		m_bvhs.push_back(std::move(bvh));
	}
}

bool Model::HasSurface() const
{
	return !m_bvhs.empty();
}

float Model::RayNearestSurface(const float orig[3], const float dir[3]) const
{
	float o[3] = { orig[0], orig[1], orig[2] };   // BVH::nearest prend vec3 (float*)
	float d[3] = { dir[0],  dir[1],  dir[2]  };
	float best = -1.f;
	for (const auto& bvh : m_bvhs)
	{
		const float t = bvh->nearest(o, d, 0.f);
		if (t >= 0.f && (best < 0.f || t < best))
			best = t;
	}
	return best;
}

const BoundingBox& Model::ComputeBBox()
{
	m_bbox = BoundingBox();   // repart d'une bbox vide
	for (Mesh* m : m_meshes.GetMeshes())
		if (m)
		{
			m->computebbox();
			m_bbox.AddBoundingBox(m->bbox());
		}
	return m_bbox;
}

Model* VModels::Add(const std::string& name)
{
	m_models.push_back(std::make_unique<Model>(name));
	return m_models.back().get();
}

bool VModels::Remove(std::size_t i)
{
	if (i >= m_models.size())
		return false;
	m_models.erase(m_models.begin() + i);
	return true;
}

void VModels::Clear()
{
	m_models.clear();
}

unsigned int VModels::GetNVertices() const
{
	unsigned int n = 0;
	for (const auto& m : m_models)
		n += m->m_meshes.GetNVertices();
	return n;
}

unsigned int VModels::GetNFaces() const
{
	unsigned int n = 0;
	for (const auto& m : m_models)
		n += m->m_meshes.GetNFaces();
	return n;
}

BoundingBox VModels::AggregateBBox(bool visibleOnly)
{
	BoundingBox bb;
	for (const auto& m : m_models)
	{
		if (visibleOnly && !m->m_visible)
			continue;
		m->ComputeBBox();
		bb.AddBoundingBox(m->m_bbox);
	}
	return bb;
}
