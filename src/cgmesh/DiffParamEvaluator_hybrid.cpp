#include "DiffParamEvaluator.h"
#include "../cgmath/context.h"

//
//
//
bool MeshAlgoTensorEvaluator::ApplyHybrid (const Context *ctx)
{
	int nv = m_pModel->m_pMesh->GetNVertices ();
	int i;

	// Local accumulation buffer (owns its tensors; freed at scope exit).
	// The shared per-vertex store is the mesh's tensor cache.
	std::vector<std::unique_ptr<Tensor>> hybrid (nv);
	for (i=0; i<nv; i++) hybrid[i] = std::make_unique<Tensor> ();

	/* normale and principal curvatures */
	// SEUL point de test du jeton pour cette methode : les trois boucles qui
	// suivent sont des recopies, et c'est ApplyDesbrun qui porte le calcul. Il
	// teste le jeton dans SA boucle sur les sommets et renonce ; il n'y a rien
	// a tester une seconde fois ici.
	if (!ApplyDesbrun (ctx))
		return false;
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
	// ApplySteiner a son corps entierement sous #if 0 : il ne modifie aucun
	// tenseur et rend true. La boucle qui suit relit donc les directions que
	// ApplyDesbrun vient d'ecrire -- l'hybride est, en l'etat, un Desbrun.
	if (!ApplySteiner (ctx))
		return false;
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
