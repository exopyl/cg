#pragma once
#include "TVector3.h"

// A plane in implicit form ax + by + cz + d = 0, packed as (a, b, c, d).
// Lightweight POD companion of the quadric error metric (plane -> quadric).
typedef float plane_t[4];

typedef double quadric_t[10];

extern void plane_init(plane_t plane, const Vector3f &v1, const Vector3f &v2, const Vector3f &v3);
extern void plane_quadric(plane_t plane_eq, quadric_t q);
void quadric_zero(quadric_t q);
void quadric_copy(quadric_t q1, quadric_t q2);
void quadric_add(quadric_t qr, quadric_t q1, quadric_t q2);
void quadric_scale(quadric_t qr, quadric_t q, float scale);
void quadric_dump(quadric_t q);
int quadric_minimize(quadric_t q, Vector3f &vnew, float *error);
double quadric_eval(quadric_t q, const Vector3f &v);
int quadric_minimize_edge(quadric_t q, Vector3f &vnew, float *error, const Vector3f &v0, const Vector3f &v1);
int quadric_minimize2(quadric_t q, Vector3f &vnew, float *error, const Vector3f &v0, const Vector3f &v1);
