#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "descriptor_differential_parameters_distribution.h"
#include "DiffParamEvaluator.h"

Cdifferential_parameters_distribution::Cdifferential_parameters_distribution (Mesh_half_edge *_model)
{
  assert (_model);
  model = _model;

  histogram = NULL;
}

Cdifferential_parameters_distribution::~Cdifferential_parameters_distribution ()
{
  if (histogram) free (histogram);
}

void
Cdifferential_parameters_distribution::compute_distribution (shape_function_type type, int _n_bins)
{
  int i;
  assert (_n_bins > 0);

  int nv = model->m_nVertices;
  int nf = model->m_nFaces;
  float *v = model->m_pVertices;
  float *fn = model->m_pFaceNormals;

  // get the tensor
  MeshAlgoTensorEvaluator *pTensorEvaluator = new MeshAlgoTensorEvaluator();
  pTensorEvaluator->Init (model);
  pTensorEvaluator->Evaluate (TENSOR_HAMANN);

  // get the differential parameter of interest
  float *data;
  int size;
  if (type == BESL)
	  size = 3*nf;
  else
	  size = nv;
  data = (float*)malloc(size*sizeof(float));
  assert (data);
  float *weights = (float*)malloc(size*sizeof(float)); // only used for Besl's histogram
  assert (weights);
 
  int iwalk = 0; // only used for BESL's histogram
  switch (type)
    {
  case BESL:
	  for (i=0; i<3*nf; i++)
	  {
		  Che_edge *he = model->m_edges[i];
		  if (he && he->m_pair)
		  {
			  int a,b;

			  a = he->m_v_begin;
			  b = he->m_v_end;
			  Vector3f v1 (v[3*a], v[3*a+1], v[3*a+2]);
			  Vector3f v2 (v[3*b], v[3*b+1], v[3*b+2]);
			  Vector3f v3 = v2 - v1;
			  weights[iwalk] = v3.getLength ();

			  a = he->m_face;
			  b = he->m_pair->m_face;
			  Vector3f na (fn[3*a], fn[3*a+1], fn[3*a+2]);
			  Vector3f nb (fn[3*b], fn[3*b+1], fn[3*b+2]);
			  float dot = na * nb;
			  if (dot >= 1.0)  dot = 1.0;
			  if (dot <= -1.0) dot = -1.0;
			  Vector3f cross = na ^ nb;
			  if (cross * v3 >= 0.0)
				  data[iwalk] = (dot+1)*3.14159/2.0;
			  else
				  data[iwalk] = (-dot+3)*3.14159/2.0;
			  //printf ("%f\n", data[iwalk]);

			  iwalk++;
		  }
	  }
	  size = iwalk;
	  break;
    case GAUSSIAN_CURVATURE:
      for (i=0; i<nv; i++)
	  {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
		if (pDiffParam)
		{
			data[i] = (pDiffParam->GetKappaMax () * pDiffParam->GetKappaMin ());
		}
	  }
      break;
    case MEAN_CURVATURE:
      for (i=0; i<nv; i++)
	  {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
	if (pDiffParam)
	  data[i] = 0.5*fabs (pDiffParam->GetKappaMax () + pDiffParam->GetKappaMin ());
	  }
      break;
    case MAX_CURVATURE:
      for (i=0; i<nv; i++)
	  {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
	if (pDiffParam)
	  data[i] = (fabs (pDiffParam->GetKappaMax ()) > fabs (pDiffParam->GetKappaMax ()))?
	    fabs (pDiffParam->GetKappaMax ()) : fabs (pDiffParam->GetKappaMax ());
	  }
      break;
    case MIN_CURVATURE:
      for (i=0; i<nv; i++)
	  {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
	if (pDiffParam)
	  data[i] = (fabs (pDiffParam->GetKappaMax ()) < fabs (pDiffParam->GetKappaMax ()))?
	    fabs (pDiffParam->GetKappaMax ()) : fabs (pDiffParam->GetKappaMax ());
	  }
      break;
    case MPEG7:
      for (i=0; i<nv; i++)
	  {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
		if (pDiffParam)
		{
			float kappa1 = pDiffParam->GetKappaMax ();
			float kappa2 = pDiffParam->GetKappaMin ();
			if (kappa1 - kappa2 == 0) continue;
			data[i] = 0.5 - atan ((kappa1 + kappa2) / (kappa1 - kappa2)) / 3.14159;
		}
	  }
      break;
    default:
      printf ("unknown type\n");
      break;
    }

  if (type == BESL)
  {
	float min = data[0], max = data[0];
	  for (i=0; i<size; i++)
	    {
		  //printf ("%f\n", data[i]);
		  if (data[i] < min) min = data[i];
		  if (data[i] > max) max = data[i];
	    }
	  min = 0.0;
	  max = 2*3.14159;

	  min = 3.14159-0.01;
	  max = 3.14159+0.01;


	 n_bins = _n_bins;
	histogram = (float*)malloc(n_bins*sizeof(float));
	assert (histogram);
	for (i=0; i<n_bins; i++) histogram[i] = 0;
	for (i=0; i<size; i++)
    {
		if (data[i]>max || data[i]<min) continue;
	  int index = (int)((n_bins-1)*(data[i]-min)/(max-min));
	  histogram[index]+=weights[i];
    }

	  free (data);
	  return;
  }

  /* look for the maximal values */
  float min, max;
  for (i=0; i<nv; i++)
    {
	  Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);

      if (pDiffParam)
	{
	  min = data[i];
	  max = min;
	  break;
	}
    }
  for (i=0; i<nv; i++)
    {
		Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
      if (pDiffParam)
	{
	  if (data[i] < min) min = data[i];
	  if (data[i] > max) max = data[i];
	}
    }

  float threshold = 0.7;
  if (type != MPEG7 && 0)
    {
      min = -0.005;//0.0;//-threshold;
      max =  0.005;//threshold;
    }



  /* compute the histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  assert (histogram);
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  for (i=0; i<nv; i++)
    {
	    Tensor *pDiffParam = pTensorEvaluator->GetDiffParam (i);
      if (pDiffParam)
	{
	  if (fabs (data[i]) > max) continue;
	  int index = (int)((n_bins-1)*(data[i]-min)/(max-min));
	  histogram[index]++;
	}
    }
  free (data);
}

void
Cdifferential_parameters_distribution::normalize_distribution (void)
{
  int i;
  float sum = 0.0;
  for (i=0; i<n_bins; i++) sum += histogram[i];
  if (sum == 0.0) return;
  for (i=0; i<n_bins; i++) histogram[i] /= sum;
}

void
Cdifferential_parameters_distribution::export_distribution (const std::string & filename)
{
  FILE *ptr = fopen (filename.c_str(), "w");
  for (int i=0; i<n_bins; i++)
	fprintf (ptr, "%f\n", histogram[i]);
    //fprintf (ptr, "%f %f\n", i*3.14159/n_bins, histogram[i]); // for Besl
  fclose (ptr);
}
