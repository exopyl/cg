#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "curve_bezier.h"
#include "berstein.h"
#include "algebra_vector3.h"

/**
* Constructor
*/
CurveBezier::CurveBezier ()
{
	m_nMaxControlPoints = 4;
	m_controlPoints = (vec3*)malloc(m_nMaxControlPoints*sizeof(vec3));
	assert (m_controlPoints);
	m_nControlPoints = 0;
}

/**
* Copy Constructor
*/
CurveBezier::CurveBezier (const CurveBezier &par_curveBezier)
{
	m_nMaxControlPoints = par_curveBezier.m_nMaxControlPoints;
	m_controlPoints = (vec3*)malloc(m_nMaxControlPoints*sizeof(vec3));
	assert (m_controlPoints);
	memcpy (m_controlPoints, par_curveBezier.m_controlPoints, m_nMaxControlPoints*sizeof(vec3));
	m_nControlPoints = par_curveBezier.m_nControlPoints;
}

/**
* Destructor
*/
CurveBezier::~CurveBezier ()
{
	if (m_controlPoints) free (m_controlPoints);
}

/**
* add a new control point
*
* /return 1 if the new control point is correctly added, 0 otherwise
*/
int CurveBezier::addControlPoint (vec3 v)
{
	return addControlPoint (v[0], v[1], v[2]);
}

int CurveBezier::addControlPoint (float x, float y, float z)
{
	if (m_nControlPoints == m_nMaxControlPoints)
	{
		m_nMaxControlPoints *= 2;
		m_controlPoints = (vec3*)realloc((void*)m_controlPoints, m_nMaxControlPoints*sizeof(vec3));
		if (m_controlPoints == NULL) return 0;
	}
	m_controlPoints[m_nControlPoints][0] = x;
	m_controlPoints[m_nControlPoints][1] = y;
	m_controlPoints[m_nControlPoints][2] = z;
	m_nControlPoints++;
	return 1;
}

int CurveBezier::eval (float t, vec3 pt)
{
	vec3_init (pt, 0., 0., 0.);
	for (int i=0; i<m_nControlPoints; i++)
	{
		pt[0] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][0];
		pt[1] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][1];
		pt[2] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][2];
	}
	return 0;
}

int CurveBezier::eval_on_x (float x, vec3 pt)
{
	float t, t1 = 0., t2 = 1.;
	vec3 pt1, pt2;
	eval (t1, pt1);
	eval (t2, pt2);
	if (pt1[0] > x && pt2[0] < x)
		return -1;
	
	do {
		t = (t1+t2)/2.;
		eval (t, pt);
		if (pt[0] < x)
		{
			t1 = t;
			vec3_copy (pt1, pt);
		}
		else
		{
			t2 = t;
			vec3_copy (pt2, pt);
		}
	} while (fabs(t1-t2) > 0.000001);

	return 0;
}

/**
* compute nPoints along the Bézier curve
*
* /return 1 if the new control point is correctly added, 0 otherwise
*/
int
CurveBezier::computeInterpolation (int par_nPoints, vec3 **par_points)
{
	if (par_nPoints < 0)
	{
		par_points = NULL;
		return 0;
	}
	vec3 *points = (vec3*)malloc(par_nPoints*sizeof(vec3));
	if (!points)
	{
		par_points = NULL;
		return 0;
	}
	float t = 0.0;
	for (int iPoint=0; iPoint<par_nPoints; iPoint++, t += 1.0f/(par_nPoints-1.0f))
	{
		points[iPoint][0] = points[iPoint][1] = points[iPoint][2] = 0.0;
		for (int i=0; i<m_nControlPoints; i++)
		{
			points[iPoint][0] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][0];
			points[iPoint][1] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][1];
			points[iPoint][2] += (float)bersteinPolynomial(m_nControlPoints-1,i,t) * m_controlPoints[i][2];
		}
	}
	*par_points = points;
	return 1;
}

