#include <stdio.h>
#include <math.h>

#include "curve_kappatau.h"
#include "common.h"

// total arc length spanned by the parameter t in [0,1]
static const float kArcLength = 3.f * 3.14f;
// integration step in arc length
static const float kDs = 0.01f;

float CurveKappaTau::curvature (float t) const
{
	return m_kappa ? m_kappa (t * kArcLength) : 0.f;
}

float CurveKappaTau::torsion (float t) const
{
	return m_tau ? m_tau (t * kArcLength) : 0.f;
}

// Integrate the Frenet-Serret equations up to arc length t*kArcLength.
bool CurveKappaTau::eval (float t, Vector3f &p) const
{
	if (!m_kappa || !m_tau)
		return false;

	float P[3] = {0.f, 0.f, 0.f};
	float T[3] = {0.f, 1.f, 0.f};
	float B[3] = {0.f, 0.f, 1.f};
	float N[3] = {-1.f, 0.f, 0.f};

	float sMax = t * kArcLength;
	for (float s = 0.f; s < sMax; s += kDs)
	{
		P[0] += kDs*T[0]; P[1] += kDs*T[1]; P[2] += kDs*T[2];

		T[0] += m_kappa(s)*kDs*N[0]; T[1] += m_kappa(s)*kDs*N[1]; T[2] += m_kappa(s)*kDs*N[2];
		float l = sqrtf (T[0]*T[0]+T[1]*T[1]+T[2]*T[2]);
		T[0] /= l; T[1] /= l; T[2] /= l;

		B[0] += -m_tau(s)*kDs*N[0]; B[1] += -m_tau(s)*kDs*N[1]; B[2] += -m_tau(s)*kDs*N[2];
		l = sqrtf (B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
		B[0] /= l; B[1] /= l; B[2] /= l;

		N[0] += B[1]*T[2] - B[2]*T[1];
		N[1] += B[2]*T[0] - B[0]*T[2];
		N[2] += B[0]*T[1] - B[1]*T[0];
		l = sqrtf (N[0]*N[0]+N[1]*N[1]+N[2]*N[2]);
		N[0] /= l; N[1] /= l; N[2] /= l;
	}

	p.Set (P[0], P[1], P[2]);
	return true;
}

void CurveKappaTau::Export (const char *filename) const
{
	if (!m_kappa || !m_tau)
		return;

	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return;
	unsigned int npointscircle = 40;

	float P[3] = {0.f, 0.f, 0.f};
	float T[3] = {0.f, 1.f, 0.f};
	float B[3] = {0.f, 0.f, 1.f};
	float N[3] = {-1.f, 0.f, 0.f};

	unsigned int nvertices = 0;
	for (float s = 0.f; s < kArcLength; s += kDs)
	{
		nvertices++;
		P[0] += kDs*T[0]; P[1] += kDs*T[1]; P[2] += kDs*T[2];

		T[0] += m_kappa(s)*kDs*N[0]; T[1] += m_kappa(s)*kDs*N[1]; T[2] += m_kappa(s)*kDs*N[2];
		float l = sqrtf (T[0]*T[0]+T[1]*T[1]+T[2]*T[2]);
		T[0] /= l; T[1] /= l; T[2] /= l;

		B[0] += -m_tau(s)*kDs*N[0]; B[1] += -m_tau(s)*kDs*N[1]; B[2] += -m_tau(s)*kDs*N[2];
		l = sqrtf (B[0]*B[0]+B[1]*B[1]+B[2]*B[2]);
		B[0] /= l; B[1] /= l; B[2] /= l;

		N[0] += B[1]*T[2] - B[2]*T[1];
		N[1] += B[2]*T[0] - B[0]*T[2];
		N[2] += B[0]*T[1] - B[1]*T[0];
		l = sqrtf (N[0]*N[0]+N[1]*N[1]+N[2]*N[2]);
		N[0] /= l; N[1] /= l; N[2] /= l;

		float radius = 0.01f;
		for (unsigned int k = 0; k < npointscircle; k++)
		{
			float angle = 2.f*M_PI*k/npointscircle;
			float d[3];
			d[0] = radius*(cosf(angle)*N[0]+sinf(angle)*B[0]);
			d[1] = radius*(cosf(angle)*N[1]+sinf(angle)*B[1]);
			d[2] = radius*(cosf(angle)*N[2]+sinf(angle)*B[2]);
			fprintf (ptr, "v %f %f %f\n", P[0]+d[0], P[1]+d[1], P[2]+d[2]);
		}
	}

	// faces
	unsigned int offset = 0;
	for (unsigned int j = 0; j < nvertices-1; j++)
	{
		unsigned int k = 0;
		for (k = 0; k < npointscircle-1; k++)
			fprintf (ptr, "f %d %d %d %d\n",
				 1+offset + npointscircle*j+k,
				 1+offset + npointscircle*j+1+k,
				 1+offset + npointscircle*(j+1)+1+k,
				 1+offset + npointscircle*(j+1)+k);

		fprintf (ptr, "f %d %d %d %d\n",
			 1+offset + npointscircle*j+k,
			 1+offset + npointscircle*j,
			 1+offset + npointscircle*(j+1),
			 1+offset + npointscircle*(j+1)+k);
	}

	// fill extremities
	for (unsigned int j = 0; j < npointscircle-2; j++)
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

	fclose (ptr);
}
