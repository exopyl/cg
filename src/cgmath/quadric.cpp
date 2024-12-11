#include <string.h>
#include <math.h>

#include "quadric.h"
#include "algebra_matrix3.h"
#include "common.h"


void plane_quadric(vec4 plane_eq, quadric_t q)
{
	q[0] = plane_eq[0] * plane_eq[0];
	q[1] = plane_eq[1] * plane_eq[1];
	q[2] = plane_eq[2] * plane_eq[2];
	q[3] = plane_eq[3] * plane_eq[3];
	q[4] = plane_eq[0] * plane_eq[1];
	q[5] = plane_eq[1] * plane_eq[2];
	q[6] = plane_eq[2] * plane_eq[3];
	q[7] = plane_eq[0] * plane_eq[2];
	q[8] = plane_eq[1] * plane_eq[3];
	q[9] = plane_eq[0] * plane_eq[3];
}

void quadric_zero(quadric_t q)
{
	memset(q, 0, sizeof(quadric_t));
}

void quadric_copy(quadric_t q1, quadric_t q2)
{
	memcpy(q1, q2, sizeof(quadric_t));
}

void quadric_add(quadric_t qr, quadric_t q1, quadric_t q2)
{
	qr[0] = q1[0] + q2[0];
	qr[1] = q1[1] + q2[1];
	qr[2] = q1[2] + q2[2];
	qr[3] = q1[3] + q2[3];
	qr[4] = q1[4] + q2[4];
	qr[5] = q1[5] + q2[5];
	qr[6] = q1[6] + q2[6];
	qr[7] = q1[7] + q2[7];
	qr[8] = q1[8] + q2[8];
	qr[9] = q1[9] + q2[9];
}

void quadric_scale(quadric_t qr, quadric_t q, float scale)
{
	for (unsigned int i = 0; i < 10; i++)
		qr[i] = q[i] * scale;
}

void quadric_dump(quadric_t q)
{
	for (unsigned int i = 0; i < 10; i++)
		printf ("%f ", q[i]);
	printf ("\n");
	return;
	printf ("%e %e %e %e\n", q[0], q[4], q[7], q[9]);
	printf ("%e %e %e %e\n", q[4], q[1], q[5], q[8]);
	printf ("%e %e %e %e\n", q[7], q[5], q[2], q[6]);
	printf ("%e %e %e %e\n", q[9], q[8], q[6], q[3]);
}

int quadric_minimize(quadric_t q, vec3 vnew, float *error)
{
	double fdet, finvdet;

	fdet = q[0] * (q[1]*q[2] - q[5]*q[5]) -
	       q[4] * (q[4]*q[2] - q[5]*q[7]) +
	       q[7] * (q[4]*q[5] - q[1]*q[7]);

	if (fabs(fdet) <= EPSILON)
		return -1;

	if (isnan(fdet))
		return -1;

	finvdet = 1.0 / fdet;
	//quadric_dump (q);
	//printf ("fdet : %e\n", fdet);
	//printf ("finvdet : %e\n", finvdet);

	double Ai[3][3];
	double vest[3];
	Ai[0][0] = -(q[2]*q[1] - q[5]*q[5]) * finvdet;
	Ai[0][1] = -(q[5]*q[7] - q[2]*q[4]) * finvdet;
	Ai[0][2] = -(q[5]*q[4] - q[1]*q[7]) * finvdet;
	Ai[1][0] = -(q[7]*q[5] - q[2]*q[4]) * finvdet;
	Ai[1][1] = -(q[2]*q[0] - q[7]*q[7]) * finvdet;
	Ai[1][2] = -(q[4]*q[7] - q[5]*q[0]) * finvdet;
	Ai[2][0] = -(q[5]*q[4] - q[7]*q[1]) * finvdet;
	Ai[2][1] = -(q[7]*q[4] - q[5]*q[0]) * finvdet;
	Ai[2][2] = -(q[1]*q[0] - q[4]*q[4]) * finvdet;

	/* vnew = - A^(-1) B */
	vest[0] = Ai[0][0] * q[9] + Ai[0][1] * q[8] + Ai[0][2] * q[6];
	vest[1] = Ai[1][0] * q[9] + Ai[1][1] * q[8] + Ai[1][2] * q[6];
	vest[2] = Ai[2][0] * q[9] + Ai[2][1] * q[8] + Ai[2][2] * q[6];
	//printf ("vest %e %e %e\n", vest[0], vest[1], vest[2]);

	/* distance = b'vnew + c */
	*error = vest[0] * q[9] + vest[1] * q[8] + vest[2] * q[6] + q[3];
	vnew[0] = (float) vest[0];
	vnew[1] = (float) vest[1];
	vnew[2] = (float) vest[2];
	//printf ("vnew %e %e %e\n", vnew[0], vnew[1], vnew[2]);
	//printf ("%e %e %e %e %e %e %e => %e => err %e\n", vest[0], q[9], vest[1], q[8], vest[2], q[6], q[3], vest[0] * q[9] + vest[1] * q[8] + vest[2] * q[6] + q[3], *error);

	return 0;
}

