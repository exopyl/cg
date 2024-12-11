#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "descriptor_shape_distribution_osada.h"

//
//
//
Cshape_distribution_osada::Cshape_distribution_osada (int _nv, float *_v, int _nf, unsigned int *_f)
{
  nv = _nv;
  nf = _nf;
  v  = _v;
  f  = _f;

  cumulative_areas = NULL;
  selected_points  = NULL;
  histogram        = NULL;

  compute_cumulative_areas ();
}

//
//
//
Cshape_distribution_osada::~Cshape_distribution_osada ()
{
  if (cumulative_areas) free (cumulative_areas);
  if (selected_points)  free (selected_points);
  if (histogram)        free (histogram);
}

static double factorielle (int k)
{
	double fact = 1.0;
	for (int i = 2; i <= k; ++i) fact *= i; 
	return fact;
}

//
//
//
void
Cshape_distribution_osada::compute_distribution_a3 (int _n_bins)
{
	// compute distances
	if (n_selected_points > 500) n_selected_points = 500;

	//int n = (int)(3*factorielle(n_selected_points)/(6*factorielle(n_selected_points-3)));
	int n = (int)(3*(n_selected_points-2)*(n_selected_points-1)*(n_selected_points)/(6));
	printf ("%d -> %d\n", n_selected_points, n);
	float *angles = (float*)malloc(n*sizeof(float));
	int i, j, k, l = 0;
	for (i=0; i<n_selected_points-2; i++)
		for (j=i+1; j<n_selected_points-1; j++)
			for (k=j+1; k<n_selected_points; k++)
			{
				vec3 v1;
				vec3 v2;
				vec3 v3;
				vec3_init (v1, selected_points[3*i], selected_points[3*i+1], selected_points[3*i+2]);
				vec3_init (v2, selected_points[3*j], selected_points[3*j+1], selected_points[3*j+2]);
				vec3_init (v3, selected_points[3*k], selected_points[3*k+1], selected_points[3*k+2]);
				
				vec3 u, v;
				vec3_subtraction (u, v2, v1);
				vec3_subtraction (v, v3, v1);
				angles[l++] = acos (vec3_dot_product(u,v) / (vec3_length (u)*vec3_length (v)));
				
				vec3_subtraction (u, v1, v2);
				vec3_subtraction (v, v3, v2);
				angles[l++] = acos (vec3_dot_product (u,v) / (vec3_length (u)*vec3_length (v)));
				
				vec3_subtraction (u, v1, v3);
				vec3_subtraction (v, v2, v3);
				angles[l++] = acos (vec3_dot_product(u,v) / (vec3_length (u)*vec3_length (v)));
			}
	
	// compute histogram
	n_bins = _n_bins;
	histogram = (float*)malloc(n_bins*sizeof(float));
	for (i=0; i<n_bins; i++) histogram[i] = 0;
	float min = angles[0];
	float max = angles[0];
	for (i=1; i<n; i++)
	{
		if (angles[i] < min) min = angles[i];
		if (angles[i] > max) max = angles[i];
	}
	printf ("A3 : %f -> %f\n", min, max);
	
	for (i=0; i<n; i++)
	{
		int index = (int)((n_bins-1)*(angles[i]-min)/(max-min));
		histogram[index]++;
	}
	free (angles);
}

