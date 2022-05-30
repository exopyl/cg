#include <stdio.h>
#include <stdlib.h>

#include "common.h"

float search_max (float *array, int n)
{
	float max = array[0];
	for (int i=1; i<n; i++)
		if (max < array[i])
			max = array[i];
	return max;
}

float search_min (float *array, int n)
{
	float min = array[0];
	for (int i=1; i<n; i++)
		if (min > array[i])
			min = array[i];
	return min;
}

void convolution (float *signal, int n, float **_convolution)
{
	float *convolution = (float*)malloc(n*sizeof(float));

	for (int i=0; i<n; i++)
	{
		convolution[i] = 0.;
		for (int j=0; j<n; j++)
			convolution[i] += signal[j]*signal[(j+i)%n];
	}

	*_convolution = convolution;
}


float
smoothstep (float a, float b, float x)
{
  if (x<a)
    return 0.0;
  if (x>b)
    return 1.0;
  x = (x-a) / (b-a);
  return (x*x*(3-2*x));
}

float
interpolation_linear (float a, float b, float x)
{
  return a*(1.0-x) + b*x;
}

float
interpolation_cosine (float a, float b, float x)
{
  float w = x*3.1415927;
  float t = (1.0 - cos(w))*0.5;
  return a*(1.0 - t) + b*t;
}

float
interpolation_cubic (float a, float b, float c, float d, float x)
{
  float p = (d-c) - (a-b);
  float q = (a-b) - p;
  float r = c-a;
  float s = b;
  return p*x*x*x + q*x*x + r*x + s;
}

//
// filters
//

// http://www.scicos.org/ScicosModNum/modnum_web/src/modnum_422/interf/scicos/help/eng/htm/RCF_c.htm
float filter_raised_cosine_filter (float T, float alpha, float x)
{
	float fabsx = fabs(x);
	if (fabsx < (1.-alpha)/(2.*T))
		return 1.;
	else if (fabsx >= (1.-alpha)/(2.*T) && fabsx <= (1.+alpha)/(2.*T))
		return 0.5 * (1. + cos ((M_PI*T/alpha)*(x-(1.-alpha)/(2*T))));
	else // if (fabsx > (1.+alpha)/(2.*T))
		return 0.;
	
}

// output
void output_1array (float *signal, int size, char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<size; i++)
    fprintf (ptr, "%f\n", signal[i]);
  fclose (ptr);
}

void output_2array (float *signal1, float *signal2, int size, char *filename)
{
  FILE *ptr = fopen (filename, "w");
  for (int i=0; i<size; i++)
    fprintf (ptr, "%f %f\n", signal1[i], signal2[i]);
  fclose (ptr);
}

// sort
static void _quicksort_indices_aux (float *array, int *indices, int l, int r)
{
  float pivot;
  int ulo, uhi;
  int ieq;
  float temp;
  int temp2;

  if (l >= r) return;
  pivot = array[(l+r)/2];
  ieq = ulo = l;
  uhi = r;
  
  while (ulo <= uhi)
    {
      if (array[uhi] > pivot) uhi--;
      else
	{
	  /* swap array */
	  temp = array[ulo];
	  array[ulo] = array[uhi];
	  array[uhi] = temp;
	  /* swap indices */
	  temp2 = indices[ulo];
	  indices[ulo] = indices[uhi];
	  indices[uhi] = temp2;

	  if (array[ulo] < pivot)
	    {
	      /* swap array */
	      temp = array[ieq];
	      array[ieq] = array[ulo];
	      array[ulo] = temp;
	      /* swap indices */
	      temp2 = indices[ieq];
	      indices[ieq] = indices[ulo];
	      indices[ulo] = temp2;

	      ieq++;
	    }
	  ulo++;
	}
    }
  _quicksort_indices_aux (array, indices, l, ieq-1);
  _quicksort_indices_aux (array, indices, uhi+1, r);
}      

int*
quicksort_indices (float *array, int size)
{
  int i;
  int *indices = (int*)malloc(size*sizeof(int));
  if (!indices) return NULL;
  for (i=0; i<size; i++)
    indices[i] = i;

  float *array_copy = (float*)malloc(size*sizeof(float));
  if (!array_copy) return NULL;
  for (i=0; i<size; i++)
    array_copy[i] = array[i];
  /*
  printf ("before\n");
  for (int i=0; i<size; i++)
    printf ("%d -> %d -> %f\n", i, indices[i], array[i]);
  */
  _quicksort_indices_aux (array_copy, indices, 0, size-1);
  /*
  printf ("after\n");
  for (int i=0; i<size; i++)
    printf ("%d -> %d -> %f\n", i, indices[i], array[i]);
  */
  return indices;
}

void
sort (int *array, int n)
{
  for (int j=n-1; j>0; j--)
    for (int i=0; i<j; i++)
      if (array[i+1] < array[i])
	{
	  int tmp = array[i+1];
	  array[i+1] = array[i];
	  array[i] = tmp;
	}
}

void
sort_2arrays (float *array1, float *array2, int n)
{
  float tmp;
  for (int j=n-1; j>0; j--)
    for (int i=0; i<j; i++)
      if (array1[i+1] < array1[i])
	{
	  tmp = array1[i+1];
	  array1[i+1] = array1[i];
	  array1[i] = tmp;

	  tmp = array2[i+1];
	  array2[i+1] = array2[i];
	  array2[i] = tmp;
	}
}


IDGenerator *IDGenerator::m_pInstance = new IDGenerator;
