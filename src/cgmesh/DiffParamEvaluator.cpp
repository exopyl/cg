#include <vector>

#include "DiffParamEvaluator.h"
#include <cgmath/context.h>
#include "../cgimg/color.h"

//
//
//
MeshAlgoTensorEvaluator::MeshAlgoTensorEvaluator ()
{
	m_pModel = nullptr;
}

//
//
//
MeshAlgoTensorEvaluator::~MeshAlgoTensorEvaluator ()
{
	Reset ();
}

//
//
//
void MeshAlgoTensorEvaluator::Reset (void)
{
	// The evaluator no longer owns the tensor buffer (it lives in the mesh,
	// owned via unique_ptr), so there is nothing to free here.
	m_pModel = nullptr;
}

//
//
//
bool MeshAlgoTensorEvaluator::Init (Mesh_half_edge *mesh)
{
	if (mesh == nullptr)
		return false;

	Reset ();
	m_pModel = mesh;

	// Prepare the mesh's per-vertex tensor storage: one default Tensor per
	// vertex. The Apply* methods overwrite each slot (a computed tensor, or
	// nullptr for border / non-manifold vertices).
	Mesh *pMesh = mesh->m_pMesh;
	pMesh->InitTensors ();
	const int n = (int)pMesh->GetNTensors ();
	for (int i = 0; i < n; i++)
		pMesh->SetTensor ((unsigned int)i, new Tensor ());
	return true;
}

//
// get
//
Tensor* MeshAlgoTensorEvaluator::GetDiffParam (int index)
{
	if (m_pModel == nullptr || index < 0)
		return nullptr;
	return TensorAt (index);
}

///////////////////////////////////
// stats
///////////////////////////////////


//
// extremal:
// 0 : minimal
// 1 : maximal
//
bool MeshAlgoTensorEvaluator::GetExtremalCurvature (CurvatureType id, int extremal, float *_curvature)
{
	if (m_pModel == nullptr) return false;

	int n = NTensors ();
	int i=0;
	float fCurvature=0.0;

	while (i<n && !TensorAt (i))
	{
		i++;
	}
	if (i == n) return false;

	// init
	fCurvature = TensorAt (i)->GetCurvature (id);

	//
	for (; i<n; i++)
	{
		if (!TensorAt (i)) continue;
		float fCurvatureTemp = TensorAt (i)->GetCurvature (id);

		switch (extremal)
		{
		case 0: // minimal
			fCurvature = (fCurvature > fCurvatureTemp)? fCurvatureTemp : fCurvature;
			break;
		case 1: // maximal
			fCurvature = (fCurvature > fCurvatureTemp)? fCurvature : fCurvatureTemp;
			break;
		default:
			break;
		}
	}

	*_curvature = fCurvature;
	return true;
}


//
//
//
bool MeshAlgoTensorEvaluator::GetCurvatures (CurvatureType id, int *_nCurvatures, float **_pCurvatures)
{
	if (m_pModel == nullptr) return false;

	int n = NTensors ();

	// init
	float *curvatures = (float*)malloc(n*sizeof(float));
	if (curvatures == nullptr) return false;
	int nCurvatures = 0;

	//
	for (int i=0; i<n; i++)
	{
		if (TensorAt (i))
			curvatures[nCurvatures++] = TensorAt (i)->GetCurvature (id);
	}

	*_nCurvatures = nCurvatures;
	*_pCurvatures = curvatures;

	return true;
}

//
//
//
bool MeshAlgoTensorEvaluator::GetCurvaturesHistogram (CurvatureType id, int nbins, float **_histogram)
{
	float min, max;
	int nCurvatures;
	float *curvatures = nullptr;

	GetExtremalCurvature (id, 0, &min);
	GetExtremalCurvature (id, 1, &max);
	GetCurvatures (id, &nCurvatures, &curvatures);

	// GetCurvatures alloue `curvatures` ; ce retour anticipe etait le SEUL des
	// quatre sorties de la fonction a ne pas le rendre
	// (cpp:S3584, DiffParamEvaluator.cpp:152).
	if (nCurvatures == 0) { if (curvatures) free (curvatures); return false; }

	// init : the histogram has nbins bins (NOT nCurvatures)
	float *histogram = (float*)malloc(nbins*sizeof(float));
	if (histogram == nullptr) { if (curvatures) free (curvatures); return false; }
	memset (histogram, 0, nbins*sizeof(float));

	// fill
	const float range = max - min;
	for (int i=0; i<nCurvatures; i++)
	{
		int index;
		if (range <= 0.f)               // all curvatures equal : single bin
			index = 0;
		else
		{
			float findex = (nbins-1) * (curvatures[i] - min) / range;
			index = (int) findex;
			if (index < 0) index = 0;                 // clamp against fp noise / out-of-range
			else if (index >= nbins) index = nbins-1;
		}
		histogram[index]++;
	}

	// normalize
	for (int i=0; i<nbins; i++)
	{
		histogram[i] /= (float)nCurvatures;
	}

	// cleaning
	if (curvatures) free (curvatures);

	// return
	*_histogram = histogram;

	return true;
}

