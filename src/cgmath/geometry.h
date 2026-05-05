#pragma once

#include <vector>

#include "algebra_vector3.h"
#include "ray.h"
#include "aabox.h"
#include "TVector2.h"

//
// Geometry
//
class Geometry
{
public:
	Geometry();

	virtual bool GetIntersectionBboxWithRay (vec3 o, vec3 d);

	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n) = 0;
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n) = 0;

	virtual void* GetMaterial (void) = 0;

	AABox *m_pAABox;
};

//
// Plane
//
class Plane : public Geometry
{
public:
	Plane ();
	Plane (Vector3f normale, float distance);
	Plane (Vector3f v1, Vector3f v2, Vector3f v3);
	Plane (Vector3f pt, Vector3f normale);
	~Plane () {};
	
	float distance_point (Vector3f v);                 // distance between the plane and a point
	float distance_point (float x, float y, float z);  // distance between the plane and a point
	int  position (Vector3f v);                        // position of a point
	void projected (Vector3f v_projected, Vector3f v); // projected of a point on the plane
	void move (Vector3f res, Vector3f v, float d);     // move a point in the direction of the normale to let it at a distance d from the plane
	
	// getters - setters
	float get_distance (void);
	void get_normale (Vector3f &normale);
	
	// least square plane fitting
	//
	// Reference : Eberly "Game Engine Design",
	//             Morgan Kaufmann, ISBN 1558605932
	//             september, 2000
	//             pp 473, 474 + code
	//
	void fitting (Vector3f *array, int n);

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return nullptr; };

private:
	// equation of a plane : ax + by + cz + d = 0
	Vector3f normale; // (a , b , c)
	float distance; // c
};

//
// Circle (2D)
//
// Reference : Havemann, "Generative Mesh Modeling", TU Braunschweig, 2005, section 2.
// Used as base primitive for the gothic tracery construction (pointed arch,
// rosette, foils, ...).
//
class Circle
{
public:
	Circle ();
	Circle (Vector2d center, double radius);
	~Circle () {};

	// Point on the circle at angle theta (radians, CCW from +x axis)
	Vector2d pointAt (double theta) const;

	// Angle from the center to point p, in [-pi, pi] (atan2)
	double angleAt (Vector2d p) const;

	// Test whether p lies on the circle within `eps`
	bool contains (Vector2d p, double eps = 1e-9) const;

	// Result of the intersection of two circles.
	// count : 0 = disjoint or concentric
	//         1 = tangent (single point in pts[0])
	//         2 = secant  (pts[0] = upper, pts[1] = lower, sorted by descending y)
	struct IntersectionResult
	{
		int      count;
		Vector2d pts[2];
	};

	// Intersection with another circle.
	// Section 2 of Havemann's thesis :
	//   d = dist(c0.center, c1.center)
	//   a = (r0^2 - r1^2 + d^2) / (2 d)
	//   h = sqrt(r0^2 - a^2)
	//   P = c0.center + a * (c1.center - c0.center) / d
	//   pts = P +/- h * perp((c1.center - c0.center) / d)
	IntersectionResult intersection (const Circle &other) const;

	// Intersection with the segment [a, b].
	// Solves |a + t*(b-a) - center|^2 = radius^2 for t in [0, 1].
	// Result points are sorted by ascending t (i.e. pts[0] is closer to `a`).
	IntersectionResult segmentIntersection (Vector2d a, Vector2d b) const;

public:
	Vector2d center;
	double   radius;
};

//
// Ellipse (2D, two-foci form)
//
// Reference : Havemann, "Generative Mesh Modeling", TU Braunschweig, 2005,
// section 2.3. Used to position the rosette : the ellipse with foci at the
// two adjacent arc centers and sumDist = sum of their radii is the locus of
// candidate rosette centers; the rosette center is the intersection of this
// ellipse with the symmetry axis of the main arch.
//
// Parametric form (in the local frame of the ellipse) :
//   ec  = (f1 + f2) / 2
//   c   = dist(f1, f2) / 2
//   a   = sumDist / 2          (semi-major)
//   b   = sqrt(a^2 - c^2)      (semi-minor, requires sumDist >= dist(f1,f2))
//   ang = atan2(f2.y - f1.y, f2.x - f1.x)
//   P(t) = ec + a*cos(t) * (cos ang, sin ang)
//             + b*sin(t) * (-sin ang, cos ang)
//
class Ellipse
{
public:
	Ellipse ();
	Ellipse (Vector2d f1, Vector2d f2, double sumDist);
	~Ellipse () {};

	// Geometric properties (degenerate to 0 if the ellipse is invalid)
	Vector2d center () const;
	double   semiMajor () const;       // a = sumDist / 2
	double   semiMinor () const;       // b = sqrt(a^2 - c^2), or 0 if invalid
	double   angle () const;           // angle of the f1->f2 axis (atan2)
	bool     valid () const;           // sumDist >= dist(f1, f2)

	// Point on the ellipse at parameter t (radians, CCW in the local frame)
	Vector2d pointAt (double t) const;

	// Test whether p lies on the ellipse within `eps` (focal-sum criterion)
	bool contains (Vector2d p, double eps = 1e-9) const;

	struct IntersectionResult
	{
		int      count;
		Vector2d pts[2];   // pts[0] = upper (max y) when count == 2
	};

