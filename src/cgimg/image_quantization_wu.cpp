/**********************************************************************
	    C Implementation of Wu's Color Quantizer (v. 2)
	    (see Graphics Gems vol. II, pp. 126-133)

Author:	Xiaolin Wu
	Dept. of Computer Science
	Univ. of Western Ontario
	London, Ontario N6A 5B7
	wu@csd.uwo.ca

Algorithm: Greedy orthogonal bipartition of RGB space for variance
	   minimization aided by inclusion-exclusion tricks.
	   For speed no nearest neighbor search is done. Slightly
	   better performance can be expected by more sophisticated
	   but more expensive versions.

The author thanks Tom Lane at Tom_Lane@G.GP.CS.CMU.EDU for much of
additional documentation and a cure to a previous bug.

Free to distribute, comments and suggestions are appreciated.
**********************************************************************/	

/*
 * modified to be compiled with g++
 */

#include <stdio.h>
#include <stdlib.h>

#define MAXCOLOR	256
#define	RED	2
#define	GREEN	1
#define BLUE	0

struct box {
    int r0;			 /* min value, exclusive */
    int r1;			 /* max value, inclusive */
    int g0;  
    int g1;  
    int b0;  
    int b1;
    int vol;
};

/* Histogram is in elements 1..HISTSIZE along each axis,
 * element 0 is for base or marginal value
 * NB: these must start out 0!
 */

float		m2[33][33][33];
long int	wt[33][33][33], mr[33][33][33],	mg[33][33][33],	mb[33][33][33];
unsigned char   *Ir, *Ig, *Ib;
int	        size; /*image size*/
int		K;    /*color look-up table size*/
unsigned short int *Qadd;

void Hist3d(long int vwt[33][33][33], long int vmr[33][33][33], long int vmg[33][33][33], long int vmb[33][33][33], float m2[33][33][33]) 
/* build 3-D color histogram of counts, r/g/b, c^2 */
{
int ind, r, g, b;
int	     inr, ing, inb, table[256];
long int i;
		
	for(i=0; i<256; ++i) table[i]=i*i;
	Qadd = (unsigned short int *)malloc(sizeof(unsigned short int)*size);
	if (Qadd==NULL) {printf("Not enough space\n"); exit(1);}
	for(i=0; i<size; ++i){
	    r = Ir[i]; g = Ig[i]; b = Ib[i];
	    inr=(r>>3)+1; 
	    ing=(g>>3)+1; 
	    inb=(b>>3)+1; 
	    Qadd[i]=ind=(inr<<10)+(inr<<6)+inr+(ing<<5)+ing+inb;

	    // original version :
	    //++vwt[ind];
	    //vmr[ind] += r;
	    //vmg[ind] += g;
	    //vmb[ind] += b;
     	    //m2[ind] += (float)(table[r]+table[g]+table[b]);

	    // modified as follows to compile with g++ :
	    vwt[inr][ing][inb]++;
	    vmr[inr][ing][inb] += r;
	    vmg[inr][ing][inb] += g;
	    vmb[inr][ing][inb] += b;
	    m2[inr][ing][inb] += (float)(table[r]+table[g]+table[b]);;
	}
}

/* At conclusion of the histogram step, we can interpret
 *   wt[r][g][b] = sum over voxel of P(c)
 *   mr[r][g][b] = sum over voxel of r*P(c)  ,  similarly for mg, mb
 *   m2[r][g][b] = sum over voxel of c^2*P(c)
 * Actually each of these should be divided by 'size' to give the usual
 * interpretation of P() as ranging from 0 to 1, but we needn't do that here.
 */

/* We now convert histogram into moments so that we can rapidly calculate
 * the sums of the above quantities over any desired box.
 */


