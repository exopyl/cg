#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <math.h>
#include <float.h>

#ifdef _MSC_VER

#pragma warning (disable : 4244) // disable : "conversion from 'double' to 'float', possible loss of data
#pragma warning (disable : 4305) // disable : "truncation from 'double' to 'float'
#pragma warning (disable : 4530) // disable : "C++ exception handler used"

#endif // _MSC_VER


// Constants rounded for 21 decimals
#ifndef M_E
#define M_E 2.71828182845904523536
#endif // M_E

#ifndef M_LOG2E
#define M_LOG2E 1.44269504088896340736
#endif // M_LOG2E

#ifndef M_LOG10E
#define M_LOG10E 0.434294481903251827651
#endif // M_LOG10E

#ifndef M_LN2
#define M_LN2 0.693147180559945309417
#endif // M_LN2

#define M_LN10 2.30258509299404568402
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif // M_PI
#define M_PI_2 1.57079632679489661923

#ifndef M_PI_4
#define M_PI_4 0.785398163397448309616
#endif // M_PI_4

#ifndef M_1_PI
#define M_1_PI 0.318309886183790671538
#endif // M_1_PI

#ifndef M_2_PI
#define M_2_PI 0.636619772367581343076
#endif // M_2_PI

#define M_1_SQRTPI 0.564189583547756286948
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT_2 0.707106781186547524401

#define DEGTORAD(x) ((x)*M_PI/180.)
#define RADTODEG(x) ((x)*180./M_PI)

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.1920928955078125e-07
#endif
#define EPSILON FLT_EPSILON

/* standard macros */
#ifndef ABS
#define ABS(a) (((a) < 0) ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN3
#define MIN3(a, b, c)  (MIN (MIN( (a) , (b) ) , (c) ))
#endif
#ifndef MAX3
#define MAX3(a, b, c)  (MAX (MAX( (a) , (b) ) , (c) ))
#endif

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifdef WIN32
#define SWAP(a, b) do { decltype(a) __tmp = a; a = b; b = __tmp; } while(0)
#else
#define SWAP(a, b) do { __typeof__(a) __tmp = a; a = b; b = __tmp; } while(0)
#endif

#define SAFE_DELETE(pointer)       if ((pointer) != NULL) { delete (pointer);    (pointer) = NULL; }
#define SAFE_ARRAY_DELETE(pointer) if ((pointer) != NULL) { delete [] (pointer); (pointer) = NULL; }
#define SAFE_FREE(pointer)         if ((pointer) != NULL) { free (pointer);      (pointer) = NULL; }


#ifdef _DEBUG
#define DASSERT(x) assert(x)
#define DPRINTF(x) printf(x)
#else
#define DASSERT(x)
#define DPRINTF(x)
#endif

static inline int sign(float number)
{
	if (number > 0.0f)
		return 1;
	if (number < 0.0f)
		return -1;
	else
		return 0;
}

// interpolations
extern float smoothstep (float a, float b, float x);

extern float interpolation_linear (float a, float b, float x);
extern float interpolation_cosine (float a, float b, float x);
extern float interpolation_cubic  (float a, float b, float c, float d, float x);

// filters
extern float filter_raised_cosine_filter (float T, float alpha, float x);

// output
extern void output_1array (float *signal, int size, char *filename);
extern void output_2array (float *signal1, float *signal2, int size, char *filename);

// sort
extern int* quicksort_indices (float *array, int size);

extern void sort (int *array, int n);

// sort array1 and update array2
extern void sort_2arrays (float *array1, float *array2, int n);

float search_min (float *array, int n);
float search_max (float *array, int n);
void convolution (float *signal, int n, float **_convolution);

// compute 1/sqrt(x) using SSE instructions for efficiency
// from Nick nicolas@capens.net
inline float rsq (float x)
{
#ifdef SSE
	__asm
	{
		movss xmm0, x
		rsqrtss xmm0, xmm0
		movss x, xmm0
	}
	return x;
#else
	return 1.0f / sqrt (x);
#endif
}

// unique IDs generator (singleton)
class IDGenerator
{
private:
	IDGenerator () : m_iCurrentId(0) {};
	~IDGenerator () {};
public:
	static IDGenerator* getInstance (void) { return m_pInstance; }

	unsigned int newId (void) { return m_iCurrentId++; };
private:
	unsigned int m_iCurrentId;
	static IDGenerator *m_pInstance;
};

#ifdef _MSC_VER

//const double infmyreal = std::numeric_limits<double>::infinity();
//const double nanmyreal = std::numeric_limits<double>::quiet_NaN();

//const double inf = (std::numeric_limits<double>::infinity());
//const double nan = (std::numeric_limits<double>::quiet_NaN());
//const double NAN = (std::numeric_limits<double>::quiet_NaN());

#ifndef INFINITY
#define INFINITY numeric_limits<float>::infinity()
#endif // INFINITY

#ifdef WIN32
    #ifndef NAN
        static const unsigned long __nan[2] = {0xffffffff, 0x7fffffff};
        #define NAN (*(const float *) __nan)
    #endif
#endif

#define isnan(x) ((x) != (x))

#else

// On linux, the std constants are incorrect, so we have
// to trick the compiler into producing inf and nan without
// producing a compile-time warning. Since sin (0.0) = 0.0,
// dividing byt it hides the divide by zero at compile time
// but procudes the correct constants.

/*
const double infmyreal = 1.0 / sin (0.0);
const double nanmyreal = 0.0 / sin (0.0);
const double inf = 1.0 / sin (0.0);
const double nan = 0.0 / sin (0.0);
*/

#endif // _MSC_VER

#endif // __COMMON_H__
