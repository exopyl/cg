#include <math.h>

#include "common.h"
#include "polynomial.h"

float polynomial2_eval (float a, float b, float c, float x)
{
	return a*x*x + b*x + c;
}

int polynomial2_find_roots (float a, float b, float c, float roots[2])
{
	float d = b*b - 4*a*c;
	if (d > 0.)
	{
		roots[0] = (-b-sqrt(d))/(2.*a);
		roots[1] = (-b+sqrt(d))/(2.*a);
		return 2;
	}
	else if (d == 0.)
	{
		roots[0] = -0.5*b/a;
		return 1;
	}
	else
	{
		return 0;
	}
	return -1;
}


float polynomial3_eval (float a, float b, float c, float d, float x)
{
	float x2 = x*x;
	return a*x*x2 + b*x2 + c*x + d;
}

// http://en.wikipedia.org/wiki/Cubic_function
// http://www.josechu.com/ecuaciones_polinomicas/cubica_solucion.htm
// http://www.hawaii.edu/suremath/jrootsCubic.html
// http://www.easycalculation.com/algebra/cubic-equation.php
int polynomial3_find_roots (float a, float b, float c, float d, float roots[3])
{
	if (a == 0.)
		return polynomial2_find_roots (b, c, d, roots);

	b /= a;
	c /= a;
	d /= a;
	a = 1.;

	float q = (3.*c - b*b)/9.;
	float r = b*(9.*c - 2.*b*b) - 27.*d;
	r /= 54.;
	float disc = q*q*q + r*r;
	float term1 = b/3.;

	if (disc > 0.) // one root real, two are complex
	{
		float s = r + sqrt (disc);
		s = (s < 0.)? -pow ((float)(-s), (float)(1./3.)) : pow((float)(s), (float)(1./3.));
		float t = r - sqrt (disc);
		t = (t < 0.)? -pow((float)(-t), (float)(1./3.)) : pow((float)(t), (float)(1./3.));
		roots[0] = -term1 + s + t;
		/*
		  term1 += (s+t)/2.
		  roots[1].real = -term1
		  roots[2].real = -term1
		  term1 = sqrt(3.)*(s-t)/2.
		  roots[1].imag = term1
		  roots[2].imag = -term1
		  
		 */
		return 1;
	}
	else if (disc == 0.) // all roots real, at least two are equal.
	{
		float r13 = (r < 0.)? -pow((float)(-r), (float)(1./3.)) : pow((float)(r), (float)(1./3.));
		roots[0] = -term1 + 2.*r13;
		roots[1] = -(r13 + term1);
		roots[2] = -(r13 + term1);
		return 2;
	}
	else // all roots are real and unequal (to get here, q < 0)
	{
		q = -q;
		float dum1 = q*q*q;
		dum1 = acos(r/sqrt(dum1));
		float r13 = 2.*sqrt(q);
		roots[0] = -term1 + r13 * cos(dum1/3.);
		roots[1] = -term1 + r13 * cos((dum1 + 2.*M_PI)/3.);
		roots[2] = -term1 + r13 * cos((dum1 + 4.*M_PI)/3.);
		return 3;
	}
	return -1;
}
