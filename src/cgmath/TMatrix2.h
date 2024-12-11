#ifndef __TMATRIX2_H__
#define __TMATRIX2_H__

#include <string.h>
#include <iostream>
using namespace std;

#include "TVector2.h"

// TMatrix2[n][m] addresses the following element :
// n = row
// m = column
// ie
// m_Mat[0][0] m_Mat[0][1]
// m_Mat[1][0] m_Mat[1][1]

template <class TValue>
class TMatrix2 
{
public:

	//
	// Constructors
	//

	TMatrix2<TValue>()
	{
		SetIdentity();
	}

	TMatrix2<TValue>(TValue *mat)
	{
		memcpy( m_matrix, mat, 4*sizeof(TValue) );
	}

	TMatrix2<TValue>(	TValue m0, TValue m2,
						TValue m1, TValue m3	)
	{
		m_matrix[0][0]=m0; m_matrix[0][1]=m2;
		m_matrix[1][0]=m1; m_matrix[1][1]=m3;
	}

	//
	// Destructor
	//

	~TMatrix2<TValue>()
	{
	}

	//
	// Operators
	//

	inline TMatrix2<TValue>& operator=(TValue *mat)
	{
		memcpy( m_matrix, mat, 4*sizeof(TValue) );
		return *this;
	}

	inline TMatrix2<TValue>& operator=(const TMatrix2<TValue> &src)
	{
		memcpy( m_matrix, src.m_matrix, 4*sizeof(TValue) );
		return *this;
	}

	inline bool operator==(const TMatrix2<TValue> &right) const
	{
		int i,j;
		bool bRet = true;
		
		for (i=0; i<2; i++) 
			for (j=0; j<2; j++) 
				if ( m_matrix[i][j] != right.m_matrix[i][j] )
					bRet = false;

		return bRet;
	}

	inline bool operator!=(const TMatrix2<TValue> &right) const
	{
		int i,j;
		bool bRet = true;
		
		for (i=0; i<2; i++) 
			for (j=0; j<2; j++) 
				if ( m_matrix[i][j] != right.m_matrix[i][j] )
					bRet = false;

		bRet = !bRet;

		return bRet;
	}

	inline TMatrix2<TValue>& operator*=(const TMatrix2<TValue>& right)
	{
		TMatrix2<TValue> res;
		int i,j,k;
		
		/*
		res.m_matrix[0][0] = m_matrix[0][0] * right.m_matrix[0][0] + m_matrix[0][1] * right.m_matrix[1][0];
		res.m_matrix[0][1] = m_matrix[0][0] * right.m_matrix[0][1] + m_matrix[0][1] * right.m_matrix[1][1];
		res.m_matrix[1][0] = m_matrix[1][0] * right.m_matrix[0][0] + m_matrix[1][1] * right.m_matrix[1][0];
		res.m_matrix[1][1] = m_matrix[1][0] * right.m_matrix[0][1] + m_matrix[1][1] * right.m_matrix[1][1];
		*/
		for (i=0; i<2; i++) 
		{
			for (j=0; j<2; j++) 
			{
				res.m_matrix[i][j] = 0.0;
				for (k=0; k<2; k++) 
				{
					res.m_matrix[i][j] += m_matrix[i][k] * (right.m_matrix[k][j]);
				}
			}
		}

		*this = res;
		return *this;
	}

