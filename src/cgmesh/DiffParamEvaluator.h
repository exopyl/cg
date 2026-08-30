#pragma once
#include "mesh_half_edge.h"

class Context;

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

	// ctx optionnel : le jeton est teste dans la boucle sur les sommets de
	// CHACUNE des methodes. Sur annulation, Evaluate rend false et n'estampille
	// PAS les tenseurs comme valides -- le maillage en porte donc de partiels,
	// que AreTensorsValid () signale comme perimes.
	bool Evaluate (TensorMethodId tensorMethodId, const Context *ctx = nullptr);

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

	bool ApplyTaubin (const Context *ctx);
	bool ApplyGoldfeather (const Context *ctx);
	bool ApplyHamann (const Context *ctx);
	bool ApplyDesbrun (const Context *ctx);

	void ApplySteinerAux (int index, float radius, int *_n_edges, int **_edges);
	bool ApplySteiner (const Context *ctx);

	bool ApplyHybrid (const Context *ctx);

	// Acces par indice au stockage de tenseurs du maillage modele. Valides
	// seulement apres Init(). TensorAt rend nullptr pour un indice hors bornes
	// ou pour un emplacement vide (sommet de bord / non manifold) ;
	// SetTensorAt prend possession de `t`, qui peut etre nullptr.
	Tensor* TensorAt (int index) { return m_pModel->m_pMesh->GetTensor ((unsigned int)index); }
	void SetTensorAt (int index, Tensor *t) { m_pModel->m_pMesh->SetTensor ((unsigned int)index, t); }
	int NTensors (void) const { return (int)m_pModel->m_pMesh->GetNTensors (); }

	// members
	Mesh_half_edge *m_pModel;
};
