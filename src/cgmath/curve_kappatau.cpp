#include <stdio.h>
#include <math.h>

#include "curve_kappatau.h"
#include "common.h"

void CurveKappaTau::Export (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	unsigned int npointscircle = 40;

	// vertices
	float P[3];
	P[0] = 0.; P[1] = 0.; P[2] = 0.;
	
	float T[3] = {0., 1., 0.};
	float B[3] = {0., 0., 1.};
	float N[3] = {-1., 0., 0.};
	
	float ds = 0.01f;
	unsigned int nvertices = 0;
	for (float s=0.; s<3*3.14; s+=ds)
	{
		nvertices++;
		P[0] += ds*T[0];
		P[1] += ds*T[1];
		P[2] += ds*T[2];
		//printf ("v %f %f %f\n", P[0], P[1], P[2]);
		
		T[0] += m_kappa(s)*ds*N[0];
		T[1] += m_kappa(s)*ds*N[1];
		T[2] += m_kappa(s)*ds*N[2];
		
		float l = sqrt (T[0]*T[0]+T[1]*T[1]+T[2]*T[2]);
		T[0] /= l;
		T[1] /= l;
		T[2] /= l;
		
		B[0] += -m_tau(s)*ds*N[0];
		B[1] += -m_tau(s)*ds*N[1];
		B[2] += -m_tau(s)*ds*N[2];
		
		l = sqrt (B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
		B[0] /= l;
		B[1] /= l;
		B[2] /= l;
		
		N[0] += B[1]*T[2] - B[2]*T[1];
		N[1] += B[2]*T[0] - B[0]*T[2];
		N[2] += B[0]*T[1] - B[1]*T[0];
		
		l = sqrt (N[0]*N[0]+N[1]*N[1]+N[2]*N[2]);
		N[0] /= l;
		N[1] /= l;
		N[2] /= l;
		
		//printf ("binormal %f %f %f\n", binormal[0], binormal[1], binormal[2]);
		//printf ("normal %f %f %f\n", normal[0], normal[1], normal[2]);
		//printf ("\n");
		
		float radius = 0.01f;
		for (unsigned int k=0; k<npointscircle; k++)
		{
			float angle = 2.*3.14159*k/npointscircle;
			float d[3];
			d[0] = radius*(cos(angle)*N[0]+sin(angle)*B[0]);
			d[1] = radius*(cos(angle)*N[1]+sin(angle)*B[1]);
			d[2] = radius*(cos(angle)*N[2]+sin(angle)*B[2]);
			
			fprintf (ptr, "v %f %f %f\n", P[0]+d[0], P[1]+d[1], P[2]+d[2]);
		}
	}
	
	// faces
	unsigned int offset = 0;
	for (unsigned int j=0; j<nvertices-1; j++)
	{
		unsigned int k=0;
		for (k=0; k<npointscircle-1; k++)
		{
			// quad
			fprintf (ptr, "f %d %d %d %d\n",
				 1+offset + npointscircle*j+k,
				 1+offset + npointscircle*j+1+k,
				 1+offset + npointscircle*(j+1)+1+k,
				 1+offset + npointscircle*(j+1)+k);
		}
		
		// quad
		fprintf (ptr, "f %d %d %d %d\n",
			 1+offset + npointscircle*j+k,
			 1+offset + npointscircle*j,
			 1+offset + npointscircle*(j+1),
			 1+offset + npointscircle*(j+1)+k);
	}
	
	// fill extremities
	for (unsigned int j=0; j<npointscircle-2; j++)
	{
		fprintf (ptr, "f %d %d %d\n",
			 1+offset + 0,
			 1+offset + j+1,
			 1+offset + j+2);
		
		
		fprintf (ptr, "f %d %d %d\n",
			 1+offset+npointscircle*nvertices - (npointscircle),
			 1+offset+npointscircle*nvertices - (npointscircle) +j+2,
			 1+offset+npointscircle*nvertices - (npointscircle) +j+1);
	}
	
	//offset+=npointscircle*stroke->npoints;

	fclose (ptr);
}