//
//
//
void MeshAlgoTensorEvaluator::Dump (void)
{
	if (m_pModel == nullptr) return;
	int n = NTensors ();
	for (int i=0; i<n; i++)
	{
		printf ("%d / %d\n", i, n);
		if (TensorAt (i)) TensorAt (i)->Dump ();
	}
}

//
// Evaluation of the tensor
//
bool MeshAlgoTensorEvaluator::Evaluate (TensorMethodId tensorMethodId, const Context *ctx)
{
	// Le code de retour de la methode est desormais LU. Il ne l'etait pas :
	// Evaluate estampillait les tenseurs valides quoi qu'il arrive, si bien
	// qu'une methode ayant renonce laissait derriere elle des tenseurs partiels
	// reputes a jour. C'est aussi ce qui rend l'annulation observable.
	bool ok = false;
	switch (tensorMethodId)
	{
	case TENSOR_HAMANN:
		ok = ApplyHamann (ctx);
		break;
	case TENSOR_TAUBIN:
		ok = ApplyTaubin (ctx);
		break;
	case TENSOR_DESBRUN:
		ok = ApplyDesbrun (ctx);
		break;
	case TENSOR_STEINER:
		ok = ApplySteiner (ctx);
		break;
	case TENSOR_GOLDFEATHER:
		ok = ApplyGoldfeather (ctx);
		break;
	case TENSOR_HYBRID:
		ok = ApplyHybrid (ctx);
		break;
	default:
		return false;
	}
	if (!ok)
		return false;

	// The Apply* methods wrote the tensors directly into the mesh's storage.
	// Stamp them as valid for the mesh's current geometry revision so stale
	// tensors (after a later geometry edit) can be detected.
	m_pModel->m_pMesh->MarkTensorsComputed ();

	return true;
}



void MeshAlgoTensorEvaluator::EvaluateColors (CurvatureType type)
{
	int nv = m_pModel->m_pMesh->GetNVertices ();

	int i;
	float r, g, b;
	// Les deux etaient malloc'es et jamais rendus : la fonction n'a aucune
	// liberation (cpp:S3584, DiffParamEvaluator.cpp:351, deux fois).
	if (nv <= 0) return;
	std::vector<float> array ((size_t)nv, 0.0f);
	std::vector<int>   defined ((size_t)nv, 0);
	
	// build the array of the curvatures
	for (i=0; i<nv; i++)
	{
		if (!TensorAt (i))
		{
			defined[i] = 0;
			continue;
		}
		defined[i] = 1;
		array[i] = fabs (TensorAt (i)->GetCurvature (type));
	}
	//for (i=0; i<nv; i++)
	//array[i] = (array[i] > 1.0)? 1.0 : array[i];
	
	// get the extremal values
	float min_value = 0.0f, max_value = 0.0f;
	bool found = false;
	for (i=0; i<nv; i++)
	{
		if (defined[i])
		{
			if (!found)
			{
				min_value = array[i];
				max_value = array[i];
				found = true;
			}
			else
			{
				if (min_value > array[i]) min_value = array[i];
				if (max_value < array[i]) max_value = array[i];
			}
		}
	}
	printf ("   %f -> %f\n", min_value, max_value);
	//min_value = 0.0;
	//max_value = 1000.0;

	m_pModel->m_pMesh->InitVertexColors();
	for (i=0; i<nv; i++)
	{
		if (defined[i])
		{
			color_jet (array[i]/max_value, &r, &g, &b);
			m_pModel->m_pMesh->SetVertexColor (i, r, g, b);
		}
		else
		{
			m_pModel->m_pMesh->SetVertexColor (i, 0.0f, 0.0f, 0.0f);
		}
	}
}

// NOTE: ComparisonCurvatures / ComparisonDirections (and their helper
// get_colors_from_array) were removed. They compared a freshly computed
// approximation against a reference set of tensors held in a *second* buffer.
// Now that the evaluator computes directly into the mesh's single tensor
// store (no intermediate buffer), there is no second set to compare against,
// so the feature no longer has a place to stand. It had no callers.

