#include "algebra_intersections.h"
#include "common.h"

//
// Intersection Segment2D Segment2D
// Reference :
// "Computational Geometry in C" O'Rourke
// http://maven.smith.edu/~orourke/books/ftp.html
//
// returns :
// 0 : no intersection
// 1 : intersection
// 2 : The segments collinearly overlap, sharing a point.
// 3 : An endpoint (vertex) of one segment is on the other segment, but case 2 doesn't hold.
//
//
static inline int fvec2_orient (vec2 pa, vec2 pb, vec2 pc)
{
	double acx, bcx, acy, bcy;
  
	acx = (double) pa[0] - (double) pc[0];
	bcx = (double) pb[0] - (double) pc[0];
	acy = (double) pa[1] - (double) pc[1];
	bcy = (double) pb[1] - (double) pc[1];
	return sign (acx * bcy - acy * bcx);
}

static int Between (vec2 a, vec2 b, vec2 c)
{
	// If ab not vertical, check betweenness on x; else on y.
	if ( a[0] != b[0] )
		return ((a[0] <= c[0]) && (c[0] <= b[0])) ||
			((a[0] >= c[0]) && (c[0] >= b[0]));
	else
		return ((a[1] <= c[1]) && (c[1] <= b[1])) ||
			((a[1] >= c[1]) && (c[1] >= b[1]));
}

static int Collinear (vec2 a, vec2 b, vec2 c)
{
	return fvec2_orient(a, b, c) == 0;
}

static unsigned int seg2_seg2_intersection_parallel (seg2 s1, seg2 s2, vec2 res)
{
	if ( !Collinear( s1.vs, s1.ve, s2.vs) )
		return 0;
	
	if ( Between( s1.vs, s1.ve, s2.vs ) )
	{
		res[0] = s2.vs[0];
		res[1] = s2.vs[1];
		return 2;
	}
	if ( Between( s1.vs, s1.ve, s2.ve ) ) {
		res[0] = s2.ve[0];
		res[1] = s2.ve[1];
		return 2;
	}
	if ( Between( s2.vs, s2.ve, s1.vs ) ) {
		res[0] = s1.vs[0];
		res[1] = s1.vs[1];
		return 2;
	}
	if ( Between( s2.vs, s2.ve, s1.ve ) ) {
		res[0] = s1.ve[0];
		res[1] = s1.ve[1];
		return 2;
	}
	return 0;
}

unsigned int seg2_seg2_intersection (seg2 s1, seg2 s2, vec2 res)
{
	double  s, t;       // The two parameters of the parametric eqns.
	double num, denom;  // Numerator and denoninator of equations.
	int code = -1;    // Return char characterizing intersection.
	
	denom = s1.vs[0] * (float)( s2.ve[1] - s2.vs[1] ) +
		s1.ve[0] * (float)( s2.vs[1] - s2.ve[1] ) +
		s2.ve[0] * (float)( s1.ve[1] - s1.vs[1] ) +
		s2.vs[0] * (float)( s1.vs[1] - s1.ve[1] );
	
	// If denom is zero, then segments are parallel: handle separately.
	if (denom == 0.0)
		return seg2_seg2_intersection_parallel (s1, s2, res);
	
	num =   s1.vs[0] * (float)( s2.ve[1] - s2.vs[1] ) +
		s2.vs[0] * (float)( s1.vs[1] - s2.ve[1] ) +
		s2.ve[0] * (float)( s2.vs[1] - s1.vs[1] );
	if ( (num == 0.0) || (num == denom) )
		code = 3;
	s = num / denom;
	//dbg("num=%lf, denom=%lf, s=%lf", num, denom, s);
	
	num = -( s1.vs[0] * (float)( s2.vs[1] - s1.ve[1] ) +
		 s1.ve[0] * (float)( s1.vs[1] - s2.vs[1] ) +
		 s2.vs[0] * (float)( s1.ve[1] - s1.vs[1] ) );
	if ( (num == 0.0) || (num == denom) )
		code = 3;
	t = num / denom;
	//dbg("num=%lf, denom=%lf, t=%lf", num, denom, t);
	
	if      ( (0.0 < s) && (s < 1.0) &&
		  (0.0 < t) && (t < 1.0) )
		code = 1;
	else if ( (0.0 > s) || (s > 1.0) ||
		  (0.0 > t) || (t > 1.0) )
		code = 0;
	
	res[0] = s1.vs[0] + s * ( s1.ve[0] - s1.vs[0] );
	res[1] = s1.vs[1] + s * ( s1.ve[1] - s1.vs[1] );
	
	return code;
}

//
// line vs ellipse
//
// ellipse : x^2/a^2 + y^2/b^2 = 1
//
unsigned int line_ellipse_intersection (vec2 line_start, vec2 line_end, vec2 ellipse_center, vec2 ellipse_radius, vec2 res1, vec2 res2)
{
	// translate center of the ellipse to the origin
	vec2 l_start, l_end;
	vec2_subtraction (l_start, line_start, ellipse_center);
	vec2_subtraction (l_end, line_end, ellipse_center);
	
	// define the line with the following parametric equation : P = l_start + t * l_dir
	vec2 l_dir;
	vec2_subtraction (l_dir, l_end, l_start);
	vec2_normalize (l_dir);

	// solve the quadratic equation a*t^2 + b*t + c = 0 with :
	float rx2 = ellipse_radius[0]*ellipse_radius[0];
	float ry2 = ellipse_radius[1]*ellipse_radius[1];
	float a = l_dir[0]*l_dir[0]*ry2 + l_dir[1]*l_dir[1]*rx2;
	float b = 2*ry2*l_start[0]*l_dir[0] + 2*rx2*l_start[1]*l_dir[1];
	float c = ry2*l_start[0]*l_start[0] + rx2*l_start[1]*l_start[1] - rx2*ry2;
	float discriminant = b*b - 4*a*c;
	if (discriminant > 0.)
	{
		float t1 = (-b-sqrt(discriminant))/(2.*a);
		float t2 = (-b+sqrt(discriminant))/(2.*a);
		vec2_init (res1,
			   ellipse_center[0] + l_start[0] + t1*l_dir[0],
			   ellipse_center[1] + l_start[1] + t1*l_dir[1]);
		vec2_init (res2,
			   ellipse_center[0] + l_start[0] + t2*l_dir[0],
			   ellipse_center[1] + l_start[1] + t2*l_dir[1]);
		return 2;
	}
	else if (discriminant == 0.)
	{
		float t = -.5*b/a;
		vec2_init (res1,
			   ellipse_center[0] + l_start[0] + t*l_dir[0],
			   ellipse_center[1] + l_start[1] + t*l_dir[1]);
		return 1;
	}

	return 0;
}

