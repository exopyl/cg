#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHybrid (void)
{
	int nv = m_pModel->m_pMesh->GetNVertices ();
	int i;

	// Local accumulation buffer (owns its tensors; freed at scope exit).
	// The shared per-vertex store is the mesh's tensor cache.
	std::vector<std::unique_ptr<Tensor>> hybrid (nv);
	for (i=0; i<nv; i++) hybrid[i] = std::make_unique<Tensor> ();

	/* normale and principal curvatures */
	ApplyDesbrun ();
	for (i=0; i<nv; i++)
    {
		if (!TensorAt (i))
		{
			hybrid[i] = nullptr;
			continue;
		}

		// Surcharges vecteur : la version `float*` de ces accesseurs indexait
		// `&n.x` comme un tableau de trois flottants (cpp:S3519, tensor.h:45/48).
		hybrid[i]->SetNormal (TensorAt (i)->GetNormal ());
		hybrid[i]->SetKappaMax (TensorAt (i)->GetKappaMax ());
		hybrid[i]->SetKappaMin (TensorAt (i)->GetKappaMin ());
    }

	/* principal directions */
	ApplySteiner ();
	for (i=0; i<nv; i++)
    {
		if (!TensorAt (i))
		{
			hybrid[i] = nullptr;
			continue;
		}

		hybrid[i]->SetDirectionMax (TensorAt (i)->GetDirectionMax ());
		hybrid[i]->SetDirectionMin (TensorAt (i)->GetDirectionMin ());
    }

	/* save the differential parameters */
	for (i=0; i<nv; i++)
    {
		if (!TensorAt (i) || !hybrid[i])
		{
			SetTensorAt (i, nullptr);
			continue;
		}

		TensorAt (i)->SetNormal (hybrid[i]->GetNormal ());
		TensorAt (i)->SetKappaMax (hybrid[i]->GetKappaMax ());
		TensorAt (i)->SetKappaMin (hybrid[i]->GetKappaMin ());

		TensorAt (i)->SetDirectionMax (hybrid[i]->GetDirectionMax ());
		TensorAt (i)->SetDirectionMin (hybrid[i]->GetDirectionMin ());
    }

	return true;
}
