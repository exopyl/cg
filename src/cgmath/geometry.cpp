#include <math.h>

#include "geometry.h"
#include "algebra_vector3.h"
#include "polynomial.h"

Geometry::Geometry()
{
	m_pAABox = NULL;
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
