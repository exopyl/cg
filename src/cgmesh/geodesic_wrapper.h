#ifndef __GEODESIC_WRAPPER_H__
#define __GEODESIC_WRAPPER_H__

//
// wrapper for the code found here :
// https://code.google.com/p/geodesic/
//
#include "geodesic_algorithm_base.h"

class GeodesicWrapper
{
public:
	GeodesicWrapper ();
	~GeodesicWrapper ();

	bool input (char *filename);
	bool SetMesh (unsigned int nvertices, float *vertices, unsigned int nfaces, unsigned int *faces);
	bool OuputDistances (float *distances);

	inline unsigned int GetNVertices (void) { return mesh.vertices().size(); };

	inline bool AddSource (unsigned int source_vertex_index)
		{
			sources.push_back(geodesic::SurfacePoint(&mesh.vertices()[source_vertex_index]));
			return true;
		}
	inline bool AddTarget (unsigned int target_vertex_index)
		{
			targets.push_back(geodesic::SurfacePoint(&mesh.vertices()[target_vertex_index]));
			return true;
		}
/*
	std::vector<geodesic::SurfacePoint> sources;
	sources.push_back(geodesic::SurfacePoint(&mesh.vertices()[0]));		//one source is located at vertex zero
	sources.push_back(geodesic::SurfacePoint(&mesh.edges()[12]));		//second source is located in the middle of edge 12
	sources.push_back(geodesic::SurfacePoint(&mesh.faces()[20]));		//third source is located in the middle of face 20

	std::vector<geodesic::SurfacePoint> targets;		//same thing with targets
	targets.push_back(geodesic::SurfacePoint(&mesh.vertices().back()));		
	targets.push_back(geodesic::SurfacePoint(&mesh.edges()[10]));		
	targets.push_back(geodesic::SurfacePoint(&mesh.faces()[3]));		
*/
	bool ComputeDistances (float *distances, geodesic::GeodesicAlgorithmBase::AlgorithmType algorithmType, int subdivision_level = 2);

	geodesic::Mesh mesh;
private:
	//geodesic::GeodesicAlgorithmExact *m_algorithm;
	geodesic::GeodesicAlgorithmBase *m_algorithm;
	std::vector<geodesic::SurfacePoint> sources;
	std::vector<geodesic::SurfacePoint> targets;
};

#endif // __GEODESIC_WRAPPER_H__
