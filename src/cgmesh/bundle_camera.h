#pragma once

#include "../cgmath/cgmath.h"

// Row-major 4x4 so the raw on-disk layout (R[0..2] = first row, ...) read by
// Bundle::Load/Load2 maps directly onto at(row, col).
using CameraMatrix = TMatrix4<float, StorageOrder::RowMajor>;

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
	CameraMatrix R;
	Vector3f T;
	CameraMatrix Rinv; // inverse R, directly updated from R
	Vector3f d; // direction of the camera, directly update from R
	Vector3f pos; // position of the camera directly updated from Rinv
};
