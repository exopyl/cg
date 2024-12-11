#ifndef __SURFACE_IMPLICIT_SAMPLES_H__
#define __SURFACE_IMPLICIT_SAMPLES_H__

#include "surface_implicit.h"

extern void update_time (float fnewTime);

extern float fSample0 (float, float, float);
extern float fSample1 (float, float, float);
extern float fSample2 (float, float, float);
extern float fSample3 (float, float, float);
extern float fSample4 (float, float, float);
extern float fSample5 (float, float, float);
extern float fSample6 (float, float, float);
extern float fSample7 (float, float, float);

#define nSamples 8
static float (*scalar_functions[nSamples]) (float, float, float) = {fSample0,
								    fSample1,
								    fSample2,
								    fSample3,
								    fSample4,
								    fSample5,
								    fSample6,
								    fSample7};



#endif // __SURFACE_IMPLICIT_SAMPLES_H__

