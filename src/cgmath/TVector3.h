#pragma once
#include <cmath>
#include <cstddef>
#include <iostream>
#include <type_traits>

template <class TValue>
class TVector3
{
public:
	// Listes d'initialisation plutot qu'affectations dans le corps : sans elles ces
	// constructeurs `constexpr` sont purement decoratifs en C++17 -- affecter un
	// membre non initialise n'est autorise en evaluation constante que depuis
	// C++20, et le projet est en C++17 (CMakeLists.txt:5). Autrement dit
	// `constexpr Vector3f v {1,2,3};` ne compilait pas.
	constexpr TVector3 (TValue _x = 0, TValue _y = 0, TValue _z = 0) noexcept
		: x (_x), y (_y), z (_z) {}

	template <class S>
	constexpr TVector3 (const TVector3<S> &src) noexcept
		: x ((TValue)src.x), y ((TValue)src.y), z ((TValue)src.z) {}

	//
	// Operators
	//
	// No user-declared copy assignment on purpose: the rule of zero keeps
	// TVector3 trivially copyable (required to malloc/realloc/memcpy arrays of
	// Vector3) and restores the implicit move. Cross-type assignment still works
	// through the converting constructor above.

	inline constexpr TVector3<TValue> operator + (const TVector3<TValue> &right) const noexcept
	{
		return TVector3(x + (TValue)right.x, y + (TValue)right.y, z + (TValue)right.z);
	}

	inline constexpr TVector3<TValue> operator - (const TVector3<TValue> &right) const noexcept
	{
		return TVector3<TValue>(x - (TValue)right.x, y - (TValue)right.y, z - (TValue)right.z);
	}

	inline constexpr TVector3<TValue> operator * (const TValue s) const noexcept
	{
		return TVector3<TValue>(x*s, y*s, z*s);
	}

	// dot product
	template <class S>
	inline constexpr TValue operator * (const TVector3<S> &right) const noexcept
	{
		return (x*(TValue)right.x + y*(TValue)right.y + z*(TValue)right.z);
	}

	inline constexpr TVector3<TValue>  operator / (const TValue s) const noexcept
	{
		return s ? TVector3<TValue>(x/s, y/s, z/s) : TVector3<TValue>(0, 0, 0);
	}

	template <class S>
	inline constexpr TVector3<TValue> &operator += (const TVector3<S> &right) noexcept
	{
		x+=(TValue)right.x;
		y+=(TValue)right.y;
		z+=(TValue)right.z;
		return *this;
	}

	inline constexpr TVector3<TValue> &operator += (const TValue xyz) noexcept
	{
		x += xyz;
		y += xyz;
		z += xyz;
		return *this;
	}

	template <class S>
	inline constexpr TVector3<TValue> &operator -= (const TVector3<S> &right) noexcept
	{
		x-=(TValue)right.x;
		y-=(TValue)right.y;
		z-=(TValue)right.z;
		return *this;
	}

	inline constexpr TVector3<TValue> &operator -= (const TValue xyz) noexcept
	{
		x -= xyz;
		y -= xyz;
		z -= xyz;
		return *this;
	}

	inline constexpr TVector3<TValue> &operator *= (const TValue s) noexcept
	{
		x*=s;
		y*=s;
		z*=s;
		return *this;
	}

	inline constexpr TVector3<TValue> &operator /= (const TValue s) noexcept
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
	constexpr bool operator == (const TVector3<S> &right) const noexcept
	{ return (x == right.x && y == right.y && z == right.z); }

	template <class S>
	constexpr bool operator != (const TVector3<S> &right) const noexcept
	{ return !(x == right.x && y == right.y && z == right.z); }

	inline operator const TValue*() const noexcept { return &x; }
	inline operator TValue*() noexcept { return &x; }
	// Selection de membre, et non arithmetique de pointeur : `&x` ne designe qu'un
	// seul TValue, donc `((TValue*)&x)[i]` lisait au-dela de l'objet -- un acces
	// hors bornes que l'analyse statique signalait a CHACUN des sites d'appel.
	// Un indice hors [0,2] rabat desormais sur z (comportement defini) au lieu de
	// sortir de l'objet. Utilisable en evaluation constante, ce que ne permettrait
	// pas une union (lire un membre inactif n'est jamais une expression constante).
	inline constexpr const TValue operator[](int i) const noexcept
	{ return (i == 0) ? x : ((i == 1) ? y : z); }
	inline constexpr TValue &operator[](int i) noexcept
	{ return (i == 0) ? x : ((i == 1) ? y : z); }

	//
	// Set
	//

	inline constexpr void Set(const TValue _x, const TValue _y, const TValue _z) noexcept
	{ x = _x; y = _y; z = _z; }

