#ifndef __MESH_HALF_EDGE_H__
#define __MESH_HALF_EDGE_H__

#include "mesh.h"
#include "half_edge.h"

/**
* class Mesh_half_edge.
*/
class Mesh_half_edge : public Mesh, public Che_mesh
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

	void create_half_edge (void);
	Che_edge *get_edge (unsigned int v1, unsigned int v2);

	inline int is_manifold (int i) { check_topology (); return m_topology_ok[i]; };

	void export_statistics (char *filename); // export statistics in html format

	////////////////////////////////////////////////////////////////////////////////
	//
	// methods inherited from Che_mesh
	//
	void edge_flip     (Che_edge *par_edge);
	void edge_split    (Che_edge *e);
	void edge_contract (Che_edge *e);
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
	float edge_length (Che_edge *edge);
	float get_average_edges_length (void); //<! Get the average edges length.
	float get_shortest_edge_length (void); //<! Get the shortest edge length.
	int   get_n_neighbours (int par_ivertex); //<! Get the number of neighbours around vertex identified by par_ivertex.
	double cotangent_weight_formula(Che_edge *edge);

	void dump (void);
	
private:
	// topology
	void check_topology (void);
	char *m_topology_ok;
	
	// border
	void check_border (void); // check_topology should be called before
	char *m_border;
};

#endif // __MESH_HALF_EDGE_H__
