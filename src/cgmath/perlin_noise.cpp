#if 0
#include <stdlib.h>
#include <math.h>

#include "perlin_noise.h"


#define s_curve(t) ( t * t * (3. - 2. * t) )

#define lerp(t, a, b) ( a + t * (b - a) )

#define setup(i,b0,b1,r0,r1)\
	t = vec[i] + N;\
	b0 = ((int)t) & BM;\
	b1 = (b0+1) & BM;\
	r0 = t - (int)t;\
	r1 = r0 - 1.;

static void normalize2(float v[2])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
}

static void normalize3(float v[3])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
  v[2] = v[2] / s;
}

Cperlin_noise::Cperlin_noise ()
{
  init ();
}

void
Cperlin_noise::init (void)
{
  int i, j, k;
  
  for (i = 0 ; i < B ; i++) {
    p[i] = i;
    
    g1[i] = (float)((random() % (B + B)) - B) / B;
    
    for (j = 0 ; j < 2 ; j++)
      g2[i][j] = (float)((random() % (B + B)) - B) / B;
    normalize2(g2[i]);

    for (j = 0 ; j < 3 ; j++)
      g3[i][j] = (float)((random() % (B + B)) - B) / B;
    normalize3(g3[i]);
  }

  while (--i) {
    k = p[i];
    p[i] = p[j = random() % B];
    p[j] = k;
  }

  for (i = 0 ; i < B + 2 ; i++) {
    p[B + i] = p[i];
    g1[B + i] = g1[i];
    for (j = 0 ; j < 2 ; j++)
      g2[B + i][j] = g2[i][j];
    for (j = 0 ; j < 3 ; j++)
      g3[B + i][j] = g3[i][j];
  }
}

double
Cperlin_noise::noise1(double arg)
{
  int bx0, bx1;
  float rx0, rx1, sx, t, u, v, vec[1];

  vec[0] = arg;

  setup(0, bx0,bx1, rx0,rx1);

  sx = s_curve(rx0);

  u = rx0 * g1[ p[ bx0 ] ];
  v = rx1 * g1[ p[ bx1 ] ];

  return lerp(sx, u, v);
}

float
Cperlin_noise::noise2(float vec[2])
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  int i, j;

  setup(0, bx0,bx1, rx0,rx1);
  setup(1, by0,by1, ry0,ry1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  sx = s_curve(rx0);
  sy = s_curve(ry0);

#define at2(rx,ry) ( rx * q[0] + ry * q[1] )

  q = g2[ b00 ] ; u = at2(rx0,ry0);
  q = g2[ b10 ] ; v = at2(rx1,ry0);
  a = lerp(sx, u, v);

  q = g2[ b01 ] ; u = at2(rx0,ry1);
  q = g2[ b11 ] ; v = at2(rx1,ry1);
  b = lerp(sx, u, v);

  return lerp(sy, a, b);
}

