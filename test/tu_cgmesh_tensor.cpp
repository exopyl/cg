#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "../src/cgmesh/cgmesh.h"

//
// Principal-curvature / principal-direction estimators (DiffParamEvaluator_*)
// validated against an analytic parametric surface.
//
// ORACLE: ParametricTorus (src/cgmesh/surface_parametric.cpp). It computes its
// tensors ANALYTICALLY from the first and second fundamental forms, per vertex,
// and shares no code with the discrete estimators under test. Its own accuracy is
// established independently in tu_cgmesh_surface_parametric.cpp -- read that
// first: an oracle nobody checks is just a second opinion.
//
// The expected curvatures are therefore READ FROM THE ORACLE rather than
// hard-coded. Changing R, r or the grid no longer requires recomputing constants
// by hand, and the two sides can never drift apart.
//
// Surface: a CLOSED torus (major radius R, minor radius r), which the 1-ring
// estimators require (a boundary-free 2-manifold). ParametricTorus wraps in both
// u and v, so the mesh has no seam and no duplicated vertices.
//   p(u,v) = ((R + r cos v) cos u, (R + r cos v) sin u, r sin v)
// Principal curvatures (F = 0, so u/v ARE the principal axes):
//   - meridian (dp/dv): |kappa| = 1/r
//   - parallel (dp/du): |kappa| = |cos v| / (R + r cos v)
//
// We test at the INNER equator (u = 0, v = pi), i.e. the vertex (R-r, 0, 0),
// which is a genuine SADDLE point (Gaussian curvature < 0):
//   - meridian curvature, magnitude 1/r      , direction = +z
//   - parallel curvature, magnitude 1/(R-r)  , direction = +y
// With R=3, r=1 the two magnitudes are 1 and 1/2: opposite signs and a clean 2:1
// ratio, so the principal directions are unambiguous.
//
// (A true monkey saddle z = x^3 - 3xy^2 is unusable here: at its characteristic
// point the whole second fundamental form vanishes, so k1 = k2 = 0 and the
// principal directions are undefined.)
//
// The comparison is orientation-robust: we never assume the sign of the
// estimator's normal, nor of the oracle's. Both sides are sorted by MAGNITUDE,
// and direction vectors are compared up to a sign flip.
//
// DIRECTIONS ARE *NOT* TAKEN FROM THE ORACLE. ParametricSurface::EvaluateTensor()
// gets the principal curvatures right but the principal directions wrong: it
// reinterprets the (u,v)-space eigenvector as a world-space (x,y,0) vector and
// projects that onto the tangent plane, which at some vertices collapses to the
// zero vector. See torus_principal_directions_are_currently_wrong in
// tu_cgmesh_surface_parametric.cpp. The expected directions below (+z meridian,
// +y parallel at u=0) are the closed-form ones, which need no oracle.
//

static const float TORUS_R = 3.0f;
static const float TORUS_r = 1.0f;

