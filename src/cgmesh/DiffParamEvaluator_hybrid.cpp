#include "DiffParamEvaluator.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHybrid (void)
{
	int nv = m_pModel->m_pMesh->m_nVertices;
	int i;

	// Local accumulation buffer (owns its tensors; freed at scope exit).
	// The shared per-vertex store is the mesh's Tensors().
	std::vector<std::unique_ptr<Tensor>> hybrid (nv);
	for (i=0; i<nv; i++) hybrid[i] = std::make_unique<Tensor> ();

	/* normale and principal curvatures */
	ApplyDesbrun ();
	for (i=0; i<nv; i++)
    {
		if (!Tensors ()[i])
		{
			hybrid[i] = nullptr;
			continue;
		}

		// Surcharges vecteur : la version `float*` de ces accesseurs indexait
		// `&n.x` comme un tableau de trois flottants (cpp:S3519, tensor.h:45/48).
		hybrid[i]->SetNormal (Tensors ()[i]->GetNormal ());
		hybrid[i]->SetKappaMax (Tensors ()[i]->GetKappaMax ());
		hybrid[i]->SetKappaMin (Tensors ()[i]->GetKappaMin ());
    }

	/* principal directions */
	ApplySteiner ();
	for (i=0; i<nv; i++)
    {
		if (!Tensors ()[i])
		{
			hybrid[i] = nullptr;
			continue;
		}

		hybrid[i]->SetDirectionMax (Tensors ()[i]->GetDirectionMax ());
		hybrid[i]->SetDirectionMin (Tensors ()[i]->GetDirectionMin ());
    }

	/* save the differential parameters */
	for (i=0; i<nv; i++)
    {
		if (!Tensors ()[i] || !hybrid[i])
		{
			Tensors ()[i] = nullptr;
			continue;
		}

		Tensors ()[i]->SetNormal (hybrid[i]->GetNormal ());
		Tensors ()[i]->SetKappaMax (hybrid[i]->GetKappaMax ());
		Tensors ()[i]->SetKappaMin (hybrid[i]->GetKappaMin ());

		Tensors ()[i]->SetDirectionMax (hybrid[i]->GetDirectionMax ());
		Tensors ()[i]->SetDirectionMin (hybrid[i]->GetDirectionMin ());
    }

	return true;
}
