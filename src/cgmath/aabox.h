#pragma once

#include "TVector3.h"
#include "ray.h"
class Triangle;

class AABox {
public:
	AABox() { AddVertex (0., 0., 0.); }
	AABox(float x, float y, float z) {
		parameters[0].x = x;
		parameters[0].y = y;
		parameters[0].z = z;

		parameters[1].x = x;
		parameters[1].y = y;
		parameters[1].z = z;
	}
	AABox(const Vector3 &min, const Vector3 &max) {
		parameters[0] = min;
		parameters[1] = max;
	}

	void AddVertex (float x, float y, float z);
	bool contains (float x, float y, float z) const;

	void Dump (void);

	bool intersection (const Ray &, float t0, float t1) const; // (t0, t1) is the interval for valid hits

	// 
	bool contains (const Triangle &) const;
	bool intersection (Triangle &) const;
	
	// corners
	Vector3 parameters[2];
};
