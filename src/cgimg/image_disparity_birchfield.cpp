//
// Implements Pixel-to-Pixel stereo algorithm, as explained in the
// technical report "Depth Discontinuities by Pixel-to-Pixel Stereo",
// STAN-CS-TR-96-1573, Stanford University, July 1996.
// http://vision.stanford.edu/~birch/p2p/
//
// Author:  Stan Birchfield, birchfield@cs.stanford.edu
// Date:  November 8, A.D. 1996
//
#include <assert.h>

#include "image_disparity_birchfield.h"

#define COLS               512//630   // number of columns in image
#define ROWS               480   // number of rows in image
#define MAXDISP        		20   // max. amount of disparity


#define SLOP         MAXDISP+1  // expand arrays so we don't have to keep
								// checking whether index is too large
#define INF              65535
#define DISCONTINUITY      255   // symbol for a depth discontinuity
#define NO_DISCONTINUITY     0   // symbol for no depth discontinuity





#define FIRST_MATCH 	65535   // symbol for first match
#define DEFAULT_COST  	  600   // prevents costs from becoming negative
#define NO_IG_PEN    	 1000   // penalty for depth discontinuity without intensity gradient

// Special options for comparing our algorithm with other possibilities.
// For our algorithm, leave them all commented.
//#define USE_ABSOLUTE_DIFFERENCE
//#define USE_SYMMETRIC_GRADIENTS
//#define USE_HYPOTHETICAL_GRADIENTS
//#define BACKWARD_LOOKING


/* These values are multiplied by two because of fillDissimilarityTable() */
static int occ_pen = 25 * 2;	
static int reward = 5 * 2;


DisparityBirchfield::DisparityBirchfield():
m_pImgLeft(NULL), m_pImgRight(NULL), m_pDisparity1(NULL), m_pDisparity2(NULL), m_pDiscontinuities1(NULL), m_pDiscontinuities2(NULL)
{
}

DisparityBirchfield::~DisparityBirchfield()
{
	m_pImgLeft = NULL;
	m_pImgRight = NULL;
	if (m_pDisparity1)
		delete m_pDisparity1;
	if (m_pDisparity2)
		delete m_pDisparity2;
	if (m_pDiscontinuities1)
		delete m_pDiscontinuities1;
	if (m_pDiscontinuities2)
		delete m_pDiscontinuities2;
}

void DisparityBirchfield::SetStereoPair(Img *pLeft, Img *pRight)
{
	m_pImgLeft = pLeft;
	m_pImgRight = pRight;

	unsigned int width = m_pImgLeft->width();
	unsigned int height = m_pImgLeft->height();

	m_pDisparity1 = new Img(width, height);
	m_pDisparity2 = new Img(width, height);
	m_pDiscontinuities1 = new Img(width, height);
	m_pDiscontinuities2 = new Img(width, height);
}

/********************************************************************* */
/* setOcclusionPenalty */
/* getOcclusionPenalty */
/* setReward */
/* getReward */
/* */
/* Provides an interface to the occlusion penalty and match reward.   */
/* Recall that the value stored in the variable is double the actual  */
/* value, since fillDissimilarityTable() doubles the dissimilarities  */
/* in an effort to keep everything integers. */
/********************************************************************* */
void DisparityBirchfield::setOcclusionPenalty(int op)
{
	if (op < 0)  {
		printf("Occlusion penalty must be nonnegative.  Setting to zero\n");
		op = 0;
	}
	occ_pen = op * 2;
}

int DisparityBirchfield::getOcclusionPenalty(void)
{
	if (occ_pen%2!=0) {
		printf("Low-level problem:  Someone must have manually set occ_pen incorrectly\n");
		return -1;
	}
	return (occ_pen/2);
}

void DisparityBirchfield::setReward(int r)
{
	if (r < 0)  {
		printf("Reward must be nonnegative.  Setting to zero\n");
		r = 0;
	}
	reward = r*2;
}

int DisparityBirchfield::getReward(void)
{
	if (occ_pen%2!=0) {
		printf("Low-level problem:  Someone must have manually set reward incorrectly\n");
		return -1;
	}
	return (reward/2);
}

