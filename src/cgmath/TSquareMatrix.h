#ifndef __TSQUAREMATRIX_H__
#define __TSQUAREMATRIX_H__

#include <iostream>
using namespace std;

// TSquareMatrix[n][m] addresses the following element :
// n = row
// m = column
// ie
// a[0][0] a[0][1] ... a[0][m]
// ...
// a[n][0] a[n][1] ... a[n][m]

enum solve_linear_system_method
{
  SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION = 0,
  SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN
};


template <class TValue>
class TSquareMatrix 
{
public:

	void initNULL ()
	{
		a = NULL;
		m_dimension = 0;

		d = NULL;
		v = NULL;
		nrot = 0;
	}

	void alloc (int n)
	{
		// matrix
		if (m_dimension != n)
		{
			desalloc ();
		}
		m_dimension = n;
		a = (TValue**)malloc(n*sizeof(TValue*));
		for (int i=0; i<n; i++)
			a[i] = (TValue*)malloc(n*sizeof(TValue));

		// eigensystem
		d = (TValue*)malloc(n*sizeof(TValue));
		v = (TValue**)malloc(n*sizeof(TValue*));
		for (int i=0; i<n; i++)
			v[i] = (TValue*)malloc(n*sizeof(TValue));

	}

	void desalloc ()
	{
		if (a)
		{
			for (int i=0; i<m_dimension; i++)
			{
				free (a[i]);
				a[i] = NULL;
			}
			free (a);
			a = NULL;
		}
		if (d)
		{
			free (d);
			d = NULL;
		}
		if (v)
		{
			for (int i=0; i<m_dimension; i++)
			{
				free (v[i]);
				v[i] = NULL;
			}
			free (v);
			v = NULL;
		}
	}


	//
	// Constructors
	//

	TSquareMatrix<TValue>()
	{
		initNULL ();
	}

	TSquareMatrix<TValue>(int n)
	{
		initNULL ();

		alloc (n);
		SetIdentity ();
	}


	TSquareMatrix<TValue>(int n, TValue *mat)
	{
		initNULL ();

		alloc (n);

		for (int i=0; i<n; i++)
			for (int j=0; j<n; j++)
			{
				a[i][j] = mat[n*i+j];
			}
	}

	//
	// Destructor
	//

	~TSquareMatrix<TValue>()
	{
		desalloc ();
	}

	//
	// Operators
	//
	inline TSquareMatrix<TValue>& operator=(const TSquareMatrix<TValue> &src)
	{
		alloc (src.m_dimension);
		for (int i=0; i<m_dimension; i++)
		{
			for (int j=0; j<m_dimension; j++)
			{
				a[i][j] = src.a[i][j];
			}
		}
			
		return *this;
	}

	inline bool operator==(const TSquareMatrix<TValue> &right) const
	{
	int i,j;
	bool bRet = true;
		
		for (i=0; i<4; i++) 
			for (j=0; j<4; j++) 
				if ( a[i][j] != right.a[i][j] )
					bRet = false;

	return bRet;
	}

	inline bool operator!=(const TSquareMatrix<TValue> &right) const
	{
	int i,j;
	bool bRet = true;
		
		for (i=0; i<4; i++) 
			for (j=0; j<4; j++) 
				if ( a[i][j] != right.a[i][j] )
					bRet = false;

		//Reverse the result
		bRet = !bRet;

	return bRet;
	}

	inline TSquareMatrix<TValue>& operator*=(const TSquareMatrix<TValue>& right)
	{
	TSquareMatrix<TValue> res (m_dimension);
	int i,j,k;
		
		for (i=0; i<m_dimension; i++) 
		{
			for (j=0; j<m_dimension; j++) 
			{
				res.a[i][j] = 0.0;
				for (k=0; k<m_dimension; k++) 
				{
					res.a[i][j] += a[i][k] * (right.a[k][j]);
				}
			}
		}

		*this = res;
	return *this;
	}

	TSquareMatrix<TValue> operator*(const TSquareMatrix<TValue>& right) const
	{
	TSquareMatrix<TValue> res;
	int i,j,k;
		
		for (i=0; i<m_dimension; i++) 
		{
			for (j=0; j<m_dimension; j++) 
			{
				res.a[i][j] = 0.0;
				for (k=0; k<m_dimension; k++) 
				{
					res.a[i][j] += a[i][k] * (right.a[k][j]);
				}
			}
		}

	return res;
	}

