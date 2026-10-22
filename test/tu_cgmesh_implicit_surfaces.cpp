#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
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
	bool ok = false;

	explicit Triangulation(ImplicitSurface* mc)
	{
		ok = mc->get_triangulation(&nvertices, &vertices, &nfaces, &faces);
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

static float eval_sphere(float x, float y, float z, void*)
{
	const float dx = x - g_sphere_cx;
	const float dy = y - g_sphere_cy;
	const float dz = z - g_sphere_cz;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float g_const_value = 0.f;
static float eval_const(float, float, float, void*)
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
struct SampleCase { const char* name; float (*fn)(float, float, float, void*); float iso; };
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
static float eval_image(float x, float y, float, void*)
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

// End-to-end stability: the tandem extraction + per-layer simplification must
// run to completion and return a usable buffer (no crash, no process exit).
//
// This used to crash inside tandem_simplify() with "cannot increment
// value-initialized map/set iterator": the contraction loop iterated over
// m_map_edges while edge_contract2() erased entries from it. Fixed by
// snapshotting the edge indices before contracting; this test guards it.
TEST(TEST_cgmesh_implicit_surface_tandem, runs_and_returns_valid_buffers)
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

// The public API now returns the *simplified* mesh: get_triangulation() on the
// tandem surface rebuilds compact buffers from the decimated half-edge mesh.
// The result must be a valid mesh with no more faces than the raw marching
// cubes output (edge contraction can only remove or keep triangles).
TEST(TEST_cgmesh_implicit_surface_tandem, public_api_returns_simplified_mesh)
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

	int tandemFaces, tandemVerts;
	{
		ImplicitSurfaceTandem* mc = new ImplicitSurfaceTandem();
		mc->set_bbox(-1., -1., -1., 1., 1., 1.);
		mc->set_resolution_per_unit(10);
		mc->set_orientation(1);
		mc->set_eval_func(eval_sphere);
		mc->set_value(g_sphere_radius);
		Triangulation t(mc);

		tandemFaces = t.nfaces;
		tandemVerts = t.nvertices;

		// the rebuilt buffer must be self-consistent
		ASSERT_GT(t.nfaces, 0);
		ASSERT_GT(t.nvertices, 0);
		for (int f = 0; f < t.nfaces; f++)
		{
			unsigned int a = t.faces[3*f], b = t.faces[3*f+1], c = t.faces[3*f+2];
			EXPECT_LT((int)a, t.nvertices);
			EXPECT_LT((int)b, t.nvertices);
			EXPECT_LT((int)c, t.nvertices);
			EXPECT_FALSE(a == b || b == c || a == c) << "degenerate face " << f;
		}
		for (int i = 0; i < 3 * t.nvertices; i++)
			EXPECT_TRUE(std::isfinite(t.vertices[i])) << "tandem coord " << i;

		delete mc;
	}

	// tandem must actually decimate (fewer faces than the raw extraction)
	EXPECT_LT(tandemFaces, baseFaces) << "simplification did not reduce the mesh";
	// closed triangle mesh: V - E + F = 2 with E = 3F/2  =>  V = F/2 + 2 < F
	EXPECT_LT(tandemVerts, tandemFaces) << "vertex count inconsistent with a surface";
}

////////////////////////////////////////////////////////////////////////////////
//
// PointCloudField: blobby implicit surface reconstructed from a point cloud
//

// Deterministic (RNG-free) Fibonacci-spiral sampling of a sphere surface.
static std::vector<float> make_sphere_cloud(int n, float radius)
{
	std::vector<float> pts;
	pts.reserve(3 * n);
	const float golden = 2.399963f; // golden angle in radians
	for (int i = 0; i < n; i++)
	{
		const float t = (n > 1) ? (float)i / (float)(n - 1) : 0.f;
		const float z = 1.f - 2.f * t;
		float s = 1.f - z * z;
		if (s < 0.f) s = 0.f;
		const float r = sqrtf(s);
		const float phi = i * golden;
		pts.push_back(radius * r * cosf(phi));
		pts.push_back(radius * r * sinf(phi));
		pts.push_back(radius * z);
	}
	return pts;
}

TEST(TEST_cgmesh_implicit_pointcloud, blobby_sphere_produces_valid_mesh)
{
	std::vector<float> cloud = make_sphere_cloud(300, 0.5f);

	PointCloudField field;
	field.Build(cloud.data(), (int)cloud.size() / 3);
	field.SetIsoDistance(0.1f);

	float vmin[3], vmax[3];
	field.GetPaddedAABB(vmin, vmax);

	ImplicitSurface mc;
	mc.set_bbox(vmin[0], vmin[1], vmin[2], vmax[0], vmax[1], vmax[2]);
	mc.set_resolution_per_unit(24);
	mc.set_orientation(1);
	mc.set_eval_func(&PointCloudField::Eval);
	mc.set_eval_data(&field);
	mc.set_value(field.GetIsoLevel());

	Triangulation t(&mc);

	ASSERT_GT(t.nfaces, 0);
	ASSERT_GT(t.nvertices, 0);

	for (int f = 0; f < t.nfaces; f++)
		for (int c = 0; c < 3; c++)
			EXPECT_LT((int)t.faces[3 * f + c], t.nvertices);

	for (int i = 0; i < 3 * t.nvertices; i++)
		EXPECT_TRUE(std::isfinite(t.vertices[i]));
}

// The field's context travels through the eval user-data pointer, so two
// independent fields must not interfere -- this pins down the reentrancy that
// the void* user_data refactor buys (no file-static globals).
TEST(TEST_cgmesh_implicit_pointcloud, eval_uses_user_data_not_globals)
{
	std::vector<float> a = { 0.f, 0.f, 0.f };       // single point at the origin
	std::vector<float> b = { 10.f, 10.f, 10.f };    // single point far away

	PointCloudField fa; fa.Build(a.data(), 1); fa.SetIsoDistance(0.1f);
	PointCloudField fb; fb.Build(b.data(), 1); fb.SetIsoDistance(0.1f);

	// At the origin, field A peaks at its own Gaussian centre (~1) while field
	// B contributes nothing (its point is far beyond the cutoff radius).
	const float va = PointCloudField::Eval(0.f, 0.f, 0.f, &fa);
	const float vb = PointCloudField::Eval(0.f, 0.f, 0.f, &fb);

	EXPECT_NEAR(va, 1.0f, 1e-3f);
	EXPECT_NEAR(vb, 0.0f, 1e-3f);
	EXPECT_GT(va, vb);
}

////////////////////////////////////////////////////////////////////////////////
//
// Indices du cache, numerotation, evaluations du champ, tandem au-dela de
// 100 000 faces (option A, jalon A3)
//
namespace
{
	// Change de signe dans une large part des cubes, sans symetrie entre x et y.
	float stripes(float x, float y, float, void*)
	{
		const float s = 0.37f * x + 0.61f * y;
		return s - std::floor(s) - 0.5f;
	}

	// Comme stripes, et depend aussi de z : une valeur lue dans la mauvaise couche se voit.
	float slantedStripes(float x, float y, float z, void*)
	{
		const float s = 0.37f * x + 0.61f * y + 0.29f * z;
		return s - std::floor(s) - 0.5f;
	}

	float ball(float x, float y, float z, void*)
	{
		return x * x + y * y + z * z - 0.36f;
	}

	// Empreinte FNV-1a de la suite des coins rendus par compute (position et indice).
	struct CornerDigest
	{
		std::uint64_t hash = 1469598103934665603ull;
		std::size_t   corners = 0;
		void mix(const void* p, std::size_t n)
		{
			const unsigned char* b = static_cast<const unsigned char*>(p);
			for (std::size_t i = 0; i < n; ++i)
			{
				hash ^= b[i];
				hash *= 1099511628211ull;
			}
		}
	};

	bool digestCorner(float x, float y, float z, int index, void* self)
	{
		CornerDigest* d = static_cast<CornerDigest*>(self);
		d->mix(&x, sizeof x);
		d->mix(&y, sizeof y);
		d->mix(&z, sizeof z);
		d->mix(&index, sizeof index);
		return ++d->corners % 3 == 0;
	}

	// Le rappel de sommet recoit le pointeur `data` d'ImplicitSurface, protege.
	class DigestSurface : public ImplicitSurface
	{
	public:
		explicit DigestSurface(CornerDigest* digest)
		{
			data = digest;
			set_vertex_func(&digestCorner);
		}
	};

	struct FieldLog
	{
		long long          evaluations = 0;
		std::vector<float> normals;
	};

	float loggedBall(float x, float y, float z, void* log)
	{
		++static_cast<FieldLog*>(log)->evaluations;
		return ball(x, y, z, nullptr);
	}

	void logNormal(float x, float y, float z, void* log)
	{
		std::vector<float>& n = static_cast<FieldLog*>(log)->normals;
		n.insert(n.end(), {x, y, z});
	}
}

// Indices des caches : le cache de valeurs (compute(1)) et l'evaluation directe
// (compute(0)) rendent la meme suite de coins, au-dela de la plage ou un indice
// calcule en float est exact (R0 = 2 895) et ou il deborde (2 897). R1 != R0 : un
// pas pris sur le mauvais axe se voit. Grille R0 x R1 x 1, noeuds entiers, champ
// a deux variables qui coupe une large part des cubes.
TEST(TEST_cgmesh_implicit_surface, cache_and_direct_evaluation_agree_beyond_float_indices)
{
	std::vector<int> sizes = {2895, 2896, 2897};
#ifndef CG_SANITIZERS_ENABLED
	sizes.push_back(4096);
#endif
	for (const int r0 : sizes)
	{
		SCOPED_TRACE(r0);
		const int r1 = r0 + 7;
		CornerDigest cached, direct;
		for (CornerDigest* digest : {&cached, &direct})
		{
			DigestSurface mc(digest);
			mc.set_bbox(0.f, 0.f, 0.f, static_cast<float>(r0), static_cast<float>(r1), 1.f);
			mc.set_resolution_per_unit(1);
			mc.set_eval_func(&stripes);
			mc.set_value(0.f);
			ASSERT_TRUE(mc.compute(digest == &cached ? 1 : 0));
		}
		EXPECT_GT(direct.corners, 1000000u);
		EXPECT_EQ(cached.corners, direct.corners);
		EXPECT_EQ(cached.hash, direct.hash);
	}
}

// Meme comparaison sur trois couches de cubes : la mise a jour des couches suivantes
// (double tampon des valeurs, recopie des indices d'aretes) au-dela des indices exacts
// en float. Le champ depend de z.
TEST(TEST_cgmesh_implicit_surface, cache_and_direct_evaluation_agree_over_several_layers)
{
	const int r0 = 2897, r1 = r0 + 7, r2 = 3;
	CornerDigest cached, direct;
	for (CornerDigest* digest : {&cached, &direct})
	{
		DigestSurface mc(digest);
		mc.set_bbox(0.f, 0.f, 0.f, static_cast<float>(r0), static_cast<float>(r1), static_cast<float>(r2));
		mc.set_resolution_per_unit(1);
		mc.set_eval_func(&slantedStripes);
		mc.set_value(0.f);
		ASSERT_TRUE(mc.compute(digest == &cached ? 1 : 0));
	}
	EXPECT_GT(direct.corners, 3000000u);
	EXPECT_EQ(cached.corners, direct.corners);
	EXPECT_EQ(cached.hash, direct.hash);
}

// Les sommets sont numerotes a partir de 0 a chaque appel : une seconde
// triangulation sur le meme objet est identique a la premiere.
TEST(TEST_cgmesh_implicit_surface, a_second_triangulation_on_the_same_object_is_identical)
{
	ImplicitSurface mc;
	mc.set_bbox(-1.f, -1.f, -1.f, 1.f, 1.f, 1.f);
	mc.set_resolution_per_unit(8);
	mc.set_eval_func(&ball);
	mc.set_value(0.f);
	const Triangulation a(&mc), b(&mc);
	ASSERT_TRUE(a.ok);
	ASSERT_TRUE(b.ok);
	ASSERT_GT(a.nfaces, 0);
	ASSERT_EQ(a.nvertices, b.nvertices);
	ASSERT_EQ(a.nfaces, b.nfaces);
	EXPECT_TRUE(std::equal(a.vertices, a.vertices + 3 * a.nvertices, b.vertices));
	EXPECT_TRUE(std::equal(a.faces, a.faces + 3 * a.nfaces, b.faces));
}

// set_boundary(1) elargit la boite et la resolution le temps d'un calcul seulement :
// une seconde triangulation sur le meme objet est identique a la premiere.
TEST(TEST_cgmesh_implicit_surface, a_second_triangulation_with_a_boundary_is_identical)
{
	ImplicitSurface mc;
	mc.set_bbox(-1.f, -1.f, -1.f, 1.f, 1.f, 1.f);
	mc.set_resolution_per_unit(8);
	mc.set_eval_func(&ball);
	mc.set_value(0.f);
	mc.set_boundary(1);
	const Triangulation a(&mc), b(&mc);
	ASSERT_TRUE(a.ok);
	ASSERT_TRUE(b.ok);
	ASSERT_GT(a.nfaces, 0);
	ASSERT_EQ(a.nvertices, b.nvertices);
	ASSERT_EQ(a.nfaces, b.nfaces);
	EXPECT_TRUE(std::equal(a.vertices, a.vertices + 3 * a.nvertices, b.vertices));
	EXPECT_TRUE(std::equal(a.faces, a.faces + 3 * a.nfaces, b.faces));
	vec3f lo, hi;
	mc.get_bbox(lo, hi);
	EXPECT_EQ(lo.fX, -1.f);
	EXPECT_EQ(lo.fZ, -1.f);
	EXPECT_EQ(hi.fY, 1.f);
	EXPECT_EQ(hi.fZ, 1.f);
}

// Avec le cache et sans rappel de normale ni de couleur, le champ est evalue une
// fois par noeud. Avec un rappel de normale, les normales (gradient, six
// evaluations par sommet d'arete) arrivent, unitaires, et la geometrie ne change pas.
TEST(TEST_cgmesh_implicit_surface, the_field_is_evaluated_once_per_node_without_normal_callback)
{
	const int r = 20;   // 2 x 10 cubes par axe
	auto make = [](ImplicitSurface& mc, FieldLog& log) {
		mc.set_bbox(-1.f, -1.f, -1.f, 1.f, 1.f, 1.f);
		mc.set_resolution_per_unit(10);
		mc.set_eval_func(&loggedBall);
		mc.set_eval_data(&log);
		mc.set_value(0.f);
	};
	FieldLog plain;
	ImplicitSurface a;
	make(a, plain);
	const Triangulation ta(&a);
	ASSERT_TRUE(ta.ok);
	ASSERT_GT(ta.nfaces, 0);
	EXPECT_EQ(plain.evaluations, static_cast<long long>(r + 1) * (r + 1) * (r + 1));
	EXPECT_TRUE(plain.normals.empty());

	FieldLog withNormals;
	ImplicitSurface b;
	make(b, withNormals);
	b.set_normal_func(&logNormal);
	const Triangulation tb(&b);
	ASSERT_TRUE(tb.ok);
	EXPECT_GT(withNormals.evaluations, plain.evaluations);
	ASSERT_EQ(withNormals.normals.size(), 3u * 3u * static_cast<std::size_t>(tb.nfaces));
	for (std::size_t i = 0; i < withNormals.normals.size(); i += 3)
	{
		const float* n = &withNormals.normals[i];
		EXPECT_NEAR(std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), 1.0, 1e-5);
	}
	ASSERT_EQ(ta.nvertices, tb.nvertices);
	ASSERT_EQ(ta.nfaces, tb.nfaces);
	EXPECT_TRUE(std::equal(ta.vertices, ta.vertices + 3 * ta.nvertices, tb.vertices));
	EXPECT_TRUE(std::equal(ta.faces, ta.faces + 3 * ta.nfaces, tb.faces));
}

