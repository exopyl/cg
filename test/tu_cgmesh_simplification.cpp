#include <gtest/gtest.h>

#include <vector>
#include <cmath>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/mesh_metrics.h"
#include "../src/cgmesh/bvh.h"

//
// QEM edge-collapse decimation (Mesh_half_edge::simplify), MVP / Etape 1.
//
// These tests double as the execution-validation called for in
// simplification.md (Etape 0): they exercise the collapse path on a real mesh
// and assert the result stays a valid, hole-free triangle mesh.
//

static Mesh_half_edge *load_rabbit()
{
	Mesh_half_edge *he = new Mesh_half_edge();
	he->m_pMesh->load("./test/data/rabbit.obj");
	he->create_half_edge();
	return he;
}

// Every face must index existing vertices and be a non-degenerate triangle.
static void expect_valid_triangle_mesh(Mesh *m)
{
	ASSERT_GT(m->m_nFaces, 0u);
	ASSERT_GT(m->m_nVertices, 0u);
	for (unsigned int f = 0; f < m->m_nFaces; f++)
	{
		Face *face = m->m_pFaces[f];
		ASSERT_NE(face, nullptr) << "face " << f << " is null (hole)";
		ASSERT_EQ(face->m_nVertices, 3u) << "face " << f << " is not a triangle";
		int a = face->m_pVertices[0];
		int b = face->m_pVertices[1];
		int c = face->m_pVertices[2];
		EXPECT_GE(a, 0);
		EXPECT_GE(b, 0);
		EXPECT_GE(c, 0);
		EXPECT_LT((unsigned int)a, m->m_nVertices);
		EXPECT_LT((unsigned int)b, m->m_nVertices);
		EXPECT_LT((unsigned int)c, m->m_nVertices);
		EXPECT_TRUE(a != b && b != c && a != c) << "degenerate face " << f;
	}
}

TEST(TEST_cgmesh_simplification, halve_rabbit)
{
	Mesh_half_edge *he = load_rabbit();
	unsigned int nf0 = he->m_pMesh->m_nFaces;
	unsigned int nv0 = he->m_pMesh->m_nVertices;
	ASSERT_GT(nf0, 0u);

	he->simplify(0.5f);

	unsigned int nf1 = he->m_pMesh->m_nFaces;
	unsigned int nv1 = he->m_pMesh->m_nVertices;
	printf("decimation 0.5 : faces %u -> %u, vertices %u -> %u\n", nf0, nf1, nv0, nv1);

	// Faces reduced and reasonably close to the target (not every collapse is
	// admissible, so we do not demand exactly nf0/2).
	EXPECT_LT(nf1, nf0);
	EXPECT_GE(nf1, nf0 / 4); // sanity: did not collapse to almost nothing
	EXPECT_LT(nv1, nv0);

	expect_valid_triangle_mesh(he->m_pMesh);

	// The compacted result must rebuild a half-edge structure without crashing.
	he->create_half_edge();
	EXPECT_NE(he->GetCheMesh(), nullptr);

	he->m_pMesh->save("export_simplification_rabbit_0.5.obj");
	delete he;
}

TEST(TEST_cgmesh_simplification, aggressive_rabbit)
{
	Mesh_half_edge *he = load_rabbit();
	unsigned int nf0 = he->m_pMesh->m_nFaces;

	he->simplify(0.1f);

	unsigned int nf1 = he->m_pMesh->m_nFaces;
	printf("decimation 0.1 : faces %u -> %u\n", nf0, nf1);

	EXPECT_LT(nf1, nf0 / 2);
	expect_valid_triangle_mesh(he->m_pMesh);
	he->m_pMesh->save("export_simplification_rabbit_0.1.obj");
	delete he;
}

TEST(TEST_cgmesh_simplification, ratio_one_is_noop)
{
	Mesh_half_edge *he = load_rabbit();
	unsigned int nf0 = he->m_pMesh->m_nFaces;

	he->simplify(1.0f); // target == current face count: no collapse

	EXPECT_EQ(he->m_pMesh->m_nFaces, nf0);
	expect_valid_triangle_mesh(he->m_pMesh);
	delete he;
}

