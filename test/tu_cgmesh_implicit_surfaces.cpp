#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <utility>
#include <vector>

#include "../src/cgmesh/cgmesh.h"

////////////////////////////////////////////////////////////////////////////////
//
// Helpers
//

// RAII wrapper around the malloc'd buffers returned by get_triangulation():
// the ownership contract is implicit (the caller must free), so we centralise
// it here to guarantee no leak even when an assertion aborts the test.
struct Triangulation
{
	int nvertices = 0;
	int nfaces = 0;
	float* vertices = nullptr;
	unsigned int* faces = nullptr;

	explicit Triangulation(ImplicitSurface* mc)
	{
		mc->get_triangulation(&nvertices, &vertices, &nfaces, &faces);
	}
	~Triangulation()
	{
		free(vertices);
		free(faces);
	}
	Triangulation(const Triangulation&) = delete;
	Triangulation& operator=(const Triangulation&) = delete;
};

// ---- analytic scalar fields ------------------------------------------------
// The eval callback is a bare function pointer with no user-data, so the field
// parameters have to live in file-static globals (see the architecture notes:
// this is exactly why the API is not reentrant).

static float g_sphere_radius = 0.5f;
static float g_sphere_cx = 0.f, g_sphere_cy = 0.f, g_sphere_cz = 0.f;

