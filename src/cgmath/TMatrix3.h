#ifndef __TMATRIX3_H__
#define __TMATRIX3_H__

#include "TVector3.h"
#include "common.h"

#include <iostream>
using namespace std;

// TMatrix3[n][m] addresses the following element :
// n = row
// m = column
// ie
// m_Mat[0][0] m_Mat[0][1] m_Mat[0][2]
// m_Mat[1][0] m_Mat[1][1] m_Mat[1][2]
// m_Mat[2][0] m_Mat[2][1] m_Mat[2][2]

template <class TValue>
class TMatrix3 
{
public:

	//
	// Constructors
	//

	TMatrix3<TValue>()
	{
		SetIdentity();
	}

	TMatrix3<TValue>(TValue *mat)
	{
		memcpy( m_Mat, mat, 9*sizeof(TValue) );
	}

	TMatrix3<TValue>(
				TValue m0, TValue m3, TValue m6,
				TValue m1, TValue m4, TValue m7,
				TValue m2, TValue m5, TValue m8
				)
	{
		m_Mat[0][0]=m0; m_Mat[0][1]=m3; m_Mat[0][2]=m6;
		m_Mat[1][0]=m1; m_Mat[1][1]=m4; m_Mat[1][2]=m7;
		m_Mat[2][0]=m2; m_Mat[2][1]=m5; m_Mat[2][2]=m8;
	}

	//
	// Destructor
	//

	~TMatrix3<TValue>()
	{
	}

	//
	// Operators
	//

	inline TMatrix3<TValue>& operator=(TValue *mat)
	{
		memcpy( m_Mat, mat, 9*sizeof(TValue) );
		return *this;
	}

	inline TMatrix3<TValue>& operator=(const TMatrix3<TValue> &src)
	{
		for (int i=0; i<3; i++) 
			for (int j=0; j<3; j++) 
				m_Mat[i][j] = src.m_Mat[i][j];
		//memcpy( m_Mat, src.m_Mat, 9*sizeof(TValue) );
		return *this;
	}

	inline bool operator==(const TMatrix3<TValue> &right) const
	{
		for (int i=0; i<3; i++) 
			for (int j=0; j<3; j++) 
				if ( m_Mat[i][j] != right.m_Mat[i][j] )
					return false;

		return true;
	}

	inline bool operator!=(const TMatrix3<TValue> &right) const
	{
		for (int i=0; i<3; i++) 
			for (int j=0; j<3; j++) 
				if ( m_Mat[i][j] != right.m_Mat[i][j] )
					return true;

		return false;
	}

	inline TMatrix3<TValue>& operator+=(const TMatrix3<TValue>& right)
	{
		TMatrix3<TValue> res;

		for (int i=0; i<3; i++) 
			for (int j=0; j<3; j++) 
				res.m_Mat[i][j] = m_Mat[i][j] + right.m_Mat[i][j];

		*this = res;
		return *this;
	}

	inline TMatrix3<TValue>& operator*=(const TMatrix3<TValue>& right)
	{
		TMatrix3<TValue> res;
		int i,j,k;

		for (i=0; i<3; i++) 
			for (j=0; j<3; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<3; k++) 
					res.m_Mat[i][j] += m_Mat[i][k] * (right.m_Mat[k][j]);
			}

