#ifndef __RAY_H__
#define __RAY_H__

#include "TVector3.h"

class Ray {
public:
	Ray() {}
	Ray(Vector3 o, Vector3 d)
		{
			origin = o;
			direction = d;
			inv_direction = Vector3(1/d.x, 1/d.y, 1/d.z);
			sign[0] = (inv_direction.x < 0);
			sign[1] = (inv_direction.y < 0);
			sign[2] = (inv_direction.z < 0);
		}
	Ray(float ox, float oy, float oz, float dx, float dy, float dz)
		{
			origin.Set (ox, oy, oz);
			direction.Set (dx, dy, dz);
			inv_direction = Vector3(1/dx, 1/dy, 1/dz);
			sign[0] = (inv_direction.x < 0);
			sign[1] = (inv_direction.y < 0);
			sign[2] = (inv_direction.z < 0);
		}
	Ray(const Ray &r)
		{
			origin = r.origin;
			direction = r.direction;
			inv_direction = r.inv_direction;
			sign[0] = r.sign[0]; sign[1] = r.sign[1]; sign[2] = r.sign[2];
		}
	
	void Dump (void);


	Vector3 origin;
	Vector3 direction;
	Vector3 inv_direction;
	int sign[3];
};

#endif // __RAY_H__
