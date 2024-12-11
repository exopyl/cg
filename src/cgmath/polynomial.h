#ifndef __POLYNOMIAL_H__
#define __POLYNOMIAL_H__

// quadric ax^2 + bx + c = 0
// cubic ax^3 + bx^2 + cx + d = 0
// quartic ax^4 + bx^3 + cx^2 + dx + e = 0

extern float polynomial2_eval     (float a, float b, float c, float x);
extern int polynomial2_find_roots (float a, float b, float c, float roots[2]);
extern float polynomial3_eval     (float a, float b, float c, float d, float x);
extern int polynomial3_find_roots (float a, float b, float c, float d, float roots[3]);

// => Herbison-Evans
extern int quartic (double a,double b,double c,double d,double rts[4],double rterr[4]);

// = >Schwarze
extern int SolveQuadric (double c[3], double s[2]);
extern int SolveCubic   (double c[4], double s[3]);
extern int SolveQuartic (double c[5], double s[4]);


#endif // __POLYNOMIAL_H__

