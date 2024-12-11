#include <math.h>

#include "points_strange_attractor_cubicmaps.h"

void
StrangeAttractor_CubicMaps::set_parameters (double _a1, double _a2, double _a3, double _a4, double _a5,
					    double _a6, double _a7, double _a8, double _a9, double _a10,
					    double _a11, double _a12, double _a13, double _a14, double _a15,
					    double _a16, double _a17, double _a18, double _a19, double _a20)
{
	a1 = _a1;	a2  = _a2;	a3  = _a3;	a4  = _a4;	a5 = _a5;
	a6  = _a6;	a7  = _a7;	a8  = _a8;	a9 = _a9;	a10 = _a10;
	a11 = _a11;	a12 = _a12;	a13 = _a13;	a14 = _a14;	a15 = _a15;
	a16 = _a16;	a17 = _a17;	a18 = _a18;	a19 = _a19;	a20 = _a20;
}

void
StrangeAttractor_CubicMaps::next (void)
{
	double xtmp = a1 + a2*x + a3*pow (x,2.0) + a4*pow (x,3.0) + a5*pow(x,2.0)*y + a6*x*y + a7*x*pow(x,2.0) + a8*y + a9*pow(y,2.0) + a10*pow(y,3.0);
	double ytmp = a11 + a12*x + a13*pow (x,2.0) + a14*pow (x,3.0) + a15*pow(x,2.0)*y + a16*x*y + a17*x*pow(x,2.0) + a18*y + a19*pow(y,2.0) + a20*pow(y,3.0);
	x = xtmp;
	y = ytmp;
}