/*
// computeIntensityGradientsX
//
// This function determines which pixels lie beside intensity
// gradients.  A pixel in the {left} scanline lies to the {left} of
// an intensity gradient if there is some intensity variation to its
// right.  Likewise, a pixel in the {right} scanline lies to the
// {right} of an intensity gradient if there is some intensity
// variation to its left.
//
// A pixel which lies beside an intensity gradient is labelled with 0;
// all other pixels are labelled with NO_IG_PEN.  This label is used
// as a penalty to enforce the constraint that depth discontinuities
// must occur at intensity gradients.  That is, a value of 0 has no
// effect on the cost function, whereas a value of NO_IG_PEN prohibits
// depth discontinuities.
//
// NOTE:  Because NO_IG_PEN is so large, this computation prohibits
// depth discontinuities that are not accompanied by intensity
// gradients, just like the ``if'' statements in the paper.  This
// method is chosen for computational speed.
*/
void computeIntensityGradientsX(Img *m_pImgLeft,
                                Img *m_pImgRight,
                                int scanline, 
                                int no_igL[COLS+SLOP], 
                                int no_igR[COLS+SLOP])
{
   int th = 5;                // minimum intensity variation within window
   int w = 3;                 // width of window
   int max1, min1, max2, min2;
   int i, j;
   
   // Initially, declare all pixels to be NOT intensity gradients
   for (i = 0 ; i < COLS ; i++)
   {
      no_igL[i] = NO_IG_PEN;
      no_igR[i] = NO_IG_PEN;
   }

   // Find intensity gradients in the left scanline
   for (i = 0 ; i < COLS - w + 1 ; i++)
   {
      max1 = 0;      min1 = INF;
      for (j = i ; j < i + w ; j++)
	  {
		  //if (imgL[scanline][j] < min1)   min1 = imgL[scanline][j];
		  //if (imgL[scanline][j] > max1)   max1 = imgL[scanline][j];
		  if (m_pImgLeft->get_r(j, scanline) < min1)   min1 = m_pImgLeft->get_r(j, scanline);
		  if (m_pImgLeft->get_r(j, scanline) > max1)   max1 = m_pImgLeft->get_r(j, scanline);
      }
      if (max1 - min1 >= th)
         no_igL[i] = 0;
   }

   // Find intensity gradients in the right scanline
   for (i = w - 1 ; i < COLS ; i++)
   {
      max2 = 0;      min2 = INF;
      for (j = i - w + 1 ; j <= i ; j++)
	  {
         //if (imgR[scanline][j] < min2)   min2 = imgR[scanline][j];
         //if (imgR[scanline][j] > max2)   max2 = imgR[scanline][j];
		 if (m_pImgRight->get_r(j, scanline) < min2)   min2 = m_pImgRight->get_r(j, scanline);
		 if (m_pImgRight->get_r(j, scanline) > max2)   max2 = m_pImgRight->get_r(j, scanline);
	  }
      if (max2 - min2 >= th)
         no_igR[i] = 0;
   }
   printf ("%d %d %d %d\n", min1, max1, min2, max2);

#ifdef USE_SYMMETRIC_GRADIENTS

   /* Shift left scanline to the right */
   for (i = COLS - 1 ; i >= 2 ; i--)  {
      no_igL[i] = NO_IG_PEN * (no_igL[i-1] && no_igL[i-2]);
   }

   /* Shift right scanline to the left */
   for (i = 0 ; i < COLS - 2 ; i++)  {
      no_igR[i] = NO_IG_PEN * (no_igR[i+1] && no_igR[i+2]);
   }

#elif defined(USE_HYPOTHETICAL_GRADIENTS)

   /* Shift left scanline to the right */
   for (i = COLS - 1 ; i >= 1 ; i--)  {
      no_igL[i] = no_igL[i-1];
   }

   /* Shift right scanline to the left */
   for (i = 0 ; i < COLS - 1 ; i++)  {
      no_igR[i] = no_igR[i+1];
   }

   /* Shift left scanline to the right */
   for (i = COLS - 1 ; i >= 1 ; i--)  {
      no_igL[i] = no_igL[i-1];
   }

   /* Shift right scanline to the left */
   for (i = 0 ; i < COLS - 1 ; i++)  {
      no_igR[i] = no_igR[i+1];
   }

   /* Shift left scanline to the right */
   for (i = COLS - 1 ; i >= 1 ; i--)  {
      no_igL[i] = no_igL[i-1];
   }

   /* Shift right scanline to the left */
   for (i = 0 ; i < COLS - 1 ; i++)  {
      no_igR[i] = no_igR[i+1];
   }

#endif
}

//
// fillDissimilarityTable
//
// Precomputes the dissimilarity values between each pair of pixels.
// The dissimilarity is defined as the minimum of:
//        min{ |I_1(x1) - I_2'(z)|, |I_1'(z) - I_2(x2)| },
// where the prime denotes the linearly interpolated image.
// Actually returns twice the dissimilarity, to keep everything integers.
//
// dimgL[x] = 2 * imgL[x];
// himgL[x] = imgL[x - 1] + imgL[x];
// himgL[x + 1] = imgL[x] + imgL[x + 1];
//
void fillDissimilarityTable(Img *pImgLeft,//unsigned char imgL[ROWS][COLS], 
							Img *pImgRight,//unsigned char imgR[ROWS][COLS],
							int dis[COLS][MAXDISP + 1], 
							int scanline)
{

#ifndef USE_ABSOLUTE_DIFFERENCE

   unsigned short int himgL[COLS + 1], himgR[COLS + 1];
   unsigned short int dimgL[COLS], dimgR[COLS];
   unsigned short int p0, p1, p2, q0, q1, q2;
   unsigned short int pmin, pmax, qmin, qmax;
   unsigned short int x, y, alpha, minn;

   p1 = pImgLeft->get_r(0, scanline);//imgL[scanline][0];
   q1 = pImgRight->get_r(0, scanline);//imgR[scanline][0];
   himgL[0] = 2 * p1;
   himgR[0] = 2 * q1;
   himgL[COLS] = 2 * pImgLeft->get_r(COLS - 1, scanline);//imgL[scanline][COLS - 1];
   himgR[COLS] = 2 * pImgRight->get_r(COLS - 1, scanline);//imgR[scanline][COLS - 1];
   dimgL[0] = 2 * p1;
   dimgR[0] = 2 * q1;

   for (y = 1 ; y < COLS ; y++)  {
      p0 = p1;
      p1 = pImgLeft->get_r(y, scanline);//imgL[scanline][y];
      q0 = q1;
      q1 = pImgRight->get_r(y, scanline);//imgR[scanline][y];
      himgL[y] = p0 + p1;
      dimgL[y] = 2 * p1;
      himgR[y] = q0 + q1;
      dimgR[y] = 2 * q1;
   }

   for (y = 0 ; y < COLS ; y++)
      for (alpha = 0 ; alpha <= MAXDISP ; alpha++)  {
         x = y + alpha;
         if (x < COLS)  {
            p0 = dimgL[x];
            p1 = himgL[x];
            p2 = himgL[x + 1];
            q0 = dimgR[y];
            q1 = himgR[y];
            q2 = himgR[y + 1];
            minn = INF;
  
            pmax = MAX3(p0, p1, p2);
            pmin = MIN3(p0, p1, p2);
            qmax = MAX3(q0, q1, q2);
            qmin = MIN3(q0, q1, q2);
      
            if (p0 >= qmin)  { 
               if (p0 <= qmax) 
                  minn = 0;
               else
                  minn = p0 - qmax;
            }
            else  {
               minn = qmin - p0;
            }
            if (minn > 0)  {
               if (q0 >= pmin)  {
                  if (q0 <= pmax) 
                     minn = 0;
                  else
                     minn = MIN(minn, q0 - pmax);
               }
               else
                  minn = MIN(minn, pmin - q0);
            }
  
            dis[y][alpha] = minn;
         }
      }

#else

   unsigned short int y, alpha;

   for (y = 0 ; y < COLS ; y++)
      for (alpha = 0 ; alpha <= MAXDISP ; alpha++)  {
         if (y+alpha < COLS)  {
            dis[y][alpha] = 2 * abs(imgL[scanline][y+alpha] - imgR[scanline][y]);
         }
      }

#endif

}