//
//
//
void
Cshape_distribution_osada::compute_distribution_d1 (int _n_bins)
{
  int i;
  // compute distances
  int n = n_selected_points;
  float *distances = (float*)malloc(n*sizeof(float));

  // centroid
  vec3 centroid;
  vec3_init (centroid, 0.0, 0.0, 0.0);
  for (i=0; i<nv; i++)
    {
	    vec3 walk;
	    vec3_init (walk, v[3*i], v[3*i+1], v[3*i+2]);
	    vec3_addition (centroid, centroid, walk);
    }
  centroid[0] /= nv;
  centroid[1] /= nv;
  centroid[2] /= nv;

  for (i=0; i<n_selected_points; i++)
    {
	    vec3 walk;
	    vec3_init (walk, selected_points[3*i], selected_points[3*i+1], selected_points[3*i+2]);
	    vec3_subtraction (walk, walk, centroid);
	    distances[i] = vec3_length (walk);
    }

  // compute histogram
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = distances[0];
  float max = distances[0];
  for (i=1; i<n; i++)
    {
      if (distances[i] < min) min = distances[i];
      if (distances[i] > max) max = distances[i];
    }
  printf ("D1 : %f -> %f\n", min, max);

  for (i=0; i<n; i++)
    {
      int index = (int)((n_bins-1)*(distances[i]-min)/(max-min));
      histogram[index]++;
    }
  free (distances);
}

//
//
//
void
Cshape_distribution_osada::compute_distribution_d2 (int _n_bins)
{
  int i,j;
  // compute distances
  int n = (n_selected_points*(n_selected_points-1))/2;
  float *distances = (float*)malloc(n*sizeof(float));
  int k = 0;
  for (i=0; i<n_selected_points-1; i++)
    for (j=i+1; j<n_selected_points; j++)
      {
	      vec3 v1, v2;
	      vec3_init (v1, selected_points[3*i], selected_points[3*i+1], selected_points[3*i+2]);
	      vec3_init (v2, selected_points[3*j], selected_points[3*j+1], selected_points[3*j+2]);
	      distances[k++] = vec3_distance (v1, v2);
      }

  // compute histogram
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = distances[0];
  float max = distances[0];
  for (i=1; i<n; i++)
    {
      if (distances[i] < min) min = distances[i];
      if (distances[i] > max) max = distances[i];
    }
  printf ("D2 : %f -> %f\n", min, max);

  for (i=0; i<n; i++)
    {
      int index = (int)((n_bins-1)*(distances[i]-min)/(max-min));
      histogram[index]++;
    }
  free (distances);
}

void
Cshape_distribution_osada::compute_distribution_d3 (int _n_bins)
{
  /* compute distances */
  //if (n_selected_points > 170) n_selected_points = 170;
  //int n = (int)(factorielle(n_selected_points)/(6*factorielle(n_selected_points-3)));

  int n = (int)((n_selected_points-2)*(n_selected_points-1)*(n_selected_points)/(6));
  printf ("%d -> %d\n", n_selected_points, n);
  float *areas = (float*)malloc(n*sizeof(float));
  int i, j, k, l = 0;
  for (i=0; i<n_selected_points-2; i++)
    for (j=i+1; j<n_selected_points-1; j++)
      for (k=j+1; k<n_selected_points; k++)
      {
	      vec3 v1, v2, v3;
	      vec3_init (v1, selected_points[3*i], selected_points[3*i+1], selected_points[3*i+2]);
	      vec3_init (v2, selected_points[3*j], selected_points[3*j+1], selected_points[3*j+2]);
	      vec3_init (v3, selected_points[3*k], selected_points[3*k+1], selected_points[3*k+2]);

	      areas[l++] = sqrt (vec3_triangle_area (v1, v2, v3));
      }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = areas[0];
  float max = areas[0];
  for (i=1; i<n; i++)
    {
      if (areas[i] < min) min = areas[i];
      if (areas[i] > max) max = areas[i];
    }
  printf ("D3 : %f -> %f\n", min, max);

  for (i=0; i<n; i++)
    {
      int index = (int)((n_bins-1)*(areas[i]-min)/(max-min));
      histogram[index]++;
    }
  free (areas);
}

