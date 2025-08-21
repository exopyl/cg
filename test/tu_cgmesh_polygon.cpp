#include <stdio.h>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

TEST(TEST_cgmesh_polygon, misc)
{
	float xmin, xmax, ymin, ymax;

	char * input = (char*)"./test/data/polygon1.dat";

	Polygon2 *porig = new Polygon2 ();
	porig->input (input);

	Polygon2 *p1 = new Polygon2 (*porig);
	//p1->clean (porig);

	//p1->dump ();
	p1->output ((char*)"polygon1.dat");
	p1->output ((char*)"polygon1.obj");
	p1->output ((char*)"polygon1.pgm");
	p1->output ((char*)"polygon1.ppm");

	p1->extrude (200., (char*)"polygon_extruded.obj");
	printf ("Length : %f\n", p1->length (INTERPOLATION_LINEAR));
	printf ("Area : %f\n", p1->area ());

	Polygon2 *p_thicker = new Polygon2 (*p1);
	p_thicker->thicken (p1, 10., 10., 1);
	p_thicker->output ((char*)"polygon_thicker.obj");
	delete p_thicker;

	Polygon2 *p_dilate = new Polygon2 (*p1);
	p_dilate->dilate (p1, 20.);
	p_dilate->output ((char*)"polygon_dilate.obj");
	delete p_dilate;

	// tesselate
#if 0 // TORESTORE
	float *_pVertices;
	unsigned int _nVertices;
	unsigned int *_pFaces;
	unsigned int _nFaces;
	p1->tesselate (&_pVertices, &_nVertices, &_pFaces, &_nFaces);
	printf ("%d %d\n", _nVertices, _nFaces);
	Mesh *m = new Mesh ();
	printf ("paf\n");
	m->SetVertices (_nVertices, _pVertices);
	printf ("boum\n");
	m->SetFaces (_nFaces, 3, _pFaces, NULL);
	m->save ((char*)"polygon_tesselate.obj");
	delete m;
#endif

	//moments
	float moments[4];
	p1->moment_0 (moments);
	printf ("moment 0 : %f\n", moments[0]);
	p1->moment_1 (moments);
	printf ("moment 1 : %f %f\n", moments[0], moments[1]);
	p1->moment_2 (moments);
	printf ("moment 2 : %f %f %f\n", moments[0], moments[1], moments[2]);
	p1->moment_2 (moments, true);
	printf ("moment 2 (centralized) : %f %f %f\n", moments[0], moments[1], moments[2]);
	p1->moment_3 (moments);
	printf ("moment 3 : %f %f %f %f\n", moments[0], moments[1], moments[2], moments[3]);

	// cleaning
	delete p1;
}