float
Cperlin_noise::noise3(float vec[3])
{
  int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
  int i, j;

  setup(0, bx0,bx1, rx0,rx1);
  setup(1, by0,by1, ry0,ry1);
  setup(2, bz0,bz1, rz0,rz1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  t  = s_curve(rx0);
  sy = s_curve(ry0);
  sz = s_curve(rz0);

#define at3(rx,ry,rz) ( rx * q[0] + ry * q[1] + rz * q[2] )

  q = g3[ b00 + bz0 ] ; u = at3(rx0,ry0,rz0);
  q = g3[ b10 + bz0 ] ; v = at3(rx1,ry0,rz0);
  a = lerp(t, u, v);

  q = g3[ b01 + bz0 ] ; u = at3(rx0,ry1,rz0);
  q = g3[ b11 + bz0 ] ; v = at3(rx1,ry1,rz0);
  b = lerp(t, u, v);

  c = lerp(sy, a, b);

  q = g3[ b00 + bz1 ] ; u = at3(rx0,ry0,rz1);
  q = g3[ b10 + bz1 ] ; v = at3(rx1,ry0,rz1);
  a = lerp(t, u, v);

  q = g3[ b01 + bz1 ] ; u = at3(rx0,ry1,rz1);
  q = g3[ b11 + bz1 ] ; v = at3(rx1,ry1,rz1);
  b = lerp(t, u, v);

  d = lerp(sy, a, b);

  return lerp(sz, c, d);
}


#ifdef __ORIGINAL_VERSION__
/* coherent noise function over 1, 2 or 3 dimensions */
/* (copyright Ken Perlin) */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define B 0x100
#define BM 0xff

#define N 0x1000
#define NP 12   /* 2^N */
#define NM 0xfff

static int p[B + B + 2];
static float g3[B + B + 2][3];
static float g2[B + B + 2][2];
static float g1[B + B + 2];
static int start = 1;

static void init(void);

#define s_curve(t) ( t * t * (3. - 2. * t) )

#define lerp(t, a, b) ( a + t * (b - a) )

#define setup(i,b0,b1,r0,r1)\
	t = vec[i] + N;\
	b0 = ((int)t) & BM;\
	b1 = (b0+1) & BM;\
	r0 = t - (int)t;\
	r1 = r0 - 1.;

double noise1(double arg)
{
  int bx0, bx1;
  float rx0, rx1, sx, t, u, v, vec[1];

  vec[0] = arg;
  if (start) {
    start = 0;
    init();
  }

  setup(0, bx0,bx1, rx0,rx1);

  sx = s_curve(rx0);

  u = rx0 * g1[ p[ bx0 ] ];
  v = rx1 * g1[ p[ bx1 ] ];

  return lerp(sx, u, v);
}

float noise2(float vec[2])
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  int i, j;

  if (start) {
    start = 0;
    init();
  }

  setup(0, bx0,bx1, rx0,rx1);
  setup(1, by0,by1, ry0,ry1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  sx = s_curve(rx0);
  sy = s_curve(ry0);

#define at2(rx,ry) ( rx * q[0] + ry * q[1] )

  q = g2[ b00 ] ; u = at2(rx0,ry0);
  q = g2[ b10 ] ; v = at2(rx1,ry0);
  a = lerp(sx, u, v);

  q = g2[ b01 ] ; u = at2(rx0,ry1);
  q = g2[ b11 ] ; v = at2(rx1,ry1);
  b = lerp(sx, u, v);

  return lerp(sy, a, b);
}

float noise3(float vec[3])
{
  int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
  int i, j;

  if (start) {
    start = 0;
    init();
  }

  setup(0, bx0,bx1, rx0,rx1);
  setup(1, by0,by1, ry0,ry1);
  setup(2, bz0,bz1, rz0,rz1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  t  = s_curve(rx0);
  sy = s_curve(ry0);
  sz = s_curve(rz0);

#define at3(rx,ry,rz) ( rx * q[0] + ry * q[1] + rz * q[2] )

  q = g3[ b00 + bz0 ] ; u = at3(rx0,ry0,rz0);
  q = g3[ b10 + bz0 ] ; v = at3(rx1,ry0,rz0);
  a = lerp(t, u, v);

  q = g3[ b01 + bz0 ] ; u = at3(rx0,ry1,rz0);
  q = g3[ b11 + bz0 ] ; v = at3(rx1,ry1,rz0);
  b = lerp(t, u, v);

  c = lerp(sy, a, b);

  q = g3[ b00 + bz1 ] ; u = at3(rx0,ry0,rz1);
  q = g3[ b10 + bz1 ] ; v = at3(rx1,ry0,rz1);
  a = lerp(t, u, v);

  q = g3[ b01 + bz1 ] ; u = at3(rx0,ry1,rz1);
  q = g3[ b11 + bz1 ] ; v = at3(rx1,ry1,rz1);
  b = lerp(t, u, v);

  d = lerp(sy, a, b);

  return lerp(sz, c, d);
}

static void normalize2(float v[2])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
}

static void normalize3(float v[3])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
  v[2] = v[2] / s;
}

static void
init(void)
{
  int i, j, k;

  for (i = 0 ; i < B ; i++) {
    p[i] = i;

    g1[i] = (float)((random() % (B + B)) - B) / B;

    for (j = 0 ; j < 2 ; j++)
      g2[i][j] = (float)((random() % (B + B)) - B) / B;
    normalize2(g2[i]);

    for (j = 0 ; j < 3 ; j++)
      g3[i][j] = (float)((random() % (B + B)) - B) / B;
    normalize3(g3[i]);
  }

  while (--i) {
    k = p[i];
    p[i] = p[j = random() % B];
    p[j] = k;
  }

  for (i = 0 ; i < B + 2 ; i++) {
    p[B + i] = p[i];
    g1[B + i] = g1[i];
    for (j = 0 ; j < 2 ; j++)
      g2[B + i][j] = g2[i][j];
    for (j = 0 ; j < 3 ; j++)
      g3[B + i][j] = g3[i][j];
  }
}

