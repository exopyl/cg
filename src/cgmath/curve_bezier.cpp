#include <stdio.h>
#include <math.h>

#include "curve_bezier.h"
#include "berstein.h"

int CurveBezier::addControlPoint (const Vector3f &v)
{
	m_controlPoints.push_back (v);
	return 1;
}

int CurveBezier::addControlPoint (float x, float y, float z)
{
	m_controlPoints.emplace_back (x, y, z);
	return 1;
}

bool CurveBezier::eval (float t, Vector3f &p) const
{
	const int n = (int)m_controlPoints.size ();
	if (n == 0)
		return false;

	p.Set (0.f, 0.f, 0.f);
	for (int i = 0; i < n; i++)
		p += m_controlPoints[i] * (float)bersteinPolynomial (n - 1, i, t);
	return true;
}

int CurveBezier::tessellate (int nPoints, std::vector<Vector3f> &out) const
{
	out.clear ();
	if (nPoints < 2 || m_controlPoints.empty ())
		return 0;
	out.reserve (nPoints);
	for (int i = 0; i < nPoints; i++)
	{
		float t = (float)i / (float)(nPoints - 1);
		Vector3f p;
		eval (t, p);
		out.push_back (p);
	}
	return (int)out.size ();
}

bool CurveBezier::eval_on_x (float x, Vector3f &p) const
{
	float t1 = 0.f, t2 = 1.f;
	Vector3f pt1, pt2;
	if (!eval (t1, pt1) || !eval (t2, pt2))
		return false;
	if (pt1.x > x && pt2.x < x)
		return false;

	do {
		float t = (t1 + t2) / 2.f;
		eval (t, p);
		if (p.x < x)
		{
			t1 = t;
			pt1 = p;
		}
		else
		{
			t2 = t;
			pt2 = p;
		}
	} while (fabs (t1 - t2) > 0.000001f);

	return true;
}

void CurveBezier::dump () const
{
	for (size_t i = 0; i < m_controlPoints.size (); i++)
		printf ("%zu : %f %f %f\n", i,
			m_controlPoints[i].x, m_controlPoints[i].y, m_controlPoints[i].z);
}

void CurveBezier::export_interpolated (const char *filename, unsigned int n) const
{
	std::vector<Vector3f> points;
	tessellate ((int)n, points);

	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return;
	for (const Vector3f &pt : points)
		fprintf (ptr, "%f %f\n", pt.x, pt.y);
	fclose (ptr);
}
