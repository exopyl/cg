#ifndef __QUADRIC_H__
#define __QUADRIC_H__

#include "algebra_vector3.h"
#include "algebra_vector4.h"

typedef double quadric_t[10];

extern void plane_quadric(vec4 plane_eq, quadric_t q);
void quadric_zero(quadric_t q);
void quadric_copy(quadric_t q1, quadric_t q2);
void quadric_add(quadric_t qr, quadric_t q1, quadric_t q2);
void quadric_scale(quadric_t qr, quadric_t q, float scale);
void quadric_dump(quadric_t q);
int quadric_minimize(quadric_t q, vec3 vnew, float *error);
double quadric_eval(quadric_t q, vec3 v);
int quadric_minimize_edge(quadric_t q, vec3 vnew, float *error, vec3 v0, vec3 v1);
int quadric_minimize2(quadric_t q, vec3 vnew, float *error, vec3 v0, vec3 v1);

#endif // __QUADRIC_H__
