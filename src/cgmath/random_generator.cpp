#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "random_generator.h"
#include "common.h"

/* Static methods */

/* gammln: returns the value ln[Tau (xx)] for xx > 0 */
/*
 * Internal arithmetic will be done in double precision, a nicety that you can omit
 * if five-figure accuracy is good enough.
 */
static float
gammln (float xx)
{
  double x,y,tmp,ser;
  static double cof[6]={76.18009172947146,
			-86.50532032941677,
			24.01409824083091,
			-1.231739572450155,
			0.1208650973866179e-2,
			-0.5395239384953e-5};
  int j;

  y=x=xx;
  tmp=x+5.5;
  tmp -= (x+0.5)*log(tmp);
  ser=1.000000000190015;
  for (j=0;j<=5;j++) ser += cof[j]/++y;
  return -tmp+log(2.5066282746310005*ser/x);
}





/* minimal standard generator */
/*
 * "Minimal" random number generator of Park and Miller.
 * Returns a uniform random deviate between 0.0 and 1.0.
 * Set or reset idum to any  integer value (except the
 * unlikely value MASK) to initialize the sequence; idum
 * must not be altered between calls for successive deviates
 * in a sequence
 */
#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define MASK 123459876

float
ran0 (long *idum)
{
  long k;
  float ans;

  *idum ^= MASK;   // XORing with MASK allows use of zero and other
  k = (*idum)/IQ;  // simple bit patterns for idum
  *idum = IA*(*idum-k*IQ)-IR*k;  // compute idum=(IA*idum)%IM without
  if (*idum < 0) *idum += IM;    // overflows by Scharge's method
  ans = AM*(*idum);    // convert idum ti a floating result
  *idum ^= MASK;    // unmask before return
  return ans;
}


/* 
 * minimal standard generator
 * with Bays-Durham shuffle and added safeguards
 */
/*
 * "Minimal" random number generator of Park and Miller with
 * Bays-Durham shuffle and added safeguards.
 * returns a uniform random deviate betwen 0.0 and 1.0 (exclusive
 * of the endpoint values). Call with idum a negative integer to
 * initialize; thereafter, do not alter idum between successive
 * deviates in a sequence. RNMX should approximate the largest
 * floating value that is less than 1.
 */
#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float
ran1 (long *idum)
{
  int j;
  long k;
  static long iy=0;
  static long iv[NTAB];
  float temp;

  if (*idum <= 0 || !iy)  // initialize
  {
    if (-(*idum) < 1) *idum=1;   // be sure to prevent idum = 0
    else *idum = -(*idum);
    for (j=NTAB+7; j>=0; j--) // load the shuffle table (after 8 wam-ups)
    {
      k=(*idum)/IQ;
      *idum=IA*(*idum-k*IQ)-IR*k;
      if (*idum < 0) *idum += IM;
      if (j < NTAB) iv[j] = *idum;
    }
    iy = iv[0];
  }
  k=(*idum)/IQ;  // start here when not initializing
  *idum=IA*(*idum-k*IQ)-IR*k;  // compute idum=(IA*idum)%IM without
  if (*idum < 0) *idum += IM;  // overflows by Schrage's method
  j = iy/NDIV;    // will be in the range 0..NTAB-1
  iy = iv[j];     // output previously stored value and refill
  iv[j] = *idum;  // the shuffle table
  if ((temp=AM*iy) > RNMX) return RNMX; // because users don't expect endpoint values
  else return temp;
}



/*
 * Long period (> 2x10^18) random number generator of L'Ecuyer with
 * Bays-Durham shuffle and added safeguards.
 * Returns a uniform random deviate between 0.0 and 1.0 (exclusive of
 * the ednpoint values). Call with idum a negative integer to initialize;
 * thereafter, do not alter idum between succesive deviates in a
 * sequence. RNMX should approximate the largest floating value that
 * is less than 1.
 */
#define IM1 2147483563
#define IM2 2147483399
#define AM1 (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014
#define IA2 40692
#define IQ1 53668
#define IQ2 52774
#define IR1 12211
#define IR2 3791
#define NTAB 32
#define NDIV1 (1+IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

float
ran2 (long *idum)
{
  int j;
  long k;
  static long idum2 = 123456789;
  static long iy = 0;
  static long iv[NTAB];
  float temp;

  if (*idum <= 0)  // initialize
  {
    if (-(*idum) < 1) *idum=1;  // be sure to prevent idum = 0
    else *idum = -(*idum);
    idum2=(*idum);
    for (j=NTAB+7; j>=0; j--)// load the shuffle table (after 8 warm-ups)
    {
      k=(*idum)/IQ1;
      *idum=IA1*(*idum-k*IQ1)-k*IR1;
      if (*idum < 0) *idum += IM1;
      if (j < NTAB) iv[j] = *idum;
    }
    iy=iv[0];
  }
  k=(*idum)/IQ1;   // start here when not initializing
  *idum=IA1*(*idum-k*IQ1)-k*IR1; // compute idum=(IA1*idum)%IM1 without
  if (*idum < 0) *idum += IM1;   // overflows by Schrage's method
  k=idum2/IQ2;
  idum2=IA2*(idum2-k*IQ2)-k*IR2; // compute idum2=(IA2*idum)%IM2 likewise
  if (idum2 < 0) idum2 += IM2;
  j=iy/NDIV1;   // will be  in the range 0..NTAB-1
  iy=iv[j]-idum2; // here idum is shuffled, idum and idum2 are
  iv[j] = *idum;  // combined to generate output
  if (iy < 1) iy += IMM1;
  if ((temp=AM1*iy) > RNMX) return RNMX; // because users don't expoect endpoint values
  else return temp;
}


