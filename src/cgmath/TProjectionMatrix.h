#ifndef __T_PROJECTION_MATRIX4_H__
#define __T_PROJECTION_MATRIX4_H__

#include "TMatrix4.h"
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
class TProjectionMatrix4 
{
public:

	//
	// Constructors
	//

	TProjectionMatrix4<TValue>()
	{
		m_Mat.SetIdentity ();
	}

	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// 3D SPECIFIC MATRIX BUILDING
	//
	/*! Builds a right-handed perspective projection matrix.
	*/
	inline void BuildProjectionMatrixPerspectiveRH(
									TValue widthOfViewVolume,
									TValue heightOfViewVolume,
									TValue zNear,
									TValue zFar)
	{
		m_Mat[0][0] = 2*zNear/widthOfViewVolume;
		m_Mat[1][0] = 0;
		m_Mat[2][0] = 0;
		m_Mat[3][0] = 0;

		m_Mat[0][1] = 0;
		m_Mat[1][1] = 2*zNear/heightOfViewVolume;
		m_Mat[2][1] = 0;
		m_Mat[3][1] = 0;

		m_Mat[0][2] = 0;
		m_Mat[1][2] = 0;
		m_Mat[2][2] = zFar/(zNear-zFar);
		m_Mat[3][2] = -1;

		m_Mat[0][3] = 0;
		m_Mat[1][3] = 0;
		m_Mat[2][3] = zNear*zFar/(zNear-zFar);
		m_Mat[3][3] = 0;
	}

	/*! Builds a left-handed perspective projection matrix.
	*/
	inline void BuildProjectionMatrixPerspectiveLH(
									TValue widthOfViewVolume,
									TValue heightOfViewVolume,
									TValue zNear,
									TValue zFar)
	{
		m_Mat[0][0] = 2*zNear/widthOfViewVolume;
		m_Mat[1][0] = 0;
		m_Mat[2][0] = 0;
		m_Mat[3][0] = 0;

		m_Mat[0][1] = 0;
		m_Mat[1][1] = 2*zNear/heightOfViewVolume;
		m_Mat[2][1] = 0;
		m_Mat[3][1] = 0;

		m_Mat[0][2] = 0;
		m_Mat[1][2] = 0;
		m_Mat[2][2] = zFar/(zFar-zNear);
		m_Mat[3][2] = 1;

		m_Mat[0][3] = 0;
		m_Mat[1][3] = 0;
		m_Mat[2][3] = zNear*zFar/(zNear-zFar);
		m_Mat[3][3] = 0;
	}

	/*! Builds a right-handed perspective projection matrix based on a field of view
	*/
	inline void BuildProjectionMatrixPerspectiveFovRH(
				TValue fieldOfViewRadians,
				TValue aspectRatio,
				TValue zNear,
				TValue zFar)
	{
//*
		TValue c,s,Q;
			
			c= (TValue) cos( 0.5f*DEGTORAD(fieldOfViewRadians) );
			s= (TValue) sin( 0.5f*DEGTORAD(fieldOfViewRadians) );
			
			Q= s/(1.0f-zNear/zFar);
//			Q= s/zNear;
			
			m_Mat[0][0]= c/(aspectRatio*Q*zNear);
			m_Mat[1][0]= 0;
			m_Mat[2][0]= 0;
			m_Mat[3][0]= 0;

			m_Mat[0][1]= 0;
			m_Mat[1][1]= c/(Q*zNear);
			m_Mat[2][1]= 0;
			m_Mat[3][1]= 0;

			m_Mat[0][2]= 0;
			m_Mat[1][2]= 0;
			m_Mat[2][2]= -1/zNear;
			m_Mat[3][2]= s/(Q*zNear);

			m_Mat[0][3]= 0;
			m_Mat[1][3]= 0;
			m_Mat[2][3]= 1;
			m_Mat[3][3]= 0;
//*/
/*
		TValue sine, cotangent, deltaZ;
		TValue radians = fieldOfViewRadians / 2.0f * 0.017453f;

		deltaZ = zFar - zNear;
		sine   = sinf(radians);

//		setIdentity();
//	  	if ((deltaZ == 0.0f) || (sine == 0.0f) || (aspect == 0.0f))
//			  return *this;
		
		cotangent = cosf(radians) / sine;


		m_Mat[0][0] = cotangent / aspectRatio;
		m_Mat[1][1] = cotangent;
		m_Mat[2][2] = - (zFar + zNear) / deltaZ;
		m_Mat[2][3] = -1.0f;
		m_Mat[3][2] = -2.0f * zNear * zFar / deltaZ;
		m_Mat[3][3] = 0.0f;
//*/
	}

	/*! Builds a left-handed perspective projection matrix based on a field of view
	*/
	inline void BuildProjectionMatrixPerspectiveFovLH(
				TValue fieldOfViewRadians,
				TValue aspectRatio,
				TValue zNear,
				TValue zFar)
	{
	TValue c;

		c= (TValue) tan( DEGTORAD(fieldOfViewRadians) );
		
	//	Q= s/(1.0f-zNear/zFar);
		
		m_Mat[0][0]= 1/c;
		m_Mat[1][0]= 0;
		m_Mat[2][0]= 0;
		m_Mat[3][0]= 0;

		m_Mat[0][1]= 0;
		m_Mat[1][1]= 1/c;
		m_Mat[2][1]= 0;
		m_Mat[3][1]= 0;

		m_Mat[0][2]= 0;
		m_Mat[1][2]= 0;
		m_Mat[2][2]= zFar/(zFar - zNear);
		m_Mat[3][2]= (zFar*zNear)/(zFar - zNear);

		m_Mat[0][3]= 0;
		m_Mat[1][3]= 0;
		m_Mat[2][3]= 1;
		m_Mat[3][3]= 0;
	}


