#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_tensor, bunny)
{
	Mesh_half_edge* model = new Mesh_half_edge("./test/data/BunnyLowPoly.stl");
	if (model == NULL)
		return;

	model->ComputeNormals();


	//
	// Tensor
	//
	MeshAlgoTensorEvaluator* pTensorEvaluator = new MeshAlgoTensorEvaluator();
	pTensorEvaluator->Init(model);

	pTensorEvaluator->Evaluate(TENSOR_DESBRUN);

	pTensorEvaluator->Evaluate(TENSOR_GOLDFEATHER);

	pTensorEvaluator->Evaluate(TENSOR_HAMANN);

	pTensorEvaluator->Evaluate(TENSOR_STEINER);
}
