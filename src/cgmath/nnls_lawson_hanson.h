#ifndef __NNLS_LAWSON_HANSON_H__
#define __NNLS_LAWSON_HANSON_H__


//
// References :
// original code : http://hesperia.gsfc.nasa.gov/~schmahl/nnls/nnls.c
// modifications :
// - Declaring parameters inside the declarator
//

/*  The original version of this code was developed by */
/*  Charles L. Lawson and Richard J. Hanson at Jet Propulsion Laboratory */
/*  1973 JUN 15, and published in the book */
/*  "SOLVING LEAST SQUARES PROBLEMS", Prentice-HalL, 1974. */
/*  Revised FEB 1995 to accompany reprinting of the book by SIAM. */
extern int nnls_c(
	// On entry, A[] contains the m by n matrix.
	// On exit, A[] contains the product matrix Q*A, where Q is an m by n orthogonal matrix generated implicitly by this subroutine
	double* A,

	// mda is the first dimensioning parameter for the array A[]
	int* mda,

	// dimensions m and n
	const int* m,
	const int* n,

	// On entry b[] contains the m-vector B.
	// On exit, b|] contains Q*B
	double* B,

	// On entry X[] need not to be initialized.
	// On exit X[] will contain the solution vector.
	double* X,

	// On exit, rnom contains the euclidean norm of the residual vector
	double* rnorm,

	// An N-array of working space.
	// On exit w will contain the dual solution vector.
	// w will satisfy w(I) = 0 for all I in set P and w(I) <= 0 for all I in set Z
	double* w,

	// An M-array of working space
	double* zz,

	// An integer working array of length at least N.
	// On exit the contents of this array define the sets P and Z as follows:
	//  INDEX(1)   THRU INDEX(NSETP) = SET P
	//  INDEX(IZ1) THRU INDEX(IZ2)   = SET Z
	//  IZ1 = NSETP + 1 = NPP1
	//  IZ2 = N
	int* index, 

	// success-failure flag with the following meanings:
	// 1 : The solution has been computed successfully
	// 2 : The dimensions of the problem are bad (either M .LE. 0 or N .LE. 0.)
	// 3 : Iteration count exceeded (more than 3*N iterations)
	int* mode
	);


#endif // __NNLS_LAWSON_HANSON_H__
