#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "descriptor_distribution_around_axis.h"

#include "mesh_half_edge.h"
#include "orientation_pca.h"

Cdistribution_around_axis::Cdistribution_around_axis (Mesh_half_edge *_model)
{
  assert (_model);
  model = _model;

  histogram1k  = NULL;
  histogram2k1 = NULL;
  histogram2k2 = NULL;
  histogram3k  = NULL;

  nv = model->m_nVertices;
  nf = model->m_nFaces;
  v  = model->m_pVertices;
  f  = model->m_pFaces;

  compute_cumulative_areas ();

  srand (30);
}


void
Cdistribution_around_axis::compute_length_dmean_variance_deviation (vec3 axis,
								    float *_length, float *_dmean, float *_variance, float *_deviation)
{
  int i, nv = model->m_nVertices;
  float  *v = model->m_pVertices;
  float length, dmean, variance, deviation;
  vec3 pt;
  float *positions = (float*)malloc(nv*sizeof(float));
  float *distances = (float*)malloc(nv*sizeof(float));

  // init the distances
  for (i=0; i<nv; i++)
    {
      float a, b, dist2, dist;
      vec3 pt;
      vec3_init (pt, v[3*i], v[3*i+1], v[3*i+2]);
      a = vec3_dot_product (pt, axis);
      b = vec3_dot_product (pt, pt);
      dist2 =  b - a*a;
      if (dist2 < 0.0) dist2 = 0.0;
      dist = sqrt (dist2);
      
      positions[i] = a;
      distances[i] = dist;
    }
  
  // compute length
  float min = positions[0];
  float max = positions[0];
  for (i=0; i<nv; i++)
    {
      if (min > positions[i]) min = positions[i];
      if (max < positions[i]) max = positions[i];
    }
  length = max - min;
  
  // compute mean value
  dmean = 0.0;
  for (i=0; i<nv; i++)
    dmean += distances[i];
  dmean /= nv;
  //printf ("dmean = %f\n", dmean);
  
  // compute variance
  variance = 0.0;
  for (i=0; i<nv; i++)
    variance += (distances[i]-dmean)*(distances[i]-dmean);
  variance /= nv;
  //printf ("variance = %f\n", variance);

  // compute deviation
  deviation = sqrt (variance);
  //printf ("deviation = %f\n", deviation);
  
  *_length    = length;
  *_dmean     = dmean;
  *_variance  = variance;
  *_deviation = deviation;
}

/*****************************/
/* first order distributions */
/*****************************/
void
Cdistribution_around_axis::compute_first_order_distributions (orientation_type type)
{
  float *mrot = (float*)malloc(9*sizeof(float));
  vec3 axis1, axis2, axis3;

  Cmesh_orientation_pca *opca = new Cmesh_orientation_pca (model);
  switch (type)
    {
    case ORIENTATION_PCA:
      opca->compute_orientation (0);
      break;
    case ORIENTATION_WEIGHTED_VERTICES:
      opca->compute_orientation (1);
      break;
    case ORIENTATION_BARYCENTER:
      opca->compute_orientation (2);
      break;
    case ORIENTATION_CPCA:
      opca->compute_orientation (3);
      break;
    default:
      printf ("unknown orientation type\n");
    }
  
  mrot = opca->get_matrix_rotation ();
  vec3_init (axis1, mrot[0], mrot[1], mrot[2]);
  vec3_init (axis2, mrot[3], mrot[4], mrot[5]);
  vec3_init (axis3, mrot[6], mrot[7], mrot[8]);

  delete opca;
  
  /* along the first axis */
  compute_length_dmean_variance_deviation (axis1, &lengths[0], &dmeans[0], &variances[0], &deviations[0]);
  compute_length_dmean_variance_deviation (axis2, &lengths[1], &dmeans[1], &variances[1], &deviations[1]);
  compute_length_dmean_variance_deviation (axis3, &lengths[2], &dmeans[2], &variances[2], &deviations[2]);
}

/************* PAQUET ****************/

