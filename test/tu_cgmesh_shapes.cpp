#include <stdio.h>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

//
//
//
float kappa (float s) { return 8; };
float tau (float s) { return sin(s)+3*sin(2*s); };
TEST(TEST_cgmesh_shapes, curves)
{
	printf ("------\n");
	printf ("Curves\n");
	printf ("------\n");
	CurveKappaTau *kt = new CurveKappaTau();
	kt->set_kappa (&kappa);
	kt->set_tau (&tau);
	kt->Export ((char*)"./shapes_curve_kappatau.obj");
	delete kt;

	CurveWindingLineOnTorus *wlot = new CurveWindingLineOnTorus ();
	wlot->Export ((char*)"./shapes_curve_windinglineontorus.obj");
	delete wlot;

	Curve01 *curve01 = new Curve01 ();
	curve01->Export ((char*)"./shapes_curve01.obj");
	delete curve01;
}

//
//
//
TEST(TEST_cgmesh_shapes, curves_basic)
{
	printf ("----------------\n");
	printf ("Surfaces : basic\n");
	printf ("----------------\n");
	printf ("capsule\n");
	Mesh *capsule = CreateCapsule (40, 5., 2.);
	capsule->save ((char*)"./shapes_capsule.obj");
	delete capsule;

	printf ("triangle\n");
	Mesh *triangle = CreateTriangle (0., 0., 0., 1., 0., 1., 0., 1., 0.);
	triangle->save ((char*)"./shapes_triangle.obj");
	delete triangle;

	printf ("quad\n");
	Mesh *quad = CreateQuad ();
	quad->save ((char*)"./shapes_quad.obj");
	delete quad;

	printf ("cube\n");
	Mesh *cube = CreateCube ();
	cube->save ((char*)"./shapes_cube.obj");
	delete cube;

	printf ("tetrahedron\n");
	Mesh *tetrahedron = CreateTetrahedron ();
	tetrahedron->save ((char*)"./shapes_tetrahedron.obj");
	delete tetrahedron;

	printf ("octahedron\n");
	Mesh *octahedron = CreateOctahedron ();
	octahedron->save ((char*)"./shapes_octahedron.obj");
	delete octahedron;

	printf ("dodecahedron\n");
	Mesh *dodecahedron = CreateDodecahedron ();
	dodecahedron->save ((char*)"./shapes_dodecahedron.obj");
	delete dodecahedron;

	printf ("disk\n");
	Mesh *disk = CreateDisk (10);
	disk->save ((char*)"./shapes_disk.obj");
	delete disk;

	printf ("cone\n");
	Mesh *cone = CreateCone (5., 2., 10, true);
	cone->save ((char*)"./shapes_cone.obj");
	delete cone;

	printf ("cylinder\n");
	Mesh *cylinder = CreateCylinder (5., 2., 10, true);
	cylinder->save ((char*)"./shapes_cylinder.obj");
	delete cylinder;

	printf ("teapot\n");
	Mesh *teapot = CreateTeapot ();
	teapot->save ((char*)"./shapes_teapot.obj");
	delete teapot;

	printf ("klein\n");
	Mesh *klein = CreateKleinBottle (40, 40);
	klein->save ((char*)"./shapes_klein.obj");
	delete klein;
}

