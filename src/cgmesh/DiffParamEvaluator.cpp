#include "DiffParamEvaluator.h"

//
//
//
MeshAlgoTensorEvaluator::MeshAlgoTensorEvaluator ()
{
	m_nDiffParams = 0;
	m_pDiffParams = NULL;
	m_pModel = NULL;
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
	if (m_pDiffParams)
	{
		for (int i=0; i<m_nDiffParams; i++)
		{
			if (m_pDiffParams[i])
			{
				delete m_pDiffParams[i];
				m_pDiffParams[i] = NULL;
			}
		}
		delete (m_pDiffParams);
		m_pDiffParams = NULL;
	}
	m_nDiffParams = 0;
	m_pModel = NULL;
}

//
//
//
bool MeshAlgoTensorEvaluator::Init (Mesh_half_edge *mesh)
{
	if (mesh == NULL)
		return false;

	Reset ();
	m_pModel = mesh;
	m_nDiffParams = mesh->m_nVertices;
	m_pDiffParams = (Tensor**)malloc(m_nDiffParams*sizeof(Tensor*));
	if (m_pDiffParams == NULL)
	{
		Reset ();
		return false;
	}
	for (int i=0; i<m_nDiffParams; i++)
	{
		m_pDiffParams[i] = new Tensor ();
	}
	return true;
}

//
// get
//
Tensor* MeshAlgoTensorEvaluator::GetDiffParam (int index)
{
	if (index >=0 && index < m_nDiffParams && m_pDiffParams)
	{
		return m_pDiffParams[index];
	}
	return NULL;
}

///////////////////////////////////
// stats
///////////////////////////////////


