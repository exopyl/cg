#include <math.h>

#include "geometry.h"
#include "algebra_vector3.h"
#include "common.h"
#include "polynomial.h"

Geometry::Geometry()
{
	m_pAABox = nullptr;
}

//
//
bool Geometry::GetIntersectionBboxWithRay (vec3 o, vec3 d)
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

Circle::IntersectionResult Circle::segmentIntersection (Vector2d a, Vector2d b) const
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
int Sphere::GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n)
{
	vec3 vCO;
	vCO[0] = o[0] - m_vCenter[0];
	vCO[1] = o[1] - m_vCenter[1];
	vCO[2] = o[2] - m_vCenter[2];

	//Compute A, B and C coefficients
	float a =      vec3_dot_product (d, d);
	float b = 2. * vec3_dot_product (d, vCO);
	float c =      vec3_dot_product (vCO, vCO) - (m_fRadius * m_fRadius);
	
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

int Sphere::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	vec3 vDirection;
	vDirection[0] = vEnd[0] - vStart[0];
	vDirection[1] = vEnd[1] - vStart[1];
	vDirection[2] = vEnd[2] - vStart[2];
	float l = sqrt (vec3_dot_product (vDirection, vDirection));
	vec3_normalize (vDirection);
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res && *_t < l);
}



//
// Torus
//
int Torus::GetIntersectionWithRay (vec3 vOrig, vec3 vDirection, float *_t, vec3 i, vec3 n)
{
	float r2 = r*r;
	float R2 = R*R;
	float oz = vOrig[2];
	float oz2 = oz*oz;
	float dz = vDirection[2];
	float dz2 = dz*dz;

	float alpha = vec3_dot_product (vDirection, vDirection);
	float alpha2 = alpha * alpha;
	float beta = 2. * vec3_dot_product (vOrig, vDirection);
	float beta2 = beta * beta;
	float gamma = vec3_dot_product (vOrig, vOrig) - r2 - R2;
	float gamma2 = gamma * gamma;

	double c[5], s[4], err[4];
	c[4] = alpha2;
	c[3] = 2*alpha*beta;
	c[2] = beta2 + 2*alpha*gamma + 4*R2*dz2;
	c[1] = 2*beta*gamma + 8*R2*oz*dz;
	c[0] = gamma2 + 4*R2*oz2 - 4*R2*r2;

	//printf ("%f %f %f %f %f\n", c[0], c[1], c[2], c[3], c[4]);

	int num, num1, num2;
	num1 = SolveQuartic (c, s);
	num2 = quartic (c[3]/c[4], c[2]/c[4], c[1]/c[4], c[0]/c[4], s, err);

	if (0 && num1 != num2)
	{
		num1 = SolveQuartic (c, s);
		if (1 || num1 > 0)
			printf ("  -%d-> %f %f %f %f\n", num1, s[0], s[1], s[2], s[3]);
		num2 = quartic (c[3]/c[4], c[2]/c[4], c[1]/c[4], c[0]/c[4], s, err);
		if (1 || num2 > 0)
		{
			printf ("  -%d-> %f %f %f %f\n", num2, s[0], s[1], s[2], s[3]);
			printf ("\n");
		}
	}

	num = num1;
	if (1 || num > 0)// && s[1]<=0. && s[2]<=0. && s[3]<=0.)
	{
		float t = -1.;
		for (int j=0; j<num; j++)
		{
			//printf ("%f ", s[j]);
			if ((t < 0. && s[j] >= 0.) ||
			    (s[j] >= 0. && s[j] < t))
				t = s[j];
		}
		//printf ("\n");
		if (t < 0.)
			return 0;

		*_t = t;
		i[0] = vOrig[0] + t*vDirection[0];
		i[1] = vOrig[1] + t*vDirection[1];
		i[2] = vOrig[2] + t*vDirection[2];

		float v[3];
		v[0] = i[0];
		v[1] = i[1];
		v[2] = 0.;
		vec3_normalize (v);
		v[0] *= R;
		v[1] *= R;

		n[0] = (i[0] - v[0]);
		n[1] = (i[1] - v[1]);
		n[2] = i[2];
		vec3_normalize (n);
		return 1;
	}
	else
		return 0;
}

int Torus::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	vec3 vDirection;
	vec3_subtraction (vDirection, vEnd, vStart);
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res && *_t < 1.);
}

//
// Triangle
//

// Reference : http://geomalgorithms.com/a06-_intersect-2.html
int Triangle::GetIntersectionWithRay (vec3 vO, vec3 vD, float *_t, vec3 i, vec3 n)
{
    vec3  u, v;        // triangle vectors
    vec3  w0, w;       // ray vectors
    float r, a, b;     // params to calc ray-plane intersect

    // get triangle edge vectors and plane normal
	vec3_subtraction (u, m_v[1], m_v[0]);
	vec3_subtraction (v, m_v[2], m_v[0]);
	vec3_cross_product (n, u, v);
    if (vec3_length (n) == 0.)      // triangle is degenerate
        return -1;                  // do not deal with this case
	vec3_normalize (n);

	vec3_subtraction (w0, vO,  m_v[0]);
	a = -vec3_dot_product (n, w0);
	b =  vec3_dot_product (n, vD);
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
	vec3_init (i,
		vO[0] + r * vD[0],
		vO[1] + r * vD[1],
		vO[2] + r * vD[2]);

    // is I inside T?
    float uu, uv, vv, wu, wv, D;
    uu = vec3_dot_product (u,u);
    uv = vec3_dot_product (u,v);
    vv = vec3_dot_product (v,v);
	vec3_subtraction (w, i, m_v[0]);
    wu = vec3_dot_product (w,u);
    wv = vec3_dot_product (w,v);
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

int Triangle::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	vec3 vDirection;
	vec3_subtraction (vDirection, vEnd, vStart);
	unsigned int res = GetIntersectionWithRay (vStart, vDirection, _t, i, n);
	return (res == 1 && *_t < 1.);
}