/*
 * Returns a uniform random deviate between 0.0 and 1.0.
 * Set idum to any negative value to initialize or reinitialize
 * the sequence
 */
#define MBIG 1000000000
#define MSEED 161803398
#define MZ 0
#define FAC (1.0/MBIG)
/* According to Knuth, any large MBIG, and any smaller (but still large)
   MSEED can be substituted for the above values*/

float
ran3 (long *idum)
{
  static int inext,inextp;
  static long ma[56];  // the value 56 (rang ma[1..55]) is special
  static int iff=0;    // and should not be modified; see Knuth
  long mj,mk;
  int i,ii,k;

  if (*idum < 0 || iff == 0)  // initialization
  {
    iff=1;
    mj=labs(MSEED-labs(*idum));  // initialize ma[55] using the seed idum
    mj %= MBIG;                  // and the large number MSEED
    ma[55]=mj;
    mk=1;
    for (i=1;i<=54;i++) // now initialize the rest of the table
    {                   // in a slightly random order,
      ii=(21*i) % 55;   // with numbers that are note especially random
      ma[ii]=mk;
      mk=mj-mk;
      if (mk < MZ) mk += MBIG;
      mj=ma[ii];
    }
    for (k=1;k<=54;k++)   // we randomize them by "warming up the
      for (i=1;i<=55;i++) // generator"
      {
	ma[i] -= ma[1+(i+30) % 55];
	if (ma[i] < MZ) ma[i] += MBIG;
      }
    inext=0;    // prepare indices for our first generated number
    inextp=31;  // the constant 31 is special; see Knuth
    *idum=1;
  }
  // Here is where we start, except on initialization
  if (++inext == 56) inext=1;   // increment inext and inextp, wrapping
  if (++inextp == 56) inextp=1; // around 56 to 1
  mj=ma[inext]-ma[inextp];  // generate a new random number subtractively
  if (mj < MZ) mj += MBIG;  // be sure that it is in range
  ma[inext]=mj;   // store it
  return mj*FAC;  // and output the derived uniform deviate
}




/***************************/
/*** Exponental deviates ***/
/***************************/
/*
 * Returns an exponentially distributed, positive, random deviate of
 * unit mean, using ran1(idum) as the source of uniform deviates
 */
float
expdev (long *idum)
{
  float ran1(long *idum);
  float dum;

  do
    dum=ran1(idum);
  while (dum == 0.0);
  return  -log(dum);
}


/**********************************/
/*** Normal (gaussian) deviates ***/
/**********************************/
/*
 * Returns a normally distributed deviate with zero mean and unit
 * variance, using ran1(idum) as the source of uniform deviates
 */
float
gasdev (long *idum)
{
  float ran1(long *idum);
  static int iset=0;
  static float gset;
  float fac,rsq,v1,v2;

  //if (*idum < 0) iset=0;  // reinitialize
  if(iset == 0){     // we don't have an extra deviate handy, so
    do{
      v1=2.0*ran1(idum)-1.0; // pick two uniform numbers in the square
      v2=2.0*ran1(idum)-1.0; // extending from -1 to +1 in each direction
      rsq=v1*v1+v2*v2;    // see if they are in the unit circle
    } while(rsq >= 1.0 || rsq == 0.0);  // and if they are not, try again
    fac=sqrt(-2.0*log(rsq)/rsq);
    // now make the Box-Muller transformation to get two normal deviates
    // return one and save the other for next time
    gset=v1*fac;
    iset=1;   // set flag
    return v2*fac;
  } else {       // we have an extra deviate handy,
    iset=0;      // so unset the flag,
    return gset; // and return it
  }
}

/**************************/
/*** Gamma distribution ***/
/**************************/
/*
 * Returns a deviate distributed as a gamma distribution of integer
 * order ia, i.e., a waiting time to the iath event in a Poisson
 * process of unit mean, using ran1(idum) as the source of uniform
 * deviates
 */
