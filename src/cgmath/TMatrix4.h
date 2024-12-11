#ifndef __TMATRIX4_H__
#define __TMATRIX4_H__

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
class TMatrix4 
{
public:

	//
	// Constructors
	//

	TMatrix4<TValue>()
	{
		SetIdentity();
	}

	TMatrix4<TValue>(TValue *mat)
	{
		memcpy( m_Mat, mat, 16*sizeof(TValue) );
	}

	TMatrix4<TValue>(
				TValue m0, TValue m4, TValue  m8, TValue m12,
				TValue m1, TValue m5, TValue  m9, TValue m13,
				TValue m2, TValue m6, TValue m10, TValue m14,
				TValue m3, TValue m7, TValue m11, TValue m15
				)
	{
		m_Mat[0][0]=m0; m_Mat[0][1]=m4; m_Mat[0][2]=m8;  m_Mat[0][3]=m12;
		m_Mat[1][0]=m1; m_Mat[1][1]=m5; m_Mat[1][2]=m9;  m_Mat[1][3]=m13;
		m_Mat[2][0]=m2; m_Mat[2][1]=m6; m_Mat[2][2]=m10; m_Mat[2][3]=m14;
		m_Mat[3][0]=m3; m_Mat[3][1]=m7; m_Mat[3][2]=m11; m_Mat[3][3]=m15;
	}

	//
	// Destructor
	//

	~TMatrix4<TValue>()
	{
	}

	//
	// Operators
	//

	inline TMatrix4<TValue>& operator=(TValue *mat)
	{
		memcpy( m_Mat, mat, 16*sizeof(TValue) );
		return *this;
	}

	inline TMatrix4<TValue>& operator=(const TMatrix4<TValue> &src)
	{
		memcpy( m_Mat, src.m_Mat, 16*sizeof(TValue) );
		return *this;
	}

	inline bool operator==(const TMatrix4<TValue> &right) const
	{
	int i,j;
	bool bRet = true;
		
		for (i=0; i<4; i++) 
			for (j=0; j<4; j++) 
				if ( m_Mat[i][j] != right.m_Mat[i][j] )
					bRet = false;

	return bRet;
	}

	inline bool operator!=(const TMatrix4<TValue> &right) const
	{
	int i,j;
	bool bRet = true;
		
		for (i=0; i<4; i++) 
			for (j=0; j<4; j++) 
				if ( m_Mat[i][j] != right.m_Mat[i][j] )
					bRet = false;

		//Reverse the result
		bRet = !bRet;

	return bRet;
	}

	inline TMatrix4<TValue>& operator*=(const TMatrix4<TValue>& right)
	{
	TMatrix4<TValue> res;
	int i,j,k;
		
		for (i=0; i<4; i++) 
		{
			for (j=0; j<4; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<4; k++) 
				{
					res.m_Mat[i][j] += m_Mat[i][k] * (right.m_Mat[k][j]);
				}
			}
		}

