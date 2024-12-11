#ifndef __ALGEBRA_INTERSECTIONS_H__
#define __ALGEBRA_INTERSECTIONS_H__

#include "algebra_vector2.h"

extern unsigned int seg2_seg2_intersection (seg2 s1, seg2 s2, vec2 res);
extern unsigned int line_ellipse_intersection (vec2 line_start, vec2 line_end, vec2 ellipse_center, vec2 ellipse_radius, vec2 res1, vec2 res2);

#endif // __ALGEBRA_INTERSECTIONS_H__