float
gamdev (int ia, long *idum)
{
  float ran1(long *idum);
  int j;
  float am,e,s,v1,v2,x,y;

  if (ia < 1) printf ("Error in routine gandev\n");
  if (ia < 6)  // use direct method, adding waiting time
  {
    x=1.0;
    for (j=1;j<=ia;j++) x *= ran1(idum);
    x = -log(x);
  }
  else {   // use rejection methode
    do {
      do {
	do {     // these four lines generate the tangent of
	  v1=ran1(idum);  // a random angle, i.e., they are
	  v2=2.0*ran1(idum)-1.0;  // equivalent to
	} while (v1*v1+v2*v2 > 1.0);  // y = tan(PI * ran1(idum))
	y=v2/v1;
	am=ia-1;
	s=sqrt(2.0*am+1.0);
	x=s*y+am;           // we decide whether to reject x:
      } while (x <= 0.0);   // reject in region of zero probability
      e=(1.0+y*y)*exp(am*log(x/am)-s*y); // ratio of prob. fn. to comparison fn.
    } while (ran1(idum) > e);  // reject on basis of a second uniform deviate
  }
  return x;
}

/************************/
/*** Poisson deviates ***/
/************************/
#define PI 3.141592654

/*
 * Returns as a floating-point number an integer value that is a random
 * deviate drawn from a Poisson distribution of mean xm, using
 * ran1(idum) as a source of uniform random deviates.
 */
float
poidev (float xm, long*idum)
{
  float gammln(float xx);
  float ran1(long *idum);
  static float sq,alxm,g,oldm=(-1.0);  // oldm is a flag for whether xm has changed
  float em,t,y;                        // since last call

  if (xm < 12.0) {        // use direct method
    if (xm != oldm) {
      oldm=xm;
      g=exp(-xm);
    }
    em = -1.0;
    t = 1.0;
    do {                // instead of adding exponential deviates it is equivalent
      ++em;             // to multiply uniform deviates. We never actually have to
      t *= ran1(idum);  // take the log, merely compare to the pre-computed
    } while (t > g);    // exponential
  } else {            // use rejection method
    if (xm != oldm) {   // if xm has changed since the last call, then pre-compute
      oldm=xm;          // some functions that occur below
      sq=sqrt(2.0*xm);
      alxm=log(xm);
      g=xm*alxm-gammln(xm+1.0);
      // the function gammln is the natural log of the gamma function
    }
    do {
      do {             // y is a deviate from a Lorentzian comparison function
	y=tan(PI*ran1(idum));
	em=sq*y+xm;    // em is y, shifted and scaled
      } while (em < 0.0);   // reject if in regime of zero probability
      em=floor(em);         // the trick for integer-valued distributions
      t=0.9*(1.0+y*y)*exp(em*alxm-gammln(em+1.0)-g);
      // the ratio of the desired distribution to the comparison function; we accept
      // or reject by comparing it to another uniform deviate. The factor 0.9 is
      // chosen so that t never exceeds 1.
    } while (ran1(idum) > t);
  }
  return em;
}


/*************************/
/*** Binomial deviates ***/
/*************************/
#define PI 3.141592654

/*
 * Returns as a floating-point number an integer value that is a random
 * deviate drawn from a binomial distribution of n trials each of probability
 * pp, using ran1(idum) as a source of uniform random deviates.
 */
float
bnldev (float pp, int n, long *idum)
{
  float gammln(float xx);
  float ran1(long *idum);
  int j;
  static int nold=(-1);
  float am,em,g,angle,p,bnl,sq,t,y;
  static float pold=(-1.0),pc,plog,pclog,en,oldg;

  p=(pp <= 0.5 ? pp : 1.0-pp);
  // The binomial distribution is invariant under changing pp to 1-pp,
  // if we also change the answer to n minus itself; we'll remember to do this below
  am=n*p;         // this is the mean of the deviate to be produced
  if (n < 25) {   // use the direct method while n is not too large
    bnl=0.0;      // this can require up to 25 calls to ran1
    for (j=1;j<=n;j++)
      if (ran1(idum) <p) ++bnl;
  } else if (am < 1.0) {  // if fewer than one event is expected out of 25
    g=exp(-am);           // or more trials, then the distribution is quite
    t=1.0;                // accurately Poisson. Use direct Poisson direct
    for (j=0;j<=n;j++) {
      t *= ran1(idum);
      if (t < g) break;
    }
    bnl=(j <= n ? j : n);
  } else {                // Use the rejection method
    if (n != nold) {    // if n has changed, then compute useful quantities
      en=n;
      oldg=gammln(en+1.0);
      nold=n;
    } if (p != pold) {  // if p has changed, then compute useful quandtities
      pc=1.0-p;
      plog=log(p);
      pclog=log(pc);
      pold=p;
    }
    sq=sqrt(2.0*am*pc);  // the following code should by now seem familiar
    do {                 // rejection method with a Lorentzian comparison function
      do {
	angle=PI*ran1(idum);
	y=tan(angle);
	em=sq*y+am;
      } while (em < 0.0 || em >= (en+1.0));  // reject
      em=floor(em);         // trick for integer-valued distribution
      t=1.2*sq*(1.0+y*y)*exp(oldg-gammln(em+1.0)
			     -gammln(en-em+1.0)+em*plog+(en-em)*pclog);
    } while (ran1(idum) > t);   // reject. This happens about 1.5 times per deviate,
    bnl=em;                     // on average
  }
  if (p != pp) bnl=n-bnl;   // remember to undo the symmetry transformation
  return bnl;
}

