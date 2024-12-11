#ifndef __POLYGON3_H__
#define __POLYGON3_H__

#include "../cgmath/cgmath.h"

class Polygon3
{
public:
	Polygon3 ();
	~Polygon3 ();

	int add_contour (unsigned int index, unsigned int nPoints, float *pPoints);

	unsigned int GetNContours (void) { return m_nContours; };
	unsigned int GetNPoints (unsigned int i) { return m_nPoints[i]; };
	float* GetPoints (unsigned int i) { return m_pPoints[i]; };
	inline void GetPoint (unsigned int ci, unsigned int vi, vec3 pt)
		{
			vec3_init (pt, m_pPoints[ci][3*vi], m_pPoints[ci][3*vi+1], m_pPoints[ci][3*vi+2]);
		};
	void GetBBox (vec3 min, vec3 max);

	void Dump (void);

private:
	unsigned int m_nContours;
	unsigned int *m_nPoints;
	float **m_pPoints;
};

#endif // __POLYGON3_H__
