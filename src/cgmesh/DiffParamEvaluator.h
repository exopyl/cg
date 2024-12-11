#ifndef __DIFF_PARAM_EVALUATOR_H__
#define __DIFF_PARAM_EVALUATOR_H__

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

//! Curvatures type.
//! The following methods are implemented.
enum CurvatureId {	CURVATURE_MAX,		//!< Maximal curvature
			CURVATURE_MIN,		//!< Minimal curvature
			CURVATURE_MEAN,		//!< Mean curvature
			CURVATURE_GAUSSIAN	//!< Gaussian curvature
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

	bool GetExtremalCurvature (CurvatureId id, int extremal, float *curvature);
	bool GetCurvatures (CurvatureId id, int *nCurvatures, float **pCurvatures);
	bool GetCurvaturesHistogram (CurvatureId id, int nbins, float **histogram);

	//
	// colors
	//
	void EvaluateColors (CurvatureId type);

	float* ComparisonCurvatures (CurvatureId type);
	float* ComparisonDirections (void);

private:
	// methods
	void Reset (void);

	bool ApplyTaubin (void);
	bool ApplyGoldfeather (void);
	bool ApplyHamann (void);
	bool ApplyDesbrun (void);

	void ApplySteinerAux (int index, float radius, int *_n_edges, Che_edge ***_edges);
	bool ApplySteiner (void);

	bool ApplyHybrid (void);
	
	// members
	Mesh_half_edge *m_pModel;
	int m_nDiffParams;
	Tensor **m_pDiffParams;
};

#endif // __DIFF_PARAM_EVALUATOR_H__
