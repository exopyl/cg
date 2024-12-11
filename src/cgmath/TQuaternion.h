#ifndef __QUATERNION_H__
#define __QUATERNION_H__

#include "common.h"
#include "TVector3.h"

#include <math.h>
#include <iostream>
using namespace std;

template <class TValue>
class TQuaternion
{
	friend class RotationImplTQuaternion;

 public:

	 //
	 // Constructors
	 //
	 
	 TQuaternion<TValue> ()
	 {
		 Set (0., 0., 0., 0.);
	 }

	 // Constructor : Initializes to (_i, _j, _k, _r)
	 TQuaternion<TValue> (TValue _i, TValue _j, TValue _k, TValue _r)
	 {
		 Set (_i, _j, _k, _r);
	 }

	 // Constructor : rotation matrix
	 TQuaternion<TValue> (TValue *m)
	 {
		TValue s;

		// trace
		TValue trace = 1 + m[0] + m[5] + m[10];
		if (trace > EPSILON)
		{
			s = 2.0 * sqrt (trace);
			x = (m[9] - m[6]) / s;
			y = (m[2] - m[8]) / s;
			z = (m[4] - m[1]) / s;
			w = 0.25 * s;
			return;
		}
		// what is the greatest value on the diagonal
		if (m[0] > m[5] && m[0] > m[10])
		{
			s = 2.0 * sqrt (1.0 + m[0] - m[5] - m[10]);
			x = 0.25 * s;
			y = (m[4] + m[1]) / s;
			z = (m[2] + m[8]) / s;
			w = (m[9] - m[6]) / s;
		}
		else if (m[5] > m[10])
		{
			s = 2.0 * sqrt (1.0 + m[5] - m[0] - m[10]);
			x = (m[4] + m[1]) / s;
			y = 0.25 * s;
			z = (m[9] + m[6]) / s;
			w = (m[2] - m[8]) / s;
		}
		else
		{
			s = 2.0 * sqrt (1.0 + m[10] - m[0] - m[5]);
			x = (m[2] + m[8]) / s;
			y = (m[9] + m[6]) / s;
			z = 0.25 * s;
			w = (m[4] - m[1]) / s;
		}
	 }

	 // Constructor : Initializes with the vector axis
	 // and the angle alpha (expressed in radian).
	 // It generates a rotation around this axis with
	 // the angle alpha.
	 TQuaternion<TValue> (TVector3<TValue> axis, TValue alpha)
	 {
		axis.Normalize ();
		TValue sina = (TValue)sin (alpha/2.0);
		TValue cosa = (TValue)cos (alpha/2.0);
		w = cosa;
		x = axis.x * sina;
		y = axis.y * sina;
		z = axis.z * sina;
	 }

	 TQuaternion<TValue> (const TQuaternion<TValue> &q)
	 {
		x = q.x;
		y = q.y;
		z = q.z;
		w = q.w;
	 }

