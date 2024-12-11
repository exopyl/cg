#include <stdio.h>
#include <stdlib.h>

#include "polygon2.h"

#ifdef WIN32
#include <windows.h>
#include <gl/GLU.h>
#else
#if 0
#include "glutess/sgi-glu.h"
#endif
#endif // WIN32

///////////////////////
//
// user data structure
//
struct glutess_state {
	// tesselation
	float *v;
	unsigned int *f;
	unsigned int fn;
	unsigned int vn;

	unsigned int vc;
	unsigned int tri[3];
	unsigned int face;
};
//
///////////////////////

///////////////////////
//
// Callbacks for "mesh_tesselate"
//
#if 0
static void glutess_begin(int type, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;

	state->vc = 0;
	//
	// we only get triangles here because we set
	// GLU_TESS_EDGE_FLAG callback to non-NULL
	//
	(void) type;
}

static void glutess_vertex(void *vertex_data, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	unsigned int vi = (unsigned int) vertex_data;
	state->tri[state->vc] = vi;

	if (state->vc == 2)
	{
		// generate face
		unsigned int fi = state->fn++;
		if (state->face != (unsigned int)-1)
		{
			// copy of existing face
			unsigned int fp = state->face;
			for (unsigned int j = 0; j < 3; j++)
			{
				unsigned int k = state->tri[j];
				state->f[3*fi+j] = state->f[3*fp+k];
			}
		}
		else
		{
			// new face
			state->f[3*fi]   = state->tri[0];
			state->f[3*fi+1] = state->tri[1];
			state->f[3*fi+2] = state->tri[2];
		}

		state->vc = 0;
	} else
		state->vc++;
}

static void glutess_edge_flag(int unused)
{
	(void) unused;
}

static void glutess_end(void *user_data)
{
	(void) user_data;
}

static void glutess_error(int errno, void *user_data)
{
	(void) user_data;

	printf ("Tessellation Error: %d\n", errno);
}

static void glutess_combine(double coords[3],
			    void *vertex_data[4],
			    float weight[4],
			    void **outData,
			    void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	unsigned int *f = state->f;
	unsigned int vi, v0, v1, v2, v3;

	// generate vertex
	vi = state->vn++;
	if (vi == (unsigned int) -1)
	{
		printf ("tesselation error: could not allocate vertex\n");
		return;
	}

	state->v[3*vi]   = (float) coords[0];
	state->v[3*vi+1] = (float) coords[1];
	state->v[3*vi+2] = (float) coords[2];

	if (state->face != (unsigned int)-1)
	{
		printf ("case not treated yet !!!\n");
/*
		unsigned int fp = state->face;
		unsigned int fvn = mesh_face_vn(mesh, fp);

		// face is self-intersecting
		// XXX: add new vertex at end
		mesh_face_resize(mesh, fp, fvn + 1);
		mesh_face_vi(mesh, fp, fvn) = vi;

		v0 = (unsigned int) vertex_data[0];
		v1 = (unsigned int) vertex_data[1];
		v2 = (unsigned int) vertex_data[2];
		v3 = (unsigned int) vertex_data[3];

		*outData = (void *) fvn;

		printf("combine added vertex %d to face %d at %d\n", vi, fp, fvn);
		*/
	} else
		*outData = (void *) vi;
}
#endif
//
///////////////////////


// tesselation
int Polygon2::tesselate (float **_pVertices, unsigned int *_nVertices,
			  unsigned int **_pFaces, unsigned int *_nFaces)
{
#if 0
	GLUtesselator *tess;
	double *coords;
	unsigned int nVertices;
	struct glutess_state state;

	// compute the number of vertices
	nVertices = get_n_points();
	coords = (double*)malloc(3*sizeof(double)*nVertices);
	if (coords == NULL)
		return NULL;

	state.v = (float*)malloc(1000*sizeof(float));
	state.vn = 0;
	state.f = (unsigned int*)malloc(1000*sizeof(unsigned int));
	state.fn = 0;
	state.face = (unsigned int)-1;

	tess = gluNewTess();
	
#ifdef WIN32
	gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void (__stdcall *)(void)) glutess_begin);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void (__stdcall *) (void)) glutess_vertex);
	gluTessCallback(tess, GLU_TESS_END_DATA, (void (__stdcall *) (void)) glutess_end);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void (__stdcall *) (void)) glutess_error);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void (__stdcall *) (void)) glutess_combine);

	// this will force GL_TRIANGLES rendering
	gluTessCallback(tess, GLU_TESS_EDGE_FLAG, (void (__stdcall *) (void)) &glutess_edge_flag);
#else
	gluTessCallback(tess, GLU_TESS_BEGIN_DATA,(void(*)()) &glutess_begin);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(*)()) &glutess_vertex);
	gluTessCallback(tess, GLU_TESS_END_DATA, (void(*)()) &glutess_end);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(*)()) &glutess_error);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(*)()) &glutess_combine);

	// this will force GL_TRIANGLES rendering
	gluTessCallback(tess, GLU_TESS_EDGE_FLAG, (void(*)()) &glutess_edge_flag); // dont compile with g++
#endif


	gluTessNormal(tess, 0, 0, 1);

	int WindingRule = GLU_TESS_WINDING_NONZERO; // normal
	//WindingRule = GLU_TESS_WINDING_POSITIVE; // used to get the "negative polygon"
	gluTessProperty(tess, GLU_TESS_WINDING_RULE, WindingRule);

	gluTessBeginPolygon(tess, &state);
	unsigned int iVertex = 0;
	for (unsigned int j=0; j<get_n_contours(); j++)
	{
		gluTessBeginContour(tess);
		float *pts = get_points(j);
		for (unsigned int i=0; i<get_n_points(j); i++)
		{
			coords[3 * iVertex + 0] = (double) pts[2*i];
			coords[3 * iVertex + 1] = (double) pts[2*i+1];
			coords[3 * iVertex + 2] = (double) 0.;
			//printf ("%f %f %f\n", coords[3 * iVertex + 0], coords[3 * iVertex + 1], coords[3 * iVertex + 2]);
			state.v[3*iVertex+0] = (float) pts[2*i];
			state.v[3*iVertex+1] = (float) pts[2*i+1];
			state.v[3*iVertex+2] = (float) 0.;
			gluTessVertex(tess, &coords[3 * iVertex], (void *) iVertex);
			iVertex++;
		}
		gluTessEndContour(tess);
	}
	gluTessEndPolygon(tess);

	gluDeleteTess(tess);

	*_pVertices = state.v;
	*_nVertices = iVertex+state.vn;
	*_pFaces = state.f;
	*_nFaces = state.fn;

	free(coords);
#endif
	return 0;
}
