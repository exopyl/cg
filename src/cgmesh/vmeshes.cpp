#include "vmeshes.h"

VMeshes::VMeshes ()
{
}

VMeshes::~VMeshes ()
{
	clean ();
}

unsigned int VMeshes::GetNVertices() const
{
	unsigned int n = 0;
	for (auto pMesh : m_Meshes)
		n += pMesh->GetNVertices();
	return n;
}

unsigned int VMeshes::GetNFaces() const
{
	unsigned int n = 0;
	for (auto pMesh : m_Meshes)
		n += pMesh->GetNFaces();
	return n;
}

size_t VMeshes::GetNMeshes() const
{
	return m_Meshes.size();
}

bool VMeshes::IsTriangleMesh() const
{
	for (auto pMesh : m_Meshes)
		if (!pMesh->IsTriangleMesh())
			return false;
	return true;
}

void VMeshes::Normalize()
{
	BoundingBox bbox;
	for (const auto& mesh : GetMeshes())
	{
		if (mesh)
		{
			mesh->computebbox();
			bbox.AddBoundingBox(mesh->bbox());
		}
	}

	float center[3];
	bbox.GetCenter(center);
	float fLargestLength = bbox.GetLargestLength();
	for (const auto& mesh : GetMeshes())
	{
		if (mesh)
		{
			mesh->translate(-center[0], -center[1], -center[2]);
			auto l = (fLargestLength == 0.f) ? 1.f : fLargestLength;
			mesh->scale(1.f / l);
			mesh->IncrementRevision();
		}
	}
}

void VMeshes::clean (void)
{
	for (auto pMesh : m_Meshes)
		delete pMesh;
	m_Meshes.clear();
}