/********************************************************************* */
/* print_phi */
/* */

void print_phi(int phi[ROWS][COLS+SLOP][MAXDISP + 1],
               int scanline,
               int y0,
               int y1,
               int d0,
               int d1)
{
   int y, d;
   
   if (y0<0)  y0=0;
   if (y0>=COLS) y0=COLS-1;
   if (y1<y0) y1=y0;
   if (y1>=COLS) y1=COLS-1;
   if (d0<0)  d0=0;
   if (d0>MAXDISP)  d0=MAXDISP;
   if (d1<d0) d1=d0;
   if (d1>MAXDISP)  d1=MAXDISP;
   if (scanline<0) scanline=0;
   if (scanline>=ROWS) scanline=ROWS-1;

   printf("\n(PRINT_PHI)\n");
   fflush(stdout);
   printf("\nSCANLINE %3d\n", scanline);
   printf("Disp: ");
   for (d=d0 ; d<=d1 ; d++) {
      printf("%3d ", d);
   }
   printf("\n-----------------------------------\n");
   for (y=y0 ; y<=y1 ; y++) {
      printf("%3d: ", y);
      for (d=d0 ; d<=d1 ; d++) {
         printf("%3d ", phi[scanline][y][d]);
      }
      printf("\n");
   }
}


/********************************************************************* */
/* print_dis */
/* */

void print_dis(int dis[ROWS][COLS+SLOP][MAXDISP + 1],
               int scanline,
               int y0,
               int y1,
               int d0,
               int d1)
{
   int y, d;
   
   if (y0<0)  y0=0;
   if (y0>=COLS) y0=COLS-1;
   if (y1<y0) y1=y0;
   if (y1>=COLS) y1=COLS-1;
   if (d0<0)  d0=0;
   if (d0>MAXDISP)  d0=MAXDISP;
   if (d1<d0) d1=d0;
   if (d1>MAXDISP)  d1=MAXDISP;
   if (scanline<0) scanline=0;
   if (scanline>=ROWS) scanline=ROWS-1;

   printf("\n(PRINT_DIS)\n");
   printf("\nSCANLINE %3d\n", scanline);
   printf("Disp: ");
   for (d=d0 ; d<=d1 ; d++) {
      printf("%3d ", d);
   }
   printf("\n-----------------------------------\n");
   for (y=y0 ; y<=y1 ; y++) {
      printf("%3d: ", y);
      for (d=d0 ; d<=d1 ; d++) {
         printf("%3d ", dis[scanline][y][d]);
      }
      printf("\n");
   }
}


/********************************************************************* */
/* normalize_phi */
/* */

void normalize_phi(int scanline,
                   int phi[ROWS][COLS+SLOP][MAXDISP + 1],
                   int scanline_interest)
{
   int minn;
   int y, deltaa;

   minn = INF;
   for (y=0 ; y<COLS ; y++)
      for (deltaa=0 ; deltaa<=MAXDISP ; deltaa++)
         if (phi[scanline][y][deltaa]<minn)
            minn = phi[scanline][y][deltaa];
   if (scanline==scanline_interest) printf("minn=%d\n", minn);
   for (y=0 ; y<COLS ; y++)
      for (deltaa=0 ; deltaa<=MAXDISP ; deltaa++)
         phi[scanline][y][deltaa] -= minn;
}


/********************************************************************* */
/* conductDPBackward */
/* */
/* Does dynamic programming using the Backward-Looking Algorithm. */
/* Fills the phi, pie_y, and pie_d tables. */
/* */
/* flag: 0 means each scanline independent */
/*       1 means use phi table in scanline above */
/*       2 means, in addition to 1, also using the values in */
/*         phi table in scanline below. */