void
Cshape_distribution_osada::compute_distribution_d4 (int _n_bins)
{
  /* compute distances */
  //if (n_selected_points > 170) n_selected_points = 170;
  //int n = (int)(factorielle(n_selected_points)/(24*factorielle(n_selected_points-4)));
  int n = (int)((n_selected_points-3)*(n_selected_points-2)*(n_selected_points-1)*(n_selected_points)/(24));

  printf ("%d -> %d\n", n_selected_points, n);
  float *volumes = (float*)malloc(n*sizeof(float));
  int i, j, k, l, m = 0;
  for (i=0; i<n_selected_points-3; i++)
    for (j=i+1; j<n_selected_points-2; j++)
      for (k=j+1; k<n_selected_points-1; k++)
		for (l=k+1; l<n_selected_points; l++)
	  {
		  vec3 v1, v2, v3, v4;
	    vec3_init (v1, selected_points[3*i], selected_points[3*i+1], selected_points[3*i+2]);
	    vec3_init (v2, selected_points[3*j], selected_points[3*j+1], selected_points[3*j+2]);
	    vec3_init (v3, selected_points[3*k], selected_points[3*k+1], selected_points[3*k+2]);
	    vec3_init (v4, selected_points[3*l], selected_points[3*l+1], selected_points[3*l+2]);
	    
	    vec3 a, b, c;
	    vec3_subtraction (a, v2, v1);
	    vec3_subtraction (b, v3, v1);
	    vec3_subtraction (c, v4, v1);

		vec3 tmp;
		vec3_cross_product (tmp, b, c);
		float volume = fabs (vec3_dot_product (a, tmp)) / 6;
	    //volume = cbrt (volume);
		volume = pow ((double)volume, (double)1.0/3.0);
	    volumes[m++] = volume;
	  }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = volumes[0];
  float max = volumes[0];
  for (i=1; i<n; i++)
    {
      if (volumes[i] < min) min = volumes[i];
      if (volumes[i] > max) max = volumes[i];
    }
  printf ("D4 : %f -> %f\n", min, max);

  for (i=0; i<n; i++)
    {
      int index = (int)((n_bins-1)*(volumes[i]-min)/(max-min));
      histogram[index]++;
    }
  free (volumes);
}

void
Cshape_distribution_osada::compute_distribution (shape_function_type type, int _n_points, int _n_bins)
{
  assert (_n_points > 0 && _n_bins > 0);
  select_points (_n_points);

  switch (type)
    {
    case A3:
      printf ("distribution A3\n");
      compute_distribution_a3 (_n_bins);
      break;
    case D1:
      printf ("distribution D1\n");
      compute_distribution_d1 (_n_bins);
      break;
    case D2:
      printf ("distribution D2\n");
      compute_distribution_d2 (_n_bins);
      break;
    case D3:
      printf ("distribution D3\n");
      compute_distribution_d3 (_n_bins);
      break;
    case D4:
      printf ("distribution D4\n");
      compute_distribution_d4 (_n_bins);
      break;
    default:
      printf ("unknown type\n");
      break;
    }
}

/********************************************/
/*** Evaluations of the shape descriptors ***/
/********************************************/
void
Cshape_distribution_osada::evaluate_distribution_a3 (int n_data, int _n_bins)
{
  float *angles = (float*)malloc(n_data*sizeof(float));
  int i;
  for (i=0; i<n_data; i++)
  {
		vec3 v1, v2, v3, u, v;
		select_random_point (v1);
		select_random_point (v2);
		select_random_point (v3);

		vec3_subtraction (u, v2, v1);
		vec3_subtraction (v, v3, v1);
		angles[i] = acos (vec3_dot_product (u,v) / (vec3_length (u)*vec3_length(v)));
  }

  // compute histogram
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = angles[0];
  float max = angles[0];
  for (i=1; i<n_data; i++)
    {
      if (angles[i] < min) min = angles[i];
      if (angles[i] > max) max = angles[i];
    }
  printf ("A3 : %f -> %f\n", min, max);

  for (i=0; i<n_data; i++)
    {
      int index = (int)((n_bins-1)*(angles[i]-min)/(max-min));
      histogram[index]++;
    }
  free (angles);
}