	TSquareMatrix<TValue> operator+ (const TSquareMatrix<TValue> &right)
	{
		assert (m_dimension == right.m_dimension);
		TValue *res = (TValue*) malloc (m_dimension*sizeof(TValue));
		for (int i=0; i<m_dimension; i++)
			for (int j=0; j<m_dimension; j++)
				res[m_dimension*i+j] = a[i][j] + right.a[i][j];
		return TSquareMatrix (m_dimension, res);
	}

	TSquareMatrix<TValue> operator+= (const TSquareMatrix<TValue> &right)
	{
		TSquareMatrix res = *this + right;
		*this = res;
		return *this;
	}

	TSquareMatrix<TValue> operator-  (const TSquareMatrix<TValue> &right)
	{
		assert (m_dimension == right.m_dimension);
		TValue *res = (TValue*) malloc (m_dimension*sizeof(TValue));
		for (int i=0; i<m_dimension; i++)
			for (int j=0; j<m_dimension; j++)
				res[i][j] = a[i][j] - right.a[i][j];
		return TSquareMatrix (m_dimension, res);
	}

	TSquareMatrix<TValue> operator-= (const TSquareMatrix<TValue> &right)
	{
		TSquareMatrix<TValue> res = *this - right;
		*this = res;
		return *this;
	}


	//
	// Set
	//

	inline void SetIdentity(void)
	{
		for (int i=0; i<m_dimension; i++)
			for (int j=0; j<m_dimension; j++)
			{
				a[i][j] = (i==j)? 1.0 : 0.0;
			}
	}

	inline void Set(int n, TValue *mat)
	{
		m_dimension = n;
		a = (TValue**)malloc(n*sizeof(TValue*));
		for (int i=0; i<n; i++)
			a[i] = (TValue*)malloc(n*sizeof(TValue));

		for (int i=0; i<n; i++)
			for (int j=0; j<n; j++)
			{
				a[i][j] = mat[n*i+j];
			}
	}

	inline void SetRandom(void)
	{
		for (int i=0; i<m_dimension; i++)
			for (int j=0; j<m_dimension; j++)
			{
				a[i][j] = (float)rand()/RAND_MAX;
			}
	}

	inline void SetRandomSymmetric(void)
	{
		for (int i=0; i<m_dimension; i++)
			for (int j=i; j<m_dimension; j++)
			{
				a[i][j] = (float)rand()/RAND_MAX;
				a[j][i] = a[i][j];

			}
	}

	//
	//
	//

	bool IsSymmetric (void)
	{
	  for (int i=0; i<=m_dimension; i++)
		for (int j=i+1; j<m_dimension; j++)
		  if (a[i][j] != a[j][i]) return false;
	  return true;
	}

	inline void Multiply(TValue *src, TValue *dst)
	{
	int i,j;
		
		for (i=0; i<m_dimension; i++) 
		{
			dst[i] = 0.0;
			for (j=0; j<m_dimension; j++) 
			{
				dst[i] += a[i][j]*src[j];
			}
		}
	}

	/*! Transpose the matrice
	*/
	void Transpose()
	{
		int i,j;
			
		TValue Temp;
		for ( i=0; i<m_dimension; i++ ) 
			for ( j=i+1; j<m_dimension; j++ )
			{
				Temp = a[i][j];
				a[i][j] = a[j][i];
				a[j][i] = Temp;
			}
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
					TempMatrice.a[i][j]= a[j][i];

			return (TMatrix4<TValue>)(&TempMatrice.a[0][0]);
	}

	/*!
	* Determinant
	* taken from Numerical Recipes in C (p. 49)
	* \return TValue - determinant
	*/
	inline TValue Determinant()
	{
		TValue d;

		TSquareMatrix<TValue> Temp;
		Temp = *this;

		int i,*indx = (int*)malloc(m_dimension*sizeof(int));
		Temp.ludcmp (indx,&d);
		for (i=0;i<m_dimension;i++) d *= Temp.a[i][i];
		if (indx) free (indx);
		return d;
	}


	/*! Calculate the inverse of the matrix
	Brute force a bit slow
	\param OutMatrix - In return contains the inverted matrix
	\return Returns false if there is no inverse matrix.
	*/
	inline bool GetInverse(TSquareMatrix<TValue>& OutMatrix)
	{
		TValue det = Determinant();
				
		if (det == 0.f)	//Impossible to inverse the matrix
			return false;

		TSquareMatrix<TValue> Temp1, Temp2;
		Temp1 = *this;
		Temp2 = *this;
	
		int n = m_dimension;
		int i,j;
		int *indx = (int*)malloc(n*sizeof(int));
		TValue d;
		TValue *col = (TValue*)malloc(n*sizeof(TValue));
		Temp2.ludcmp (indx, &d);
		for (j=0; j<n; j++)
		{
			for (i=0; i<n; i++) col[i] = 0.0;
			col[j] = 1.0;
			Temp2.lubksb (indx, col);
			for (i=0; i<n; i++) Temp1.a[i][j] = col[i];
		}

		if (indx) free (indx);
		if (col) free (col);

		OutMatrix = Temp1;
		return true;
	}