void conductDPBackward(int scanline,
                       int phi[ROWS][COLS+SLOP][MAXDISP + 1],
                       int pie_y[ROWS][COLS+SLOP][MAXDISP + 1],
                       int pie_d[ROWS][COLS+SLOP][MAXDISP + 1],
                       int dis[ROWS][COLS+SLOP][MAXDISP + 1],
                       int no_igL[ROWS][COLS+SLOP],
                       int no_igR[ROWS][COLS+SLOP],
                       int flag,
                       int scanline_interest)
{
   int y, deltaa;                   /* the match following (y_p, delta_p) */
   int y_p, delta_p;                /* the current match */
   int phi_new, phi_best;
   int pie_y_best, pie_d_best;
/*      printf("Starting...\n");  fflush(stdout); */

   for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
      phi[scanline][0][delta_p] = DEFAULT_COST + dis[scanline][0][delta_p];
      pie_y[scanline][0][delta_p] = FIRST_MATCH;
      pie_d[scanline][0][delta_p] = FIRST_MATCH;
   }

   for (y = 1 ; y < COLS ; y++)  {
      /* printf("y=%d\n", y);  fflush(stdout); */
      for (deltaa = 0 ; deltaa <= MAXDISP ; deltaa++)  {

         phi_best = INF;

         for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
            y_p = y - MAX(1, delta_p - deltaa + 1);
            if (y_p>=0) {
               if (deltaa==delta_p ||
                   (deltaa>delta_p && !no_igL[scanline][y+deltaa-1]) ||
                   (deltaa<delta_p && !no_igR[scanline][y_p+1])) {
                  phi_new = 
                     occ_pen * (deltaa != delta_p);
                     if (scanline==scanline_interest && y==12) {
                        printf("## phi_new=%d\n", phi_new);
                     }
                  if (flag>0 && scanline>0) {
                     int s = phi[scanline][y_p][delta_p] +
                        phi[scanline-1][y][deltaa];
                     if (scanline==scanline_interest && y==12) {
                        printf("## s=%d\n", s);
                     }
/*                     int n = 2; */
                     if (0 && deltaa>delta_p) {
                        int yy = y_p+1;
                        while (yy+delta_p<y+deltaa) {
                           if (yy<0 || yy>=COLS+SLOP) printf("****yy=%d\n", yy);
                           assert(yy>=0 && yy<COLS+SLOP);
                           assert(delta_p>=0 && delta_p<=MAXDISP);
                           s += phi[scanline-1][yy][delta_p];
                           /*                          n++; */
                           yy++;
                        }
                     } else if (0 && deltaa<delta_p) {
                        int yy = y_p+1;
                        while (yy<y) {
                           assert(yy>=0 && yy<COLS+SLOP);
                           assert(delta_p>=0 && delta_p<=MAXDISP);
                           s += phi[scanline-1][yy][deltaa];
                           /* n++; */
                           yy++;
                        }                           
                           
/*                     while (d<delta_p) { */
                        /*                      d++; */
                        /*s += phi[scanline-1][y][d]; */
                        /*n++; */
                     }
/*                     phi_new += (phi[scanline][y_p][delta_p] + */
/*                         phi[scanline-1][y][deltaa])/2; */
                     phi_new += s;
                  } else {
                     phi_new += phi[scanline][y_p][delta_p];
                  }
/*                  if (scanline==scanline_interest && y==549) printf("## delta=%2d, delta_p=%2d, phi_new=%d\n", deltaa, delta_p, phi_new); */
                     if (scanline==scanline_interest && y==12) {
                        printf("## phi_new=%d\n", phi_new);
                     }
                  if (phi_new < phi_best) {
         if (scanline==scanline_interest && y==12) printf("!! New Phi_best !!\n");
                     phi_best = phi_new;
                     pie_y_best = y_p;
                     pie_d_best = delta_p;
                  }
               }
            }
         }
         if (scanline==scanline_interest && y==12) printf("--## delta=%2d, phi_best=%d\n", deltaa, phi_best);
         phi[scanline][y][deltaa] = phi_best +
            dis[scanline][y][deltaa] - reward;

         /*     if (scanline == scanline_interest && y<15 && deltaa==4) { */
         /* printf("[%3d][%3d][%2d]:  phi=%d\n", */
         /*        scanline, y, deltaa, phi[scanline][y][deltaa]); */
         /*} */
         pie_y[scanline][y][deltaa] = pie_y_best;
         pie_d[scanline][y][deltaa] = pie_d_best;
      }
   }
   
   if (scanline>0) {
      for (y=0 ; y<COLS ; y++)
         for (deltaa=0 ; deltaa<=MAXDISP ; deltaa++)
            phi[scanline][y][deltaa] /= ((scanline-1)*COLS+y);
   }
   
   if (scanline==scanline_interest) {
/*      print_phi(phi, scanline, 0, 50, 0, 14); */
      print_phi(phi, scanline, 0, 150, 0, 5);
   }
   
/*   normalize_phi(scanline, phi, scanline_interest); */
   
/*   if (scanline==scanline_interest) { */
/*      print_phi(phi, scanline, 0, 50, 0, 14); */
/*      print_phi(phi, scanline, 0, 150, 0, 5); */
/*   } */
   
/*      printf("Ending...\n");  fflush(stdout); */
}


/********************************************************************* */
/* conductDPFaster */
/* */
/* Does dynamic programming using the Faster Algorithm. */
/* Fills the phi, pie_y, and pie_d tables. */

