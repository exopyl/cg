#ifndef __POLYGON2_H__
#define __POLYGON2_H__

#define SIGNATURE_DEVIATION 0
#define SIGNATURE_CURVATURE 1
#define SIGNATURE_BOUTIN    2

#define INTERPOLATION_LINEAR 0
#define INTERPOLATION_COSINE 1

//
// Contour 2D
//
class Polygon2
{
public:
	Polygon2 ();
	Polygon2 (const Polygon2 &pol);
	~Polygon2 ();

	Polygon2 &operator=(const Polygon2 &pol);

	// input
	void input (float *pPoints, int n);
	void input (float *x, float *y, int n);
	void input (Polygon2 *contour, int interpolation_type, int nn); // parameterize an existing contour
	void input (char *filename);
private:
	void input_from_svg_path (char *path);

public:
	// edit
	float* add_contour (unsigned int index, unsigned int nPoints, float *pPoints);
	int add_polygon2d (Polygon2 *pol);

	// output
	void dump ();
	void output (char *filename);

	int set_point (unsigned int iContour, unsigned int iPoint, float x, float y);
	int get_point (unsigned int iContour, unsigned int iPoint, float *x, float *y);

	int    get_n_contours (void) { return m_nContours; };
	float* get_points (unsigned int iContour) { return m_pPoints[iContour]; };
	int    get_n_points (unsigned int iContour) { return m_nPoints[iContour]; };
	int    get_n_points (void) {
		int nPoints = 0;
		for (unsigned int i=0; i<m_nContours; i++)
			nPoints += get_n_points (i);
		return nPoints; };
	
	void  get_bbox (float *xmin, float *xmax, float *ymin, float *ymax);
	float length (int interpolation_type);
	float area   (void);
	float area   (int i1, int i2, int i3);
	
	void smooth (void);
	void flip_x (void);
	void apply_PCA (void);
	void translate (float tx, float ty);
	void rotate (float angle);
	void centerize (void);
	void center (float *xc, float *yc);
	
	int is_point_inside (float x, float y);

	// order of the points
	void inverse_order (void);
	int  is_trigonometric_order (void);
	
	int  generalized_barycentric_coordinates (float pt[2], float *coords);

	// moments
	void moment_0 (float m[1]);
	void moment_1 (float m[2]);
	void moment_2 (float m[3], bool centralized=false);
	void moment_3 (float m[4]);
	
	// symmetry
	void search_symmetry_zabrodsky (float *xc, float *yc, float *angle);
	void search_symmetry_signature (int signature_type, int interpolation_type, int nbins);
	void search_symmetry_pridmore (void);
	void search_symmetry_pridmore_bis (void);
	
	// matching
	float matching_arkin (Polygon2 *contour, int nn);
	
	// clean a polygon
	int clean (Polygon2* polygon);

	// tesselation
	int tesselate (float **pVertices, unsigned int *nVertices,
		       unsigned int **pFaces, unsigned int *nFaces);

	void thicken (Polygon2* polygon, float ithickness, float othickness, int bOpen=1);
	void dilate (Polygon2* polygon, float d);

	void extrude (float fHeight, char *filename);
	
//private:
	int alloc_contours (int nContours);
	
	//
	unsigned int m_nContours;
	unsigned int *m_nPoints;
	float **m_pPoints;
};


#endif // __POLYGON2_H__
