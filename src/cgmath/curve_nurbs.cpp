#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "curve_nurbs.h"

/**
* Constructor
*/
CurveNURBS::CurveNURBS ()
{
	m_nMaxControlPoints = 4;
	m_controlPoints = (vec3*)malloc(m_nMaxControlPoints*sizeof(vec3));
	m_weights = (float*)malloc(m_nMaxControlPoints*sizeof(float));

	m_nKnots = 0;
	m_knots = NULL;
	m_nControlPoints = 0;
}

/**
* Copy Constructor
*/
CurveNURBS::CurveNURBS (const CurveNURBS &curveNURBS)
{
	m_nMaxControlPoints = curveNURBS.m_nMaxControlPoints;

	m_controlPoints = (vec3*)malloc(m_nMaxControlPoints*sizeof(vec3));
	memcpy (m_controlPoints, curveNURBS.m_controlPoints, m_nMaxControlPoints*sizeof(vec3));
	m_nControlPoints = curveNURBS.m_nControlPoints;

	m_weights = (float*)malloc(m_nMaxControlPoints*sizeof(float));
	memcpy (m_weights, curveNURBS.m_weights, m_nMaxControlPoints*sizeof(float));

	m_degree = curveNURBS.m_degree;

	m_nKnots = curveNURBS.m_nKnots;
	if (curveNURBS.m_knots)
		memcpy (m_knots, curveNURBS.m_knots, m_nKnots*sizeof(float));
}

/**
* Destructor
*/
CurveNURBS::~CurveNURBS ()
{
	if (m_controlPoints)
		free (m_controlPoints);
	if (m_weights)
		free (m_weights);
	if (m_knots)
		free (m_knots);
}

/**
* Add a new control point
*
* /return 1 if the new control point is correctly added, 0 otherwise
*/
int
CurveNURBS::addControlPoint (float x, float y, float z, float weight)
{
	if (m_nControlPoints == m_nMaxControlPoints)
	{
		m_nMaxControlPoints *= 2;
		m_controlPoints = (vec3*)realloc((void*)m_controlPoints, m_nMaxControlPoints*sizeof(vec3));
		if (m_controlPoints == NULL) return 0;
		m_weights = (float*)realloc((void*)m_weights, m_nMaxControlPoints*sizeof(vec3));
		if (m_weights == NULL) return 0;
	}
	m_controlPoints[m_nControlPoints][0] = x;
	m_controlPoints[m_nControlPoints][1] = y;
	m_controlPoints[m_nControlPoints][2] = z;
	m_weights[m_nControlPoints] = weight;
	m_nControlPoints++;
	return 1;
}

/**
*
*/
int
CurveNURBS::setKnots (float *knots, int size)
{
	m_nKnots = size;
	m_degree = m_nKnots - m_nControlPoints - 1;
	m_knots = (float*)malloc(m_nKnots*sizeof(float));
	m_knots = (float*)memcpy ((void*)m_knots, (void*)knots, m_nKnots*sizeof(float));
	return 1;
}

