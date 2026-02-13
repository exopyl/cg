#include "algebra_quaternion.h"

void quaternion_init (quaternion q, float w, float x, float y, float z)
{
	if (q == NULL)
		return;

	q[0] = x;
	q[1] = y;
	q[2] = z;
	q[3] = w;
}

void quaternion_init_axis_angle (quaternion q, vec3 axis, float angle)
{
	if (q == NULL)
		return;

	float sin_a = sin (angle / 2.);
	float cos_a = cos (angle / 2.);

	vec3_normalize (axis);
	q[0] = axis[0] * sin_a;
	q[1] = axis[1] * sin_a;
	q[2] = axis[2] * sin_a;
	q[3] = cos_a;
}


int quaternion_init_matrix3 (quaternion q, mat3 m)
{
	float trace = 1.0 + m[0][0] + m[1][1] + m[2][2];
	if (trace > 0.)
	{
		float s = 0.5/sqrt(trace);
		q[3] = 0.25 / s;
		q[0] = (m[2][1] - m[1][2]) * s;
		q[1] = (m[0][2] - m[2][0]) * s;
		q[2] = (m[1][0] - m[0][1]) * s;
	}
	else
	{
		if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) // m[0][0]
		{
			float s = sqrt( 1.0 + m[0][0] - m[1][1] - m[2][2] ) * 2.;
			
			q[0] = 0.5 / s;
			q[1] = (m[0][1] + m[1][0] ) / s;
			q[2] = (m[0][2] + m[2][0] ) / s;
			q[3] = (m[1][2] + m[2][1] ) / s;
		}
		else if (m[1][1] > m[2][2]) // m[1][1]
		{
			float s = sqrt( 1.0 + m[1][1] - m[0][0] - m[2][2] ) * 2.;
			
			q[0] = (m[0][1] + m[1][0] ) / s;
			q[1] = 0.5 / s;
			q[2] = (m[1][2] + m[2][1] ) / s;
			q[3] = (m[0][2] + m[2][0] ) / s;
		}
		else // m[2][2]
		{
			float s = sqrt( 1.0 + m[2][2] - m[0][0] - m[1][1] ) * 2.;
			
			q[0] = (m[0][2] + m[2][0] ) / s;
			q[1] = (m[1][2] + m[2][1] ) / s;
			q[2] = 0.5 / s;
			q[3] = (m[0][1] + m[1][0] ) / s;
		}
	}
	
	quaternion_normalize (q);
	return 1;
}

//
// Spherical Linear intERPolation
// SLERP (p,q,t,theta) = ( p sin((1-t)theta)+q sin(t theta) ) / sin (theta)
// Reference : http://en.wikipedia.org/wiki/Slerp
//
int quaternion_init_slerp (quaternion res, quaternion p, quaternion q, float t)
{
	// clamp the t parameter
	if (t < 0.) t = 0.;
	if (t > 1.) t = 1.;
	
	// calculate the angle between the two rotations
	float costheta = p[3] * q[3] + p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
	if (costheta >= 1.)
		costheta = 1.;
	if (costheta < -1.)
		costheta = -1.;
	float theta = acos(costheta);
	
	// if theta = 0 then return q
	if (fabs(theta) < 0.00001)
	{
		quaternion_init (res, p[3], p[0], p[1], p[2]);
		return 1;
	}
	
	// calculate temporary values.
	float sinTheta = sqrt(1.0 - costheta*costheta);

	// if theta*2 = 180 degrees then result is undefined
	if (fabs(sinTheta) < 0.0001)
	{
		quaternion_init (res,
				 (p[3] * 0.5 + q[3] * 0.5),
				 (p[0] * 0.5 + q[0] * 0.5),
				 (p[1] * 0.5 + q[1] * 0.5),
				 (p[2] * 0.5 + q[2] * 0.5) );
		return 1;
	}
	float ratioA = sin((1 - t) * theta) / sinTheta;
	float ratioB = sin(t * theta) / sinTheta;
	
	// calculate the interpolated rotation
	quaternion_init (res,
			 (p[3] * ratioA + q[3] * ratioB),
			 (p[0] * ratioA + q[0] * ratioB),
			 (p[1] * ratioA + q[1] * ratioB),
			 (p[2] * ratioA + q[2] * ratioB) );
	return 1;
}

void quaternion_copy (quaternion copy, quaternion source)
{
	if (!copy || !source)
		return;

	copy[3] = source[3];
	copy[0] = source[0];
	copy[1] = source[1];
	copy[2] = source[2];
}

float quaternion_norm (quaternion q)
{
	if (q == NULL)
		return -1.;

	return sqrt(q[0]*q[0] + q[1] *q[1] + q[2]*q[2] + q[3]*q[3]);
}

int quaternion_normalize (quaternion q)
{
	if (q == NULL)
		return -1;

	float norm = quaternion_norm (q);
	if (norm == 0.)
		return 0;

	q[0] /= norm;
	q[1] /= norm;
	q[2] /= norm;
	q[3] /= norm;

	return 1;
}

void quaternion_convert_to_matrix3 (quaternion q, mat3 M)
{
        float n, s;
        float xs, ys, zs;
        float wx, wy, wz;
        float xx, xy, xz;
        float yy, yz, zz;

	// compute rotation matrix
        n = (q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]);
        s = (n > 0.0f) ? (2.0f / n) : 0.0f;

        xs = q[0] * s;
        ys = q[1] * s;
        zs = q[2] * s;
        wx = q[3] * xs;
        wy = q[3] * ys;
        wz = q[3] * zs;
        xx = q[0] * xs;
        xy = q[0] * ys;
        xz = q[0] * zs;
        yy = q[1] * ys;
        yz = q[1] * zs;
        zz = q[2] * zs;
	
        M[0][0] = 1.0f - (yy + zz);
        M[1][0] = xy + wz;
        M[2][0] = xz - wy;
	       
        M[0][1] = xy - wz;
        M[1][1] = 1.0f - (xx + zz);
        M[2][1] = yz + wx;
	       
        M[0][2] = xz + wy;
        M[1][2] = yz - wx;
        M[2][2] = 1.0f - (xx + yy);
}

void quaternion_dump (quaternion q)
{
	printf ("quaternion : %f %f %f %f", q[0], q[1], q[2], q[3]);
}

int  quaternion_rotate (quaternion q, vec3 res, vec3 orig)
{
	// q is normalized
	auto qx = q[0];
	auto qy = q[1];
	auto qz = q[2];
	auto qw = q[3];
	auto x = orig[0];
	auto y = orig[1];
	auto z = orig[2];

	float uvx = qy * z - qz * y;
	float uvy = qz * x - qx * z;
	float uvz = qx * y - qy * x;

	float uuvx = qy * uvz - qz * uvy;
	float uuvy = qz * uvx - qx * uvz;
	float uuvz = qx * uvy - qy * uvx;

	uvx *= (2.0f * qw);
	uvy *= (2.0f * qw);
	uvz *= (2.0f * qw);

	uuvx *= 2.0f;
	uuvy *= 2.0f;
	uuvz *= 2.0f;

	res[0] = x + uvx + uuvx;
	res[1] = y + uvy + uuvy;
	res[2] = z + uvz + uuvz;

	return 0;
}

