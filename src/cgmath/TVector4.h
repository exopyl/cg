#pragma once
#include <cstddef>
#include <iostream>
#include <type_traits>

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

	// Selection de membre, et non arithmetique de pointeur : `&x` ne designe qu'un
	// seul TValue, donc `((TValue*)&x)[i]` lisait au-dela de l'objet. Strictement
	// equivalent a l'ancien code POUR TOUT i -- le rabattement sur x hors [0,3]
	// existait deja. Meme defaut que TVector2/TVector3, non signale par l'analyse
	// statique faute de site d'appel instrumente : corrige par coherence.
	inline constexpr const TValue  operator[](int i) const
	{
		return (i == 1) ? y : ((i == 2) ? z : ((i == 3) ? w : x));
	}

	inline constexpr TValue &operator[](int i)
	{
		return (i == 1) ? y : ((i == 2) ? z : ((i == 3) ? w : x));
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

	inline TVector4<TValue> Clamp(const TValue min, const TValue max)
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

	friend std::ostream & operator<< ( std::ostream & out, const TVector4<TValue> &right)
	{
		return out << "( " << right.x << " , " << right.y << " , " << right.z << " , " << right.w << " )";
	}

	friend std::istream & operator >> (std::istream & in, TVector4<TValue> &right)
	{
		return in >> right.x >> right.y >> right.z >> right.w;
	}

public:

    TValue x, y, z, w;
};

// Meme promesse de disposition memoire que TVector3 : `&x` publie comme base d'un
// tableau de 4 scalaires par les conversions implicites (TMatrix4, code OpenGL).
// `is_trivially_copyable` n'est pas asserti : le constructeur de copie et
// l'operateur d'affectation explicites ci-dessus l'empechent (regle de zero non
// respectee), sans consequence tant qu'aucun tableau de TVector4 n'est memcpy.
static_assert (sizeof (TVector4<float>)  == 4 * sizeof (float),  "TVector4<float> : padding interdit");
static_assert (sizeof (TVector4<double>) == 4 * sizeof (double), "TVector4<double> : padding interdit");
static_assert (offsetof (TVector4<float>, w) == 3 * sizeof (float), "TVector4<float> : w mal place");
static_assert (std::is_standard_layout_v<TVector4<float>>,        "TVector4 : standard-layout requis");

typedef TVector4<int   > Vector4i;
typedef TVector4<float > Vector4f;
typedef TVector4<double> Vector4d;
typedef TVector4<float > Vector4;
