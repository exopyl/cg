#ifndef __TVECTOR4_H__
#define __TVECTOR4_H__

#include <iostream>
using namespace std;

#include "TVector3.h"

template <class TValue>
class TVector4
{
public:
	TVector4<TValue>(TValue _x = 0, TValue _y = 0, TValue _z = 0, TValue _w = 0)
	{
		x = _x; y = _y; z = _z; w = _w;
	}

	TVector4<TValue>(TValue xyzw)
	{
		x = y = z = w = xyzw;
	}

	TVector4<TValue>(const TVector4<TValue> &src)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = src.w;
	}
 
	TVector4<TValue>(const TVector3<TValue> &src, const TValue _w = 1)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = _w;
	}

	TVector4<TValue>(TValue *mat)
	{
		x = mat[0];
		y = mat[1];
		z = mat[2];
		w = mat[3];
	}

	//
	// Operators
	//

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
		return (i==0 || i==1 || i==2 || i==3)? ((TValue*)&x)[i] : ((TValue*)&x)[0];
	}

	inline TValue &operator[](int i)
	{
		return (i==0 || i==1 || i==2 || i==3)? ((TValue*)&x)[i] : ((TValue*)&x)[0];
	}
 
	inline TVector4<TValue> &operator = (const TVector4<TValue> &src)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = src.w;
		return *this;
	}

	inline TVector4<TValue> &operator = (const TVector3<TValue> &src)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = 1.0f;
		return *this;
	}

	inline TVector4<TValue> operator + (const TVector4<TValue> &right)
	{
		return TVector4<TValue>(right.x + x, right.y + y, right.z + z, right.w + w );
	}

	inline TVector4<TValue> operator - (const TVector4<TValue>  &right)
	{
		return TVector4<TValue>(-right.x + x, -right.y + y, -right.z + z, -right.w + w );
	}

	inline TVector4<TValue> operator * (const TValue s)
	{
		return TVector4<TValue>(x*s, y*s, z*s, w*s);
	}

	inline TVector4<TValue> operator / (const TValue s)
	{
		return s ? TVector4<TValue>(x/s, y/s, z/s, w/s) : TVector4<TValue>(0, 0, 0, 0);
	}

	inline TVector4<TValue> &operator += (const TVector4<TValue> &right)
	{
		x += right.x;
		y += right.y;
		z += right.z;
		w += right.w;
		return *this;
	}

	inline TVector4<TValue> operator -= (const TVector4<TValue> &right)
	{
		x -= right.x;
		y -= right.y;
		z -= right.z;
		w -= right.w;
		return *this;
	}

	inline TVector4<TValue> clamp(const TValue min, const TValue max)
	{
		x = (x < min) ? min : (x > max) ? max : x;
		y = (y < min) ? min : (y > max) ? max : y;
		z = (z < min) ? min : (z > max) ? max : z;
		w = (w < min) ? min : (w > max) ? max : w;
		return *this;
	}

	inline TVector4<TValue> operator *= (const TValue s)
	{
		x *= s;
		y *= s;
		z *= s;
		w *= s;
		return *this;
	} 

	inline TVector4<TValue> operator /= (const TValue s)
	{
		if(s)
		{
			x /= s;
			y /= s;
			z /= s;
			w /= s;
		}
		return *this;
	}

	inline bool operator == (const TVector4<TValue> &right)
	{
		return (
			x == right.x &&
			y == right.y &&
			z == right.z &&
			w == right.w
			);
	}

	bool operator != (const TVector4<TValue> &right)
	{
		return !(
			x == right.x &&
			y == right.y &&
			z == right.z &&
			w == right.w
			);
	}

	//
	// Set
	//

	inline void Set(TValue xyzw)
	{
		x = y = z = w = xyzw;
	}

	inline void Set(TValue _x, TValue _y, TValue _z, TValue _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	inline void Set(const TVector4<TValue> &src)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = src.w;   
	}

	inline void Set(const TVector3<TValue>  &src, const TValue _w = 1)
	{
		x = src.x;
		y = src.y;
		z = src.z;
		w = _w;   
	}

	//
	// IOstream
	//

	friend ostream & operator<< ( ostream & out, const TVector4<TValue> &right)
	{
		return out << "( " << right.x << " , " << right.y << " , " << right.z << " , " << right.w << " )";
	}

	friend istream & operator >> (istream & in, TVector4<TValue> &right)
	{
		return in >> right.x >> right.y >> right.z >> right.w;
	}

public:

    TValue x, y, z, w;
};

typedef TVector4<int   > Vector4i;
typedef TVector4<float > Vector4f;
typedef TVector4<double> Vector4d;
typedef TVector4<float > Vector4;

#endif	// __TVECTOR4_H__
