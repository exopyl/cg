#ifndef __CURVE_DISCRETE_H__
#define __CURVE_DISCRETE_H__

class CurveDiscrete
{
public:
	CurveDiscrete ();
	~CurveDiscrete ();

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

#endif // __CURVE_DISCRETE_H__