//
// Etape 2 — feature preservation flag.
//

// A flat NxN grid in the z=0 plane, split into a left half (material 0) and a
// right half (material 1) along x=0.5. The plane is geometrically featureless
// (any collapse has zero quadric error), so only the MATERIAL SEAM along x=0.5
// can keep that line of vertices alive — which isolates the feature mechanism
// from geometry.
static Mesh_half_edge *make_material_seam_grid(int N)
{
	int side = N + 1;
	std::vector<float> verts;
	for (int j = 0; j < side; j++)
		for (int i = 0; i < side; i++)
		{
			verts.push_back((float)i / N);
			verts.push_back((float)j / N);
			verts.push_back(0.f);
		}

	std::vector<unsigned int> faces;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
		{
			unsigned int v00 = j * side + i;
			unsigned int v10 = j * side + (i + 1);
			unsigned int v01 = (j + 1) * side + i;
			unsigned int v11 = (j + 1) * side + (i + 1);
			faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
			faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
		}

	Mesh_half_edge *he = new Mesh_half_edge((int)(verts.size() / 3), verts.data(),
	                                        (int)(faces.size() / 3), faces.data());

	for (unsigned int f = 0; f < he->m_pMesh->m_nFaces; f++)
	{
		Face *fc = he->m_pMesh->m_pFaces[f];
		float cx = (verts[3 * fc->m_pVertices[0]] +
		            verts[3 * fc->m_pVertices[1]] +
		            verts[3 * fc->m_pVertices[2]]) / 3.f;
		fc->SetMaterialId(cx < 0.5f ? 0u : 1u);
	}
	he->create_half_edge();
	return he;
}

static int count_on_seam(Mesh *m)
{
	int c = 0;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
		if (fabs(m->m_pVertices[3 * i] - 0.5f) < 1e-4f)
			c++;
	return c;
}

TEST(TEST_cgmesh_simplification, feature_material_seam_preserved)
{
	Mesh_half_edge *on = make_material_seam_grid(20);
	Mesh_half_edge *off = make_material_seam_grid(20);

	on->simplify(0.3f, {/*preserve_features*/ true});
	off->simplify(0.3f, {/*preserve_features*/ false});

	int s_on = count_on_seam(on->m_pMesh);
	int s_off = count_on_seam(off->m_pMesh);
	printf("material seam — vertices kept on x=0.5 : preserve=%d, no-preserve=%d\n", s_on, s_off);

	// The seam survives markedly better with preservation on.
	EXPECT_GT(s_on, s_off);
	expect_valid_triangle_mesh(on->m_pMesh);
	expect_valid_triangle_mesh(off->m_pMesh);

	delete on;
	delete off;
}

//
// Etape 3 (Voie A) — appearance attribute preservation.
//

// A flat NxN grid with a per-vertex colour set to a LINEAR field of position:
// colour = (x, y, 0). Edge-parameter interpolation preserves a linear field
// exactly, so every surviving vertex must keep colour == its own (x, y).
static Mesh_half_edge *make_colored_grid(int N)
{
	int side = N + 1;
	std::vector<float> verts;
	for (int j = 0; j < side; j++)
		for (int i = 0; i < side; i++)
		{
			verts.push_back((float)i / N);
			verts.push_back((float)j / N);
			verts.push_back(0.f);
		}

	std::vector<unsigned int> faces;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
		{
			unsigned int v00 = j * side + i;
			unsigned int v10 = j * side + (i + 1);
			unsigned int v01 = (j + 1) * side + i;
			unsigned int v11 = (j + 1) * side + (i + 1);
			faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
			faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
		}

	Mesh_half_edge *he = new Mesh_half_edge((int)(verts.size() / 3), verts.data(),
	                                        (int)(faces.size() / 3), faces.data());

	Mesh *m = he->m_pMesh;
	m->m_pVertexColors.resize(m->m_nVertices * 3);
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		m->m_pVertexColors[3 * i] = m->m_pVertices[3 * i];     // r = x
		m->m_pVertexColors[3 * i + 1] = m->m_pVertices[3 * i + 1]; // g = y
		m->m_pVertexColors[3 * i + 2] = 0.f;
	}
	he->create_half_edge();
	return he;
}

