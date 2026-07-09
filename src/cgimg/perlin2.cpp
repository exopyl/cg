#include <math.h>
#include <stdlib.h>

#include "perlin2.h"

#define mymax(a, b) ((a > b) ? a : b)
#define snoise(x) (2 * ((float) noise((x))) - 1)
#define boxstep(a,b,x) (clamp(((x)-(a))/((b)-(a)),0,1))
#define MINFILTERWIDTH 1.0e-7


static matrice m2 = { -0.5F, 1.5F,-1.5F, 0.5F,
		      1.0F,-2.5F, 2.0F,-0.5F,
		      -0.5F, 0.0F, 0.5F, 0.0F,
		      0.0F, 1.0F, 0.0F, 0.0F } ;


Cperlin2::Cperlin2 ()
{
  int i,j;
  persistence = 0.25;
  n_octaves   = 4;
  
  du = 0.0;
  dv = 0.0;
  N[0] = 0.0; N[1] = 0.0; N[2] = 1.0;
  I[0] = 0.0; I[1] = 0.0; I[2] = -1.0;
  Ci[0] = 1.0; Ci[1] = 1.0; Ci[2] = 1.0;
  Oi[0] = 1.0; Oi[1] = 1.0; Oi[2] = 1.0;
  Cs[0] = 1.0; Cs[1] = 1.0; Cs[2] = 1.0;
  Os[0] = 1.0; Os[1] = 1.0; Os[2] = 1.0;
  
  w = 240;
  h = 150;
  image = (unsigned char*) malloc (3*w*h*sizeof(unsigned char));
  for (i=0 ; i<h ; i++)
    {
      float t = (float) i/(w-1);
      for (j=0 ; j<w ; j++)
	{
	  int ind = (i*w+j)*3 ;
	  float s =(float) j/(w-1) ;
	  float Kd = 0.8F;
	  granite(s,t,Kd);
	  image[ind]   = (unsigned char) (Ci[0]*255);
	  image[ind+1] = (unsigned char) (Ci[1]*255);
	  image[ind+2] = (unsigned char) (Ci[2]*255);
	}	
    }
  FILE *ptr;
  ptr = fopen ("output.ppm", "w");
  if (ptr == nullptr) return;
  fprintf (ptr, "P6\n%d %d\n255\n", w, h);
  for (i=h-1; i>=0; i--)
    fwrite (&image[3*w*i], sizeof(unsigned char), 3*w, ptr);
  fclose (ptr);
}

Cperlin2::~Cperlin2 ()
{
  if (image)
    free (image);
}

float
Cperlin2::Interpolate (float a,float b,float x)
{
  float ft = x * 3.1415927 ;
  float f = (1 - cos(ft)) * .5 ;
  return  (a*(1-f) + b*f) ;
}

