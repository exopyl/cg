#include <stdio.h>
#include "offscreen_rendering_factory.h"
#include "../cgmesh/cgmesh.h"

void
take_photographs (Mesh *model, char *filefront, char *fileside, char *fileup)
{
	float eyex, eyey, eyez;
	float cx, cy, cz;
	float upx, upy, upz;
	float l, r, b, t;
	float near_plane, far_plane;
	
	int nv = model->m_nVertices;
	float *v = model->m_pVertices;
	int nf = model->m_nFaces;
	const BoundingBox& bbox= model->bbox ();
	float min[3], max[3];
	bbox.GetMinMax(min, max);
	float xmin = min[0];
	float xmax = max[0];
	float ymin = min[1];
	float ymax = max[1];
	float zmin = min[2];
	float zmax = max[2];



	// create the offscreen_factory
	Coffscreen_rendering *ofr = new Coffscreen_rendering (model);

	// front
	printf ("FRONT:\n");
	eyex = xmax; eyey = 0.0; eyez = 0.0;
	cx = 0.0; cy = 0.0; cz = 0.0;
	upx = 0.0; upy = 0.0; upz = 1.0;

	l = ymin; r = ymax;
	b = zmin; t = zmax;

	near_plane = 0.0;
	far_plane  = xmax - xmin + 1.0;

	ofr->set_parameters_glulookat (eyex, eyey, eyez, cx, cy, cz, upx, upy, upz);
	ofr->set_parameters_glortho (l, r, b, t, near_plane, far_plane);

	printf ("front : %f x %f\n", r-l, t-b);
	ofr->rendering (filefront);
	ofr->save_render (filefront);


	// side
	printf ("SIDE:\n");
	eyex = 0.0; eyey = ymin; eyez = 0.0;
	cx = 0.0; cy = 0.0; cz = 0.0;
	upx = 0.0; upy = 0.0; upz = 1.0;

	l = xmin; r = xmax;
	b = zmin; t = zmax;

	near_plane = 0.0;
	far_plane  = ymax - ymin + 1.0;

	ofr->set_parameters_glulookat (eyex, eyey, eyez, cx, cy, cz, upx, upy, upz);
	ofr->set_parameters_glortho (l, r, b, t, near_plane, far_plane);

	printf ("side : %f x %f\n", r-l, t-b);
	ofr->rendering (fileside);
	ofr->save_render (fileside);


	// up
	printf ("UP:\n");
	eyex = 0.0; eyey = 0.0; eyez = zmax;
	cx = 0.0; cy = 0.0; cz = 0.0;
	upx = -1.0; upy = 0.0; upz = 0.0;

	l = ymin; r = ymax;
	b = -xmax; t = -xmin;

	near_plane = 0.0;
	far_plane  = zmax - zmin + 1.0;

	ofr->set_parameters_glulookat (eyex, eyey, eyez, cx, cy, cz, upx, upy, upz);
	ofr->set_parameters_glortho (l, r, b, t, near_plane, far_plane);

	printf ("up : %f x %f\n", r-l, t-b);
	ofr->rendering (fileup);
	ofr->save_render (fileup);

	// destroy the offscreen factory
	delete ofr;
}


