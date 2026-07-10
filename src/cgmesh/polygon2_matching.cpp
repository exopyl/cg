#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "polygon2.h"
#include "../cgmath/cgmath.h"

float
Polygon2::matching_arkin (Polygon2 *pol, int nn)
{
	int i,s,t;
	// acos of a dot product of unit vectors can exceed [-1,1] by rounding -> NaN;
	// clamp so identical shapes give a finite (0) turning-function difference.
	auto sacos = [](float d){ if (d > 1.f) d = 1.f; else if (d < -1.f) d = -1.f; return (float)acos(d); };

	Polygon2 *pol1 = new Polygon2 ();
	pol1->input (this, INTERPOLATION_LINEAR, nn);
	Polygon2 *pol2 = new Polygon2 ();
	pol2->input (pol, INTERPOLATION_LINEAR, nn);

	// alias the interleaved x,y storage as flat float arrays
	float *pA = (float*)pol1->m_contours[0].data();
	float *pB = (float*)pol2->m_contours[0].data();

	float *thetaA = (float*)malloc(nn*sizeof(float));
	float *thetaB = (float*)malloc(nn*sizeof(float));

	/* init angle with resoect to axis 0x : angle0 in [0,2*Pi] */
	Vector3f ox, v;
	ox.Set (1., 0., 0.);
	v.Set (pA[2*1] - pA[2*0], pA[2*1+1] - pA[2*0+1], 0.);
	(v).Normalize ();

	float angle0 = sacos ((ox).DotProduct (v));
	if (v[1] < 0.0) angle0 = 2*3.14159 - angle0;
	thetaA[0] = angle0;

	v.Set (pB[2*1] - pB[2*0], pB[2*1+1] - pB[2*0+1], 0.0);
	(v).Normalize ();
	angle0 = sacos ((ox).DotProduct (v));
	if (v[1] < 0.0) angle0 = 2*3.14159 - angle0;
	thetaB[0] = angle0;
	
	for (i=1; i<nn; i++)
	{
		/* fill the theta arrays with respect to deviation */
		float deviation;
		
		Vector3f v3;
		Vector3f v1;
		v1.Set (pA[2*((i+1)%nn)] - pA[2*i], pA[2*((i+1)%nn)+1] - pA[2*i+1], 0.0);
		Vector3f v2;
		v2.Set (pA[2*((i+2)%nn)] - pA[2*((i+1)%nn)], pA[2*((i+2)%nn)+1] - pA[2*((i+1)%nn)+1], 0.0);
		(v1).Normalize ();
		(v2).Normalize ();
		deviation = sacos ((v1).DotProduct (v2));
		v3 = (v1).CrossProduct (v2);
		if (v3[2] >= 0.0) thetaA[i] = thetaA[i-1] + deviation;
		else              thetaA[i] = thetaA[i-1] - deviation;

		v1.Set (pB[2*((i+1)%nn)] - pB[2*i], pB[2*((i+1)%nn)+1] - pB[2*i+1], 0.0);
		v2.Set (pB[2*((i+2)%nn)] - pB[2*((i+1)%nn)], pB[2*((i+2)%nn)+1] - pB[2*((i+1)%nn)+1], 0.0);
		(v1).Normalize ();
		(v2).Normalize ();
		deviation = sacos ((v1).DotProduct (v2));
		v3 = (v1).CrossProduct (v2);
		if (v3[2] >= 0.0) thetaB[i] = thetaB[i-1] + deviation;
		else              thetaB[i] = thetaB[i-1] - deviation;
	}
	output_1array (thetaA, nn, "thetaA.dat");
	output_1array (thetaB, nn, "thetaB.dat");
	
	/* compute the function d to minimize */
	float *d = (float*)malloc(nn*sizeof(float));
	assert (d);
	
	/* pre computation */
	float intA = 0.0;
	float intB = 0.0;
	for (s=0; s<nn; s++)
	{
		intA += thetaA[s]/nn;
		intB += thetaB[s]/nn;
	}
	
	/* computation */
	for (t=0; t<nn; t++)
	{
		float tmp, term1, term2;
		
		term1 = 0.0;
		for (s=0; s<nn; s++)
		{
			tmp = (thetaA[(s+t)%nn] - thetaB[s]);//2*3.14159*((s+t)) - 2*3.14159*s;
			if ((s+t)%nn != (s+t))
				tmp += 2*3.14159;
			tmp /= nn;
			term1 += tmp * tmp;
		}
		term1 *= nn;
		
		tmp = (intB - intA - 2*3.14159*t/nn);
		term2 = tmp * tmp;
		
		tmp =  2*3.14159*t/nn;
		
		//assert (term1 - term2 >= 0);
		d[t] = sqrt (fabs(term1 - term2));
	}
	output_1array (d, nn, "d.dat");
	
	int index = search_min (d, nn);
	printf ("t = %d\n", index);
	printf ("r = %f\n", (intB - intA - 2*3.14159*index/nn)*180.0/3.14159);
	printf ("d -> %f\n", d[index]);
	
	return d[index];
}
