#pragma once

#include <vector>

#include "TVector3.h"

/**
* ICurve : minimal parametric-curve interface.
*
* A curve is a mapping t in [0,1] -> point in R^3. This is the greatest common
* denominator shared by control-point curves (Bezier, NURBS) and analytic
* curves. Differential geometry (Frenet frame, curvature, torsion) is provided
* by the derived interface IAnalyticCurve, not here.
*/
class ICurve
{
public:
	virtual ~ICurve () = default;

	/**
	* Position on the curve at parameter t in [0,1].
	*
	* @return false if the curve cannot be evaluated (e.g. no control points).
	*/
	virtual bool eval (float t, Vector3f &p) const = 0;

	/**
	* Sample the curve at nPoints (>= 2) uniformly-spaced parameters over [0,1].
	* out is cleared then filled.
	*
	* @return the number of points produced (0 on error).
	*/
	virtual int tessellate (int nPoints, std::vector<Vector3f> &out) const = 0;
};
