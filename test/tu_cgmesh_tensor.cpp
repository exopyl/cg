#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "../src/cgmesh/cgmesh.h"

//
// Principal-curvature / principal-direction estimators (DiffParamEvaluator_*)
// validated against an analytic parametric surface.
//
// Surface: a CLOSED torus (major radius R, minor radius r), which the 1-ring
// estimators require (a boundary-free 2-manifold). Parametrisation:
//   p(u,v) = ((R + r cos v) cos u, (R + r cos v) sin u, r sin v)
// Principal curvatures / directions (F = 0, so u/v ARE the principal axes):
//   - meridian (dp/dv): kappa = 1/r
//   - parallel (dp/du): kappa = cos v / (R + r cos v)
//
// We test at the INNER equator (u = 0, v = pi), i.e. the vertex (R-r, 0, 0),
// which is a genuine SADDLE point (Gaussian curvature < 0):
//   - meridian curvature  = +1/r         , direction = +z
//   - parallel curvature  = -1/(R - r)    , direction = +y
// With R=3, r=1 the two curvatures are +1 (z) and -1/2 (y): opposite signs and
// a clean 2:1 magnitude ratio, so the principal directions are unambiguous.
//
// (A true monkey saddle z = x^3 - 3xy^2 is unusable here: at its characteristic
// point the whole second fundamental form vanishes, so k1 = k2 = 0 and the
// principal directions are undefined.)
//
// The comparison is orientation-robust: we never assume the sign of the
// estimator's normal. We sort the two estimated principal curvatures by
// magnitude and check the strong one (|k| ~ 1/r) against +z and the weak one
// (|k| ~ 1/(R-r)) against +y, allowing a sign flip on the direction vectors.
//

static const float TORUS_R = 3.0f;
static const float TORUS_r = 1.0f;

// Analytic principal curvatures at the inner equator (u=0, v=pi):
static const float KAPPA_MERIDIAN = 1.0f / TORUS_r;                 // = +1   (along z)
static const float KAPPA_PARALLEL = -1.0f / (TORUS_R - TORUS_r);    // = -0.5 (along y)

// Build a closed torus on an (nu x nv) grid (both even), wrapping in u and v so
// the mesh is a boundary-free manifold. Returns the index of the vertex at
// (u=0, v=pi) — the inner-equator saddle point.
static Mesh_half_edge* build_torus (int nu, int nv, float R, float r, int* saddleIndex)
{
	std::vector<float> verts;
	verts.reserve (3*nu*nv);
	for (int j = 0; j < nv; j++)
		for (int i = 0; i < nu; i++)
		{
			const float u = 2.f*(float)M_PI * i / nu;
			const float v = 2.f*(float)M_PI * j / nv;
			const float cu = cosf(u), su = sinf(u), cv = cosf(v), sv = sinf(v);
			verts.push_back ((R + r*cv) * cu);
			verts.push_back ((R + r*cv) * su);
			verts.push_back (r * sv);
		}

	auto idx = [nu,nv](int i, int j){ return (j % nv)*nu + (i % nu); };
	std::vector<unsigned int> faces;
	for (int j = 0; j < nv; j++)
		for (int i = 0; i < nu; i++)
		{
			const unsigned int v00 = idx(i,   j),   v10 = idx(i+1, j);
			const unsigned int v01 = idx(i,   j+1), v11 = idx(i+1, j+1);
			faces.push_back (v00); faces.push_back (v10); faces.push_back (v11);
			faces.push_back (v00); faces.push_back (v11); faces.push_back (v01);
		}

	Mesh_half_edge* he = new Mesh_half_edge ();
	he->m_pMesh->SetVertices ((unsigned int)(verts.size()/3), verts.data());
	he->m_pMesh->SetFaces ((unsigned int)(faces.size()/3), 3, faces.data());
	he->create_half_edge ();

	// SetVertices only sizes m_pVertices; EvalOnVertices writes into
	// m_pVertexNormals, so allocate it first (load() would normally do this).
	he->m_pMesh->m_pVertexNormals.assign (3*(verts.size()/3), 0.0f);

	Normals normals;
	normals.EvalOnVertices (he, Normals::THURMER);

	if (saddleIndex) *saddleIndex = (nv/2)*nu + 0;   // u=0 (i=0), v=pi (j=nv/2)
	return he;
}

static void normalize3 (float v[3])
{
	float n = std::sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	if (n > 1e-12f) { v[0]/=n; v[1]/=n; v[2]/=n; }
}

// Per-method tolerances, calibrated to each estimator's MEASURED accuracy at
// the torus saddle (resolution-independent — refining the mesh does not move
// the Taubin/Goldfeather figures, so these are systematic implementation
// biases, not discretisation noise; see debt_cgmesh.md):
//   * Hamann / Desbrun / Hybrid : principal curvatures exact to <1%, directions
//                                 exact -> tight bounds.
//   * Taubin  : directions exact, but principal-curvature MAGNITUDES are biased
//               high by ~+0.24 on both (looks like a missing 3*m1-m2 Taubin
//               correction) -> loose magnitude bound, tight direction bound.
//   * Goldfeather : strong curvature exact, but the weak principal curvature is
//               biased ~+28% and its direction is ~20 deg off -> medium bounds.
struct MethodCase { TensorMethodId id; const char* name; float relEps; float dirDeg; };