void
Cshape_distribution_osada::evaluate_distribution_d1 (int n_data, int _n_bins)
{
  int i;
  float *distances = (float*)malloc(n_data*sizeof(float));

  /* centroid */
  vec3 centroid;
  vec3_init (centroid, 0.0, 0.0, 0.0);
  for (i=0; i<nv; i++)
    {
      vec3 walk;
      vec3_init (walk, v[3*i], v[3*i+1], v[3*i+2]);
      vec3_addition (centroid, centroid, walk);
    }
  centroid[0] /= nv;
  centroid[1] /= nv;
  centroid[2] /= nv;

  for (i=0; i<n_data; i++)
    {
      vec3 walk;
      select_random_point (walk);
      distances[i] = vec3_distance (walk, centroid);
    }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = distances[0];
  float max = distances[0];
  for (i=1; i<n_data; i++)
    {
      if (distances[i] < min) min = distances[i];
      if (distances[i] > max) max = distances[i];
    }
  min = 0.0;
  printf ("D1 : %f -> %f\n", min, max);

  for (i=0; i<n_data; i++)
    {
      int index = (int)((n_bins-1)*(distances[i]-min)/(max-min));
      histogram[index]++;
    }
  free (distances);
}

void
Cshape_distribution_osada::evaluate_distribution_d2 (int n_data, int _n_bins)
{
  int i;
  float *distances = (float*)malloc(n_data*sizeof(float));
  for (i=0; i<n_data; i++)
  {
	  vec3 v1, v2, v1v2;
	  select_random_point (v1);
	  select_random_point (v2);
	  distances[i] = vec3_distance (v1, v2);
  }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = distances[0];
  float max = distances[0];
  for (i=1; i<n_data; i++)
    {
      if (distances[i] < min) min = distances[i];
      if (distances[i] > max) max = distances[i];
    }
  printf ("D2 : %f -> %f\n", min, max);

  for (i=0; i<n_data; i++)
    {
      int index = (int)((n_bins-1)*(distances[i]-min)/(max-min));
      histogram[index]++;
    }
  free (distances);
}

void
Cshape_distribution_osada::evaluate_distribution_d3 (int n_data, int _n_bins)
{
  float *areas = (float*)malloc(n_data*sizeof(float));
  int i;
  for (i=0; i<n_data; i++)
  {
	vec3 v1, v2, v3;
	select_random_point (v1);
	select_random_point (v2);
	select_random_point (v3);
	areas[i] = sqrt (vec3_triangle_area (v1, v2, v3));
  }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = areas[0];
  float max = areas[0];
  for (i=1; i<n_data; i++)
    {
      if (areas[i] < min) min = areas[i];
      if (areas[i] > max) max = areas[i];
    }
  printf ("D3 : %f -> %f\n", min, max);

  for (i=0; i<n_data; i++)
    {
      int index = (int)((n_bins-1)*(areas[i]-min)/(max-min));
      histogram[index]++;
    }
  free (areas);
}

void
Cshape_distribution_osada::evaluate_distribution_d4 (int n_data, int _n_bins)
{
  float *volumes = (float*)malloc(n_data*sizeof(float));
  int i;
  for (i=0; i<n_data; i++)
  {
	 vec3 v1, v2, v3, v4, a, b, c;
	select_random_point (v1);
	select_random_point (v2);
	select_random_point (v3);
	select_random_point (v4);
	    
	vec3_subtraction (a, v2, v1);
	vec3_subtraction (b, v3, v1);
	vec3_subtraction (c, v4, v1);

	vec3 tmp;
	vec3_cross_product (tmp, b, c);
	float volume = fabs (vec3_dot_product (a, tmp)) / 6;
	//volume = cbrt (volume);
	volume = pow ((double)volume, (double)1.0/3.0);
	volumes[i] = volume;
  }

  /* compute histogram */
  n_bins = _n_bins;
  histogram = (float*)malloc(n_bins*sizeof(float));
  for (i=0; i<n_bins; i++) histogram[i] = 0;
  float min = volumes[0];
  float max = volumes[0];
  for (i=1; i<n_data; i++)
    {
      if (volumes[i] < min) min = volumes[i];
      if (volumes[i] > max) max = volumes[i];
    }
  printf ("D4 : %f -> %f\n", min, max);

  for (i=0; i<n_data; i++)
    {
      int index = (int)((n_bins-1)*(volumes[i]-min)/(max-min));
      histogram[index]++;
    }
  free (volumes);
}

