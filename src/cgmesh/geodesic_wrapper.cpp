#include "geodesic_wrapper.h"

#include "geodesic_algorithm_exact.h"
#include "geodesic_algorithm_dijkstra.h"
#include "geodesic_algorithm_subdivision.h"

GeodesicWrapper::GeodesicWrapper ()
{
	m_algorithm = NULL;
}

GeodesicWrapper::~GeodesicWrapper ()
{
	delete m_algorithm;
}

bool GeodesicWrapper::input (char *filename)
{
	std::vector<double> points;	
	std::vector<unsigned> faces;

	bool success = geodesic::read_mesh_from_file(filename, points, faces);
	if(!success)
		return false;

	//create internal mesh data structure including edges
	mesh.initialize_mesh_data(points, faces);

	return true;
}

bool GeodesicWrapper::SetMesh (unsigned int _nvertices, float *_vertices, unsigned int _nfaces, unsigned int *_faces)
{
	std::vector<double> points;	
	std::vector<unsigned> faces;

	points.resize(_nvertices*3);
	for(unsigned int i=0; i<3*_nvertices; ++i)
		points[i] = _vertices[i];

	faces.resize(_nfaces*3);
	for (unsigned int i=0; i<3*_nfaces; ++i)
		faces[i] = _faces[i];

	//create internal mesh data structure including edges
	mesh.initialize_mesh_data(points, faces);

	return true;
}

bool GeodesicWrapper::ComputeDistances (float *distances, geodesic::GeodesicAlgorithmBase::AlgorithmType algorithmType, int subdivision_level)
{
	switch (algorithmType)
	{
	case geodesic::GeodesicAlgorithmBase::EXACT:
		m_algorithm = new geodesic::GeodesicAlgorithmExact (&mesh);
		break;
	case geodesic::GeodesicAlgorithmBase::DIJKSTRA:
		m_algorithm = new geodesic::GeodesicAlgorithmDijkstra (&mesh);
		break;
	case geodesic::GeodesicAlgorithmBase::SUBDIVISION:
		m_algorithm = new geodesic::GeodesicAlgorithmSubdivision (&mesh, subdivision_level);
		break;
	default:
		break;
	}

	if (!distances)
		return false;

	m_algorithm->propagate(sources); //cover the whole mesh
	m_algorithm->print_statistics();

	unsigned int nv = mesh.vertices().size();
	for(unsigned int i=0; i<nv; ++i)
	{
		geodesic::SurfacePoint p(&mesh.vertices()[i]);		
		double distance;
		
		//for a given surface point, find closets source and distance to this source
		unsigned best_source = m_algorithm->best_source(p,distance);

		distances[i] = distance;
	}

/*
		//------------------first task: compute the pathes to the targets----
		std::vector<geodesic::SurfacePoint> path;
		for(int i=0; i<targets.size(); ++i)
		{
			algorithm->trace_back(targets[i], path);
			print_info_about_path(path);
		}

		//------------------second task: for each source, find the furthest vertex that it covers ----
		std::vector<double> max_distance(sources.size(), 0.0);		//distance to the furthest vertex that is covered by the given source
		for(int i=0; i<mesh.vertices().size(); ++i)
		{
			geodesic::SurfacePoint p(&mesh.vertices()[i]);
			double distance;
			unsigned best_source = algorithm->best_source(p,distance);

			max_distance[best_source] = std::max(max_distance[best_source], distance);
		}

		std::cout << std::endl;
		for(int i=0; i<max_distance.size(); ++i)
		{
			std::cout << "distance to the furthest vertex that is covered by source " << i 
					<< " is " << max_distance[i] 
					<< std::endl;
		}
*/

	return true;
}
