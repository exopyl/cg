#ifndef __POINTS_STRANGE_ATTRACTOR_PICKOVER_H__
#define __POINTS_STRANGE_ATTRACTOR_PICKOVER_H__

#include "points_strange_attractor.h"

class StrangeAttractor_Pickover : public StrangeAttractor
{
public:
	StrangeAttractor_Pickover () {};
	void set_parameters (double _a, double _b, double _c, double _d);
	void next (void);

private:
	double a, b, c, d;
};

#endif // __POINTS_STRANGE_ATTRACTOR_PICKOVER_H__