double quadric_eval(quadric_t q, vec3 v)
{
	/* vAv + 2bv + c */
	return v[0]*v[0]*q[0] + v[1]*v[1]*q[1] + v[2]*v[2]*q[2] +
		2*v[1]*v[2]*q[5] + 2*v[0]*v[1]*q[4] + 2*v[0]*v[2]*q[7] +
		2*v[0]*q[9] + 2*v[1]*q[8] + 2*v[2]*q[6] +
		q[3];
}

int quadric_minimize_edge(quadric_t q, vec3 vnew, float *error, vec3 v0, vec3 v1)
{
	vec3 d;

	vec3_subtraction(d, v0, v1);

	mat3 A;
	A[0][0] = q[0];
	A[0][1] = q[4];
	A[0][2] = q[7];
	A[1][0] = q[4];
	A[1][1] = q[1];
	A[1][2] = q[5];
	A[2][0] = q[7];
	A[2][1] = q[5];
	A[2][2] = q[2];

	vec3 Av1;
	mat3_transform(Av1, A, v1);

	vec3 Ad;
	mat3_transform(Ad, A, d);

	double det = 2.0 * vec3_dot_product(d, Ad);
	if (fabs(det) < EPSILON)
		return -1;

	if (isnan(det))
		return -1;

	vec3 qv;
	qv[0] = q[9];
	qv[1] = q[8];
	qv[2] = q[6];
	double a = -(2.0 * vec3_dot_product(qv, d) +
		     vec3_dot_product(d, Av1) +
		     vec3_dot_product(v1, Ad)) /
		(2.0 * vec3_dot_product(d, Ad));
	
	if (a < 0.0)
		a = 0.0;
	else if(a > 1.0)
		a = 1.0;

	vec3_scale(vnew, d, a);
	vec3_addition(vnew, vnew, v1);

	/* vAv + 2bv + c */
	*error = quadric_eval(q, vnew);

	return 0;
}

int quadric_minimize2(quadric_t q, vec3 vnew, float *error, vec3 v0, vec3 v1)
{
	if (quadric_minimize(q, vnew, error) == 0)
	{
		//printf ("A\n");
		return 0;
	}

	if (quadric_minimize_edge(q, vnew, error, v0, v1) == 0)
	{
		//printf ("B\n");
		return 0;
	}
	//printf ("C\n");
	
	vec3 vm;

	vec3_addition(vm, v0, v1);
	vec3_scale(vm, vm, 0.5);

	double e0 = quadric_eval(q, v0);
	double e1 = quadric_eval(q, v1);
	double em = quadric_eval(q, vm);

	if (isnan(e0) || isnan(e1) || isnan(em))
		return -1;

	if (e0 < e1) {
		if (em < e0) {
			*error = em;
			vec3_copy(vnew, vm);
			return 0;
		} else {
			*error = e0;
			vec3_copy(vnew, v0);
			return 0;
		}
	} else {
		if (em < e1) {
			*error = em;
			vec3_copy(vnew, vm);
			return 0;
		} else {
			*error = e1;
			vec3_copy(vnew, v1);
			return 0;
		}
	}
}