// Build a closed torus from ParametricTorus and copy its geometry into a
// Mesh_half_edge, the structure the 1-ring estimators need. `oracle` keeps the
// ParametricTorus alive so its analytic tensors stay readable by the caller.
//
// Returns the index of the vertex at (u=0, v=pi) -- the inner-equator saddle.
// Generate() lays vertices out as index = v*nu + u with fV = v/nv, so v = pi is
// row nv/2. The caller asserts the resulting position, which pins that mapping.
static Mesh_half_edge* build_torus (unsigned int nu, unsigned int nv, float R, float r,
				    std::unique_ptr<ParametricTorus>& oracle, int* saddleIndex)
{
	oracle = std::make_unique<ParametricTorus> (nu, nv, R, r);
	if (!oracle->Generate ())
		return nullptr;

	const unsigned int nVerts = oracle->GetNVertices ();
	std::vector<float> verts (3 * (size_t)nVerts);
	for (unsigned int i = 0; i < nVerts; i++)
		oracle->GetVertex (i, &verts[3 * (size_t)i]);

	std::vector<unsigned int> faces;
	faces.reserve (3 * (size_t)oracle->GetNFaces ());
	for (unsigned int f = 0; f < oracle->GetNFaces (); f++)
	{
		if (oracle->GetFaceNVertices (f) != 3)
			continue;
		for (int k = 0; k < 3; k++)
			faces.push_back ((unsigned int)oracle->GetFaceVertex (f, k));
	}

	Mesh_half_edge* he = new Mesh_half_edge ();
	he->m_pMesh->SetVertices (nVerts, verts.data ());
	he->m_pMesh->SetFaces ((unsigned int)(faces.size () / 3), 3, faces.data ());
	he->create_half_edge ();

	// SetVertices only sizes m_pVertices; EvalOnVertices writes into
	// GetVertexNormals (), so allocate it first (load() would normally do this).
	he->m_pMesh->InitVertexNormals ();

	Normals normals;
	normals.EvalOnVertices (he, Normals::THURMER);

	if (saddleIndex) *saddleIndex = (int)((nv / 2) * nu);   // u=0, v=pi
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
	std::unique_ptr<ParametricTorus> oracle;
	Mesh_half_edge* he = build_torus (120, 60, TORUS_R, TORUS_r, oracle, &saddle);
	ASSERT_NE (he, nullptr);

	// L'index du sommet de selle vient d'une convention d'indexation interne a
	// Generate(). On la verifie plutot que de la supposer : (R-r, 0, 0).
	float P[3];
	ASSERT_GE (oracle->GetVertex ((unsigned int)saddle, P), 0);
	ASSERT_NEAR (P[0], TORUS_R - TORUS_r, 1e-4f) << "le sommet de selle n'est pas ou on croit";
	ASSERT_NEAR (P[1], 0.0f, 1e-4f);
	ASSERT_NEAR (P[2], 0.0f, 1e-4f);

	// Verite de terrain analytique, lue sur l'oracle au MEME sommet.
	Tensor* truth = oracle->GetTensor ((unsigned int)saddle);
	ASSERT_NE (truth, nullptr) << "l'oracle n'a pas de tenseur au sommet de selle";
	const float tA = truth->GetKappaMax (), tB = truth->GetKappaMin ();
	ASSERT_LT (tA * tB, 0.0f) << "l'oracle ne voit pas une selle : le test ne mesure plus rien";
	const bool truthFirstIsBig = std::fabs (tA) >= std::fabs (tB);
	const float truthBig   = truthFirstIsBig ? tA : tB;   // meridien,  |k| = 1/r
	const float truthSmall = truthFirstIsBig ? tB : tA;   // parallele, |k| = 1/(R-r)

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

	// sort by magnitude, same as the oracle above: strong = meridian (|1/r|, along
	// z), weak = parallel (1/(R-r), along y)
	const bool firstIsBig = std::fabs(k1) >= std::fabs(k2);
	const float  kBig   = firstIsBig ? k1 : k2;
	const float  kSmall = firstIsBig ? k2 : k1;
	const float* dBig   = firstIsBig ? d1 : d2;
	const float* dSmall = firstIsBig ? d2 : d1;

	const float dotBigZ   = std::fabs (dBig[2]);    // |dBig . z|   (meridian)
	const float dotSmallY = std::fabs (dSmall[1]);  // |dSmall . y| (parallel)

	printf ("[%-11s] |kBig|=%.3f (oracle %.3f)  |kSmall|=%.3f (oracle %.3f)  K=%+.3f  |dBig.z|=%.3f |dSmall.y|=%.3f\n",
		GetParam().name, std::fabs(kBig), std::fabs(truthBig),
		std::fabs(kSmall), std::fabs(truthSmall), k1*k2, dotBigZ, dotSmallY);

	// saddle: opposite-signed principal curvatures (Gaussian curvature < 0)
	EXPECT_LT (k1*k2, 0.0f) << GetParam().name;

	// magnitudes within the method's calibrated relative tolerance
	const float relEps = GetParam().relEps;
	EXPECT_NEAR (std::fabs(kBig),   std::fabs(truthBig),   relEps*std::fabs(truthBig))   << GetParam().name;
	EXPECT_NEAR (std::fabs(kSmall), std::fabs(truthSmall), relEps*std::fabs(truthSmall)) << GetParam().name;

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
	std::unique_ptr<ParametricTorus> oracle;
	Mesh_half_edge* he = build_torus (120, 60, TORUS_R, TORUS_r, oracle, &saddle);
	ASSERT_NE (he, nullptr);

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

// ============================================================================
//  Contrat de l'API de tenseurs de Mesh
// ============================================================================
//
// Le stockage est vertex-parallele : un emplacement par sommet, un emplacement
// nul signifiant « pas de tenseur ici » (sommet de bord ou non manifold). Les
// tests ci-dessous fixent les trois points de ce contrat qui ne se devinent pas
// a la lecture d'une signature.

namespace {

// Remplissage par reference : Mesh n'est ni copiable ni deplacable, donc un
// retour par valeur depuis une variable nommee ne compile pas.
void MakeTwoTriangles (Mesh &m)
{
	m.Init (4, 2);
	float v[12] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  1.f,1.f,0.f,  0.f,1.f,0.f };
	m.SetVertices (4, v);
	m.SetFace (0, 0, 1, 2);
	m.SetFace (1, 0, 2, 3);
}

} // namespace