void
M3d(long int vwt[33][33][33], long int vmr[33][33][33], long int vmg[33][33][33], long int vmb[33][33][33], float m2[33][33][33]) /* compute cumulative moments. */
{
	//register unsigned short int ind1, ind2;
	unsigned char i, r, g, b;
	long int line, line_r, line_g, line_b, area[33], area_r[33], area_g[33], area_b[33];
	float line2, area2[33];

    for(r=1; r<=32; ++r){
	for(i=0; i<=32; ++i) 
	{
	    area[i]=area_r[i]=area_g[i]=area_b[i]=0;
		area2[i]=0.f;
	}
	for(g=1; g<=32; ++g){
	    line = line_r = line_g = line_b = 0;
		line2 = 0.;
	    for(b=1; b<=32; ++b){

		// original version :
		//ind1 = (r<<10) + (r<<6) + r + (g<<5) + g + b; /* [r][g][b] */
		//line += vwt[ind1];
		//line_r += vmr[ind1]; 
		//line_g += vmg[ind1]; 
		//line_b += vmb[ind1];
		//line2 += m2[ind1];
		// modified as follows to compile with g++ :
		line += vwt[r][g][b];
		line_r += vmr[r][g][b]; 
		line_g += vmg[r][g][b]; 
		line_b += vmb[r][g][b];
		line2 += m2[r][g][b];

		area[b] += line;
		area_r[b] += line_r;
		area_g[b] += line_g;
		area_b[b] += line_b;
		area2[b] += line2;

		// original version :
		//ind2 = ind1 - 1089; /* [r-1][g][b] */
		//vwt[ind1] = vwt[ind2] + area[b];
		//vmr[ind1] = vmr[ind2] + area_r[b];
		//vmg[ind1] = vmg[ind2] + area_g[b];
		//vmb[ind1] = vmb[ind2] + area_b[b];
		//m2[ind1] = m2[ind2] + area2[b];
		// modified as follows to compile with g++ :
		vwt[r][g][b] = vwt[r-1][g][b] + area[b];
		vmr[r][g][b] = vmr[r-1][g][b] + area_r[b];
		vmg[r][g][b] = vmg[r-1][g][b] + area_g[b];
		vmb[r][g][b] = vmb[r-1][g][b] + area_b[b];
		m2[r][g][b] = m2[r-1][g][b] + area2[b];
	    }
	}
    }
}


long int Vol(struct box *cube, long int mmt[33][33][33]) 
/* Compute sum over a box of any given statistic */
{
    return( mmt[cube->r1][cube->g1][cube->b1] 
	   -mmt[cube->r1][cube->g1][cube->b0]
	   -mmt[cube->r1][cube->g0][cube->b1]
	   +mmt[cube->r1][cube->g0][cube->b0]
	   -mmt[cube->r0][cube->g1][cube->b1]
	   +mmt[cube->r0][cube->g1][cube->b0]
	   +mmt[cube->r0][cube->g0][cube->b1]
	   -mmt[cube->r0][cube->g0][cube->b0] );
}

/* The next two routines allow a slightly more efficient calculation
 * of Vol() for a proposed subbox of a given box.  The sum of Top()
 * and Bottom() is the Vol() of a subbox split in the given direction
 * and with the specified new upper bound.
 */

long int Bottom(struct box *cube, unsigned char dir, long int mmt[33][33][33])
/* Compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1 */
/* (depending on dir) */
{
    switch(dir){
	case RED:
	    return( -mmt[cube->r0][cube->g1][cube->b1]
		    +mmt[cube->r0][cube->g1][cube->b0]
		    +mmt[cube->r0][cube->g0][cube->b1]
		    -mmt[cube->r0][cube->g0][cube->b0] );
	    break;
	case GREEN:
	    return( -mmt[cube->r1][cube->g0][cube->b1]
		    +mmt[cube->r1][cube->g0][cube->b0]
		    +mmt[cube->r0][cube->g0][cube->b1]
		    -mmt[cube->r0][cube->g0][cube->b0] );
	    break;
	case BLUE:
	    return( -mmt[cube->r1][cube->g1][cube->b0]
		    +mmt[cube->r1][cube->g0][cube->b0]
		    +mmt[cube->r0][cube->g1][cube->b0]
		    -mmt[cube->r0][cube->g0][cube->b0] );
	    break;
	default:
		return 0;
   }
}


long int Top(struct box *cube, unsigned char dir, int pos, long int mmt[33][33][33])
/* Compute remainder of Vol(cube, mmt), substituting pos for */
/* r1, g1, or b1 (depending on dir) */
{
    switch(dir){
	case RED:
	    return( mmt[pos][cube->g1][cube->b1] 
		   -mmt[pos][cube->g1][cube->b0]
		   -mmt[pos][cube->g0][cube->b1]
		   +mmt[pos][cube->g0][cube->b0] );
	    break;
	case GREEN:
	    return( mmt[cube->r1][pos][cube->b1] 
		   -mmt[cube->r1][pos][cube->b0]
		   -mmt[cube->r0][pos][cube->b1]
		   +mmt[cube->r0][pos][cube->b0] );
	    break;
	case BLUE:
	    return( mmt[cube->r1][cube->g1][pos]
		   -mmt[cube->r1][cube->g0][pos]
		   -mmt[cube->r0][cube->g1][pos]
		   +mmt[cube->r0][cube->g0][pos] );
	    break;
	default:
		return 0;
    }
}


