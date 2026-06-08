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
		Vector3 n;
		if (!Tensors ()[i])
		{
			hybrid[i] = nullptr;
			continue;
		}

		Tensors ()[i]->GetNormal (n);
		hybrid[i]->SetNormal (n);
		hybrid[i]->SetKappaMax (Tensors ()[i]->GetKappaMax ());
		hybrid[i]->SetKappaMin (Tensors ()[i]->GetKappaMin ());
    }

	/* principal directions */
	ApplySteiner ();
	for (i=0; i<nv; i++)
    {
		Vector3 tmax, tmin;

		if (!Tensors ()[i])
		{
			hybrid[i] = nullptr;
			continue;
		}

		Tensors ()[i]->GetDirectionMax (tmax);
		hybrid[i]->SetDirectionMax (tmax.x, tmax.y, tmax.z);

		Tensors ()[i]->GetDirectionMin (tmin);
		hybrid[i]->SetDirectionMin (tmin.x, tmin.y, tmin.z);
    }

	/* save the differential parameters */
	for (i=0; i<nv; i++)
    {
		if (!Tensors ()[i] || !hybrid[i])
		{
			Tensors ()[i] = nullptr;
			continue;
		}

		Vector3 n;
		hybrid[i]->GetNormal (n);
		Tensors ()[i]->SetNormal (n);
		Tensors ()[i]->SetKappaMax (hybrid[i]->GetKappaMax ());
		Tensors ()[i]->SetKappaMin (hybrid[i]->GetKappaMin ());

		Vector3 tmax, tmin;

		hybrid[i]->GetDirectionMax (tmax);
		Tensors ()[i]->SetDirectionMax (tmax.x, tmax.y, tmax.z);

		hybrid[i]->GetDirectionMin (tmin);
		Tensors ()[i]->SetDirectionMin (tmin.x, tmin.y, tmin.z);
    }

	return true;
}
