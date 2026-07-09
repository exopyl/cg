#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "curve_nurbs.h"

int
CurveNURBS::addControlPoint (float x, float y, float z, float weight)
{
	m_controlPoints.emplace_back (x, y, z);
	m_weights.push_back (weight);
	return 1;
}

int
CurveNURBS::setKnots (const float *knots, int size)
{
	m_knots.assign (knots, knots + size);
	m_degree = (int)m_knots.size () - (int)m_controlPoints.size () - 1;
	return 1;
}

/**
* Normalizes the knots
*/
int
CurveNURBS::normalizeKnots (void)
{
	float length = 0.0;
	for (float k : m_knots)
		length += k;

	if (length >= 0.00001)
	{
		for (float &k : m_knots)
			k /= length;
		return 1;
	}
	return 0;
}

/**
* B-spline basis function (Cox-de Boor recursion).
*
* @param i : index
* @param j : degree
* @param u : u in [0,1]
*
* ref :
* http://web.cs.wpi.edu/~matt/courses/cs563/talks/nurbs.html
* http://en.wikipedia.org/wiki/Non-uniform_rational_B-spline
*/
float
CurveNURBS::basisFunction (int i, int j, float u) const
{
	if (j == 0)
	{
		return (m_knots[i] <= u && u < m_knots[i+1]) ? 1.f : 0.f;
	}
	else
	{
		float n1 = (u - m_knots[i]) * basisFunction (i, j-1, u);
		float d1 = (m_knots[i+j] - m_knots[i]);
		float n2 = (m_knots[i+j+1] - u) * basisFunction (i+1, j-1, u);
		float d2 = (m_knots[i+j+1] - m_knots[i+1]);
		float a = 1.0, b = 1.0;
		if (d1 > 0.00001) a = n1 / d1;
		if (d2 > 0.00001) b = n2 / d2;
		return a + b;
	}
}

/**
* Position on the NURBS curve at t in [0,1].
*
*         sum(i = 0, n){w_i * P_i * N_i,k(u)}
*  C(u) = -------------------------------------
*                sum(i = 0, n){w_i * N_i,k(u)}
*/
bool
CurveNURBS::eval (float t, Vector3f &p) const
{
	const int n = (int)m_controlPoints.size ();
	if (n == 0 || m_knots.empty ())
		return false;

	if (t >= 1.0f) t = 1.0f; // for numerical robustness

	p.Set (0.f, 0.f, 0.f);
	float denom = 0.0;
	for (int i = 0; i < n; i++)
	{
		float tmp = m_weights[i] * basisFunction (i, m_degree, t);
		p += m_controlPoints[i] * tmp;
		denom += tmp;
	}
	if (denom > 0.00001f)
		p /= denom;
	return true;
}

int
CurveNURBS::tessellate (int nPoints, std::vector<Vector3f> &out) const
{
	out.clear ();
	if (nPoints < 2 || m_controlPoints.empty () || m_knots.empty ())
		return 0;
	out.reserve (nPoints);
	for (int i = 0; i < nPoints; i++)
	{
		float t = (float)i / (float)(nPoints - 1);
		Vector3f p;
		eval (t, p);
		out.push_back (p);
	}
	return (int)out.size ();
}

/**
* dump
*/
void
CurveNURBS::dump (void) const
{
	printf ("nControlPoints : %d\n", (int)m_controlPoints.size ());
	for (size_t i = 0; i < m_controlPoints.size (); i++)
		printf ("%zu : %f %f %f (%f)\n", i,
			m_controlPoints[i].x, m_controlPoints[i].y, m_controlPoints[i].z, m_weights[i]);

	printf ("m_degree : %d\n", m_degree);
	printf ("m_nKnots : %d\n", (int)m_knots.size ());
	for (size_t i = 0; i < m_knots.size (); i++)
		printf ("%zu : %f\n", i, m_knots[i]);
}

/**
* with gnuplot :
* plot "output.txt" using 1:2 with lines, ...
*/
void CurveNURBS::dumpAllBasisFunctions (int nPoints) const
{
	FILE *ptr = fopen ("output.txt", "w");
	if (!ptr)
		return;
	for (int i = 0; i < nPoints; i++)
	{
		float t = (float)i / nPoints;
		fprintf (ptr, "%f ", t);
		for (int j = 0; j < (int)m_controlPoints.size (); j++)
			fprintf (ptr, "%f ", basisFunction (j, m_degree, t));
		fprintf (ptr, "\n");
	}
	fclose (ptr);
}

void CurveNURBS::dumpBasisFunction (int index, int nPoints) const
{
	assert (index >= 0 && index < (int)m_controlPoints.size ());
	for (int i = 0; i < nPoints; i++)
	{
		float t = (float)i / nPoints;
		printf ("t : %f => %f\n", t, basisFunction (index, m_degree, t));
	}
}