void
Cshape_distribution_osada::evaluate_distribution (shape_function_type type, int _n_data, int _n_bins)
{
  assert (_n_data > 0 && _n_bins > 0);

  switch (type)
    {
    case A3:
      printf ("distribution A3\n");
      evaluate_distribution_a3 (_n_data, _n_bins);
      break;
    case D1:
      printf ("distribution D1\n");
      evaluate_distribution_d1 (_n_data, _n_bins);
      break;
    case D2:
      printf ("distribution D2\n");
      evaluate_distribution_d2 (_n_data, _n_bins);
      break;
    case D3:
      printf ("distribution D3\n");
      evaluate_distribution_d3 (_n_data, _n_bins);
      break;
    case D4:
      printf ("distribution D4\n");
      evaluate_distribution_d4 (_n_data, _n_bins);
      break;
    default:
      printf ("unknown type\n");
      break;
    }
}

void
Cshape_distribution_osada::normalize_distribution (void)
{
  int i;
  float sum = 0.0;
  for (i=0; i<n_bins; i++) sum += histogram[i];
  if (sum == 0.0) return;
  for (i=0; i<n_bins; i++) histogram[i] /= sum;
}

void
Cshape_distribution_osada::export_distribution (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<n_bins; i++)
    fprintf (ptr, "%f\n", histogram[i]);
  fclose (ptr);
}

void
Cshape_distribution_osada::compute_cumulative_areas (void)
{
  n_cumulative_areas = nf;
  cumulative_areas = (float*)malloc(n_cumulative_areas*sizeof(float));
  assert (cumulative_areas);

  int i;
  for (i=0; i<nf; i++)
    {
      int a, b, c;
      a = f[3*i];
      b = f[3*i+1];
      c = f[3*i+2];

      vec3 v1, v2, v3;
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

void
Cshape_distribution_osada::select_points (int _n_points)
{
  int i;  
  n_selected_points = _n_points;
  if (selected_points) free (selected_points);
  selected_points = (float*)malloc(3*n_selected_points*sizeof(float));
  assert (selected_points);

  for (i=0; i<n_selected_points; i++)
    {
      vec3 point;
      select_random_point (point);
      selected_points[3*i]   = point[0];
      selected_points[3*i+1] = point[1];
      selected_points[3*i+2] = point[2];
    }
}

int
Cshape_distribution_osada::find_area_position (float random_area, int istart, int iend)
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
Cshape_distribution_osada::select_random_point (vec3 &point)
{
  int i; // index of the random face
  float random_area = (total_area*rand()/(RAND_MAX+1.0));

  // look for the random face
  i = find_area_position (random_area, 0, n_cumulative_areas-1);

  vec3 v1, v2, v3;
  vec3_init (v1, v[3*f[3*i]], v[3*f[3*i]+1], v[3*f[3*i]+2]);
  vec3_init (v2, v[3*f[3*i+1]], v[3*f[3*i+1]+1], v[3*f[3*i+1]+2]);
  vec3_init (v3, v[3*f[3*i+2]], v[3*f[3*i+2]+1], v[3*f[3*i+2]+2]);
  
  float r1 = rand()/(RAND_MAX+1.0);
  float r2 = rand()/(RAND_MAX+1.0);
  float sr1 = sqrt (r1);
  float x = (1-sr1)*v1[0] + sr1*(1-r2)*v2[0] + sr1*r2*v3[0];
  float y = (1-sr1)*v1[1] + sr1*(1-r2)*v2[1] + sr1*r2*v3[1];
  float z = (1-sr1)*v1[2] + sr1*(1-r2)*v2[2] + sr1*r2*v3[2];
  vec3_init (point, x, y, z);

  return i;
}

