#include <string.h>

#include "algebra_matrix4.h"

int mat4_init (mat4 m,
		       float m00, float m01, float m02, float m03,
		       float m10, float m11, float m12, float m13,
		       float m20, float m21, float m22, float m23,
		       float m30, float m31, float m32, float m33)
{
	m[0]  = m00; m[1]  = m01; m[2]  = m02; m[3]  = m03;
	m[4]  = m10; m[5]  = m11; m[6]  = m12; m[7]  = m13;
	m[8]  = m20; m[9]  = m21; m[10] = m22; m[11] = m23;
	m[12] = m30; m[13] = m31; m[14] = m32; m[15] = m33;

	return 1;
}

int mat4_set_identity (float *m)
{
	memset (m, 0, 16*sizeof(float));
	m[0] = 1.;
	m[5] = 1.;
	m[10] = 1.;
	m[15] = 1.;

	return 1;
}

int mat4_get_inverse (float *_dst, float *_src)
{
	int i,j;
	float tmp[12]; // temp array for pairs
	float src[16]; // array of transpose source matrix
	float dst[16];
	float det;     // determinant

	float tmp2[16];
	for (int i=0; i<16; i++)
		tmp2[i] = _src[i];

	// transpose matrix
	for (i = 0; i < 4; i++)
	{
		src[i]      = tmp2[i*4];
		src[i + 4]  = tmp2[i*4 + 1];
		src[i + 8]  = tmp2[i*4 + 2];
		src[i + 12] = tmp2[i*4 + 3];
	}

	// calculate pairs for first 8 elements (cofactors)
	tmp[0]  = src[10] * src[15];
	tmp[1]  = src[11] * src[14];
	tmp[2]  = src[9]  * src[15];
	tmp[3]  = src[11] * src[13];
	tmp[4]  = src[9]  * src[14];
	tmp[5]  = src[10] * src[13];
	tmp[6]  = src[8]  * src[15];
	tmp[7]  = src[11] * src[12];
	tmp[8]  = src[8]  * src[14];
	tmp[9]  = src[10] * src[12];
	tmp[10] = src[8]  * src[13];
	tmp[11] = src[9]  * src[12];

	// calculate first 8 elements (cofactors)
	dst[0] =  tmp[0]*src[5] + tmp[3]*src[6] + tmp[4]*src[7];
	dst[0] -= tmp[1]*src[5] + tmp[2]*src[6] + tmp[5]*src[7];
	dst[1] =  tmp[1]*src[4] + tmp[6]*src[6] + tmp[9]*src[7];
	dst[1] -= tmp[0]*src[4] + tmp[7]*src[6] + tmp[8]*src[7];
	dst[2] =  tmp[2]*src[4] + tmp[7]*src[5] + tmp[10]*src[7];
	dst[2] -= tmp[3]*src[4] + tmp[6]*src[5] + tmp[11]*src[7];
	dst[3] =  tmp[5]*src[4] + tmp[8]*src[5] + tmp[11]*src[6];
	dst[3] -= tmp[4]*src[4] + tmp[9]*src[5] + tmp[10]*src[6];
	dst[4] =  tmp[1]*src[1] + tmp[2]*src[2] + tmp[5]*src[3];
	dst[4] -= tmp[0]*src[1] + tmp[3]*src[2] + tmp[4]*src[3];
	dst[5] =  tmp[0]*src[0] + tmp[7]*src[2] + tmp[8]*src[3];
	dst[5] -= tmp[1]*src[0] + tmp[6]*src[2] + tmp[9]*src[3];
	dst[6] =  tmp[3]*src[0] + tmp[6]*src[1] + tmp[11]*src[3];
	dst[6] -= tmp[2]*src[0] + tmp[7]*src[1] + tmp[10]*src[3];
	dst[7] =  tmp[4]*src[0] + tmp[9]*src[1] + tmp[10]*src[2];
	dst[7] -= tmp[5]*src[0] + tmp[8]*src[1] + tmp[11]*src[2];

	// calculate pairs for second 8 elements (cofactors)
	tmp[0] = src[2]*src[7];
	tmp[1] = src[3]*src[6];
	tmp[2] = src[1]*src[7];
	tmp[3] = src[3]*src[5];
	tmp[4] = src[1]*src[6];
	tmp[5] = src[2]*src[5];
	tmp[6] = src[0]*src[7];
	tmp[7] = src[3]*src[4];
	tmp[8] = src[0]*src[6];
	tmp[9] = src[2]*src[4];
	tmp[10] = src[0]*src[5];
	tmp[11] = src[1]*src[4];

	// calculate second 8 elements (cofactors)
	dst[8] = tmp[0]*src[13] + tmp[3]*src[14] + tmp[4]*src[15];
	dst[8] -= tmp[1]*src[13] + tmp[2]*src[14] + tmp[5]*src[15];
	dst[9] = tmp[1]*src[12] + tmp[6]*src[14] + tmp[9]*src[15];
	dst[9] -= tmp[0]*src[12] + tmp[7]*src[14] + tmp[8]*src[15];
	dst[10] = tmp[2]*src[12] + tmp[7]*src[13] + tmp[10]*src[15];
	dst[10]-= tmp[3]*src[12] + tmp[6]*src[13] + tmp[11]*src[15];
	dst[11] = tmp[5]*src[12] + tmp[8]*src[13] + tmp[11]*src[14];
	dst[11]-= tmp[4]*src[12] + tmp[9]*src[13] + tmp[10]*src[14];
	dst[12] = tmp[2]*src[10] + tmp[5]*src[11] + tmp[1]*src[9];
	dst[12]-= tmp[4]*src[11] + tmp[0]*src[9] + tmp[3]*src[10];
	dst[13] = tmp[8]*src[11] + tmp[0]*src[8] + tmp[7]*src[10];
	dst[13]-= tmp[6]*src[10] + tmp[9]*src[11] + tmp[1]*src[8];
	dst[14] = tmp[6]*src[9] + tmp[11]*src[11] + tmp[3]*src[8];
	dst[14]-= tmp[10]*src[11] + tmp[2]*src[8] + tmp[7]*src[9];
	dst[15] = tmp[10]*src[10] + tmp[4]*src[8] + tmp[9]*src[9];
	dst[15]-= tmp[8]*src[9] + tmp[11]*src[10] + tmp[5]*src[8];

	// calculate determinant
	det=src[0]*dst[0]+src[1]*dst[1]+src[2]*dst[2]+src[3]*dst[3];
	if (det == 0.0)
		return 0;

	// calculate matrix inverse
	det = 1/det;
	for (j = 0; j < 16; j++)
		_dst[j] = dst[j] * det;
	
	return 1;
}

int mat4_transform (float *dst, float *m, float *v)
{
	float t[4];
	t[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];// + m[3];
	t[1] = m[4] * v[0] + m[5] * v[1] + m[6] * v[2];// + m[7];
	t[2] = m[8] * v[0] + m[9] * v[1] + m[10] * v[2];// + m[11];
	t[3] = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15];

	dst[0] = t[0] / t[3];
	dst[1] = t[1] / t[3];
	dst[2] = t[2] / t[3];

	return 1;
}
