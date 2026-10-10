#include "topology_stats.h"

#include "mesh.h"
#include "mesh_data_manager.h"
#include "vmodels.h"

namespace peles
{
namespace core
{

TopologyStats ComputeTopologyStats (Model& model)
{
	TopologyStats stats;

	for (Mesh* mesh : model.m_meshes.GetMeshes ())
	{
		if (mesh == nullptr)
			continue;

		// LE CACHE PAR REVISION DE cgmesh EST LA SEULE SOURCE : recalculer ici
		// donnerait un second chemin de calcul, donc deux resultats possibles
		// pour la meme question.
		const MeshDataManager::TopologicIssues& issues =
		    MeshDataManager::GetInstance ().GetTopologicIssues (mesh);
		stats.borders += (unsigned int)issues.borders.size () / 2;
		stats.nonManifoldEdges += (unsigned int)issues.nonManifoldEdges.size () / 2;
	}

	return stats;
}

} // namespace core
} // namespace peles
