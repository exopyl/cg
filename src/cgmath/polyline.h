#pragma once

//
// ProfilePolyline : a discrete list of 3D points.
//
// This is NOT a parametric curve (no eval(t)); it is a profile used to
// generate revolution surfaces and frames, and to import/export OBJ point
// lists. It was formerly named CurveDiscrete and has been reclassified out of
// the curve hierarchy.
//
class ProfilePolyline
{
public:
	ProfilePolyline ();
	~ProfilePolyline ();

	int init (int nVertices);
	int set_position (int pi, float x, float y, float z);

	int inverse_order (void);

	int generate_surface_revolution (unsigned int nSlices, unsigned int *_nVertices, float **_pVertices, unsigned int *_nFaces, unsigned int **_pFaces);
	int generate_frame (float width, float height, unsigned int *_nVertices, float **_pVertices, unsigned int *_nFaces, unsigned int **_pFaces);

	int import_obj (char *filename);
	int export_obj (char *filename);
private:
	int m_nPoints;
	float *m_pPoints;
};
