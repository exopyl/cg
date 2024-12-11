#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "points_strange_attractor_quadraticmaps3D.h"

void
StrangeAttractor_QuadraticMaps3D::set_parameters (double _a0, double _a1, double _a2, double _a3, double _a4, double _a5,
													  double _a6, double _a7, double _a8, double _a9, double _a10, double _a11,
													  double _a12, double _a13, double _a14, double _a15, double _a16, double _a17)
{
	a[0] = _a0; a[1] = _a1; a[2] = _a2; a[3] = _a3; a[4] = _a4; a[5] = _a5;
	a[6] = _a6; a[7] = _a7; a[8] = _a8; a[9] = _a9; a[10] = _a10; a[11] = _a11;
	a[12] = _a12; a[13] = _a13; a[14] = _a14; a[15] = _a15; a[16] = _a16; a[17] = _a17;
}

void
StrangeAttractor_QuadraticMaps3D::set_parameters (char signature[18])
{
	for (int i=0; i<18; i++)
	{
		s[i] = signature[i];
		a[i] = convert (signature[i]);
	}
}

void
StrangeAttractor_QuadraticMaps3D::set_parameters_random (void)
{
	for (int i=0; i<18; i++)
	{
		a[i] = random_parameter ();
		printf ("a[%d] = %f\n", i, a[i]);
	}
}

void
StrangeAttractor_QuadraticMaps3D::set_signature_random  (void)
{
	for (int i=0; i<18; i++)
	{
	}
}

bool
StrangeAttractor_QuadraticMaps3D::get_signature (char **signature, int *length)
{
	char *_signature = (char*)malloc(19*sizeof(char));
	if (_signature == NULL)
		return false;

	int i=0;
	for (i=0; i<18; i++)
		_signature[i]=s[i];
	_signature[i]='\0';
	*signature = _signature;
	*length = 18;
	return true;
};

void
StrangeAttractor_QuadraticMaps3D::next ()
{
	double xtmp = a[0] + a[1]*x + a[2]*pow (x,2.0) + a[3]*x*y + a[4]*y + a[5]*pow (y,2.0);
	double ytmp = a[6] + a[7]*x + a[8]*pow (x,2.0) + a[9]*x*y + a[10]*y + a[11]*pow (y,2.0);
	double ztmp = a[12] + a[13]*x + a[14]*pow (x,2.0) + a[15]*x*y + a[16]*y + a[17]*pow (y,2.0);
	x = xtmp;
	y = ytmp;
	z = ztmp;
}
