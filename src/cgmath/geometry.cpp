#include <math.h>

#include "geometry.h"
#include "common.h"
#include "polynomial.h"

// Closest point on a triangle to p (Ericson, "Real-Time Collision Detection",
// section 5.1.5). Returns the squared distance; writes the closest point to
// closest_out when provided. Implemented with plain scalar math so it accepts
// const inputs and has no side effects.
float point_triangle_distance2 (const float *p, const float *a, const float *b, const float *c,
                                float *closest_out)
{
	const float ab[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] };
	const float ac[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] };
	const float ap[3] = { p[0]-a[0], p[1]-a[1], p[2]-a[2] };

	auto dot = [](const float u[3], const float v[3]) {
		return u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
	};

	float cl[3];

	const float d1 = dot(ab, ap), d2 = dot(ac, ap);
	if (d1 <= 0.f && d2 <= 0.f)                       // vertex region A
	{
		cl[0]=a[0]; cl[1]=a[1]; cl[2]=a[2];
	}
	else
	{
		const float bp[3] = { p[0]-b[0], p[1]-b[1], p[2]-b[2] };
		const float d3 = dot(ab, bp), d4 = dot(ac, bp);
		const float cp[3] = { p[0]-c[0], p[1]-c[1], p[2]-c[2] };
		const float d5 = dot(ab, cp), d6 = dot(ac, cp);
		const float vc = d1*d4 - d3*d2;
		const float vb = d5*d2 - d1*d6;
		const float va = d3*d6 - d5*d4;

		if (d3 >= 0.f && d4 <= d3)                    // vertex region B
		{
			cl[0]=b[0]; cl[1]=b[1]; cl[2]=b[2];
		}
		else if (d6 >= 0.f && d5 <= d6)               // vertex region C
		{
			cl[0]=c[0]; cl[1]=c[1]; cl[2]=c[2];
		}
		else if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) // edge AB
		{
			const float v = d1 / (d1 - d3);
			for (int i=0;i<3;i++) cl[i] = a[i] + v*ab[i];
		}
		else if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) // edge AC
		{
			const float w = d2 / (d2 - d6);
			for (int i=0;i<3;i++) cl[i] = a[i] + w*ac[i];
		}
		else if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) // edge BC
		{
			const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			for (int i=0;i<3;i++) cl[i] = b[i] + w*(c[i] - b[i]);
		}
		else                                          // interior face region
		{
			const float denom = 1.f / (va + vb + vc);
			const float v = vb * denom, w = vc * denom;
			for (int i=0;i<3;i++) cl[i] = a[i] + ab[i]*v + ac[i]*w;
		}
	}

	if (closest_out)
	{
		closest_out[0]=cl[0]; closest_out[1]=cl[1]; closest_out[2]=cl[2];
	}
	const float dx=p[0]-cl[0], dy=p[1]-cl[1], dz=p[2]-cl[2];
	return dx*dx + dy*dy + dz*dz;
}

Geometry::Geometry()
{
	m_pAABox = nullptr;
}

//
//
bool Geometry::GetIntersectionBboxWithRay (const Vector3f &o, const Vector3f &d)
{
	if (m_pAABox)
	{
		Ray r (Vector3 (o[0], o[1], o[2]), Vector3 (d[0], d[1], d[2]));
		return m_pAABox->intersection (r, 0., 100.);
	}
	else
		return true;
};


//
// Circle (2D)
//

Circle::Circle ()
	: center(0.0, 0.0), radius(0.0)
{
}

Circle::Circle (Vector2d _center, double _radius)
	: center(_center), radius(_radius)
{
}

Vector2d Circle::pointAt (double theta) const
{
	return Vector2d (center.x + radius * cos(theta),
	                 center.y + radius * sin(theta));
}

double Circle::angleAt (Vector2d p) const
{
	return atan2(p.y - center.y, p.x - center.x);
}

bool Circle::contains (Vector2d p, double eps) const
{
	double dx = p.x - center.x;
	double dy = p.y - center.y;
	double d  = sqrt(dx * dx + dy * dy);
	return fabs(d - radius) < eps;
}

