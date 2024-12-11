#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "polygon2.h"

void
Polygon2::search_symmetry_pridmore_bis (void)
{
	if (m_nContours != 1)
		return;

  int i,j;
  int n = m_nPoints[0];
  float *pPoints = m_pPoints[0];
  int **A;
  int theta_max = 360;
  int rho_min, rho_max, rho_size;

  /*
   * p3 linked with p2
   * p4 linked with p1
   */
  float x1, x2, x3, x4, y1, y2, y3, y4;
  float xc1, xc2, yc1, yc2, xc1c2, yc1c2;
  rho_min = rho_max = 0;
  for (i=0; i<n; i++)
    {
      x1 = pPoints[2*i];
      y1 = pPoints[2*i+1];
      x2 = pPoints[2*((i+1)%n)];
      y2 = pPoints[2*((i+1)%n)+1];
      for (j=0; j<n; j++)
	{
	  if (j != i)
	    {
	      x3 = pPoints[2*j];
	      y3 = pPoints[2*j+1];
	      x4 = pPoints[2*((j+1)%n)];
	      y4 = pPoints[2*((j+1)%n)+1];
	      /* test if p1p2 and p3p4 are perpendicular */
	      if ((x2-x1)*(x4-x3)+(y2-y1)*(y4-y3) == 0.0)
		continue;
	      xc1 = (x1+x4)/2.0;
	      yc1 = (y1+y4)/2.0;
	      xc2 = (x2+x3)/2.0;
	      yc2 = (y2+y3)/2.0;

	      /* c1c2 */
	      xc1c2 = xc2 - xc1;
	      yc1c2 = yc2 - yc1;
	      
	      /* deduce the parameters theta and rho for Hough space */
	      float theta;
	      int rho1, rho2;
	      if (xc1c2 == 0.0) theta = 3.14159/2.0;
	      else theta = atan (fabs(yc1c2/xc1c2));
	      
	      if (xc1c2 < 0.0)
		{
		  xc1c2 *= -1;
		  yc1c2 *= -1;
		}
	      
	      if (xc1c2*yc1c2 > 0.0)
		{
		  if ((yc1*xc2-yc2*xc1)/(xc2-xc1) >= 0.0) theta += 3.14159/2.0;
		  else theta += 3.14159*3.0/2.0;
		}
	      else
		{
		  if ((yc1*xc2-yc2*xc1)/(xc2-xc1) >= 0.0) theta = 3.14159/2.0 - theta;
		  else theta = 3.14159*3.0/2.0 - theta;
		}

	      rho1 = (int)(xc1*cos(theta)+yc1*sin(theta));
	      rho2 = (int)(xc2*cos(theta)+yc2*sin(theta));
	      //printf ("%f %f\n", rho1, rho2);
	      if (rho_min > rho1) rho_min = rho1;
	      if (rho_max < rho1) rho_max = rho1;
	    }
	}
    }
  printf ("%d < rho < %d\n", rho_min, rho_max);
  rho_size = rho_max-rho_min+1;
  printf ("-> rho_size = %d\n", rho_size);

  /* memory allocation and initialization */
  A = (int**)malloc(rho_size*sizeof(int*));
  assert (A);
  for (i=0; i<rho_size; i++)
    {
      A[i] = (int*)malloc(theta_max*sizeof(int));
      assert (A[i]);
      A[i] = (int*)memset ((void*)A[i], 0, theta_max*sizeof(int));
    }

  /* fill the accumulator A[rho][theta] */
  for (i=0; i<n; i++)
    {
      x1 = pPoints[2*i];
      y1 = pPoints[2*i+1];
      x2 = pPoints[2*((i+1)%n)];
      y2 = pPoints[2*((i+1)%n)+1];
      for (j=0; j<n; j++)
	{
	  if (j != i)
	    {
	      x3 = pPoints[2*j];
	      y3 = pPoints[2*j+1];
	      x4 = pPoints[2*((j+1)%n)];
	      y4 = pPoints[2*((j+1)%n)+1];
	      /* test if p1p2 and p3p4 are perpendicular */
	      if ((x2-x1)*(x4-x3)+(y2-y1)*(y4-y3) == 0.0)
		continue;
	      xc1 = (x1+x4)/2.0;
	      yc1 = (y1+y4)/2.0;
	      xc2 = (x2+x3)/2.0;
	      yc2 = (y2+y3)/2.0;

	      /* c1c2 */
	      xc1c2 = xc2 - xc1;
	      yc1c2 = yc2 - yc1;
	      
	      /* deduce the parameters theta and rho for Hough space */
	      float theta;
	      int rho1;
	      if (xc1c2 == 0.0) theta = 3.14159/2.0;
	      else theta = atan (fabs(yc1c2/xc1c2));
	      
	      if (xc1c2 < 0.0)
		{
		  xc1c2 *= -1;
		  yc1c2 *= -1;
		}
	      
	      if (xc1c2*yc1c2 > 0.0)
		{
		  if ((yc1*xc2-yc2*xc1)/(xc2-xc1) >= 0.0) theta += 3.14159/2.0;
		  else theta += 3.14159*3.0/2.0;
		}
	      else
		{
		  if ((yc1*xc2-yc2*xc1)/(xc2-xc1) >= 0.0) theta = 3.14159/2.0 - theta;
		  else theta = 3.14159*3.0/2.0 - theta;
		}

	      rho1 = (int)(xc1*cos(theta)+yc1*sin(theta));
	      //rho2 = (int)(xc2*cos(theta)+yc2*sin(theta));
	      A[(int)(rho1-rho_min)][(int)(theta*180.0/3.14159)]++;
	    }
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
      fprintf (ptr, "%d\n", A[i][j]);
  fclose (ptr);

  /* cleaning */
  for (i=0; i<rho_size; i++)
    if (A[i]) free (A[i]);
  if (A) free (A);
}

