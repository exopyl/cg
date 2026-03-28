#ifndef __MESH_DATA_MANAGER_H__
#define __MESH_DATA_MANAGER_H__

#include <map>
#include <memory>
#include <vector>
#include <stdint.h>
#include "mesh.h"
#include "mesh_half_edge.h"

/**
 * class MeshDataManager
 * 
 * Manages derived data from Mesh, such as Mesh_half_edge and topological issues,
 * with a revision-based invalidation system.
 */
class MeshDataManager
{
public:
    struct TopologicIssues
    {
        std::vector<unsigned int> nonManifoldEdges;
        std::vector<unsigned int> borders;
    };

private:
    struct CachedHalfEdge
    {
        std::unique_ptr<Mesh_half_edge> pHalfEdge;
        uint64_t revision;
    };

    struct CachedTopologicIssues
    {
        TopologicIssues issues;
        uint64_t revision;
    };

    struct CachedTriangles
    {
        std::vector<unsigned int> triangles;
        uint64_t revision;
    };

    std::map<Mesh*, CachedHalfEdge> m_halfEdges;
    std::map<Mesh*, CachedTopologicIssues> m_topologicIssues;
    std::map<Mesh*, CachedTriangles> m_triangles;
    std::map<Mesh*, int> m_vertexArrayIds;

    MeshDataManager() = default;
    ~MeshDataManager() = default;

public:
    static MeshDataManager& GetInstance()
    {
        static MeshDataManager instance;
        return instance;
    }

    int GetVertexArrayId(Mesh* pMesh);

    // Get a valid Mesh_half_edge for the given mesh.
    // If it doesn't exist or is outdated, it will be recomputed.
    Mesh_half_edge* GetHalfEdge(Mesh* pMesh)
    {
        if (!pMesh) return nullptr;

        uint64_t currentRevision = pMesh->GetRevision();
        auto it = m_halfEdges.find(pMesh);

        if (it != m_halfEdges.end())
        {
            if (it->second.revision == currentRevision)
            {
                return it->second.pHalfEdge.get();
            }
            // Outdated, recompute
            it->second.pHalfEdge = std::make_unique<Mesh_half_edge>(pMesh);
            it->second.revision = currentRevision;
            return it->second.pHalfEdge.get();
        }

        // Not found, create
        CachedHalfEdge cached;
        cached.pHalfEdge = std::make_unique<Mesh_half_edge>(pMesh);
        cached.revision = currentRevision;
        Mesh_half_edge* ptr = cached.pHalfEdge.get();
        m_halfEdges[pMesh] = std::move(cached);
        return ptr;
    }

    const TopologicIssues& GetTopologicIssues(Mesh* pMesh)
    {
        static TopologicIssues emptyIssues;
        if (!pMesh) return emptyIssues;

        uint64_t currentRevision = pMesh->GetRevision();
        auto it = m_topologicIssues.find(pMesh);

        if (it != m_topologicIssues.end() && it->second.revision == currentRevision)
        {
            return it->second.issues;
        }

        // Recompute
        CachedTopologicIssues cached;
        pMesh->GetTopologicIssues(cached.issues.nonManifoldEdges, cached.issues.borders);
        cached.revision = currentRevision;
        m_topologicIssues[pMesh] = std::move(cached);
        return m_topologicIssues[pMesh].issues;
    }

    const std::vector<unsigned int>& GetTriangles(Mesh* pMesh)
    {
        static std::vector<unsigned int> emptyTriangles;
        if (!pMesh) return emptyTriangles;

        uint64_t currentRevision = pMesh->GetRevision();
        auto it = m_triangles.find(pMesh);

        if (it != m_triangles.end() && it->second.revision == currentRevision)
        {
            return it->second.triangles;
        }

        // Recompute
        CachedTriangles cached;
        unsigned int* pTris = pMesh->GetTriangles();
        if (pTris)
        {
            cached.triangles.assign(pTris, pTris + 3 * pMesh->m_nFaces);
            free(pTris);
        }
        cached.revision = currentRevision;
        m_triangles[pMesh] = std::move(cached);
        return m_triangles[pMesh].triangles;
    }

    // Invalidate all cached data for a specific mesh
    void Invalidate(Mesh* pMesh)
    {
        m_halfEdges.erase(pMesh);
        m_topologicIssues.erase(pMesh);
        m_triangles.erase(pMesh);
    }

    // Clear all caches
    void Clear()
    {
        m_halfEdges.clear();
        m_topologicIssues.clear();
        m_triangles.clear();
        m_vertexArrayIds.clear();
    }
};

#endif // __MESH_DATA_MANAGER_H__
