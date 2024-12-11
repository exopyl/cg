#ifndef __TVECTOR2_H__
#define __TVECTOR2_H__

#include <iostream>
using namespace std;

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

	inline const TValue operator[](int i) const
	{
		return (i==0 || i==1)? ((TValue*)&x)[i] : ((TValue*)&x)[0];
	}

	inline TValue &operator[](int i)
	{
		return (i==0 || i==1)? ((TValue*)&x)[i] : ((TValue*)&x)[0];
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
		return sqrt(getLength2());
	}

	template <class S>
	inline const TValue DotProduct(const TVector2<S> &t) const
	{
		return x*t.x + y*t.y;
	}

	template <class S>
	inline const TValue getDistance(const TVector2<S> &v2)  const 
	{
		return sqrt(
				(v2.x - x) * (v2.x - x) +
				(v2.y - y) * (v2.y - y)
				);
	}
	
	template <class S>
	inline const TValue getAngle(const TVector2<S> &v2)  const 
	{             
		return acos( DotProduct(v2) / (getLength() * v2.getLength()));
	}

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TVector2<TValue> &right)
	{
		return out << "( " << right.x << " , " << right.y <<" )";
	}

	friend istream & operator >> (istream & in, TVector2<TValue> &right)
	{
		return in >> right.x >> right.y;
	}

public:

	TValue x, y;
};

typedef TVector2<int> Vector2i;
typedef TVector2<float> Vector2f;
typedef TVector2<double> Vector2d;
typedef TVector2<float> Vector2;

#endif	// __TVECTOR2_H__