Circle::IntersectionResult Circle::intersection (const Circle &other) const
{
	IntersectionResult res;
	res.count = 0;

	Vector2d delta = other.center - center;
	double d2 = delta.x * delta.x + delta.y * delta.y;
	double d  = sqrt(d2);

	// Concentric (or coincident) : no well-defined intersection.
	if (d < 1e-12)
		return res;

	double rsum = radius + other.radius;
	double rdif = radius - other.radius;

	// Disjoint or one strictly inside the other.
	if (d > rsum || d < fabs(rdif))
		return res;

	double a  = (radius * radius - other.radius * other.radius + d2) / (2.0 * d);
	double h2 = radius * radius - a * a;

	// Tangent case : a single intersection point.
	if (h2 <= 0.0)
	{
		res.count = 1;
		res.pts[0] = Vector2d (center.x + a * delta.x / d,
		                       center.y + a * delta.y / d);
		return res;
	}

	double h = sqrt(h2);

	double px = center.x + a * delta.x / d;
	double py = center.y + a * delta.y / d;

	// perp(delta / d) = (-delta.y / d, delta.x / d)
	double nx = -delta.y / d;
	double ny =  delta.x / d;

	Vector2d p1 (px + h * nx, py + h * ny);
	Vector2d p2 (px - h * nx, py - h * ny);

	// pts[0] = upper (max y), pts[1] = lower.
	if (p1.y >= p2.y)
	{
		res.pts[0] = p1;
		res.pts[1] = p2;
	}
	else
	{
		res.pts[0] = p2;
		res.pts[1] = p1;
	}
	res.count = 2;
	return res;
}

Circle::IntersectionResult Circle::segmentIntersection (const Vector2d& a, const Vector2d& b) const
{
	IntersectionResult res;
	res.count = 0;

	// P(t) = a + t * d, with d = b - a, t in [0, 1].
	// |P(t) - center|^2 = r^2  becomes  A t^2 + 2 B t + C = 0  with
	//   A = d.d, B = w.d, C = w.w - r^2  where w = a - center.
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	double wx = a.x - center.x;
	double wy = a.y - center.y;

	double A = dx * dx + dy * dy;
	if (A < 1e-24)
	{
		// Degenerate segment (a == b). It lies on the circle iff |w| == r.
		if (fabs(sqrt(wx * wx + wy * wy) - radius) < 1e-12)
		{
			res.count = 1;
			res.pts[0] = a;
		}
		return res;
	}

	double B = wx * dx + wy * dy;
	double C = wx * wx + wy * wy - radius * radius;

	double disc = B * B - A * C;
	if (disc < 0.0)
		return res;

	if (disc < 1e-24)
	{
		// Tangent line. Single root.
		double t = -B / A;
		if (t >= -1e-12 && t <= 1.0 + 1e-12)
		{
			res.count = 1;
			res.pts[0] = Vector2d (a.x + t * dx, a.y + t * dy);
		}
		return res;
	}

	double sq = sqrt(disc);
	double t1 = (-B - sq) / A;   // smaller root
	double t2 = (-B + sq) / A;   // larger  root

	bool t1Valid = (t1 >= -1e-12 && t1 <= 1.0 + 1e-12);
	bool t2Valid = (t2 >= -1e-12 && t2 <= 1.0 + 1e-12);

	if (t1Valid && t2Valid)
	{
		res.count = 2;
		res.pts[0] = Vector2d (a.x + t1 * dx, a.y + t1 * dy);
		res.pts[1] = Vector2d (a.x + t2 * dx, a.y + t2 * dy);
	}
	else if (t1Valid)
	{
		res.count = 1;
		res.pts[0] = Vector2d (a.x + t1 * dx, a.y + t1 * dy);
	}
	else if (t2Valid)
	{
		res.count = 1;
		res.pts[0] = Vector2d (a.x + t2 * dx, a.y + t2 * dy);
	}
	return res;
}


//
// Ellipse (2D, two-foci form)
//

Ellipse::Ellipse ()
	: f1(0.0, 0.0), f2(0.0, 0.0), sumDist(0.0)
{
}

Ellipse::Ellipse (Vector2d _f1, Vector2d _f2, double _sumDist)
	: f1(_f1), f2(_f2), sumDist(_sumDist)
{
}

Vector2d Ellipse::center () const
{
	return Vector2d ((f1.x + f2.x) / 2.0, (f1.y + f2.y) / 2.0);
}