void conductDPFaster(int scanline,
                     int phi[ROWS][COLS+SLOP][MAXDISP + 1],
                     int pie_y[ROWS][COLS+SLOP][MAXDISP + 1],
                     int pie_d[ROWS][COLS+SLOP][MAXDISP + 1],
                     int dis[ROWS][COLS+SLOP][MAXDISP + 1],
                     int no_igL[ROWS][COLS+SLOP],
                     int no_igR[ROWS][COLS+SLOP],
                     int flag)
{
   int y, deltaa;                   /* the match following (y_p, delta_p) */
   int y_p, delta_p;                /* the current match */
   int ymin, xmin[COLS + SLOP];     /* used to prune bad nodes */
   int phi_new;

   /* Initialize arrays */
   for (y_p = 1 ; y_p < COLS ; y_p++)  {
      xmin[y_p] = INF;
      for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)
         phi[scanline][y_p][delta_p] = INF;
   }

   for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
      phi[scanline][0][delta_p] = DEFAULT_COST + dis[scanline][0][delta_p];
      pie_y[scanline][0][delta_p] = FIRST_MATCH;
      pie_d[scanline][0][delta_p] = FIRST_MATCH;
      xmin[0] = phi[scanline][0][delta_p];
   }

   for (y_p = 0 ; y_p < COLS ; y_p++)  {

      /* Determine ymin */
      ymin = INF;
      for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
         ymin = MIN(ymin, phi[scanline][y_p][delta_p]);
      }

      for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {

         /* Expand good y nodes */
         if ( phi[scanline][y_p][delta_p] <= ymin )  {
            y = y_p + 1;
            for (deltaa = delta_p + 1 ; deltaa <= MAXDISP ; deltaa++)  {
               phi_new = phi[scanline][y_p][delta_p] +
                  dis[scanline][y][deltaa] - reward + occ_pen
                  + no_igL[scanline][y + deltaa];
               if (phi_new < phi[scanline][y][deltaa])  {
                  phi[scanline][y][deltaa] = phi_new;
                  pie_y[scanline][y][deltaa] = y_p;
                  pie_d[scanline][y][deltaa] = delta_p;
                  xmin[y+deltaa] = MIN(xmin[y+deltaa], phi_new);
               }  /* end if(phi_new) */
            }  /* end for(deltaa) */
         }  /* end if(phi[][] <= ymin) */

         /* Expand good x nodes */
         if ( phi[scanline][y_p][delta_p] <= xmin[y_p+delta_p] ) {
            for (deltaa = 0 ; deltaa < delta_p ; deltaa++)  {
               y = y_p + delta_p - deltaa + 1;
               phi_new = phi[scanline][y_p][delta_p] +
                  dis[scanline][y][deltaa] - reward + occ_pen
                  + no_igR[scanline][y_p];
               if (phi_new < phi[scanline][y][deltaa])  {
                  phi[scanline][y][deltaa] = phi_new;
                  pie_y[scanline][y][deltaa] = y_p;
                  pie_d[scanline][y][deltaa] = delta_p;
                  xmin[y+deltaa] = MIN(xmin[y+deltaa], phi_new);
               }  /* end if(phi_new) */
            }  /* end for(deltaa) */
         }  /* end if(phi[][] <= xmin[]) */

         /* Expand all nodes */
         phi_new = phi[scanline][y_p][delta_p] +
            dis[scanline][y_p+1][delta_p] - reward;
         if ( phi_new < phi[scanline][y_p+1][delta_p] )  {
            phi[scanline][y_p+1][delta_p] = phi_new;
            pie_y[scanline][y_p+1][delta_p] = y_p;
            pie_d[scanline][y_p+1][delta_p] = delta_p;
            xmin[y_p+1+delta_p] = MIN(xmin[y_p+1+delta_p], phi_new);
         }  /* end(if) */
      }  /* end for(delta_p) */
   }  /* end for(y_p) */
}


//
// find_ending_match
//
// finds ending match $m_{N_m}$
//
void find_ending_match(int scanline,
                       int phi[ROWS][COLS+SLOP][MAXDISP + 1],
                       int *pie_y_best,
                       int *pie_d_best)
{
   int phi_best;
   int deltaa, y;
   
   phi_best = INF;
   for (deltaa = 0 ; deltaa <= MAXDISP ; deltaa++)  {
      y = COLS - 1 - deltaa;
      if (phi[scanline][y][deltaa] <= phi_best)  {
         phi_best = phi[scanline][y][deltaa];
         *pie_y_best = y;
         *pie_d_best = deltaa;
      }
   }
}


/********************************************************************* */
/* extract_matches */
/* */
/* This code extracts matches from phi and pie tables.   */
/* It is only included for debugging purposes, and its */
/* results are used by no one. */
/*
extract_matches(int scanline,
                int pie_y[ROWS][COLS+SLOP][MAXDISP + 1],
                int pie_d[ROWS][COLS+SLOP][MAXDISP + 1])
{
#if 0   
   int matches[2][COLS];
   int num_matches;
   int tmp_y, tmp_d;
   int zz = 0;
   
   y_p = pie_y_best;
   delta_p = pie_d_best;
   while (y_p != FIRST_MATCH && delta_p != FIRST_MATCH)  {
      matches[0][zz] = y_p + delta_p;
      matches[1][zz] = y_p;
      tmp_y = pie_y[y_p][delta_p];
      tmp_d = pie_d[y_p][delta_p];
      y_p = tmp_y;
      delta_p = tmp_d;
      zz++;
   }
   num_matches = y;
#endif  
}
*/