	/*! Calculate the inverse of the current matrix and assign
	the result to our object
	\return Returns false if there is no inverse matrix.
	*/
	inline bool SetInverse()
	{
		TSquareMatrix<TValue> OutMatrix;

		bool bRet = GetInverse( OutMatrix );

		*this = OutMatrix;
		return bRet;
	}

	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// 
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	/*!
	* LU decomposition
	* from Numerical Recipes in C (p. 46)
	*
	* It is used by the LU decomposition as a method to solve a linear system.
	*/
	#define TINY 1.0e-20;
	void ludcmp (int *indx, TValue *d)
	{
		int n = m_dimension;
	  int i,imax,j,k;
	  TValue big,dum,sum,temp;
	  TValue *vv;
	  
	  vv=(TValue*)malloc(n*sizeof(TValue));
	  *d=1.0;
	  for (i=0;i<n;i++) {
		big=0.0;
		for (j=0;j<n;j++)
		  if ((temp=fabs(a[i][j])) > big) big=temp;
		if (big == 0.0) printf ("Singular matrix in routine ludcmp\n");
		vv[i]=1.0/big;
	  }
	  for (j=0;j<n;j++) {
		for (i=0;i<j;i++) {
		  sum=a[i][j];
		  for (k=0;k<i;k++) sum -= a[i][k]*a[k][j];
		  a[i][j]=sum;
		}
		big=0.0;
		for (i=j;i<n;i++) {
		  sum=a[i][j];
		  for (k=0;k<j;k++)
		sum -= a[i][k]*a[k][j];
		  a[i][j]=sum;
		  if ( (dum=vv[i]*fabs(sum)) >= big) {
		big=dum;
		imax=i;
		  }
		}
		if (j != imax) {
		  for (k=0;k<n;k++) {
		dum=a[imax][k];
		a[imax][k]=a[j][k];
		a[j][k]=dum;
		  }
		  *d = -(*d);
		  vv[imax]=vv[j];
		}
		indx[j]=imax;
		if (a[j][j] == 0.0) a[j][j]=TINY;
		if (j != n) {
		  dum=1.0/(a[j][j]);
		  for (i=j+1;i<n;i++) a[i][j] *= dum;
		}
	  }
	  free (vv);
	}
	#undef TINY

	/**
	* forward substitution and backsubstitution
	* taken from Numerical Recipes in C (p. 47)
	*
	* It is used by the LU decomposition as a method to solve a linear system.
	*/
	void lubksb(int *indx, TValue b[])
	{
	  int i,ii=-1,ip,j;
	  int n = m_dimension;
	  TValue sum;

	  for (i=0;i<n;i++) {
		ip=indx[i];
		sum=b[ip];
		b[ip]=b[i];
		if (ii>-1)
		  for (j=ii;j<=i-1;j++) sum -= a[i][j]*b[j];
		else if (sum) ii=i;
		b[i]=sum;
	  }
	  for (i=n-1;i>=0;i--) {
		sum=b[i];
		for (j=i+1;j<n;j++) sum -= a[i][j]*b[j];
		b[i]=sum/a[i][i];
	  }
	}

