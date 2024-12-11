#ifndef __BERSTEIN_H__
#define __BERSTEIN_H__

#include <math.h>

extern double factorial (unsigned int n); // factorial
extern int    binomialCoefficient (int n, int k); // binomial coefficient
extern double bersteinPolynomial (int par_maxDegree, int par_degree, float par_value); // Berstein polynomial

#endif /* __BERSTEIN_H__ */