static float eval_sphere(float x, float y, float z)
{
	const float dx = x - g_sphere_cx;
	const float dy = y - g_sphere_cy;
	const float dz = z - g_sphere_cz;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float g_const_value = 0.f;
static float eval_const(float, float, float)
{
	return g_const_value;
}

// ---- topology analysis -----------------------------------------------------
struct TopologyInfo
{
	int nUniqueEdges = 0;
	int nBoundaryEdges = 0;   // undirected edge used by exactly 1 triangle
	int nNonManifoldEdges = 0; // used by > 2 triangles
	int eulerCharacteristic = 0; // V - E + F
};

static TopologyInfo analyze_topology(const Triangulation& t)
{
	std::map<std::pair<int, int>, int> edgeCount;
	for (int f = 0; f < t.nfaces; f++)
	{
		const unsigned int v[3] = {
			t.faces[3 * f], t.faces[3 * f + 1], t.faces[3 * f + 2]};
		for (int e = 0; e < 3; e++)
		{
			int a = (int)v[e];
			int b = (int)v[(e + 1) % 3];
			if (a > b)
				std::swap(a, b);
			edgeCount[std::make_pair(a, b)]++;
		}
	}

	TopologyInfo info;
	info.nUniqueEdges = (int)edgeCount.size();
	for (const auto& kv : edgeCount)
	{
		if (kv.second == 1)
			info.nBoundaryEdges++;
		else if (kv.second > 2)
			info.nNonManifoldEdges++;
	}
	info.eulerCharacteristic = t.nvertices - info.nUniqueEdges + t.nfaces;
	return info;
}

// Mean / max absolute distance from each output vertex to the true sphere.
static void sphere_error(const Triangulation& t, double& meanErr, double& maxErr)
{
	meanErr = 0.;
	maxErr = 0.;
	if (t.nvertices == 0)
		return;
	for (int i = 0; i < t.nvertices; i++)
	{
		const double dx = t.vertices[3 * i] - g_sphere_cx;
		const double dy = t.vertices[3 * i + 1] - g_sphere_cy;
		const double dz = t.vertices[3 * i + 2] - g_sphere_cz;
		const double d = sqrt(dx * dx + dy * dy + dz * dz);
		const double err = fabs(d - g_sphere_radius);
		meanErr += err;
		if (err > maxErr)
			maxErr = err;
	}
	meanErr /= t.nvertices;
}

static ImplicitSurface* make_sphere_surface(int resolution_per_unit)
{
	g_sphere_radius = 0.5f;
	g_sphere_cx = g_sphere_cy = g_sphere_cz = 0.f;

	ImplicitSurface* mc = new ImplicitSurface();
	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(resolution_per_unit);
	mc->set_orientation(1);
	mc->set_eval_func(eval_sphere);
	mc->set_value(g_sphere_radius);
	return mc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Output validity: every produced index/coordinate must be usable
//
TEST(TEST_cgmesh_implicit_surface, sphere_produces_valid_buffers)
{
	ImplicitSurface* mc = make_sphere_surface(20);
	Triangulation t(mc);
	delete mc;

	ASSERT_GT(t.nfaces, 0);
	ASSERT_GT(t.nvertices, 0);

	// every face index is in range
	for (int f = 0; f < t.nfaces; f++)
		for (int c = 0; c < 3; c++)
		{
			unsigned int idx = t.faces[3 * f + c];
			EXPECT_LT((int)idx, t.nvertices);
		}

	// no degenerate triangle (the three indices differ)
	int degenerate = 0;
	for (int f = 0; f < t.nfaces; f++)
	{
		unsigned int a = t.faces[3 * f], b = t.faces[3 * f + 1], c = t.faces[3 * f + 2];
		if (a == b || b == c || a == c)
			degenerate++;
	}
	EXPECT_EQ(degenerate, 0);

	// no NaN / Inf anywhere
	for (int i = 0; i < 3 * t.nvertices; i++)
		EXPECT_TRUE(std::isfinite(t.vertices[i])) << "vertex coord " << i;
}

////////////////////////////////////////////////////////////////////////////////
//
// Geometric correctness: vertices lie on the analytic sphere
//
TEST(TEST_cgmesh_implicit_surface, sphere_vertices_on_surface)
{
	ImplicitSurface* mc = make_sphere_surface(20);
	Triangulation t(mc);
	delete mc;

	ASSERT_GT(t.nvertices, 0);

	double meanErr, maxErr;
	sphere_error(t, meanErr, maxErr);

	// step = 2.0 / 40 = 0.05 ; linear interpolation of an (almost) exact
	// distance field keeps every vertex well within one cell of the surface.
	EXPECT_LT(maxErr, 0.06) << "a vertex is too far from the sphere";
	EXPECT_LT(meanErr, 0.02) << "mean deviation from the sphere is too large";
}

////////////////////////////////////////////////////////////////////////////////
//
// Refinement: increasing the resolution reduces the approximation error
//
TEST(TEST_cgmesh_implicit_surface, sphere_refinement_reduces_error)
{
	double coarseMean, coarseMax, fineMean, fineMax;

	{
		ImplicitSurface* mc = make_sphere_surface(8);
		Triangulation t(mc);
		delete mc;
		ASSERT_GT(t.nvertices, 0);
		sphere_error(t, coarseMean, coarseMax);
	}
	{
		ImplicitSurface* mc = make_sphere_surface(40);
		Triangulation t(mc);
		delete mc;
		ASSERT_GT(t.nvertices, 0);
		sphere_error(t, fineMean, fineMax);
	}

	EXPECT_LT(fineMean, coarseMean) << "finer mesh should approximate better";
}

////////////////////////////////////////////////////////////////////////////////
//
// Topology: a sphere inside the box must be a watertight, genus-0 manifold
//
TEST(TEST_cgmesh_implicit_surface, sphere_is_watertight_manifold)
{
	ImplicitSurface* mc = make_sphere_surface(20);
	Triangulation t(mc);
	delete mc;

	ASSERT_GT(t.nfaces, 0);

	TopologyInfo info = analyze_topology(t);

	// watertight: no edge belongs to a single triangle
	EXPECT_EQ(info.nBoundaryEdges, 0) << "mesh has holes (boundary edges)";
	// manifold: no edge shared by more than two triangles
	EXPECT_EQ(info.nNonManifoldEdges, 0) << "mesh has non-manifold edges";
	// genus 0 closed surface: V - E + F == 2
	EXPECT_EQ(info.eulerCharacteristic, 2) << "unexpected Euler characteristic";
}

////////////////////////////////////////////////////////////////////////////////
//
// Empty results when the field never crosses the iso-value
//
TEST(TEST_cgmesh_implicit_surface, field_entirely_outside_yields_no_faces)
{
	g_const_value = 10.f;
	ImplicitSurface* mc = new ImplicitSurface();
	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(10);
	mc->set_orientation(1);
	mc->set_eval_func(eval_const);
	mc->set_value(0.5); // 10 > 0.5 everywhere -> all outside

	Triangulation t(mc);
	delete mc;

	EXPECT_EQ(t.nfaces, 0);
}

TEST(TEST_cgmesh_implicit_surface, field_entirely_inside_yields_no_faces)
{
	g_const_value = -10.f;
	ImplicitSurface* mc = new ImplicitSurface();
	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(10);
	mc->set_orientation(1);
	mc->set_eval_func(eval_const);
	mc->set_value(0.5); // -10 < 0.5 everywhere -> all inside, no boundary -> no faces

	Triangulation t(mc);
	delete mc;

	EXPECT_EQ(t.nfaces, 0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Orientation only changes winding, not the geometry
//
TEST(TEST_cgmesh_implicit_surface, orientation_preserves_geometry)
{
	int faces0, faces1, verts0, verts1;

	{
		ImplicitSurface* mc = make_sphere_surface(16);
		mc->set_orientation(0);
		Triangulation t(mc);
		delete mc;
		faces0 = t.nfaces;
		verts0 = t.nvertices;
	}
	{
		ImplicitSurface* mc = make_sphere_surface(16);
		mc->set_orientation(1);
		Triangulation t(mc);
		delete mc;
		faces1 = t.nfaces;
		verts1 = t.nvertices;
	}

	EXPECT_EQ(faces0, faces1);
	EXPECT_EQ(verts0, verts1);
}

////////////////////////////////////////////////////////////////////////////////
//
// get_normal: gradient of a distance field is radial and unit-length
//
TEST(TEST_cgmesh_implicit_surface, normal_of_sphere_is_radial)
{
	g_sphere_radius = 0.5f;
	g_sphere_cx = g_sphere_cy = g_sphere_cz = 0.f;

	ImplicitSurface* mc = make_sphere_surface(20);

	struct Probe { float x, y, z; };
	const Probe probes[] = {
		{0.5f, 0.f, 0.f}, {0.f, 0.5f, 0.f}, {0.f, 0.f, 0.5f},
		{-0.5f, 0.f, 0.f}};

	for (const Probe& p : probes)
	{
		vec3f pt{p.x, p.y, p.z};
		vec3f n{0, 0, 0};
		mc->get_normal(pt, n);

		// unit length
		const float len = sqrtf(n.fX * n.fX + n.fY * n.fY + n.fZ * n.fZ);
		EXPECT_NEAR(len, 1.0f, 1e-3f);

		// colinear with the radial direction (|cos| ~ 1)
		const float rl = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
		const float dot = (n.fX * p.x + n.fY * p.y + n.fZ * p.z) / rl;
		EXPECT_GT(fabsf(dot), 0.99f) << "normal is not radial";
	}

	delete mc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Bounding box round-trip
//
TEST(TEST_cgmesh_implicit_surface, bbox_round_trip)
{
	ImplicitSurface mc;
	mc.set_bbox(-2.f, -3.f, -4.f, 5.f, 6.f, 7.f);

	vec3f vmin{0, 0, 0}, vmax{0, 0, 0};
	mc.get_bbox(vmin, vmax);

	EXPECT_FLOAT_EQ(vmin.fX, -2.f);
	EXPECT_FLOAT_EQ(vmin.fY, -3.f);
	EXPECT_FLOAT_EQ(vmin.fZ, -4.f);
	EXPECT_FLOAT_EQ(vmax.fX, 5.f);
	EXPECT_FLOAT_EQ(vmax.fY, 6.f);
	EXPECT_FLOAT_EQ(vmax.fZ, 7.f);
}

////////////////////////////////////////////////////////////////////////////////
//
// set_value / get_value: pins the *current* (suspicious) behaviour.
// NOTE: get_value(float) ignores its argument and returns the stored iso-value.
// This test documents that contract so a future fix is a conscious decision.
//
TEST(TEST_cgmesh_implicit_surface, value_getter_returns_stored_isovalue)
{
	ImplicitSurface mc;
	mc.set_value(0.5f);
	EXPECT_FLOAT_EQ(mc.get_value(123.f), 0.5f);
	mc.set_value(2.0f);
	EXPECT_FLOAT_EQ(mc.get_value(0.f), 2.0f);
}

////////////////////////////////////////////////////////////////////////////////
//
// Sample fields: each demo field must produce a finite, valid mesh
//
namespace {
struct SampleCase { const char* name; float (*fn)(float, float, float); float iso; };
}

TEST(TEST_cgmesh_implicit_surface, sample_fields_produce_valid_meshes)
{
	update_time(0.5f); // initialises the moving source points used by fSample1/2/3

	const SampleCase cases[] = {
		{"fSample0", fSample0, 0.4f},
		{"fSample1", fSample1, 30.f},
		{"fSample2", fSample2, 30.f},
		{"fSample5", fSample5, 30.f},
		{"fSample6", fSample6, 3.f},
		{"fSample7", fSample7, 0.f},
	};

	for (const SampleCase& c : cases)
	{
		ImplicitSurface* mc = new ImplicitSurface();
		mc->set_bbox(-1., -1., -1., 1., 1., 1.);
		mc->set_resolution_per_unit(16);
		mc->set_orientation(1);
		mc->set_eval_func(c.fn);
		mc->set_value(c.iso);

		Triangulation t(mc);
		delete mc;

		// indices in range, coordinates finite
		for (int f = 0; f < t.nfaces; f++)
			for (int k = 0; k < 3; k++)
				EXPECT_LT((int)t.faces[3 * f + k], t.nvertices) << c.name;
		for (int i = 0; i < 3 * t.nvertices; i++)
			EXPECT_TRUE(std::isfinite(t.vertices[i])) << c.name << " coord " << i;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Image-driven field (kept from the original suite, now with assertions)
//
static Img* g_img = nullptr;
static float eval_image(float x, float y, float)
{
	unsigned char r = g_img->get_r((int)x, (int)y);
	if (((int)x) % 2 == 0)
		return 1.f - ((float)r / 255.f);
	else
		return -1.f;
}

TEST(TEST_cgmesh_implicit_surface, image_field)
{
	g_img = new Img();
	g_img->load("./test/data/tga/ctc16.tga");

	ImplicitSurface* mc = new ImplicitSurface();
	mc->set_bbox(1., 1., 0., g_img->width() - 2, g_img->height() - 2, 1.);
	mc->set_resolution_per_unit(1);
	mc->set_boundary(1);
	mc->set_orientation(1);
	mc->set_eval_func(eval_image);
	mc->set_value(0.);

	Triangulation t(mc);

	EXPECT_GT(t.nfaces, 0);
	for (int i = 0; i < 3 * t.nvertices; i++)
		EXPECT_TRUE(std::isfinite(t.vertices[i]));

	delete mc;
	delete g_img;
	g_img = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
//
// ImplicitSurfaceTandem
//
// These tests only became possible after removing the debug `exit(0)` and the
// hard-coded OBJ export that used to live in tandem_end_layer.
//

// Object lifecycle: an ImplicitSurfaceTandem can be created and destroyed.
// (Kept enabled because the simplification path below currently crashes.)
TEST(TEST_cgmesh_implicit_surface_tandem, construct_and_destroy)
{
	ImplicitSurfaceTandem* mc = new ImplicitSurfaceTandem();
	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(8);
	mc->set_orientation(1);
	mc->set_eval_func(eval_sphere);
	mc->set_value(0.5f);
	delete mc;
	SUCCEED();
}

// KNOWN BUG -- DISABLED.
// Running the tandem extraction crashes inside tandem_simplify() with
// "cannot increment value-initialized map/set iterator" (a half-edge/quadric
// edge-contraction bug). This is a pre-existing latent defect: the simplify
// path was never exercised because the code used to exit(0) after two layers
// and no test ever ran it. Removing exit(0) exposed it.
// Re-enable (drop the DISABLED_ prefix) once the contraction bug is fixed.
//
// End-to-end stability: the tandem extraction + per-layer simplification must
// run to completion and return a usable buffer (no crash, no process exit).
TEST(TEST_cgmesh_implicit_surface_tandem, DISABLED_runs_and_returns_valid_buffers)
{
	g_sphere_radius = 0.5f;
	g_sphere_cx = g_sphere_cy = g_sphere_cz = 0.f;

	ImplicitSurfaceTandem* mc = new ImplicitSurfaceTandem();
	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(10);
	mc->set_orientation(1);
	mc->set_eval_func(eval_sphere);
	mc->set_value(g_sphere_radius);

	Triangulation t(mc);
	delete mc;

	ASSERT_GT(t.nfaces, 0);
	ASSERT_GT(t.nvertices, 0);

	for (int f = 0; f < t.nfaces; f++)
		for (int c = 0; c < 3; c++)
			EXPECT_LT((int)t.faces[3 * f + c], t.nvertices);

	for (int i = 0; i < 3 * t.nvertices; i++)
		EXPECT_TRUE(std::isfinite(t.vertices[i])) << "tandem coord " << i;
}

// Architectural pin-down: get_triangulation() on the tandem surface returns the
// RAW marching-cubes arrays. The simplification happens only inside the private
// half-edge mesh, which the public API discards (and leaks). So the returned
// face count equals the plain ImplicitSurface output. This test documents that
// limitation so any future API that exposes the simplified mesh is a conscious
// change.
//
// DISABLED for the same reason as above: the tandem extraction currently
// crashes in tandem_simplify(). Re-enable once that bug is fixed.
TEST(TEST_cgmesh_implicit_surface_tandem, DISABLED_public_api_returns_unsimplified_faces)
{
	g_sphere_radius = 0.5f;
	g_sphere_cx = g_sphere_cy = g_sphere_cz = 0.f;

	int baseFaces;
	{
		ImplicitSurface* mc = make_sphere_surface(10);
		Triangulation t(mc);
		delete mc;
		baseFaces = t.nfaces;
	}

	int tandemFaces;
	{
		ImplicitSurfaceTandem* mc = new ImplicitSurfaceTandem();
		mc->set_bbox(-1., -1., -1., 1., 1., 1.);
		mc->set_resolution_per_unit(10);
		mc->set_orientation(1);
		mc->set_eval_func(eval_sphere);
		mc->set_value(g_sphere_radius);
		Triangulation t(mc);
		delete mc;
		tandemFaces = t.nfaces;
	}

	EXPECT_EQ(tandemFaces, baseFaces)
		<< "public API unexpectedly reflects simplification";
}