//
//
//
TEST(TEST_cgmesh_shapes, parametric_surface)
{
	printf ("---------------------\n");
	printf ("Surfaces : parametric\n");
	printf ("---------------------\n");

	printf ("sphere...\n");
	ParametricSphere *sphere = new ParametricSphere (5, 5);
	sphere->save ((char*)"./shapes_sphere.obj");
	delete sphere;

	printf ("elliptic helicoid...\n");
	EllipticHelicoid *ellipticHelicoid = new EllipticHelicoid ();
	ellipticHelicoid->save ((char*)"./shapes_elliptic_helicoid.obj");
	delete ellipticHelicoid;

	printf ("seashell...\n");
	SeaShell *seashell = new SeaShell ();
	seashell->save ((char*)"./shapes_seashell.obj");
	delete seashell;

	printf ("seashell von seggern...\n");
	SeaShellVonSeggern *seashellvonseggern = new SeaShellVonSeggern ();
	seashellvonseggern->save ((char*)"./shapes_seashellvonseggern.obj");
	delete seashellvonseggern;

	printf ("corkscrew surface...\n");
	CorkscrewSurface *corkscrewsurface = new CorkscrewSurface ();
	corkscrewsurface->save ((char*)"./shapes_corkscrewsurface.obj");
	delete corkscrewsurface;

	printf ("mobius strip...\n");
	MobiusStrip *mobiusstrip = new MobiusStrip ();
	mobiusstrip->save ((char*)"./shapes_mobiusstrip.obj");
	delete mobiusstrip;

	printf ("radial wave...\n");
	RadialWave *radialwave = new RadialWave ();
	radialwave->save ((char*)"./shapes_radial_wave.obj");
	delete radialwave;

	printf ("torus...\n");
	ParametricTorus *torus = new ParametricTorus ();
	torus->save ((char*)"./shapes_torus.obj");
	delete torus;
	/*
	printf ("trumpet...\n");
	Trumpet *trumpet = new Trumpet ();
	trumpet->save ((char*)"./shapes_trumpet.obj");
	delete trumpet;

	printf ("cylinder...\n");
	Cylinder *cylinder = new Cylinder ();
	cylinder->save ((char*)"./shapes_cylinder.obj");
	delete cylinder;
	*/

	printf ("breather...\n");
	Breather *breather = new Breather ();
	breather->save ((char*)"./shapes_breather.obj");
	delete breather;

	printf ("borromean ring...\n");
	BorromeanRing *borromeanring = new BorromeanRing ();
	borromeanring->save ((char*)"./shapes_borromeanring.obj");
	delete borromeanring;

	printf ("borromean rings...\n");
	BorromeanRings *borromeanrings = new BorromeanRings ();
	borromeanrings->save ((char*)"./shapes_borromeanrings.obj");
	delete borromeanrings;

	printf ("torus knot...\n");
	TorusKnot *torusknot = new TorusKnot ();
	torusknot->save ((char*)"./shapes_torus_knot.obj");
	delete torusknot;

	printf ("cinquefoil knot...\n");
	CinquefoilKnot *cinquefoilknot = new CinquefoilKnot ();
	cinquefoilknot->save ((char*)"./shapes_cinquefoil_knot.obj");
	delete cinquefoilknot;

	printf ("trefoil knot1...\n");
	TrefoilKnot1 *trefoilknot1 = new TrefoilKnot1 ();
	trefoilknot1->save ((char*)"./shapes_trefoil_knot1.obj");
	delete trefoilknot1;

	printf ("trefoil knot2...\n");
	TrefoilKnot2 *trefoilknot2 = new TrefoilKnot2 ();
	trefoilknot2->save ((char*)"./shapes_trefoil_knot2.obj");
	delete trefoilknot2;
}

TEST(TEST_cgmesh_shapes, append)
{
	unsigned int n = 6;
	unsigned int nhalf = n*3;
	float radius = 10.;
	
	unsigned int nsphere = 15;
	
	//Mesh *res = new Mesh ();
	Mesh *res = CreateCube ();
	res->scale_xyz (8., 8., 0.4);
	res->translate (0., 0., -0.6);
	
	for (float z=0; z<2*radius; z+=0.06)
	{
		float angle = 2.*z*3.14159/(2*radius);
		angle *= 7.;
		float s = sin (acos (z/radius - 1.));
		float x = s*radius*cos(angle);
		float y = s*radius*sin(angle);
		ParametricSphere *tmp = new ParametricSphere (nsphere, nsphere);
		tmp->translate (x, y, z);
		res->Append (tmp);
		
	}
	res->save ("./shapes_append.obj");
}

TEST(TEST_cgmesh_shapes, fractal_surface)
{
	printf ("------------------\n");
	printf ("Surfaces : fractal\n");
	printf ("------------------\n");
	Mesh *menger = CreateMengerSponge (2);
	menger->save ((char*)"./shapes_menger.obj");
	delete menger;
}

