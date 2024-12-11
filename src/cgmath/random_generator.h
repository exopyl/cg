#ifndef __RANDOM_GENERATOR_H__
#define __RANDOM_GENERATOR_H__

/*
 * Code and details in "Numerical recipes in C"
 * http://www.ulib.org/webRoot/Books/Numerical_Recipes/bookc.html
 * homepage: http://www.nr.com/
 */

/* Uniform generators */
extern float ran0 (long *idum);
extern float ran1 (long *idum);
extern float ran2 (long *idum);
extern float ran3 (long *idum);

/* Special generators (based on ran1) */
extern float expdev (long *idum);                   /* Exponental deviates        */
extern float gasdev (long *idum);                   /* Normal (gaussian) deviates */
extern float gamdev (int ia, long *idum);           /* Gamma distribution         */
extern float poidev (float xm, long *idum);         /* Poisson deviates           */
extern float bnldev (float pp, int n, long *idum);  /* Binomial deviates          */

#endif /* __RANDOM_GENERATOR_H__ */
