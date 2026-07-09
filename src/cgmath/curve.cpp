#include <stdio.h>
#include <math.h>

#include "curve.h"

// finite-difference step in parameter space
static const float kFdStep = 1.0e-3f;
// full-turn parameter mapping for the analytic sample curves: t in [0,1] -> [0,2pi]
static const float kTwoPi = 6.28318530718f;

// evaluate position, clamping the parameter so finite-difference neighbours
// stay inside [0,1]
static Vector3f evalClamped (const IAnalyticCurve &c, float t)
{
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;
	Vector3f p;
	c.eval (t, p);
	return p;
}

// v = dP/dt (central difference)
void IAnalyticCurve::velocity (float t, Vector3f &v) const
{
	const float h = kFdStep;
	if (t < 2*h) t = 2*h;
	if (t > 1.f - 2*h) t = 1.f - 2*h;
	Vector3f pp = evalClamped (*this, t + h);
	Vector3f pm = evalClamped (*this, t - h);
	v = (pp - pm) * (1.0f / (2.0f * h));
}

// a = d2P/dt2 (central difference)
void IAnalyticCurve::acceleration (float t, Vector3f &a) const
{
	const float h = kFdStep;
	if (t < 2*h) t = 2*h;
	if (t > 1.f - 2*h) t = 1.f - 2*h;
	Vector3f pp = evalClamped (*this, t + h);
	Vector3f p0 = evalClamped (*this, t);
	Vector3f pm = evalClamped (*this, t - h);
	a = (pp - p0 * 2.0f + pm) * (1.0f / (h * h));
}

// j = d3P/dt3 (central difference), used for torsion
static Vector3f jerk (const IAnalyticCurve &c, float t)
{
	const float h = kFdStep;
	if (t < 2*h) t = 2*h;
	if (t > 1.f - 2*h) t = 1.f - 2*h;
	Vector3f p2 = evalClamped (c, t + 2*h);
	Vector3f p1 = evalClamped (c, t + h);
	Vector3f m1 = evalClamped (c, t - h);
	Vector3f m2 = evalClamped (c, t - 2*h);
	return (p2 - p1 * 2.0f + m1 * 2.0f - m2) * (1.0f / (2.0f * h * h * h));
}

float IAnalyticCurve::speed (float t) const
{
	Vector3f v;
	velocity (t, v);
	return v.getLength ();
}

// T = v/|v| ; B = (v x a)/|v x a| ; N = B x T
void IAnalyticCurve::frenetFrame (float t, Vector3f &T, Vector3f &N, Vector3f &B) const
{
	Vector3f v, a;
	velocity (t, v);
	acceleration (t, a);

	T = v;
	T.Normalize ();

	B = v.CrossProduct (a);
	B.Normalize ();

	N = B.CrossProduct (T);
	N.Normalize ();
}

// kappa = |v x a| / |v|^3
float IAnalyticCurve::curvature (float t) const
{
	Vector3f v, a;
	velocity (t, v);
	acceleration (t, a);
	float speed3 = v.getLength ();
	speed3 = speed3 * speed3 * speed3;
	if (speed3 < 1.0e-9f)
		return 0.f;
	return v.CrossProduct (a).getLength () / speed3;
}

// tau = (v x a) . j / |v x a|^2
float IAnalyticCurve::torsion (float t) const
{
	Vector3f v, a;
	velocity (t, v);
	acceleration (t, a);
	Vector3f vxa = v.CrossProduct (a);
	float d = vxa.getLength2 ();
	if (d < 1.0e-9f)
		return 0.f;
	return (vxa * jerk (*this, t)) / d;
}

int IAnalyticCurve::tessellate (int nPoints, std::vector<Vector3f> &out) const
{
	out.clear ();
	if (nPoints < 2)
		return 0;
	out.reserve (nPoints);
	for (int i = 0; i < nPoints; i++)
	{
		float t = (float)i / (float)(nPoints - 1);
		Vector3f p;
		if (!eval (t, p))
			return 0;
		out.push_back (p);
	}
	return (int)out.size ();
}

// tube mesh along the curve
void IAnalyticCurve::Export (const char *filename) const
{
	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return;

	const unsigned int npointscircle = 80;
	const unsigned int nsteps = 200;
	const float radius = 0.2f;

	unsigned int nvertices = 0;
	for (unsigned int step = 0; step < nsteps; step++)
	{
		float t = (float)step / (float)(nsteps - 1);
		Vector3f p, T, N, B;
		eval (t, p);
		frenetFrame (t, T, N, B);
		nvertices++;

		for (unsigned int k = 0; k < npointscircle; k++)
		{
			float angle = kTwoPi * k / npointscircle;
			Vector3f d = N * (radius * cosf (angle)) + B * (radius * sinf (angle));
			fprintf (ptr, "v %f %f %f\n", p.x + d.x, p.y + d.y, p.z + d.z);
		}
	}

	unsigned int offset = 0;
	for (unsigned int j = 0; j < nvertices - 1; j++)
	{
		unsigned int k = 0;
		for (k = 0; k < npointscircle - 1; k++)
			fprintf (ptr, "f %d %d %d %d\n",
				 1 + offset + npointscircle*j + k,
				 1 + offset + npointscircle*j + 1 + k,
				 1 + offset + npointscircle*(j+1) + 1 + k,
				 1 + offset + npointscircle*(j+1) + k);

		fprintf (ptr, "f %d %d %d %d\n",
			 1 + offset + npointscircle*j + k,
			 1 + offset + npointscircle*j,
			 1 + offset + npointscircle*(j+1),
			 1 + offset + npointscircle*(j+1) + k);
	}

	fclose (ptr);
}

//
// Curve01
//
bool Curve01::eval (float t, Vector3f &p) const
{
	float a = kTwoPi * t;
	p.Set (m_r * cosf (a), m_r * sinf (a), m_a * cosf (m_m * a));
	return true;
}

//
// CurveHelical
//
bool CurveHelical::eval (float t, Vector3f &p) const
{
	float a = kTwoPi * t;
	p.Set (cosf (m * a), sinf (m * a), n * a);
	return true;
}

//
// CurveWindingLineOnTorus
//
bool CurveWindingLineOnTorus::eval (float t, Vector3f &r) const
{
	float a = kTwoPi * t;
	r.Set ((p + q * cosf (n * a)) * cosf (m * a),
	       (p + q * cosf (n * a)) * sinf (m * a),
	       q * sinf (n * a));
	return true;
}