float Var(struct box *cube)
/* Compute the weighted variance of a box */
/* NB: as with the raw statistics, this is really the variance * size */
{
	float dr, dg, db, xx;

    dr = (float)Vol(cube, mr); 
    dg = (float)Vol(cube, mg); 
    db = (float)Vol(cube, mb);
    xx =  m2[cube->r1][cube->g1][cube->b1] 
	 -m2[cube->r1][cube->g1][cube->b0]
	 -m2[cube->r1][cube->g0][cube->b1]
	 +m2[cube->r1][cube->g0][cube->b0]
	 -m2[cube->r0][cube->g1][cube->b1]
	 +m2[cube->r0][cube->g1][cube->b0]
	 +m2[cube->r0][cube->g0][cube->b1]
	 -m2[cube->r0][cube->g0][cube->b0];

    return( xx - (dr*dr+dg*dg+db*db)/(float)Vol(cube,wt) );    
}

/* We want to minimize the sum of the variances of two subboxes.
 * The sum(c^2) terms can be ignored since their sum over both subboxes
 * is the same (the sum for the whole box) no matter where we split.
 * The remaining terms have a minus sign in the variance formula,
 * so we drop the minus sign and MAXIMIZE the sum of the two terms.
 */


float Maximize(struct box *cube, unsigned char dir, int first, int last, int *cut,
	       long int whole_r, long int whole_g, long int whole_b, long int whole_w)
{
long int half_r, half_g, half_b, half_w;
long int base_r, base_g, base_b, base_w;
int i;
float temp, max;

    base_r = Bottom(cube, dir, mr);
    base_g = Bottom(cube, dir, mg);
    base_b = Bottom(cube, dir, mb);
    base_w = Bottom(cube, dir, wt);
    max = 0.0;
    *cut = -1;
    for(i=first; i<last; ++i){
	half_r = base_r + Top(cube, dir, i, mr);
	half_g = base_g + Top(cube, dir, i, mg);
	half_b = base_b + Top(cube, dir, i, mb);
	half_w = base_w + Top(cube, dir, i, wt);
        /* now half_x is sum over lower half of box, if split at i */
        if (half_w == 0) {      /* subbox could be empty of pixels! */
          continue;             /* never split into an empty box */
	} else
        temp = ((float)half_r*half_r + (float)half_g*half_g +
                (float)half_b*half_b)/half_w;

	half_r = whole_r - half_r;
	half_g = whole_g - half_g;
	half_b = whole_b - half_b;
	half_w = whole_w - half_w;
        if (half_w == 0) {      /* subbox could be empty of pixels! */
          continue;             /* never split into an empty box */
	} else
        temp += ((float)half_r*half_r + (float)half_g*half_g +
                 (float)half_b*half_b)/half_w;

    	if (temp > max) {max=temp; *cut=i;}
    }
    return(max);
}

int
Cut(struct box *set1, struct box *set2)
{
unsigned char dir;
int cutr, cutg, cutb;
float maxr, maxg, maxb;
long int whole_r, whole_g, whole_b, whole_w;

    whole_r = Vol(set1, mr);
    whole_g = Vol(set1, mg);
    whole_b = Vol(set1, mb);
    whole_w = Vol(set1, wt);

    maxr = Maximize(set1, RED, set1->r0+1, set1->r1, &cutr,
		    whole_r, whole_g, whole_b, whole_w);
    maxg = Maximize(set1, GREEN, set1->g0+1, set1->g1, &cutg,
		    whole_r, whole_g, whole_b, whole_w);
    maxb = Maximize(set1, BLUE, set1->b0+1, set1->b1, &cutb,
		    whole_r, whole_g, whole_b, whole_w);

    if( (maxr>=maxg)&&(maxr>=maxb) ) {
	dir = RED;
	if (cutr < 0) return 0; /* can't split the box */
    }
    else
    if( (maxg>=maxr)&&(maxg>=maxb) ) 
	dir = GREEN;
    else
	dir = BLUE; 

    set2->r1 = set1->r1;
    set2->g1 = set1->g1;
    set2->b1 = set1->b1;

    switch (dir){
	case RED:
	    set2->r0 = set1->r1 = cutr;
	    set2->g0 = set1->g0;
	    set2->b0 = set1->b0;
	    break;
	case GREEN:
	    set2->g0 = set1->g1 = cutg;
	    set2->r0 = set1->r0;
	    set2->b0 = set1->b0;
	    break;
	case BLUE:
	    set2->b0 = set1->b1 = cutb;
	    set2->r0 = set1->r0;
	    set2->g0 = set1->g0;
	    break;
    }

    set1->vol=(set1->r1-set1->r0)*(set1->g1-set1->g0)*(set1->b1-set1->b0);
    set2->vol=(set2->r1-set2->r0)*(set2->g1-set2->g0)*(set2->b1-set2->b0);

    return 1;
}


