#ifndef __CURVE_NURBS_H__
#define __CURVE_NURBS_H__

#include "algebra_vector3.h"

/**
* Curve NURBS
*/
class CurveNURBS
{
public:
	CurveNURBS ();
	CurveNURBS (const CurveNURBS &curveNURBS);
	//CurveNURBS* clone () { return new CurveNURBS (*this); };
	void dump (void);

	// specific methods
	~CurveNURBS ();

	int addControlPoint (float x, float y, float z, float weight);
	int setKnots (float *knots, int size);
	int normalizeKnots (void);

	int nControlPoints (void) { return m_nControlPoints; };
	void getControlPoints (int index, vec3 &v)
	{
		v[0] = m_controlPoints[index][0];
		v[1] = m_controlPoints[index][1];
		v[2] = m_controlPoints[index][2];
	};
	
	float basisFunction (int i, int j, float u);
	int computeInterpolation (int par_nPoints, vec3 **par_points);
	
	void dumpAllBasisFunctions (int nPoints);
	void dumpBasisFunction (int index, int nPoints);

private:
	int m_nMaxControlPoints;

	// control points
	int m_nControlPoints;
	vec3 *m_controlPoints;

	// weights
	float *m_weights;

	// knots
	int m_degree;
	int m_nKnots;
	float *m_knots;
};

#endif // __CURVE_NURBS_H__
