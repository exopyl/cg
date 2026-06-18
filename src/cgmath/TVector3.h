#pragma once
#include <cmath>
#include <iostream>

template <class TValue>
class TVector3
{
public:
	TVector3<TValue>(TValue _x = 0, TValue _y = 0, TValue _z = 0)
	{ x = _x; y = _y; z = _z; }

	template <class S>
	TVector3<TValue>(const TVector3<S> &src)
	{
		x = (TValue)src.x;
		y = (TValue)src.y;
		z = (TValue)src.z;
	}

	//
	// Operators
	//
	// No user-declared copy assignment on purpose: the rule of zero keeps
	// TVector3 trivially copyable (required to malloc/realloc/memcpy arrays of
	// Vector3) and restores the implicit move. Cross-type assignment still works
	// through the converting constructor above.

	inline TVector3<TValue> operator + (const TVector3<TValue> &right) const
	{
		return TVector3(x + (TValue)right.x, y + (TValue)right.y, z + (TValue)right.z);
	}

	inline TVector3<TValue> operator - (const TVector3<TValue> &right) const
	{
		return TVector3<TValue>(x - (TValue)right.x, y - (TValue)right.y, z - (TValue)right.z);
	}

	inline TVector3<TValue> operator * (const TValue s) const
	{
		return TVector3<TValue>(x*s, y*s, z*s);
	}

	// dot product
	template <class S>
	inline TValue operator * (const TVector3<S> &right) const
	{
		return (x*(TValue)right.x + y*(TValue)right.y + z*(TValue)right.z);
	}

	inline TVector3<TValue>  operator / (const TValue s) const
	{
		return s ? TVector3<TValue>(x/s, y/s, z/s) : TVector3<TValue>(0, 0, 0);
	}

	template <class S>
	inline TVector3<TValue> &operator += (const TVector3<S> &right)
	{
		x+=(TValue)right.x;
		y+=(TValue)right.y;
		z+=(TValue)right.z;
		return *this;
	}

	inline TVector3<TValue> &operator += (const TValue xyz)
	{
		x += xyz;
		y += xyz;
		z += xyz;
		return *this;
	}

	template <class S>
	inline TVector3<TValue> &operator -= (const TVector3<S> &right)
	{
		x-=(TValue)right.x;
		y-=(TValue)right.y;
		z-=(TValue)right.z;
		return *this;
	}

	inline TVector3<TValue> &operator -= (const TValue xyz)
	{
		x -= xyz;
		y -= xyz;
		z -= xyz;
		return *this;
	}

	inline TVector3<TValue> &operator *= (const TValue s) 
	{
		x*=s;
		y*=s;
		z*=s;
		return *this;
	}

	inline TVector3<TValue> &operator /= (const TValue s)
	{
		if (s) 
		{
			TValue t = 1./s;
			x*=t;
			y*=t;
			z*=t;
		}
		return *this;
	}

	template <class S>
	bool operator == (const TVector3<S> &right) const
	{ return (x == right.x && y == right.y && z == right.z); }

	template <class S>
	bool operator != (const TVector3<S> &right) const
	{ return !(x == right.x && y == right.y && z == right.z); }

	inline operator const TValue*() const { return &x; }
	inline operator TValue*() { return &x; }
	inline const TValue operator[](int i) const { return ((TValue*)&x)[i]; }
	inline TValue &operator[](int i) { return ((TValue*)&x)[i]; }

	//
	// Set
	//

	inline void Set(const TValue _x, const TValue _y, const TValue _z)
	{ x = _x; y = _y; z = _z; }

	inline void Set(const TValue xyz)
	{ x = y = z = xyz; }

	template <class S>
	inline void Set(const TVector3<S> & t)
	{ x = t.x; y = t.y; z = t.z; }

	//
	// Utils
	//

	inline TVector3<TValue> &Clamp(TValue min, TValue max)
	{
		x = (x > max)? max : (x < min)? min : x;
		y = (y > max)? max : (y < min)? min : y;
		z = (z > max)? max : (z < min)? min : z;
		return *this;
	}

	inline TVector3<TValue> &Normalize()
	{
		TValue l = getLength();

		if(!l)
		{
			Set(0,0,0);
		}
		else
		{
			TValue t = (TValue)(1.)/l;
			x*=t;
			y*=t;
			z*=t;
		}

		return *this;
	}

	inline const TValue getLength2() const
	{
		return  x*x + y*y + z*z;
	}

	inline const TValue getLength() const
	{
		return std::sqrt(getLength2());
	}

	template <class S>
	inline const TValue DotProduct(const TVector3<S> &t) const
	{
		return x*t.x + y*t.y + z*t.z;
	}