	TMatrix2<TValue> operator*(const TMatrix2<TValue>& right) const
	{
		TMatrix2<TValue> res;
		int i,j,k;
		
		for (i=0; i<2; i++) 
		{
			for (j=0; j<2; j++) 
			{
				res.m_matrix[i][j] = 0.0;
				for (k=0; k<2; k++) 
				{
					res.m_matrix[i][j] += m_matrix[i][k] * (right.m_matrix[k][j]);
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
		m_matrix[0][0]=1; m_matrix[1][0]=0;
		m_matrix[0][1]=0; m_matrix[1][1]=1;
	}

	inline TValue *Multiply(TValue *mat)
	{
		TMatrix2<TValue> res;
		int i,j,k;
		TMatrix2<TValue> TempMatrice(mat);
		
		for (i=0; i<2; i++) 
		{
			for (j=0; j<2; j++) 
			{
				res.m_matrix[i][j] = 0.0;
				for (k=0; k<2; k++) 
				{
					res.m_matrix[i][j] += m_matrix[i][k] * (TempMatrice.m_matrix[k][j]);
				}
			}
		}

		*this = res;

	return (TMatrix2<TValue>)(&res.m_matrix[0][0]);
	}


	/*! Transform a point or a vector using the matrix
	\param vec - The point/vector to transform
	*/
	inline void TransformPoint( TVector2<TValue> *vec )
	{
		TValue x = vec->x;
		TValue y = vec->y;

		vec->x = x * m_matrix[0][0] + y * m_matrix[0][1];
		vec->y = x * m_matrix[1][0] + y * m_matrix[1][1];
	}

	/*! Transform a vector using the matrix
	\param vec - The vector to transform
	*/
	inline void TransformVector( TVector2<TValue> *vec )
	{
		TValue x = vec->x;
		TValue y = vec->y;

		vec->x = x * m_matrix[0][0] + y * m_matrix[0][1];
		vec->y = x * m_matrix[1][0] + y * m_matrix[1][1];
	}

	TVector2<TValue> operator*(const TVector2<TValue>& v) const
	{
		TVector2<TValue> res;
		res.Set (
				m_matrix[0][0]*v.x + m_matrix[0][1]*v.y,
				m_matrix[1][0]*v.x + m_matrix[1][1]*v.y);
		return res;
	}
/*
	inline friend Vector2<TValue> operator* (const Vector2<TValue> &t,const Matrix2<TValue> &mat)
	{
		return Vector2<TValue>(
				mat.m_matrix[0][0]*t.x + mat.m_matrix[0][1]*t.y,
				mat.m_matrix[1][0]*t.x + mat.m_matrix[1][1]*t.y);
	}

	inline friend Vector2<TValue> operator* (const Matrix2<TValue> &mat,const Vector2<TValue> &t)
	{
		return Vector2<TValue>(
				mat.m_matrix[0][0]*t.x + mat.m_matrix[0][1]*t.y,
				mat.m_matrix[1][0]*t.x + mat.m_matrix[1][1]*t.y);
	}
*/
	void Transpose()
	{
		TValue temp = m_matrix[0][1];
		m_matrix[0][1] = m_matrix[1][0];
		m_matrix[1][0] = temp;
	}

	TValue *GetTranspose(TValue *TransposeMatrix)
	{
		int i,j;
		TMatrix2<TValue> TempMatrice(TransposeMatrix);
			
		for ( i=0; i<2; i++ ) 
			for ( j=0; j<2; j++ ) 
				TempMatrice.m_matrix[i][j]= m_matrix[j][i];

		return (TValue)(&TempMatrice.m_matrix[0][0]);
	}

	inline TValue Determinant()
	{
		TValue d = (m_matrix[0][0] * m_matrix[1][1] - m_matrix[1][0] * m_matrix[0][1]);

		return d;
	}

	/*! Get the inverse of the matrix
	ref : http://www.maths.abdn.ac.uk/~igc/tch/eg1006/notes/node119.html
	\param OutMatrix - In return contains the inverted matrix
	\return Returns FALSE if there is no inverse matrix.
	*/
	inline bool GetInverse(TMatrix2<TValue>& OutMatrix)
	{
		// Inversion of the matrix by using Cramers rule
		const TMatrix2<TValue> &m = *this;

		TValue d = Determinant();
				
		if (d == 0.f)	// Impossible to inverse the matrix
			return false;

		// To avoid multiple division
		d = 1.f / d;

		OutMatrix.m_matrix[0][0] =  m_matrix[1][1] * d;
		OutMatrix.m_matrix[1][0] = -m_matrix[1][0] * d;
		OutMatrix.m_matrix[0][1] = -m_matrix[0][1] * d;
		OutMatrix.m_matrix[1][1] =  m_matrix[0][0] * d;

		return true;
	}

	/*! Inverse the matrix
	\return Returns false if there is no inverse matrix.
	*/
	inline bool SetInverse()
	{
		TMatrix2<TValue> OutMatrix;

		bool bRet = GetInverse( OutMatrix );

		*this = OutMatrix;
		return bRet;
	}

	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline TValue *GetMatPtr()
	{
		return &m_matrix[0][0];
	}

	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline operator TValue* ()
	{
		return &m_matrix[0][0];
	}

	//
	// Algebra
	//

	bool SolveLinearsystem (TVector2<TValue>& right, TVector2<TValue>& sol)
	{
		TValue d = Determinant();
				
		if (d == 0.f)	// Impossible to inverse the matrix
			return false;

		// To avoid multiple division
		d = 1.f / d;

		sol.Set (	 d * (m_matrix[1][1] * right.x - m_matrix[0][1] * right.y),
					-d * (m_matrix[1][0] * right.x - m_matrix[0][0] * right.y)	);
		return true;
	}

	// Solve the eigensystem
	// @evector1 : first eigenvector (normalized)
	// @evector2 : second eigenvector (normalized)
	// @evalues : first eigenvalue in evalues.x, second eigenvalue in evalues.y
	// @return 1 if everything is ok, 0 if there is a problem
	bool SolveEigensystem (TVector2<TValue>& evector1, TVector2<TValue>& evector2, TVector2<TValue>& evalues)
	{
		TValue a = m_matrix[0][0];
		TValue b = m_matrix[0][1];
		TValue c = m_matrix[1][0];
		TValue d = m_matrix[1][1];

		if (b == 0.0 && c == 0.0)
		{
			evalues.Set (a, d);
			evector1.Set (1.0, 0.0);
			evector2.Set (0.0, 1.0);
			return true;
		}

		TValue delta = (a+d)*(a+d) - 4*(a*d-b*c);
		if (delta < 0.0)
		{
			return false;
		}

		evalues.Set (((a+d) + sqrt(delta)) / 2.0, ((a+d) - sqrt(delta)) / 2.0);

		if (b == 0.0)
			evector1.Set (1.0, -c / (d - evalues.x));
		else
			evector1.Set (1.0, (evalues.x - a) / b);

		if (b == 0.0)
			evector2.Set (1.0, -c / (d - evalues.y));
		else
			evector2.Set (1.0, (evalues.y - a) / b);
		evector1.Normalize ();
		evector2.Normalize ();

		return true;
	}

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TMatrix2<TValue> &right)
	{
		return out << "( " << right.m_matrix[0][0] << " , " << right.m_matrix[0][1] << " )" << endl \
			<< "( " << right.m_matrix[1][0] << " , " << right.m_matrix[1][1] << " )";
	}

public:

	TValue m_matrix[2][2];
};

typedef TMatrix2<int>    Matrix2i;
typedef TMatrix2<float>  Matrix2f;
typedef TMatrix2<double> Matrix2d;
typedef TMatrix2<float>  Matrix2;

#endif	// __TMATRIX2_H__
