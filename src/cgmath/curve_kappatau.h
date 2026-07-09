#pragma once

#include "curve.h"

//
// Curve defined intrinsically by its curvature kappa(s) and torsion tau(s),
// reconstructed by integrating the Frenet-Serret equations.
// http://www.cs.sjsu.edu/faculty/rucker/kaptaudoc/ktpaper.htm
//
// example :
//   float kappa (float s) { return 8; }
//   float tau   (float s) { return sin(s)+3*sin(2*s); }
//
class CurveKappaTau : public IAnalyticCurve
{
public:
	CurveKappaTau () = default;

	void set_kappa (float (*f)(float)) { m_kappa = f; }
	void set_tau (float (*f)(float)) { m_tau = f; }

	// ICurve : position at t in [0,1], obtained by Frenet-Serret integration
	bool eval (float t, Vector3f &p) const override;

	// IAnalyticCurve : curvature and torsion are known by construction
	float curvature (float t) const override;
	float torsion (float t) const override;

	// tube mesh (integrates the frame directly)
	void Export (const char *filename) const override;

private:
	float (*m_kappa)(float) = nullptr;
	float (*m_tau)(float) = nullptr;
};