//
// extremal:
// 0 : minimal
// 1 : maximal
//
bool MeshAlgoTensorEvaluator::GetExtremalCurvature (CurvatureId id, int extremal, float *_curvature)
{
	if (m_pDiffParams == NULL) return false;

	int i=0;
	float fCurvature=0.0;
	
	while (m_pDiffParams[i] == 0 && i<m_nDiffParams)
	{
		i++;
	}
	if (i == m_nDiffParams) return false;

	// init
	switch (id)
	{
	case CURVATURE_MAX:
		fCurvature = m_pDiffParams[i]->GetKappaMax ();
		break;
	case CURVATURE_MIN:
		fCurvature = m_pDiffParams[i]->GetKappaMin ();
		break;
	case CURVATURE_MEAN:
		fCurvature = (m_pDiffParams[i]->GetKappaMax () + m_pDiffParams[i]->GetKappaMin ()) / 2.0;
		break;
	case CURVATURE_GAUSSIAN:
		fCurvature = (m_pDiffParams[i]->GetKappaMax () * m_pDiffParams[i]->GetKappaMin ());
		break;
	default:
		break;
	}

	//
	for (i; i<m_nDiffParams; i++)
	{
		if (m_pDiffParams[i] == NULL) continue;
		float fCurvatureTemp;

		switch (id)
		{
		case CURVATURE_MAX:
			fCurvatureTemp = m_pDiffParams[i]->GetKappaMax ();
			break;
		case CURVATURE_MIN:
			fCurvatureTemp = m_pDiffParams[i]->GetKappaMin ();
			break;
		case CURVATURE_MEAN:
			fCurvatureTemp = (m_pDiffParams[i]->GetKappaMax () + m_pDiffParams[i]->GetKappaMin ()) / 2.0;
			break;
		case CURVATURE_GAUSSIAN:
			fCurvatureTemp = (m_pDiffParams[i]->GetKappaMax () * m_pDiffParams[i]->GetKappaMin ());
			break;
		default:
			break;
		}

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
bool MeshAlgoTensorEvaluator::GetCurvatures (CurvatureId id, int *_nCurvatures, float **_pCurvatures)
{
	if (m_pDiffParams == NULL) return false;

	// init
	float *curvatures = (float*)malloc(m_nDiffParams*sizeof(float));
	if (curvatures == NULL) return false;
	int nCurvatures = 0;

	//
	for (int i=0; i<m_nDiffParams; i++)
	{
		if (m_pDiffParams[i] != NULL)
		{
			switch (id)
			{
			case CURVATURE_MAX:
				curvatures[nCurvatures++] = m_pDiffParams[i]->GetKappaMax ();
				break;
			case CURVATURE_MIN:
				curvatures[nCurvatures++] = m_pDiffParams[i]->GetKappaMin ();
				break;
			case CURVATURE_MEAN:
				curvatures[nCurvatures++] = (m_pDiffParams[i]->GetKappaMax () + m_pDiffParams[i]->GetKappaMin ()) / 2.0;
				break;
			case CURVATURE_GAUSSIAN:
				curvatures[nCurvatures++] = (m_pDiffParams[i]->GetKappaMax () * m_pDiffParams[i]->GetKappaMin ());
				break;
			default:
				break;
			}
			
		}
	}

	*_nCurvatures = nCurvatures;
	*_pCurvatures = curvatures;

	return true;
}

//
//
//
bool MeshAlgoTensorEvaluator::GetCurvaturesHistogram (CurvatureId id, int nbins, float **_histogram)
{
	float min, max;
	int nCurvatures;
	float *curvatures = NULL;

	switch (id)
	{
	case CURVATURE_MAX:
		GetExtremalCurvature (CURVATURE_MAX, 0, &min);
		GetExtremalCurvature (CURVATURE_MAX, 1, &max);
		GetCurvatures (CURVATURE_MAX, &nCurvatures, &curvatures);
		break;
	case CURVATURE_MIN:
		GetExtremalCurvature (CURVATURE_MIN, 0, &min);
		GetExtremalCurvature (CURVATURE_MIN, 1, &max);
		GetCurvatures (CURVATURE_MIN, &nCurvatures, &curvatures);
		break;
	case CURVATURE_MEAN:
		GetExtremalCurvature (CURVATURE_MEAN, 0, &min);
		GetExtremalCurvature (CURVATURE_MEAN, 1, &max);
		GetCurvatures (CURVATURE_MEAN, &nCurvatures, &curvatures);
		break;
	case CURVATURE_GAUSSIAN:
		GetExtremalCurvature (CURVATURE_GAUSSIAN, 0, &min);
		GetExtremalCurvature (CURVATURE_GAUSSIAN, 1, &max);
		GetCurvatures (CURVATURE_GAUSSIAN, &nCurvatures, &curvatures);
		break;
	}

	if (nCurvatures == 0) return false;

	// init
	float *histogram = (float*)malloc(nCurvatures*sizeof(float));
	if (histogram == NULL) return false;
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
	for (int i=0; i<m_nDiffParams; i++)
	{
		printf ("%d / %d\n", i, m_nDiffParams);
		if (m_pDiffParams[i]) m_pDiffParams[i]->Dump ();
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
			//ApplySteiner ();
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
	
	m_pModel->m_pTensors = m_pDiffParams;

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

void MeshAlgoTensorEvaluator::EvaluateColors (CurvatureId type)
{
	int nv = m_pModel->m_nVertices;
	float *v = m_pModel->m_pVertices;
	float *vn = m_pModel->m_pVertexNormals;
	Che_edge** m_edges_vertex = m_pModel->m_edges_vertex;

	int i;
	float r, g, b;
	float *array = (float*)malloc(nv*sizeof(float));
	int *defined = (int*)malloc(nv*sizeof(int));
	
	// build the array of the curvatures
	for (i=0; i<nv; i++)
	{
		if (!m_pDiffParams[i])
		{
			defined[i] = 0;
			continue;
		}
		defined[i] = 1;
		float kappa1 = m_pDiffParams[i]->GetKappaMax ();
		float kappa2 = m_pDiffParams[i]->GetKappaMin ();
		switch (type)
		{
		case CURVATURE_GAUSSIAN:
			array[i] = fabs (kappa1*kappa2);
			break;
		case CURVATURE_MEAN:
			array[i] = fabs ((kappa1+kappa2)/2.0);
			break;
		case CURVATURE_MAX:
			array[i] = fabs (kappa1);
			break;
		case CURVATURE_MIN:
			array[i] = fabs (kappa2);
			break;
		default:
			printf ("type unknown\n");
			break;
		}
	}
	//for (i=0; i<nv; i++)
	//array[i] = (array[i] > 1.0)? 1.0 : array[i];
	
	// get the extremal values
	float min_value, max_value;
	for (i=0; i<nv; i++)
	{
		if (defined[i])
		{
			min_value = array[i];
			max_value = array[i];
			break;
		}
	}
	for (i=0; i<nv; i++)
	{
		if (!defined[i]) continue;
		//printf ("array[%d] = %f\n", i, array[i]);
		min_value = (min_value > array[i])? array[i] : min_value;
		max_value = (max_value < array[i])? array[i] : max_value;
	}
	printf ("   %f -> %f\n", min_value, max_value);
	//min_value = 0.0;
	//max_value = 1000.0;
	
	m_pModel->InitVertexColors();
	for (i=0; i<nv; i++)
	{
		if (defined[i])
		{
			get_jet_color (array[i]/max_value, &r, &g, &b);
			m_pModel->m_pVertexColors[3*i]   = r;
			m_pModel->m_pVertexColors[3*i+1] = g;
			m_pModel->m_pVertexColors[3*i+2] = b;
		}
		else
		{
			m_pModel->m_pVertexColors[3*i]   = 0.0;
			m_pModel->m_pVertexColors[3*i+1] = 0.0;
			m_pModel->m_pVertexColors[3*i+2] = 0.0;
		}
	}
}

static float* get_colors_from_array (unsigned int n, float *array, int *defined)
{
	int i;
	float r, g, b;
	float *colors = (float*)malloc(3*n*sizeof(float));
	
	/* set the absolute value */
	for (i=0; i<n; i++)
		array[i] = fabs (array[i]);
	
	/* get the maximal value */
	float min_value, max_value;
	for (i=0; i<n; i++)
	{
		if (defined[i])
		{
			min_value = array[i];
			max_value = array[i];
			break;
		}
	}
	for (i=0; i<n; i++)
	{
		if (!defined[i]) continue;
		//printf ("array[%d] = %f\n", i, array[i]);
		min_value = (min_value > array[i])? array[i] : min_value;
		max_value = (max_value < array[i])? array[i] : max_value;
	}
	printf ("   %f -> %f\n", min_value, max_value);
	
	for (i=0; i<n; i++)
	{
		if (defined[i])
		{
			get_jet_color (array[i]/max_value, &r, &g, &b);
			colors[3*i]   = r;
			colors[3*i+1] = g;
			colors[3*i+2] = b;
		}
		else
		{
			colors[3*i]   = 0.0;
			colors[3*i+1] = 0.0;
			colors[3*i+2] = 0.0;
		}
	}
	return colors;
}


float* MeshAlgoTensorEvaluator::ComparisonCurvatures (CurvatureId type)
{
	unsigned int nv = m_pModel->m_nVertices;
	Tensor **pTensors = m_pModel->m_pTensors;

	int i;
	float *array = (float*)malloc(nv*sizeof(float));
	int *defined = (int*)malloc(nv*sizeof(int));
	
	for (i=0; i<nv; i++)
	{
		if (!m_pDiffParams[i])
		{
			defined[i] = 0;
			continue;
		}
		defined[i] = 1;
		float kappa1 = m_pDiffParams[i]->GetKappaMax ();
		float kappa2 = m_pDiffParams[i]->GetKappaMin ();
		float kappa1_real = pTensors[i]->GetKappaMax ();
		float kappa2_real = pTensors[i]->GetKappaMin ();
		switch (type)
		{
		case CURVATURE_GAUSSIAN:
			array[i] = fabs (kappa1*kappa2 - kappa1_real*kappa2_real);
			break;
		case CURVATURE_MEAN:
			array[i] = fabs ((kappa1+kappa2)/2.0 - (kappa1_real+kappa2_real)/2.0);
			break;
		case CURVATURE_MAX:
			array[i] = fabs (kappa1 - kappa1_real);
			break;
		case CURVATURE_MIN:
			array[i] = fabs (kappa2 - kappa2_real);
			break;
		default:
			printf ("type unknown\n");
			break;
		}
	}
	
	float *colors = get_colors_from_array (nv, array, defined);
	free (array);
	free (defined);
	return colors;
}

float* MeshAlgoTensorEvaluator::ComparisonDirections (void)
{
	unsigned int nv = m_pModel->m_nVertices;
	Tensor **pTensors = m_pModel->m_pTensors;

	int i;
	float *array = (float*)malloc(nv*sizeof(float));
	int *defined = (int*)malloc(nv*sizeof(int));
	
	for (i=0; i<nv; i++)
	{
		if (!m_pDiffParams[i])
		{
			defined[i] = 0;
			continue;
		}
		defined[i] = 1;
		vec3 dir_max, dir_max_approximated;
		pTensors[i]->GetDirectionMax (dir_max);
		m_pDiffParams[i]->GetDirectionMax (dir_max_approximated);
		
		vec3_normalize (dir_max);
		vec3_normalize (dir_max_approximated);
		array[i] = 1.0 - fabs (vec3_dot_product (dir_max, dir_max_approximated));
	}
	
	float *colors = get_colors_from_array (nv, array, defined);
	free (array);
	free (defined);
	return colors;
}

