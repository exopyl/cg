#include <stdlib.h>
#include <math.h>

#include "minimization_simulated_annealing.h"

#include "random_generator.h"

#define GET_PSUM \
					for (n=1;n<=ndim;n++) {\
					for (sum=0.0,m=1;m<=mpts;m++) sum += p[m][n];\
					psum[n]=sum;}
static long idum;
static float tt;

float amotsa(float **p, float y[], float psum[], int ndim, float pb[],
	float *yb, float (*funk)(float []), int ihi, float *yhi, float fac)
{
  float ran1(long *idum);
  int j;
  float fac1,fac2,yflu,ytry,*ptry;
  
  ptry=(float*)malloc((ndim+1)*sizeof(float));//vector(1,ndim);
  fac1=(float)(1.0-fac)/ndim;
  fac2=(float)(fac1-fac);
  for (j=1;j<=ndim;j++)
    ptry[j]=psum[j]*fac1-p[ihi][j]*fac2;
  ytry=(*funk)(ptry);
  if (ytry <= *yb) {
    for (j=1;j<=ndim;j++) pb[j]=ptry[j];
    *yb=ytry;
  }
  yflu=ytry-tt*log(ran1(&idum));
  if (yflu < *yhi) {
    y[ihi]=ytry;
    *yhi=yflu;
    for (j=1;j<=ndim;j++) {
      psum[j] += ptry[j]-p[ihi][j];
      p[ihi][j]=ptry[j];
    }
  }
  free (ptry);//free_vector(ptry,1,ndim);
  return yflu;
}

void amebsa(float **p, float y[], int ndim, float pb[], float *yb, float ftol,
	    float (*funk)(float []), int *iter, float temptr)
{
  float amotsa(float **p, float y[], float psum[], int ndim, float pb[],
	       float *yb, float (*funk)(float []), int ihi, float *yhi, float fac);
  float ran1(long *idum);
  int i,ihi,ilo,j,m,n,mpts=ndim+1;
  float rtol,sum,swap,yhi,ylo,ynhi,ysave,yt,ytry,*psum;
  
  psum=(float*)malloc((ndim+1)*sizeof(float));//vector(1,ndim);
  tt = -temptr;
  GET_PSUM
    for (;;) {
      ilo=1;
      ihi=2;
      ynhi=ylo=y[1]+tt*log(ran1(&idum));
      yhi=y[2]+tt*log(ran1(&idum));
      if (ylo > yhi) {
	ihi=1;
	ilo=2;
	ynhi=yhi;
	yhi=ylo;
	ylo=ynhi;
      }
      for (i=3;i<=mpts;i++) {
	yt=y[i]+tt*log(ran1(&idum));
	if (yt <= ylo) {
	  ilo=i;
	  ylo=yt;
	}
	if (yt > yhi) {
	  ynhi=yhi;
	  ihi=i;
	  yhi=yt;
	} else if (yt > ynhi) {
	  ynhi=yt;
	}
      }
      rtol=(float)(2.0f*fabs(yhi-ylo)/(fabs(yhi)+fabs(ylo)));
      if (rtol < ftol || *iter < 0) {
	swap=y[1];
	y[1]=y[ilo];
	y[ilo]=swap;
	for (n=1;n<=ndim;n++) {
	  swap=p[1][n];
	  p[1][n]=p[ilo][n];
	  p[ilo][n]=swap;
	}
	break;
      }
      *iter -= 2;
      ytry=amotsa(p,y,psum,ndim,pb,yb,funk,ihi,&yhi,-1.0);
      if (ytry <= ylo) {
	ytry=amotsa(p,y,psum,ndim,pb,yb,funk,ihi,&yhi,2.0);
      } else if (ytry >= ynhi) {
	ysave=yhi;
	ytry=amotsa(p,y,psum,ndim,pb,yb,funk,ihi,&yhi,0.5);
	if (ytry >= ysave) {
	  for (i=1;i<=mpts;i++) {
	    if (i != ilo) {
	      for (j=1;j<=ndim;j++) {
		psum[j]=.5f*(p[i][j]+p[ilo][j]);
		p[i][j]=psum[j];
	      }
	      y[i]=(*funk)(psum);
	    }
	  }
	  *iter -= ndim;
	  GET_PSUM
	    }
      } else ++(*iter);
    }
  free (psum);//free_vector(psum,1,ndim);
}

#undef GET_PSUM