TEST(TEST_cgmesh_simplification, attributes_linear_color_preserved)
{
	Mesh_half_edge *he = make_colored_grid(20);

	he->simplify(0.3f, {/*preserve_features*/ false, 45.0f, /*preserve_attributes*/ true});

	Mesh *m = he->m_pMesh;
	ASSERT_EQ(m->m_pVertexColors.size(), m->m_nVertices * 3u)
		<< "colours were not carried into the result";

	float max_err = 0.f;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		float er = fabs(m->m_pVertexColors[3 * i] - m->m_pVertices[3 * i]);
		float eg = fabs(m->m_pVertexColors[3 * i + 1] - m->m_pVertices[3 * i + 1]);
		if (er > max_err) max_err = er;
		if (eg > max_err) max_err = eg;
	}
	printf("linear colour field max deviation after decimation : %g\n", max_err);
	EXPECT_LT(max_err, 1e-3f); // linear field preserved exactly by interpolation

	expect_valid_triangle_mesh(m);
	delete he;
}

TEST(TEST_cgmesh_simplification, attributes_off_drops_colors)
{
	Mesh_half_edge *he = make_colored_grid(20);

	he->simplify(0.3f, {/*preserve_features*/ false, 45.0f, /*preserve_attributes*/ false});

	// Colours are not carried; the rebuilt mesh has no per-vertex colours.
	EXPECT_NE(he->m_pMesh->m_pVertexColors.size(), he->m_pMesh->m_nVertices * 3u);
	expect_valid_triangle_mesh(he->m_pMesh);
	delete he;
}

//
// Etape 3 (Voie B) — generalized R^6 quadric (position + colour).
//

// Flat grid whose per-vertex colour is a SHARP STEP in x (red=0 left of x=0.5,
// red=1 right). The plane is geometrically featureless, so only an
// attribute-aware metric (Voie B) can avoid blurring colours across the step.
static Mesh_half_edge *make_step_colored_grid(int N)
{
	int side = N + 1;
	std::vector<float> verts;
	for (int j = 0; j < side; j++)
		for (int i = 0; i < side; i++)
		{
			verts.push_back((float)i / N);
			verts.push_back((float)j / N);
			verts.push_back(0.f);
		}

	std::vector<unsigned int> faces;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
		{
			unsigned int v00 = j * side + i;
			unsigned int v10 = j * side + (i + 1);
			unsigned int v01 = (j + 1) * side + i;
			unsigned int v11 = (j + 1) * side + (i + 1);
			faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
			faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
		}

	Mesh_half_edge *he = new Mesh_half_edge((int)(verts.size() / 3), verts.data(),
	                                        (int)(faces.size() / 3), faces.data());
	Mesh *m = he->m_pMesh;
	m->m_pVertexColors.resize(m->m_nVertices * 3);
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		m->m_pVertexColors[3 * i] = (m->m_pVertices[3 * i] < 0.5f) ? 0.f : 1.f;
		m->m_pVertexColors[3 * i + 1] = 0.f;
		m->m_pVertexColors[3 * i + 2] = 0.f;
	}
	he->create_half_edge();
	return he;
}

// Count surviving vertices sitting on the colour boundary x=0.5.
static int count_on_half(Mesh *m)
{
	int c = 0;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
		if (fabs(m->m_pVertices[3 * i] - 0.5f) < 1e-4f)
			c++;
	return c;
}

