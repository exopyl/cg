#ifndef __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS_H__
#define __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS_H__

#include "points_strange_attractor.h"

/**
*
* Examples:
*
* AGHNFODVNJCP (-1.2, -0.6, -0.5,  0.1, -0.7,  0.2, -0.9,  0.9,  0.1, -0.3, -1.0,  0.3)
* BCQAFMFVPXKQ (-1.1, -1.0,  0.4, -1.2, -0.7,  0.0, -0.7,  0.9,  0.3,  1.1, -0.2,  0.4)
* DSYUECINGQNV (-0.9,  0.6,  1.2,  0.8, -0.8, -1.0, -0.4,  0.1, -0.6,  0.4,  0.1,  0.9) 
* ELXAPXMPQOBT (-0.8, -0.1,  1.1, -1.2,  0.3,  1.1,  0.0,  0.3,  0.4,  0.2, -1.1,  0.7) 
* EYYMKTUMXUVC (-0.8,  1.2,  1.2,  0.0, -0.2,  0.7,  0.8,  0.0,  1.1,  0.8,  0.9, -1.0) 
* JTTSMBOGLLQF (-0.3,  0.7,  0.7,  0.6,  0.0, -1.1,  0.2, -0.6, -0.1, -0.1,  0.4, -0.7) 
* OUGFJKDHSAJU ( 0.2,  0.8, -0.6, -0.7, -0.3, -0.2, -0.9, -0.5,  0.6, -1.2, -0.3,  0.8) 
* QKOCSIDVTPGY
*
*/

class StrangeAttractor_QuadraticMaps : public StrangeAttractor
{
public:
	StrangeAttractor_QuadraticMaps () {};
	void set_parameters (double _a1, double _a2, double _a3, double _a4, double _a5, double _a6,
						 double _a7, double _a8, double _a9, double _a10, double _a11, double _a12);
	void set_parameters (char signature[12]);

	void next (void);
	
private:
	double a[12];
};

#endif // __POINTS_STRANGE_ATTRACTOR_QUADRATICMAPS_H__
