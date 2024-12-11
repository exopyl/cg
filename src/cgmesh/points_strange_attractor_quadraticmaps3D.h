#ifndef __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS3D_H__
#define __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS3D_H__

#include "points_strange_attractor.h"

class StrangeAttractor_QuadraticMaps3D : public StrangeAttractor
{
public:
	StrangeAttractor_QuadraticMaps3D () {};
	void set_parameters (double _a0, double _a1, double _a2, double _a3, double _a4, double _a5,
			     double _a6, double _a7, double _a8, double _a9, double _a10, double _a11,
			     double _a12, double _a13, double _a14, double _a15, double _a16, double _a17);
	void set_parameters (char signature[18]);
	void set_parameters_random (void);
	void set_signature_random  (void);

	bool get_signature (char **signature, int *length);

	void next (void);
	
private:
	char   s[18];
	double a[18];
};

#endif // __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS3D_H__
