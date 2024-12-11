#include <stdio.h>

#include "bundle_camera.h"

BundleCamera::BundleCamera ()
{
	filename = NULL;
	w = 0;
	h = 0;

	f_pxl = 0.;
	f_mm = 0.;
	CCDWidth_mm = 0.;
	CCDHeight_mm = 0.;
	k1 = 0.;
	k2 = 0.;

	mat4_set_identity (R);
	mat4_set_identity (Rinv);
	vec3_init (T, 0., 0., 0.);
	vec3_init (d, 0., 0., 0.);
	vec3_init (pos, 0., 0., 0.);
}

BundleCamera::~BundleCamera ()
{
}

void BundleCamera::Dump ()
{
	printf ("[camera] filename : %s (%d x %d)\n", filename, w, h);
	printf ("[camera] focal length : %f (pxl) %f (mm)\n", f_pxl, f_mm);
	printf ("[camera] CCD width : %f (mm)\n", CCDWidth_mm);
	printf ("[camera] CCD height : %f (mm)\n", CCDHeight_mm);
	printf ("[camera] radial distoriont coefficients : %f %f\n", k1, k2);
	printf ("[camera] rotation :\n %f %f %f\n %f %f %f\n %f %f %f\n",
		R[0], R[1], R[2],
		R[4], R[5], R[6],
		R[8], R[9], R[10]);
	printf ("[camera] rotation inv :\n %f %f %f\n %f %f %f\n %f %f %f\n",
		Rinv[0], Rinv[1], Rinv[2],
		Rinv[4], Rinv[5], Rinv[6],
		Rinv[8], Rinv[9], Rinv[10]);
	printf ("[camera] translation : %f %f %f\n", T[0], T[1], T[2]);
	printf ("[camera] direction : %f %f %f\n", d[0], d[1], d[2]);
}
