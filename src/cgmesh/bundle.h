#ifndef __BUNDLE_H__
#define __BUNDLE_H__

#include "cgmesh.h"

#include "bundle_camera.h"

class Bundle
{
public:
	Bundle ();
	~Bundle ();

	int Load (char *filename);
	int Load2 (char *bundlefilename, char *imageslistfilename, char *rootpath);

	int DeleteRedundantCameras (void);
	int project_textures_naive (Mesh *mesh, char *bundleoutfilename, char *imageslistfilename, char *rootpath);
	//int project_textures_lempitsky07 (Mesh *mesh, char *bundleoutfilename, char *imageslistfilename, char *rootpath);
	
public:
// cameras
	Mesh *mesh;
	unsigned int cameras_n;
	BundleCamera **cameras;

// points
	unsigned int n_points;
	unsigned int **pt_visible_from_cameras; // for each pt, stores the cameras from where it is visible
	unsigned int *n_pt_visible_from_cameras; // for each pt, number of cameras from where it is visible
};

#endif // __BUNDLE_H__