TEST (TEST_cgmesh_tensor_api, init_gives_one_null_slot_per_vertex)
{
	Mesh m;
	MakeTwoTriangles (m);
	EXPECT_EQ (m.GetNTensors (), 0u) << "aucun tenseur avant InitTensors";

	m.InitTensors ();
	ASSERT_EQ (m.GetNTensors (), m.GetNVertices ());
	for (unsigned int i = 0; i < m.GetNTensors (); i++)
		EXPECT_EQ (m.GetTensor (i), nullptr) << "emplacement " << i;
}

// SetTensor PREND POSSESSION. Un indice hors bornes ne fait donc pas croitre le
// tableau -- ce qui casserait le parallelisme avec les sommets -- mais DETRUIT
// l'objet confie, faute de quoi l'appelant fuirait sans le savoir.
TEST (TEST_cgmesh_tensor_api, set_tensor_out_of_range_is_a_no_op)
{
	Mesh m;
	MakeTwoTriangles (m);
	m.InitTensors ();
	const unsigned int n = m.GetNTensors ();

	m.SetTensor (n + 100, new Tensor ());

	EXPECT_EQ (m.GetNTensors (), n) << "le tableau ne doit pas croitre";
	EXPECT_EQ (m.GetTensor (n + 100), nullptr);
}

TEST (TEST_cgmesh_tensor_api, set_tensor_replaces_the_slot)
{
	Mesh m;
	MakeTwoTriangles (m);
	m.InitTensors ();

	Tensor *t = new Tensor ();
	t->SetKappaMax (2.5f);
	m.SetTensor (1, t);

	ASSERT_NE (m.GetTensor (1), nullptr);
	EXPECT_FLOAT_EQ (m.GetTensor (1)->GetKappaMax (), 2.5f);
	EXPECT_EQ (m.GetTensor (0), nullptr) << "les autres emplacements sont intacts";

	// Remplacer par nullptr est licite : c'est ainsi que les estimateurs
	// marquent un sommet de bord.
	m.SetTensor (1, nullptr);
	EXPECT_EQ (m.GetTensor (1), nullptr);
}

// Le cache est estampille contre la revision de GEOMETRIE. Ecrire un tenseur ne
// la modifie pas : si c'etait le cas, le tampon ne concorderait jamais et le
// cache ne pourrait jamais etre valide.
TEST (TEST_cgmesh_tensor_api, writing_tensors_leaves_the_geometry_revision_alone)
{
	Mesh m;
	MakeTwoTriangles (m);
	const uint64_t revision = m.GetRevision ();

	m.InitTensors ();
	m.SetTensor (0, new Tensor ());
	m.MarkTensorsComputed ();
	EXPECT_EQ (m.GetRevision (), revision);
	EXPECT_TRUE (m.AreTensorsValid ());

	m.ClearTensors ();
	EXPECT_EQ (m.GetRevision (), revision);
	EXPECT_FALSE (m.AreTensorsValid ()) << "ClearTensors invalide le tampon";
}

// Muter la geometrie, en revanche, perime le cache.
TEST (TEST_cgmesh_tensor_api, a_geometry_edit_makes_the_tensors_stale)
{
	Mesh m;
	MakeTwoTriangles (m);
	m.InitTensors ();
	m.SetTensor (0, new Tensor ());
	m.MarkTensorsComputed ();
	ASSERT_TRUE (m.AreTensorsValid ());

	m.IncrementRevision ();
	EXPECT_FALSE (m.AreTensorsValid ());
}
