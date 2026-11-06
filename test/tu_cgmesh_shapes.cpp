#include <stdio.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

namespace
{

// Maillage non vide, coordonnees finies, faces d'au moins trois sommets valides.
void ExpectSoundMesh (const Mesh &m, const char *name)
{
	SCOPED_TRACE (name);
	ASSERT_GT (m.GetNVertices (), 0u);
	ASSERT_GT (m.GetNFaces (), 0u);
	for (unsigned v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		ASSERT_TRUE (std::isfinite (p[0]) && std::isfinite (p[1]) && std::isfinite (p[2])) << v;
	}
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
	{
		const int n = m.GetFaceNVertices (f);
		ASSERT_GE (n, 3) << f;
		for (int k = 0; k < n; ++k)
		{
			const int vi = m.GetFaceVertex (f, static_cast<unsigned> (k));
			ASSERT_GE (vi, 0) << f;
			ASSERT_LT (static_cast<unsigned> (vi), m.GetNVertices ()) << f;
		}
	}
}

// Coque fermee et orientee sur les indices bruts, faces de toute arite : chaque
// arete orientee apparait une fois, et son opposee aussi.
void ExpectClosedOrientedShell (const Mesh &m, const char *name)
{
	std::map<std::pair<int, int>, int> directed;
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
	{
		const int n = m.GetFaceNVertices (f);
		for (int k = 0; k < n; ++k)
			++directed[{m.GetFaceVertex (f, static_cast<unsigned> (k)),
			            m.GetFaceVertex (f, static_cast<unsigned> ((k + 1) % n))}];
	}
	std::size_t bad = 0;
	for (const auto &[edge, count] : directed)
	{
		const auto opposite = directed.find ({edge.second, edge.first});
		if (count != 1 || opposite == directed.end () || opposite->second != 1)
			++bad;
	}
	EXPECT_EQ (bad, 0u) << name;
}

// Le fichier ecrit se relit avec le meme nombre de sommets et de faces.
void ExpectSavedAndReloaded (const Mesh &m, const char *filename)
{
	ASSERT_EQ (m.save (filename), 0) << filename;
	Mesh reloaded;
	ASSERT_EQ (reloaded.load (filename), 0) << filename;
	EXPECT_EQ (reloaded.GetNVertices (), m.GetNVertices ()) << filename;
	EXPECT_EQ (reloaded.GetNFaces (), m.GetNFaces ()) << filename;
}

std::size_t CountObjVertices (const char *filename)
{
	std::ifstream f (filename);
	std::size_t n = 0;
	std::string line;
	while (std::getline (f, line))
		if (line.size () > 2 && line[0] == 'v' && line[1] == ' ')
			++n;
	return n;
}

} // namespace

//
//
//
float kappa (float s) { return 8; };
float tau (float s) { return sin(s)+3*sin(2*s); };
TEST(TEST_cgmesh_shapes, curves)
{
	// courbe definie par sa courbure et sa torsion : elle les rend telles quelles
	CurveKappaTau kt;
	kt.set_kappa (&kappa);
	kt.set_tau (&tau);
	EXPECT_FLOAT_EQ (kt.curvature (0.25f), 8.f);
	kt.Export ((char*)"./shapes_curve_kappatau.obj");
	EXPECT_GT (CountObjVertices ("./shapes_curve_kappatau.obj"), 0u);

	// enroulement sur le tore de rayons 4 et 1 : (rho - 4)^2 + z^2 = 1
	CurveWindingLineOnTorus wlot;
	std::vector<Vector3f> pts;
	ASSERT_EQ (wlot.tessellate (200, pts), 200);
	for (const Vector3f &p : pts)
	{
		const float rho = std::hypot (p.x, p.y);
		EXPECT_NEAR ((rho - 4.f) * (rho - 4.f) + p.z * p.z, 1.f, 1e-4f);
	}
	wlot.Export ((char*)"./shapes_curve_windinglineontorus.obj");
	// tube : 200 pas x 80 points par cercle
	EXPECT_EQ (CountObjVertices ("./shapes_curve_windinglineontorus.obj"), 200u * 80u);

	// (6 cos t, 6 sin t, cos 12 t) : sur le cylindre de rayon 6, |z| <= 1
	Curve01 curve01;
	ASSERT_EQ (curve01.tessellate (100, pts), 100);
	for (const Vector3f &p : pts)
	{
		EXPECT_NEAR (std::hypot (p.x, p.y), 6.f, 1e-4f);
		EXPECT_LE (std::fabs (p.z), 1.f + 1e-6f);
	}
	curve01.Export ((char*)"./shapes_curve01.obj");
	EXPECT_EQ (CountObjVertices ("./shapes_curve01.obj"), 200u * 80u);
}

