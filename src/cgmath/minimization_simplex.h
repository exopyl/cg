#ifndef __MINIMIZATION_SIMPLEX_H__
#define __MINIMIZATION_SIMPLEX_H__

/* example

   float **simplex;
   simplex = (float**)malloc(4*sizeof(float*));
   for (i=1; i<=3; i++)
   simplex[i] = (float*)malloc(3*sizeof(float));
   float y[4];
   int nfunk;
   
   simplex[1][1] = 3.0;  simplex[1][2] = 6.0;
   simplex[2][1] = 3.1;  simplex[2][2] = 6.0;
   simplex[3][1] = 3.0;  simplex[3][2] = 6.1;
   y[1] = function (simplex[1]);
   y[2] = function (simplex[2]);
   y[3] = function (simplex[3]);
   printf ("%f %f %f\n", simplex[1][1], simplex[1][2], y[1]);
   printf ("%f %f %f\n", simplex[2][1], simplex[2][2], y[2]);
   printf ("%f %f %f\n", simplex[3][1], simplex[3][2], y[3]);
   
   amoeba (simplex, y, 2, 0.0001, &function, &nfunk);
   printf ("Nelder Mead ---> %f %f %f\n", simplex[1][1], simplex[1][2], function(simplex[1]));
*/

extern void amoeba (float **p, float y[], int ndim, float ftol, float (*funk)(float []), int *nfunk);

#endif /* __MINIMIZATION_SIMPLE_H__ */
