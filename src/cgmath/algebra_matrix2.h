#pragma once

#include "algebra_vector2.h"

typedef float mat2[2][2];

extern int mat2_init (mat2 m, float m00, float m01, float m10, float m11);
extern int mat2_transform (vec2 res, mat2 m, vec2 v);
extern void mat2_copy (mat2 dst, mat2 src);
extern float mat2_determinant (mat2 m);
extern int mat2_inverse (mat2 m);
extern int mat2_solve_eigensystem (mat2 m, vec2 evector1, vec2 evector2, vec2 evalues);
extern void mat2_dump (mat2 m);
