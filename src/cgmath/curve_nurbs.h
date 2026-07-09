#pragma once

#include <vector>

#include "icurve.h"

/**
* Non-uniform rational B-spline (NURBS) curve.
*/
class CurveNURBS : public ICurve
{
public:
	CurveNURBS () = default;

	void dump () const;

	int addControlPoint (float x, float y, float z, float weight);
	int setKnots (const float *knots, int size);
	int normalizeKnots (void);

	int nControlPoints (void) const { return (int)m_controlPoints.size (); }
	bool getControlPoints (int index, Vector3f &v) const
	{
		if (index < 0 || index >= (int)m_controlPoints.size ())
			return false;
		v = m_controlPoints[index];
		return true;
	}

	float basisFunction (int i, int j, float u) const;

	// ICurve
	bool eval (float t, Vector3f &p) const override;
	int tessellate (int nPoints, std::vector<Vector3f> &out) const override;

	void dumpAllBasisFunctions (int nPoints) const;
	void dumpBasisFunction (int index, int nPoints) const;

private:
	std::vector<Vector3f> m_controlPoints;
	std::vector<float> m_weights;
	std::vector<float> m_knots;
	int m_degree = 0;
};
