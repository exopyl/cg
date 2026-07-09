#include <stdio.h>

#include "bundle_camera.h"

BundleCamera::BundleCamera ()
{
	filename = nullptr;
	w = 0;
	h = 0;

	f_pxl = 0.;
	f_mm = 0.;
	CCDWidth_mm = 0.;
	CCDHeight_mm = 0.;
	k1 = 0.;
	k2 = 0.;

	R.SetIdentity ();
	Rinv.SetIdentity ();
	T.Set (0., 0., 0.);
	d.Set (0., 0., 0.);
	pos.Set (0., 0., 0.);
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
		R.at(0,0), R.at(0,1), R.at(0,2),
		R.at(1,0), R.at(1,1), R.at(1,2),
		R.at(2,0), R.at(2,1), R.at(2,2));
	printf ("[camera] rotation inv :\n %f %f %f\n %f %f %f\n %f %f %f\n",
		Rinv.at(0,0), Rinv.at(0,1), Rinv.at(0,2),
		Rinv.at(1,0), Rinv.at(1,1), Rinv.at(1,2),
		Rinv.at(2,0), Rinv.at(2,1), Rinv.at(2,2));
	printf ("[camera] translation : %f %f %f\n", T[0], T[1], T[2]);
	printf ("[camera] direction : %f %f %f\n", d[0], d[1], d[2]);
}