TEST(TEST_cgmesh_simplification, attribute_metric_linear_color_exact)
{
	Mesh_half_edge *he = make_colored_grid(20);

	// Voie B: {preserve_features=false, angle, preserve_attributes=true, attribute_metric=true}
	he->simplify(0.3f, {false, 45.0f, true, true});

	Mesh *m = he->m_pMesh;
	ASSERT_EQ(m->m_pVertexColors.size(), m->m_nVertices * 3u);
	float max_err = 0.f;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		float er = fabs(m->m_pVertexColors[3 * i] - m->m_pVertices[3 * i]);
		float eg = fabs(m->m_pVertexColors[3 * i + 1] - m->m_pVertices[3 * i + 1]);
		if (er > max_err) max_err = er;
		if (eg > max_err) max_err = eg;
	}
	printf("Voie B linear colour field max deviation : %g\n", max_err);
	EXPECT_LT(max_err, 1e-3f); // R^6 quadric reproduces a linear field exactly
	expect_valid_triangle_mesh(m);
	delete he;
}

TEST(TEST_cgmesh_simplification, attribute_metric_preserves_step)
{
	Mesh_half_edge *B = make_step_colored_grid(24);
	Mesh_half_edge *A = make_step_colored_grid(24);

	// Voie B with a moderate colour weight: crossing the step costs more, so
	// collapses are steered away from the boundary. (A very large weight makes
	// the nD costs numerically ill-conditioned on this perfectly flat grid, so
	// the ordering benefit gets swamped — a moderate weight is the meaningful
	// regime.)
	B->simplify(0.5f, {false, 45.0f, true, true, 30.0f});
	A->simplify(0.5f, {false, 45.0f, true, false}); // Voie A: geometry-only QEM

	int nb = count_on_half(B->m_pMesh);
	int na = count_on_half(A->m_pMesh);
	printf("vertices kept on colour boundary x=0.5 : Voie B=%d, Voie A=%d\n", nb, na);

	// The attribute metric resists collapsing across the colour step, so it
	// keeps markedly more vertices along the boundary than geometry-only QEM
	// (which sees a featureless flat plane there).
	EXPECT_GT(nb, na);
	expect_valid_triangle_mesh(B->m_pMesh);
	expect_valid_triangle_mesh(A->m_pMesh);

	delete B;
	delete A;
}

//
// Etape 4 — error-bounded stop (max_error, proxy QEM).
//
TEST(TEST_cgmesh_simplification, max_error_stops_early)
{
	Mesh_half_edge *ref = load_rabbit(); // pristine copy for error measurement

	Mesh_half_edge *he = load_rabbit();
	unsigned int nf0 = he->m_pMesh->m_nFaces;

	// Very aggressive target (2%) but a tight error bound (0.5% of bbox diag):
	// the error bound must halt decimation well before the face target.
	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features = false;
	opt.preserve_attributes = false;
	opt.max_error = 0.005f;
	he->simplify(0.02f, opt);

	unsigned int nf = he->m_pMesh->m_nFaces;
	float rel = mesh_hausdorff_relative(*ref->m_pMesh, *he->m_pMesh);
	printf("max_error stop : faces %u -> %u (2%% target = %u), relative Hausdorff = %g\n",
	       nf0, nf, (unsigned)(nf0 * 0.02f), rel);

	// Stopped early: many more faces than the raw 2% target would give.
	EXPECT_GT(nf, (unsigned)(nf0 * 0.02f));
	// Achieved deviation stays in the right ballpark of the requested bound
	// (proxy, so allow generous slack).
	EXPECT_LT(rel, 0.05f);
	expect_valid_triangle_mesh(he->m_pMesh);

	delete ref;
	delete he;
}