double Ellipse::semiMajor () const
{
	return sumDist / 2.0;
}

double Ellipse::semiMinor () const
{
	double dx = f2.x - f1.x;
	double dy = f2.y - f1.y;
	double cf = sqrt(dx * dx + dy * dy) / 2.0;   // half inter-focal distance
	double a  = sumDist / 2.0;
	if (a < cf)
		return 0.0;
	return sqrt(a * a - cf * cf);
}

double Ellipse::angle () const
{
	return atan2(f2.y - f1.y, f2.x - f1.x);
}

bool Ellipse::valid () const
{
	double dx = f2.x - f1.x;
	double dy = f2.y - f1.y;
	double cf = sqrt(dx * dx + dy * dy) / 2.0;
	return sumDist / 2.0 >= cf;
}

Vector2d Ellipse::pointAt (double t) const
{
	Vector2d ec = center();
	double a   = semiMajor();
	double b   = semiMinor();
	double ang = angle();
	double cosA = cos(ang);
	double sinA = sin(ang);
	double ct = cos(t);
	double st = sin(t);
	return Vector2d (ec.x + a * ct * cosA - b * st * sinA,
	                 ec.y + a * ct * sinA + b * st * cosA);
}

bool Ellipse::contains (Vector2d p, double eps) const
{
	double d1x = p.x - f1.x;
	double d1y = p.y - f1.y;
	double d2x = p.x - f2.x;
	double d2y = p.y - f2.y;
	double d1 = sqrt(d1x * d1x + d1y * d1y);
	double d2 = sqrt(d2x * d2x + d2y * d2y);
	return fabs(d1 + d2 - sumDist) < eps;
}

Ellipse::IntersectionResult Ellipse::verticalIntersection (double vx) const
{
	IntersectionResult res;
	res.count = 0;

	if (!valid())
		return res;

	Vector2d ec = center();
	double a   = semiMajor();
	double b   = semiMinor();
	double ang = angle();
	double cosA = cos(ang);
	double sinA = sin(ang);

	// P(t).x = ec.x + a*cos(t)*cosA - b*sin(t)*sinA = vx
	// Rewrite as R*cos(t + phi) = rhs.
	double R   = sqrt(a * a * cosA * cosA + b * b * sinA * sinA);
	double rhs = vx - ec.x;

	if (R < 1e-12)
		return res;

	double rOverR = rhs / R;
	if (rOverR > 1.0 + 1e-12 || rOverR < -1.0 - 1e-12)
		return res;

	// Clamp for safety against rounding noise.
	if (rOverR >  1.0) rOverR =  1.0;
	if (rOverR < -1.0) rOverR = -1.0;

	double phi   = atan2(b * sinA, a * cosA);
	double base  = -phi;
	double delta = acos(rOverR);

	if (delta < 1e-12)
	{
		// Tangent line.
		Vector2d p = pointAt(base);
		res.count = 1;
		res.pts[0] = p;
		return res;
	}

	Vector2d p1 = pointAt(base + delta);
	Vector2d p2 = pointAt(base - delta);

	if (p1.y >= p2.y)
	{
		res.pts[0] = p1;
		res.pts[1] = p2;
	}
	else
	{
		res.pts[0] = p2;
		res.pts[1] = p1;
	}
	res.count = 2;
	return res;
}


//
// Arc (2D oriented arc of a circle)
//

Arc::Arc ()
	: circle(), angleStart(0.0), angleEnd(0.0), ccw(true)
{
}

Arc::Arc (Circle _circle, double _angleStart, double _angleEnd, bool _ccw)
	: circle(_circle), angleStart(_angleStart), angleEnd(_angleEnd), ccw(_ccw)
{
}

Arc::Arc (Circle _circle, Vector2d from, Vector2d to, bool _ccw)
	: circle(_circle),
	  angleStart(_circle.angleAt(from)),
	  angleEnd  (_circle.angleAt(to)),
	  ccw(_ccw)
{
}

double Arc::spanAngle () const
{
	const double TWO_PI = 2.0 * M_PI;
	double delta = angleEnd - angleStart;
	// We do *not* normalize modulo 2*PI : the caller may legitimately pass
	// angleEnd = angleStart +/- 2*PI to express a full circle. We only flip
	// across one full turn when the raw delta has the wrong sign for the
	// requested direction.
	if (ccw && delta < 0.0)
		delta += TWO_PI;
	else if (!ccw && delta > 0.0)
		delta -= TWO_PI;
	return delta;
}