		*this = res;
	return *this;
	}

	TMatrix4<TValue> operator*(const TMatrix4<TValue>& right) const
	{
	TMatrix4<TValue> res;
	int i,j,k;
		
		for (i=0; i<4; i++) 
		{
			for (j=0; j<4; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<4; k++) 
				{
					res.m_Mat[i][j] += m_Mat[i][k] * (right.m_Mat[k][j]);
				}
			}
		}

	return res;
	}

	//
	// Set
	//

	inline void SetIdentity(void)
	{
		m_Mat[0][0]=1; m_Mat[1][0]=0; m_Mat[2][0]=0; m_Mat[3][0]=0;
		m_Mat[0][1]=0; m_Mat[1][1]=1; m_Mat[2][1]=0; m_Mat[3][1]=0;
		m_Mat[0][2]=0; m_Mat[1][2]=0; m_Mat[2][2]=1; m_Mat[3][2]=0;
		m_Mat[0][3]=0; m_Mat[1][3]=0; m_Mat[2][3]=0; m_Mat[3][3]=1;
	}

	// ref : https://www.opengl.org/wiki/GluLookAt_code
	inline void SetLookAt(Vector3 &eye, Vector3 &center, Vector3 &up)
	{
		Vector3 forward = eye - center;
		forward.Normalize();
		Vector3 side; // side = forward x up
		up.Normalize();
		side.CrossProduct(up, forward);
		side.Normalize();
		up.CrossProduct(forward, side); // recompute up as: up = side x forward

		//------------------
		m_Mat[0][0] = side[0];
		m_Mat[1][0] = side[1];
		m_Mat[2][0] = side[2];
		//------------------
		m_Mat[0][1] = up[0];
		m_Mat[1][1] = up[1];
		m_Mat[2][1] = up[2];
		//------------------
		m_Mat[0][2] = forward[0];
		m_Mat[1][2] = forward[1];
		m_Mat[2][2] = forward[2];
		//------------------
		m_Mat[0][3] = m_Mat[1][3] = m_Mat[2][3] = 0.0;
		//------------------
		m_Mat[3][0] =  -(side * eye);
		m_Mat[3][1] =  -(up * eye);
		m_Mat[3][2] =  -(forward * eye);
		//------------------
		m_Mat[3][3] = 1.0;
	}

	// ref : // https://www.opengl.org/wiki/GluPerspective_code
	inline void frustrum(float left, float right, float bottom, float top, float znear, float zfar)
	{
		float temp, temp2, temp3, temp4;
		temp = 2.0 * znear;
		temp2 = right - left;
		temp3 = top - bottom;
		temp4 = zfar - znear;
		m_Mat[0][0] = temp / temp2;
		m_Mat[0][1] = 0.0;
		m_Mat[0][2] = 0.0;
		m_Mat[0][3] = 0.0;
		m_Mat[1][0] = 0.0;
		m_Mat[1][1] = temp / temp3;
		m_Mat[1][2] = 0.0;
		m_Mat[1][3] = 0.0;
		m_Mat[2][0] = (right + left) / temp2;
		m_Mat[2][1] = (top + bottom) / temp3;
		m_Mat[2][2] = (-zfar - znear) / temp4;
		m_Mat[2][3] = -1.0;
		m_Mat[3][0] = 0.0;
		m_Mat[3][1] = 0.0;
		m_Mat[3][2] = (-temp * zfar) / temp4;
		m_Mat[3][3] = 0.0;
	}

	// ref : // https://www.opengl.org/wiki/GluPerspective_code
	inline void SetPerspective(float fovy /* degrees */, float aspect, float znear, float zfar)
	{
		float ymax, xmax;
		ymax = znear * tanf(fovy * M_PI / 360.0);
		//ymin = -ymax;
		//xmin = -ymax * aspectRatio;
		xmax = ymax * aspect;
		frustrum(-xmax, xmax, -ymax, ymax, znear, zfar);
	}

	// ref : http://www.songho.ca/opengl/gl_projectionmatrix.html#ortho (not tested)
	inline void SetOrtho(float left, float right, float bottom, float top, float n, float f)
	{
		m_Mat[0][0] = 2.0f / (right - left);
		m_Mat[0][1] = 0.f;
		m_Mat[0][2] = 0.f;
		m_Mat[0][3] = 0.f;
		m_Mat[1][0] = 0.f;
		m_Mat[1][1] = 2.0f / (top - bottom);
		m_Mat[1][2] = 0.f;
		m_Mat[1][3] = 0.f;
		m_Mat[2][0] = 0.f;
		m_Mat[2][1] = 0.f;
		m_Mat[2][2] = -2.0f / (f - n);
		m_Mat[2][3] = 0.f;
		m_Mat[3][0] = -(right + left) / (right - left);
		m_Mat[3][1] = -(top + bottom) / (top - bottom);
		m_Mat[3][2] = -(f + n) / (f - n);
		m_Mat[3][3] = 1.f;
	}

	inline TValue *Multiply(TValue *mat)
	{
	TMatrix4<TValue> res;
	int i,j,k;
	TMatrix4<TValue> TempMatrice(mat);
		
		for (i=0; i<4; i++) 
		{
			for (j=0; j<4; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<4; k++) 
				{
					res.m_Mat[i][j] += m_Mat[i][k] * (TempMatrice.m_Mat[k][j]);
				}
			}
		}

		*this = res;

		return (TMatrix4<TValue>)(&res.m_Mat[0][0]);
	}


	//Exactly the same function as above in the form of operator
	inline friend TVector3<TValue> operator * (const TVector3<TValue> &t,const TMatrix4<TValue> &mat)
	{
		return TVector3<TValue>(
				mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3],
				mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3],
				mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]
				);
	}

	inline friend TVector3<TValue> operator * (const TMatrix4<TValue> &mat,const TVector3<TValue> &t)
	{
		return TVector3<TValue>(
				mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3],
				mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3],
				mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]
				);
	}

	inline friend TVector4<TValue> operator * (const TMatrix4<TValue> &mat, const TVector4<TValue> &t)
	{
		return TVector4<TValue>(
				mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3]*t.w,
				mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3]*t.w,
				mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]*t.w,
				mat.m_Mat[3][0]*t.x + mat.m_Mat[3][1]*t.y + mat.m_Mat[3][2]*t.z + mat.m_Mat[3][3]*t.w
				);
	}

	inline friend TVector4<TValue> operator * (const TVector4<TValue> &t,const TMatrix4<TValue> &mat)
	{
		return TVector4<TValue>(
				mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3]*t.w,
				mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3]*t.w,
				mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]*t.w,
				mat.m_Mat[3][0]*t.x + mat.m_Mat[3][1]*t.y + mat.m_Mat[3][2]*t.z + mat.m_Mat[3][3]*t.w
				);
	}

	inline friend void operator *=(TVector3<TValue> &t,const TMatrix4<TValue> &mat)
	{
		t.Set(
			mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3],
			mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3],
			mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]
			);
	}

	inline friend void operator *=(TVector4<TValue> &t,const TMatrix4<TValue> &mat)
	{
		t.Set(
			mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z + mat.m_Mat[0][3]*t.w,
			mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z + mat.m_Mat[1][3]*t.w,
			mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z + mat.m_Mat[2][3]*t.w,
			mat.m_Mat[3][0]*t.x + mat.m_Mat[3][1]*t.y + mat.m_Mat[3][2]*t.z + mat.m_Mat[3][3]*t.w
			);
	}


	/*! Transpose the matrice
	*/
	void Transpose()
	{
		int i,j;
		TMatrix4<TValue> TempMatrice;
			
		for ( i=0; i<4; i++ ) 
			for ( j=0; j<4; j++ ) 
				TempMatrice.m_Mat[i][j]= m_Mat[j][i];

		//Now copy the result
		for ( i=0; i<4; i++ ) 
			for ( j=0; j<4; j++ ) 
				m_Mat[i][j] = TempMatrice.m_Mat[i][j];
	}

	/*! Return transpose matrice
	\param TransposeMatrix - The matrix to transpose
	\return TValue* - Transpose matrix
	*/
	TValue *GetTranspose(TValue *TransposeMatrix)
	{
		int i,j;
		TMatrix4<TValue> TempMatrice(TransposeMatrix);
			
			for ( i=0; i<4; i++ ) 
				for ( j=0; j<4; j++ ) 
					TempMatrice.m_Mat[i][j]= m_Mat[j][i];

			return (TMatrix4<TValue>)(&TempMatrice.m_Mat[0][0]);
	}

	inline TValue Determinant()
	{
		TValue d = (m_Mat[0][0] * m_Mat[1][1] - m_Mat[1][0] * m_Mat[0][1]) * (m_Mat[2][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[2][3])
			- (m_Mat[0][0] * m_Mat[2][1] - m_Mat[2][0] * m_Mat[0][1]) * (m_Mat[1][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[1][3])
			+ (m_Mat[0][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[0][1]) * (m_Mat[1][2] * m_Mat[2][3] - m_Mat[2][2] * m_Mat[1][3])
			+ (m_Mat[1][0] * m_Mat[2][1] - m_Mat[2][0] * m_Mat[1][1]) * (m_Mat[0][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[0][3])
			- (m_Mat[1][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[1][1]) * (m_Mat[0][2] * m_Mat[2][3] - m_Mat[2][2] * m_Mat[0][3])
			+ (m_Mat[2][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[2][1]) * (m_Mat[0][2] * m_Mat[1][3] - m_Mat[1][2] * m_Mat[0][3]);

		return d;
	}

	/*! Calculate the inverse of the current matrix
	Brute force a bit slow
	\param OutMatrix - In return contains the inverted matrix
	\return Returns false if there is no inverse matrix.
	*/
	inline bool GetInverse(TMatrix4<TValue>& OutMatrix)
	{
		// Calculates the inverse of this Matrix 
		// The inverse is calculated using Cramers rule.
		// If no inverse exists then 'false' is returned.

		const TMatrix4<TValue> &m = *this;

		TValue d = Determinant();
				
			if (d == 0.f)	//Impossible to inverse the matrix
				return false;

			//To avoid multiple division
			d = 1.f / d;

			//First row
			OutMatrix.m_Mat[0][0] = d * (
				  m_Mat[1][1] * (m_Mat[2][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[2][3])
				+ m_Mat[2][1] * (m_Mat[3][2] * m_Mat[1][3] - m_Mat[1][2] * m_Mat[3][3])
				+ m_Mat[3][1] * (m_Mat[1][2] * m_Mat[2][3] - m_Mat[2][2] * m_Mat[1][3])
				);

			OutMatrix.m_Mat[1][0] = d * (
				  m_Mat[1][2] * (m_Mat[2][0] * m_Mat[3][3] - m_Mat[3][0] * m_Mat[2][3])
				+ m_Mat[2][2] * (m_Mat[3][0] * m_Mat[1][3] - m_Mat[1][0] * m_Mat[3][3])
				+ m_Mat[3][2] * (m_Mat[1][0] * m_Mat[2][3] - m_Mat[2][0] * m_Mat[1][3])
				);

			OutMatrix.m_Mat[2][0] = d * (
				m_Mat[1][3] * (m_Mat[2][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[2][1])
				+ m_Mat[2][3] * (m_Mat[3][0] * m_Mat[1][1] - m_Mat[1][0] * m_Mat[3][1])
				+ m_Mat[3][3] * (m_Mat[1][0] * m_Mat[2][1] - m_Mat[2][0] * m_Mat[1][1])
				);

			OutMatrix.m_Mat[3][0] = d * (
				m_Mat[1][0] * (m_Mat[3][1] * m_Mat[2][2] - m_Mat[2][1] * m_Mat[3][2])
				+ m_Mat[2][0] * (m_Mat[1][1] * m_Mat[3][2] - m_Mat[3][1] * m_Mat[1][2])
				+ m_Mat[3][0] * (m_Mat[2][1] * m_Mat[1][2] - m_Mat[1][1] * m_Mat[2][2])
				);

			//Second row
			OutMatrix.m_Mat[0][1] = d * (
				  m_Mat[2][1] * (m_Mat[0][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[0][3])
				+ m_Mat[3][1] * (m_Mat[2][2] * m_Mat[0][3] - m_Mat[0][2] * m_Mat[2][3])
				+ m_Mat[0][1] * (m_Mat[3][2] * m_Mat[2][3] - m_Mat[2][2] * m_Mat[3][3])
				);

			OutMatrix.m_Mat[1][1] = d * (
				  m_Mat[2][2] * (m_Mat[0][0] * m_Mat[3][3] - m_Mat[3][0] * m_Mat[0][3])
				+ m_Mat[3][2] * (m_Mat[2][0] * m_Mat[0][3] - m_Mat[0][0] * m_Mat[2][3])
				+ m_Mat[0][2] * (m_Mat[3][0] * m_Mat[2][3] - m_Mat[2][0] * m_Mat[3][3])
				);

			OutMatrix.m_Mat[2][1] = d * (
				  m_Mat[2][3] * (m_Mat[0][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[0][1])
				+ m_Mat[3][3] * (m_Mat[2][0] * m_Mat[0][1] - m_Mat[0][0] * m_Mat[2][1])
				+ m_Mat[0][3] * (m_Mat[3][0] * m_Mat[2][1] - m_Mat[2][0] * m_Mat[3][1])
				);

			OutMatrix.m_Mat[3][1] = d * (
				  m_Mat[2][0] * (m_Mat[3][1] * m_Mat[0][2] - m_Mat[0][1] * m_Mat[3][2])
				+ m_Mat[3][0] * (m_Mat[0][1] * m_Mat[2][2] - m_Mat[2][1] * m_Mat[0][2])
				+ m_Mat[0][0] * (m_Mat[2][1] * m_Mat[3][2] - m_Mat[3][1] * m_Mat[2][2])
				);

			//Third row
			OutMatrix.m_Mat[0][2] = d * (
				  m_Mat[3][1] * (m_Mat[0][2] * m_Mat[1][3] - m_Mat[1][2] * m_Mat[0][3])
				+ m_Mat[0][1] * (m_Mat[1][2] * m_Mat[3][3] - m_Mat[3][2] * m_Mat[1][3])
				+ m_Mat[1][1] * (m_Mat[3][2] * m_Mat[0][3] - m_Mat[0][2] * m_Mat[3][3])
				);

			OutMatrix.m_Mat[1][2] = d * (
				  m_Mat[3][2] * (m_Mat[0][0] * m_Mat[1][3] - m_Mat[1][0] * m_Mat[0][3])
				+ m_Mat[0][2] * (m_Mat[1][0] * m_Mat[3][3] - m_Mat[3][0] * m_Mat[1][3])
				+ m_Mat[1][2] * (m_Mat[3][0] * m_Mat[0][3] - m_Mat[0][0] * m_Mat[3][3])
				);

			OutMatrix.m_Mat[2][2] = d * (
				  m_Mat[3][3] * (m_Mat[0][0] * m_Mat[1][1] - m_Mat[1][0] * m_Mat[0][1])
				+ m_Mat[0][3] * (m_Mat[1][0] * m_Mat[3][1] - m_Mat[3][0] * m_Mat[1][1])
				+ m_Mat[1][3] * (m_Mat[3][0] * m_Mat[0][1] - m_Mat[0][0] * m_Mat[3][1])
				);

			OutMatrix.m_Mat[3][2] = d * (
				  m_Mat[3][0] * (m_Mat[1][1] * m_Mat[0][2] - m_Mat[0][1] * m_Mat[1][2])
				+ m_Mat[0][0] * (m_Mat[3][1] * m_Mat[1][2] - m_Mat[1][1] * m_Mat[3][2])
				+ m_Mat[1][0] * (m_Mat[0][1] * m_Mat[3][2] - m_Mat[3][1] * m_Mat[0][2])
				);

			//Fourth row
			OutMatrix.m_Mat[0][3] = d * (
				  m_Mat[0][1] * (m_Mat[2][2] * m_Mat[1][3] - m_Mat[1][2] * m_Mat[2][3])
				+ m_Mat[1][1] * (m_Mat[0][2] * m_Mat[2][3] - m_Mat[2][2] * m_Mat[0][3])
				+ m_Mat[2][1] * (m_Mat[1][2] * m_Mat[0][3] - m_Mat[0][2] * m_Mat[1][3])
				);

			OutMatrix.m_Mat[1][3] = d * (
				  m_Mat[0][2] * (m_Mat[2][0] * m_Mat[1][3] - m_Mat[1][0] * m_Mat[2][3])
				+ m_Mat[1][2] * (m_Mat[0][0] * m_Mat[2][3] - m_Mat[2][0] * m_Mat[0][3])
				+ m_Mat[2][2] * (m_Mat[1][0] * m_Mat[0][3] - m_Mat[0][0] * m_Mat[1][3])
				);

			OutMatrix.m_Mat[2][3] = d * (
				  m_Mat[0][3] * (m_Mat[2][0] * m_Mat[1][1] - m_Mat[1][0] * m_Mat[2][1])
				+ m_Mat[1][3] * (m_Mat[0][0] * m_Mat[2][1] - m_Mat[2][0] * m_Mat[0][1])
				+ m_Mat[2][3] * (m_Mat[1][0] * m_Mat[0][1] - m_Mat[0][0] * m_Mat[1][1])
				);

			OutMatrix.m_Mat[3][3] = d * (
				  m_Mat[0][0] * (m_Mat[1][1] * m_Mat[2][2] - m_Mat[2][1] * m_Mat[1][2])
				+ m_Mat[1][0] * (m_Mat[2][1] * m_Mat[0][2] - m_Mat[0][1] * m_Mat[2][2])
				+ m_Mat[2][0] * (m_Mat[0][1] * m_Mat[1][2] - m_Mat[1][1] * m_Mat[0][2])
				);

		return true;
	}

	/*! Calculate the inverse of the current matrix and assign
	the result to our object
	\return Returns false if there is no inverse matrix.
	*/
	inline bool SetInverse()
	{
		TMatrix4<TValue> OutMatrix;

		bool bRet = GetInverse( OutMatrix );

		*this = OutMatrix;
		return bRet;
	}


	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline TValue *GetMatPtr()
	{
		return &m_Mat[0][0];
	}

	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline operator TValue* ()
	{
		return &m_Mat[0][0];
	}   

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TMatrix4<TValue> &right)
	{
		return out << "( " << right.m_Mat[0][0] << " , " << right.m_Mat[0][1] << " , " << right.m_Mat[0][2] << " , " << right.m_Mat[0][3] << " )" << endl \
			<< "( " << right.m_Mat[1][0] << " , " << right.m_Mat[1][1] << " , " << right.m_Mat[1][2] << " , " << right.m_Mat[1][3] << " )" << endl \
			<< "( " << right.m_Mat[2][0] << " , " << right.m_Mat[2][1] << " , " << right.m_Mat[2][2] << " , " << right.m_Mat[2][3] << " )" << endl \
			<< "( " << right.m_Mat[3][0] << " , " << right.m_Mat[3][1] << " , " << right.m_Mat[3][2] << " , " << right.m_Mat[3][3] << " )";
	}

public:

	TValue m_Mat[4][4];
};

typedef TMatrix4<int>    Matrix4i;
typedef TMatrix4<float>  Matrix4f;
typedef TMatrix4<double> Matrix4d;
typedef TMatrix4<float>  Matrix4;

#endif	// __MATRIX4_H__
