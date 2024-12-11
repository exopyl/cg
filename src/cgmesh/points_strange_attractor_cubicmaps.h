#ifndef __POINTS_STRANGE_ATTRACTOR_CUBICMAPS_H__
#define __POINTS_STRANGE_ATTRACTOR_CUBICMAPS_H__

#include "points_strange_attractor.h"

class StrangeAttractor_CubicMaps : public StrangeAttractor
{
public:
	StrangeAttractor_CubicMaps () {};
	void set_parameters (double _a1, double _a2, double _a3, double _a4, double _a5,
			     double _a6, double _a7, double _a8, double _a9, double _a10,
			     double _a11, double _a12, double _a13, double _a14, double _a15,
			     double _a16, double _a17, double _a18, double _a19, double _a20);
	void next (void);
	
private:
	double a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20;
};

#endif // __POINTS_STRANGE_ATTRACTOR_CUBICMAPS_H__
