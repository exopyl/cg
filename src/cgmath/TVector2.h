#pragma once
#include <cmath>
#include <cstddef>
#include <iostream>
#include <type_traits>

template <class TValue>
class TVector2
{
public:
	TVector2(TValue _x = (TValue)0, TValue _y = (TValue)0)
	{ x = _x; y = _y; }

	template <class S>
	TVector2(const TVector2<S> &src)
	{ x = src.x; y = src.y; }

	~TVector2() {};

	//
	// Operators
	//
	template <class S>
	inline TVector2<TValue> &operator= (const TVector2<S>& right)
	{
		x = (TValue)right.x;
		y = (TValue)right.y;
		return *this;
	}

	template <class S>
	inline TVector2<TValue> operator+ (const TVector2<S> &right) const
	{
		return TVector2(x + (TValue)right.x, y + (TValue)right.y);
	}

	template <class S>
	inline TVector2<TValue> operator- (const TVector2<S> &right) const
	{
		return TVector2(x - (TValue)right.x, y - (TValue)right.y);
	}

	inline TVector2<TValue> operator * (const TValue s)
	{
		return TVector2<TValue>(x*s, y*s);
	}

	template <class S>
	inline TValue operator * (const TVector2<S> &right)
	{
		return TValue(x*right.x + y*right.y);
	}

	inline TVector2<TValue>  operator / (const TValue s)
	{
		return (s)? TVector2<TValue>(x/s,y/s) : TVector2<TValue>(0,0);
	}

	template <class S>
	inline TVector2<TValue> &operator += (const TVector2<S> &right)
	{
		x+=(TValue)right.x;
		y+=(TValue)right.y;
		return *this;
	}

	template <class S>
	inline TVector2<TValue> &operator -= (const TVector2<S> &right)
	{
		x-=(TValue)right.x;
		y-=(TValue)right.y;
		return *this;
	}

	inline TVector2<TValue> &operator *= (const TValue s)
	{
		x*=s;
		y*=s;
		return *this;
	}

	inline TVector2<TValue> &operator /= (const TValue s)
	{
		if(s)
		{
			x/=s;
			y/=s;
		}
		return *this;
	}

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
	// equivalent a l'ancien code POUR TOUT i -- le rabattement sur x hors [0,1]
	// existait deja -- simplement sans le cast de pointeur.
	inline constexpr const TValue operator[](int i) const
	{
		return (i == 1) ? y : x;
	}

	inline constexpr TValue &operator[](int i)
	{
		return (i == 1) ? y : x;
	}

	template <class S>
	bool operator == (const TVector2<S> &right)
	{
		return (x == right.x && y == right.y);
	}

	template <class S>
	bool operator != (const TVector2<S> &right)
	{
		return !(x == right.x && y == right.y );
	}

	//
	// Set
	//

	void Set(TValue _x, TValue _y)
	{
		x = _x;
		y = _y;
	}

	//
	// Utils
	//

	inline void Clamp(TValue min, TValue max)
	{
		x = (x > max)? max : (x < min)? min : x;
		y = (y > max)? max : (y < min)? min : y;
	}

	inline TVector2<TValue> &Normalize()
	{
		TValue length = getLength();

		if(!length)
		{
			Set(0,0);
		}
		else
		{
			x/=length;
			y/=length;
		}

		return *this;
	}

	inline const TValue getLength2() const
	{
		return  x*x + y*y;
	}

	inline const TValue getLength() const
	{
		return std::sqrt(getLength2());
	}

	template <class S>
	inline const TValue DotProduct(const TVector2<S> &t) const
	{
		return x*t.x + y*t.y;
	}

	template <class S>
	inline const TValue getDistance(const TVector2<S> &v2)  const 
	{
		return std::sqrt(
				(v2.x - x) * (v2.x - x) +
				(v2.y - y) * (v2.y - y)
				);
	}
	
	template <class S>
	inline const TValue getAngle(const TVector2<S> &v2)  const
	{
		return std::acos( DotProduct(v2) / (getLength() * v2.getLength()));
	}

	// Z component of the 2D cross product (scalar). Positive when this->v2 is CCW.
	template <class S>
	inline const TValue CrossProduct(const TVector2<S> &v2) const
	{
		return x * (TValue)v2.y - y * (TValue)v2.x;
	}

	// Rotation by +90 degrees: (x, y) -> (-y, x)
	inline TVector2<TValue> Perp() const
	{
		return TVector2<TValue>(-y, x);
	}

	// Linear interpolation between a and b at t in [0, 1]
	static inline TVector2<TValue> Lerp(const TVector2<TValue> &a,
	                                     const TVector2<TValue> &b,
	                                     TValue t)
	{
		return TVector2<TValue>(a.x + t * (b.x - a.x),
		                        a.y + t * (b.y - a.y));
	}

	// Static distance between two points
	template <class S>
	static inline TValue Distance(const TVector2<TValue> &a,
	                               const TVector2<S> &b)
	{
		TValue dx = (TValue)b.x - a.x;
		TValue dy = (TValue)b.y - a.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	// Signed angle from `from` to `to` via atan2, in [-pi, pi].
	// Positive when `to` is CCW relative to `from`.
	template <class S>
	static inline TValue SignedAngle(const TVector2<TValue> &from,
	                                  const TVector2<S> &to)
	{
		TValue tox = (TValue)to.x;
		TValue toy = (TValue)to.y;
		return std::atan2(from.x * toy - from.y * tox,
		                  from.x * tox + from.y * toy);
	}

	//
	// IOstream
	//

	friend std::ostream & operator << ( std::ostream & out, const TVector2<TValue> &right)
	{
		return out << "( " << right.x << " , " << right.y <<" )";
	}

	friend std::istream & operator >> (std::istream & in, TVector2<TValue> &right)
	{
		return in >> right.x >> right.y;
	}

public:

	TValue x, y;
};

// Meme promesse de disposition memoire que TVector3 : les conversions implicites
// vers TValue* publient `&x` comme base d'un tableau de 2 scalaires, et polygon2
// s'en sert massivement (`(float*)m_contours[i].data()`). Verifiee, pas supposee.
// NB : `is_trivially_copyable` n'est PAS asserti ici -- le destructeur vide
// `~TVector2() {}` declare plus haut l'empeche (et supprime le move implicite).
// Le retirer restaurerait la regle de zero, comme documente dans TVector3.h.
static_assert (sizeof (TVector2<float>)  == 2 * sizeof (float),  "TVector2<float> : padding interdit");
static_assert (sizeof (TVector2<double>) == 2 * sizeof (double), "TVector2<double> : padding interdit");
static_assert (offsetof (TVector2<float>, y) == sizeof (float),   "TVector2<float> : y mal place");
static_assert (std::is_standard_layout_v<TVector2<float>>,       "TVector2 : standard-layout requis");

typedef TVector2<int> Vector2i;
typedef TVector2<float> Vector2f;
typedef TVector2<double> Vector2d;
typedef TVector2<float> Vector2;