//
// compute_dm_and_dd
//
// Computes disparity map and depth discontinuities
//
void compute_dm_and_dd(int scanline,
                       int pie_y_best,
                       int pie_d_best,
                       int pie_y[COLS+SLOP][MAXDISP + 1],
                       int pie_d[COLS+SLOP][MAXDISP + 1],
                       unsigned char disparity_map[ROWS][COLS],
                       unsigned char depth_discontinuities[ROWS][COLS])
{
   int x, x1, x2, y1, y2, deltaa1, deltaa2;
   
   y1 = pie_y_best;         deltaa1 = pie_d_best;         x1 = y1 + deltaa1;
   y2 = pie_y[y1][deltaa1]; deltaa2 = pie_d[y1][deltaa1]; x2 = y2 + deltaa2;
   
   for (x = COLS - 1 ; x >= x1 ; x--)  {
      disparity_map[scanline][x] = deltaa1;
      depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
   }
   
   while (y2 != FIRST_MATCH)  {
      if (deltaa1 == deltaa2)  {
         disparity_map[scanline][x2] = deltaa2;
         depth_discontinuities[scanline][x2] = NO_DISCONTINUITY;
      }
      else if (deltaa2 > deltaa1)  {
         disparity_map[scanline][x2] = deltaa2;
         depth_discontinuities[scanline][x2] = DISCONTINUITY;
      }
      else {
         disparity_map[scanline][x1 - 1] = deltaa2;
         depth_discontinuities[scanline][x1 - 1] = DISCONTINUITY;
         for (x = x1 - 2 ; x >= x2 ; x--)  {
            disparity_map[scanline][x] = deltaa2;
            depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
         }
      }
      y1 = y2;                 deltaa1 = deltaa2;             x1 = y1 + deltaa1;
      y2 = pie_y[y1][deltaa1]; deltaa2 = pie_d[y1][deltaa1]; x2 = y2 + deltaa2;
   }
   
   for (x = y1 + deltaa1 - 1 ; x >= 0 ; x--)  {
      disparity_map[scanline][x] = deltaa1;
      depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
   }
}


/********************************************************************* */
/* joinDissimilarites */

void joinDissimilarities(int dis[ROWS][COLS+SLOP][MAXDISP + 1],
                         int height)
{
   int hh = height/2;
   int newdis[ROWS][COLS+SLOP][MAXDISP + 1];
   int x, y, disp;
   int yy;
   int dsum, n;

   assert(height % 2 == 1);

   memcpy((int *) newdis, (int *) dis, ROWS * (COLS+SLOP) * (MAXDISP+1) * sizeof(int));

   for (x=0 ; x<COLS+SLOP ; x++) {
      for (y=0 ; y<ROWS ; y++) {
         for (disp=0 ; disp<=MAXDISP ; disp++) {
            dsum = dis[y][x][disp];
            n = 1;
            yy = MAX(0, y-hh);
            while (yy<y) {
               if (x>490) 
                  dsum += dis[yy][x][disp];
               else
                  dsum += dis[y][x][disp];
               n++;
               yy++;
            }
            yy = MIN(ROWS-1, y+hh);
            while (yy>y) {
               if (x>490) 
                  dsum += dis[yy][x][disp];
               else
                  dsum += dis[y][x][disp];
               n++;
               yy--;
            }
            newdis[y][x][disp] = dsum;
         }
      }
   }

   memcpy((int *) dis, (int *) newdis, ROWS * (COLS+SLOP) * (MAXDISP+1) * sizeof(int));
}


/********************************************************************* */
void countDis(int dis[ROWS][COLS+SLOP][MAXDISP + 1])
{
   int x, y, d;
   int sum;

   for (d=0 ; d<=MAXDISP ; d++) {
      sum = 0;
      for (x=490 ; x<COLS ; x++) {
         for (y=123 ; y<131 ; y++) {
            sum += dis[y][x][d];
         }
      }
      printf("   %%%%  dis=%2d,  count=%d\n", d, sum);
   }
}


/********************************************************************* */
/* */
/*
void saveIGtoPGM(int no_igL[ROWS][COLS+SLOP], char *fname)
{
   //unsigned char tmp[ROWS][COLS+SLOP];
   Img *tmp = new Img (ROWS, COLS+SLOP);
   int val;
   int tx, ty;

   for (ty=0 ; ty<ROWS ; ty++) {
      for (tx=0 ; tx<COLS+SLOP ; tx++) {
         val = no_igL[ty][tx];
         if (val<0) val=0;
         if (val>255) val=255;
         //tmp[ty][tx] = (unsigned char) val;
		 tmp->set_pixel(tx, ty, (unsigned char) val, (unsigned char) val, (unsigned char) val, 255);
      }
   }
   //pgmWriteFile(fname, (unsigned char *) tmp, COLS+SLOP, ROWS);
   tmp->save(fname);
   delete tmp;
}
*/

/********************************************************************* */
/* */
/*
static void _readIGXFileHelper(char *ptr_ig,
                               int q_no_ig[ROWS][COLS+SLOP],
                               char c)
{
   int magic, ncols, nrows, maxval;
   char fname[80];
   unsigned char tmp[ROWS][COLS+SLOP];
   unsigned char *pri;
   int *pro;
   int i;

   sprintf(fname, ptr_ig, c);
   pgmReadHeaderFile(fname, &magic, &ncols, &nrows, &maxval);
   if (ncols!=COLS+SLOP || nrows!=ROWS)
      printf("IG File '%s' is of the wrong size.\n", ptr_ig);
   pgmReadFile(fname, (unsigned char *) tmp, &ncols, &nrows);
   pri=(unsigned char *) tmp;
   pro=(int *) q_no_ig;
   for (i=ROWS*(COLS+SLOP) ; i>0 ; i--)  *pro++ = (int) *pri++;
}
*/

/********************************************************************* */
/* */
/*
void readIGXFiles(char *ptr_ig,
                  int q_no_igL[ROWS][COLS+SLOP],
                  int q_no_igR[ROWS][COLS+SLOP])
{   
   if (strstr(ptr_ig, "%c")==NULL)
      printf("IG File '%s' is invalid.  Must contain '%%c'.\n", ptr_ig);

   _readIGXFileHelper(ptr_ig, q_no_igL, 'L');
   _readIGXFileHelper(ptr_ig, q_no_igR, 'R');
}
*/

