#include <stdio.h>
#include <stdlib.h>

#include "polygon2.h"
#ifdef USE_GLUTESS
#include "glutess/sgi-glu.h"
#endif

// Callbacks for "polygon_merge_contours"
struct glutess_state {
	Polygon2 *polygon;
	unsigned int n_vertices_per_contour;
	unsigned int ntmp;
	unsigned int current_contour;
	unsigned int current_vertex;
	double *tmp;
};

static void glutess_begin(int type, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	printf ("begin contour %d\n", state->current_contour);

	if (state->current_contour >= polygon->m_nContours) {
		printf ("realloc polygon\n");
		polygon->m_pPoints = (float**)realloc(polygon->m_pPoints, (polygon->m_nContours + 1)*sizeof(float*));
		polygon->m_nPoints = (unsigned int*)realloc(polygon->m_nPoints, (polygon->m_nContours + 1)*sizeof(unsigned int));
		polygon->m_nPoints[polygon->m_nContours] = 0;
		polygon->m_nContours++;
	}

	polygon->m_pPoints[state->current_contour] = (float*)malloc(2*state->n_vertices_per_contour*sizeof(float));
	polygon->m_nPoints[state->current_contour] = state->n_vertices_per_contour;

	state->current_vertex = 0;
	(void) type;
}

static void glutess_end(void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	polygon->m_nPoints[state->current_contour] = state->current_vertex;
	state->current_contour++;
	// optional but cleaner : resize correctly the size of polygon->pContours
}

static void glutess_vertex(void *vertex_data, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	unsigned long ntmp = (unsigned long) vertex_data;
	double *coords = (double*)&state->tmp[3*ntmp];

	if (state->current_vertex >= polygon->m_nPoints[state->current_contour]) {
		unsigned int nVertices = polygon->m_nPoints[state->current_contour];
		polygon->m_pPoints[state->current_contour] = (float*)realloc(polygon->m_pPoints[state->current_contour], 2*(nVertices+1)*sizeof(float));
		polygon->m_nPoints[state->current_contour] = nVertices + 1;
	}

	polygon->set_point (state->current_contour, state->current_vertex, coords[0], coords[1]);

	state->current_vertex++;
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
	//dbg ("COMBINE : %f %f", coords[0], coords[1]);
	(void) vertex_data;
	(void) weight;

	struct glutess_state *state = (struct glutess_state *) user_data;

	state->tmp = (double*)realloc (state->tmp, (state->ntmp+1) * 2 * sizeof(double));
	state->tmp[2*state->ntmp+0] = coords[0];
	state->tmp[2*state->ntmp+1] = coords[1];
	//state->tmp[3*state->ntmp+2] = coords[2];

	*outData = (void *)(unsigned long) state->ntmp;
	state->ntmp++;
}

//
// merge contours defining a polygon to avoid overlapping contours
//
int Polygon2::clean (Polygon2* polygon)
{
#ifdef USE_GLUTESS
	if (polygon == NULL || polygon->m_nContours == 0)
		return -1;

	// polygon merged
	m_nContours = 5*polygon->m_nContours;
	m_nPoints = (unsigned int*) malloc (m_nContours*sizeof(unsigned int));
	m_pPoints = (float**) malloc (m_nContours*sizeof(float*));

	for (unsigned int i=0; i<polygon->m_nContours; i++)
	{
		m_pPoints[i] = NULL;
		m_nPoints[i] = 0;
	}

	// compute the number of vertices
	unsigned int nVertices = 0;
	for (unsigned int i=0; i<polygon->m_nContours; i++)
		nVertices += polygon->m_nPoints[i];

	double *coords = (double*) malloc(2 * sizeof(double) * nVertices);
	if (coords == NULL)
		return NULL;

	struct glutess_state state;
	state.polygon = this;//polygon_merged;
	state.n_vertices_per_contour = 2*nVertices;
	state.ntmp = nVertices;
	state.tmp = coords;
	state.current_contour = 0;

	GLUtesselator *tess = gluNewTess();

	gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void(*)()) &glutess_begin);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(*)()) &glutess_vertex);
	gluTessCallback(tess, GLU_TESS_END_DATA, (void(*)()) &glutess_end);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(*)()) &glutess_error);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(*)()) &glutess_combine);

	gluTessNormal(tess, 0, 0, 1);

	gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
	gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, 1);
	//gluTessProperty(tess, GLU_TESS_TOLERANCE, 0.0);
	
	gluTessBeginPolygon(tess, &state);
	unsigned int iVertex = 0;
	for (unsigned int j=0; j<polygon->m_nContours; j++)
	{
		gluTessBeginContour(tess);
		for (unsigned int i=0; i<polygon->m_nPoints[j]; i++)
		{
			state.tmp[2 * iVertex + 0] = (double) polygon->m_pPoints[j][2*i];
			state.tmp[2 * iVertex + 1] = (double) polygon->m_pPoints[j][2*i+1];
			//state.tmp[3 * iVertex + 2] = (double) polygon->m_pPoints[j][i].p[2];
			//dbg ("adding %d : %f %f %f", i, coords[3 * iVertex + 0], coords[3 * iVertex + 1], coords[3 * iVertex + 2]);

			gluTessVertex(tess, &state.tmp[2 * iVertex], (void *)(unsigned long) iVertex);
			iVertex++;
		}
		gluTessEndContour(tess);
	}
	gluTessEndPolygon(tess);

	gluDeleteTess(tess);

	// cleaning
	free(state.tmp);
#endif
	return 0;
}
