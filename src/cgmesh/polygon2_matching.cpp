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
	
	Polygon2 *pol1 = new Polygon2 ();
	pol1->input (this, INTERPOLATION_LINEAR, nn);
	Polygon2 *pol2 = new Polygon2 ();
	pol2->input (pol, INTERPOLATION_LINEAR, nn);
	
	float *thetaA = (float*)malloc(nn*sizeof(float));
	float *thetaB = (float*)malloc(nn*sizeof(float));
	
	/* init angle with resoect to axis 0x : angle0 in [0,2*Pi] */
	vec3 ox, v;
	vec3_init (ox, 1., 0., 0.);
	vec3_init (v, pol1->m_pPoints[2*1] - pol1->m_pPoints[2*0], pol1->m_pPoints[2*1+1] - pol1->m_pPoints[2*0+1], 0.);
	vec3_normalize (v);
	
	float angle0 = acos (vec3_dot_product (ox, v));
	if (v[1] < 0.0) angle0 = 2*3.14159 - angle0;
	thetaA[0] = angle0;
	
	vec3_init (v, pol2->m_pPoints[2*1] - pol2->m_pPoints[2*0], pol2->m_pPoints[2*1+1] - pol2->m_pPoints[2*0+1], 0.0);
	vec3_normalize (v);
	angle0 = acos (vec3_dot_product (ox, v));
	if (v[1] < 0.0) angle0 = 2*3.14159 - angle0;
	thetaB[0] = angle0;
	
	for (i=1; i<nn; i++)
	{
		/* fill the theta arrays with respect to deviation */
		float deviation;
		
		vec3 v3;
		vec3 v1;
		vec3_init (v1, pol1->m_pPoints[2*((i+1)%nn)] - pol1->m_pPoints[2*i+1], pol1->m_pPoints[2*((i+1)%nn)+1] - pol1->m_pPoints[2*i+1], 0.0);
		vec3 v2;
		vec3_init (v2, pol1->m_pPoints[2*((i+2)%nn)] - pol1->m_pPoints[2*((i+1)%nn)], pol1->m_pPoints[2*((i+2)%nn)+1] - pol1->m_pPoints[2*((i+1)%nn)+1], 0.0);
		vec3_normalize (v1);
		vec3_normalize (v2);
		deviation = acos (vec3_dot_product (v1, v2));
		vec3_cross_product (v3, v1, v2);
		if (v3[2] >= 0.0) thetaA[i] = thetaA[i-1] + deviation;
		else              thetaA[i] = thetaA[i-1] - deviation;
		
		vec3_init (v1, pol2->m_pPoints[2*((i+1)%nn)] - pol2->m_pPoints[2*i], pol2->m_pPoints[2*((i+1)%nn)+1] - pol2->m_pPoints[2*i+1], 0.0);
		vec3_init (v2, pol2->m_pPoints[2*((i+2)%nn)] - pol2->m_pPoints[2*((i+1)%nn)], pol2->m_pPoints[2*((i+2)%nn)+1] - pol2->m_pPoints[2*((i+1)%nn)+1], 0.0);
		vec3_normalize (v1);
		vec3_normalize (v2);
		deviation = acos (vec3_dot_product (v1, v2));
		vec3_cross_product (v3, v1, v2);
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
