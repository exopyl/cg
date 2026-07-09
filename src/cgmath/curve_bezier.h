#pragma once

#include <vector>

#include "icurve.h"

/**
* Bezier curve defined by its control points.
*/
class CurveBezier : public ICurve
{
public:
	CurveBezier () = default;

	void dump () const;
	void export_interpolated (const char *filename, unsigned int n) const;

	int getDegree () const { return (int)m_controlPoints.size () - 1; }
	bool getControlPoint (int index, Vector3f &v) const
	{
		if (index < 0 || index >= (int)m_controlPoints.size ())
			return false;
		v = m_controlPoints[index];
		return true;
	}

	// Construction of the Bezier curve
	int addControlPoint (const Vector3f &v);
	int addControlPoint (float x, float y, float z);

	// ICurve
	bool eval (float t, Vector3f &p) const override;
	int tessellate (int nPoints, std::vector<Vector3f> &out) const override;

	// find the point on the curve whose x-coordinate is x (x assumed monotone)
	bool eval_on_x (float x, Vector3f &p) const;

private:
	std::vector<Vector3f> m_controlPoints;
};