double Arc::length () const
{
	return fabs(spanAngle()) * circle.radius;
}

Vector2d Arc::pointAt (double t) const
{
	double theta = angleStart + t * spanAngle();
	return circle.pointAt(theta);
}

Vector2d Arc::tangentAt (double t) const
{
	double theta = angleStart + t * spanAngle();
	double s = ccw ? 1.0 : -1.0;
	return Vector2d (-s * sin(theta), s * cos(theta));
}

Vector2d Arc::normalAt (double t) const
{
	// Inward normal : (center - point) / radius = -(cos theta, sin theta).
	double theta = angleStart + t * spanAngle();
	return Vector2d (-cos(theta), -sin(theta));
}

std::vector<Vector2d> Arc::tessellate (int n) const
{
	if (n < 1) n = 1;
	std::vector<Vector2d> pts;
	pts.reserve(n + 1);
	double span = spanAngle();
	for (int i = 0; i <= n; ++i)
	{
		double t = (double)i / (double)n;
		double theta = angleStart + t * span;
		pts.push_back(circle.pointAt(theta));
	}
	return pts;
}

std::vector<Vector2d> Arc::tessellateAdaptive (double maxAngleRad) const
{
	if (maxAngleRad <= 0.0)
		return tessellate(1);
	double span = fabs(spanAngle());
	int n = (int)ceil(span / maxAngleRad);
	if (n < 1) n = 1;
	return tessellate(n);
}


//
// Sphere
//

// Refernce : http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
int Sphere::GetIntersectionWithRay (const Vector3f &o, const Vector3f &d, float *_t, Vector3f &i, Vector3f &n)
{
	Vector3f vCO;
	vCO[0] = o[0] - m_vCenter[0];
	vCO[1] = o[1] - m_vCenter[1];
	vCO[2] = o[2] - m_vCenter[2];

	//Compute A, B and C coefficients
	float a =      (d).DotProduct (d);
	float b = 2. * (d).DotProduct (vCO);
	float c =      (vCO).DotProduct (vCO) - (m_fRadius * m_fRadius);
	
	//Find discriminant
	float disc = b*b - 4*a*c;
    
	// if discriminant is negative there are no real roots, so return 
	// false as ray misses sphere
	if (disc < 0)
		return 0;

	// compute q as described above
	float distSqrt = sqrtf(disc);
	float q;
	if (b < 0)
		q = (-b - distSqrt)/2.0;
	else
		q = (-b + distSqrt)/2.0;

	// compute t0 and t1
	float t0 = q / a;
	float t1 = c / q;

	// make sure t0 is smaller than t1
	if (t0 > t1)
	{
		// if t0 is bigger than t1 swap them around
		float temp = t0;
		t0 = t1;
		t1 = temp;
	}

	// if t1 is less than zero, the object is in the ray's negative direction
	// and consequently the ray misses the sphere
	float t;
	if (t1 < 0)
		return 0;

	if (t0 < 0) // if t0 is less than zero, the intersection point is at t1
		t = t1;
	else // else the intersection point is at t0
		t = t0;

	*_t = t;

	i[0] = o[0] + t*d[0];
	i[1] = o[1] + t*d[1];
	i[2] = o[2] + t*d[2];
	
	n[0] = i[0]-m_vCenter[0];
	n[1] = i[1]-m_vCenter[1];
	n[2] = i[2]-m_vCenter[2];
	
	return 1;
}

int Sphere::GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, float *_t, Vector3f &i, Vector3f &n)
{
	Vector3f vDirection;
	vDirection[0] = vEnd[0] - vStart[0];
	vDirection[1] = vEnd[1] - vStart[1];
	vDirection[2] = vEnd[2] - vStart[2];
	float l = sqrt ((vDirection).DotProduct (vDirection));
	(vDirection).Normalize ();
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res && *_t < l);
}