	inline constexpr void Set(const TValue xyz) noexcept
	{ x = y = z = xyz; }

	template <class S>
	inline constexpr void Set(const TVector3<S> & t) noexcept
	{ x = t.x; y = t.y; z = t.z; }

	//
	// Utils
	//

	inline constexpr TVector3<TValue> &Clamp(TValue min, TValue max) noexcept
	{
		x = (x > max)? max : (x < min)? min : x;
		y = (y > max)? max : (y < min)? min : y;
		z = (z > max)? max : (z < min)? min : z;
		return *this;
	}

	inline TVector3<TValue> &Normalize() noexcept
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

	inline constexpr const TValue getLength2() const noexcept
	{
		return  x*x + y*y + z*z;
	}

	inline const TValue getLength() const noexcept
	{
		return std::sqrt(getLength2());
	}

	template <class S>
	inline constexpr const TValue DotProduct(const TVector3<S> &t) const noexcept
	{
		return x*t.x + y*t.y + z*t.z;
	}

	template <class S>
	static inline constexpr TValue DotProduct(const TVector3<S> &u, const TVector3<S> &v) noexcept
	{
		return u.x*v.x + u.y*v.y + u.z*v.z;
	}

	template <class S>
	inline constexpr TVector3<TValue> operator ^(const TVector3<S> &t) const noexcept
	{
		return TVector3<TValue>(
					y   * t.z  -  z   * t.y,
					t.x * z    -  t.z * x,
					x   * t.y  -  y   * t.x
					);
	}

	template <class S>
	inline constexpr TVector3<TValue> &operator ^=(const TVector3<S> &t) noexcept
	{
		Set(
			y   * t.z - z   * t.y,
			t.x * z   - t.z * x,
			x   * t.y - y   * t.x
			);
		return *this;
	}

	template <class S>
	inline constexpr void CrossProduct(const TVector3<S> &t1, const TVector3<S> &t2) noexcept
	{
		x = t1.y * t2.z - t1.z * t2.y;
		y = t1.z * t2.x - t1.x * t2.z;
		z = t1.x * t2.y - t1.y * t2.x;
	}

	template <class S>
	inline constexpr TVector3<TValue> CrossProduct(const TVector3<S> &p) const noexcept
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
	static inline constexpr TVector3<TValue> evaluate_triangle_normal (const TVector3<TValue> &v1,
								 const TVector3<TValue> &v2,
								 const TVector3<TValue> &v3) noexcept
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
						     const TVector3<TValue> &v3) noexcept
	{
		return (TValue)(0.5) * evaluate_triangle_normal(v1, v2, v3).getLength();
	}

	template <class S>
	inline const TValue getDistance(const TVector3<S> &v2)  const noexcept
	{
		return std::sqrt(
				(v2.x - x) * (v2.x - x) +
				(v2.y - y) * (v2.y - y) +
				(v2.z - z) * (v2.z - z)
				);
	}

	template <class S>
	inline const TValue getAngle(const TVector3<S> &v2)  const noexcept
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
	constexpr TVector3<TValue> &Barycenter ( const TVector3<S> &v1, const TVector3<S> &v2, const TVector3<S> &v3) noexcept
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

// Les conversions implicites vers TValue* publient `&x` comme base d'un tableau de
// 3 scalaires, et une soixantaine de sites en dependent : polygon2.h:43
// `(float*)m_contours[i].data()`, TMatrix4::SetLookAt, geometry.h:15
// point_triangle_distance2, tensor.h GetNormal(float*)... Cette promesse de
// disposition memoire n'est donc pas supprimable a court terme : on la VERIFIE ici
// au lieu de la supposer. Couvre aussi la trivial-copyability dont dependent les
// malloc/realloc/memcpy de tableaux de Vector3 documentes plus haut.
static_assert (sizeof (TVector3<float>)  == 3 * sizeof (float),  "TVector3<float> : padding interdit");
static_assert (sizeof (TVector3<double>) == 3 * sizeof (double), "TVector3<double> : padding interdit");
static_assert (offsetof (TVector3<float>, y) ==     sizeof (float), "TVector3<float> : y mal place");
static_assert (offsetof (TVector3<float>, z) == 2 * sizeof (float), "TVector3<float> : z mal place");
static_assert (std::is_standard_layout_v<TVector3<float>>,    "TVector3 : standard-layout requis");
static_assert (std::is_trivially_copyable_v<TVector3<float>>, "TVector3 : malloc/memcpy de tableaux");

typedef TVector3<int>		Vector3i;
typedef TVector3<float>		Vector3f;
typedef TVector3<double>	Vector3d;
typedef TVector3<float>		Vector3;