/*
matchScanlines

 Used in STAN-CS-TR96-1573 and ICCV'98

Matches each pair of scanlines independently from the other pairs,
using the Faster Algorithm, which is dynamic programming with
pruning.

NOTES
On indexing the matrices, phi(y,deltaa) is the total cost of the
match sequence ending with the match (y+deltaa,y).  In other
words, y is the index into the right scanline and deltaa is the
disparity.  For the sake of efficiency, these matrices are
transposed from those in the paper.
*/

int q_no_igL[ROWS][COLS+SLOP];          // indicates no intensity gradient
int q_no_igR[ROWS][COLS+SLOP];

void DisparityBirchfield::Process(void)
{
	int phi[COLS+SLOP][MAXDISP + 1];      /* cost of a match sequence */
	int pie_y[COLS+SLOP][MAXDISP + 1];    /* points to the immediately */
	int pie_d[COLS+SLOP][MAXDISP + 1];    /*    preceding match */
	int dis[COLS+SLOP][MAXDISP + 1];      /* dissimilarity b/w two pixels */
	int no_igL[COLS+SLOP];                /* indicates no intensity gradient */
	int no_igR[COLS+SLOP];
	int scanline;                         /* the current scanline */
	int y, deltaa;                        /* the match following (y_p, delta_p) */
	int y_p, delta_p;                     /* the current match */
	int ymin, xmin[COLS + SLOP];          /* used to prune bad nodes */
	int phi_new;
	int phi_best, pie_y_best, pie_d_best;

	//printf("Parameters:  occ=%d, rew=%d, ptr_ig='%s'\n", 
	//	getOcclusionPenalty(), getReward(), ptr_ig ? ptr_ig : "NULL");

	//if (ptr_ig != NULL) {
	//	readIGXFiles(ptr_ig, q_no_igL, q_no_igR);
	//}

	for (scanline = 0 ; scanline < ROWS ; scanline++)
	{
		if (scanline % 50 == 0 && ROWS > 200)
			printf("     scanline %d\n", scanline);

		// Fill tables
		fillDissimilarityTable(m_pImgLeft, m_pImgRight, dis, scanline);
		//if (ptr_ig == NULL)
			computeIntensityGradientsX(m_pImgLeft, m_pImgLeft, scanline, no_igL, no_igR);
		//else
		//{
		//	memcpy(no_igL, q_no_igL[scanline], (COLS+SLOP)*sizeof(int));
		//	memcpy(no_igR, q_no_igR[scanline], (COLS+SLOP)*sizeof(int));
		//<}

#ifdef BACKWARD_LOOKING

		for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
			phi[0][delta_p] = DEFAULT_COST + dis[0][delta_p];
			pie_y[0][delta_p] = FIRST_MATCH;
			pie_d[0][delta_p] = FIRST_MATCH;
		}

		for (y = 1 ; y < COLS ; y++)  {
			// printf("y=%d\n", y);  fflush(stdout);
			for (deltaa = 0 ; deltaa <= MAXDISP ; deltaa++)  {

				phi_best = INF;

				for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)  {
					y_p = y - max(1, delta_p - deltaa + 1);
					if (y_p>=0) {
						if (deltaa==delta_p ||
							(deltaa>delta_p && !no_igL[y+deltaa-1]) ||
							(deltaa<delta_p && !no_igR[y_p+1])) {
								phi_new = phi[y_p][delta_p] + occ_pen * (deltaa != delta_p);
								if (phi_new < phi_best) {
									phi_best = phi_new;
									pie_y_best = y_p;
									pie_d_best = delta_p;
								}
						}
					}
				}
				phi[y][deltaa] = phi_best + dis[y][deltaa]-reward;
				pie_y[y][deltaa] = pie_y_best;
				pie_d[y][deltaa] = pie_d_best;
			}
		}

#else
		// Initialize arrays
		for (y_p = 1 ; y_p < COLS ; y_p++)
		{
			xmin[y_p] = INF;
			for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)
				phi[y_p][delta_p] = INF;
		}

		for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)
		{
			phi[0][delta_p] = DEFAULT_COST + dis[0][delta_p];
			pie_y[0][delta_p] = FIRST_MATCH;
			pie_d[0][delta_p] = FIRST_MATCH;
			xmin[0] = phi[0][delta_p];
		}

		for (y_p = 0 ; y_p < COLS ; y_p++)
		{
			// Determine ymin
			ymin = INF;
			for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)
				ymin = MIN(ymin, phi[y_p][delta_p]);

			for (delta_p = 0 ; delta_p <= MAXDISP ; delta_p++)
			{
				// Expand good y nodes
				if ( phi[y_p][delta_p] <= ymin )
				{
					y = y_p + 1;
					for (deltaa = delta_p + 1 ; deltaa <= MAXDISP ; deltaa++)
					{
						phi_new = phi[y_p][delta_p] + dis[y][deltaa] - reward + occ_pen + no_igL[y + deltaa];
						if (phi_new < phi[y][deltaa])
						{
							phi[y][deltaa] = phi_new;
							pie_y[y][deltaa] = y_p;
							pie_d[y][deltaa] = delta_p;
							xmin[y+deltaa] = MIN(xmin[y+deltaa], phi_new);
						}
					}
				}

				// Expand good x nodes
				if ( phi[y_p][delta_p] <= xmin[y_p+delta_p] )
				{
					for (deltaa = 0 ; deltaa < delta_p ; deltaa++)
					{
						y = y_p + delta_p - deltaa + 1;
						phi_new = phi[y_p][delta_p] + dis[y][deltaa] - reward + occ_pen
							+ no_igR[y_p];
						if (phi_new < phi[y][deltaa])
						{
							phi[y][deltaa] = phi_new;
							pie_y[y][deltaa] = y_p;
							pie_d[y][deltaa] = delta_p;
							xmin[y+deltaa] = MIN(xmin[y+deltaa], phi_new);
						}
					}
				}

				// Expand all nodes
				phi_new = phi[y_p][delta_p] + dis[y_p+1][delta_p] - reward;
				if ( phi_new < phi[y_p+1][delta_p] )
				{
					phi[y_p+1][delta_p] = phi_new;
					pie_y[y_p+1][delta_p] = y_p;
					pie_d[y_p+1][delta_p] = delta_p;
					xmin[y_p+1+delta_p] = MIN(xmin[y_p+1+delta_p], phi_new);
				}
			}
		}

