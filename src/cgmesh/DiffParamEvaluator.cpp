#include "DiffParamEvaluator.h"

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
	// vertex. The Apply* methods overwrite each slot (reset() to a computed
	// tensor, or = nullptr for border / non-manifold vertices).
	auto& tensors = mesh->m_pMesh->m_pTensors;
	tensors.clear ();
	tensors.resize (mesh->m_pMesh->m_nVertices);
	for (auto& t : tensors)
		t = std::make_unique<Tensor> ();
	return true;
}

//
// get
//
Tensor* MeshAlgoTensorEvaluator::GetDiffParam (int index)
{
	if (m_pModel && index >= 0 && index < NTensors ())
		return Tensors ()[index].get ();
	return nullptr;
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

	while (i<n && !Tensors ()[i])
	{
		i++;
	}
	if (i == n) return false;

	// init
	fCurvature = Tensors ()[i]->GetCurvature (id);

	//
	for (; i<n; i++)
	{
		if (!Tensors ()[i]) continue;
		float fCurvatureTemp = Tensors ()[i]->GetCurvature (id);

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
		if (Tensors ()[i])
			curvatures[nCurvatures++] = Tensors ()[i]->GetCurvature (id);
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

	if (nCurvatures == 0) return false;

	// init
	float *histogram = (float*)malloc(nCurvatures*sizeof(float));
	if (histogram == nullptr) return false;
	memset (histogram, 0, nbins*sizeof(float));

	// fill
	float findex;
	int index;
	for (int i=0; i<nCurvatures; i++)
	{
		findex = (nbins-1) * (curvatures[i] - min) / (max - min);
		index = (int) findex;
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
		if (Tensors ()[i]) Tensors ()[i]->Dump ();
	}
}

//
// Evaluation of the tensor
//
bool MeshAlgoTensorEvaluator::Evaluate (TensorMethodId tensorMethodId)
{
	switch (tensorMethodId)
	{
	case TENSOR_HAMANN:
		{
			ApplyHamann ();
		}
		break;
	case TENSOR_TAUBIN:
		{
			ApplyTaubin ();
		}
		break;
	case TENSOR_DESBRUN:
		{
			ApplyDesbrun ();
		}
		break;
	case TENSOR_STEINER:
		{
			ApplySteiner ();
		}
		break;
	case TENSOR_GOLDFEATHER:
		{
			ApplyGoldfeather ();
		}
		break;
	default:
		return false;
		break;
	}

	// The Apply* methods wrote the tensors directly into the mesh's storage.
	// Stamp them as valid for the mesh's current geometry revision so stale
	// tensors (after a later geometry edit) can be detected.
	m_pModel->m_pMesh->MarkTensorsComputed ();

	return true;
}



/* jet (inspired by MatLab) */
/*
*'red':   ((0., 0, 0), (0.35, 0, 0), (0.66, 1, 1), (0.89,1, 1), (1, 0.5, 0.5)),
*'green': ((0., 0, 0), (0.125,0, 0), (0.375,1, 1), (0.64,1, 1),(0.91,0,0), (1, 0, 0)),   
*'blue':  ((0., 0.5, 0.5), (0.11, 1, 1), (0.34, 1, 1), (0.65,0, 0), (1, 0, 0))}
*/
static float ri[5] = {0, 0.35, 0.66, 0.89, 1};
static float rv[5] = {0, 0, 1, 1, 0.5};
static float gi[6] = {0, 0.125, 0.375, 0.64, 0.91, 1};
static float gv[6] = {0, 0, 1, 1, 0, 0};
static float bi[5] = {0, 0.11, 0.34, 0.65, 1};
static float bv[5] = {0.5, 1, 1, 0, 0};

void
static get_jet_color (float index, float *r, float *g, float *b)
{
	int j;
	// red
	for (j=1; j<5; j++) if (ri[j] > index) break;
	*r = ((rv[j]-rv[j-1])*index+rv[j-1]*ri[j]-rv[j]*ri[j-1])/(ri[j]-ri[j-1]);
	
	// green
	for (j=1; j<5; j++) if (gi[j] > index) break;
	*g = ((gv[j]-gv[j-1])*index+gv[j-1]*gi[j]-gv[j]*gi[j-1])/(gi[j]-gi[j-1]);
	
	// blue
	for (j=1; j<5; j++) if (bi[j] > index) break;
	*b = ((bv[j]-bv[j-1])*index+bv[j-1]*bi[j]-bv[j]*bi[j-1])/(bi[j]-bi[j-1]);
}

void MeshAlgoTensorEvaluator::EvaluateColors (CurvatureType type)
{
	int nv = m_pModel->m_pMesh->m_nVertices;

	int i;
	float r, g, b;
	float *array = (float*)malloc(nv*sizeof(float));
	int *defined = (int*)malloc(nv*sizeof(int));
	
	// build the array of the curvatures
	for (i=0; i<nv; i++)
	{
		if (!Tensors ()[i])
		{
			defined[i] = 0;
			continue;
		}
		defined[i] = 1;
		array[i] = fabs (Tensors ()[i]->GetCurvature (type));
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
			get_jet_color (array[i]/max_value, &r, &g, &b);
			m_pModel->m_pMesh->m_pVertexColors[3*i]   = r;
			m_pModel->m_pMesh->m_pVertexColors[3*i+1] = g;
			m_pModel->m_pMesh->m_pVertexColors[3*i+2] = b;
		}
		else
		{
			m_pModel->m_pMesh->m_pVertexColors[3*i]   = 0.0;
			m_pModel->m_pMesh->m_pVertexColors[3*i+1] = 0.0;
			m_pModel->m_pMesh->m_pVertexColors[3*i+2] = 0.0;
		}
	}
}

// NOTE: ComparisonCurvatures / ComparisonDirections (and their helper
// get_colors_from_array) were removed. They compared a freshly computed
// approximation against a reference set of tensors held in a *second* buffer.
// Now that the evaluator computes directly into the mesh's single tensor
// store (no intermediate buffer), there is no second set to compare against,
// so the feature no longer has a place to stand. It had no callers.