TEST(TEST_cgmesh_shapes, strange_attractors)
{
	printf ("---------------------------\n");
	printf ("Points : strange attractors\n");
	printf ("---------------------------\n");
	StrangeAttractor_QuadraticMaps3D* sa1 = NULL;
	StrangeAttractor_Pickover*        sa2 = NULL;
	/*
	const int size = 10000;
	static int index_start = 0;
	static int index_end   = 0;
	static double X[size];
	static double Y[size];
	static double Z[size];
	*/

	sa1 = new StrangeAttractor_QuadraticMaps3D ();
	sa1->set_parameters ((char*)"AGHNFODVNJCPPJKKPK");
	//sa1->set_parameters (0.2, 0.8, -0.6, -0.7, -0.3, -0.2, -0.9, -0.5, 0.6, -1.2, -0.3, 0.8,
	//		     //0.2, 0.3, -0.1, -0.2, 0.3, -0.2);
	//		     0.3, -0.3, -0.2, -0.2, 0.3, -0.2);
	sa1->export_asc ((char*)"./shapes_quadraticmaps3D.asc", 5000);

/*	
	sa2 = new StrangeAttractor_Pickover ();
	sa2->set_origin(0.5, 0.5);
	sa2->set_parameters (-2.90, -2.03, 1.44, 0.70);
	sa2->save ("pickover.obj", 5000);
	//sa2->set_parameters (-1.01, -0.14, 0.10, 0.70);
*/
}

TEST(TEST_cgmesh_shapes, frame)
{
	CurveDiscrete* profil = new CurveDiscrete();
	unsigned int nVertices;
	float* pVertices;
	unsigned int nFaces, * pFaces;
	profil->import_obj((char*)"./test/data/curve.obj");
	profil->generate_frame(40, 30, &nVertices, &pVertices, &nFaces, &pFaces);
	Mesh* frame = new Mesh();
	frame->SetVertices(nVertices, pVertices);
	frame->SetFaces(nFaces, 4, pFaces);
	frame->save((char*)"frame.obj");
	delete profil;
	delete frame;
}

TEST(TEST_cgmesh_shapes, revolution_surface)
{
	CurveDiscrete *profil = new CurveDiscrete ();
	profil->import_obj ((char*)"./test/data/curve.obj");
	float *pVertices;
	unsigned int nVertices;
	unsigned int *pFaces;
	unsigned int nFaces;
	//profil->inverse_order ();
	FILE *ptr = fopen ("./shapes_tasse.obj", "w");
	int nSlices = 90;
	profil->generate_surface_revolution (nSlices, &nVertices, &pVertices, &nFaces, &pFaces);
	fprintf (ptr, "mtllib \"shapes_tasse.mtl\"\n\n");
	for (unsigned int i=0; i<nVertices; i++)
		fprintf (ptr, "v %f %f %f\n", pVertices[3*i], pVertices[3*i+1], pVertices[3*i+2]);
	
	fprintf (ptr, "usemtl material_0\n");
	for (unsigned int i=0; i<nFaces; i++)
	{
		if (i>=42*nSlices && i<43*(nSlices))
		{
			fprintf (ptr, "usemtl material_1\n");
			fprintf (ptr, "vt %f %f\n", (float)(i-42*nSlices)/nSlices, 0.);
			fprintf (ptr, "vt %f %f\n", (float)(i-42*nSlices+1)/nSlices, 0.);
			fprintf (ptr, "vt %f %f\n", (float)(i-42*nSlices+1)/nSlices, 1.);
			fprintf (ptr, "vt %f %f\n", (float)(i-42*nSlices)/nSlices, 1.);
			fprintf (ptr, "f %d/-4 %d/-3 %d/-2 %d/-1\n", 1+pFaces[4*i], 1+pFaces[4*i+1], 1+pFaces[4*i+2], 1+pFaces[4*i+3]);
			fprintf (ptr, "usemtl material_0\n");
		}
		else
		{
			fprintf (ptr, "f %d %d %d %d\n", 1+pFaces[4*i], 1+pFaces[4*i+1], 1+pFaces[4*i+2], 1+pFaces[4*i+3]);
		}
	}
	fclose (ptr);
	delete profil;
}