	/*! Builds a right-handed orthogonal projection matrix
	*/
	inline void BuildProjectionMatrixOrthoRH(
								TValue widthOfViewVolume,
								TValue heightOfViewVolume,
								TValue zNear,
								TValue zFar)
	{
		m_Mat[0][0] = 2/widthOfViewVolume;
		m_Mat[1][0] = 0;
		m_Mat[2][0] = 0;
		m_Mat[3][0] = 0;

		m_Mat[0][1] = 0;
		m_Mat[1][1] = 2/heightOfViewVolume;
		m_Mat[2][1] = 0;
		m_Mat[3][1] = 0;

		m_Mat[0][2] = 0;
		m_Mat[1][2] = 0;
		m_Mat[2][2] = 1/(zNear-zFar);
		m_Mat[3][2] = 0;

		m_Mat[0][3] = 0;
		m_Mat[1][3] = 0;
		m_Mat[2][3] = zNear/(zNear-zFar);
		m_Mat[3][3] = 1;
	}

	/*! Builds a left-handed orthogonal projection matrix
	*/
	inline void BuildProjectionMatrixOrthoLH(
				TValue widthOfViewVolume,
				TValue heightOfViewVolume,
				TValue zNear,
				TValue zFar)
	{
		m_Mat[0][0] = 2/widthOfViewVolume;
		m_Mat[1][0] = 0;
		m_Mat[2][0] = 0;
		m_Mat[3][0] = 0;

		m_Mat[0][1] = 0;
		m_Mat[1][1] = 2/heightOfViewVolume;
		m_Mat[2][1] = 0;
		m_Mat[3][1] = 0;

		m_Mat[0][2] = 0;
		m_Mat[1][2] = 0;
		m_Mat[2][2] = 1/(zNear-zFar);
		m_Mat[3][2] = 0;

		m_Mat[0][3] = 0;
		m_Mat[1][3] = 0;
		m_Mat[2][3] = zNear/(zNear-zFar);
		m_Mat[3][3] = -1;
	}

	/*! Builds a right-handed look-at matrix like D3D and OGL (gluLookAt)
	\param position - Eye position in 3D space
	\param target - target position (look at point) in 3D space
	\param upVector - Up vector (orientation)
	*/
	inline void BuildCameraLookAtMatrixRH(
				TVector3<TValue> &position,
				TVector3<TValue> &target,
				TVector3<TValue> &upVector
				)
	{
		TVector3<TValue> zaxis = position - target;
		zaxis.Normalize();

		TVector3<TValue> xaxis = upVector.CrossProduct(zaxis);
		xaxis.Normalize();

		TVector3<TValue> yaxis = zaxis.CrossProduct(xaxis);

		TValue dot0 = -xaxis.DotProduct(position);
		TValue dot1 = -yaxis.DotProduct(position);
		TValue dot2 = -zaxis.DotProduct(position);

		m_Mat[0][0] = xaxis.x;  m_Mat[1][0] = yaxis.x;  m_Mat[2][0] = zaxis.x;  m_Mat[3][0] = 0;
		m_Mat[0][1] = xaxis.y;  m_Mat[1][1] = yaxis.y;  m_Mat[2][1] = zaxis.y;  m_Mat[3][1] = 0;
		m_Mat[0][2] = xaxis.z;  m_Mat[1][2] = yaxis.z;  m_Mat[2][2] = zaxis.z;  m_Mat[3][2] = 0;
		m_Mat[0][3] = dot0;     m_Mat[1][3] = dot1;     m_Mat[2][3] = dot2;     m_Mat[3][3] = 1;
	}
	
	/*! Builds a left-handed look-at matrix like D3D
	\param position - Eye position in 3D space
	\param target - target position (look at point) in 3D space
	\param upVector - Up vector (orientation)
	*/
	inline void BuildCameraLookAtMatrixLH(
						TVector3<TValue> &position,
						TVector3<TValue> &target,
						TVector3<TValue> &upVector
						)
	{
		TVector3<TValue> zaxis = target - position;
		zaxis.Normalize();

		TVector3<TValue> xaxis = upVector.CrossProduct(zaxis);
		xaxis.Normalize();

		TVector3<TValue> yaxis = zaxis.CrossProduct(xaxis);

		TValue dot0 = -xaxis.DotProduct(position);
		TValue dot1 = -yaxis.DotProduct(position);
		TValue dot2 = -zaxis.DotProduct(position);

		m_Mat[0][0] = xaxis.x;  m_Mat[1][0] = yaxis.x;  m_Mat[2][0] = zaxis.x;  m_Mat[3][0] = 0;
		m_Mat[0][1] = xaxis.y;  m_Mat[1][1] = yaxis.y;  m_Mat[2][1] = zaxis.y;  m_Mat[3][1] = 0;
		m_Mat[0][2] = xaxis.z;  m_Mat[1][2] = yaxis.z;  m_Mat[2][2] = zaxis.z;  m_Mat[3][2] = 0;
		m_Mat[0][3] = dot0;     m_Mat[1][3] = dot1;     m_Mat[2][3] = dot2;     m_Mat[3][3] = 1;
	}
	//
	// 3D SPECIFIC MATRIX BUILDING - END
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

public:

	TMatrix4<TValue> m_Mat;
}

#endif // __T_PROJECTION_MATRIX4_H__