/**
* compute nPoints along the Bézier curve (quadratic curve, ie 3 control points, ie degree = 2)
*
* /return 1 if the new control point is correctly added, 0 otherwise
*/
int
CurveBezier::computeInterpolation3 (int par_nPoints, vec3 **par_points)
{
	if (getDegree () != 2)
	{
		par_points = NULL;
		return 0;
	}
	vec3 *points = (vec3*)malloc(par_nPoints*sizeof(vec3));
	if (!points)
	{
		par_points = NULL;
		return 0;
	}
	float t = 0.0;
	for (int iPoint=0; iPoint<par_nPoints; iPoint++, t += 1.0f/(par_nPoints-1))
	{
		float t2 = t * t;
		float tm1 = 1.0f - t;
		float tm12 = tm1 * tm1;

		points[iPoint][0] = tm12 * m_controlPoints[0][0] + 2.0f * tm1 * t * m_controlPoints[1][0] + t2 * m_controlPoints[2][0];
		points[iPoint][1] = tm12 * m_controlPoints[0][1] + 2.0f * tm1 * t * m_controlPoints[1][1] + t2 * m_controlPoints[2][1];
		points[iPoint][2] = tm12 * m_controlPoints[0][2] + 2.0f * tm1 * t * m_controlPoints[1][2] + t2 * m_controlPoints[2][2];
	}
	*par_points = points;
	return 1;
}

/**
* compute nPoints along the Bézier curve (cubic curve, ie 4 control points, ie degree = 3)
*
* /return 1 if the new control point is correctly added, 0 otherwise
*/
int
CurveBezier::computeInterpolation4 (int par_nPoints, vec3 **par_points)
{
	if (getDegree () != 3)
	{
		par_points = NULL;
		return 0;
	}
	vec3 *points = (vec3*)malloc(par_nPoints*sizeof(vec3));
	if (!points)
	{
		par_points = NULL;
		return 0;
	}
	float t = 0.0;
	for (int iPoint=0; iPoint<par_nPoints; iPoint++, t += 1.0f/(par_nPoints-1.0f))
	{
		float t3 = t * t * t;
		float tm1 = 1.0f - t;
		float tm13 = tm1 * tm1 * tm1;

		points[iPoint][0] = tm13 * m_controlPoints[0][0] + 3.0f * tm1 * tm1 * t * m_controlPoints[1][0] + 3.0f * tm1 * t * t * m_controlPoints[2][0] + t3 * m_controlPoints[3][0];
		points[iPoint][1] = tm13 * m_controlPoints[0][1] + 3.0f * tm1 * tm1 * t * m_controlPoints[1][1] + 3.0f * tm1 * t * t * m_controlPoints[2][1] + t3 * m_controlPoints[3][1];
		points[iPoint][2] = tm13 * m_controlPoints[0][2] + 3.0f * tm1 * tm1 * t * m_controlPoints[1][2] + 3.0f * tm1 * t * t * m_controlPoints[2][2] + t3 * m_controlPoints[3][2];
	}
	*par_points = points;
	return 1;
}


