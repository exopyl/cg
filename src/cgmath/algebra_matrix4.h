#pragma once

typedef float mat4[16];

extern int mat4_init (mat4 m,
		       float m00, float m01, float m02, float m03,
		       float m10, float m11, float m12, float m13,
		       float m20, float m21, float m22, float m23,
		       float m30, float m31, float m32, float m33);
extern int mat4_set_identity (float *m);
extern int mat4_get_inverse (float *dst, float *src);
extern int mat4_transform (float *dst, float *m, float *v);