float
Cperlin2::Noise (int x)
{
  x = (x<<13)^x;
  return ( 1.0 - ( (x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

float
Cperlin2::Noise (int x, int y)
{
  int n = x + y * 57 ;
  n = (n<<13) ^ n;
  return ( 1.0 - ( (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0); 
}

float
Cperlin2::SmoothedNoise_1 (float x)
{
  return (Noise((int) x)/2  +  Noise((int) x-1)/4  +  Noise((int) x+1)/4) ;
}

float
Cperlin2::SmoothedNoise_1(float x, float y)
{
  float corners = ( Noise((int) x-1,(int) y-1)+Noise((int) x+1,(int) y-1)+Noise((int) x-1,(int) y+1)+Noise((int) x+1,(int) y+1) ) / 16 ;
  float sides   = ( Noise((int) x-1,(int) y)  +Noise((int) x+1,(int) y)  +Noise((int) x,(int) y-1)  +Noise((int) x,(int) y+1) ) /  8 ;
  float center  =  Noise((int) x,(int) y) / 4 ;
  return (corners + sides + center) ;
}
 
float
Cperlin2::InterpolatedNoise_1(float x)
{
  int integer_X =(int) floor(x) ;
  float fractional_X = x - integer_X ;
  float v1 = SmoothedNoise_1(integer_X) ;
  float v2 = SmoothedNoise_1(integer_X + 1) ;
  return (Interpolate(v1 , v2 , fractional_X)) ;
}

float
Cperlin2::InterpolatedNoise_1 (float x, float y)
{
  int integer_X =(int) floor(x) ;
  float fractional_X = x - integer_X ;
  int integer_Y =(int) floor(y) ;
  float fractional_Y = y - integer_Y ;
  float v1 = SmoothedNoise_1(integer_X,     integer_Y) ;
  float v2 = SmoothedNoise_1(integer_X + 1, integer_Y) ;
  float v3 = SmoothedNoise_1(integer_X,     integer_Y + 1) ;
  float v4 = SmoothedNoise_1(integer_X + 1, integer_Y + 1) ;
  float i1 = Interpolate(v1 , v2 , fractional_X) ;
  float i2 = Interpolate(v3 , v4 , fractional_X) ;
  return (Interpolate(i1 , i2 , fractional_Y)) ;
}

float
Cperlin2::PerlinNoise_1D (float x)
{
  float total = 0;
  float p = persistence;
  int n = n_octaves - 1;
  for ( int i = 0 ; i <= n ; i++ )
    {
      float frequency = 2<<i;
      float amplitude = pow(p,i);
      total += InterpolatedNoise_1(x * frequency) * amplitude;
    }
  return (total) ;
}

float
Cperlin2::PerlinNoise_2D (float x, float y)
{
  float total = 0;
  float p = persistence;
  int n = n_octaves - 1;
  for (int i = 0 ; i <= n ; i++)
    {
      float frequency = 2<<i;
      float amplitude = pow(p,i);
      total += InterpolatedNoise_1(x * frequency,y*frequency) * amplitude;
    }
  return (total) ;
}

void
Cperlin2::multiplication (float *c, float f) /* c is a color */
{
  c[0] *= f;
  c[1] *= f;
  c[2] *= f;
}

float
Cperlin2::noise (float s)
{
  return ((float) (1+PerlinNoise_1D(s))/2.0F);
}

float
Cperlin2::noise (float s,float t)
{
  return ((1+PerlinNoise_2D(s,t))/2.0F);
}

void
Cperlin2::noise (float *res, float *p)
{
  res[0] = noise(p[0],p[1]);
  res[1] = noise(p[1],p[0]*2);
}

float
Cperlin2::vvvvnoise(float *p)
{
  return(noise(p[0],p[1]));
}

float
Cperlin2::clamp(float v,float min,float max)
{
  if ( v < min )
    return(min) ;
  if ( v > max )
    return(max) ;
  return(v) ;
}

float
Cperlin2::smoothstep(float min,float max,float val)
{
  if ( val < min )
    return(min) ;
  if ( val > max )
    return(max) ;
  return((max-min)*val+min) ;
}

void
Cperlin2::mix(float *res, float *min, float *max, float val) /* colors */
{
  res[0] = (1-val)*min[0]+val*max[0];
  res[1] = (1-val)*min[1]+val*max[1];
  res[2] = (1-val)*min[2]+val*max[2];
}

void
Cperlin2::lisse(float *res, float *t1, float *t2, float *t3, float *t4, float t, matrice m)
{ 
  int j,k;
  float tt[4],ttt[4],x,y,z;
  tt[0] = t*t*t ;
  tt[1] = t*t ;
  tt[2] = t ;
  tt[3] = 1 ;
  for (j=0; j<4; j++)
    for (k=0, ttt[j]=0; k<4 ;k++)
      ttt[j] += tt[k] * m[k][j] ;
  x = y = z = 0 ;

  x += ttt[0] * t1[0];
  y += ttt[0] * t1[1];
  z += ttt[0] * t1[2];

  x += ttt[1] * t2[0];
  y += ttt[1] * t2[1];
  z += ttt[1] * t2[2];

  x += ttt[2] * t3[0];
  y += ttt[2] * t3[1];
  z += ttt[2] * t3[2];

  x += ttt[3] * t4[0];
  y += ttt[3] * t4[1];
  z += ttt[3] * t4[2];

  res[0] = x; res[1] = y; res[2] = z;
}

void
Cperlin2::spline(float *res, float csp, float *c1, float *c2, float *c3, float *c4, float *c5, float *c6,
		 float *c7, float *c8, float *c9, float *c10, float *c11, float *c12, float *c13)
{
  int p =(int) (csp*10) ;
  if ( p == 10 )
    p = 9;
  float tt = csp*10 - p ;
  float t1[3], t2[3], t3[3], t4[3];
  switch (p)
    {
    case 0:
      t1[0] = c1[0]; t1[1] = c1[1]; t1[2] = c1[2];
      t2[0] = c2[0]; t2[1] = c2[1]; t2[2] = c2[2];
      t3[0] = c3[0]; t3[1] = c3[1]; t3[2] = c3[2];
      t4[0] = c4[0]; t4[1] = c4[1]; t4[2] = c4[2];
      break;
    case 1:
      t1[0] = c2[0]; t1[1] = c2[1]; t1[2] = c2[2];
      t2[0] = c3[0]; t2[1] = c3[1]; t2[2] = c3[2];
      t3[0] = c4[0]; t3[1] = c4[1]; t3[2] = c4[2];
      t4[0] = c5[0]; t4[1] = c5[1]; t4[2] = c5[2];
      break;
    case 2:
      t1[0] = c3[0]; t1[1] = c3[1]; t1[2] = c3[2];
      t2[0] = c4[0]; t2[1] = c4[1]; t2[2] = c4[2];
      t3[0] = c5[0]; t3[1] = c5[1]; t3[2] = c5[2];
      t4[0] = c6[0]; t4[1] = c6[1]; t4[2] = c6[2];
      break;
    case 3:
      t1[0] = c4[0]; t1[1] = c4[1]; t1[2] = c4[2];
      t2[0] = c5[0]; t2[1] = c5[1]; t2[2] = c5[2];
      t3[0] = c6[0]; t3[1] = c6[1]; t3[2] = c6[2];
      t4[0] = c7[0]; t4[1] = c7[1]; t4[2] = c7[2];
      break;
    case 4:
      t1[0] = c5[0]; t1[1] = c5[1]; t1[2] = c5[2];
      t2[0] = c6[0]; t2[1] = c6[1]; t2[2] = c6[2];
      t3[0] = c7[0]; t3[1] = c7[1]; t3[2] = c7[2];
      t4[0] = c8[0]; t4[1] = c8[1]; t4[2] = c8[2];
      break;
    case 5:
      t1[0] = c6[0]; t1[1] = c6[1]; t1[2] = c6[2];
      t2[0] = c7[0]; t2[1] = c7[1]; t2[2] = c7[2];
      t3[0] = c8[0]; t3[1] = c8[1]; t3[2] = c8[2];
      t4[0] = c9[0]; t4[1] = c9[1]; t4[2] = c9[2];
      break;
    case 6:
      t1[0] = c7[0]; t1[1] = c7[1]; t1[2] = c7[2];
      t2[0] = c8[0]; t2[1] = c8[1]; t2[2] = c8[2];
      t3[0] = c9[0]; t3[1] = c9[1]; t3[2] = c9[2];
      t4[0] = c10[0]; t4[1] = c10[1]; t4[2] = c10[2];
      break ;
    case 7:
      t1[0] = c8[0]; t1[1] = c8[1]; t1[2] = c8[2];
      t2[0] = c9[0]; t2[1] = c9[1]; t2[2] = c9[2];
      t3[0] = c10[0]; t3[1] = c10[1]; t3[2] = c10[2];
      t4[0] = c11[0]; t4[1] = c11[1]; t4[2] = c11[2];
      break ;
    case 8:
      t1[0] = c9[0]; t1[1] = c9[1]; t1[2] = c9[2];
      t2[0] = c10[0]; t2[1] = c10[1]; t2[2] = c10[2];
      t3[0] = c11[0]; t3[1] = c11[1]; t3[2] = c11[2];
      t4[0] = c12[0]; t4[1] = c12[1]; t4[2] = c12[2];
      break ;
    case 9:
      t1[0] = c10[0]; t1[1] = c10[1]; t1[2] = c10[2];
      t2[0] = c11[0]; t2[1] = c11[1]; t2[2] = c11[2];
      t3[0] = c12[0]; t3[1] = c12[1]; t3[2] = c12[2];
      t4[0] = c13[0]; t4[1] = c13[1]; t4[2] = c13[2];
      break;
    }
  lisse (res, t1, t2, t3, t4, tt, m2);
}

void Cperlin2::granite (float s,float t,float Kd)
{
  float sum = 0;
  float freq = 1.0;
  for (int i=0 ; i<6 ; i++)
    {
      sum += fabs(.5 - noise(12*freq*s,12*freq*t))/freq*1.4 ;
      freq *= 2; }
  if ( sum > 1/Kd )
    sum = 1/Kd ;
  Ci[0] = sum * Kd;
  Ci[1] = sum * Kd;
  Ci[2] = sum * Kd;
}

