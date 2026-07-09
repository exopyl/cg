#pragma once

#include "icurve.h"

/**
* IAnalyticCurve : a curve that also exposes differential geometry.
*
* On top of ICurve (eval + tessellate) it provides the Frenet frame, curvature
* and torsion, plus a tube-mesh Export. The differential quantities have
* numerical default implementations built on eval() (central finite
* differences), so a concrete analytic curve only needs to provide eval().
* A curve that knows some quantities in closed form (e.g. CurveKappaTau knows
* curvature and torsion by construction) may override them.
*/
class IAnalyticCurve : public ICurve
{
public:
	// first and second derivatives w.r.t. t (numerical, central differences)
	void velocity (float t, Vector3f &v) const;
	void acceleration (float t, Vector3f &a) const;
	float speed (float t) const;

	// Frenet frame: unit tangent T, principal normal N, binormal B
	void frenetFrame (float t, Vector3f &T, Vector3f &N, Vector3f &B) const;

	// intrinsic quantities (numerical defaults; override when known analytically)
	virtual float curvature (float t) const;
	virtual float torsion (float t) const;

	// ICurve: default tessellation samples eval() uniformly over [0,1]
	int tessellate (int nPoints, std::vector<Vector3f> &out) const override;

	// export a tube mesh along the curve to a Wavefront .obj file
	virtual void Export (const char *filename) const;
};

//
// Curve01 : sample analytic curve r(t) = (r cos t, r sin t, a cos(m t))
//
class Curve01 : public IAnalyticCurve
{
public:
	Curve01 () { m_m = 12; m_a = 1; m_r = 6.; }

	bool eval (float t, Vector3f &p) const override;

private:
	float m_r; // radius
	float m_m; // angular rate
	float m_a; // amplitude
};

//
// CurveHelical
//
class CurveHelical : public IAnalyticCurve
{
public:
	CurveHelical () { m = 3.; n = 2.; }

	bool eval (float t, Vector3f &p) const override;

private:
	float m; // angular rate
	float n; // height
};

//
// CurveWindingLineOnTorus
// ref : http://www.maplesoft.com/applications/view.aspx?SID=4019&view=html
//
class CurveWindingLineOnTorus : public IAnalyticCurve
{
public:
	CurveWindingLineOnTorus () { p = 4; q = 1; m = 2; n = 10; }

	bool eval (float t, Vector3f &r) const override;

private:
	float p; // big radius
	float q; // small radius
	float m; // angular rate
	float n; // angular rate
};