	// Intersection with the vertical line x = vx.
	// Algorithm : R*cos(t + phi) = vx - ec.x  with
	//   R   = hypot(a*cos ang, b*sin ang)
	//   phi = atan2(b*sin ang, a*cos ang)
	IntersectionResult verticalIntersection (double vx) const;

public:
	Vector2d f1, f2;
	double   sumDist;
};

//
// Arc (2D oriented arc of a circle)
//
// Reference : Havemann, "Generative Mesh Modeling", TU Braunschweig, 2005,
// sections 2 and 3.5. Base primitive used by every gothic operator (pointed
// arch, offsets, foils, trefoil, fillets) and by the tessellation pipeline.
//
// Parameterization :
//   span(t)    = signed angle swept from angleStart to angleEnd, depending on
//                ccw. Always >= 0 when ccw, <= 0 when !ccw, in [-2*PI, 2*PI].
//   theta(t)   = angleStart + t * span,        t in [0, 1]
//   pointAt(t) = circle.pointAt(theta(t))
//
// Conventions :
//   - tangentAt(t) is a unit vector consistent with the direction of travel
//     (rotates +90 degrees from the outward radial when ccw, -90 when !ccw).
//   - normalAt(t) is the inward normal : (center - pointAt(t)) / radius,
//     independent of ccw. Spec : "normale interieure (vers le centre)".
//
class Arc
{
public:
	Arc ();
	Arc (Circle circle, double angleStart, double angleEnd, bool ccw);
	Arc (Circle circle, Vector2d from, Vector2d to, bool ccw);
	~Arc () {};

	// Signed angle swept by the arc.
	//   ccw  : in [0,        2*PI]
	//   !ccw : in [-2*PI,    0   ]
	double spanAngle () const;

	// Arc length : |spanAngle| * radius
	double length () const;

	// Point on the arc at parameter t in [0, 1]
	Vector2d pointAt (double t) const;

	// Unit tangent at t (consistent with the direction of travel)
	Vector2d tangentAt (double t) const;

	// Inward normal at t : (center - pointAt(t)) / radius. Independent of ccw.
	Vector2d normalAt (double t) const;

	// Uniform tessellation : returns n+1 points sampled at t = i/n. n is
	// clamped to >= 1.
	std::vector<Vector2d> tessellate (int n) const;

	// Adaptive tessellation : at most maxAngleRad swept per segment.
	// Equivalent to tessellate(ceil(|spanAngle| / maxAngleRad)).
	// maxAngleRad <= 0 falls back to a single segment.
	std::vector<Vector2d> tessellateAdaptive (double maxAngleRad) const;

public:
	Circle circle;
	double angleStart;
	double angleEnd;
	bool   ccw;
};

//
// Sphere
//
class Sphere : public Geometry
{
public:
	Sphere () {
		m_vCenter[0] = 0.;
		m_vCenter[1] = 0.;
		m_vCenter[2] = 0.;
		m_fRadius = 1.;
		m_pAABox = new AABox (Vector3 (-1., -1., -1.), Vector3 (1., 1., 1.));
	};
	~Sphere () { if (m_pAABox) delete m_pAABox; };

	void SetCenter (float vCenter[3])
	{
		m_vCenter[0] = vCenter[0];
		m_vCenter[1] = vCenter[1];
		m_vCenter[2] = vCenter[2];
		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};
	void SetCenter (float x, float y, float z)
	{
		m_vCenter[0] = x;
		m_vCenter[1] = y;
		m_vCenter[2] = z;
		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};
	void SetRadius (float fRadius)
	{
		m_fRadius = fRadius; 
 		delete m_pAABox;
		m_pAABox = new AABox (Vector3 (m_vCenter[0]-m_fRadius, m_vCenter[1]-m_fRadius, m_vCenter[2]-m_fRadius),
				      Vector3 (m_vCenter[0]+m_fRadius, m_vCenter[1]+m_fRadius, m_vCenter[2]+m_fRadius));
	};

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return nullptr; };

private:
	float m_vCenter[3];
	float m_fRadius;
};

//
// Torus
//
class Torus : public Geometry
{
public:
	Torus () {
		R = 1.; r = 0.2;
		m_pAABox = nullptr;
	};
	Torus (float _R, float _r) {
		R = _R;
		r = _r;
		m_pAABox = nullptr;
	};
	~Torus () { if (m_pAABox) delete m_pAABox; };

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return nullptr; };

public:
	float R, r;
};

//
// triangle
//
class Triangle : public Geometry
{
public:
	Triangle () {
		for (int i=0; i<3; i++)
			vec3_init (m_v[i], 0., 0., 0.);
		m_pAABox = nullptr;
	};
	~Triangle () { if (m_pAABox) delete m_pAABox; };

	inline void SetVertex (int i, float x, float y, float z)
	{
		vec3_init (m_v[i], x, y, z);
		if (m_pAABox)
			m_pAABox->AddVertex (x, y, z);
		else
			m_pAABox = new AABox (x, y, z);
	}
	void Init (float x1, float y1, float z1,
			   float x2, float y2, float z2,
			   float x3, float y3, float z3)
	{
		vec3_init (m_v[0], x1, y1, z1);
		vec3_init (m_v[1], x2, y2, z2);
		vec3_init (m_v[2], x3, y3, z3);
		if (m_pAABox)
			delete m_pAABox;
		m_pAABox = new AABox (x1, y1, z1);
		m_pAABox->AddVertex (x2, y2, z2);
		m_pAABox->AddVertex (x3, y3, z3);
	}

	// intersections
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);

	virtual void* GetMaterial (void) { return nullptr; };

public:
	vec3 m_v[3];
};
