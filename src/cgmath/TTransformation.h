#ifndef __TTRANSFORMATION_H__
#define __TTRANSFORMATION_H__

#include "TVector4.h"

#include <iostream>
using namespace std;

// TMatrix4[n][m] addresses the following element :
// n = row
// m = column
// ie
// m_Mat[0][0] m_Mat[0][1] m_Mat[0][2] m_Mat[0][3]
// m_Mat[1][0] m_Mat[1][1] m_Mat[1][2] m_Mat[1][3]
// m_Mat[2][0] m_Mat[2][1] m_Mat[2][2] m_Mat[2][3]
// m_Mat[3][0] m_Mat[3][1] m_Mat[3][2] m_Mat[3][3]

template <class TValue>
class TTransformation 
{
public:

	TTransformation<TValue>()
	{
		m_Mat.SetIdentity();
	}

	//
	// Translation
	//

	inline void SetTranslation(TValue x, TValue y, TValue z)
	{
		SetIdentity();
		m_Mat[0][3] = x; m_Mat[1][3] = y; m_Mat[2][3] = z;
	}
	

	inline void SetTranslation(TVector3<TValue> vTrans)
	{
		SetTranslation( vTrans.x, vTrans.y, vTrans.z );
	}

	inline void SetTranslation_X( TValue fX )
	{
		SetIdentity();
		m_Mat[0][3] = fX;
	}

	inline void SetTranslation_Y( TValue fY )
	{
		SetIdentity();
		m_Mat[1][3] = fY;
	}

	inline void SetTranslation_Z( TValue fZ )
	{
		SetIdentity();
		m_Mat[2][3] = fZ;
	}

	//
	// ROTATION
	//

	/*! Create a rotation matrice
	\param angle - Angle value for the rotation
	\param x - X value rotation vector
	\param y - Y Value rotation vector
	\param z - Z Value rotation vector
	*/
	inline void SetRotation( TValue angle, TValue x, TValue y, TValue z )
	{
		FLOAT32 length;
		FLOAT32 c,s,t;
		FLOAT32 theta = RS_DEGTORAD(angle);
		
		// Normalize
		length = sqrtf(x*x + y*y + z*z);
		
		// Too close to 0, can't make a normalized vector
		if (length < 0.000001f)
			return;
		
		x /= length; y /= length; z /= length;
		
		// Do the trig
		c = cosf(theta);
		s = sinf(theta);
		t = 1-c;   
		
		// Build the rotation matrix
		m_Mat[0][0] = t*x*x + c;   m_Mat[1][0] = t*x*y + s*z; m_Mat[2][0] = t*x*z - s*y; m_Mat[3][0] = 0;
		m_Mat[0][1] = t*x*y - s*z; m_Mat[1][1] = t*y*y + c;   m_Mat[2][1] = t*y*z + s*x; m_Mat[3][1] = 0;
		m_Mat[0][2] = t*x*z + s*y; m_Mat[1][2] = t*y*z - s*x; m_Mat[2][2] = t*z*z + c;   m_Mat[3][2] = 0;
		m_Mat[0][3] = 0;           m_Mat[1][3] = 0;           m_Mat[2][3] = 0;           m_Mat[3][3] = 1;
	}

	/*! Create a rotation matrice
	\param angle - Angle value for the rotation
	\param x - X value rotation vector
	\param y - Y Value rotation vector
	\param z - Z Value rotation vector
	*/
	void SetRotation( TValue angle, TVector3<TValue> vRot )
	{
		SetRotation( angle, vRot.x, vRot.y, vRot.z );
	}

	/*! Create a rotation matrice around a single axis
	\param angle - Angle value for the rotation around X Axis
	*/
	void SetRotation_X( TValue angle )
	{
		TValue s = (TValue)sin(RS_DEGTORAD(angle));
		TValue c = (TValue)cos(RS_DEGTORAD(angle));

		SetIdentity();

		m_Mat[1][1] =  c;   m_Mat[2][1] =  s;
		m_Mat[1][2] = -s;   m_Mat[2][2] =  c;
	}

	/*! Create a rotation matrice around a single axis
	\param angle - Angle value for the rotation around Y Axis
	*/
	void SetRotation_Y( TValue angle )
	{
		TValue s = (TValue)sin(RS_DEGTORAD(angle));
		TValue c = (TValue)cos(RS_DEGTORAD(angle));

		SetIdentity();

		m_Mat[0][0] =  c;   m_Mat[2][0] = -s;
		m_Mat[0][2] =  s;   m_Mat[2][2] =  c;
	}

	/*! Create a rotation matrice around a single axis
	\param angle - Angle value for the rotation around Z Axis
	*/
	void SetRotation_Z( TValue angle )
	{
		TValue s = (TValue)sin(RS_DEGTORAD(angle));
		TValue c = (TValue)cos(RS_DEGTORAD(angle));

		SetIdentity();

		m_Mat[0][0] =  c;   m_Mat[1][0] =  s;
		m_Mat[0][1] = -s;   m_Mat[1][1] =  c;
	}
	//
	// ROTATION - END
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// TRANSFORMATION
	//
	/*! Set the values for scaling
	\
	*/
	inline void SetScale( TValue x, TValue y, TValue z )
	{
		m_Mat[0][0] = x;
		m_Mat[1][1] = y;
		m_Mat[2][2] = z;
	}

	/*! Transform a point or a vector using the matrix (Rotation scale and translation)
	\param vec - The point/vector to transform
	*/
	inline void TransformPoint( TVector3<TValue> *vec )
	{
		TValue x = vec->x;
		TValue y = vec->y;
		TValue z = vec->z;

		//Since it's a point we are computing using the translation
		vec->x = x * m_Mat[0][0] + y * m_Mat[0][1] + z * m_Mat[0][2] + m_Mat[0][3];
		vec->y = x * m_Mat[1][0] + y * m_Mat[1][1] + z * m_Mat[1][2] + m_Mat[1][3];
		vec->z = x * m_Mat[2][0] + y * m_Mat[2][1] + z * m_Mat[2][2] + m_Mat[2][3];
	}

	/*! Transform a vector using the matrix (Rotation scale only)
	\param vec - The vector to transform
	*/
	inline void TransformVector( TVector3<TValue> *vec )
	{
		TValue x = vec->x;
		TValue y = vec->y;
		TValue z = vec->z;

		//No translation, only rotation
		vec->x = x * m_Mat[0][0] + y * m_Mat[0][1] + z * m_Mat[0][2];
		vec->y = x * m_Mat[1][0] + y * m_Mat[1][1] + z * m_Mat[1][2];
		vec->z = x * m_Mat[2][0] + y * m_Mat[2][1] + z * m_Mat[2][2];
	}

public:
	TMatrix4<TValue> m_Mat;
};

typedef TTransformation<float>  Transformationf;
typedef TTransformation<double> Transformationd;
typedef TTransformation<float>  Transformation;

#endif // __TTRANSFORMATION_H__