//
// Torus
//
int Torus::GetIntersectionWithRay (const Vector3f &vOrig, const Vector3f &vDirection, float *_t, Vector3f &i, Vector3f &n)
{
	float r2 = r*r;
	float R2 = R*R;
	float oz = vOrig[2];
	float oz2 = oz*oz;
	float dz = vDirection[2];
	float dz2 = dz*dz;

	float alpha = (vDirection).DotProduct (vDirection);
	float alpha2 = alpha * alpha;
	float beta = 2. * (vOrig).DotProduct (vDirection);
	float beta2 = beta * beta;
	float gamma = (vOrig).DotProduct (vOrig) - r2 - R2;
	float gamma2 = gamma * gamma;

	double c[5], s[4];
	c[4] = alpha2;
	c[3] = 2*alpha*beta;
	c[2] = beta2 + 2*alpha*gamma + 4*R2*dz2;
	c[1] = 2*beta*gamma + 8*R2*oz*dz;
	c[0] = gamma2 + 4*R2*oz2 - 4*R2*r2;

	int num = SolveQuartic (c, s);

	// smallest non-negative root = nearest forward intersection
	float t = -1.;
	for (int j=0; j<num; j++)
	{
		if ((t < 0. && s[j] >= 0.) ||
		    (s[j] >= 0. && s[j] < t))
			t = s[j];
	}
	if (t < 0.)
		return 0;

	*_t = t;
	i[0] = vOrig[0] + t*vDirection[0];
	i[1] = vOrig[1] + t*vDirection[1];
	i[2] = vOrig[2] + t*vDirection[2];

	Vector3f v;
	v[0] = i[0];
	v[1] = i[1];
	v[2] = 0.;
	(v).Normalize ();
	v[0] *= R;
	v[1] *= R;

	n[0] = (i[0] - v[0]);
	n[1] = (i[1] - v[1]);
	n[2] = i[2];
	(n).Normalize ();
	return 1;
}

int Torus::GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, float *_t, Vector3f &i, Vector3f &n)
{
	Vector3f vDirection;
	vDirection = vEnd - vStart;
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res && *_t < 1.);
}

//
// Triangle
//

// Reference : http://geomalgorithms.com/a06-_intersect-2.html
int Triangle::GetIntersectionWithRay (const Vector3f &vO, const Vector3f &vD, float *_t, Vector3f &i, Vector3f &n)
{
    Vector3f u, v;        // triangle vectors
    Vector3f w0, w;       // ray vectors
    float r, a, b;     // params to calc ray-plane intersect

    // get triangle edge vectors and plane normal
	u = m_v[1] - m_v[0];
	v = m_v[2] - m_v[0];
	n = (u).CrossProduct (v);
    if ((n).getLength () == 0.)      // triangle is degenerate
        return -1;                  // do not deal with this case
	(n).Normalize ();

	w0 = vO -  m_v[0];
	a = -(n).DotProduct (w0);
	b =  (n).DotProduct (vD);
    if (fabs(b) < 0.000001)
	{								// ray is  parallel to triangle plane
        if (a == 0)                 // ray lies in triangle plane
            return 2;
        else return 0;              // ray disjoint from plane
    }
	if (b >= 0.) // culling : triangle doesn't face the ray
		return 0;

    // get intersect point of ray with triangle plane
    r = a / b;
    if (r < 0.0)                    // ray goes away from triangle
        return 0;                   // => no intersect
    // for a segment, also test if (r > 1.0) => no intersect

	// intersect point of ray and plane
	i.Set (
		vO[0] + r * vD[0],
		vO[1] + r * vD[1],
		vO[2] + r * vD[2]);

    // is I inside T?
    float uu, uv, vv, wu, wv, D;
    uu = (u).DotProduct (u);
    uv = (u).DotProduct (v);
    vv = (v).DotProduct (v);
	w = i - m_v[0];
    wu = (w).DotProduct (u);
    wv = (w).DotProduct (v);
    D = uv * uv - uu * vv;

    // get and test parametric coords
    float s, t;
    s = (uv * wv - vv * wu) / D;
    if (s < 0.0 || s > 1.0)         // I is outside T
        return 0;
    t = (uv * wu - uu * wv) / D;
    if (t < 0.0 || (s + t) > 1.0)  // I is outside T
        return 0;

	*_t = r;

    return 1;                       // I is in T
}

int Triangle::GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, float *_t, Vector3f &i, Vector3f &n)
{
	Vector3f vDirection;
	vDirection = vEnd - vStart;
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res == 1 && *_t < 1.);
}