	/*!
	* solve linear system gauss jordan
	* from Numerical Recipes in C (p. 39)
	*/
	void gaussj (TValue *b, int m)
	{
		int n = m_dimension;
	  int *indxc,*indxr,*ipiv;
	  int i,icol,irow,j,k,l,ll;
	  TValue big,dum,pivinv,temp;
	  
	  indxc = (int*)malloc(n*sizeof(int));
	  indxr = (int*)malloc(n*sizeof(int));
	  ipiv  = (int*)malloc(n*sizeof(int));
	  for (j=0;j<n;j++) ipiv[j]=0;
	  for (i=0;i<n;i++) {
		big=0.0;
		for (j=0;j<n;j++)
		  if (ipiv[j] != 1)
		for (k=0;k<n;k++) {
		  if (ipiv[k] == 0) {
			if (fabs(a[j][k]) >= big) {
			  big=fabs(a[j][k]);
			  irow=j;
			  icol=k;
			}
		  } else if (ipiv[k] > 1) printf ("Singular Matrix-1 in solve linear system gauss\n");
		}
		++(ipiv[icol]);
		if (irow != icol) {
			for (l=0;l<n;l++) SWAP(a[irow][l],a[icol][l]);
			SWAP(b[irow],b[icol]);
							}
		indxr[i]=irow;
		indxc[i]=icol;
		if (a[icol][icol] == 0.0) printf ("Singular Matrix-2 in solve linear system gauss\n");
		pivinv=1.0/a[icol][icol];
		a[icol][icol]=1.0;
		for (l=0;l<n;l++) a[icol][l] *= pivinv;
		b[icol] *= pivinv;
		for (ll=0;ll<n;ll++)
		  if (ll != icol) {
		dum=a[ll][icol];
		a[ll][icol]=0.0;
		for (l=0;l<n;l++) a[ll][l] -= a[icol][l]*dum;
		b[ll] -= b[icol]*dum;
		  }
	  }
	  for (l=n-1;l>=0;l--) {
		if (indxr[l] != indxc[l])
		  for (k=0;k<n;k++)
		SWAP(a[k][indxr[l]],a[k][indxc[l]]);
	  }
	  free (ipiv);
	  free (indxr);
	  free (indxc);
	}
	#undef SWAP

	/**
	* Solve the linear system using the Gauss Jordan method.
	*/
	void solve_linear_system_gauss_jordan (TValue *right, TValue *result)
	{
	  int i;
		TSquareMatrix<TValue> Temp;
		Temp = *this;
	  
	  // shift the right element
	  TValue *b = (TValue*)malloc(m_dimension*sizeof(TValue));
	  for (i=0; i<m_dimension; i++)
		{
		  b[i] = right[i];
		}
	  
	  gaussj (b, 0);
	  
	  // init the result
	  if (!result) result = (TValue*)malloc(m_dimension*sizeof(TValue));
	  for (i=0; i<m_dimension; i++) result[i] = b[i];
	  
	  if (b)
		{
		  free (b);
		}
	  
		*this = Temp;

	}

	/**
	* solve linear system lu decomposition
	* taken from Numerical Recipes in C (p. 48)
	*/
	void solve_linear_system_lu_decomposition (TValue *right, TValue *result)
	{
	  int i;
	  
		TSquareMatrix<TValue> Temp;
		Temp = *this;
	  
	  int *indx = (int*)malloc(m_dimension*sizeof(int));
	  TValue d;
	  ludcmp (indx, &d);
	  lubksb (indx, right);
	  
	  // init the result
	  if (!result) result = (TValue*)malloc(m_dimension*sizeof(TValue));
	  for (i=0; i<m_dimension; i++) result[i] = right[i];
	  
	  if (indx) free (indx);
	  
		*this = Temp;
	}

