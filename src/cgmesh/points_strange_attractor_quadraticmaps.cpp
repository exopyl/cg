#include <math.h>

#include "points_strange_attractor_quadraticmaps.h"

void
StrangeAttractor_QuadraticMaps::set_parameters (double _a0, double _a1, double _a2, double _a3, double _a4, double _a5,
												   double _a6, double _a7, double _a8, double _a9, double _a10, double _a11)
{
	a[0] = _a0; a[1] = _a1; a[2] = _a2; a[3] = _a3; a[4] = _a4; a[5] = _a5;
	a[6] = _a6; a[7] = _a7; a[8] = _a8; a[9] = _a9; a[10] = _a10; a[11] = _a11;
}

void
StrangeAttractor_QuadraticMaps::set_parameters (char signature[12])
{
	for (int i=0; i<12; i++) a[i] = convert (signature[i]);
}

void
StrangeAttractor_QuadraticMaps::next ()
{
	double xtmp = a[0] + a[1]*x + a[2]*pow (x,2.0) + a[3]*x*y + a[4]*y + a[5]*pow (y,2.0);
	double ytmp = a[6] + a[7]*x + a[8]*pow (x,2.0) + a[9]*x*y + a[10]*y + a[11]*pow (y,2.0);
	x = xtmp;
	y = ytmp;
}