void Mark(struct box *cube, int label, unsigned char *tag)
{
int r, g, b;

    for(r=cube->r0+1; r<=cube->r1; ++r)
       for(g=cube->g0+1; g<=cube->g1; ++g)
	  for(b=cube->b0+1; b<=cube->b1; ++b)
	    tag[(r<<10) + (r<<6) + r + (g<<5) + g + b] = label;
}

int MedianCut_Wu(unsigned char *pPixels, int iSize, int ncolors)
{
	struct box			cube[MAXCOLOR];
	unsigned char		*tag;
	unsigned char		lut_r[MAXCOLOR], lut_g[MAXCOLOR], lut_b[MAXCOLOR];
	int					next;
	long int	i, weight;
	int		k;
	float				vv[MAXCOLOR], temp;

	/* input R,G,B components into Ir, Ig, Ib;
	   set size to width*height */
	size = iSize;
	Ir = (unsigned char*)malloc(size*sizeof(unsigned char));
	Ig = (unsigned char*)malloc(size*sizeof(unsigned char));
	Ib = (unsigned char*)malloc(size*sizeof(unsigned char));
	for (i=0; i<size; i++)
	{
		Ir[i] = pPixels[4*i];
		Ig[i] = pPixels[4*i+1];
		Ib[i] = pPixels[4*i+2];
	}

	//no. of colors
	K = ncolors;

	for (int ii=0; ii<33; ii++)
		for (int jj=0; jj<33; jj++)
			for (int kk=0; kk<33; kk++)
			{
				mr[ii][jj][kk] = 0;
				mg[ii][jj][kk] = 0;
				mb[ii][jj][kk] = 0;
				m2[ii][jj][kk] = 0.;
			}

	Hist3d(wt, mr, mg, mb, m2);
	free(Ig); free(Ib); free(Ir);
	printf("Histogram done\n");

	M3d(wt, mr, mg, mb, m2);
	printf("Moments done\n");

	cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
	cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;
	next = 0;
        for(i=1; i<K; ++i){
            if (Cut(&cube[next], &cube[i]))
	    {
		    /* volume test ensures we won't try to cut one-cell box */
		    vv[next] = (cube[next].vol>1) ? (float)Var(&cube[next]) : 0.f;
		    vv[i] = (cube[i].vol>1) ? (float)Var(&cube[i]) : 0.f;
	    }
	    else
	    {
              vv[next] = 0.0;   /* don't try to split this box again */
              i--;              /* didn't create box i */
	    }

	    next = 0; temp = vv[0];
            for(k=1; k<=i; ++k)
                if (vv[k] > temp) {
                    temp = vv[k]; next = k;
		}
            if (temp <= 0.0) {
              K = i+1;
              fprintf(stderr, "Only got %d boxes\n", K);
              break;
	    }
	}
    	printf("Partition done\n");

	/* the space for array m2 can be freed now */

	tag = (unsigned char *)malloc(33*33*33);
	if (tag==NULL) {printf("Not enough space\n"); exit(1);}
	for(k=0; k<K; ++k){
	    Mark(&cube[k], k, tag);
	    weight = Vol(&cube[k], wt);
	    if (weight) {
		lut_r[k] = (unsigned char)(Vol(&cube[k], mr) / weight);
		lut_g[k] = (unsigned char)(Vol(&cube[k], mg) / weight);
		lut_b[k] = (unsigned char)(Vol(&cube[k], mb) / weight);
	    }
	    else{
	      fprintf(stderr, "bogus box %d\n", k);
	      lut_r[k] = lut_g[k] = lut_b[k] = 0;		
	    }
	}

	for(i=0; i<size; ++i) Qadd[i] = tag[Qadd[i]];
	
	/* output lut_r, lut_g, lut_b as color look-up table contents,
	   Qadd as the quantized image (array of table addresses). */

	for (i=0; i<size; i++)
	{
		pPixels[4*i]   = lut_r[Qadd[i]];
		pPixels[4*i+1] = lut_g[Qadd[i]];
		pPixels[4*i+2] = lut_b[Qadd[i]];
	}

	return 0;
}
