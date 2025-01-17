#ifndef __STRANGE_ATTRACTOR_H__
#define __STRANGE_ATTRACTOR_H__

#include <stdio.h>

class StrangeAttractor
{
public:
	StrangeAttractor () { x = y = z = 0.0; };
	~StrangeAttractor () {};

	virtual void set_origin (double xorig, double yorig, double zorig = 0.0);
	virtual double getX (void) { return x; };
	virtual double getY (void) { return y; };
	virtual double getZ (void) { return z; };
	bool get_signature (char **signature, int *length) { *signature = 0; *length = 0; return true; };

	virtual void next (void) {};
	
	void export_obj (char *filename, unsigned int npts);
	void export_asc (char *filename, unsigned int npts);
protected:
	double random_parameter (void);
	char   random_signature (void);

	double convert (char c);

	double x, y, z;
};

#endif // __STRANGE_ATTRACTOR_H__