TEST(TEST_cgmesh_simplification, exact_error_bounds_surface)
{
	Mesh_half_edge *ref = load_rabbit();
	Mesh_half_edge *he = load_rabbit();
	unsigned int nf0 = he->m_pMesh->m_nFaces;

	ref->m_pMesh->computebbox();
	float diag = ref->m_pMesh->bbox_diagonal_length();

	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features = false;
	opt.preserve_attributes = false;
	opt.max_error = 0.02f;     // 2% of the bbox diagonal
	opt.exact_error = true;    // reliable surface bound
	he->simplify(0.05f, opt);

	unsigned int nf = he->m_pMesh->m_nFaces;

	// What the gate guarantees: every decimated VERTEX lies within max_error of
	// the original surface. Measure that directly (BVH on the original).
	BVH bref;
	bref.build(*ref->m_pMesh);
	float worst_vertex = 0.f;
	for (unsigned int i = 0; i < he->m_pMesh->m_nVertices; i++)
	{
		Vector3f p;
		p.Set ( he->m_pMesh->m_pVertices[3*i], he->m_pMesh->m_pVertices[3*i+1], he->m_pMesh->m_pVertices[3*i+2]);
		float d = sqrtf(bref.closest_distance2(p));
		if (d > worst_vertex) worst_vertex = d;
	}
	// For information: full sampled Hausdorff (includes face centroids, which the
	// per-vertex gate does NOT bound — triangle interiors over curved regions).
	HausdorffResult h = mesh_hausdorff(*he->m_pMesh, *ref->m_pMesh);
	printf("exact bound : faces %u -> %u | worst decimated VERTEX = %g (bound %g) | sampled Hausdorff a->b = %g\n",
	       nf0, nf, worst_vertex, 0.02f * diag, h.a_to_b);

	EXPECT_LT(nf, nf0);                                  // it decimated
	EXPECT_LE(worst_vertex, 0.02f * diag * 1.001f);      // gate honoured (tiny float slack)
	expect_valid_triangle_mesh(he->m_pMesh);

	delete ref;
	delete he;
}

//
// Voie B — UV wedges (SplitVerticesByUVSeams + preserve_uv).
//

TEST(TEST_cgmesh_simplification, uv_seam_split)
{
	// One quad (two triangles) sharing edge v0-v2. Face 0 references uv index 0
	// for v0, face 1 references uv index 3 for v0 -> v0 is a UV seam and must be
	// duplicated; v2 keeps the same uv index on both faces -> not duplicated.
	Mesh *m = new Mesh();
	float V[] = {0,0,0, 1,0,0, 1,1,0, 0,1,0};
	unsigned int F[] = {0,1,2, 0,2,3};
	m->SetVertices(4, V);
	m->SetFaces(2, 3, F);
	m->m_nTextureCoordinates = 5;
	m->m_pTextureCoordinates = {0.f,0.f, 1.f,0.f, 1.f,1.f, 0.5f,0.5f, 0.f,1.f};

	Face *f0 = m->m_pFaces[0];
	f0->m_bUseTextureCoordinates = true; f0->ActivateTextureCoordinatesIndices();
	f0->SetTexCoord(0u, 0u); f0->SetTexCoord(1u, 1u); f0->SetTexCoord(2u, 2u);
	Face *f1 = m->m_pFaces[1];
	f1->m_bUseTextureCoordinates = true; f1->ActivateTextureCoordinatesIndices();
	f1->SetTexCoord(0u, 3u); f1->SetTexCoord(1u, 2u); f1->SetTexCoord(2u, 4u);

	m->SplitVerticesByUVSeams();

	EXPECT_EQ(m->m_nVertices, 5u);                          // v0 split into two
	EXPECT_EQ(m->m_pTextureCoordinates.size(), 10u);        // now vertex-parallel
	// Idempotent: a second call changes nothing.
	m->SplitVerticesByUVSeams();
	EXPECT_EQ(m->m_nVertices, 5u);
	delete m;
}

