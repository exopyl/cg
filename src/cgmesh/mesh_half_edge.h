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

	// Options controlling what the QEM decimation preserves. Defaults keep the
	// most faithful result; pass an instance to relax individual guarantees.
	struct SimplifyOptions
	{
		// When true, sharp interior edges (dihedral angle above
		// feature_angle_deg) and material seams get perpendicular constraint
		// quadrics so collapses keep them in place (Garland-Heckbert boundary
		// constraints). Mesh borders are always kept (the collapse primitive
		// cannot remove them), independently of this flag.
		bool preserve_features = true;

		// Crease detection threshold, in degrees (used iff preserve_features).
		float feature_angle_deg = 45.0f;

		// When true, per-vertex colours and normals are interpolated at each
		// collapse and carried into the result instead of being recomputed/
		// dropped (appearance-preserving decimation, Voie A). Per-corner UVs
		// are not transported (deferred to a wedge layer).
		bool preserve_attributes = true;

		// Voie B (Hoppe / Garland-Heckbert 1998): when true AND the mesh has
		// per-vertex colours, decimation uses a generalized quadric over
		// R^6 = position(3) + colour(3). Attribute error then drives both the
		// collapse cost and the optimal vertex (position+colour fall out of the
		// minimization), so colour discontinuities are preserved by the metric
		// itself. Falls back to Voie A when there are no colours.
		// Per-corner UVs still require a wedge layer and are not handled here.
		bool attribute_metric = false;

		// Relative weight of the colour error versus the geometric error in the
		// R^6 quadric (used iff attribute_metric). Larger => colour preserved
		// more aggressively at the expense of geometric fidelity.
		float attribute_weight = 1.0f;

		// Error-bounded stopping criterion, as a fraction of the bbox diagonal
		// (0 = disabled). Decimation stops as soon as the cheapest remaining
		// collapse would exceed this error. This uses the QEM cost as a cheap
		// PROXY for surface deviation (cost ~ squared distance to the supporting
		// planes), NOT a guaranteed Hausdorff bound; measure the achieved error
		// with mesh_hausdorff() (mesh_metrics.h). Applies to the geometric path
		// only (ignored when attribute_metric is on, where the cost mixes
		// geometry and attributes). Combined with target_ratio: whichever stops
		// first wins.
		float max_error = 0.0f;

		// Voie B (UV wedges): when true, texture coordinates are preserved
		// through decimation. The mesh is first made UV-vertex-parallel via
		// SplitVerticesByUVSeams() (a wedge -> duplicated vertices), so UV
		// seams become topological borders that the collapse primitive keeps
		// frozen; the per-vertex UV is then interpolated at each collapse and
		// emitted on the result. Per-corner UV is not added to the nD quadric
		// (that R^6->R^8 extension is a further step). No effect without UVs.
		bool preserve_uv = false;

		// How max_error is enforced:
		//   false (default) -> cheap QEM PROXY (cost ~ distance to local support
		//     planes). Fast, conservative, but not a guaranteed surface bound.
		//   true -> RELIABLE surface bound: a BVH is built over the ORIGINAL
		//     mesh and every collapse whose new vertex would lie farther than
		//     max_error*diagonal from the original surface is REJECTED (true
		//     point-to-surface distance). Costs one closest-point query per
		//     attempted collapse. Bounds the deviation of the decimated VERTICES
		//     from the original surface — a real surface-relative bound, not a
		//     full two-sided Hausdorff envelope (triangle interiors and the
		//     reverse direction are not gated). No effect when max_error == 0.
		bool exact_error = false;
	};

	// Simplification (QEM edge-collapse decimation).
	// target_ratio in [0,1]: fraction of the original face count to keep
	// (0.5 -> halve the faces). Replaces m_pMesh with a compacted result.
	void simplify (float target_ratio = 0.5f,
	               const SimplifyOptions &options = SimplifyOptions());

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
