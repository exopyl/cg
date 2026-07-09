#pragma once
#include "../cgmath/cgmath.h"

//
// Curvature type.
//
// Single, shared enumeration for the four curvature scalars derivable from a
// Tensor. It replaces the two previously divergent enums (Tensor::eCurvature
// and CurvatureId) which used the *same* value names in *inverted* orders, a
// silent min/max swap waiting to happen. Scoped (enum class) so the values
// can never implicitly convert to int or collide.
//
enum class CurvatureType { Min, Max, Mean, Gaussian };

//
// Tensor
//
class Tensor
{
public:
	Tensor () { Reset (); };
	~Tensor () {};
	
	//
	// getters - setters
	//
	void Reset (void)
		{
			SetNormal (0., 0., 0.);
			SetDirectionMax (0., 0., 0.);
			SetDirectionMin (0., 0., 0.);
			kappa_max = 0.0;
			kappa_min = 0.0;
		}
	void SetKappaMax (float kappa) { kappa_max = kappa; };
	void SetKappaMin (float kappa) { kappa_min = kappa; };

	void SetNormal (float x, float y, float z) { normal.Set (x, y, z);  };
	void SetNormal (const float *n) { normal.Set (n[0], n[1], n[2]); };
	void SetDirectionMax (float x, float y, float z) { direction_max.Set (x, y, z);  };
	void SetDirectionMax (const float *dmax) { direction_max.Set (dmax[0], dmax[1], dmax[2]); };
	void SetDirectionMin (float x, float y, float z) { direction_min.Set (x, y, z);  };
	void SetDirectionMin (const float *dmin) { direction_min.Set (dmin[0], dmin[1], dmin[2]); };

	void  GetNormal (float *n) { n[0]=normal.x; n[1]=normal.y; n[2]=normal.z; };
	float GetKappaMax (void) { return kappa_max; };
	float GetKappaMin (void) { return kappa_min; };
	void  GetDirectionMax (float *dmax) { dmax[0]=direction_max.x; dmax[1]=direction_max.y; dmax[2]=direction_max.z; };
	void  GetDirectionMin (float *dmin) { dmin[0]=direction_min.x; dmin[1]=direction_min.y; dmin[2]=direction_min.z; };

	//
	// derived curvatures
	//
	// Mean (H) and Gaussian (K) curvatures are *derived* from the principal
	// curvatures, so they live here rather than being recomputed with the
	// same literal formula at every call site.
	//
	float GetMeanCurvature (void) const { return (kappa_max + kappa_min) / 2.0f; };
	float GetGaussianCurvature (void) const { return kappa_max * kappa_min; };

	//! Return the requested curvature scalar.
	float GetCurvature (CurvatureType type) const
		{
			switch (type)
			{
			case CurvatureType::Min:      return kappa_min;
			case CurvatureType::Max:      return kappa_max;
			case CurvatureType::Mean:     return GetMeanCurvature ();
			case CurvatureType::Gaussian: return GetGaussianCurvature ();
			}
			return 0.0f;
		};

	void Dump (void)
		{
			printf ("tensor :\n");
			printf (" normale :\n   %f %f %f\n", normal[0], normal[1], normal[2]);
			printf ("   kappa_max = %f\n", kappa_max);
			printf ("   kappa_min = %f\n", kappa_min);
			printf (" direction max :\n   %f %f %f\n", direction_max[0], direction_max[1], direction_max[2]);
			printf (" direction min :\n   %f %f %f\n", direction_min[0], direction_min[1], direction_min[2]);
		}
	
	
private:
	Vector3f normal;
	float kappa_max, kappa_min;
	Vector3f direction_max;
	Vector3f direction_min;
};