	template <class S>
	static inline TValue DotProduct(const TVector3<S> &u, const TVector3<S> &v)
	{
		return u.x*v.x + u.y*v.y + u.z*v.z;
	}

	template <class S>
	inline TVector3<TValue> operator ^(const TVector3<S> &t) const
	{
		return TVector3<TValue>(
					y   * t.z  -  z   * t.y,
					t.x * z    -  t.z * x,
					x   * t.y  -  y   * t.x
					);
	}  

	template <class S>
	inline TVector3<TValue> &operator ^=(const TVector3<S> &t)
	{
		Set(
			y   * t.z - z   * t.y,
			t.x * z   - t.z * x,
			x   * t.y - y   * t.x
			);
		return *this;
	}

	template <class S>
	inline void CrossProduct(const TVector3<S> &t1, const TVector3<S> &t2)
	{
		x = t1.y * t2.z - t1.z * t2.y;
		y = t1.z * t2.x - t1.x * t2.z;
		z = t1.x * t2.y - t1.y * t2.x;
	}

	template <class S>
	inline TVector3<TValue> CrossProduct(const TVector3<S> &p) const
	{
		const TValue val1 = y * p.z - z * p.y;
		const TValue val2 = p.x * z - p.z * x;
		const TValue val3 = x * p.y - y * p.x;
		return TVector3<TValue>( val1, val2, val3 );
	}

	//
	// Triangle helpers (replacements for vec3_triangle_normal / vec3_triangle_area)
	//
	// Unnormalized normal of the triangle (v1, v2, v3): n = (v2 - v1) x (v3 - v1).
	// Differences and cross product are accumulated in double then cast back, to
	// reproduce vec3_triangle_normal bit-for-bit on TVector3<float>.
	static inline TVector3<TValue> evaluate_triangle_normal (const TVector3<TValue> &v1,
								 const TVector3<TValue> &v2,
								 const TVector3<TValue> &v3)
	{
		const double ux = (double)v2.x - (double)v1.x;
		const double uy = (double)v2.y - (double)v1.y;
		const double uz = (double)v2.z - (double)v1.z;

		const double vx = (double)v3.x - (double)v1.x;
		const double vy = (double)v3.y - (double)v1.y;
		const double vz = (double)v3.z - (double)v1.z;

		return TVector3<TValue>( (TValue)(uy * vz - uz * vy),
					 (TValue)(uz * vx - ux * vz),
					 (TValue)(ux * vy - uy * vx) );
	}

	// Area of the triangle (v1, v2, v3) = 0.5 * length of its unnormalized normal.
	static inline TValue evaluate_triangle_area (const TVector3<TValue> &v1,
						     const TVector3<TValue> &v2,
						     const TVector3<TValue> &v3)
	{
		return (TValue)(0.5) * evaluate_triangle_normal(v1, v2, v3).getLength();
	}

	template <class S>
	inline const TValue getDistance(const TVector3<S> &v2)  const
	{
		return std::sqrt(
				(v2.x - x) * (v2.x - x) +
				(v2.y - y) * (v2.y - y) +
				(v2.z - z) * (v2.z - z)
				);
	}

	template <class S>
	inline const TValue getAngle(const TVector3<S> &v2)  const
	{
		const TValue denom = getLength() * v2.getLength();
		if (denom == (TValue)0) return (TValue)0;
		// Clamp to [-1,1]: rounding can push the ratio slightly out of acos' domain.
		TValue c = DotProduct(v2) / denom;
		if (c < (TValue)-1) c = (TValue)-1;
		else if (c > (TValue)1) c = (TValue)1;
		return std::acos(c);
	}

	template <class S>
	TVector3<TValue> &Barycenter ( TVector3<S> v1, TVector3<S> v2, TVector3<S> v3)
	{
		x = (v1.x + v2.x + v3.x) / 3.0;
		y = (v1.y + v2.y + v3.y) / 3.0;
		z = (v1.z + v2.z + v3.z) / 3.0;
		return *this;
	}

	//
	// IOstream
	//

	friend std::ostream & operator << ( std::ostream & out, const TVector3<TValue> &right)
	{
		return out << "( " << right.x << " , " << right.y << " , " << right.z <<" )";
	}

	friend std::istream & operator >> (std::istream & in, TVector3<TValue> &right)
	{
		return in >> right.x >> right.y >> right.z;
	}

public:

    TValue x, y, z;
};

typedef TVector3<int>		Vector3i;
typedef TVector3<float>		Vector3f;
typedef TVector3<double>	Vector3d;
typedef TVector3<float>		Vector3;
