#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "minimization_simplex.h"

static float
amotry (float **p, float y[], float psum[], int ndim, float (*funk)(float []), int ihi, float fac)
{
  int j;
  float fac1,fac2,ytry,*ptry;
  
  ptry=(float*)malloc((ndim+1)*sizeof(float));//vector(1,ndim);
  fac1=(1.f-fac)/ndim;
  fac2=fac1-fac;
  for (j=1;j<=ndim;j++) ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
  ytry=(*funk)(ptry);
  if (ytry < y[ihi]) {
    y[ihi]=ytry;
    for (j=1;j<=ndim;j++) {
      psum[j] += ptry[j]-p[ihi][j];
      p[ihi][j]=ptry[j];
    }
  }
  free (ptry);//free_vector(ptry,1,ndim);
  return ytry;
}


#define TINY 1.0e-10
#define NMAX 5000
#define GET_PSUM \
					for (j=1;j<=ndim;j++) {\
					for (sum=0.0,i=1;i<=mpts;i++) sum += p[i][j];\
					psum[j]=sum;}
#define SWAP(a,b) {swap=(a);(a)=(b);(b)=swap;}

void
amoeba(float **p, float y[], int ndim, float ftol, float (*funk)(float []), int *nfunk)
{
  float amotry(float **p, float y[], float psum[], int ndim,
  float (*funk)(float []), int ihi, float fac);
  int i,ihi,ilo,inhi,j,mpts=ndim+1;
  float rtol,sum,swap,ysave,ytry,*psum;
  
  psum=(float*)malloc((ndim+1)*sizeof(float));//vector(1,ndim);
  *nfunk=0;
  GET_PSUM
    for (;;) {
      ilo=1;
      ihi = y[1]>y[2] ? (inhi=2,1) : (inhi=1,2);
      for (i=1;i<=mpts;i++) {
	if (y[i] <= y[ilo]) ilo=i;
	if (y[i] > y[ihi]) {
	  inhi=ihi;
	  ihi=i;
	} else if (y[i] > y[inhi] && i != ihi) inhi=i;
      }
	  rtol=2.f*fabs(y[ihi]-y[ilo])/(fabs(y[ihi])+fabs(y[ilo])+(float)TINY);
      if (rtol < ftol) {
	SWAP(y[1],y[ilo])
	  for (i=1;i<=ndim;i++)
	    SWAP(p[1][i],p[ilo][i])
	      break;
      }
      if (*nfunk >= NMAX)
	{
	  printf ("NMAX exceeded\n");//nrerror("NMAX exceeded");
	  break;
	}
      *nfunk += 2;
      ytry=amotry(p,y,psum,ndim,funk,ihi,-1.0);
      if (ytry <= y[ilo])
	ytry=amotry(p,y,psum,ndim,funk,ihi,2.0);
      else if (ytry >= y[inhi]) {
	ysave=y[ihi];
	ytry=amotry(p,y,psum,ndim,funk,ihi,0.5);
	if (ytry >= ysave) {
	  for (i=1;i<=mpts;i++) {
	    if (i != ilo) {
	      for (j=1;j<=ndim;j++)
		p[i][j]=psum[j]=.5f*(p[i][j]+p[ilo][j]);
	      y[i]=(*funk)(psum);
	    }
	  }
	  *nfunk += ndim;
	  GET_PSUM
	    }
      } else --(*nfunk);
    }
  free (psum);//free_vector(psum,1,ndim);
}
#undef SWAP
#undef GET_PSUM
#undef NMAX
#undef TINY