//
//
//
TEST(TEST_cgmesh_shapes, curves_basic)
{
	struct Expected { const char *name; std::unique_ptr<Mesh> mesh; unsigned vertices, faces; bool closed; };
	Expected shapes[] = {
		{"./shapes_capsule.obj",      std::unique_ptr<Mesh> (CreateCapsule (40, 5., 2.)), 2*40*20+2, 4*40*20, false},
		{"./shapes_triangle.obj",     std::unique_ptr<Mesh> (CreateTriangle (0., 0., 0., 1., 0., 1., 0., 1., 0.)), 3, 1, false},
		{"./shapes_quad.obj",         std::unique_ptr<Mesh> (CreateQuad ()), 4, 1, false},
		{"./shapes_cube.obj",         std::unique_ptr<Mesh> (CreateCube ()), 8, 6, true},
		{"./shapes_tetrahedron.obj",  std::unique_ptr<Mesh> (CreateTetrahedron ()), 4, 4, true},
		{"./shapes_octahedron.obj",   std::unique_ptr<Mesh> (CreateOctahedron ()), 6, 8, true},
		{"./shapes_dodecahedron.obj", std::unique_ptr<Mesh> (CreateDodecahedron ()), 20, 12, true},
		{"./shapes_disk.obj",         std::unique_ptr<Mesh> (CreateDisk (10)), 10, 1, false},
		{"./shapes_cone.obj",         std::unique_ptr<Mesh> (CreateCone (5., 2., 10, true)), 11, 11, false},
		{"./shapes_cylinder.obj",     std::unique_ptr<Mesh> (CreateCylinder (5., 2., 10, true)), 20, 12, false},
		{"./shapes_klein.obj",        std::unique_ptr<Mesh> (CreateKleinBottle (40, 40)), 40*40, 2*(39*40+40), false},
	};
	for (Expected &s : shapes)
	{
		ExpectSoundMesh (*s.mesh, s.name);
		EXPECT_EQ (s.mesh->GetNVertices (), s.vertices) << s.name;
		EXPECT_EQ (s.mesh->GetNFaces (), s.faces) << s.name;
		if (s.closed)
			ExpectClosedOrientedShell (*s.mesh, s.name);
		ExpectSavedAndReloaded (*s.mesh, s.name);
	}

	// theiere : tables embarquees, comptes figes
	const std::unique_ptr<Mesh> teapot (CreateTeapot ());
	ExpectSoundMesh (*teapot, "teapot");
	ExpectSavedAndReloaded (*teapot, "./shapes_teapot.obj");
}

//
//
//
TEST(TEST_cgmesh_shapes, parametric_surface)
{
	std::vector<std::pair<const char *, std::unique_ptr<Mesh>>> surfaces;
	surfaces.emplace_back ("./shapes_sphere.obj", std::make_unique<ParametricSphere> (5, 5));
	surfaces.emplace_back ("./shapes_elliptic_helicoid.obj", std::make_unique<EllipticHelicoid> ());
	surfaces.emplace_back ("./shapes_seashell.obj", std::make_unique<SeaShell> ());
	surfaces.emplace_back ("./shapes_seashellvonseggern.obj", std::make_unique<SeaShellVonSeggern> ());
	surfaces.emplace_back ("./shapes_corkscrewsurface.obj", std::make_unique<CorkscrewSurface> ());
	surfaces.emplace_back ("./shapes_mobiusstrip.obj", std::make_unique<MobiusStrip> ());
	surfaces.emplace_back ("./shapes_radial_wave.obj", std::make_unique<RadialWave> ());
	surfaces.emplace_back ("./shapes_torus.obj", std::make_unique<ParametricTorus> ());
	surfaces.emplace_back ("./shapes_breather.obj", std::make_unique<Breather> ());
	surfaces.emplace_back ("./shapes_borromeanring.obj", std::make_unique<BorromeanRing> ());
	surfaces.emplace_back ("./shapes_borromeanrings.obj", std::make_unique<BorromeanRings> ());
	surfaces.emplace_back ("./shapes_torus_knot.obj", std::make_unique<TorusKnot> ());
	surfaces.emplace_back ("./shapes_cinquefoil_knot.obj", std::make_unique<CinquefoilKnot> ());
	surfaces.emplace_back ("./shapes_trefoil_knot1.obj", std::make_unique<TrefoilKnot1> ());
	surfaces.emplace_back ("./shapes_trefoil_knot2.obj", std::make_unique<TrefoilKnot2> ());

	for (const auto &[name, mesh] : surfaces)
	{
		ExpectSoundMesh (*mesh, name);
		ExpectSavedAndReloaded (*mesh, name);
	}
}

