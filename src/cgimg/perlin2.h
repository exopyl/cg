#pragma once

#include "../cgmath/cgmath.h"

typedef float matrice[4][4] ;

class Cperlin2
{
 public:
  Cperlin2 ();
  ~Cperlin2 ();

  void granite (float s, float t, float Kd);

  /* methods */
 private:
  float Interpolate (float a,float b,float x);
  float Noise (int x);
  float Noise (int x, int y);
  float SmoothedNoise_1 (float x);
  float SmoothedNoise_1 (float x, float y);
  float InterpolatedNoise_1 (float x);
  float InterpolatedNoise_1 (float x, float y);
  float PerlinNoise_1D (float x);
  float PerlinNoise_2D (float x, float y);

  void multiplication (float *c, float f);
  float noise (float s);
  float noise (float s, float t);
  void noise (float *res, float *p);
  float vvvvnoise (float *p);
  float clamp (float v,float min,float max);
  float smoothstep (float min,float max,float val);
  void mix (float *res, float *min, float *max, float val);
  void lisse (float *res, float *t1, float *t2, float *t3, float *t4, float t, matrice m);
  void spline(float *res, float csp, float *c1, float *c2, float *c3, float *c4, float *c5, float *c6,
	      float *c7, float *c8, float *c9, float *c10, float *c11, float *c12, float *c13);
  
 private:
  float persistence;
  int   n_octaves;

  float du, dv;
  float N[3], I[3];
  float Ci[3], Oi[3], Cs[3], Os[3];

  unsigned char *image;
  int w, h;
};
