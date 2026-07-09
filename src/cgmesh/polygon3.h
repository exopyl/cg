#pragma once
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
	inline void GetPoint (unsigned int ci, unsigned int vi, Vector3f &pt)
		{
			pt.Set (m_pPoints[ci][3*vi], m_pPoints[ci][3*vi+1], m_pPoints[ci][3*vi+2]);
		};
	void GetBBox (Vector3f &min, Vector3f &max);

	void Dump (void);

private:
	unsigned int m_nContours;
	unsigned int *m_nPoints;
	float **m_pPoints;
};