// Flat grid carrying a VERTEX-PARALLEL linear UV field uv=(x,y).
static Mesh_half_edge *make_uv_grid(int N)
{
	int side = N + 1;
	std::vector<float> verts;
	for (int j = 0; j < side; j++)
		for (int i = 0; i < side; i++)
		{
			verts.push_back((float)i / N);
			verts.push_back((float)j / N);
			verts.push_back(0.f);
		}
	std::vector<unsigned int> faces;
	for (int j = 0; j < N; j++)
		for (int i = 0; i < N; i++)
		{
			unsigned int v00 = j*side+i, v10 = j*side+i+1, v01 = (j+1)*side+i, v11 = (j+1)*side+i+1;
			faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
			faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
		}
	Mesh_half_edge *he = new Mesh_half_edge((int)(verts.size()/3), verts.data(), (int)(faces.size()/3), faces.data());
	Mesh *m = he->m_pMesh;
	m->m_nTextureCoordinates = m->m_nVertices;
	m->m_pTextureCoordinates.resize(m->m_nVertices * 2);
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		m->m_pTextureCoordinates[2*i]   = m->m_pVertices[3*i];   // u = x
		m->m_pTextureCoordinates[2*i+1] = m->m_pVertices[3*i+1]; // v = y
	}
	he->create_half_edge();
	return he;
}

TEST(TEST_cgmesh_simplification, preserve_uv_linear_exact)
{
	Mesh_half_edge *he = make_uv_grid(20);

	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features = false;
	opt.preserve_uv = true;
	he->simplify(0.3f, opt);

	Mesh *m = he->m_pMesh;
	ASSERT_EQ(m->m_pTextureCoordinates.size(), m->m_nVertices * 2u) << "UV not emitted vertex-parallel";

	float max_err = 0.f;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		float eu = fabs(m->m_pTextureCoordinates[2*i]   - m->m_pVertices[3*i]);
		float ev = fabs(m->m_pTextureCoordinates[2*i+1] - m->m_pVertices[3*i+1]);
		if (eu > max_err) max_err = eu;
		if (ev > max_err) max_err = ev;
	}
	printf("preserve_uv linear field max deviation : %g\n", max_err);
	EXPECT_LT(max_err, 1e-3f); // linear UV field preserved exactly
	expect_valid_triangle_mesh(m);
	delete he;
}

TEST(TEST_cgmesh_simplification, attribute_metric_uv_linear_exact)
{
	// nD metric over R^5 = position(3) + UV(2): a linear UV field must be
	// reproduced exactly.
	Mesh_half_edge *he = make_uv_grid(20);
	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features = false;
	opt.preserve_uv = true;
	opt.attribute_metric = true; // UV enters the quadric (no colours here -> R^5)
	he->simplify(0.3f, opt);

	Mesh *m = he->m_pMesh;
	ASSERT_EQ(m->m_pTextureCoordinates.size(), m->m_nVertices * 2u);
	float max_err = 0.f;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		float eu = fabs(m->m_pTextureCoordinates[2*i]   - m->m_pVertices[3*i]);
		float ev = fabs(m->m_pTextureCoordinates[2*i+1] - m->m_pVertices[3*i+1]);
		if (eu > max_err) max_err = eu;
		if (ev > max_err) max_err = ev;
	}
	printf("nD UV (R^5) linear field max deviation : %g\n", max_err);
	EXPECT_LT(max_err, 1e-3f);
	expect_valid_triangle_mesh(m);
	delete he;
}

