#pragma once

#include <string.h>

#include "algebra_vector3.h"

typedef float mat3[3][3];
// fmat3[n][m] addresses the following elements :
// n = row
// m = column
// ie
// ( fmat3[0][0] fmat3[0][1] fmat3[0][2] )
// ( fmat3[1][0] fmat3[1][1] fmat3[1][2] )
// ( fmat3[2][0] mat3[2][1] mat3[2][2] )

extern int mat3_init (mat3 m,
		       float m00, float m01, float m02,
		       float m10, float m11, float m12,
		       float m20, float m21, float m22);
extern int mat3_init_array (mat3 m, float *array);
extern int mat3_init_identity (mat3 m);
extern int mat3_init_rotation_from_vec3_to_vec3 (mat3 m, vec3 vDir1, vec3 vDir2);
extern int mat3_init_rotation_from_euler_angles (mat3 m, float rotx, float roty, float rotz);

extern void mat3_transform(vec3 r, mat3 m, vec3 v);


extern void mat3_dump (mat3 m);

extern void mat3_mul(mat3 r, mat3 a, mat3 b);
extern void mat3_scale(mat3 res, mat3 src, float s);

extern void mat3_transpose(mat3 m);

static inline void mat3_copy(mat3 d, mat3 s)
{
	memcpy(d, s, sizeof(mat3));
}

extern int mat3_inverse (mat3 m);
extern int mat3_is_symmetric (mat3 m);
extern float mat3_determinant (mat3 m);
extern int mat3_solve_eigensystem (mat3 m,
				    vec3 evalues,
				    vec3 evector1,
				    vec3 evector2,
				    vec3 evector3);
extern int mat3_solve_linearsystem (mat3 m, vec3 right, vec3 res);