/***********************/
/* first order (cords) */
/***********************/
void
Cdistribution_around_axis::compute_first_order_distributions_paquet (orientation_type type, int _npoints, int _nbins)
{
  float *mrot = (float*)malloc(9*sizeof(float));
  vec3 axis1, axis2, axis3;

  Cmesh_orientation_pca *opca = new Cmesh_orientation_pca (model);
  switch (type)
    {
    case ORIENTATION_PCA:
      opca->compute_orientation (0);
      break;
    case ORIENTATION_WEIGHTED_VERTICES:
      opca->compute_orientation (1);
      break;
    case ORIENTATION_BARYCENTER:
      opca->compute_orientation (2);
      break;
    case ORIENTATION_CPCA:
      opca->compute_orientation (3);
      break;
    default:
      printf ("unknown orientation type\n");
    }
  
  mrot = opca->get_matrix_rotation ();

  vec3_init (axis1, mrot[0], mrot[1], mrot[2]);
  vec3_init (axis2, mrot[3], mrot[4], mrot[5]);
  vec3_init (axis3, mrot[6], mrot[7], mrot[8]);

  opca->apply_orientation ();
  model->ComputeNormals ();
  int i, j;

  npoints = _npoints;
  nbins = _nbins;
  float angle1, angle2;

  /* init histograms */
  histogram1k = (float*)malloc(nbins*sizeof(float));
  assert (histogram1k);
  for (i=0; i<nbins; i++) histogram1k[i] = 0;

  histogram2k1 = (float*)malloc(nbins*sizeof(float));
  histogram2k2 = (float*)malloc(nbins*sizeof(float));
  assert (histogram2k1);
  assert (histogram2k2);
  for (i=0; i<nbins; i++) histogram2k1[i] = 0;
  for (i=0; i<nbins; i++) histogram2k2[i] = 0;

  histogram3k = (float**)malloc(nbins*sizeof(float*));
  assert (histogram3k);
  for (i=0; i<nbins; i++)
    {
      histogram3k[i] = (float*)malloc(nbins*sizeof(float));
      assert (histogram3k[i]);
      for (j=0; j<nbins; j++) histogram3k[i][j] = 0;
    }

  /* fill the histograms */
  int index1, index2;
  for (i=0; i<npoints; i++)
    {
	  Vector3f walk;
	  select_random_point (walk);
	  walk.Normalize ();

	  angle1 = vec3_dot_product (axis1, walk);
      if (angle1 > 1.0)  angle1 =  1.0;
      if (angle1 < -1.0) angle1 = -1.0;
      angle1 = acos (angle1);

      angle2 = vec3_dot_product (axis2, walk);
      if (angle2 > 1.0)  angle2 =  1.0;
      if (angle2 < -1.0) angle2 = -1.0;
      angle2 = acos (angle2);
      //printf ("%f %f\n", angle1, angle2);

      index1 = (int)((nbins-1)*(angle1/3.14159));
      index2 = (int)((nbins-1)*(angle2/3.14159));
      //printf ("%d %d\n", index1, index2);

      // first kind
      histogram1k[index1]++;
      histogram1k[index2]++;

      // second kind
      histogram2k1[index1]++;
      histogram2k2[index2]++;

      // third kind
      histogram3k[index1][index2]++;
    }

  /* normalize the histograms */
  float sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram1k[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram1k[i] /= sum;
      
  sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram2k1[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram2k1[i] /= sum;
  
  sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram2k2[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram2k2[i] /= sum;

  sum = 0.0;
  for (i=0; i<nbins; i++)
    for (j=0; j<nbins; j++)
      sum +=  histogram3k[i][j];
  if (sum != 0.0)
    for (i=0; i<nbins; i++)
      for (j=0; j<nbins; j++)
	histogram3k[i][j] /= sum;
}


//
// second order
//
void
Cdistribution_around_axis::compute_second_order_distributions_paquet (orientation_type type, int _npoints, int _nbins)
{
  float *mrot = (float*)malloc(9*sizeof(float));
  vec3 axis1, axis2, axis3;
  vec3 v1, v2, v3;
  vec3 nv1, nv2, nv3;

  Cmesh_orientation_pca *opca = new Cmesh_orientation_pca (model);
  switch (type)
    {
    case ORIENTATION_PCA:
      opca->compute_orientation (0);
      break;
    case ORIENTATION_WEIGHTED_VERTICES:
      opca->compute_orientation (1);
      break;
    case ORIENTATION_BARYCENTER:
      opca->compute_orientation (2);
      break;
    case ORIENTATION_CPCA:
      opca->compute_orientation (3);
      break;
    default:
      printf ("unknown orientation type\n");
    }
  
  mrot = opca->get_matrix_rotation ();
  vec3_init (axis1, mrot[0], mrot[1], mrot[2]);
  vec3_init (axis2, mrot[3], mrot[4], mrot[5]);
  vec3_init (axis3, mrot[6], mrot[7], mrot[8]);

  opca->apply_orientation ();
  model->ComputeNormals ();
  int i, j, nv = model->m_nVertices;
  float *vn = model->m_pVertexNormals;
  float *fn = model->m_pFaceNormals;
  assert (vn);

  npoints = _npoints;
  nbins = _nbins;
  float angle1, angle2;

  /* init histograms */
  histogram1k = (float*)malloc(nbins*sizeof(float));
  assert (histogram1k);
  for (i=0; i<nbins; i++) histogram1k[i] = 0;

  histogram2k1 = (float*)malloc(nbins*sizeof(float));
  histogram2k2 = (float*)malloc(nbins*sizeof(float));
  assert (histogram2k1);
  assert (histogram2k2);
  for (i=0; i<nbins; i++) histogram2k1[i] = 0;
  for (i=0; i<nbins; i++) histogram2k2[i] = 0;

  histogram3k = (float**)malloc(nbins*sizeof(float*));
  assert (histogram3k);
  for (i=0; i<nbins; i++)
    {
      histogram3k[i] = (float*)malloc(nbins*sizeof(float));
      assert (histogram3k[i]);
      for (j=0; j<nbins; j++) histogram3k[i][j] = 0;
    }

  /* fill the histograms */
  int index1, index2;
  for (i=0; i<npoints; i++)
    {
	  Vector3f walk;
	  int iwalk;
	  iwalk = select_random_point (walk);

	  int a,b,c;
	  a = f[iwalk]->GetVertex(0);
	  b = f[iwalk]->GetVertex(1);
	  c = f[iwalk]->GetVertex(2);

	  vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
	  vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
	  vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
	  float w1, w2, w3;
	  w1 = vec3_triangle_area (walk, v2, v3);
	  w2 = vec3_triangle_area (walk, v1, v3);
	  w3 = vec3_triangle_area (walk, v1, v2);

	  vec3_init (nv1, vn[3*a], vn[3*a+1], vn[3*a+2]);
	  vec3_init (nv2, vn[3*b], vn[3*b+1], vn[3*b+2]);
	  vec3_init (nv3, vn[3*c], vn[3*c+1], vn[3*c+2]);

	  vec3 vnwalk;
	  vec3_init (vnwalk, 
		     w1*nv1[0]+w2*nv2[0]+w3*nv3[0],
		     w1*nv1[1]+w2*nv2[1]+w3*nv3[1],
		     w1*nv1[2]+w2*nv2[2]+w3*nv3[2]);
	  vec3_normalize (vnwalk);

	  angle1 = vec3_dot_product (axis1, vnwalk);
      if (angle1 > 1.0)  angle1 =  1.0;
      if (angle1 < -1.0) angle1 = -1.0;
      angle1 = acos (angle1);

      angle2 = vec3_dot_product (axis2, vnwalk);
      if (angle2 > 1.0)  angle2 =  1.0;
      if (angle2 < -1.0) angle2 = -1.0;
      angle2 = acos (angle2);

      index1 = (int)((nbins-1)*(angle1/3.14159));
      index2 = (int)((nbins-1)*(angle2/3.14159));
      //printf ("%d %d\n", index1, index2);

      // first kind
      histogram1k[index1]++;
      histogram1k[index2]++;

      // second kind
      histogram2k1[index1]++;
      histogram2k2[index2]++;

      // third kind
      histogram3k[index1][index2]++;
    }

  /* normalize the histograms */
  float sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram1k[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram1k[i] /= sum;
      
  sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram2k1[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram2k1[i] /= sum;
  
  sum = 0.0;
  for (i=0; i<nbins; i++) sum +=  histogram2k2[i];
  if (sum != 0.0) for (i=0; i<nbins; i++) histogram2k2[i] /= sum;

  sum = 0.0;
  for (i=0; i<nbins; i++)
    for (j=0; j<nbins; j++)
      sum +=  histogram3k[i][j];
  if (sum != 0.0)
    for (i=0; i<nbins; i++)
      for (j=0; j<nbins; j++)
	histogram3k[i][j] /= sum;
}


void
Cdistribution_around_axis::export_histogram1k (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<nbins; i++)
    fprintf (ptr, "%f\n", histogram1k[i]);
  fclose (ptr);
}

void
Cdistribution_around_axis::export_histogram2k1 (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<nbins; i++)
    fprintf (ptr, "%f\n", histogram2k1[i]);
  fclose (ptr);
}

void
Cdistribution_around_axis::export_histogram2k2 (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<nbins; i++)
    fprintf (ptr, "%f\n", histogram2k2[i]);
  fclose (ptr);
}

void
Cdistribution_around_axis::export_histogram3k (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<nbins; i++)
    {
      for (int j=0; j<nbins; j++)
	//fprintf (ptr, "%d %d %f\n", i, j, histogram3k[i][j]);
	fprintf (ptr, "%f\n", histogram3k[i][j]);
      fprintf (ptr, "\n");
    }
  fclose (ptr);
}


void
Cdistribution_around_axis::normalize_distributions (void)
{
  int i,j;
  float sum1k = 0.0;
  float sum2k1 = 0.0;
  float sum2k2 = 0.0;
  float sum3k = 0.0;
  for (i=0; i<nbins; i++)
  {
	  sum1k  += histogram1k[i];
	  sum2k1 += histogram2k1[i];
	  sum2k2 += histogram2k2[i];
  }
  for (i=0; i<nbins; i++)
	  for (j=0; j<nbins; j++)
		  sum3k  += histogram3k[i][j];
  if (sum1k == 0.0 || sum2k1 == 0.0 || sum2k2 == 0.0 || sum3k == 0.0) return;
  for (i=0; i<nbins; i++)
  {
	  histogram1k[i]  /= sum1k;
	  histogram2k1[i] /= sum2k1;
	  histogram2k2[i] /= sum2k2;
  }
  for (i=0; i<nbins; i++)
	  for (j=0; j<nbins; j++)
		  histogram3k[i][j] /= sum3k;
}

void
Cdistribution_around_axis::compute_cumulative_areas (void)
{
  n_cumulative_areas = nf;
  cumulative_areas = (float*)malloc(n_cumulative_areas*sizeof(float));
  assert (cumulative_areas);

  int i;
  for (i=0; i<nf; i++)
    {
      int a, b, c;
      a = f[i]->GetVertex(0);
      b = f[i]->GetVertex(1);
      c = f[i]->GetVertex(2);

      vec3 v1;
      vec3 v2;
      vec3 v3;
      vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
      vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
      vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
      cumulative_areas[i] = vec3_triangle_area (v1, v2, v3);
    }
  for (i=1; i<nf; i++)
    {
      cumulative_areas[i] += cumulative_areas[i-1];
    }
  total_area = cumulative_areas[nf-1];
}

int
Cdistribution_around_axis::find_area_position (float random_area, int istart, int iend)
{
	if (istart+1 == iend)
		return istart;

	int imedium = (istart+iend)/2;
	if (cumulative_areas[imedium] >= random_area)
		return find_area_position (random_area, istart, imedium);
	else
		return find_area_position (random_area, imedium, iend);
}

int
Cdistribution_around_axis::select_random_point (vec3 point)
{
  int i;
  float random_area = (total_area*rand()/(RAND_MAX+1.0));

  /* look for the random face */
  i = find_area_position (random_area, 0, n_cumulative_areas-1);

  /* random face is i */

  vec3 v1, v2, v3;
      int a, b, c;
      a = f[i]->GetVertex(0);
      b = f[i]->GetVertex(1);
      c = f[i]->GetVertex(2);
  vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
  vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
  vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
  
  float r1 = rand()/(RAND_MAX+1.0);
  float r2 = rand()/(RAND_MAX+1.0);
  float sr1 = sqrt (r1);
  float x = (1-sr1)*v1[0] + sr1*(1-r2)*v2[0] + sr1*r2*v3[0];
  float y = (1-sr1)*v1[1] + sr1*(1-r2)*v2[1] + sr1*r2*v3[1];
  float z = (1-sr1)*v1[2] + sr1*(1-r2)*v2[2] + sr1*r2*v3[2];
  vec3_init (point, x, y, z);

  return i;
}