TEST(TEST_cgmesh_simplification, attribute_metric_uv_and_color_exact)
{
	// nD metric over R^8 = position(3) + colour(3) + UV(2). Linear colour and
	// UV fields must both be reproduced exactly.
	Mesh_half_edge *he = make_uv_grid(20);
	Mesh *m = he->m_pMesh;
	m->m_pVertexColors.resize(m->m_nVertices * 3);
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		m->m_pVertexColors[3*i]   = m->m_pVertices[3*i];     // r = x
		m->m_pVertexColors[3*i+1] = m->m_pVertices[3*i+1];   // g = y
		m->m_pVertexColors[3*i+2] = 0.f;
	}

	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features = false;
	opt.preserve_uv = true;
	opt.attribute_metric = true; // colour + UV -> R^8
	he->simplify(0.3f, opt);

	m = he->m_pMesh; // simplify() replaced the mesh; re-fetch (old pointer freed)
	ASSERT_EQ(m->m_pTextureCoordinates.size(), m->m_nVertices * 2u);
	ASSERT_EQ(m->m_pVertexColors.size(), m->m_nVertices * 3u);
	float max_err = 0.f;
	for (unsigned int i = 0; i < m->m_nVertices; i++)
	{
		float ex = m->m_pVertices[3*i], ey = m->m_pVertices[3*i+1];
		float e1 = fabs(m->m_pTextureCoordinates[2*i] - ex);
		float e2 = fabs(m->m_pTextureCoordinates[2*i+1] - ey);
		float e3 = fabs(m->m_pVertexColors[3*i] - ex);
		float e4 = fabs(m->m_pVertexColors[3*i+1] - ey);
		if (e1 > max_err) max_err = e1;
		if (e2 > max_err) max_err = e2;
		if (e3 > max_err) max_err = e3;
		if (e4 > max_err) max_err = e4;
	}
	printf("nD colour+UV (R^8) linear fields max deviation : %g\n", max_err);
	EXPECT_LT(max_err, 1e-3f);
	expect_valid_triangle_mesh(m);
	delete he;
}

// Diagnostic: decimate a welded STL (the sinaia path) and report topology.
TEST(TEST_cgmesh_simplification, diag_stl_decimation)
{
	Mesh_half_edge *he = new Mesh_half_edge();
	he->m_pMesh->load("./test/data/rabbit.obj");
	Mesh *m = he->m_pMesh;
	printf("STL loaded: %u verts, %u faces\n", m->m_nVertices, m->m_nFaces);

	unsigned int vBefore = m->m_nVertices;
	m->MergeVertices();
	printf("after merge: %u verts (was %u), %u faces\n", m->m_nVertices, vBefore, m->m_nFaces);

	std::vector<unsigned int> nonManifold, borders;
	m->GetTopologicIssues(nonManifold, borders);
	printf("topology: %zu non-manifold edges, %zu border edges\n", nonManifold.size(), borders.size());

	he->create_half_edge();
	he->simplify(0.3f, {false}); // geometry-only

	Mesh *r = he->m_pMesh;
	int degenerate = 0, badIndex = 0;
	for (unsigned int f = 0; f < r->m_nFaces; f++)
	{
		Face *face = r->m_pFaces[f];
		if (!face) { degenerate++; continue; }
		int a = face->m_pVertices[0], b = face->m_pVertices[1], c = face->m_pVertices[2];
		if (a == b || b == c || a == c) degenerate++;
		if ((unsigned)a >= r->m_nVertices || (unsigned)b >= r->m_nVertices || (unsigned)c >= r->m_nVertices) badIndex++;
	}
	std::vector<unsigned int> nmAfter, bAfter;
	r->GetTopologicIssues(nmAfter, bAfter);
	printf("decimated: %u verts, %u faces, %d degenerate, %d bad-index, "
	       "%zu non-manifold edges, %zu border edges\n",
	       r->m_nVertices, r->m_nFaces, degenerate, badIndex, nmAfter.size(), bAfter.size());

	// Guarantee: a manifold input (bunny after merge: 0 non-manifold) stays
	// manifold after decimation. Edge-collapse cannot CREATE non-manifold edges
	// here; it can only carry through ones already present in the input.
	EXPECT_EQ(degenerate, 0);
	EXPECT_EQ(badIndex, 0);
	EXPECT_EQ(nmAfter.size(), 0u);

	r->save("export_diag_stl_0.3.obj");
	delete he;
}

TEST(TEST_cgmesh_simplification, feature_flag_runs_on_rabbit)
{
	// Both modes must produce a valid mesh on a real model.
	Mesh_half_edge *on = load_rabbit();
	on->simplify(0.3f, {/*preserve_features*/ true, 30.0f});
	expect_valid_triangle_mesh(on->m_pMesh);
	delete on;

	Mesh_half_edge *off = load_rabbit();
	off->simplify(0.3f, {/*preserve_features*/ false});
	expect_valid_triangle_mesh(off->m_pMesh);
	delete off;
}
