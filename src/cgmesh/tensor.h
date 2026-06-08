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

	void SetNormal (float x, float y, float z) { vec3_init (normal, x, y, z);  };
	void SetNormal (vec3 n) { vec3_copy (normal, n); };
	void SetDirectionMax (float x, float y, float z) { vec3_init (direction_max, x, y, z);  };
	void SetDirectionMax (vec3 dmax) { vec3_copy (direction_max, dmax); };
	void SetDirectionMin (float x, float y, float z) { vec3_init (direction_min, x, y, z);  };
	void SetDirectionMin (vec3 dmin) { vec3_copy (direction_min, dmin); };

	void  GetNormal (vec3 n) { vec3_copy (n, normal); };
	float GetKappaMax (void) { return kappa_max; };
	float GetKappaMin (void) { return kappa_min; };
	void  GetDirectionMax (vec3 dmax) { vec3_copy (dmax, direction_max); };
	void  GetDirectionMin (vec3 dmin) { vec3_copy (dmin, direction_min); };

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
	vec3 normal;
	float kappa_max, kappa_min;
	vec3 direction_max;
	vec3 direction_min;
};
