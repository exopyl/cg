#include "mesh_data_manager.h"
#include "../cgre/mesh_renderer.h"

int MeshDataManager::GetVertexArrayId(Mesh* pMesh)
{
    if (!pMesh) return -1;

    auto it = m_vertexArrayIds.find(pMesh);
    if (it != m_vertexArrayIds.end())
    {
        return it->second;
    }

    // Register mesh in VertexArray mode
    int id = MeshRenderer::getInstance()->AddMesh(pMesh, CG_RENDERING_VERTEX_ARRAY);
    m_vertexArrayIds[pMesh] = id;
    return id;
}
