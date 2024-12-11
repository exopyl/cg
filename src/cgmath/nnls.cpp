#include <stdlib.h>
#include <stdio.h>

#include "nnls.h"
#include "nnls_lawson_hanson.h"

int nnls_solve(double* A, int m, int n, double *B, double *X)
{
	int mode;
	int tmda = m; // ? 0 or m ?
	const int tm = m;
	const int tn = n;
	double rnorm;
	double *w = (double*)malloc(n*sizeof(double));
	double *zz = (double*)malloc(m*sizeof(double));
	int *index = (int*)malloc(10*n*sizeof(int));

	nnls_c (A, &tmda, &tm, &tn, B, X, &rnorm, w, zz, index, &mode);
	printf ("rnorm : %.6f\n", rnorm);

	printf ("mode = %d\n", mode);
	printf ("w : ");
	for (int i=0; i<n; i++)
		printf ("%.3f ", w[i]);
	printf ("\n");
	printf ("B : ");
	for (int i=0; i<n; i++)
		printf ("%.3f ", B[i]);
	printf ("\n");
	printf ("mda = %d\n", tmda);

	// cleaning
	free (index);
	free (zz);
	free (w);
	
	return mode;
}