/**
* Interpolation by recursive approach
*/
int
CurveBezier::computeInterpolationRecursive3aux (int par_level, vec3 **par_points, int par_pos)
{
	vec3 *points = *par_points;
    if (par_level <= 0)
	{
		points[par_pos][0]   = m_controlPoints[0][0];
		points[par_pos][1]   = m_controlPoints[0][1];
		points[par_pos][2]   = m_controlPoints[0][2];
		points[par_pos+1][0] = m_controlPoints[3][0];
		points[par_pos+1][1] = m_controlPoints[3][1];
		points[par_pos+1][2] = m_controlPoints[3][2];
    }
	else
	{
        // subdivide into 2 Bezier segments
		CurveBezier *left = new CurveBezier ();
		vec3 l1, l2, l3, l4;
		l1[0] = m_controlPoints[0][0];
		l1[1] = m_controlPoints[0][1];
		l1[2] = m_controlPoints[0][2];
		l2[0] = (m_controlPoints[0][0] + m_controlPoints[1][0] ) / 2.0f;
		l2[1] = (m_controlPoints[0][1] + m_controlPoints[1][1] ) / 2.0f;
		l2[2] = (m_controlPoints[0][2] + m_controlPoints[1][2] ) / 2.0f;
		l3[0] = (m_controlPoints[0][0] + 2.0f*m_controlPoints[1][0] + m_controlPoints[2][0] ) / 4.0f;
		l3[1] = (m_controlPoints[0][1] + 2.0f*m_controlPoints[1][1] + m_controlPoints[2][1] ) / 4.0f;
		l3[2] = (m_controlPoints[0][2] + 2.0f*m_controlPoints[1][2] + m_controlPoints[2][2] ) / 4.0f;
		l4[0] = (m_controlPoints[0][0] + 3.0f*m_controlPoints[1][0] + 3.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 8.0f;
		l4[1] = (m_controlPoints[0][1] + 3.0f*m_controlPoints[1][1] + 3.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 8.0f;
		l4[2] = (m_controlPoints[0][2] + 3.0f*m_controlPoints[1][2] + 3.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 8.0f;
		left->addControlPoint (l1);
		left->addControlPoint (l2);
		left->addControlPoint (l3);
		left->addControlPoint (l4);
		left->computeInterpolationRecursive3aux (par_level-1, par_points, par_pos);
		delete left;

		CurveBezier *right = new CurveBezier ();
		vec3 r1, r2, r3, r4;
		r1[0] = (m_controlPoints[0][0] + 3.0f*m_controlPoints[1][0] + 3.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 8.0f;
		r1[1] = (m_controlPoints[0][1] + 3.0f*m_controlPoints[1][1] + 3.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 8.0f;
		r1[2] = (m_controlPoints[0][2] + 3.0f*m_controlPoints[1][2] + 3.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 8.0f;
		r2[0] = (m_controlPoints[1][0] + 2.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 4.0f;
		r2[1] = (m_controlPoints[1][1] + 2.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 4.0f;
		r2[2] = (m_controlPoints[1][2] + 2.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 4.0f;
		r3[0] = (m_controlPoints[2][0] + m_controlPoints[3][0] ) / 2.0f;
		r3[1] = (m_controlPoints[2][1] + m_controlPoints[3][1] ) / 2.0f;
		r3[2] = (m_controlPoints[2][2] + m_controlPoints[3][2] ) / 2.0f;
		r4[0] = m_controlPoints[3][0];
		r4[1] = m_controlPoints[3][1];
		r4[2] = m_controlPoints[3][2];
		right->addControlPoint (r1);
		right->addControlPoint (r2);
		right->addControlPoint (r3);
		right->addControlPoint (r4);
		right->computeInterpolationRecursive3aux (par_level-1, par_points, par_pos+(int)pow (2.0, par_level-1));
		delete right;
    }

	return 0;
}

int
CurveBezier::computeInterpolationRecursive3 (int par_level, int &par_nPoints, vec3 **par_points)
{
	if (getDegree () != 3) return 0;
	par_nPoints = 1 + (int)pow (2.0,par_level);

	vec3 *points = (vec3*)malloc(par_nPoints*sizeof(vec3));
	if (!points)
	{
		par_points = NULL;
		return 0;
	}

	computeInterpolationRecursive3aux (par_level, &points, 0);
	*par_points = points;

	return 1;
}

/**
* Interpolation by recursive approach
*/
bool
CurveBezier::isSufficentlyFlat (float tolerance)
{
	assert (getDegree () == 3);

	dump ();
	double ux = 3.0*m_controlPoints[1][0] - 2.0*m_controlPoints[0][0] - m_controlPoints[3][0]; ux *= ux;
	double uy = 3.0*m_controlPoints[1][1] - 2.0*m_controlPoints[0][1] - m_controlPoints[3][1]; uy *= uy;
	double vx = 3.0*m_controlPoints[2][0] - 2.0*m_controlPoints[3][0] - m_controlPoints[0][0]; vx *= vx;
	double vy = 3.0*m_controlPoints[2][1] - 2.0*m_controlPoints[3][1] - m_controlPoints[0][1]; vy *= vy;

	if (ux < vx) ux = vx;
	if (uy < vy) uy = vy;
	printf ("d = %f\n", ux+uy);
	return (ux+uy <= tolerance); /* tolerance is 16*tol^2 */
}

int
CurveBezier::computeInterpolationRecursiveFlatness3 (float tolerance, int &par_nPoints, vec3 **par_points)
{
	if (getDegree () != 3) return 0;

	if (isSufficentlyFlat (tolerance) == false)
	{
		printf ("false\n");

        // subdivide into 2 Bezier segments
		vec3 l1, l2, l3, l4;
		l1[0] = m_controlPoints[0][0];
		l1[1] = m_controlPoints[0][1];
		l1[2] = m_controlPoints[0][2];
		l2[0] = (m_controlPoints[0][0] + m_controlPoints[1][0] ) / 2.0f;
		l2[1] = (m_controlPoints[0][1] + m_controlPoints[1][1] ) / 2.0f;
		l2[2] = (m_controlPoints[0][2] + m_controlPoints[1][2] ) / 2.0f;
		l3[0] = (m_controlPoints[0][0] + 2.0f*m_controlPoints[1][0] + m_controlPoints[2][0] ) / 4.0f;
		l3[1] = (m_controlPoints[0][1] + 2.0f*m_controlPoints[1][1] + m_controlPoints[2][1] ) / 4.0f;
		l3[2] = (m_controlPoints[0][2] + 2.0f*m_controlPoints[1][2] + m_controlPoints[2][2] ) / 4.0f;
		l4[0] = (m_controlPoints[0][0] + 3.0f*m_controlPoints[1][0] + 3.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 8.0f;
		l4[1] = (m_controlPoints[0][1] + 3.0f*m_controlPoints[1][1] + 3.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 8.0f;
		l4[2] = (m_controlPoints[0][2] + 3.0f*m_controlPoints[1][2] + 3.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 8.0f;
		CurveBezier *left = new CurveBezier ();
		left->addControlPoint (l1);
		left->addControlPoint (l2);
		left->addControlPoint (l3);
		left->addControlPoint (l4);
		int leftNPoints;
		vec3 *leftPoints;
		left->computeInterpolationRecursiveFlatness3 (tolerance, leftNPoints, &leftPoints);
		printf ("leftNPoints : %d\n", leftNPoints);
		delete left;

		vec3 r1, r2, r3, r4;
		r1[0] = (m_controlPoints[0][0] + 3.0f*m_controlPoints[1][0] + 3.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 8.0f;
		r1[1] = (m_controlPoints[0][1] + 3.0f*m_controlPoints[1][1] + 3.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 8.0f;
		r1[2] = (m_controlPoints[0][2] + 3.0f*m_controlPoints[1][2] + 3.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 8.0f;
		r2[0] = (m_controlPoints[1][0] + 2.0f*m_controlPoints[2][0] + m_controlPoints[3][0] ) / 4.0f;
		r2[1] = (m_controlPoints[1][1] + 2.0f*m_controlPoints[2][1] + m_controlPoints[3][1] ) / 4.0f;
		r2[2] = (m_controlPoints[1][2] + 2.0f*m_controlPoints[2][2] + m_controlPoints[3][2] ) / 4.0f;
		r3[0] = (m_controlPoints[2][0] + m_controlPoints[3][0] ) / 2.0f;
		r3[1] = (m_controlPoints[2][1] + m_controlPoints[3][1] ) / 2.0f;
		r3[2] = (m_controlPoints[2][2] + m_controlPoints[3][2] ) / 2.0f;
		r4[0] = m_controlPoints[3][0];
		r4[1] = m_controlPoints[3][1];
		r4[2] = m_controlPoints[3][2];
		CurveBezier *right = new CurveBezier ();
		right->addControlPoint (r1);
		right->addControlPoint (r2);
		right->addControlPoint (r3);
		right->addControlPoint (r4);
		int rightNPoints;
		vec3 *rightPoints;
		left->computeInterpolationRecursiveFlatness3 (tolerance, rightNPoints, &rightPoints);
		printf ("rightNPoints : %d\n", rightNPoints);
		delete right;

		//printf ("%d %d\n", leftNPoints, rightNPoints);
		int nPoints = leftNPoints + rightNPoints - 1;
		printf ("nPoints : %d\n", nPoints);
		vec3 *points = (vec3*)malloc(nPoints*sizeof(vec3));
		if (!points)
		{
			par_nPoints = 0;
			par_points = NULL;
			return 0;
		}
		int i,j;
		for (i=0; i<leftNPoints; i++)
		{
			points[i][0] = leftPoints[i][0];
			points[i][1] = leftPoints[i][1];
			points[i][2] = leftPoints[i][2];
		}
		for (j=1; j<leftNPoints; j++, i++)
		{
			points[i][0] = rightPoints[j][0];
			points[i][1] = rightPoints[j][1];
			points[i][2] = rightPoints[j][2];
		}
		free (leftPoints);
		free (rightPoints);
		par_nPoints = nPoints;
		*par_points = points;
	}
	else
	{
		printf ("true\n");
		par_nPoints = 2;
		vec3 *points = (vec3*)malloc(par_nPoints*sizeof(vec3));
		if (!points)
		{
			par_nPoints = 0;
			par_points = NULL;
			return 0;
		}
		points[0][0] = m_controlPoints[0][0];
		points[0][1] = m_controlPoints[0][1];
		points[0][2] = m_controlPoints[0][2];
		points[1][0] = m_controlPoints[3][0];
		points[1][1] = m_controlPoints[3][1];
		points[1][2] = m_controlPoints[3][2];
		*par_points = points;
	}

	return 1;
}

/**
* compute the arc length of Cubic Bezier Curves (cubic curve, ie 4 control points, ie degree = 3)
*
* %return 1 if the new control point is correctly added, 0 otherwise
*
* ref : http://steve.hollasch.net/cgindex/curves/cbezarclen.html
*/
static double q1, q2, q3, q4, q5;
double balf (double t)                   // Bezier Arc Length Function
{
    double result = q5 + t*(q4 + t*(q3 + t*(q2 + t*q1)));
    result = sqrt(result);
    return result;
}

float
CurveBezier::length4 (void)
{
    vec3 k1, k2, k3, k4;

	for (int i=0; i<3; i++)
	{
		k1[i] = -m_controlPoints[0][i] + 3*(m_controlPoints[1][i] - m_controlPoints[2][i]) + m_controlPoints[3][i];
	    k2[i] = 3*(m_controlPoints[0][i] + m_controlPoints[2][i]) - 6*m_controlPoints[1][i];
		k3[i] = 3*(m_controlPoints[1][i] - m_controlPoints[0][i]);
	    k4[i] = m_controlPoints[0][i];
	}

    q1 = 9.0*(k1[0]*k1[0] + k1[1]*k1[1]);
    q2 = 12.0*(k1[0]*k2[0] + k1[1]*k2[1]);
    q3 = 3.0*(k1[0]*k3[0] + k1[1]*k3[1]) + 4.0*(k2[0]*k2[0] + k2[1]*k2[1]);
    q4 = 4.0*(k2[0]*k3[0] + k2[1]*k3[1]);
    q5 = k3[0]*k3[0] + k3[1]*k3[1];

    float result=0.0;// = Simpson(balf, 0, 1, 1024, 0.001);
    return result;
}

/**
* dump
*/
void
CurveBezier::dump (void)
{
	for (int i=0; i<m_nControlPoints; i++)
		printf ("%d : %f %f %f\n", i, m_controlPoints[i][0], m_controlPoints[i][1], m_controlPoints[i][2]);
}
void
CurveBezier::export_interpolated (char *filename, unsigned int n)
{
	vec3 *bezier_interpolated = (vec3*)malloc(n*sizeof(vec3));
	computeInterpolation (n, &bezier_interpolated);
	
	FILE *ptr = fopen (filename, "w");
	for (unsigned int i=0; i<n; i++)
		fprintf (ptr, "%f %f\n", bezier_interpolated[i][0], bezier_interpolated[i][1]);
	fclose (ptr);

	free (bezier_interpolated);
}
