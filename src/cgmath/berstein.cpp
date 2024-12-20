#include "berstein.h"

// factorials until 20!
//long factorials[21] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800,
//					39916800, 479001600, 6227020800, 87178291200, 1307674368000, 20922789888000, 355687428096000,
//					6402373705728000, 121645100408832000, 2432902008176640000};
long factorials[11] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};

/**
* factorial
*/
double
factorial (unsigned int n)
{
	return (n >= 21)? (factorial(n) * factorial(n-1)) : factorials[n];
}

// binomial coefficient (n,k) with n>=k. If n<k, the coefficient is invalid => returns -1.0
static int binomialCoefficients[21][21] = {{1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 3, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 4, 6, 4, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 5, 10, 10, 5, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 15, 20, 15, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 7, 21, 35, 35, 21, 7, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 8, 28, 56, 70, 56, 28, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 36, 84, 126, 126, 84, 36, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 10, 45, 120, 210, 252, 210, 120, 45, 10, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 11, 55, 165, 330, 462, 462, 330, 165, 55, 11, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 12, 66, 220, 495, 792, 924, 792, 495, 220, 66, 12, 1, -1, -1, -1, -1, -1, -1,-1, -1},
{1, 13, 78, 286, 715, 1287, 1716, 1716, 1287, 715, 286, 78, 13, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 14, 91, 364, 1001, 2002, 3003, 3432, 3003, 2002, 1001, 364, 91, 14, 1, -1, -1, -1, -1, -1, -1},
{1, 15, 105, 455, 1365, 3003, 5005, 6435, 6435, 5005, 3003, 1365, 455, 105, 15, 1, -1, -1, -1, -1, -1},
{1, 16, 120, 560, 1820, 4368, 8008, 11440, 12870, 11440, 8008, 4368, 1820, 560, 120, 16, 1, -1, -1, -1, -1},
{1, 17, 136, 680, 2380, 6188, 12376, 19448, 24310, 24310, 19448, 12376, 6188, 2380, 680, 136, 17, 1, -1, -1, -1},
{1, 18, 153, 816, 3060, 8568, 18564, 31824, 43758, 48620, 43758, 31824, 18564, 8568, 3060, 816, 153, 18, 1, -1, -1},
{1, 19, 171, 969, 3876, 11628, 27132, 50388, 75582, 92378, 92378, 75582, 50388, 27132, 11628, 3876, 969, 171, 19, 1, -1},
{1, 20, 190, 1140, 4845, 15504, 38760, 77520, 125970, 167960, 184756, 167960, 125970, 77520, 38760, 15504, 4845, 1140, 190, 20, 1}};

/**
* binomial coefficient
*/
int binomialCoefficient (int n, int k)
{
	return binomialCoefficients[n][k];
	//return (factorial(n) / (factorial(k) * factorial(n-k)));
}

/**
* Berstein polynomial
*/
double bersteinPolynomial (int par_maxDegree, int par_degree, float par_value)
{
	return binomialCoefficient (par_maxDegree, par_degree) * pow (par_value, par_degree) * pow (1-par_value,par_maxDegree-par_degree);
}

