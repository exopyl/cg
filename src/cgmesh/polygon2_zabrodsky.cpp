#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "polygon2.h"

void
Polygon2::search_symmetry_zabrodsky (float *xc, float *yc, float *slope)
{
	if (m_nContours != 1)
		return;

  int i,j;
  int n = m_nPoints[0];
  float *pPoints = m_pPoints[0];

  // centerize the contour
  centerize ();

  // alloc memory
  float *coeff = (float*)malloc(n*sizeof(float));
  float *angle = (float*)malloc(n*sizeof(float));

  // search the symmetry
  float xp, yp;
  for (i=0; i<n; i++)
    {
	    // init
	    angle[i] = 0.0;
	    coeff[i] = 0.0;

      /* test all the points */
      xp = pPoints[2*i];
      yp = pPoints[2*i+1];
      
      /* slope of the line */
      float theta = acos ((xp) / (sqrt (xp*xp+yp*yp)));
      if (yp < 0.0) theta = 2*3.14159 - theta;
      angle[i] = theta;
      if (angle[i] >= 3.14159) angle[i] -= 3.14159;

      /* compute the coefficient of symmetry */
      float x1, y1, x2, y2, x11, y11;
      int i1, i2;
      for (j=1; j<n/2; j++)
	{
	  i1 = (i+j >= n)? i+j-n : i+j;
	  i2 = (i-j < 0)? i-j+n : i-j;
	  x1 = pPoints[2*i1];
	  y1 = pPoints[2*i+11];
	  x2 = pPoints[2*i2];
	  y2 = pPoints[2*i2+1];
      
	  /* (x11,y11) symmetric point through y = tan(theta) x */
	  x11 = x1*cos(2*theta) + y1*sin(2*theta);
	  y11 = x1*sin(2*theta) - y1*cos(2*theta);

	  /* distance between P11 and P2 */
	  float d = sqrt ((x11-x2)*(x11-x2)+(y11-y2)*(y11-y2));

	  /* save the result */
	  coeff[i] += d;
	}
    }

  /* output */
  //output_1array (angle, n, "zab_angle.dat");
  //output_1array (coeff, n, "zab_coeff.dat");
  //sort_2arrays (angle, coeff, n);
  
  /* looking for the minimum */
  int imin = 0;
  for (i=1; i<n; i++)
    if (coeff[imin] > coeff[i]) imin = i;
  //printf ("%d -> %f\n", imin, coeff[imin]);
  
  xp = pPoints[2*imin];
  yp = pPoints[2*imin+1];
  float theta = acos ((xp) / (sqrt (xp*xp+yp*yp)));
  if (yp < 0.0) theta = 2*3.14159 - theta;
  while (theta >= 3.14159) theta -= 3.14159;
  //printf ("---> zabrodsky's method : %f deg\n", theta*180.0/3.14159);

  /* results */
  *xc = 0.0;
  *yc = 0.0;
  *slope = theta;

  /* create the closest symmetry */
  float *xs = (float*)malloc(n*sizeof(float));
  float *ys = (float*)malloc(n*sizeof(float));
  assert (xs && ys);
  xs[imin] = pPoints[2*imin];
  ys[imin] = pPoints[2*imin+1];
  theta = acos ((xp) / (sqrt (xp*xp+yp*yp)));
  if (yp < 0.0) theta = 2*3.14159 - theta;
  for (j=1; j<=n/2; j++)
    {
      int i1 = (imin+j)%n;
      int i2 = (imin-j+n)%n;
      float x1 = pPoints[2*i1];
      float y1 = pPoints[2*i1+1];
      float x2 = pPoints[2*i2];
      float y2 = pPoints[2*i2+1];

      /* (x11,y11) symmetric point through y = tan(theta) x */
      float x11 = x1*cos(2*theta) + y1*sin(2*theta);
      float y11 = x1*sin(2*theta) - y1*cos(2*theta);

      /* average of (x11,y11) and (x2,y2) */
      float xm = (x11+x2)/2;
      float ym = (y11+y2)/2;

      xs[i1] = xm*cos(2*theta) + ym*sin(2*theta);
      ys[i1] = xm*sin(2*theta) - ym*cos(2*theta);
      xs[i2] = xm;
      ys[i2] = ym;
    }
  Polygon2 *sym = new Polygon2 ();
  sym->input (xs, ys, n);
  //sym->write_file ("symmetry.dat");
}