	 // Initializes to (0,0,0,1), ie a zero degree rotation.
	void Set (const TValue _x, const TValue _y, const TValue _z, const TValue _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	//
	// Operations
	//

	inline TQuaternion<TValue> &operator=  (const TQuaternion<TValue> &q)
	{
		x = q.x;
		y = q.y;
		z = q.z;
		w = q.w;
		return *this;
	}

	inline TQuaternion<TValue>  operator+  (const TQuaternion<TValue> &q)
	{
		return TQuaternion (x+q.x, y+q.y, z+q.z, w+q.w);
	}

	inline TQuaternion<TValue>  operator+= (const TQuaternion<TValue> &q)
	{
		TQuaternion<TValue> res = *this + q;
		*this = res;
		return *this;
	}

	inline TQuaternion<TValue>  operator-  (const TQuaternion<TValue> &q)
	{
		return TQuaternion (x-q.x, y-q.y, z-q.z, w-q.w);
	}

	inline TQuaternion<TValue>  operator-= (const TQuaternion<TValue> &q)
	{
		TQuaternion<TValue> res = *this - q;
		*this = res;
		return *this;
	}

	inline TQuaternion<TValue>  operator*  (const TValue d)
	{
		return TQuaternion (d*x, d*y, d*z, d*w);
	}

	inline TQuaternion<TValue>  operator*  (const TQuaternion<TValue> &q)
	{
		TValue r1=w, r2=q.w;
		TValue u1=x, u2=q.x;
		TValue v1=y, v2=q.y;
		TValue w1=z, w2=q.z;

		return TQuaternion<TValue> (	r1*u2+r2*u1+v1*w2-w1*v2,
									r1*v2-u1*w2+v1*r2+w1*u2,
									r1*w2+u1*v2-u2*v1+r2*w1,
									r1*r2-u1*u2-v1*v2-w1*w2		);
	}

	inline TQuaternion<TValue>  operator*= (const TQuaternion<TValue> &q)
	{
		TQuaternion res = *this * q;
		*this = res;
		return *this;
	}

	inline TQuaternion<TValue>  operator/  (const TValue d)
	{
		return TQuaternion<TValue> (x/d, y/d, z/d, w/d);
	}

	bool operator== (const TQuaternion &s)
	{
		return ( x == s.x && y == s.y && z == s.z && w == s.w );
	}

	bool operator!= (const TQuaternion &q)
	{
		return !(*this == q);
	}

	inline operator const TValue*() const
	{
		return &x;
	}

	inline operator TValue*()
	{
		return &x;
	}   

	inline const TValue  operator[](int i) const
	{
		return ((TValue*)&x)[i];
	}

	inline TValue &operator[](int i)
	{
		return ((TValue*)&x)[i];
	}

	void Conjugate (void)
	{
		Set (-x, -y, -z, w);
	}

	TQuaternion<TValue> GetConjugate (void)
	{
		return TQuaternion (-x, -y, -z, w);
	}

	bool IsUnit (void)
	{
		return (Length() == 1.0)?true:false;
	}

	TValue Length (void)
	{
		return sqrt (w*w + x*x + y*y + z*z);
	}

	TQuaternion<TValue> Inverse (void)
	{
		Conjugate() / Length();
	}

	TQuaternion<TValue> GetInverse (void)
	{
		TQuaternion<TValue> res = GetConjugate();
		res = res / Length();
		return res;
	}

	void Normalize (void)
	{
		TValue invl = (TValue) 1.0 / Length(); 
		x *= invl;  y *= invl;  z *= invl; w *= invl;
	}

	void get_parameters (TValue *_x, TValue *_y, TValue *_z, TValue *_w)
	{
		*_x = x; *_y = y; *_z = z; *_w = w;
	}

	// Spherical Linear intERPolation
	// SLERP (p,q,t,theta) = ( p sin((1-t)theta)+q sin(t theta) ) / sin (theta)
	void slerp (TQuaternion<TValue>* p, TQuaternion<TValue>* q, TValue t)
	{
		//TQuaternion<TValue> *r; // result

		// Calculate angle beteen them. 
		TValue costheta = p->w * q->w + p->x * q->x + p->y * q->y + p->z * q->z; 
		TValue theta = acos(costheta); 
		
		// if theta = 0 then return q 
		if (fabs(theta) < 0.01)
		{
			Set (p->x, p->y, p->z, p->w);
			return;
		} 
		
		// Calculate temporary values. 
		TValue sinTheta = sqrt(1.0 - costheta*costheta); 
		
		// if theta*2 = 180 degrees then result is undefined 
		if (fabs(sinTheta) < 0.01)
		{ 
			Set (
				(p->z * 0.5 + q->z * 0.5),
				(p->x * 0.5 + q->x * 0.5), 
				(p->y * 0.5 + q->y * 0.5),
				(p->w * 0.5 + q->w * 0.5)
				);
			return;
		} 

		TValue ratioA = sin((1 - t) * theta) / sinTheta; 
		TValue ratioB = sin(t * theta) / sinTheta; 
		
		//calculate TQuaternion.
		Set (
			(p->z * ratioA + q->z * ratioB),
			(p->x * ratioA + q->x * ratioB), 
			(p->y * ratioA + q->y * ratioB), 
			(p->w * ratioA + q->w * ratioB)
			);
	}

	// Cubic interpolation
	//  SQUAD (q_i,a_i,a_{i+1},q_{i+1},t) = slerp ( slerp(q_i,q_{i+1},t) , slerp(a_i,a_{i+1},t) , 2t(1-t) )
	//
	// with :
	//                    /   ( ln (q_i^{-1}*q_{i+1}) + ln (q_i^{-1}*q_{i-1}) ) \
	//   a_i = q_i * exp (  - -------------------------------------------------  )
	//                    \                        4                            /
	//
	// sources :
	// - [shoe87] Shoemake, K., TQuaternion Calculus and Fast Animation, SIGGRAPH Course Notes, 10, 101-21, 1987. 
	// - http://www.theory.org/software/qfa/writeup/node12.html
	// - http://www.lrde.epita.fr/~theo/dload/cg1.pdf
	void squad (TQuaternion<TValue> *par_q1, TQuaternion<TValue> *par_a1, TQuaternion<TValue> *par_a2, TQuaternion<TValue> *par_q2, float par_t)
	{
		TQuaternion<TValue> tmp1, tmp2, tmp3;

		tmp1.slerp (par_q1, par_q2, par_t);
		tmp2.slerp (par_a1, par_a2, par_t);
		tmp3.slerp (&tmp1, &tmp2, 2*par_t*(1-par_t));

		Set (tmp3.x, tmp3.y, tmp3.z, tmp3.w);
	}

	// quaternion as a rotation
	// Apply a rotation, represented by the quaternion, to a vector.
	// \param dest : the result of the rotation.
	// \param orig : the initial coordinates of the vector.
	void rotate (TVector3<TValue> &dest, TVector3<TValue> orig)
	{
		TQuaternion q(orig.x, orig.y, orig.z, 0.0);
		TQuaternion quat_pt_rotated = *this * q * GetInverse();
		dest.Set (quat_pt_rotated.x, quat_pt_rotated.y, quat_pt_rotated.z);
	}

	// Get the rotation represented by the quaternion
	// into a matrix in the OpenGL format
	void get_matrix_rotation (float *m) // OpenGL format
	{
		m[0]  = 1 - 2 * (y*y + z*z);
		m[1]  =     2 * (x*y - z*w);
		m[2]  =     2 * (x*z + y*w);
		m[3]  = 0.0;

		m[4]  =     2 * (x*y + z*w);
		m[5]  = 1 - 2 * (x*x + z*z);
		m[6]  =     2 * (y*z - x*w);
		m[7]  = 0.0;

		m[8]  =     2 * (x*z - y*w);
		m[9]  =     2 * (y*z + x*w);
		m[10] = 1 - 2 * (x*x + y*y);
		m[11] = 0.0;

		m[12] = 0.0;
		m[13] = 0.0;
		m[14] = 0.0;
		m[15] = 1.0;
	}

	// Gets the axis and the angle of the rotation represented by the quaternion.
	void get_axis_angle (TVector3<TValue> axis, TValue *alpha)
	{
		Normalize ();
		TValue cosa = w;
		*alpha = (TValue)acos (cosa) * (TValue)2.0;
		TValue sina = (TValue)sqrt (1.0 - cosa*cosa);
		if (fabs (sina) < 0.0005) sina = 1.0;
		axis.Set (x / sina, y / sina,  z / sina);
	}

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TQuaternion<TValue> &right)
	{
		return out << "TQuaternion ( " << right.x << " , " << right.y << " , " << right.z << " , " << right.w <<" )";
	}

	friend istream & operator >> (istream & in, TQuaternion<TValue> &right)
	{
		return in >> right.x >> right.y >> right.z >> right.w;
	}

 public:
  TValue x, y, z, w;
};

typedef TQuaternion<float>		Quaternionf;
typedef TQuaternion<double>		Quaterniond;
typedef TQuaternion<float>		Quaternion;

#endif /* __QUATERNION_H__ */