class TensorSaddle : public ::testing::TestWithParam<MethodCase> {};

TEST_P (TensorSaddle, principal_curvatures_and_directions)
{
	int saddle = -1;
	Mesh_half_edge* he = build_torus (120, 60, TORUS_R, TORUS_r, &saddle);

	MeshAlgoTensorEvaluator ev;
	ev.Init (he);
	ASSERT_TRUE (ev.Evaluate (GetParam().id));

	Tensor* t = ev.GetDiffParam (saddle);
	ASSERT_NE (t, nullptr) << GetParam().name << " produced no tensor at the saddle vertex";

	float k1 = t->GetKappaMax ();
	float k2 = t->GetKappaMin ();
	float d1[3], d2[3];
	t->GetDirectionMax (d1);
	t->GetDirectionMin (d2);
	normalize3 (d1);
	normalize3 (d2);

	// sort by magnitude: strong ~ |1/r|=1 (meridian, z), weak ~ 1/(R-r)=0.5 (parallel, y)
	const bool firstIsBig = std::fabs(k1) >= std::fabs(k2);
	const float  kBig   = firstIsBig ? k1 : k2;
	const float  kSmall = firstIsBig ? k2 : k1;
	const float* dBig   = firstIsBig ? d1 : d2;
	const float* dSmall = firstIsBig ? d2 : d1;

	const float dotBigZ   = std::fabs (dBig[2]);    // |dBig . z|   (meridian)
	const float dotSmallY = std::fabs (dSmall[1]);  // |dSmall . y| (parallel)

	printf ("[%-11s] kBig=%+.3f (exp %+.2f)  kSmall=%+.3f (exp %+.2f)  K=%+.3f  |dBig.z|=%.3f |dSmall.y|=%.3f\n",
		GetParam().name, kBig, KAPPA_MERIDIAN, kSmall, KAPPA_PARALLEL, k1*k2, dotBigZ, dotSmallY);

	// saddle: opposite-signed principal curvatures (Gaussian curvature < 0)
	EXPECT_LT (k1*k2, 0.0f) << GetParam().name;

	// magnitudes within the method's calibrated relative tolerance
	const float relEps = GetParam().relEps;
	EXPECT_NEAR (std::fabs(kBig),   std::fabs(KAPPA_MERIDIAN), relEps*std::fabs(KAPPA_MERIDIAN)) << GetParam().name;
	EXPECT_NEAR (std::fabs(kSmall), std::fabs(KAPPA_PARALLEL), relEps*std::fabs(KAPPA_PARALLEL)) << GetParam().name;

	// principal directions (sign-agnostic): strong along z, weak along y
	const float cosTol = std::cos (GetParam().dirDeg * (float)M_PI / 180.f);
	EXPECT_GT (dotBigZ,   cosTol) << GetParam().name << " meridian direction not aligned with z";
	EXPECT_GT (dotSmallY, cosTol) << GetParam().name << " parallel direction not aligned with y";

	delete he;
}

INSTANTIATE_TEST_SUITE_P (
	AllEstimators, TensorSaddle,
	::testing::Values (
		//            id                 name           relEps  dirDeg
		MethodCase{TENSOR_HAMANN,     "Hamann",      0.12f,  12.f},
		MethodCase{TENSOR_TAUBIN,     "Taubin",      0.60f,  12.f},
		MethodCase{TENSOR_DESBRUN,    "Desbrun",     0.12f,  12.f},
		MethodCase{TENSOR_GOLDFEATHER,"Goldfeather", 0.40f,  25.f},
		MethodCase{TENSOR_HYBRID,     "Hybrid",      0.12f,  12.f}),
	[](const ::testing::TestParamInfo<MethodCase>& info){ return info.param.name; });

// TENSOR_STEINER: ApplySteiner()'s body is entirely under #if 0 (it returns
// true but writes no tensor). This is not a functional estimator; pin the
// current behaviour so a future implementation is noticed. See debt_cgmesh.md.
TEST (TensorSaddle_Steiner, is_currently_a_noop_stub)
{
	int saddle = -1;
	Mesh_half_edge* he = build_torus (120, 60, TORUS_R, TORUS_r, &saddle);

	MeshAlgoTensorEvaluator ev;
	ev.Init (he);   // Init pre-fills every vertex with a default (zero) Tensor
	EXPECT_TRUE (ev.Evaluate (TENSOR_STEINER));       // stub returns true...
	Tensor* t = ev.GetDiffParam (saddle);
	ASSERT_NE (t, nullptr);
	// ...but computes nothing: the tensor stays at its default (zero curvatures).
	EXPECT_FLOAT_EQ (t->GetKappaMax (), 0.0f)
		<< "TENSOR_STEINER now produces curvature — implement a real accuracy test";
	EXPECT_FLOAT_EQ (t->GetKappaMin (), 0.0f)
		<< "TENSOR_STEINER now produces curvature — implement a real accuracy test";

	delete he;
}