/**
* Normalizes the knots
*/
int
CurveNURBS::normalizeKnots (void)
{
	int i;
	float length = 0.0;

	for (i=0; i<m_nKnots; i++)
		length += m_knots[i];

	if (length >= 0.00001)
	{
		for (i=0; i<m_nKnots; i++)
			m_knots[i] /= length;
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
* compute nPoints along the NURBS curve
*
* @param i : index
* @param j : degree
* @param u : u \in [0,1]
*
* @return 1 if the new control point is correctly added, 0 otherwise
* 
* ref :
* http://web.cs.wpi.edu/~matt/courses/cs563/talks/nurbs.html
* http://www.nar-associates.com/nurbs/c_code.html#chapter4
* http://en.wikipedia.org/wiki/Non-uniform_rational_B-spline
*/
float
CurveNURBS::basisFunction (int i, int j, float u)
{
	/*
			u - t_i
	   N_i,k(u) = ----------- * N_i,k-1(u) +
	              t_i+k - t_i

		            t_i+k+1 - u
			   ---------------  * N_i+1,k-1(u)
			   t_i+k+1 - t_i+1

	and
			    / 1, if t_i <= u < t_i+1
		N_i,0(u) = <
                            \ 0, else

	*/
	if (j==0)
	{
		printf ("%d\n", m_knots[i] <= u && u < m_knots[i+1]);
		return (m_knots[i] <= u && u < m_knots[i+1])? 1. : 0.;
	}
	else
	{
		//assert (i>=0 && i+j+1<m_nKnots);
		float n1 = (u-m_knots[i])*basisFunction(i,j-1,u);
		float d1 = (m_knots[i+j]-m_knots[i]);
		float n2 = (m_knots[i+j+1]-u)*basisFunction(i+1,j-1,u);
		float d2 = (m_knots[i+j+1]-m_knots[i+1]);
		float a = 1.0, b = 1.0;
		if (d1 > 0.00001) a = n1 / d1;
		if (d2 > 0.00001) b = n2 / d2;
		printf ("a+b = %f\n", a+b);
		return a + b;
	}
}

int
CurveNURBS::computeInterpolation (int _nPoints, vec3 **_points)
{
	if (_nPoints < 0)
	{
		_points = NULL;
		return 0;
	}
	vec3 *points = (vec3*)malloc(_nPoints*sizeof(vec3));
	if (!points)
	{
		_points = NULL;
		return 0;
	}
	float t = 0.0;
	for (int iPoint=0; iPoint<_nPoints; iPoint++, t += 1.0f/(_nPoints-1.0f))
	{
		printf ("=> %f\n", t);
		if (t >= 1.0) t = 1.0; // for numerical robustness
/*
			sum(i = 0, n){w_i * P_i * N_i,k(u)}
		C(u) = -------------------------------------,
	   		  sum(i = 0, n){w_i * N_i,k(u)}

	where

		w_i : weights
		P_i : control points (vector)
		N_i,k : normalized B-spline basis functions of degree k
*/
		points[iPoint][0] = points[iPoint][1] = points[iPoint][2] = 0.0;
		float denom = 0.0;
		//printf ("iPoint : %d\n", iPoint);
		for (int i=0; i<m_nControlPoints; i++)
		{
			//printf ("%d %f ", i, t);
			float tmp = m_weights[i] * basisFunction(i,m_degree,t);
			//printf (" => %f\n", tmp);

			points[iPoint][0] += tmp * m_controlPoints[i][0];
			points[iPoint][1] += tmp * m_controlPoints[i][1];
			points[iPoint][2] += tmp * m_controlPoints[i][2];
			denom += tmp;
		}
		if (denom > 0.00001)
		{
			points[iPoint][0] /= denom;
			points[iPoint][1] /= denom;
			points[iPoint][2] /= denom;
		}
	}
	*_points = points;

	return 1;
}

/**
* dump
*/
void
CurveNURBS::dump (void)
{
	int i;

	printf ("nControlPoints : %d\n", m_nControlPoints);
	for (i=0; i<m_nControlPoints; i++)
		printf ("%d : %f %f %f (%f)\n", i, m_controlPoints[i][0], m_controlPoints[i][1], m_controlPoints[i][2], m_weights[i]);

	printf ("m_degree : %d\n", m_degree);
	printf ("m_mKnots : %d\n", m_nKnots);
	for (i=0; i<m_nKnots; i++)
		printf ("%d : %f\n", i, m_knots[i]);
}

/**
* with gnuplot :
* plot "output.txt" using 1:2 with lines, "output.txt" using 1:3 with lines, "output.txt" using 1:4 with lines, "output.txt" using 1:5 with lines, "output.txt" using 1:6 with lines, "output.txt" using 1:7 with lines
*/
void CurveNURBS::dumpAllBasisFunctions (int nPoints)
{
	FILE *ptr = fopen ("output.txt", "w");
	for (int i=0; i<nPoints; i++)
	{
		float t = (float)i/nPoints;
		fprintf (ptr, "%f ", t);
		for (int j=0; j<m_nControlPoints; j++)
		{
			float value = basisFunction (j, m_degree, t);
			fprintf (ptr, "%f ", value);
		}
		fprintf (ptr, "\n");
	}
	fclose (ptr);
}


void CurveNURBS::dumpBasisFunction (int index, int nPoints)
{
	assert (index >= 0 && index < m_nControlPoints);
	for (int i=0; i<nPoints; i++)
	{
		float t = (float)i/nPoints;
		float value = basisFunction (index, m_degree, t);
		printf ("t : %f => %f\n", t, value);
	}
}