#endif  /* BACKWARD_LOOKING */


		/* find ending match $m_k$ */

		phi_best = INF;
		for (deltaa = 0 ; deltaa <= MAXDISP ; deltaa++)
		{
			y = COLS - 1 - deltaa;
			if (phi[y][deltaa] <= phi_best)
			{
				phi_best = phi[y][deltaa];
				pie_y_best = y;
				pie_d_best = deltaa;
			}
		}


		/******** */
#if 0 
		/* This code extracts matches from phi and pie tables. */
		/* It is only included for debugging purposes, and its */
		/* results are used by no one. */

		{
			int matches[2][COLS];
			int num_matches;
			int tmp_y, tmp_d;
			int zz = 0;

			y_p = pie_y_best;
			delta_p = pie_d_best;
			while (y_p != FIRST_MATCH && delta_p != FIRST_MATCH)  {
				matches[0][zz] = y_p + delta_p;
				matches[1][zz] = y_p;
				tmp_y = pie_y[y_p][delta_p];
				tmp_d = pie_d[y_p][delta_p];
				y_p = tmp_y;
				delta_p = tmp_d;
				zz++;
			}
			num_matches = y;
		}
#endif
		/******** */



		/* Compute disparity map and depth discontinuities */
		{
			int x, x1, x2, y1, y2, deltaa1, deltaa2;

			y1 = pie_y_best;         deltaa1 = pie_d_best;         x1 = y1 + deltaa1;
			y2 = pie_y[y1][deltaa1]; deltaa2 = pie_d[y1][deltaa1]; x2 = y2 + deltaa2;

			for (x = COLS - 1 ; x >= x1 ; x--)  {
				m_pDisparity1->set_pixel(x, scanline, deltaa1, deltaa1, deltaa1, 255);//disparity_map[scanline][x] = deltaa1;
				m_pDiscontinuities1->set_pixel(x, scanline, NO_DISCONTINUITY, NO_DISCONTINUITY, NO_DISCONTINUITY, 255);//depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
			}

			while (y2 != FIRST_MATCH)
			{
				if (deltaa1 == deltaa2)
				{
					m_pDisparity1->set_pixel(x2, scanline, deltaa2, deltaa2, deltaa2, 255);//disparity_map[scanline][x2] = deltaa2;
					m_pDiscontinuities1->set_pixel(x2, scanline, NO_DISCONTINUITY, NO_DISCONTINUITY, NO_DISCONTINUITY, 255);//depth_discontinuities[scanline][x2] = NO_DISCONTINUITY;
				}
				else if (deltaa2 > deltaa1)
				{
					m_pDisparity1->set_pixel(x2, scanline, deltaa2, deltaa2, deltaa2, 255);//disparity_map[scanline][x2] = deltaa2;
					m_pDiscontinuities1->set_pixel(x2, scanline, DISCONTINUITY, DISCONTINUITY, DISCONTINUITY, 255);//depth_discontinuities[scanline][x2] = DISCONTINUITY;
				}
				else
				{
					m_pDisparity1->set_pixel(x1-1, scanline, deltaa2, deltaa2, deltaa2, 255);//disparity_map[scanline][x1 - 1] = deltaa2;
					m_pDiscontinuities1->set_pixel(x1-1, scanline, DISCONTINUITY, DISCONTINUITY, DISCONTINUITY, 255);//depth_discontinuities[scanline][x1 - 1] = DISCONTINUITY;
					for (x = x1 - 2 ; x >= x2 ; x--)
					{
						m_pDisparity1->set_pixel(x, scanline, deltaa2, deltaa2, deltaa2, 255);//disparity_map[scanline][x] = deltaa2;
						m_pDiscontinuities1->set_pixel(x, scanline, NO_DISCONTINUITY, NO_DISCONTINUITY, NO_DISCONTINUITY, 255);//depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
					}
				}
				y1 = y2;                 deltaa1 = deltaa2;            x1 = y1 + deltaa1;
				y2 = pie_y[y1][deltaa1]; deltaa2 = pie_d[y1][deltaa1]; x2 = y2 + deltaa2;
			}

			for (x = y1 + deltaa1 - 1 ; x >= 0 ; x--)
			{
				m_pDisparity1->set_pixel(x, scanline, deltaa1, deltaa1, deltaa1, 255);//disparity_map[scanline][x] = deltaa1;
				m_pDiscontinuities1->set_pixel(x, scanline, NO_DISCONTINUITY, NO_DISCONTINUITY, NO_DISCONTINUITY, 255);//depth_discontinuities[scanline][x] = NO_DISCONTINUITY;
			}
		}
	}
}

void DisparityBirchfield::PostProcess(void)
{
}
