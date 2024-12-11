#include <stdio.h>
#include <math.h>

#include "curve.h"
#include "common.h"

// T = v / |v|
void Curve::GetUnitTangentVector (float t, float T[3])
{
	float v[3];
	GetVelocity (t, v);
	float s;
	GetSpeed (t, &s);
	T[0] = v[0] / s;
	T[1] = v[1] / s;
	T[2] = v[2] / s;
}


// B = v x a / |v x a|
void Curve::GetUnitBinormalVector (float t, float B[3])
{
	float v[3], a[3];
	GetVelocity (t, v);
	GetAcceleration (t, a);
	B[0] = v[1]*a[2] - v[2]*a[1];
	B[1] = v[2]*a[0] - v[0]*a[2];
	B[2] = v[0]*a[1] - v[1]*a[0];
	float l = sqrt (B[0]*B[0] + B[1]*B[1] + B[2]*B[2]);
	B[0] /= l;
	B[1] /= l;
	B[2] /= l;
}

// N = B X T
void Curve::GetUnitPrincipalNormalVector (float t, float N[3])
{
	float B[3], T[3];
	GetUnitBinormalVector (t, B);
	GetUnitTangentVector (t, T);
	N[0] = B[1]*T[2] - B[2]*T[1];
	N[1] = B[2]*T[0] - B[0]*T[2];
	N[2] = B[0]*T[1] - B[1]*T[0];
	float l = sqrt (N[0]*N[0] + N[1]*N[1] + N[2]*N[2]);
	N[0] /= l;
	N[1] /= l;
	N[2] /= l;
}

