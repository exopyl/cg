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
  vec3_init (N, 0.0, 0.0, 1.0);
  vec3_init (I, 0.0, 0.0, -1.0);
  vec3_init (Ci, 1.0, 1.0, 1.0);
  vec3_init (Oi, 1.0, 1.0, 1.0);
  vec3_init (Cs, 1.0, 1.0, 1.0);
  vec3_init (Os, 1.0, 1.0, 1.0);
  
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
  if (ptr == NULL) return;
  fprintf (ptr, "P6\n%d %d\n255\n", w, h);
  for (i=h-1; i>=0; i--)
    fwrite (&image[3*w*i], sizeof(unsigned char), 3*w, ptr);
  fclose (ptr);
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
Cperlin2::multiplication (vec3 &c, float f) /* c is a color */
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
Cperlin2::noise (vec3 res, vec3 p)
{
  res[0] = noise(p[0],p[1]);
  res[1] = noise(p[1],p[0]*2);
}

float
Cperlin2::vvvvnoise(vec3 p)
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
Cperlin2::mix(vec3 res, vec3 min, vec3 max, float val) /* colors */
{
  res[0] = (1-val)*min[0]+val*max[0];
  res[1] = (1-val)*min[1]+val*max[1];
  res[2] = (1-val)*min[2]+val*max[2];
}

void
Cperlin2::lisse(vec3 res, vec3 t1, vec3 t2, vec3 t3, vec3 t4, float t, matrice m)
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

  vec3_init (res, x, y, z);
}

void
Cperlin2::spline(vec3 res, float csp, vec3 c1, vec3 c2, vec3 c3, vec3 c4, vec3 c5, vec3 c6,
		 vec3 c7, vec3 c8, vec3 c9, vec3 c10, vec3 c11, vec3 c12, vec3 c13)
{
  int p =(int) (csp*10) ;
  if ( p == 10 )
    p = 9;
  float tt = csp*10 - p ;
  vec3 t1, t2, t3, t4;
  switch (p)
    {
    case 0:
      vec3_copy (t1, c1);
      vec3_copy (t2, c2);
      vec3_copy (t3, c3);
      vec3_copy (t4, c4);
      break;
    case 1:
      vec3_copy (t1, c2);
      vec3_copy (t2, c3);
      vec3_copy (t3, c4);
      vec3_copy (t4, c5);
      break;
    case 2:
      vec3_copy (t1, c3);
      vec3_copy (t2, c4);
      vec3_copy (t3, c5);
      vec3_copy (t4, c6);
      break;
    case 3:
      vec3_copy (t1, c4);
      vec3_copy (t2, c5);
      vec3_copy (t3, c6);
      vec3_copy (t4, c7);
      break;
    case 4:
      vec3_copy (t1, c5);
      vec3_copy (t2, c6);
      vec3_copy (t3, c7);
      vec3_copy (t4, c8);
      break;
    case 5:
      vec3_copy (t1, c6);
      vec3_copy (t2, c7);
      vec3_copy (t3, c8);
      vec3_copy (t4, c9);
      break;
    case 6:
      vec3_copy (t1, c7);
      vec3_copy (t2, c8);
      vec3_copy (t3, c9);
      vec3_copy (t4, c10);
      break ;
    case 7:
      vec3_copy (t1, c8);
      vec3_copy (t2, c9);
      vec3_copy (t3, c10);
      vec3_copy (t4, c11);
      break ;
    case 8:
      vec3_copy (t1, c9);
      vec3_copy (t2, c10);
      vec3_copy (t3, c11);
      vec3_copy (t4, c12);
      break ;
    case 9:
      vec3_copy (t1, c10);
      vec3_copy (t2, c11);
      vec3_copy (t3, c12);
      vec3_copy (t4, c13);
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