// Le tandem accumule ses donnees par face et par sommet avec les triangles : au-dela
// de 100 000 faces avant decimation, la sortie reste un maillage valide.
TEST(TEST_cgmesh_implicit_surface, tandem_beyond_100000_faces_gives_a_valid_mesh)
{
	auto configure = [](ImplicitSurface& mc) {
		mc.set_bbox(-1.f, -1.f, -1.f, 1.f, 1.f, 1.f);
		mc.set_resolution_per_unit(100);
		mc.set_eval_func(&ball);
		mc.set_value(0.f);
	};
	ImplicitSurface plain;
	configure(plain);
	const Triangulation before(&plain);
	ASSERT_TRUE(before.ok);
	ASSERT_GT(before.nfaces, 100000);

	ImplicitSurfaceTandem tandem;
	configure(tandem);
	const Triangulation t(&tandem);
	ASSERT_TRUE(t.ok);
	ASSERT_GT(t.nfaces, 0);
	ASSERT_GT(t.nvertices, 0);
	for (int f = 0; f < 3 * t.nfaces; f++)
		ASSERT_LT(static_cast<int>(t.faces[f]), t.nvertices);
	for (int i = 0; i < 3 * t.nvertices; i++)
		ASSERT_TRUE(std::isfinite(t.vertices[i]));
}

