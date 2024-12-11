#ifndef __ALGEBRA_QUATERNION_H__
#define __ALGEBRA_QUATERNION_H__

#include "algebra_matrix3.h"

// quaternion addresses the following elements : x, y, z, w
typedef float quaternion[4];

extern void quaternion_init (quaternion q, float x, float y, float z, float w);
extern void quaternion_init_axis_angle (quaternion q, vec3 axis, float angle);
extern int  quaternion_init_matrix3 (quaternion q, mat3 m);
extern int  quaternion_init_slerp (quaternion res, quaternion p, quaternion q, float t);

extern void quaternion_copy (quaternion copy, quaternion source);

extern float quaternion_norm (quaternion q);
extern int   quaternion_normalize (quaternion q);
extern void  quaternion_convert_to_matrix3 (quaternion q, mat3 mat);
extern void  quaternion_dump (quaternion q);

extern int  quaternion_rotate (quaternion q, vec3 res, vec3 v);

#endif // __ALGEBRA_QUATERNION_H__

