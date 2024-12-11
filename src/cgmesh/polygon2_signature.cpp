#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "polygon2.h"
#include "../cgmath/common.h"

void
Polygon2::search_symmetry_signature (int signature_type, int interpolation_type, int nbins)
{
	if (m_nContours != 1)
		return;
	
	int i,j;
	int n = m_nPoints[0];
	float *pPoints = m_pPoints[0];
	float *xx = (float*)malloc(n*sizeof(float));
	float *yy = (float*)malloc(n*sizeof(float));
	
	for (i=0; i<n; i++)
		xx[i] = yy[i] = 0.0;
	
	/* create the signature : signature_type */
	switch (signature_type)
	{
	case SIGNATURE_DEVIATION: /* deviation */
		for (i=0; i<n; i++)
		{
			/* before  : i1 */
			/* current : i2 */
			/* after   : i3 */
			int i1, i2, i3;
			i1 = i-1;
			i2 = i;
			i3 = i+1;
			if (i1 == -1) { i1 = n-1; xx[n-1] = 0.0; }
			if (i3 == n)  i3 = 0;
			
			/* length between v1 and v2 */
			xx[i] = xx[i-1] + sqrt ((pPoints[2*i2]-pPoints[2*i1])*(pPoints[2*i2]-pPoints[2*i1])
						+
						(pPoints[2*i2+1]-pPoints[2*i1+1])*(pPoints[2*i2+1]-pPoints[2*i1+1]));
			
			/* deviation */
			float v1x = pPoints[2*i2]-pPoints[2*i1];
			float v1y = pPoints[2*i2+1]-pPoints[2*i1+1];
			float l1 = sqrt (v1x*v1x+v1y*v1y);
			float v2x = pPoints[2*i3]-pPoints[2*i2];
			float v2y = pPoints[2*i3+1]-pPoints[2*i2+1];
			float l2 = sqrt (v2x*v2x+v2y*v2y);
			double dot_product = (v1x*v2x + v1y*v2y) / (l1*l2);
			if (dot_product >= 1.0) dot_product = 0.999999;
			yy[i] = acos (dot_product) * 180.0/3.14159;
			if (v1x*v2y-v1y*v2x < 0.0) yy[i] *= -1;
		}
		break;
	case SIGNATURE_CURVATURE: /* curvature */
		for (i=0; i<n; i++)
		{
			int i1, i2, i3;
			i1 = i-1;
			i2 = i;
			i3 = i+1;
			if (i1 == -1) i1 = n-1;
			if (i3 == n)  i3 = 0;
			
			/* length between v2 and v1 */
			xx[i] = xx[i-1] + sqrt ((pPoints[2*i2]-pPoints[2*i1])*(pPoints[2*i2]-pPoints[2*i1])
						+
						(pPoints[2*i2+1]-pPoints[2*i1+1])*(pPoints[2*i2+1]-pPoints[2*i1+1]));
			
			/* curvature by the computation of the radius of the circle */
			/* method found on http://perso.wanadoo.fr/math.15873/Cercl3p.html */
			float x1 = pPoints[2*i1];
			float y1 = pPoints[2*i1+1];
			float x2 = pPoints[2*i2];
			float y2 = pPoints[2*i2+1];
			float x3 = pPoints[2*i3];
			float y3 = pPoints[2*i3+1];
			float a1 = (y1-y3)/(x1-x3);
			float xe = (x1+x3)/2.0;
			float ye = (y1+y3)/2.0;
			float b1 = ye+xe/a1;
			float a2 = (y2-y3)/(x2-x3);
			float xf = (x2+x3)/2.0;
			float yf = (y2+y3)/2.0;
			float b2 = yf+xf/a2;
			float x0 = (b2-b1)/((1/a2)-(1/a1));
			float y0 = b1-x0/a1;
			float r = sqrt ((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0));
			if (r < 10000.0)
				yy[i] = 1.0/r;
			else
				yy[i] = 0.0;
		}
		break;
	case SIGNATURE_BOUTIN: /* Mireille Boutin */
		for (i=0; i<n; i++)
		{
			int i1, i2, i3, i4;
			i1 = (i-1+n)%n;
			i2 = (i+n)%n;
			i3 = (i+1+n)%n;
			i4 = (i+2+n)%n;
			
			float x1 = pPoints[2*i1], y1 = pPoints[2*i1+1];
			float x2 = pPoints[2*i2], y2 = pPoints[2*i2+1];
			float x3 = pPoints[2*i3], y3 = pPoints[2*i3+1];
			float x4 = pPoints[2*i4], y4 = pPoints[2*i4+1];
			
			xx[i] = xx[i1] + sqrt ((x4-x3)*(x4-x3) + (y4-y3)*(y4-y3));
			yy[i] = sqrt ((x4-x2)*(x4-x2) + (y4-y2)*(y4-y2));
			float delta123 = (x3-x1)*(y2-y1)-(y3-y1)*(x2-x1);
			float delta234 = (x4-x2)*(y3-y2)-(y4-y2)*(x3-x2);
			float sign = (delta123*delta234 >= 0.0)? 1.0 : -1.0;
			yy[i] *= sign;
		}
		break;
	default:
      ;
	}
	output_1array (xx, n, "xx.dat");
	output_1array (yy, n, "yy.dat");
	

	/* create the signal : interpolation_type, int nbins */
	float *signal;
	float length = xx[n-1];
	signal = (float*)malloc(nbins*sizeof(float)); 
	signal[0]       = yy[0];
	signal[nbins-1] = yy[n-1];
	j = 0;
	for (i=1; i<nbins-1; i++)
	{
		float ac = i*length/nbins;
		while (xx[j+1] < ac) j++;
		assert (j<n);
		
		/*** interpolations ***/
		switch (interpolation_type)
		{
		case INTERPOLATION_LINEAR: /* linear interpolation */
		{
			float t = (ac - xx[j]) / (xx[j+1] - xx[j]);
			signal[i] = yy[j]*(1-t) + yy[j+1]*t;
		}
		break;
		case INTERPOLATION_COSINE: /* cosine interpolation */
		{
			float t = (ac - xx[j]) / (xx[j+1] - xx[j]);
			float ft = t*3.14159;
			float f = (1-cos(ft))*0.5;
			signal[i] = yy[j]*(1-f) + yy[j+1]*f;
		}
		break;
		default:
			;
		}
	}
	output_1array (signal, nbins, "signal.dat");
	
	/* convolution */
	float *signal_convolution;
	convolution (signal, nbins, &signal_convolution);
	output_1array (signal_convolution, nbins, "convolution.dat");
	
	/* looking for the index of the maximum */
	int max = search_max (signal_convolution, nbins);
	
	/* interpret the max */
	float l = max*xx[n-1]/nbins;
	for (i=0; i<n; i++)
		if (xx[i+1]>l) break;
	float xp = pPoints[2*i];
	float yp = pPoints[2*i+1];
	float theta = acos ((xp) / (sqrt (xp*xp+yp*yp)));
	if (yp < 0.0) theta = 2*3.14159 - theta;
	while (theta >= 3.14159) theta -= 3.14159;
	printf ("---> signature's method : %f deg\n", theta*180.0/3.14159);
	
	/* equation of the line of symmetry */
	/* y = ax + b */
	float a = yp/xp;
	printf ("%f*x\n", a);
	
	delete (xx);
	delete (yy);
}
