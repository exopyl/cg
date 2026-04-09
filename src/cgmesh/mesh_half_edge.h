#pragma once
#include "mesh.h"
#include "half_edge.h"
#include <memory>
#include <vector>

/**
* class Mesh_half_edge.
*
* Combines a Mesh (geometry) with a Che_mesh (half-edge topology) via composition.
*/
class Mesh_half_edge
{
	friend class MeshAlgoTensorEvaluator;

	friend class MeshAlgoSmoothingLaplacian;
	friend class MeshAlgoSmoothingTaubin;

	friend class MeshAlgoSubdivisionLoop;
	friend class MeshAlgoSubdivisionKarbacher;

	friend class SnakeList;
	friend class Cregions_vertices;
	friend class Cregions_faces;
	friend class Cmesh_orientation_pca;
	friend class Cset_lines;

	friend class Citerator_half_edge_vertex;

public:
	Mesh_half_edge ();//!<  Constructor
	Mesh_half_edge (int par_nv, float *par_v, int par_nf, unsigned int *par_f);
	Mesh_half_edge (Mesh *pMesh);
	Mesh_half_edge (const char *par_filename);//!<  Constructor
	~Mesh_half_edge ();//!< Destructor

	// Composition members
	Mesh     *m_pMesh;
	Che_mesh* GetCheMesh();

	void create_half_edge (void);
	int get_edge (unsigned int v1, unsigned int v2); // returns edge index, -1 if not found

	bool is_manifold (unsigned int i);
	bool is_border (unsigned int i);

	void export_statistics (const std::string & filename); // export statistics in html format

	////////////////////////////////////////////////////////////////////////////////
	//
	// half-edge operations (take edge index)
	//
	void edge_flip     (int par_edge);
	void edge_split    (int e);
	void edge_contract (int e);
	//
	////////////////////////////////////////////////////////////////////////////////

	// Simplification
	void simplify (void);

	// Dijkstra
public:
	void dijkstra_shortest_path (int par_source, int par_target, int *par_n, int **par_path);

	// find the local maximal in a neighborough
	void search_maximal (float *par_fdata, float par_angle_max);

	// misc
	float edge_length (int edge);  // takes edge index
	float get_average_edges_length (void); //<! Get the average edges length.
	float get_shortest_edge_length (void); //<! Get the shortest edge length.
	int   get_n_neighbours (int par_ivertex); //<! Get the number of neighbours around vertex identified by par_ivertex.
	double cotangent_weight_formula(int edge); // takes edge index

	void dump (void);

private:
	std::unique_ptr<Che_mesh> m_pCheMesh;

	// topology
	void check_topology (void);
	std::vector<char> m_topology_ok;

	// border
	void check_border (void); // check_topology should be called before
	std::vector<char> m_border;
};
