#pragma once

#include "TVector2.h"

// 2D segment (start/end points).
typedef struct seg2
{
	Vector2f vs;
	Vector2f ve;
} seg2;

extern unsigned int seg2_seg2_intersection (seg2 s1, seg2 s2, float* res);
extern unsigned int line_ellipse_intersection (const float* line_start, const float* line_end, const float* ellipse_center, const float* ellipse_radius, float* res1, float* res2);