	/*!
	* Solve the linear system.
	* \param right : .
	* \param result : .
	* \param id : the method used to solve it.
	* SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN
	*
	* The matrix is modified
	*/
	int SolveLinearSystem (TValue *right, TValue *result, solve_linear_system_method id)
	{
	  if (!Determinant ())
		{
		  printf ("can't solve linear system (determinant is nil)\n");
		  return 0;
		}
	  
	  switch (id)
		{
		case SOLVE_LINEAR_SYSTEM_LU_DECOMPOSITION:
		  solve_linear_system_lu_decomposition (right, result);
		  break;
		case SOLVE_LINEAR_SYSTEM_GAUSS_JORDAN:
		  solve_linear_system_gauss_jordan (right, result);
		  break;
		default:
		  break;
		}
	  return 1;
	}

	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// Eigensystem
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	#define ROTATE(a,i,j,k,l) g=a[i][j];h=a[k][l];a[i][j]=g-s*(h+g*tau);a[k][l]=h+s*(g-h*tau);
	void jacobi ()
	{
	  int n = m_dimension;
	  int j,iq,ip,i;
	  TValue tresh,theta,tau,t,sm,s,h,g,c,*b,*z;
	  b = (TValue*)malloc(n*sizeof(TValue));
	  z = (TValue*)malloc(n*sizeof(TValue));
	  for (ip=0;ip<n;ip++) {
		for (iq=0;iq<n;iq++)
		  v[ip][iq]=0.0;
		v[ip][ip]=1.0;
	  }
	  for (ip=0;ip<n;ip++) {
		b[ip]=d[ip]=a[ip][ip];
		z[ip]=0.0;
	  }
	  nrot=0;
	  for (i=0;i<100;i++) {
		sm=0.0;
		for (ip=0;ip<n-1;ip++) {
		  for (iq=ip+1;iq<n;iq++)
		sm += fabs(a[ip][iq]);
		}
		if (sm == 0.0) {
		  free (z);
		  free (b);
		  return;
		}
		if (i < 4)
		  tresh=0.2*sm/(n*n);
		else
		  tresh=0.0;
		for (ip=0;ip<n-1;ip++) {
		  for (iq=ip+1;iq<n;iq++) {
		g=100.0*fabs(a[ip][iq]);
		if (i > 4 && (TValue)(fabs(d[ip])+g) == (TValue)fabs(d[ip])
			&& (TValue)(fabs(d[iq])+g) == (TValue)fabs(d[iq]))
		  a[ip][iq]=0.0;
		else if (fabs(a[ip][iq]) > tresh) {
		  h=d[iq]-d[ip];
		  if ((TValue)(fabs(h)+g) == (TValue)fabs(h))
			t=(a[ip][iq])/h;
		  else {
			theta=0.5*h/(a[ip][iq]);
			t=1.0/(fabs(theta)+sqrt(1.0+theta*theta));
			if (theta < 0.0) t = -t;
		  }
		  c=1.0/sqrt(1+t*t);
		  s=t*c;
		  tau=s/(1.0+c);
		  h=t*a[ip][iq];
		  z[ip] -= h;
		  z[iq] += h;
		  d[ip] -= h;
		  d[iq] += h;
		  a[ip][iq]=0.0;
		  for (j=0;j<=ip-1;j++) {
			ROTATE(a,j,ip,j,iq)
			  }
		  for (j=ip+1;j<=iq-1;j++) {
			ROTATE(a,ip,j,j,iq)
			  }
		  for (j=iq+1;j<n;j++) {
			ROTATE(a,ip,j,iq,j)
			  }
		  for (j=0;j<n;j++) {
			ROTATE(v,j,ip,j,iq)
			  }
		  ++nrot;
		}
		  }
		}
		for (ip=0;ip<n;ip++) {
		  b[ip] += z[ip];
		  d[ip] =  b[ip];
		  z[ip] =  0.0;
		}
	  }
	  printf ("Too many iterations in routine jacobi\n");
	}
	#undef ROTATE

	void sort(void)
	{
	  int k,j,i;
	  TValue p;
	  int n = m_dimension;
	  for (i=0;i<n-1;i++)
	  {
		p=d[k=i];
		for (j=i+1;j<n;j++)
		  if (d[j] >= p) p=d[k=j];
		if (k != i)
		{
		  d[k]=d[i];
		  d[i]=p;
		  for (j=0;j<n;j++)
		  {
			p=v[j][i];
			v[j][i]=v[j][k];
			v[j][k]=p;
		  }
		}
	  }
	}

	/*! Solve the eigensystem
	\return Returns false if the eigensystem can't be solved.
	*/
	bool SolveEigenSystem (void)
	{
		TSquareMatrix<TValue> Temp;
		Temp = *this;
		
		Temp.jacobi ();
		Temp.sort ();

		memcpy (d, Temp.d, m_dimension*sizeof(TValue));
		for (int i=0; i<m_dimension; i++)
		{
			memcpy (v[i], Temp.v[i], m_dimension*sizeof(TValue));
		}

		return true;
	}

	void GetEigenValues (TValue *eigenvalues)
	{
		for (int i=0; i<m_dimension; i++)
		{
			eigenvalues[i] = d[i];
		}
	}

	void GetEigenVector (int index, TValue *eigenvector)
	{
		for (int i=0; i<m_dimension; i++)
		{
			eigenvector[i] = v[i][index];
		}
	}

//

	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline TValue *GetMatPtr()
	{
		return &a[0][0];
	}

	/*! Return a pointer to the array of value used as our matrix
	\return TValue* - Pointer to a linear array of value
	*/
	inline operator TValue* ()
	{
		return &a[0][0];
	}   

	//
	// IOstream
	//

	friend ostream & operator << ( ostream & out, const TSquareMatrix<TValue> &right)
	{
		for (int i=0; i<right.m_dimension; i++)
		{
			for (int j=0; j<right.m_dimension; j++)
			{
				out << right.a[i][j] << " ";
			}
			out << endl;
		}
		return out;
	}

public:

  int m_dimension;
  TValue **a;


  // eigensystem
  int nrot;
  TValue *d; // eigenvalues
  TValue **v; // eigenvectors
};

//typedef TSquareMatrix<int>    SquareMatrixi;
typedef TSquareMatrix<float>  SquareMatrixf;
typedef TSquareMatrix<double> SquareMatrixd;
typedef TSquareMatrix<float>  SquareMatrix;

#endif	// __TSQUAREMATRIX_H__
