#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#include "../src/cgmath/cgmath.h"

// ref : http://cran.r-project.org/web/packages/nnls/nnls.pdf
static double Xcolfunc (double wavenum, double location, double delta)
{
	double t = 2*(wavenum - location)/delta;
	double t2 = t*t;
	return exp(-log(2.f) * t2);
}

TEST(TEST_cgmath_nnls, nnls3)
{
	int i,j;
	double e;
	int m = 51;
	int n = 3;

	// simulate a matrix A
	// with 3 columns, each containig an exponential decay
	double *t = (double*)malloc(m*sizeof(double));
	for (i=0,e=0; e<=2.; e+=.04,i++)
		t[i] = e;
	double k[3] = {.5, .6, 1.};

	double *A = (double*)malloc(m*n*sizeof(double));
	memset (A, 0, m*n*sizeof(double));
	for (j=0; j<m; j++)
		for (i=0; i<n; i++)
			A[j*n+i] = exp(-k[i]*t[j]);

	// simulate a matrix X
	// with 3 columns, each containing a Gaussian shape
	// the Gaussian shapes are non-negative
	double *wavenum = (double*)malloc(m*sizeof(double));
	for (i=0,e=18000; e<=28000; e+=200,i++)
		wavenum[i] = e;
	double location[3] = {25000, 22000, 20000};
	double delta[3] = {3000, 3000, 3000};

	double *X = (double*)malloc(m*n*sizeof(double));
	memset (X, 0, m*n*sizeof(double));
	for (j=0; j<m; j++)
		for (i=0; i<n; i++)
			X[j*n+i] = Xcolfunc(wavenum[j], location[i], delta[i]);

	// set seed for reproducibility
	srand(3300);

	// simulated data is the product of A and X with some
	// spherical Gaussian noise added
	double *matdat = (double*)malloc(m*n*sizeof(double));
	memset (matdat, 0, m*n*sizeof(double));
	for (j=0; j<m; j++)
		for (i=0; i<n; i++)
			matdat[j*n+i] = A[j*n+i] * t[j] + .005*(double)rand()/RAND_MAX;

	// estimate the rows of X using NNLS criteria
	double *XX = (double*)malloc(m*n*sizeof(double));
	memset (XX, 0, m*n*sizeof(double));
	double *X_nnls = (double*)malloc(n*sizeof(double));
	for (j=0; j<m; j++)
		for (i=0; i<n; i++)
			XX[j*n+i] = nnls_solve (A, m, n, matdat, X_nnls);


	for (i=0; i<n; i++)
		printf ("%f ", X_nnls[i]);
	printf ("\n");
}
