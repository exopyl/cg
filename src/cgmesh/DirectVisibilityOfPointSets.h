#ifndef __DIRECT_VISIBILITY_OF_POINT_SETS_H__
#define __DIRECT_VISIBILITY_OF_POINT_SETS_H__

//
// Reference :
// Direct Visibility of Point Sets, 
// Sagi Katz, Ayellet Tal, and Ronen Basri. 
// SIGGRAPH 2007, ACM Transactions on Graphics, Volume 26, Issue 3, August 2007, 24/1-24/11 
// http://webee.technion.ac.il/~ayellet/papers.html
//


class DirectVisibilityOfPointSets
{
public:
	DirectVisibilityOfPointSets ();

	void SetCamera (float x, float y, float z)
	{
		m_camera[0] = x;
		m_camera[1] = y;
		m_camera[2] = z;
	};
	void SetPoints (int nPoints, float *points);

	float* ComputeVisiblePoints (void);

public:

	int   m_nPoints;
	float *m_pointsFlipped;
	float *m_points;
	float m_camera[3];

	int   m_nPointsVisible;
	float *m_pointsVisible;
};

#endif // __DIRECT_VISIBILITY_OF_POINT_SETS_H__
