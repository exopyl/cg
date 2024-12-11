#ifndef __MINIMIZATION_SIMULATED_ANNEALING_H__
#define __MINIMIZATION_SIMULATED_ANNEALING_H__

/* example
  
   float **simplex;
   simplex = (float**)malloc(4*sizeof(float*));
   for (i=1; i<=3; i++)
   simplex[i] = (float*)malloc(3*sizeof(float));
   float y[4];
   
   simplex[1][1] = 3.0;  simplex[1][2] = 6.0;
   simplex[2][1] = 3.1;  simplex[2][2] = 6.0;
   simplex[3][1] = 3.0;  simplex[3][2] = 6.1;
   y[1] = function (simplex[1]);
   y[2] = function (simplex[2]);
   y[3] = function (simplex[3]);
   
   float yb[4];
   float pb[4];
   int iter = 100;
   float temptr = 100.0;
   for (i=1; i<1000; i++)
   {
   amebsa(simplex, y, 2, pb, yb, 0.0001, &function, &iter, temptr);
   temptr = (1.0-0.7)*temptr;
   iter = 100;
   }
   printf ("Simulated annealing ---> %f %f %f\n", simplex[1][1], simplex[1][2], function(simplex[1]));
*/

extern void amebsa(float **p, float y[], int ndim, float pb[], float *yb, float ftol,
		   float (*funk)(float []), int *iter, float temptr);

#endif /* __MINIMIZATION_SIMULATED_ANNEALING_H__ */