// export
void Curve::Export (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	unsigned int npointscircle = 80;

	// vertices
	unsigned int nvertices = 0;
	float p[3], normal[3], binormal[3], d[3];
	for (float t=0; t<3.14159; t+=0.01)
	{
		nvertices++;
		GetPosition (t, p);
		GetUnitBinormalVector (t, binormal);
		GetUnitPrincipalNormalVector (t, normal);
		//printf ("binormal %f %f %f\n", binormal[0], binormal[1], binormal[2]);
		//printf ("normal %f %f %f\n", normal[0], normal[1], normal[2]);
		//printf ("\n");

		float radius = 0.2;
		for (unsigned int k=0; k<npointscircle; k++)
		{
			float angle = 2.*3.14159*k/npointscircle;
			d[0] = radius*(cos(angle)*normal[0]+sin(angle)*binormal[0]);
			d[1] = radius*(cos(angle)*normal[1]+sin(angle)*binormal[1]);
			d[2] = radius*(cos(angle)*normal[2]+sin(angle)*binormal[2]);
			
			fprintf (ptr, "v %f %f %f\n", p[0]+d[0], p[1]+d[1], p[2]+d[2]);
		}
	}
	
	// faces
	unsigned int offset = 0;
	unsigned int iface = 0;
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






void Curve01::GetPosition (float t, float p[3]) const
{
	p[0] = m_r*cos(t);
	p[1] = m_r*sin(t);
	p[2] = m_a*cos (m_m*t);
}

// v = dr / dt
void Curve01::GetVelocity (float t, float v[3]) const
{
	v[0] = -m_r*sin(t);
	v[1] =  m_r*cos(t);
	v[2] = -m_a*m_m*sin(m_m*t);
}

// a = d^2r / dt^2
void Curve01::GetAcceleration (float t, float a[3]) const
{
	a[0] = -m_r*cos(t);
	a[1] = -m_r*sin(t);
	a[2] = -m_a*m_m*m_m*cos(m_m*t);
}

// j = d^3r / dt^3
void Curve01::GetJerk (float t, float j[3]) const
{
	j[0] = 0.;
	j[1] = 0.;
	j[2] = 0.;
}

// |v|
void Curve01::GetSpeed (float t, float *s) const
{
	*s = sqrt (m_r*m_r + m_a*m_m*cos(m_m*t)*m_a*m_m*cos(m_m*t));
}

// \int_0^2\Pi |v|dt
void Curve01::GetArclength (float t, float *s) const
{
	*s = 0.;
}

// k = |dT / ds| = aN / |v|^2 = |v x a| / |v|^3
void Curve01::GetCurvature (float t, float *kappa) const
{
}

// t = 
void Curve01::GetTorsion (float t, float *nu) const
{
}

// a_T = aT = d^2s / dt^2
void Curve01::GetTangentialAcceleration (float t, float aT[3]) const
{
}

// a_N = aN = kappa nu^2
void Curve01::GetNormalAcceleration (float t, float aT[3]) const
{
}

//
// CurveHelical
//
void CurveHelical::GetPosition (float t, float p[3]) const
{
	p[0] = cos(m*t);
	p[1] = sin(m*t);
	p[2] = n*t;
}

// v = dr / dt
void CurveHelical::GetVelocity (float t, float v[3]) const
{
	v[0] = -m*sin(m*t);
	v[1] = m*cos(m*t);
	v[2] = n;
}

// a = d^2r / dt^2
void CurveHelical::GetAcceleration (float t, float a[3]) const
{
	a[0] = -m*m*cos(m*t);
	a[1] = -m*m*sin(m*t);
	a[2] = 0.;
}

// j = d^3r / dt^3
void CurveHelical::GetJerk (float t, float j[3]) const
{
	j[0] = m*m*m*sin(m*t);
	j[1] = -m*m*m*cos(m*t);
	j[2] = 0.;
}

// |v|
void CurveHelical::GetSpeed (float t, float *s) const
{
	*s = sqrt (m*m + n*n);
}

// \int_0^2\Pi |v|dt
void CurveHelical::GetArclength (float t, float *s) const
{
	*s = 0.;
}

// k = |dT / ds| = aN / |v|^2 = |v x a| / |v|^3
void CurveHelical::GetCurvature (float t, float *kappa) const
{
}

// t = 
void CurveHelical::GetTorsion (float t, float *nu) const
{
}

// a_T = aT = d^2s / dt^2
void CurveHelical::GetTangentialAcceleration (float t, float aT[3]) const
{
}

// a_N = aN = kappa nu^2
void CurveHelical::GetNormalAcceleration (float t, float aT[3]) const
{
}


//
// CurveWindingLineOnTorus
//
void CurveWindingLineOnTorus::GetPosition (float t, float r[3]) const
{
	r[0] = (p + q*cos(n*t)) * cos(m*t);
	r[1] = (p + q*cos(n*t)) * sin(m*t);
	r[2] = q*sin(n*t);
}

// dr / dt
void CurveWindingLineOnTorus::GetVelocity (float t, float v[3]) const
{
	v[0] = -q*n*sin(n*t)*cos(m*t) - (p + q*cos(n*t))*m*sin(m*t);
	v[1] = -q*n*sin(n*t)*sin(m*t) + (p + q*cos(n*t))*m*cos(m*t);
	v[2] = q*n*cos(n*t);
}

// d^2r / dt^2
void CurveWindingLineOnTorus::GetAcceleration (float t, float a[3]) const
{
	a[0] = -q*n*n*cos(n*t)*cos(m*t) + 2*q*n*sin(n*t)*m*sin(m*t) - (p + q*cos(n*t))*m*m*cos(m*t);
	a[1] = -q*n*n*cos(n*t)*sin(m*t) - 2*q*n*sin(n*t)*m*cos(m*t) - (p + q*cos(n*t))*m*m*sin(m*t);
	a[2] = -q*n*n*sin(n*t);
}

// d^3r / dt^3
void CurveWindingLineOnTorus::GetJerk (float t, float j[3]) const
{
	j[0] = 0.;
	j[1] = 0.;
	j[2] = 0.;
}

// |v|
void CurveWindingLineOnTorus::GetSpeed (float t, float *s) const
{
	*s = sqrt (p*p*m*m + 2*m*m*p*q*cos(n*t) + m*m*q*q*cos(n*t)*cos(n*t) + q*q*n*n);
}

// \int_0^2\Pi |v|dt
void CurveWindingLineOnTorus::GetArclength (float t, float *s) const
{
	
}

// k = |dT / ds| = aN / |v|^2 = |v x a| / |v|^3
void CurveWindingLineOnTorus::GetCurvature (float t, float *kappa) const
{
}

// t = 
void CurveWindingLineOnTorus::GetTorsion (float t, float *nu) const
{
}

// a_T = aT = d^2s / dt^2
void CurveWindingLineOnTorus::GetTangentialAcceleration (float t, float aT[3]) const
{
}

// a_N = aN = kappa nu^2
void CurveWindingLineOnTorus::GetNormalAcceleration (float t, float aT[3]) const
{
}
