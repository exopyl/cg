#pragma once
#include "mesh_half_edge.h"

//
//
//

//! Methods to evaluate the tensor.
//! The following methods are implemented.
enum TensorMethodId {	TENSOR_HAMANN,		//!< Hamann, "Curvature Approximation for Triangulated Surfaces", 1993
			TENSOR_TAUBIN,		//!< Taubin, "Estimating The Tensor of Curvature of a Surface From a Polyhedral Approximation", 1999
			TENSOR_DESBRUN,		//!< Meyer, Desbrun, Schroder, Barr, "Discrete Differential-Geometry Operators for Triangulated 2-Manifolds", 2000
			TENSOR_STEINER,		//!< Steiner, 2003
			TENSOR_GOLDFEATHER, //!< Goldfeather, Interrante, "A Novel Cubic-Order Algorithm for Approximating Principal Directions Vectors", 2004
			TENSOR_HYBRID		//!< Method mixing the previous methods
};



//
// TensorEvaluator
//
class MeshAlgoTensorEvaluator
{
public:
	MeshAlgoTensorEvaluator ();
	~MeshAlgoTensorEvaluator ();

	bool Init (Mesh_half_edge *model);
	bool Evaluate (TensorMethodId tensorMethodId);

	void Dump ();

	// get
	Tensor* GetDiffParam (int index);

	bool GetExtremalCurvature (CurvatureType id, int extremal, float *curvature);
	bool GetCurvatures (CurvatureType id, int *nCurvatures, float **pCurvatures);
	bool GetCurvaturesHistogram (CurvatureType id, int nbins, float **histogram);

	//
	// colors
	//
	void EvaluateColors (CurvatureType type);

private:
	// methods
	void Reset (void);

	bool ApplyTaubin (void);
	bool ApplyGoldfeather (void);
	bool ApplyHamann (void);
	bool ApplyDesbrun (void);

	void ApplySteinerAux (int index, float radius, int *_n_edges, int **_edges);
	bool ApplySteiner (void);

	bool ApplyHybrid (void);

	// Non-owning view onto the model mesh's per-vertex tensor storage.
	// All the Apply* methods write through this; valid only after Init().
	std::vector<std::unique_ptr<Tensor>> & Tensors (void) const { return m_pModel->m_pMesh->m_pTensors; }
	int NTensors (void) const { return (int)m_pModel->m_pMesh->m_pTensors.size (); }

	// members
	Mesh_half_edge *m_pModel;
};
