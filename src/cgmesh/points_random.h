#ifndef __POINTS_RANDOM_H__
#define __POINTS_RANDOM_H__

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "pointset.h"

// todo : add poisson disk sampling of a triangle mesh http://rodolphe-vaillant.fr/?e=37

/**
* PointSetGenerator
*/
class PointSetGenerator : public PointSet
{
public:
	virtual void generate (int nPoints) = 0;
	virtual void reset (void) = 0;
};


/**
*
*/
class PointSetRandom : public PointSetGenerator
{
public:
	PointSetRandom () { m_nPoints = 0; m_points = NULL; };
	~PointSetRandom () { if (m_points) free (m_points); };
	void generate (int nPoints)
	{
		int i = m_nPoints;
		m_nPoints += nPoints;
		m_points = (float*)realloc((void*)m_points, 3*m_nPoints*sizeof(float));
		if (!m_points) return;
		for (; i<m_nPoints; i++)
		{
			// in a sphere
			/*
			m_points[3*i]   = 2.0*(float)rand()/RAND_MAX - 1.0;
			m_points[3*i+1] = 2.0*(float)rand()/RAND_MAX - 1.0;
			m_points[3*i+2] = 2.0*(float)rand()/RAND_MAX - 1.0;
			if (sqrt (m_points[3*i]*m_points[3*i] + m_points[3*i+1]*m_points[3*i+1] + m_points[3*i+2]*m_points[3*i+2]) > 1.0) i--;
			*/

			// in a cube
			m_points[3*i]   = 2.0*(float)rand()/RAND_MAX - 1.0;
			m_points[3*i+1] = 2.0*(float)rand()/RAND_MAX - 1.0;
			m_points[3*i+2] = 2.0*(float)rand()/RAND_MAX - 1.0;
		}
	};
	void reset (void) {m_nPoints = 0; if (m_points) free (m_points); m_points = NULL; };
private:
};

#endif // __POINTS_RANDOM_H__
