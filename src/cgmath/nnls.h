#ifndef __NNLS_H__
#define __NNLS_H__

// http://en.wikipedia.org/wiki/Non-negative_least_squares


/*
Algorithm NNLS: NONNEGATIVE LEAST SQUARES

Given an m rows by n columns matrix A and a m-vector B, compute an N-vector X that solves
the least squares problem :
A * X = B with X > 0
*/

/*
A : m x n matrix
B : m matrix
X : solution vector (should be allocated)
*/
extern int nnls_solve(double* A, const int m, const int n, double *B, double *X);


// toadd ?
// http://www.jasoncantarella.com/wordpress/software/tsnnls/


#endif // __NNLS_H__