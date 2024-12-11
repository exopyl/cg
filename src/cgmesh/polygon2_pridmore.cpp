#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "polygon2.h"

void
Polygon2::search_symmetry_pridmore (void)
{
	if (m_nContours != 1)
		return;

  int i,j,k;
  int n = m_nPoints[0];
  float *pPoints = m_pPoints[0];
  int **A, *B;
  int theta_max = 360;
  int rho_min, rho_max, rho_size;

  /* compute the size of the accumulator */
  rho_min = 0;
  rho_max = 0;
  for (i=0; i<n; i++)
    {
      for (j=0; j<theta_max; j++)
	{
	  float theta_walk = (float)(j*3.14159/180.0);
	  int rho_walk = (int)(pPoints[2*i]*cos(theta_walk)+pPoints[2*i+1]*sin(theta_walk));
	  if (rho_walk < rho_min) rho_min = rho_walk;
	  if (rho_walk > rho_max) rho_max = rho_walk;
	}
    }
  rho_size = rho_max - rho_min + 1;
  //printf ("%d < rho < %d\n", rho_min, rho_max);
  
  /* memory allocation and initialization */
  A = (int**)malloc(rho_size*sizeof(int*));
  assert (A);
  for (i=0; i<rho_size; i++)
    {
      A[i] = (int*)malloc(theta_max*sizeof(int));
      assert (A[i]);
      A[i] = (int*)memset ((void*)A[i], 0, theta_max*sizeof(int));
    }
  B = (int*)malloc(theta_max*sizeof(int));
  assert (B);
  B = (int*)memset ((void*)B, 0, theta_max*sizeof(int));

  /* fill the accumulator A[rho][theta] */
  for (i=0; i<n; i++)
    {
      for (j=0; j<theta_max; j++)
	{
	  float theta_walk = (float)(j*3.14159/180.0);
	  int rho_walk = (int)(pPoints[2*i]*cos(theta_walk)+pPoints[2*i+1]*sin(theta_walk));
	  A[rho_walk-rho_min][j]++;
	}
    }

  /*
   * look at the maximal value in the accumulator
   * (only for the creation of the output)
   */
  int max_value = 0;
  for (i=0; i<rho_size; i++)
    for (j=0; j<theta_max; j++)
      if (A[i][j] > max_value) max_value = A[i][j];

  /* output A */
  FILE *ptr;
  ptr = fopen ("A.pgm", "w");
  fprintf (ptr, "P2\n%d %d\n%d\n", theta_max, rho_size, max_value);
  for (i=0; i<rho_size; i++)
    for (j=0; j<theta_max; j++)
      fprintf (ptr, "%d\n", max_value-A[i][j]);
  fclose (ptr);

  /* fill the accumulator B[theta] */
  for (i=0; i<rho_size; i++)
    for (j=1; j<theta_max-3; j++)
      {
	/* is it a local maximum ? */
	if (A[i][j-1] < A[i][j] && A[i][j+1] < A[i][j]) /* yes ! */
	  {
	    for (k=j+2; k<theta_max-1; k++)
	      if (A[i][k-1] < A[i][k] && A[i][k+1] < A[i][k]) /* another local maximum ? */
		B[(int)((j+k)/2)]++;
	  }
      }

  /* output B */
  ptr = fopen ("B.dat", "w");
  for (i=0; i<theta_max; i++)
    fprintf (ptr, "%d\n", B[i]);
  fclose (ptr);

  /* look at the maximal value */
  int i_max = 0;
  for (i=1; i<theta_max; i++)
    if (B[i_max] < B[i]) i_max = i;
  printf ("angle for symmetry : %d\n", i_max);
  

  /* cleaning */
  for (i=0; i<rho_size; i++)
    if (A[i]) free (A[i]);
  if (A) free (A);
  if (B) free (B);  
}