TEST(TEST_cgmesh_shapes, append)
{
	const float radius = 10.;
	const unsigned int nsphere = 15;

	std::unique_ptr<Mesh> res (CreateCube ());
	res->scale_xyz (8., 8., 0.4);
	res->translate (0., 0., -0.6);
	const unsigned cubeVertices = res->GetNVertices ();
	const unsigned cubeFaces = res->GetNFaces ();

	const ParametricSphere probe (nsphere, nsphere);
	unsigned nSpheres = 0;
	for (float z=0; z<2*radius; z+=0.06)
	{
		float angle = 2.*z*3.14159/(2*radius);
		angle *= 7.;
		float s = sin (acos (z/radius - 1.));
		float x = s*radius*cos(angle);
		float y = s*radius*sin(angle);
		ParametricSphere tmp (nsphere, nsphere);
		tmp.translate (x, y, z);
		res->Append (&tmp);
		++nSpheres;
	}
	EXPECT_EQ (res->GetNVertices (), cubeVertices + nSpheres * probe.GetNVertices ());
	EXPECT_EQ (res->GetNFaces (), cubeFaces + nSpheres * probe.GetNFaces ());
	ExpectSoundMesh (*res, "append");
	EXPECT_EQ (res->save ("./shapes_append.obj"), 0);
}

TEST(TEST_cgmesh_shapes, strange_attractors)
{
	StrangeAttractor_QuadraticMaps3D sa1;
	sa1.set_parameters ((char*)"AGHNFODVNJCPPJKKPK");
	sa1.export_asc ((char*)"./shapes_quadraticmaps3D.asc", 5000);

	// une ligne par point : x y z r g b nx ny nz, coordonnees finies
	std::ifstream f ("./shapes_quadraticmaps3D.asc");
	ASSERT_TRUE (f.good ());
	std::string line;
	unsigned n = 0;
	while (std::getline (f, line))
	{
		std::istringstream in (line);
		double x, y, z;
		ASSERT_TRUE (static_cast<bool> (in >> x >> y >> z)) << n;
		EXPECT_TRUE (std::isfinite (x) && std::isfinite (y) && std::isfinite (z)) << n;
		++n;
	}
	EXPECT_EQ (n, 5000u);
}

TEST(TEST_cgmesh_shapes, frame)
{
	ProfilePolyline profil;
	unsigned int nVertices = 0;
	float* pVertices = nullptr;
	unsigned int nFaces = 0, * pFaces = nullptr;
	ASSERT_EQ (profil.import_obj((char*)"./test/data/curve.obj"), 0);
	profil.generate_frame(40, 30, &nVertices, &pVertices, &nFaces, &pFaces);
	std::unique_ptr<float, decltype(&free)> verticesOwner (pVertices, &free);
	std::unique_ptr<unsigned int, decltype(&free)> facesOwner (pFaces, &free);

	// quatre coins par point du profil (5 points dans curve.obj), un quadrangle chacun
	EXPECT_EQ (nVertices, 4u * 5u);
	EXPECT_EQ (nFaces, 4u * 5u);
	Mesh frame;
	frame.SetVertices(nVertices, pVertices);
	frame.SetFaces(nFaces, 4, pFaces);
	ExpectSoundMesh (frame, "frame");
	ExpectSavedAndReloaded (frame, "frame.obj");
}

TEST(TEST_cgmesh_shapes, revolution_surface)
{
	ProfilePolyline profil;
	ASSERT_EQ (profil.import_obj ((char*)"./test/data/curve.obj"), 0);
	float *pVertices = nullptr;
	unsigned int nVertices = 0;
	unsigned int *pFaces = nullptr;
	unsigned int nFaces = 0;
	const int nSlices = 90;
	profil.generate_surface_revolution (nSlices, &nVertices, &pVertices, &nFaces, &pFaces);
	std::unique_ptr<float, decltype(&free)> verticesOwner (pVertices, &free);
	std::unique_ptr<unsigned int, decltype(&free)> facesOwner (pFaces, &free);

	// 5 points de profil : 5 anneaux de 90 sommets, 4 bandes de 90 quadrangles
	ASSERT_EQ (nVertices, 5u * nSlices);
	ASSERT_EQ (nFaces, 4u * nSlices);

	// revolution autour de z : rayon de chaque anneau = |x| du point de profil
	// (0, 0, 2, 3, 3), hauteur conservee (0, 1, 1, 2, 0)
	const float radius[5] = {0.f, 0.f, 2.f, 3.f, 3.f};
	const float height[5] = {0.f, 1.f, 1.f, 2.f, 0.f};
	for (unsigned i = 0; i < nVertices; ++i)
	{
		const unsigned ring = i / nSlices;
		EXPECT_NEAR (std::hypot (pVertices[3*i], pVertices[3*i+1]), radius[ring], 1e-5f) << i;
		EXPECT_FLOAT_EQ (pVertices[3*i+2], height[ring]) << i;
	}
	for (unsigned i = 0; i < 4 * nFaces; ++i)
		ASSERT_LT (pFaces[i], nVertices) << i;

	Mesh tasse;
	tasse.SetVertices (nVertices, pVertices);
	tasse.SetFaces (nFaces, 4, pFaces);
	ExpectSavedAndReloaded (tasse, "./shapes_tasse.obj");
}
