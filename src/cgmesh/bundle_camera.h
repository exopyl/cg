#ifndef __BUNDLE_CAMERA_H__
#define __BUNDLE_CAMERA_H__

#include "../cgmath/cgmath.h"

class BundleCamera
{
	friend class Bundle;
public:
	BundleCamera ();
	~BundleCamera ();

	void Dump();

public:
	// photo
	char *filename;
	unsigned int w;
	unsigned int h;

	// internal geometry
	float f_mm; // focal length (mm)
	float f_pxl; // focal length (pixels)
	float CCDWidth_mm; // ccd width (mm)
	float CCDHeight_mm;
	float k1, k2;

	// external geometry
	mat4 R;
	vec3 T;
	mat4 Rinv; // inverse R, directly updated from R
	vec3 d; // direction of the camera, directly update from R
	vec3 pos; // position of the camera directly updated from Rinv
};

#endif // __BUNDLE_CAMERA_H__