		*this = res;
		return *this;
	}

	inline TMatrix3<TValue>& operator*= (TValue s)
	{
		for (int i=0; i<3; i++) 
			for (int j=0; j<3; j++) 
				m_Mat[i][j] *= s;

		return *this;
	}

	inline TMatrix3<TValue> operator* (TValue s)
	{
		return TMatrix3<TValue>(m_Mat[0][0]*s, m_Mat[0][1]*s, m_Mat[0][2]*s,
	m_Mat[1][0]*s, m_Mat[1][1]*s, m_Mat[1][2]*s,
	   m_Mat[2][0]*s, m_Mat[2][1]*s, m_Mat[2][2]*s);
		}


	TMatrix3<TValue> operator* (const TMatrix3<TValue>& right) const
	{
	TMatrix3<TValue> res;
	int i,j,k;
		
		for (i=0; i<3; i++) 
			for (j=0; j<3; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<3; k++) 
					res.m_Mat[i][j] += m_Mat[i][k] * (right.m_Mat[k][j]);
			}

	return res;
	}

	//
	// Set
	//

	inline void SetIdentity(void)
	{
		m_Mat[0][0]=1; m_Mat[0][1]=0; m_Mat[0][2]=0;
		m_Mat[1][0]=0; m_Mat[1][1]=1; m_Mat[1][2]=0;
		m_Mat[2][0]=0; m_Mat[2][1]=0; m_Mat[2][2]=1;
	}

	inline void Set(
				TValue m0, TValue m3, TValue m6,
				TValue m1, TValue m4, TValue m7,
				TValue m2, TValue m5, TValue m8
				)
	{
		m_Mat[0][0]=m0; m_Mat[0][1]=m3; m_Mat[0][2]=m6;
		m_Mat[1][0]=m1; m_Mat[1][1]=m4; m_Mat[1][2]=m7;
		m_Mat[2][0]=m2; m_Mat[2][1]=m5; m_Mat[2][2]=m8;
	}

	inline TValue *Multiply(TValue *mat)
	{
	TMatrix3<TValue> res;
	int i,j,k;
	TMatrix3<TValue> TempMatrice(mat);
		
		for (i=0; i<3; i++) 
			for (j=0; j<3; j++) 
			{
				res.m_Mat[i][j] = 0.0;
				for (k=0; k<3; k++) 
					res.m_Mat[i][j] += m_Mat[i][k] * (TempMatrice.m_Mat[k][j]);
			}

		*this = res;

		return (TMatrix3<TValue>)(&res.m_Mat[0][0]);
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
		TValue length;
		TValue c,s,t;
		TValue theta = 3.14159*angle/180.;//RS_DEGTORAD(angle);
		
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
		m_Mat[0][0] = t*x*x + c;   m_Mat[0][1] = t*x*y - s*z; m_Mat[0][2] = t*x*z + s*y;
		m_Mat[1][0] = t*x*y + s*z; m_Mat[1][1] = t*y*y + c;   m_Mat[1][2] = t*y*z - s*x;
		m_Mat[2][0] = t*x*z - s*y; m_Mat[2][1] = t*y*z + s*x; m_Mat[2][2] = t*z*z + c;
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
		TValue s = (TValue)sin(DEGTORAD(angle));
		TValue c = (TValue)cos(DEGTORAD(angle));

		SetIdentity();

		m_Mat[1][1] =  c;   m_Mat[1][2] = -s;
		m_Mat[2][1] =  s;   m_Mat[2][2] =  c;
	}

	/*! Create a rotation matrice around a single axis
	\param angle - Angle value for the rotation around Y Axis
	*/
	void SetRotation_Y( TValue angle )
	{
		TValue s = (TValue)sin(DEGTORAD(angle));
		TValue c = (TValue)cos(DEGTORAD(angle));

		SetIdentity();

		m_Mat[0][0] =  c;   m_Mat[0][2] =  s;
		m_Mat[2][0] = -s;   m_Mat[2][2] =  c;
	}

	/*! Create a rotation matrice around a single axis
	\param angle - Angle value for the rotation around Z Axis
	*/
	void SetRotation_Z( TValue angle )
	{
		TValue s = (TValue)sin(DEGTORAD(angle));
		TValue c = (TValue)cos(DEGTORAD(angle));

		SetIdentity();

		m_Mat[0][0] =  c;   m_Mat[1][1] = -s;
		m_Mat[1][0] =  s;   m_Mat[1][1] =  c;
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
	inline void TransformPoint( TVector3<TValue> &vec )
	{
		TValue x = vec.x;
		TValue y = vec.y;
		TValue z = vec.z;

		//Since it's a point we are computing using the translation
		vec.x = x * m_Mat[0][0] + y * m_Mat[0][1] + z * m_Mat[0][2];
		vec.y = x * m_Mat[1][0] + y * m_Mat[1][1] + z * m_Mat[1][2];
		vec.z = x * m_Mat[2][0] + y * m_Mat[2][1] + z * m_Mat[2][2];
	}

	//Exactly the same function as above in the form of operator
	inline friend TVector3<TValue> operator* (const TMatrix3<TValue> &mat,const TVector3<TValue> &t)
	{
		return TVector3<TValue>(
				mat.m_Mat[0][0]*t.x + mat.m_Mat[0][1]*t.y + mat.m_Mat[0][2]*t.z,
				mat.m_Mat[1][0]*t.x + mat.m_Mat[1][1]*t.y + mat.m_Mat[1][2]*t.z,
				mat.m_Mat[2][0]*t.x + mat.m_Mat[2][1]*t.y + mat.m_Mat[2][2]*t.z
				);
	}


	//
	// TRANSFORMATION - END
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// MATH
	//


	/*! isSymmetric
	*/
	bool isSymmetric (void)
	{
		if (m_Mat[0][1] == m_Mat[1][0] &&
			m_Mat[0][2] == m_Mat[2][0] &&
			m_Mat[1][2] == m_Mat[2][1] )
			return true;
		else
			return false;
	}

	/*! Transpose the matrix
	*/
	void Transpose()
	{
		for (int i=0; i<3; i++ ) 
			for (int j=i+1; j<3; j++ )
			{
				TValue tmp = m_Mat[i][j];
				m_Mat[i][j] = m_Mat[j][i];
				m_Mat[j][i] = tmp;
			}
	}

	/*! Return transpose matrix
	\param TransposeMatrix - The matrix to transpose
	\return TValue* - Transpose matrix
	*/
	/*
	TValue *GetTranspose(TValue *TransposeMatrix)
	{
		int i,j;
		TMatrix3<TValue> TempMatrice(TransposeMatrix);
			
			for ( i=0; i<3; i++ ) 
				for ( j=0; j<3; j++ ) 
					TempMatrice.m_Mat[i][j]= m_Mat[j][i];

			return (TMatrix3<TValue>)(&TempMatrice.m_Mat[0][0]);
	}
	*/

	inline TValue Determinant()
	{
		TValue d = m_Mat[0][0] * (m_Mat[1][1]*m_Mat[2][2] - m_Mat[2][1]*m_Mat[1][2])
				 - m_Mat[1][0] * (m_Mat[0][1]*m_Mat[2][2] - m_Mat[2][1]*m_Mat[0][2])
				 + m_Mat[2][0] * (m_Mat[0][1]*m_Mat[1][2] - m_Mat[1][1]*m_Mat[0][2]);

		return d;
	}

	/*! Inverse the matrix
	\return Returns FALSE if there is no inverse matrix.
	*/
	inline bool Inverse(void)
	{
		// Calculates the inverse of this Matrix 
		// The inverse is calculated using Cramers rule.
		// If no inverse exists then 'FALSE' is returned.

		TValue d = Determinant();
		if (d == 0.f)	//Impossible to inverse the matrix
			return false;

		//To avoid multiple division
		d = 1. / d;

		// http://fr.wikipedia.org/wiki/Matrice_inversible
		TValue m00 = (m_Mat[1][1]*m_Mat[2][2] - m_Mat[1][2]*m_Mat[2][1]) * d;
		TValue m01 = (m_Mat[0][2]*m_Mat[2][1] - m_Mat[0][1]*m_Mat[2][2]) * d;
		TValue m02 = (m_Mat[0][1]*m_Mat[1][2] - m_Mat[0][2]*m_Mat[1][1]) * d;
		
		TValue m10 = (m_Mat[1][2]*m_Mat[2][0] - m_Mat[1][0]*m_Mat[2][2]) * d;
		TValue m11 = (m_Mat[0][0]*m_Mat[2][2] - m_Mat[0][2]*m_Mat[2][0]) * d;
		TValue m12 = (m_Mat[0][2]*m_Mat[1][0] - m_Mat[0][0]*m_Mat[1][2]) * d;
		
		TValue m20 = (m_Mat[2][1]*m_Mat[1][0] - m_Mat[1][1]*m_Mat[2][0]) * d;
		TValue m21 = (m_Mat[0][1]*m_Mat[2][0] - m_Mat[0][0]*m_Mat[2][1]) * d;
		TValue m22 = (m_Mat[0][0]*m_Mat[1][1] - m_Mat[0][1]*m_Mat[1][0]) * d;

		Set (	m00, m01, m02,
				m10, m11, m12,
				m20, m21, m22	);

		return true;
	}

	/*! Calculate the inverse of the current matrix
	\param OutMatrix - In return contains the inverted matrix
	\return Returns FALSE if there is no inverse matrix.
	*/
	inline bool GetInverse(TMatrix3<TValue>& OutMatrix)
	{
		// Calculates the inverse of this Matrix 
		// The inverse is calculated using Cramers rule.
		// If no inverse exists then 'FALSE' is returned.

		const TMatrix3<TValue> &m = *this;

		TValue d = Determinant();
				
		if (d == 0.f)	//Impossible to inverse the matrix
			return false;

		//To avoid multiple division
		d = 1.f / d;

/*
  TValue inv[9];
  TValue fdet, finvdet;
  
  inv[0] = m[4]*m[8] - m[5]*m[7];
  inv[1] = m[2]*m[7] - m[1]*m[8];
  inv[2] = m[1]*m[5] - m[2]*m[4];
  inv[3] = m[5]*m[6] - m[3]*m[8];
  inv[4] = m[0]*m[8] - m[2]*m[6];
  inv[5] = m[2]*m[3] - m[0]*m[5];
  inv[6] = m[3]*m[7] - m[4]*m[6];
  inv[7] = m[1]*m[6] - m[0]*m[7];
  inv[8] = m[0]*m[4] - m[1]*m[3];

  fdet = m[0]*inv[0] + m[1]*inv[3] + m[2]*inv[6];
  if (fdet == 0.0) return false;
  finvdet = 1.0f/fdet;

  m[0] = inv[0] * finvdet;
  m[1] = inv[1] * finvdet;
  m[2] = inv[2] * finvdet;
  m[3] = inv[3] * finvdet;
  m[4] = inv[4] * finvdet;
  m[5] = inv[5] * finvdet;
  m[6] = inv[6] * finvdet;
  m[7] = inv[7] * finvdet;
  m[8] = inv[8] * finvdet;

  return true;
*/
		return true;
	}

	/*! Calculate the inverse of the current matrix and assign
	the result to our object
	\return Returns FALSE if there is no inverse matrix.
	*/
	inline bool SetInverse()
	{
		TMatrix3<TValue> OutMatrix;

		bool bRet = GetInverse( OutMatrix );

		*this = OutMatrix;
		return bRet;
	}

	//
	// MATH - END
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

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
	// Algebra
	//
/**
* Used by the method solving the eigensystem.
*/
void tridiagonal (TValue diag[3], TValue subd[3])
{
  TValue fm00 = m_Mat[0][0];
  TValue fm01 = m_Mat[0][1];
  TValue fm02 = m_Mat[0][2];
  TValue fm11 = m_Mat[1][1];
  TValue fm12 = m_Mat[1][2];
  TValue fm22 = m_Mat[2][2];
  
  diag[0] = fm00;
  subd[2] = 0.0;
  if (fm02 != 0.0)
  {
      TValue length = sqrt (fm01*fm01 + fm02*fm02);
      TValue invlength = 1.0 / length;
      TValue q;
      fm01 *= invlength;
      fm02 *= invlength;
      q = 2.0*fm01*fm12 + fm02*(fm22-fm11);
      diag[1] = fm11+fm02*q;
      diag[2] = fm22-fm02*q;
      subd[0] = length;
      subd[1] = fm12-fm01*q;
	  Set (	1.0, 0.0, 0.0,
			0.0, fm01, fm02,
			0.0, fm02, -fm01	);
  }
  else
  {
      diag[1] = fm11;
      diag[2] = fm22;
      subd[0] = fm01;
      subd[1] = fm12;
	  SetIdentity ();
  }
}

/**
* QL algorithm. This transform is used to solve the eigensystem.
*/
bool QLalgorithm (TValue diag[3], TValue subd[3])
{
	int imaxiter = 32;
	int i0, i1=0, i2, i3, i4;
	
	for (i0=0; i0<3; i0++)
    {
		TValue G, R;
		TValue sinus, cosinus, P;
		
		for (i2=i0; i2<=1; i2++)
		{
			TValue tmp = fabs (diag[i2]) + fabs(diag[i2+1]);
			if (fabs(subd[i2])+tmp == tmp)
				break;
		}
		if (i2 == i0)
			break;
		
		G = (diag[i0+1] - diag[i0]) / (2.0*subd[i0]);
		R = sqrt (G*G+1.0);
		if (G < 0.0)
			G = diag[i2] - diag[i0] + subd[i0] / (G - R);
		else
			G = diag[i2] - diag[i0] + subd[i0] / (G + R);
		
		sinus = 1.0;
		cosinus = 1.0;
		P = 0.0;
		
		for (i3=i2-1; i3>=i0; i3--)
		{
			TValue F = sinus*subd[i3];
			TValue B = cosinus*subd[i3];
			if (fabs(F) >= fabs(G))
			{
				cosinus = G/F;
				R = sqrt(cosinus*cosinus + 1.0);
				subd[i3+1] = F*R;
				sinus = 1.0/R;
				cosinus *= sinus;
			}
			else
			{
				sinus = F/G;
				R = sqrt (sinus*sinus + 1.0);
				subd[i3+1]=G*R;
				cosinus = 1.0/R;
				sinus *= cosinus;
			}
			G = diag[i3+1] - P;
			R = (diag[i3] - G)*sinus + 2.0*B*cosinus;
			P = sinus*R;
			diag[i3+1] = G + P;
			G = cosinus*R - B;
			
			for (i4=0; i4<3; i4++)
			{
				F = m_Mat[i4][i3+1];
				m_Mat[i4][i3+1] = sinus*m_Mat[i4][i3] + cosinus*F;
				m_Mat[i4][i3] = cosinus*m_Mat[i4][i3] - sinus*F;
			}
		}
		diag[i0] -= P;
		subd[i0] = G;
		subd[i2] = 0.0;
    }
	if (i1 == imaxiter)
		return false;
	
	return true;
}

/**
* Used by the method solving the eigensystem.
*/
void decreasingsort (TValue diag[3], TValue subd[3])
{
  int i0, i1, i2;

  for (i0=0; i0<=1; i0++)
    {
      TValue max;
      i1 = i0;
      max = diag[i1];
      for (i2=i0+1; i2<3; i2++)
	{
	  if (diag[i2] > max)
	    {
	      i1 = i2;
	      max = diag[i1];
	    }
	}
      
      if (i1 != i0)
	{
	  diag[i1] = diag[i0];
	  diag[i0] = max;

	  for (i2=0; i2<3; i2++)
	    {
	      TValue tmp = m_Mat[i2][i0];
	      m_Mat[i2][i0] = m_Mat[i2][i1];
		  m_Mat[i2][i1] = tmp;
	    }
	}
    }
}

/**
* Solve the eigensystem based on the matrix.
*
* The steps followed by the algorithm are :
*
* tridiagonal.
*
* QLalgorithm.
*
* decreasingsort.
*
* The vector v contains the 3 eigenvalues.
* The eigenvectors associated to the eigenvalues are stored in the
* vectors e1, e2 and e3.
*
* \returns false if the eigensystem can't be solved , true otherwise.
*/
int SolveEigensystem (TVector3<TValue>& e1, TVector3<TValue>& e2, TVector3<TValue>& e3, TVector3<TValue>& v)
{
  if (Determinant() == 0)
  {
	  return false;
  }

  if (isSymmetric ())
  {
	  TMatrix3<TValue> tmp (*this);

	  TValue diag[3] = {0.0, 0.0, 0.0};
	  TValue subd[3] = {0.0, 0.0, 0.0};
	  tmp.tridiagonal    (diag, subd);
	  tmp.QLalgorithm    (diag, subd);
	  tmp.decreasingsort (diag, subd);

	  e1.Set (tmp.m_Mat[0][0], tmp.m_Mat[1][0], tmp.m_Mat[2][0]);
	  e2.Set (tmp.m_Mat[0][1], tmp.m_Mat[1][1], tmp.m_Mat[2][1]);
	  e3.Set (tmp.m_Mat[0][2], tmp.m_Mat[1][2], tmp.m_Mat[2][2]);
	  v.Set (diag[0], diag[1], diag[2]);
	  return true;
  }
  else
  {
	  return false;
  }
}

/**
* Solve the linear system based on the matrix.
*
* The vector right contains the right part of the linear system.
* The vector sol contains the solution.
*
* \returns false if the linear system can't be solved , true otherwise.
*/
	bool SolveLinearsystem (TVector3<TValue> right, TVector3<TValue> &sol)
	{
		TValue det = Determinant ();
		if (!det)
		{
		  return false;
		}

		det = 1./det;

		// Cramer's rule
		TMatrix3<TValue> tmp;
		tmp.Set (right.x, m_Mat[0][1], m_Mat[0][2],
			right.y, m_Mat[1][1], m_Mat[1][2],
			right.z, m_Mat[2][1], m_Mat[2][2]);
		TValue resultx = tmp.Determinant () * det;

		tmp.Set (m_Mat[0][0], right.x, m_Mat[0][2],
			m_Mat[1][0], right.y, m_Mat[1][2],
			m_Mat[2][0], right.z, m_Mat[2][2]);
		TValue resulty = tmp.Determinant () * det;

		tmp.Set (m_Mat[0][0], m_Mat[0][1], right.x,
			m_Mat[1][0], m_Mat[1][1], right.y,
			m_Mat[2][0], m_Mat[2][1], right.z);
		TValue resultz = tmp.Determinant () * det;

		sol.Set (resultx, resulty, resultz);

		return true;
	}

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TMatrix3<TValue> &right)
	{
		return out << "( " << right.m_Mat[0][0] << " , " << right.m_Mat[0][1] << " , " << right.m_Mat[0][2] << " )" << endl \
			<< "( " << right.m_Mat[1][0] << " , " << right.m_Mat[1][1] << " , " << right.m_Mat[1][2] << " )" << endl \
			<< "( " << right.m_Mat[2][0] << " , " << right.m_Mat[2][1] << " , " << right.m_Mat[2][2] << " )";
	}

public:

	TValue m_Mat[3][3];
};

typedef TMatrix3<int>    Matrix3i;
typedef TMatrix3<float>  Matrix3f;
typedef TMatrix3<double> Matrix3d;
typedef TMatrix3<float>  Matrix3;

#endif	// __TMATRIX3_H__
