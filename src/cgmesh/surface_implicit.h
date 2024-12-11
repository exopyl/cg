#ifndef __SURFACE_IMPLICIT_H__
#define __SURFACE_IMPLICIT_H__

#include <stdlib.h>

struct vec3f { float fX, fY, fZ; };

typedef float (*mc_eval_func_t)   (float, float, float);
typedef void  (*mc_color_func_t)  (float, float, float);
typedef bool  (*mc_vertex_func_t) (float, float, float, int, void*);
typedef void  (*mc_normal_func_t) (float, float, float);
typedef void  (*mc_face_completed_func_t) (void*);
typedef void  (*mc_layer_completed_func_t) (void*);

class ImplicitSurface
{
public:
	ImplicitSurface ();
	~ImplicitSurface () {};

	inline void set_bbox (float minx, float miny, float minz, float maxx, float maxy, float maxz)
		{
			min[0] = minx; min[1] = miny; min[2] = minz;
			max[0] = maxx; max[1] = maxy; max[2] = maxz;
		};
	inline void get_bbox (vec3f &vmin, vec3f &vmax)
		{
			vmin.fX = min[0]; vmin.fY = min[1]; vmin.fZ = min[2];
			vmax.fX = max[0]; vmax.fY = max[1]; vmax.fZ = max[2];
		};
	inline void set_value (float _fValue) { fValue = _fValue; };
	inline float get_value (float _fValue) { return fValue; };
	inline void set_boundary (int _bBoundary) { bBoundary = _bBoundary; };
	inline void set_orientation (int bInf)
		{
			orientation[0] = 0;
			if (bInf)
			{
				orientation[1] = 1;
				orientation[2] = 2;
			}
			else
			{
				orientation[1] = 2;
				orientation[2] = 1;
			}
		}
	inline void set_resolution_per_unit (int _resolution)
		{
			for (int i=0; i<3; i++)
			{
				resolution[i] = (int)((max[i]-min[i])*_resolution);
				step[i] = (max[i]-min[i])/(resolution[i]);
			}
		};
	inline void set_eval_func (mc_eval_func_t _eval_func) { eval_func = _eval_func; };
	inline void set_color_func (mc_color_func_t _color_func) { color_func = _color_func; };
	inline void set_normal_func (mc_normal_func_t _normal_func) { normal_func = _normal_func; };
	inline void set_vertex_func (mc_vertex_func_t _vertex_func) { vertex_func = _vertex_func; };
	inline void set_face_completed_func (mc_face_completed_func_t _face_completed_func) { face_completed_func = _face_completed_func; };

	void compute (int bUsecache);
	void get_triangulation (int *nvertices, float **vertices, int *nfaces, unsigned int **faces);

	void get_normal (vec3f &pt, vec3f &normal);

protected:
	virtual void get_triangulation_pre (void);
	virtual void get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces);

	mc_face_completed_func_t face_completed_func;
	mc_layer_completed_func_t layer_completed_func;
	void *data;

private:
	void compute_cube (unsigned int iX, unsigned int iY, unsigned iZ);
	int orientation[3];
	int bBoundary;

	float min[3], max[3];
	float resolution[3];
	float step[3];
	float fValue;
	
	mc_eval_func_t eval_func;
	mc_color_func_t color_func;
	mc_vertex_func_t vertex_func;
	mc_normal_func_t normal_func;

	// temporary variables used in mc_compute
	float *fValueCached;
	int *fIndicesCached;
	int iCurrentVertex;
};

#endif // __SURFACE_IMPLICIT_H__
