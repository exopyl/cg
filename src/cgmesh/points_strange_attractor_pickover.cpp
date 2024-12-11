#include <math.h>
#include <stdio.h>

#include "points_strange_attractor_pickover.h"

void StrangeAttractor_Pickover::set_parameters (double _a, double _b, double _c, double _d)
{
	a = _a;	b = _b;	c = _c;	d = _d;
}

void StrangeAttractor_Pickover::next (void)
{
	double xtmp = sin (b*y) + c*sin (b*x);
	double ytmp = sin (a*x) + d*sin (a*y);
	x = xtmp;
	y = ytmp;
}

