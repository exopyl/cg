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

  void multiplication (vec3 &c, float f);
  float noise (float s);
  float noise (float s, float t);
  void noise (vec3 res, vec3 p);
  float vvvvnoise (vec3 p);
  float clamp (float v,float min,float max);
  float smoothstep (float min,float max,float val);
  void mix (vec3 res, vec3 min, vec3 max, float val);
  void lisse (vec3 res, vec3 t1, vec3 t2, vec3 t3, vec3 t4, float t, matrice m);
  void spline(vec3 res, float csp, vec3 c1, vec3 c2, vec3 c3, vec3 c4, vec3 c5, vec3 c6,
	      vec3 c7, vec3 c8, vec3 c9, vec3 c10, vec3 c11, vec3 c12, vec3 c13);
  
 private:
  float persistence;
  int   n_octaves;

  float du, dv;
  vec3 N, I;
  vec3 Ci, Oi, Cs, Os;

  unsigned char *image;
  int w, h;
};