#endif // __ORIGINAL_VERSION__

#endif // 0






#if 0

// Version 4d
// JAVA REFERENCE IMPLEMENTATION OF IMPROVED NOISE IN 4D - COPYRIGHT 2002 KEN PERLIN.

public final class ImprovedNoise4D {
   static public double noise(double x, double y, double z, double w) {
      int X = (int)Math.floor(x) & 255,                  // FIND UNIT HYPERCUBE
          Y = (int)Math.floor(y) & 255,                  // THAT CONTAINS POINT.
          Z = (int)Math.floor(z) & 255,
          W = (int)Math.floor(w) & 255;
      x -= Math.floor(x);                                // FIND RELATIVE X,Y,Z,W
      y -= Math.floor(y);                                // OF POINT IN CUBE.
      z -= Math.floor(z);
      w -= Math.floor(w);
      double a = fade(x),                                // COMPUTE FADE CURVES
             b = fade(y),                                // FOR EACH OF X,Y,Z,W.
             c = fade(z),
             d = fade(w);
      int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z,      // HASH COORDINATES OF
          B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z,      // THE 16 CORNERS OF
          AAA = p[AA]+W, AAB = p[AA+1]+W,                // THE HYPERCUBE.
          ABA = p[AB]+W, ABB = p[AB+1]+W,
          BAA = p[BA]+W, BAB = p[BA+1]+W,
          BBA = p[BB]+W, BBB = p[BB+1]+W;

      return lerp(d,                                     // INTERPOLATE DOWN.
          lerp(c,lerp(b,lerp(a,grad(p[AAA  ], x  , y  , z  , w), 
                               grad(p[BAA  ], x-1, y  , z  , w)),
                        lerp(a,grad(p[ABA  ], x  , y-1, z  , w), 
                               grad(p[BBA  ], x-1, y-1, z  , w))),

                 lerp(b,lerp(a,grad(p[AAB  ], x  , y  , z-1, w), 
                               grad(p[BAB  ], x-1, y  , z-1, w)),
                        lerp(a,grad(p[ABB  ], x  , y-1, z-1, w),
                               grad(p[BBB  ], x-1, y-1, z-1, w)))),

          lerp(c,lerp(b,lerp(a,grad(p[AAA+1], x  , y  , z  , w-1), 
                               grad(p[BAA+1], x-1, y  , z  , w-1)),
                        lerp(a,grad(p[ABA+1], x  , y-1, z  , w-1), 
                               grad(p[BBA+1], x-1, y-1, z  , w-1))),

                 lerp(b,lerp(a,grad(p[AAB+1], x  , y  , z-1, w-1), 
                               grad(p[BAB+1], x-1, y  , z-1, w-1)),
                        lerp(a,grad(p[ABB+1], x  , y-1, z-1, w-1),
                               grad(p[BBB+1], x-1, y-1, z-1, w-1)))));
   }
   static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
   static double lerp(double t, double a, double b) { return a + t * (b - a); }
   static double grad(int hash, double x, double y, double z, double w) {
      int h = hash & 31; // CONVERT LO 5 BITS OF HASH TO 32 GRAD DIRECTIONS.
      double a=y,b=z,c=w;            // X,Y,Z
      switch (h >> 3) {          // OR, DEPENDING ON HIGH ORDER 2 BITS:
      case 1: a=w;b=x;c=y;break;     // W,X,Y
      case 2: a=z;b=w;c=x;break;     // Z,W,X
      case 3: a=y;b=z;c=w;break;     // Y,Z,W
      }
      return ((h&4)==0 ? -a:a) + ((h&2)==0 ? -b:b) + ((h&1)==0 ? -c:c);
   }
   static final int p[] = new int[512], permutation[] = { 151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
   };
   static { for (int i=0; i < 256 ; i++) p[256+i] = p[i] = permutation[i]; }
}


#endif