// Garde de resolution : une resolution nulle ou une couche de plus de 2^28 noeuds
// est refusee sans evaluer le champ.
TEST(TEST_cgmesh_implicit_surface, out_of_range_resolutions_are_refused)
{
	struct Case { const char* name; float box[6]; };
	const Case cases[] = {
		{"flat in z", {0.f, 0.f, 0.f, 4.f, 4.f, 0.5f}},
		{"layer of 16 385 x 16 385 nodes", {0.f, 0.f, 0.f, 16384.f, 16384.f, 1.f}},
	};
	for (const Case& c : cases)
	{
		SCOPED_TRACE(c.name);
		FieldLog log;
		ImplicitSurface mc;
		mc.set_bbox(c.box[0], c.box[1], c.box[2], c.box[3], c.box[4], c.box[5]);
		mc.set_resolution_per_unit(1);
		mc.set_eval_func(&loggedBall);
		mc.set_eval_data(&log);
		Triangulation t(&mc);
		EXPECT_FALSE(t.ok);
		EXPECT_EQ(t.nvertices, 0);
		EXPECT_EQ(t.nfaces, 0);
		EXPECT_EQ(t.vertices, nullptr);
		EXPECT_EQ(t.faces, nullptr);
		EXPECT_EQ(log.evaluations, 0);
	}
}
