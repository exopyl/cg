// Tests du slicing multi-maillages (src/cgmesh/mesh_slicing.h). Conception,
// limites D1-D8 et plan de tests T1-T15 : docs/mesh_slicing.md.
//
// Toutes les fixtures sont construites EN MEMOIRE a partir des generateurs de
// cg (CreateCube, ParametricTorus, CreateOctahedron, CreateCylinder, texte
// extrude) : aucun fichier de donnees, hors les polices de test/data/fonts.
//
// Les tests marques "caracterisation" figent la valeur OBSERVEE d'une limite
// connue (D1-D8) : ils sont faits pour echouer le jour ou cette limite sera
// levee, et devront alors etre revus.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../src/cgmath/font.h"
#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/extrude_contours.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_slicing.h"
#include "../src/cgmesh/surface_basic.h"
#include "../src/cgmesh/surface_parametric.h"
#include "../src/cgmesh/text_extrude.h"

using namespace cgmesh::slicing;

namespace
{

const double kPi = 3.14159265358979323846;

// --- fixtures ----------------------------------------------------------------

// [x0, x0+sx] x [y0, y0+sy] x [z0, z0+sz], triangule, oriente vers l'exterieur.
std::unique_ptr<Mesh> unitCube (float x0, float y0, float z0, float sx, float sy, float sz)
{
	std::unique_ptr<Mesh> m (CreateCube (true));   // [-1, 1]^3
	m->scale_xyz (0.5f * sx, 0.5f * sy, 0.5f * sz);
	m->translate (x0 + 0.5f * sx, y0 + 0.5f * sy, z0 + 0.5f * sz);
	return m;
}

// Tore couche dans le plan XY, z dans [dz - r, dz + r].
std::unique_ptr<Mesh> lyingTorus (unsigned int nu, unsigned int nv, float R, float r, float dz)
{
	std::unique_ptr<Mesh> m (new ParametricTorus (nu, nv, R, r));
	m->translate (0.f, 0.f, dz);
	return m;
}

std::unique_ptr<Mesh> octahedronAt (float cx, float cy, float cz)
{
	std::unique_ptr<Mesh> m (CreateOctahedron ());
	m->translate (cx, cy, cz);
	return m;
}

std::unique_ptr<Mesh> meshFrom (const std::vector<float>& v, const std::vector<unsigned int>& f,
                                unsigned int perFace = 3)
{
	std::unique_ptr<Mesh> m (new Mesh ());
	m->SetVertices ((unsigned int)(v.size () / 3), v.data ());
	std::vector<unsigned int> faces (f);
	m->SetFaces ((unsigned int)(faces.size () / perFace), perFace, faces.data ());
	return m;
}

// Prisme "coin" : base [0,1]^2 en z = 0, face haute [0,0.5]x[0,1] en z = 1,
// face inclinee de x = 1 (z = 0) a x = 0.5 (z = 1). Section : 1 - z/2. Six
// quadrangles, la face inclinee est la face d'indice 5.
std::unique_ptr<Mesh> wedge ()
{
	const std::vector<float> v = {
		0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0,
		0, 0, 1,   0.5f, 0, 1,   0.5f, 1, 1,   0, 1, 1 };
	const std::vector<unsigned int> f = {
		0, 3, 2, 1,     // z = 0
		4, 5, 6, 7,     // z = 1
		0, 4, 7, 3,     // x = 0
		0, 1, 5, 4,     // y = 0
		3, 7, 6, 2,     // y = 1
		1, 2, 6, 5 };   // inclinee
	return meshFrom (v, f, 4);
}

void appendTo (Mesh& dst, std::unique_ptr<Mesh> src)
{
	dst.Append (src.get ());
}

// Sommets transformes par M (pour l'oracle O3).
void applyMatrix (Mesh& m, const Matrix4f& M)
{
	std::vector<float> v = m.GetVertices ();
	for (size_t i = 0; i + 2 < v.size (); i += 3)
	{
		const double x = v[i], y = v[i + 1], z = v[i + 2];
		float o[3];
		for (int r = 0; r < 3; ++r)
			o[r] = (float)(M.at (r, 0) * x + M.at (r, 1) * y + M.at (r, 2) * z + M.at (r, 3));
		v[i] = o[0]; v[i + 1] = o[1]; v[i + 2] = o[2];
	}
	m.SetVertices (m.GetNVertices (), v.data ());
}

Matrix4f scaleMatrix (float sx, float sy, float sz)
{
	Matrix4f m;
	m.at (0, 0) = sx; m.at (1, 1) = sy; m.at (2, 2) = sz;
	return m;
}

Matrix4f translationMatrix (float tx, float ty, float tz)
{
	Matrix4f m;
	m.at (0, 3) = tx; m.at (1, 3) = ty; m.at (2, 3) = tz;
	return m;
}

// --- oracles -------------------------------------------------------------------

// Theoreme de la divergence : positif pour un maillage ferme oriente dehors.
double signedVolume (const Mesh& m)
{
	const std::vector<unsigned int> t = m.GetTriangles ();
	const std::vector<float>& v = m.GetVertices ();
	double vol = 0.;
	for (size_t i = 0; i + 2 < t.size (); i += 3)
	{
		const float* a = &v[3 * t[i]];
		const float* b = &v[3 * t[i + 1]];
		const float* c = &v[3 * t[i + 2]];
		vol += (double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1])
		     - (double)a[1] * ((double)b[0] * c[2] - (double)b[2] * c[0])
		     + (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0]);
	}
	return vol / 6.;
}

double regularPolygonArea (int n, double rho)
{
	return 0.5 * n * rho * rho * std::sin (2. * kPi / n);
}

double areaD (const std::vector<Vector2f>& pts)
{
	double s = 0.;
	const size_t n = pts.size ();
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2f& a = pts[i];
		const Vector2f& b = pts[(i + 1) % n];
		s += (double)a.x * b.y - (double)b.x * a.y;
	}
	return 0.5 * s;
}

double regionArea (const SliceRegion& r)
{
	double a = 0.;
	for (const ExtrudeContour& c : r.contours)
		a += areaD (c.pts);
	return a;
}

double layerArea (const SliceLayer& l)
{
	double a = 0.;
	for (const SliceRegion& r : l)
		a += regionArea (r);
	return a;
}

size_t holeCount (const SliceLayer& l)
{
	size_t n = 0;
	for (const SliceRegion& r : l)
		n += r.contours.size () - 1;
	return n;
}

// O4 : enveloppe positive (isHole faux), trous negatifs (isHole vrai).
void expectO4 (const SliceLayer& l, const char* what)
{
	for (const SliceRegion& r : l)
	{
		ASSERT_FALSE (r.contours.empty ()) << what;
		EXPECT_FALSE (r.contours[0].isHole) << what;
		EXPECT_GT (areaD (r.contours[0].pts), 0.) << what;
		for (size_t k = 1; k < r.contours.size (); ++k)
		{
			EXPECT_TRUE (r.contours[k].isHole) << what;
			EXPECT_LT (areaD (r.contours[k].pts), 0.) << what;
		}
	}
}

SliceLayer sliceOne (const Mesh& m, float z, Nesting nesting = Nesting::Containment,
                     const Matrix4f& M = Matrix4f (), const Mesh* hollowing = nullptr)
{
	SliceInput in;
	in.mesh = &m;
	in.matrix = M;
	in.hollowing = hollowing;
	SliceOptions opt;
	opt.nesting = nesting;
	opt.threads = 1;
	const std::vector<SliceLayer> out = sliceMeshes ({ in }, { z }, opt);
	return out.empty () ? SliceLayer () : out[0];
}

// Les sommets de `pts` sont-ils, a permutation pres, ceux de `expected` ?
bool sameVertexSet (const std::vector<Vector2f>& pts, const std::vector<Vector2f>& expected,
                    float tol = 1e-4f)
{
	if (pts.size () != expected.size ()) return false;
	for (const Vector2f& e : expected)
	{
		bool hit = false;
		for (const Vector2f& p : pts)
			if (std::fabs (p.x - e.x) <= tol && std::fabs (p.y - e.y) <= tol) { hit = true; break; }
		if (!hit) return false;
	}
	return true;
}

void topology (const Mesh& m, size_t& nonManifold, size_t& borders)
{
	std::vector<unsigned int> nm, b;
	m.GetTopologicIssues (nm, b);
	nonManifold = nm.size ();
	borders = b.size ();
}

} // namespace

namespace
{
const char* kFontPath = "./test/data/fonts/DejaVuSans.ttf";

// Texte extrude de z = 0 a z = depth (T11), soude pour etre ferme.
std::unique_ptr<Mesh> textSolid (const std::string& text, float depth,
                                 std::vector<ExtrudeContour>& contours)
{
	Font font;
	if (!font.loadFromFile (kFontPath)) return nullptr;
	TextExtrudeOptions opt;
	opt.size = 10.f;
	opt.depth = depth;
	contours.clear ();
	if (!text_to_contours (font, text, opt, contours)) return nullptr;
	ExtrudedMeshBuilder b;
	ExtrudeAppendOptions ao;
	ao.zBottom = 0.f;
	ao.zTop = depth;
	if (!b.Append (contours, ao)) return nullptr;
	std::unique_ptr<Mesh> m (b.Build ());
	if (m) m->MergeVertices ();
	return m;
}
} // namespace

// --- fixtures -----------------------------------------------------------------

TEST (TEST_mesh_slicing, fixtures_are_closed_and_outward)
{
	struct Case { const char* name; std::unique_ptr<Mesh> mesh; double volume; };
	std::vector<Case> cases;
	cases.push_back ({ "unitCube", unitCube (0, 0, 0, 1, 1, 1), 1. });
	cases.push_back ({ "unitCube scaled", unitCube (1, 2, 3, 2, 3, 4), 24. });
	cases.push_back ({ "octahedron", octahedronAt (0, 0, 0), 4. / 3. });
	cases.push_back ({ "wedge", wedge (), 0.75 });
	{
		std::unique_ptr<Mesh> cyl (CreateCylinder (5.5f, 0.5f, 12, true, true));
		cases.push_back ({ "cylinder", std::move (cyl), regularPolygonArea (12, 0.5) * 5.5 });
	}
	{
		// volume du tore discretise : pas de formule fermee simple, on
		// verifie le signe et l'ordre de grandeur (2 pi^2 R r^2)
		cases.push_back ({ "torus", lyingTorus (20, 20, 5.f, 2.f, 2.f), -1. });
	}
	for (Case& c : cases)
	{
		ASSERT_TRUE (c.mesh) << c.name;
		const double vol = signedVolume (*c.mesh);
		EXPECT_GT (vol, 0.) << c.name << " : orientation interieure";
		if (c.volume > 0.)
			EXPECT_NEAR (vol, c.volume, 1e-4 * c.volume) << c.name;
		else
			EXPECT_NEAR (vol, 2. * kPi * kPi * 5. * 4., 0.1 * 2. * kPi * kPi * 5. * 4.) << c.name;
		size_t nm = 0, borders = 0;
		topology (*c.mesh, nm, borders);
		EXPECT_EQ (nm, 0u) << c.name;
		EXPECT_EQ (borders, 0u) << c.name;
	}

	std::vector<ExtrudeContour> contours;
	std::unique_ptr<Mesh> text = textSolid ("8oB@", 2.f, contours);
	ASSERT_TRUE (text) << "police de test introuvable : " << kFontPath;
	double inputArea = 0.;
	for (const ExtrudeContour& c : contours) inputArea += areaD (c.pts);
	EXPECT_GT (signedVolume (*text), 0.);
	EXPECT_NEAR (signedVolume (*text), 2. * inputArea, 1e-3 * 2. * inputArea);
	size_t nm = 0, borders = 0;
	topology (*text, nm, borders);
	EXPECT_EQ (nm, 0u);
	EXPECT_EQ (borders, 0u);
}

// --- T15 : intersection et chemins ------------------------------------------

TEST (TEST_mesh_slicing, T15_cube_segments_and_path)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	const PlateMesh pm = buildPlateMesh (*cube, Matrix4f ());
	EXPECT_EQ (pm.triangleCount (), 12);
	EXPECT_FALSE (pm.mirrored);

	std::vector<SliceSegment> segs;
	intersectAtZ (pm, 0.5f, segs);
	EXPECT_EQ (segs.size (), 8u);   // deux triangles par face laterale

	SliceLinker linker (segs);
	EXPECT_TRUE (linker.isManifoldOnIndices ());
	linker.execute ();
	std::vector<std::vector<Vector3f>> paths;
	std::vector<std::vector<int>> faces;
	traceChains (segs, paths, faces);
	ASSERT_EQ (paths.size (), 1u);
	EXPECT_EQ (paths[0].size (), 9u);     // 8 segments, fermeture repetee
	EXPECT_EQ (faces[0].size (), 8u);
	EXPECT_FLOAT_EQ (paths[0].front ().x, paths[0].back ().x);
	EXPECT_FLOAT_EQ (paths[0].front ().y, paths[0].back ().y);

	std::vector<SliceLoop> loops;
	sliceLoopsAtZ (pm, 0.5f, loops);
	ASSERT_EQ (loops.size (), 1u);
	// sortie brute : enveloppe dans le sens horaire
	EXPECT_LT (contourSignedAreaD (loops[0].pts), 0.);
	cleanContour (loops[0].pts, 0.01, 0.001);
	EXPECT_EQ (loops[0].pts.size (), 4u);
	EXPECT_NEAR (std::fabs (contourSignedAreaD (loops[0].pts)), 1., 1e-6);
}

TEST (TEST_mesh_slicing, T15_single_triangle_vertex_on_plane)
{
	// nOn == 1 : un sommet dans le plan, les deux autres de part et d'autre
	{
		auto tri = meshFrom ({ 0, 0, 0,   1, 0, -1,   1, 1, 1 }, { 0, 1, 2 });
		const PlateMesh pm = buildPlateMesh (*tri, Matrix4f ());
		std::vector<SliceSegment> segs;
		intersectAtZ (pm, 0.f, segs);
		ASSERT_EQ (segs.size (), 1u);
		int degenerate = 0;
		for (int k = 0; k < 2; ++k)
			if (segs[0].edges[k][0] == segs[0].edges[k][1]) degenerate++;
		EXPECT_EQ (degenerate, 1);
	}
	// nOn == 2 : une arete dans le plan. Une seule entree (bord du maillage) :
	// gardee.
	{
		auto tri = meshFrom ({ 0, 0, 0,   1, 0, 0,   0, 1, 1 }, { 0, 1, 2 });
		const PlateMesh pm = buildPlateMesh (*tri, Matrix4f ());
		std::vector<SliceSegment> segs;
		intersectAtZ (pm, 0.f, segs);
		ASSERT_EQ (segs.size (), 1u);
		EXPECT_EQ (segs[0].edges[0][0], segs[0].edges[0][1]);
		EXPECT_EQ (segs[0].edges[1][0], segs[0].edges[1][1]);
		EXPECT_NEAR (std::sqrt (segs[0].length2 ()), 1.f, 1e-6f);
	}
	// nOn == 1 avec les deux autres sommets du meme cote : rien
	{
		auto tri = meshFrom ({ 0, 0, 0,   1, 0, 1,   0, 1, 1 }, { 0, 1, 2 });
		const PlateMesh pm = buildPlateMesh (*tri, Matrix4f ());
		std::vector<SliceSegment> segs;
		intersectAtZ (pm, 0.f, segs);
		EXPECT_TRUE (segs.empty ());
	}
}

// --- T1 : plan confondu avec la face superieure --------------------------------

// La face du haut (nOn == 3, normale +Z) depose des pseudo-entrees "dessus",
// opposees aux murs "dessous" : chaque arete haute est gardee par decision,
// d'ou un carre de quatre points -- resultat voulu par la convention solide
// ferme.
TEST (TEST_mesh_slicing, T1_plane_on_top_face)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	const SliceLayer l = sliceOne (*cube, 1.f);
	ASSERT_EQ (l.size (), 1u);
	ASSERT_EQ (l[0].contours.size (), 1u);
	EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
	EXPECT_NEAR (regionArea (l[0]), 1., 1e-6);
	expectO4 (l, "T1");
}

// --- T2 : deux cubes partageant une arete verticale ----------------------------

TEST (TEST_mesh_slicing, T2_two_cubes_sharing_an_edge)
{
	Mesh m;
	appendTo (m, unitCube (0, 0, 0, 1, 1, 1));
	appendTo (m, unitCube (1, 1, 0, 1, 1, 1));
	m.MergeVertices ();
	ASSERT_EQ (m.GetNVertices (), 14u);   // l'arete x = y = 1 est commune

	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		const SliceLayer l = sliceOne (m, 0.5f, nesting);
		if (nesting == Nesting::Winding)
		{
			// l'union NonZero fond deux carres qui se touchent en un point ?
			// Clipper2 les rend en UNE ou DEUX regions selon sa convention ;
			// seule l'aire est un oracle ici.
			EXPECT_NEAR (layerArea (l), 2., 1e-5);
			continue;
		}
		ASSERT_EQ (l.size (), 2u);
		std::vector<Vector2f> a = { Vector2f (0, 0), Vector2f (1, 0), Vector2f (1, 1), Vector2f (0, 1) };
		std::vector<Vector2f> b = { Vector2f (1, 1), Vector2f (2, 1), Vector2f (2, 2), Vector2f (1, 2) };
		int found = 0;
		for (const SliceRegion& r : l)
		{
			ASSERT_EQ (r.contours.size (), 1u);
			if (sameVertexSet (r.contours[0].pts, a) || sameVertexSet (r.contours[0].pts, b)) found++;
			EXPECT_NEAR (regionArea (r), 1., 1e-5);
		}
		EXPECT_EQ (found, 2);
		expectO4 (l, "T2");
	}
}

// --- T3 : evidement --------------------------------------------------------------

TEST (TEST_mesh_slicing, T3_cube_with_hollowing)
{
	auto outer = unitCube (0, 0, 0, 1, 1, 1);
	auto inner = unitCube (0.2f, 0.2f, 0.2f, 0.6f, 0.6f, 0.6f);   // MEME orientation
	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		const SliceLayer l = sliceOne (*outer, 0.5f, nesting, Matrix4f (), inner.get ());
		ASSERT_EQ (l.size (), 1u);
		ASSERT_EQ (l[0].contours.size (), 2u);
		EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
		EXPECT_EQ (l[0].contours[1].pts.size (), 4u);
		EXPECT_NEAR (regionArea (l[0]), 0.64, 1e-5);
		EXPECT_FALSE (l[0].faceIds.empty ());
		expectO4 (l, "T3");
	}
}

// --- T4 : transformations ------------------------------------------------------

TEST (TEST_mesh_slicing, T4_three_transformed_instances)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	Matrix4f rot;
	rot.SetRotateZ ((float)(kPi / 4.));
	const Matrix4f mats[3] = { rot, translationMatrix (1, 2, 0), scaleMatrix (2, 3, 4) };

	std::vector<SliceInput> inputs (3);
	for (int i = 0; i < 3; ++i) { inputs[i].mesh = cube.get (); inputs[i].matrix = mats[i]; }
	const std::vector<SliceLayer> out = sliceMeshes (inputs, { 0.5f });
	ASSERT_EQ (out.size (), 1u);
	const SliceLayer& l = out[0];
	ASSERT_EQ (l.size (), 3u);
	expectO4 (l, "T4");

	const float h = (float)(std::sqrt (2.) / 2.);
	const std::vector<Vector2f> expected[3] = {
		{ Vector2f (0, 0), Vector2f (h, h), Vector2f (0, 2 * h), Vector2f (-h, h) },
		{ Vector2f (1, 2), Vector2f (2, 2), Vector2f (2, 3), Vector2f (1, 3) },
		{ Vector2f (0, 0), Vector2f (2, 0), Vector2f (2, 3), Vector2f (0, 3) } };
	const double areas[3] = { 1., 1., 6. };
	for (int i = 0; i < 3; ++i)
	{
		ASSERT_EQ (l[i].sourceMesh, i);
		ASSERT_EQ (l[i].contours.size (), 1u);
		EXPECT_TRUE (sameVertexSet (l[i].contours[0].pts, expected[i])) << "instance " << i;
		EXPECT_NEAR (regionArea (l[i]), areas[i], 1e-5) << "instance " << i;

		// O3 : (mesh, M) == (mesh transforme par M, I)
		auto pre = unitCube (0, 0, 0, 1, 1, 1);
		applyMatrix (*pre, mats[i]);
		const SliceLayer p = sliceOne (*pre, 0.5f);
		ASSERT_EQ (p.size (), 1u);
		EXPECT_NEAR (regionArea (p[0]), regionArea (l[i]), 1e-6) << "O3, instance " << i;
		EXPECT_TRUE (sameVertexSet (p[0].contours[0].pts, l[i].contours[0].pts, 1e-6f)) << "O3, instance " << i;
	}
}

// --- T5 : echelle puis rotation autour de Y --------------------------------------

TEST (TEST_mesh_slicing, T5_scale_then_rotate_around_y)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	Matrix4f rot;
	rot.SetRotateY ((float)(kPi / 4.));
	const Matrix4f M = rot * scaleMatrix (2, 1, 1);

	const SliceLayer l = sliceOne (*cube, 0.5f, Nesting::Containment, M);
	ASSERT_EQ (l.size (), 1u);
	ASSERT_EQ (l[0].contours.size (), 1u);
	// Section du pave [0,2]x[0,1]x[0,1] tourne de 45 deg : pres du coin haut
	// (s, c), largeur 2 (sqrt(2)/2 - 0.5), sur toute la profondeur y = 1.
	EXPECT_NEAR (regionArea (l[0]), std::sqrt (2.) - 1., 1e-5);
	expectO4 (l, "T5");

	auto pre = unitCube (0, 0, 0, 1, 1, 1);
	applyMatrix (*pre, M);
	const SliceLayer p = sliceOne (*pre, 0.5f);
	ASSERT_EQ (p.size (), 1u);
	EXPECT_NEAR (regionArea (p[0]), regionArea (l[0]), 1e-6);
}

// --- T5b : matrice miroir (D4) -------------------------------------------------

// Repere plateau et retournement des triangles sous det(M) < 0 (D4) : l'aire
// et l'orientation de sortie sont celles de l'objet non symetrise, dans les
// deux modes, et la boucle BRUTE garde le sens horaire.
TEST (TEST_mesh_slicing, T5b_mirror_matrix_keeps_orientation)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	const Matrix4f M = scaleMatrix (-1, 1, 1);

	const PlateMesh pm = buildPlateMesh (*cube, M);
	EXPECT_TRUE (pm.mirrored);
	std::vector<SliceLoop> loops;
	sliceLoopsAtZ (pm, 0.5f, loops);
	ASSERT_EQ (loops.size (), 1u);
	EXPECT_LT (contourSignedAreaD (loops[0].pts), 0.) << "D4 : la boucle s'est inversee";

	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		const SliceLayer l = sliceOne (*cube, 0.5f, nesting, M);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_NEAR (regionArea (l[0]), 1., 1e-5);
		std::vector<Vector2f> e = { Vector2f (-1, 0), Vector2f (0, 0), Vector2f (0, 1), Vector2f (-1, 1) };
		EXPECT_TRUE (sameVertexSet (l[0].contours[0].pts, e));
		expectO4 (l, "T5b");
	}

	// Un objet miroir ne s'annule pas avec son voisin en NonZero : le cube et
	// son symetrique cote a cote font deux fois l'aire.
	std::vector<SliceInput> inputs (2);
	inputs[0].mesh = cube.get ();
	inputs[1].mesh = cube.get ();
	inputs[1].matrix = M;
	SliceOptions opt;
	opt.nesting = Nesting::Winding;
	const std::vector<SliceLayer> out = sliceMeshes (inputs, { 0.5f }, opt);
	ASSERT_EQ (out.size (), 1u);
	EXPECT_NEAR (layerArea (out[0]), 2., 1e-5);
}

// --- T6 : deux octaedres qui se touchent par un sommet -------------------------

TEST (TEST_mesh_slicing, T6_vertex_non_manifold)
{
	Mesh m;
	appendTo (m, octahedronAt (0, 0, 0));
	appendTo (m, octahedronAt (2, 0, 0));
	m.MergeVertices ();
	ASSERT_EQ (m.GetNVertices (), 11u);   // le sommet (1,0,0) est commun

	// z = 0,5 : aucun sommet dans le plan
	{
		const SliceLayer l = sliceOne (m, 0.5f);
		ASSERT_EQ (l.size (), 2u);
		for (const SliceRegion& r : l)
			EXPECT_NEAR (regionArea (r), 0.5, 1e-5);
		expectO4 (l, "T6 z=0.5");
	}
	// z = 0 : le plan passe par l'equateur des deux, donc par le sommet commun
	{
		const SliceLayer l = sliceOne (m, 0.f);
		ASSERT_EQ (l.size (), 2u);
		for (const SliceRegion& r : l)
		{
			EXPECT_EQ (r.contours.size (), 1u);
			EXPECT_NEAR (regionArea (r), 2., 1e-5);
		}
		expectO4 (l, "T6 z=0");
	}
}

// --- T7 : minima locaux ------------------------------------------------------------

TEST (TEST_mesh_slicing, T7_octahedra_on_their_tips)
{
	// pointes basses en z = 0 ; 0,25 ; 0,75 ; 1,25 (valeurs exactes en float)
	const float tips[4] = { 0.f, 0.25f, 0.75f, 1.25f };
	Mesh m;
	for (int i = 0; i < 4; ++i)
		appendTo (m, octahedronAt (3.f * i, 0.f, tips[i] + 1.f));

	const std::vector<float> zs = { 0.1f, 0.5f, 1.1f, 1.5f, 0.75f };
	const size_t expected[5] = { 1, 2, 3, 4, 2 };
	SliceInput in;
	in.mesh = &m;
	const std::vector<SliceLayer> out = sliceMeshes ({ in }, zs);
	ASSERT_EQ (out.size (), zs.size ());
	for (size_t k = 0; k < zs.size (); ++k)
	{
		// z = 0,75 : plan tangent a la pointe du troisieme octaedre, qui ne
		// rend aucune region (plan tangent exact, D5)
		EXPECT_EQ (out[k].size (), expected[k]) << "z = " << zs[k];
		expectO4 (out[k], "T7");
	}
}

// --- T8 : tore couche ---------------------------------------------------------------

TEST (TEST_mesh_slicing, T8_lying_torus)
{
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);   // z dans [0, 4]

	// z = 2 : les anneaux v = 0 (rayon 7) et v = 10 (rayon 3) sont dans le plan
	{
		const SliceLayer l = sliceOne (*torus, 2.f);
		ASSERT_EQ (l.size (), 1u);
		ASSERT_EQ (l[0].contours.size (), 2u);
		EXPECT_EQ (l[0].contours[0].pts.size (), 20u);
		EXPECT_EQ (l[0].contours[1].pts.size (), 20u);
		EXPECT_NEAR (areaD (l[0].contours[0].pts), regularPolygonArea (20, 7.), 1e-3);
		EXPECT_NEAR (-areaD (l[0].contours[1].pts), regularPolygonArea (20, 3.), 1e-3);
		expectO4 (l, "T8 z=2");
	}
	// z = 1,5 : aucun sommet dans le plan
	{
		const SliceLayer l = sliceOne (*torus, 1.5f);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_EQ (l[0].contours.size (), 2u);
		expectO4 (l, "T8 z=1.5");
	}
	// z = 0 et z = 4 : plan tangent exact (D5). Les sommets de l'anneau
	// tangent sont EXACTEMENT dans le plan, et chaque arete de l'anneau a deux
	// entrees (nOn == 2) du MEME cote : elle est retiree avant emission, et le
	// plan ne rend aucune region.
	for (const float z : { 0.f, 4.f })
	{
		const SliceLayer l = sliceOne (*torus, z);
		SCOPED_TRACE (z);
		EXPECT_EQ (l.size (), 0u);
	}

	// O1 : conservation du volume, dz = h / 200, plans au milieu des tranches
	const int n = 200;
	const double h = 4., dz = h / n;
	std::vector<float> zs;
	for (int k = 0; k < n; ++k)
		zs.push_back ((float)((k + 0.5) * dz));
	SliceInput in;
	in.mesh = torus.get ();
	// Un seul thread : le multi-thread n'est pas l'objet de ce test (O2 le
	// couvre), et en Debug le tas CRT le rend PLUS lent : 1,45 s par defaut
	// dans la suite complete, 0,1 s sur un thread (mesures Debug MSVC).
	SliceOptions one;
	one.threads = 1;
	const std::vector<SliceLayer> out = sliceMeshes ({ in }, zs, one);
	double vol = 0.;
	for (const SliceLayer& l : out)
		vol += layerArea (l) * dz;
	const double ref = signedVolume (*torus);
	EXPECT_NEAR (vol, ref, 0.01 * ref);
}

// --- T9 : supports ------------------------------------------------------------------

TEST (TEST_mesh_slicing, T9_cube_with_supports)
{
	auto cube = unitCube (0, 0, 5, 10, 10, 5);
	Mesh supports;
	const float at[4][2] = { { 2, 2 }, { 8, 2 }, { 2, 8 }, { 8, 8 } };
	for (int i = 0; i < 4; ++i)
	{
		std::unique_ptr<Mesh> c (CreateCylinder (5.5f, 0.5f, 12, true, true));
		c->translate (at[i][0], at[i][1], 0.f);
		appendTo (supports, std::move (c));
	}
	std::vector<SliceInput> inputs (2);
	inputs[0].mesh = cube.get ();
	inputs[1].mesh = &supports;
	inputs[1].isSupport = true;

	const std::vector<float> zs = { 1.f, 4.5f, 5.25f, 7.f };
	const std::vector<SliceLayer> out = sliceMeshes (inputs, zs);
	ASSERT_EQ (out.size (), zs.size ());
	const double supportArea = regularPolygonArea (12, 0.5);

	for (int k = 0; k < 2; ++k)   // sous le cube : quatre supports
	{
		ASSERT_EQ (out[k].size (), 4u);
		for (const SliceRegion& r : out[k])
		{
			EXPECT_TRUE (r.isSupport);
			EXPECT_EQ (r.sourceMesh, 1);
			EXPECT_NEAR (regionArea (r), supportArea, 1e-5);
		}
	}
	// penetration : le cube et les supports coexistent, sans fusion
	{
		const SliceLayer& l = out[2];
		ASSERT_EQ (l.size (), 5u);
		int nSupports = 0;
		for (const SliceRegion& r : l)
		{
			if (r.isSupport) { nSupports++; EXPECT_NEAR (regionArea (r), supportArea, 1e-5); }
			else { EXPECT_EQ (r.sourceMesh, 0); EXPECT_NEAR (regionArea (r), 100., 1e-3); }
		}
		EXPECT_EQ (nSupports, 4);
	}
	ASSERT_EQ (out[3].size (), 1u);
	EXPECT_FALSE (out[3][0].isSupport);
	EXPECT_NEAR (regionArea (out[3][0]), 100., 1e-3);
	for (const SliceLayer& l : out) expectO4 (l, "T9");
}

// --- T10 : face manquante -------------------------------------------------------

TEST (TEST_mesh_slicing, T10_wedge_with_missing_face)
{
	auto w = wedge ();
	w->RemoveFace (5);   // la face inclinee

	// Caracterisation (face manquante). L'aire n'est PAS 1 - z/2 : le contour
	// ouvert par la face retiree n'est pas referme, pour deux raisons :
	//   - la breche mesure 1 (de y = 0 a y = 1), et le comblement du linker ne
	//     relie que des extremites a moins de 0,005 ;
	//   - traceChains s'arrete AVANT un segment dont l'extremite lointaine
	//     n'est pas chainee : sur une chaine ouverte, les deux segments de bout
	//     ressortent comme deux chemins de deux points, ecartes ensuite comme
	//     contours degeneres.
	// Observe pour z = 0,1 ... 0,9 : UNE region, un quadrilatere d'aire z/2 --
	// la chaine tronquee de ses deux bouts, refermee implicitement.
	for (int i = 1; i <= 9; ++i)
	{
		const float z = 0.1f * i;
		const SliceLayer l = sliceOne (*w, z);
		SCOPED_TRACE (z);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_EQ (l[0].contours.size (), 1u);
		EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
		EXPECT_NEAR (layerArea (l), z / 2., 1e-5);
		expectO4 (l, "T10");
	}
	// z = 0 : base coplanaire. La base (nOn = 3, normale -Z) depose des
	// pseudo-entrees "dessous", opposees aux trois faces laterales "dessus" :
	// leurs aretes basses sont gardees. L'arete de la face retiree n'a que la
	// pseudo-entree de la base : rien n'est emis. Chaine ouverte de trois
	// segments, donc trois chemins de deux points (traceChains), tous ecartes
	// comme contours degeneres : la couche est VIDE.
	{
		const SliceLayer l = sliceOne (*w, 0.f);
		EXPECT_TRUE (l.empty ());
	}
}

// --- T11 : texte extrude (nombreux trous) -------------------------------------

TEST (TEST_mesh_slicing, T11_engraved_text)
{
	std::vector<ExtrudeContour> contours;
	std::unique_ptr<Mesh> text = textSolid ("8oB@", 2.f, contours);
	ASSERT_TRUE (text) << "police de test introuvable : " << kFontPath;

	// Trous par glyphe, comptes EXPLICITES (et non relus sur l'entree, ce qui
	// serait tautologique pour le mode Winding, fonde sur la meme union) :
	// 8 -> 2, o -> 1, B -> 2 ; @ -> kAtHoles, releve sur DejaVuSans et fige :
	// UN seul trou (la panse du "a"), l'espace entre l'anneau et le "a"
	// restant ouvert sur l'exterieur dans ce dessin. Total : 6 trous.
	const size_t kAtHoles = 1;
	const struct { const char* glyph; size_t holes; } glyphs[4] = {
		{ "8", 2 }, { "o", 1 }, { "B", 2 }, { "@", kAtHoles } };
	size_t expectedHoles = 0;
	for (const auto& g : glyphs)
	{
		std::vector<ExtrudeContour> gc;
		ASSERT_TRUE (textSolid (g.glyph, 2.f, gc));
		size_t holes = 0, hulls = 0;
		for (const ExtrudeContour& c : gc) (areaD (c.pts) < 0. ? holes : hulls)++;
		EXPECT_EQ (holes, g.holes) << "glyphe " << g.glyph;
		EXPECT_EQ (hulls, 1u) << "glyphe " << g.glyph << " : aucun ilot attendu";
		expectedHoles += g.holes;
	}
	// quatre glyphes disjoints, sans ilot : quatre regions
	const size_t expectedRegions = 4;

	double inputArea = 0.;
	for (const ExtrudeContour& c : contours)
		inputArea += areaD (c.pts);

	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		SCOPED_TRACE (nesting == Nesting::Containment ? "Containment" : "Winding");
		const SliceLayer l = sliceOne (*text, 1.f, nesting);
		EXPECT_NEAR (layerArea (l), inputArea, 1e-3 * inputArea);
		EXPECT_EQ (holeCount (l), expectedHoles);
		EXPECT_EQ (l.size (), expectedRegions);
		expectO4 (l, "T11");
	}
}

// --- T12 : solide imbrique (D7) -----------------------------------------------

TEST (TEST_mesh_slicing, T12_nested_solid)
{
	Mesh m;
	appendTo (m, unitCube (0, 0, 0, 3, 3, 3));
	appendTo (m, unitCube (1, 1, 1, 1, 1, 1));   // MEME orientation : plein

	// Containment : le cube interieur devient un trou. Caracterisation de D7.
	{
		const SliceLayer l = sliceOne (m, 1.5f, Nesting::Containment);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_EQ (l[0].contours.size (), 2u);
		EXPECT_NEAR (layerArea (l), 8., 1e-5);
	}
	// Winding : l'enroulement le garde plein
	{
		const SliceLayer l = sliceOne (m, 1.5f, Nesting::Winding);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_EQ (l[0].contours.size (), 1u);
		EXPECT_NEAR (layerArea (l), 9., 1e-5);
		expectO4 (l, "T12");
	}
}

// --- T14 : linker sur segments synthetiques ------------------------------------

namespace
{
// Carre (0,0)-(1,1) en quatre segments orientes ; le segment k porte ses
// extremites sur les "aretes" (k, 10 + k) et (k + 1, 11 + k).
std::vector<SliceSegment> squareSegments ()
{
	const float p[5][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 } };
	std::vector<SliceSegment> s (4);
	for (int k = 0; k < 4; ++k)
	{
		s[k].faceId = k;
		s[k].points[0] = Vector3f (p[k][0], p[k][1], 0.f);
		s[k].points[1] = Vector3f (p[k + 1][0], p[k + 1][1], 0.f);
		s[k].edges[0][0] = k;           s[k].edges[0][1] = 10 + k;
		s[k].edges[1][0] = (k + 1) % 4; s[k].edges[1][1] = 10 + (k + 1) % 4;
	}
	return s;
}

// Chaine et rend le nombre de chemins, et pour le premier : points distincts
// et fermeture.
void linkAndCheck (std::vector<SliceSegment>& s, const char* what)
{
	SliceLinker linker (s);
	linker.execute ();
	std::vector<std::vector<Vector3f>> paths;
	std::vector<std::vector<int>> faces;
	traceChains (s, paths, faces);
	ASSERT_EQ (paths.size (), 1u) << what;
	ASSERT_EQ (paths[0].size (), 5u) << what;
	EXPECT_FLOAT_EQ (paths[0].front ().x, paths[0].back ().x) << what;
	EXPECT_FLOAT_EQ (paths[0].front ().y, paths[0].back ().y) << what;
	Contour2d c;
	for (const Vector3f& v : paths[0]) c.push_back (Vector2d (v.x, v.y));
	cleanContour (c);
	EXPECT_EQ (c.size (), 4u) << what;
	EXPECT_NEAR (std::fabs (contourSignedAreaD (c)), 1., 1e-9) << what;
}
} // namespace

TEST (TEST_mesh_slicing, T14_linker_square)
{
	std::vector<SliceSegment> s = squareSegments ();
	{
		SliceLinker probe (s);
		EXPECT_TRUE (probe.isManifoldOnIndices ());
		EXPECT_TRUE (probe.checkManifold ());
	}
	linkAndCheck (s, "carre");
}

TEST (TEST_mesh_slicing, T14_linker_duplicated_edge)
{
	std::vector<SliceSegment> s = squareSegments ();
	s.push_back (s[0]);   // segment en double
	{
		SliceLinker probe (s);
		EXPECT_FALSE (probe.isManifoldOnIndices ());
	}
	linkAndCheck (s, "arete dupliquee");
}

TEST (TEST_mesh_slicing, T14_linker_topological_holes)
{
	// Une jonction a la fois, puis les quatre : les cles d'arete ne
	// correspondent plus, seuls les points coincident -- le comblement par
	// proximite doit refermer.
	for (int junction = 0; junction <= 4; ++junction)
	{
		std::vector<SliceSegment> s = squareSegments ();
		for (int k = 0; k < 4; ++k)
		{
			if (junction < 4 && k != junction) continue;
			const int next = (k + 1) % 4;
			s[next].edges[0][0] = 100 + k;
			s[next].edges[0][1] = 200 + k;
		}
		{
			SliceLinker probe (s);
			EXPECT_FALSE (probe.isManifoldOnIndices ());
		}
		const std::string what = "trou topologique " + std::to_string (junction);
		linkAndCheck (s, what.c_str ());
	}
}

// --- T13 : reparations sur defauts synthetiques --------------------------------

namespace
{
struct Soup
{
	std::vector<float> v;
	std::vector<unsigned int> f;
};

Soup soupOf (const Mesh& m)
{
	Soup s;
	s.v = m.GetVertices ();
	s.f = m.GetTriangles ();
	return s;
}

unsigned int addVertexCopy (Soup& s, unsigned int i)
{
	const unsigned int n = (unsigned int)(s.v.size () / 3);
	s.v.push_back (s.v[3 * i]);
	s.v.push_back (s.v[3 * i + 1]);
	s.v.push_back (s.v[3 * i + 2]);
	return n;
}

struct Observation
{
	bool finished = false;
	std::string error;          // message de l'exception, le cas echeant
	size_t regions = 0;
	double area = 0.;
	size_t openLoops = 0;
};

// Tranche sous delai de garde : une boucle infinie du linker ferait echouer le
// test au lieu de bloquer la suite. En cas de depassement, le thread est
// abandonne (detache) ; il garde le maillage en vie par son shared_ptr.
Observation observe (std::shared_ptr<const Mesh> mesh, float z, int seconds = 20)
{
	auto result = std::make_shared<Observation> ();
	auto done = std::make_shared<std::promise<void>> ();
	std::future<void> f = done->get_future ();
	std::thread ([mesh, z, result, done] () {
		try
		{
			const Mesh& m = *mesh;
			const PlateMesh pm = buildPlateMesh (m, Matrix4f ());
			std::vector<SliceLoop> loops;
			sliceLoopsAtZ (pm, z, loops);
			for (const SliceLoop& l : loops)
			{
				if (l.pts.size () < 2) { result->openLoops++; continue; }
				const double dx = l.pts.front ().x - l.pts.back ().x;
				const double dy = l.pts.front ().y - l.pts.back ().y;
				if (dx * dx + dy * dy > 1e-10) result->openLoops++;
			}
			const SliceLayer layer = sliceOne (m, z);
			result->regions = layer.size ();
			result->area = layerArea (layer);
			result->finished = true;
		}
		catch (const std::exception& e) { result->error = e.what (); }
		catch (...) { result->error = "exception non standard"; }
		done->set_value ();
	}).detach ();
	if (f.wait_for (std::chrono::seconds (seconds)) != std::future_status::ready)
	{
		Observation timeout;
		timeout.error = "delai de garde depasse";
		return timeout;
	}
	return *result;
}
} // namespace

TEST (TEST_mesh_slicing, T13_repairs_on_synthetic_defects)
{
	const unsigned int nu = 20;
	auto torus = lyingTorus (nu, 20, 5.f, 2.f, 2.f);
	const Soup base = soupOf (*torus);
	const float zs[2] = { 2.f, 2.3f };
	double healthy[2];
	for (int k = 0; k < 2; ++k)
		healthy[k] = layerArea (sliceOne (*torus, zs[k]));

	struct Defect { std::string name; std::shared_ptr<Mesh> mesh; double factor; };
	std::vector<Defect> defects;

	{   // (a) face dupliquee
		Soup s = base;
		s.f.insert (s.f.end (), { s.f[0], s.f[1], s.f[2] });
		defects.push_back ({ "a_duplicated_face", meshFrom (s.v, s.f), 1. });
	}
	{   // (b) face retournee
		Soup s = base;
		std::swap (s.f[1], s.f[2]);
		defects.push_back ({ "b_flipped_face", meshFrom (s.v, s.f), 1. });
	}
	{   // (c) couture non soudee sur la colonne u = 0
		Soup s = base;
		const unsigned int nv0 = (unsigned int)(s.v.size () / 3);
		std::vector<int> copyOf (nv0, -1);
		for (size_t t = 0; t + 2 < s.f.size (); t += 3)
		{
			bool hasFirst = false, hasLast = false;
			for (int k = 0; k < 3; ++k)
			{
				hasFirst = hasFirst || (s.f[t + k] % nu == 0);
				hasLast = hasLast || (s.f[t + k] % nu == nu - 1);
			}
			if (!(hasFirst && hasLast)) continue;
			for (int k = 0; k < 3; ++k)
			{
				const unsigned int i = s.f[t + k];
				if (i % nu != 0) continue;
				if (copyOf[i] < 0) copyOf[i] = (int)addVertexCopy (s, i);
				s.f[t + k] = (unsigned int)copyOf[i];
			}
		}
		defects.push_back ({ "c_unwelded_seam", meshFrom (s.v, s.f), 1. });
	}
	{   // (d) sommet pince : deux tores soudes en un point (7, 0, 2)
		Soup s = base;
		auto other = lyingTorus (nu, 20, 5.f, 2.f, 2.f);
		other->translate (14.f, 0.f, 0.f);
		const Soup o = soupOf (*other);
		const unsigned int shift = (unsigned int)(s.v.size () / 3);
		s.v.insert (s.v.end (), o.v.begin (), o.v.end ());
		const unsigned int pinchA = 0;          // u = 0, v = 0 : (7, 0, 2)
		const unsigned int pinchB = nu / 2;     // u = 10, v = 0 : (-7, 0, 2) + 14
		for (const unsigned int i : o.f)
			s.f.push_back (i == pinchB ? pinchA : i + shift);
		defects.push_back ({ "d_pinched_vertex", meshFrom (s.v, s.f), 2. });
	}
	{   // (e) arete de longueur nulle : le sommet (u = 0, v = 0) dedouble
		Soup s = base;
		const unsigned int P = 0, A = nu - 1, B = 1;
		const unsigned int Pp = addVertexCopy (s, P);
		for (size_t t = 0; t + 2 < s.f.size (); t += 3)
		{
			bool hasP = false, above = false;
			for (int k = 0; k < 3; ++k)
			{
				hasP = hasP || s.f[t + k] == P;
				above = above || (s.f[t + k] >= nu && s.f[t + k] < 2 * nu);
			}
			if (!(hasP && above)) continue;
			for (int k = 0; k < 3; ++k)
				if (s.f[t + k] == P) s.f[t + k] = Pp;
		}
		s.f.insert (s.f.end (), { P, A, Pp, Pp, B, P });
		defects.push_back ({ "e_zero_length_edge", meshFrom (s.v, s.f), 1. });
	}

	for (const Defect& d : defects)
	{
		for (int k = 0; k < 2; ++k)
		{
			SCOPED_TRACE (d.name + " z=" + std::to_string (zs[k]));
			const Observation o = observe (d.mesh, zs[k]);
			ASSERT_TRUE (o.finished) << o.error;
			// Observe : les cinq defauts sont TOUS absorbes, aux deux altitudes
			// (rapport 1,000000, aucune boucle ouverte). Le cas (a) a z = 2
			// depend de l'emission INVALIDE d'une arete a trois entrees (face
			// dupliquee) : sans elle, l'anneau s'ouvre (2 boucles ouvertes,
			// 2 regions).
			EXPECT_EQ (o.openLoops, 0u);
			EXPECT_EQ (o.regions, (size_t)d.factor);
			EXPECT_NEAR (o.area / (d.factor * healthy[k]), 1., 0.02);
		}
	}
}

// --- O2 : determinisme ----------------------------------------------------------

namespace
{
void expectSameLayers (const std::vector<SliceLayer>& a, const std::vector<SliceLayer>& b,
                       const char* what)
{
	ASSERT_EQ (a.size (), b.size ()) << what;
	for (size_t k = 0; k < a.size (); ++k)
	{
		ASSERT_EQ (a[k].size (), b[k].size ()) << what << " couche " << k;
		for (size_t r = 0; r < a[k].size (); ++r)
		{
			const SliceRegion& ra = a[k][r];
			const SliceRegion& rb = b[k][r];
			EXPECT_EQ (ra.sourceMesh, rb.sourceMesh) << what;
			EXPECT_EQ (ra.faceIds, rb.faceIds) << what;
			EXPECT_EQ (ra.hollowFaceIds, rb.hollowFaceIds) << what;
			EXPECT_NEAR (regionArea (ra), regionArea (rb), 1e-9) << what;
			ASSERT_EQ (ra.contours.size (), rb.contours.size ()) << what;
			for (size_t c = 0; c < ra.contours.size (); ++c)
			{
				ASSERT_EQ (ra.contours[c].pts.size (), rb.contours[c].pts.size ()) << what;
				for (size_t i = 0; i < ra.contours[c].pts.size (); ++i)
				{
					EXPECT_EQ (ra.contours[c].pts[i].x, rb.contours[c].pts[i].x) << what;
					EXPECT_EQ (ra.contours[c].pts[i].y, rb.contours[c].pts[i].y) << what;
				}
			}
		}
	}
}
} // namespace

TEST (TEST_mesh_slicing, O2_determinism_threads_runs_and_plane_order)
{
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);
	auto outer = unitCube (0, 0, 0, 1, 1, 1);
	auto inner = unitCube (0.2f, 0.2f, 0.2f, 0.6f, 0.6f, 0.6f);
	std::vector<SliceInput> inputs (2);
	inputs[0].mesh = torus.get ();
	inputs[1].mesh = outer.get ();
	inputs[1].hollowing = inner.get ();
	inputs[1].matrix = translationMatrix (0.5f, 0.5f, 0.f);

	std::vector<float> zs;
	for (int k = 0; k <= 80; ++k) zs.push_back (0.05f * k);   // sommets dans le plan compris

	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		SliceOptions one, many, dflt;
		one.nesting = many.nesting = dflt.nesting = nesting;
		one.threads = 1;
		many.threads = 7;
		const std::vector<SliceLayer> a = sliceMeshes (inputs, zs, one);
		const std::vector<SliceLayer> b = sliceMeshes (inputs, zs, many);
		const std::vector<SliceLayer> c = sliceMeshes (inputs, zs, dflt);
		const std::vector<SliceLayer> d = sliceMeshes (inputs, zs, many);
		expectSameLayers (a, b, "1 vs 7 threads");
		expectSameLayers (a, c, "1 vs defaut");
		expectSameLayers (b, d, "deux executions");

		// ordre des plans : plans inverses, sortie inversee
		std::vector<float> rev (zs.rbegin (), zs.rend ());
		std::vector<SliceLayer> e = sliceMeshes (inputs, rev, many);
		std::reverse (e.begin (), e.end ());
		expectSameLayers (a, e, "ordre des plans");
	}
}

// Le balayage de sliceMeshes et le filtrage direct de sliceLoopsAtZ voient les
// memes triangles, dans le meme ordre.
TEST (TEST_mesh_slicing, sweep_matches_direct_filtering)
{
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);
	const PlateMesh pm = buildPlateMesh (*torus, Matrix4f ());
	std::vector<float> zs;
	for (int k = 0; k <= 40; ++k) zs.push_back (0.1f * k);
	SliceInput in;
	in.mesh = torus.get ();
	const std::vector<SliceLayer> swept = sliceMeshes ({ in }, zs);
	std::vector<SliceLayer> direct;
	for (const float z : zs)
	{
		std::vector<SliceLoop> loops;
		sliceLoopsAtZ (pm, z, loops);
		direct.push_back (buildRegions (loops, 0, false, false, Nesting::Containment));
	}
	expectSameLayers (swept, direct, "balayage vs filtrage");
}

TEST (TEST_mesh_slicing, empty_inputs_and_progress)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	SliceInput in;
	in.mesh = cube.get ();

	int calls = 0;
	const SliceProgress count = [&] (int) { calls++; };
	EXPECT_TRUE (sliceMeshes ({ in }, {}, SliceOptions (), count).empty ());
	EXPECT_EQ (calls, 0);

	// entree sans maillage : ignoree
	SliceInput none;
	const std::vector<SliceLayer> empty = sliceMeshes ({ none }, { 0.5f });
	ASSERT_EQ (empty.size (), 1u);
	EXPECT_TRUE (empty[0].empty ());

	std::vector<float> zs;
	for (int k = 0; k < 250; ++k) zs.push_back (0.004f * k + 0.001f);
	std::vector<int> seen;
	SliceOptions opt;
	opt.threads = 4;
	const std::vector<SliceLayer> out = sliceMeshes ({ in, in }, zs, opt,
	                                                 [&] (int p) { seen.push_back (p); });
	ASSERT_EQ (out.size (), zs.size ());
	ASSERT_FALSE (seen.empty ());
	EXPECT_TRUE (std::is_sorted (seen.begin (), seen.end ()));
	EXPECT_EQ (seen.back (), 100);
	for (const SliceLayer& l : out) EXPECT_EQ (l.size (), 2u);
}

// --- fonctions libres ---------------------------------------------------------

TEST (TEST_mesh_slicing, contour_helpers)
{
	// fermeture repetee et points proches
	Contour2d c = { Vector2d (0, 0), Vector2d (1, 0), Vector2d (1, 0.0001), Vector2d (1, 1),
	                Vector2d (0, 1), Vector2d (0, 0) };
	removeClosePoints (c, 0.001);
	EXPECT_EQ (c.size (), 4u);

	// angle plat
	Contour2d flat = { Vector2d (0, 0), Vector2d (0.5, 0), Vector2d (1, 0), Vector2d (1, 1), Vector2d (0, 1) };
	removeFlatAngle (flat, 0.001);
	EXPECT_EQ (flat.size (), 4u);
	EXPECT_TRUE (isClockwise ({ Vector2d (0, 0), Vector2d (0, 1), Vector2d (1, 1), Vector2d (1, 0) }));
	EXPECT_FALSE (isClockwise ({ Vector2d (0, 0), Vector2d (1, 0), Vector2d (1, 1), Vector2d (0, 1) }));

	// huit : deux lobes qui se touchent au point (1, 1)
	const Contour2d eight = { Vector2d (0, 0), Vector2d (1, 0), Vector2d (1, 1), Vector2d (2, 1),
	                          Vector2d (2, 2), Vector2d (1, 2), Vector2d (1, 1), Vector2d (0, 1) };
	std::vector<Contour2d> pieces;
	ASSERT_TRUE (splitAtRepeatedPoints (eight, pieces, 1e-9));
	ASSERT_EQ (pieces.size (), 2u);
	EXPECT_NEAR (std::fabs (contourSignedAreaD (pieces[0])) + std::fabs (contourSignedAreaD (pieces[1])), 2., 1e-12);
	EXPECT_FALSE (splitAtRepeatedPoints (flat, pieces));

	// imbrication : enveloppe, trou, ilot dans le trou, et un voisin
	const auto square = [] (double x0, double y0, double s) {
		return Contour2d { Vector2d (x0, y0), Vector2d (x0 + s, y0), Vector2d (x0 + s, y0 + s), Vector2d (x0, y0 + s) };
	};
	const std::vector<Contour2d> soup = { square (1, 1, 8), square (0, 0, 10), square (4, 4, 2), square (20, 0, 1) };
	const std::vector<NestedContour> nested = nestContours (soup);
	ASSERT_EQ (nested.size (), 3u);
	int withHole = 0;
	for (const NestedContour& n : nested)
	{
		if (n.outer == 1) { ASSERT_EQ (n.holes.size (), 1u); EXPECT_EQ (n.holes[0], 0); withHole++; }
		else EXPECT_TRUE (n.holes.empty ());
	}
	EXPECT_EQ (withHole, 1);
}

// --- provenance des faces ---------------------------------------------------------

// Evidement a PLUS de faces que le principal : un indice de face de
// l'evidement verse dans faceIds depasserait les faces du cube.
TEST (TEST_mesh_slicing, T3b_hollowing_face_provenance)
{
	auto outer = unitCube (0, 0, 0, 1, 1, 1);
	std::unique_ptr<Mesh> hollow (CreateCylinder (0.6f, 0.3f, 64, true, true));
	hollow->translate (0.5f, 0.5f, 0.2f);
	const unsigned int nMain = outer->GetNFaces ();
	const unsigned int nHollow = hollow->GetNFaces ();
	ASSERT_GT (nHollow, nMain);

	for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
	{
		SCOPED_TRACE (nesting == Nesting::Containment ? "Containment" : "Winding");
		const SliceLayer l = sliceOne (*outer, 0.5f, nesting, Matrix4f (), hollow.get ());
		ASSERT_EQ (l.size (), 1u);
		ASSERT_EQ (l[0].contours.size (), 2u);
		EXPECT_NEAR (regionArea (l[0]), 1. - regularPolygonArea (64, 0.3), 1e-4);
		EXPECT_FALSE (l[0].faceIds.empty ());
		EXPECT_FALSE (l[0].hollowFaceIds.empty ());
		for (const int f : l[0].faceIds)
			EXPECT_TRUE (f >= 0 && (unsigned int)f < nMain) << f;
		for (const int f : l[0].hollowFaceIds)
			EXPECT_TRUE (f >= 0 && (unsigned int)f < nHollow) << f;
		// les faces du cylindre coupees a mi-hauteur sont ses 128 triangles
		// lateraux (indices 0..127, deux par cote)
		for (const int f : l[0].hollowFaceIds)
			EXPECT_LT (f, 128);
	}
}

// Mode Winding : le rattachement apres coup rend a chaque region les faces de
// SON cube, et d'aucun autre.
TEST (TEST_mesh_slicing, winding_face_attachment_two_cubes)
{
	Mesh m;
	appendTo (m, unitCube (0, 0, 0, 1, 1, 1));   // faces 0..11
	appendTo (m, unitCube (3, 0, 0, 1, 1, 1));   // faces 12..23
	const SliceLayer l = sliceOne (m, 0.5f, Nesting::Winding);
	ASSERT_EQ (l.size (), 2u);
	for (const SliceRegion& r : l)
	{
		ASSERT_FALSE (r.faceIds.empty ());
		EXPECT_TRUE (r.hollowFaceIds.empty ());
		float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
		ASSERT_TRUE (contoursBBox (r.contours, x0, y0, x1, y1));
		const bool first = x1 < 2.f;
		for (const int f : r.faceIds)
			EXPECT_TRUE (first ? (f >= 0 && f < 12) : (f >= 12 && f < 24)) << f;
	}
}

// --- altitudes non finies ----------------------------------------------------------

TEST (TEST_mesh_slicing, non_finite_altitudes_give_empty_layers)
{
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);
	SliceInput in;
	in.mesh = torus.get ();
	const float nan = std::numeric_limits<float>::quiet_NaN ();
	const float inf = std::numeric_limits<float>::infinity ();
	for (const unsigned int threads : { 1u, 3u })
	{
		SliceOptions opt;
		opt.threads = threads;
		const std::vector<SliceLayer> ref = sliceMeshes ({ in }, { 2.5f, 1.2f }, opt);
		const std::vector<SliceLayer> out = sliceMeshes ({ in }, { 2.5f, nan, 1.2f, -inf }, opt);
		ASSERT_EQ (out.size (), 4u);
		EXPECT_TRUE (out[1].empty ());
		EXPECT_TRUE (out[3].empty ());
		expectSameLayers ({ out[0], out[2] }, ref, "couches finies");
	}
	int last = -1;
	const std::vector<SliceLayer> none = sliceMeshes ({ in }, { nan }, SliceOptions (),
	                                                  [&] (int p) { last = p; });
	ASSERT_EQ (none.size (), 1u);
	EXPECT_TRUE (none[0].empty ());
	EXPECT_EQ (last, 100);
}

// --- contourRegions (contour_ops) ----------------------------------------------

namespace
{
ExtrudeContour squareContour (float x0, float y0, float s, bool ccw)
{
	ExtrudeContour c;
	c.pts = { Vector2f (x0, y0), Vector2f (x0 + s, y0), Vector2f (x0 + s, y0 + s), Vector2f (x0, y0 + s) };
	if (!ccw) std::reverse (c.pts.begin (), c.pts.end ());
	return c;
}

void expectRegionConvention (const std::vector<std::vector<ExtrudeContour>>& regions)
{
	for (const std::vector<ExtrudeContour>& r : regions)
	{
		ASSERT_FALSE (r.empty ());
		EXPECT_FALSE (r[0].isHole);
		EXPECT_GT (contourSignedArea (r[0].pts), 0.f);
		for (size_t k = 1; k < r.size (); ++k)
		{
			EXPECT_TRUE (r[k].isHole);
			EXPECT_LT (contourSignedArea (r[k].pts), 0.f);
		}
	}
}
} // namespace

TEST (TEST_mesh_slicing, contour_regions_nonzero_and_evenodd)
{
	EXPECT_TRUE (contourRegions ({}).empty ());

	// orientations alternees : enveloppe, trou, ilot dans le trou
	const std::vector<ExtrudeContour> alternating = {
		squareContour (0, 0, 10, true), squareContour (1, 1, 8, false), squareContour (4, 4, 2, true) };
	for (const bool evenOdd : { false, true })
	{
		SCOPED_TRACE (evenOdd ? "EvenOdd" : "NonZero");
		const auto regions = contourRegions (alternating, evenOdd);
		ASSERT_EQ (regions.size (), 2u);   // l'ilot est une region a part
		expectRegionConvention (regions);
		size_t withHole = 0;
		double area = 0.;
		for (const auto& r : regions)
		{
			if (r.size () == 2) withHole++;
			for (const ExtrudeContour& c : r) area += contourSignedArea (c.pts);
		}
		EXPECT_EQ (withHole, 1u);
		EXPECT_NEAR (area, 100. - 64. + 4., 1e-4);
	}

	// meme orientation partout : NonZero fond tout, EvenOdd alterne
	const std::vector<ExtrudeContour> same = {
		squareContour (0, 0, 10, true), squareContour (1, 1, 8, true), squareContour (4, 4, 2, true) };
	{
		const auto regions = contourRegions (same, false);
		ASSERT_EQ (regions.size (), 1u);
		EXPECT_EQ (regions[0].size (), 1u);
		EXPECT_NEAR (contourSignedArea (regions[0][0].pts), 100.f, 1e-3f);
		expectRegionConvention (regions);
	}
	{
		const auto regions = contourRegions (same, true);
		ASSERT_EQ (regions.size (), 2u);
		expectRegionConvention (regions);
	}

	// entree en sens horaire : la sortie reste enveloppe > 0
	{
		const auto regions = contourRegions ({ squareContour (0, 0, 1, false) }, false);
		ASSERT_EQ (regions.size (), 1u);
		expectRegionConvention (regions);
	}
}

// --- aretes contenues dans le plan (convention solide ferme) ------------------

namespace
{
// Marche : profil en L dans (x, z) -- (0,0) (2,0) (2,1) (1,1) (1,2) (0,2), sens
// trigonometrique --, extrude sur y dans [0, 1]. Volume 3. Le palier est la
// face z = 1, x dans [1, 2], de normale +Z. Tout en triangles, pour que l'ordre
// des faces puisse etre permute librement.
Soup stepSoup ()
{
	const float prof[6][2] = { { 0, 0 }, { 2, 0 }, { 2, 1 }, { 1, 1 }, { 1, 2 }, { 0, 2 } };
	Soup s;
	for (int side = 0; side < 2; ++side)          // sommets 0..5 : y = 0, 6..11 : y = 1
		for (int i = 0; i < 6; ++i)
			s.v.insert (s.v.end (), { prof[i][0], (float)side, prof[i][1] });
	// flancs y = 0 (normale -Y) et y = 1 (+Y) : eventail depuis (0,0), valide
	// pour ce L (le sommet (0,0) voit tout le profil)
	const unsigned int fan[4][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 4 }, { 0, 4, 5 } };
	for (const auto& t : fan)
	{
		s.f.insert (s.f.end (), { t[0], t[1], t[2] });
		s.f.insert (s.f.end (), { 6 + t[0], 6 + t[2], 6 + t[1] });
	}
	// murs : pour l'arete i -> i+1 du profil, quad (a0, a1, b1, b0)
	for (unsigned int i = 0; i < 6; ++i)
	{
		const unsigned int a0 = i, b0 = (i + 1) % 6, a1 = 6 + a0, b1 = 6 + b0;
		s.f.insert (s.f.end (), { a0, a1, b1, a0, b1, b0 });
	}
	return s;
}

// Meme soupe, triangles dans un autre ordre (permutation deterministe).
Soup permuted (Soup s, unsigned int stride)
{
	const size_t nt = s.f.size () / 3;
	std::vector<unsigned int> f;
	f.reserve (s.f.size ());
	for (size_t k = 0; k < nt; ++k)
	{
		const size_t t = (k * stride + 1) % nt;   // stride premier avec nt
		f.insert (f.end (), { s.f[3 * t], s.f[3 * t + 1], s.f[3 * t + 2] });
	}
	s.f = f;
	return s;
}

// Segments bruts du plan z : variete sur indices ? sommets non-varietes ?
void rawManifold (const Mesh& m, float z, bool& onIndices, unsigned int& nonManifoldVertices,
                  unsigned int& borders)
{
	const PlateMesh pm = buildPlateMesh (m, Matrix4f ());
	std::vector<SliceSegment> segs;
	intersectAtZ (pm, z, segs);
	SliceLinker linker (segs);
	onIndices = linker.isManifoldOnIndices ();
	unsigned int edges = 0;
	linker.checkManifold (&edges, &borders, &nonManifoldVertices);
}

// Sortie BRUTE (sliceLoopsAtZ) : une seule boucle, dans le sens horaire (aire
// signee < 0), comme en T15.
void expectRawClockwise (const Mesh& m, float z)
{
	const PlateMesh pm = buildPlateMesh (m, Matrix4f ());
	std::vector<SliceLoop> loops;
	sliceLoopsAtZ (pm, z, loops);
	ASSERT_EQ (loops.size (), 1u);
	EXPECT_LT (contourSignedAreaD (loops[0].pts), 0.);
}

const Nesting kBothModes[2] = { Nesting::Containment, Nesting::Winding };
const char* modeName (Nesting n) { return n == Nesting::Containment ? "Containment" : "Winding"; }
} // namespace

// Non-regression de la convention solide ferme sur le cas le plus simple : les
// sections haute et basse du cube sont fermees, et la couche brute est variete.
TEST (TEST_mesh_slicing, cube_top_and_bottom_faces_close_the_section)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	for (const float z : { 0.f, 1.f })
	{
		SCOPED_TRACE (z);
		bool onIndices = false;
		unsigned int nmv = 99, borders = 99;
		rawManifold (*cube, z, onIndices, nmv, borders);
		EXPECT_TRUE (onIndices);
		EXPECT_EQ (nmv, 0u);
		EXPECT_EQ (borders, 0u);
		expectRawClockwise (*cube, z);
		for (const Nesting nesting : kBothModes)
		{
			SCOPED_TRACE (modeName (nesting));
			const SliceLayer l = sliceOne (*cube, z, nesting);
			ASSERT_EQ (l.size (), 1u);
			ASSERT_EQ (l[0].contours.size (), 1u);
			EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
			EXPECT_NEAR (regionArea (l[0]), 1., 1e-6);
			expectO4 (l, "cube");
		}
	}
}

TEST (TEST_mesh_slicing, step_landing_in_the_plane)
{
	const Soup s = stepSoup ();
	auto step = meshFrom (s.v, s.f);
	EXPECT_NEAR (signedVolume (*step), 3., 1e-6);
	size_t nm = 0, b = 0;
	topology (*step, nm, b);
	EXPECT_EQ (nm, 0u);
	EXPECT_EQ (b, 0u);

	// palier dans le plan : section fermee [0,2] x [0,1], sans reparation
	{
		bool onIndices = false;
		unsigned int nmv = 99, borders = 99;
		rawManifold (*step, 1.f, onIndices, nmv, borders);
		EXPECT_TRUE (onIndices);
		EXPECT_EQ (nmv, 0u);
		EXPECT_EQ (borders, 0u);
		expectRawClockwise (*step, 1.f);
		for (const Nesting nesting : kBothModes)
		{
			SCOPED_TRACE (modeName (nesting));
			const SliceLayer l = sliceOne (*step, 1.f, nesting);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_TRUE (sameVertexSet (l[0].contours[0].pts,
			                            { Vector2f (0, 0), Vector2f (2, 0), Vector2f (2, 1), Vector2f (0, 1) }));
			EXPECT_NEAR (regionArea (l[0]), 2., 1e-6);
			expectO4 (l, "marche z=1");
		}
	}
	const float zs[4] = { 0.f, 0.5f, 1.5f, 2.f };
	const double areas[4] = { 2., 2., 1., 1. };
	for (int k = 0; k < 4; ++k)
	{
		SCOPED_TRACE (zs[k]);
		expectRawClockwise (*step, zs[k]);
		for (const Nesting nesting : kBothModes)
		{
			SCOPED_TRACE (modeName (nesting));
			const SliceLayer l = sliceOne (*step, zs[k], nesting);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_NEAR (layerArea (l), areas[k], 1e-6);
			expectO4 (l, "marche");
		}
	}
}

// Marche tete en bas (rotation de 180 deg autour de X, puis relevee de 2) :
// le palier, a z = 1, regarde vers le BAS.
TEST (TEST_mesh_slicing, upside_down_step_landing_facing_down)
{
	const Soup s = stepSoup ();
	auto step = meshFrom (s.v, s.f);
	Matrix4f M = scaleMatrix (1, -1, -1);
	M.at (1, 3) = 1.f;
	M.at (2, 3) = 2.f;
	applyMatrix (*step, M);
	EXPECT_NEAR (signedVolume (*step), 3., 1e-6);   // rotation : orientation gardee

	bool onIndices = false;
	unsigned int nmv = 99, borders = 99;
	rawManifold (*step, 1.f, onIndices, nmv, borders);
	EXPECT_TRUE (onIndices);
	EXPECT_EQ (nmv, 0u);
	EXPECT_EQ (borders, 0u);

	const float zs[5] = { 0.f, 0.5f, 1.f, 1.5f, 2.f };
	const double areas[5] = { 1., 1., 2., 2., 2. };
	for (int k = 0; k < 5; ++k)
	{
		SCOPED_TRACE (zs[k]);
		expectRawClockwise (*step, zs[k]);
		for (const Nesting nesting : kBothModes)
		{
			SCOPED_TRACE (modeName (nesting));
			const SliceLayer l = sliceOne (*step, zs[k], nesting);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_NEAR (layerArea (l), areas[k], 1e-6);
			expectO4 (l, "marche inversee");
		}
	}
}

// Face quasi horizontale (pente 2e-6) traversee par le plan (D3) : elle n'est
// pas rejetee sur sa normale, et le contour est ferme.
TEST (TEST_mesh_slicing, nearly_horizontal_face_crossing_the_plane)
{
	// pave [0,1]^2 x [0, ztop (x)], ztop (x) = 1 + 2e-6 x ; plan z = 1 + 1e-6
	const float t = 1.f + 2e-6f;
	std::unique_ptr<Mesh> slab = meshFrom (
		{ 0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0,
		  0, 0, 1,   1, 0, t,   1, 1, t,   0, 1, 1 },
		{ 0, 3, 2, 1,   4, 5, 6, 7,   0, 4, 7, 3,   0, 1, 5, 4,   3, 7, 6, 2,   1, 2, 6, 5 }, 4);
	ASSERT_GT (signedVolume (*slab), 0.);
	const float z = 1.f + 1e-6f;

	const PlateMesh pm = buildPlateMesh (*slab, Matrix4f ());
	std::vector<SliceLoop> loops;
	sliceLoopsAtZ (pm, z, loops);
	ASSERT_EQ (loops.size (), 1u);
	const double dx = loops[0].pts.front ().x - loops[0].pts.back ().x;
	const double dy = loops[0].pts.front ().y - loops[0].pts.back ().y;
	EXPECT_LT (dx * dx + dy * dy, 1e-12) << "contour ouvert";

	const SliceLayer l = sliceOne (*slab, z);
	ASSERT_EQ (l.size (), 1u);
	EXPECT_GE (l[0].contours[0].pts.size (), 4u);
	// section x > (z - 1) / 2e-6 ~ 0,5, a l'arrondi float des cotes pres
	EXPECT_NEAR (regionArea (l[0]), 0.5, 0.1);
	expectO4 (l, "face quasi horizontale");
}

// La decision par arete ne depend pas de l'ordre des faces.
TEST (TEST_mesh_slicing, face_order_does_not_change_the_section)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	const Soup meshes[2] = { soupOf (*cube), stepSoup () };
	const std::vector<float> zs[2] = { { 0.f, 0.5f, 1.f }, { 0.f, 0.5f, 1.f, 1.5f, 2.f } };
	for (int m = 0; m < 2; ++m)
	{
		auto ref = meshFrom (meshes[m].v, meshes[m].f);
		for (const unsigned int stride : { 7u, 11u })   // premiers avec 12 et 20 triangles
		{
			const Soup p = permuted (meshes[m], stride);
			auto perm = meshFrom (p.v, p.f);
			for (const float z : zs[m])
			{
				SCOPED_TRACE ("maillage " + std::to_string (m) + " pas " + std::to_string (stride) +
				              " z=" + std::to_string (z));
				const SliceLayer a = sliceOne (*ref, z);
				const SliceLayer b = sliceOne (*perm, z);
				ASSERT_EQ (a.size (), b.size ());
				for (size_t r = 0; r < a.size (); ++r)
				{
					EXPECT_NEAR (regionArea (a[r]), regionArea (b[r]), 1e-9);
					ASSERT_EQ (a[r].contours.size (), b[r].contours.size ());
					for (size_t c = 0; c < a[r].contours.size (); ++c)
						EXPECT_TRUE (sameVertexSet (a[r].contours[c].pts, b[r].contours[c].pts, 1e-6f));
				}
			}
		}
	}
}

namespace
{
// Prisme en L : profil (0,0) (2,0) (2,1) (1,1) (1,2) (0,2) dans (x, y), sens
// trigonometrique, extrude sur z dans [0, 1]. Volume 3. Murs en quadrangles,
// chapeaux en faces a SIX sommets (non convexes), commencant au sommet
// `start` du profil. Depuis (2,1), l'eventail naif sortait des triangles
// retournes (sonde du relecteur : 3 regions, aire 2 au lieu de 3).
std::unique_ptr<Mesh> lPrismWithHexCaps (unsigned int start)
{
	const float prof[6][2] = { { 0, 0 }, { 2, 0 }, { 2, 1 }, { 1, 1 }, { 1, 2 }, { 0, 2 } };
	std::unique_ptr<Mesh> m (new Mesh (12, 8));
	for (unsigned int i = 0; i < 6; ++i)
	{
		m->SetVertex (i, prof[i][0], prof[i][1], 0.f);        // bas
		m->SetVertex (6 + i, prof[i][0], prof[i][1], 1.f);    // haut
	}
	for (unsigned int i = 0; i < 6; ++i)   // murs (a0, b0, b1, a1), normale exterieure
	{
		auto f = m->FaceAt (i);
		const unsigned int a0 = i, b0 = (i + 1) % 6;
		f->SetQuad (a0, b0, 6 + b0, 6 + a0);
	}
	auto top = m->FaceAt (6);      // +Z : sens trigonometrique
	auto bottom = m->FaceAt (7);   // -Z : sens horaire
	top->SetNVertices (6);
	bottom->SetNVertices (6);
	for (unsigned int k = 0; k < 6; ++k)
	{
		top->SetVertex (k, 6 + (start + k) % 6);
		bottom->SetVertex (k, (start + 6 - k) % 6);
	}
	return m;
}
} // namespace

// Face horizontale non convexe a N > 3 sommets : le vote se fait sur la normale
// de la face, et la triangulation ne rend aucun triangle retourne.
TEST (TEST_mesh_slicing, non_convex_horizontal_caps)
{
	for (const unsigned int start : { 2u, 0u })   // depuis (2,1), puis depuis (0,0)
	{
		auto prism = lPrismWithHexCaps (start);
		SCOPED_TRACE ("chapeaux depuis le sommet " + std::to_string (start));
		EXPECT_NEAR (signedVolume (*prism), 3., 1e-6);
		size_t nm = 0, b = 0;
		topology (*prism, nm, b);
		EXPECT_EQ (nm, 0u);
		EXPECT_EQ (b, 0u);

		// La triangulation de chaque chapeau couvre le L sans triangle retourne.
		// glutess peut rendre un triangle d'aire NULLE (trois sommets alignes,
		// ici (0,2) (2,0) (1,1)) : il est tolere, la somme des aires signees
		// fait foi.
		const PlateMesh pm = buildPlateMesh (*prism, Matrix4f ());
		double capArea[2] = { 0., 0. };
		for (int t = 0; t < pm.triangleCount (); ++t)
		{
			if (pm.faceOfTriangle[t] < 6) continue;
			const bool isTop = pm.faceOfTriangle[t] == 6;
			const float* a = &pm.vertices[3 * pm.triangles[3 * t]];
			const float* bb = &pm.vertices[3 * pm.triangles[3 * t + 1]];
			const float* c = &pm.vertices[3 * pm.triangles[3 * t + 2]];
			const float nz = (bb[0] - a[0]) * (c[1] - a[1]) - (bb[1] - a[1]) * (c[0] - a[0]);
			capArea[isTop ? 0 : 1] += 0.5 * nz;
			if (std::fabs (nz) > 1e-6f)
			{
				EXPECT_EQ (nz > 0.f, isTop) << "triangle " << t << " retourne";
			}
			EXPECT_FLOAT_EQ (pm.faceNormalZ[t], isTop ? 1.f : -1.f);
		}
		EXPECT_NEAR (capArea[0], 3., 1e-6);
		EXPECT_NEAR (capArea[1], -3., 1e-6);

		for (const Nesting nesting : { Nesting::Containment, Nesting::Winding })
			for (const float z : { 0.f, 0.5f, 1.f })
			{
				SCOPED_TRACE (std::string (nesting == Nesting::Containment ? "Containment" : "Winding") +
				              " z=" + std::to_string (z));
				const SliceLayer l = sliceOne (*prism, z, nesting);
				ASSERT_EQ (l.size (), 1u);
				EXPECT_EQ (l[0].contours.size (), 1u);
				EXPECT_NEAR (regionArea (l[0]), 3., 1e-6);
				expectO4 (l, "chapeaux non convexes");
			}
	}
}

// Pave [0,1]^2 x [0,2] dont chaque mur est coupe en deux quadrangles a z = 1 :
// aretes horizontales DANS le plan, sans face horizontale. Chaque arete a deux
// entrees de cotes OPPOSES (quad du bas : troisieme sommet dessous ; quad du
// haut : dessus), et est gardee une fois. Non-regression du cas nominal "arete
// dans le plan entre deux murs" ; les cas limites de la convention sont la
// marche (T-jonction palier / contremarche), la face quasi horizontale et les
// chapeaux non convexes.
TEST (TEST_mesh_slicing, walls_split_at_the_plane)
{
	std::vector<float> v;
	for (int level = 0; level < 3; ++level)   // anneaux z = 0, 1, 2 (4 sommets chacun)
	{
		const float z = (float)level;
		v.insert (v.end (), { 0, 0, z,   1, 0, z,   1, 1, z,   0, 1, z });
	}
	std::vector<unsigned int> f = { 0, 3, 2, 1,   8, 9, 10, 11 };   // bas (-Z), haut (+Z)
	for (unsigned int level = 0; level < 2; ++level)
		for (unsigned int i = 0; i < 4; ++i)
		{
			const unsigned int a0 = 4 * level + i, b0 = 4 * level + (i + 1) % 4;
			f.insert (f.end (), { a0, b0, b0 + 4, a0 + 4 });
		}
	auto box = meshFrom (v, f, 4);
	EXPECT_NEAR (signedVolume (*box), 2., 1e-6);

	bool onIndices = false;
	unsigned int nmv = 99, borders = 99;
	rawManifold (*box, 1.f, onIndices, nmv, borders);
	EXPECT_TRUE (onIndices);
	EXPECT_EQ (nmv, 0u);
	EXPECT_EQ (borders, 0u);
	for (const float z : { 0.f, 1.f, 2.f })
	{
		SCOPED_TRACE (z);
		const SliceLayer l = sliceOne (*box, z);
		ASSERT_EQ (l.size (), 1u);
		EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
		EXPECT_NEAR (regionArea (l[0]), 1., 1e-6);
	}
}

// Face NON horizontale et non convexe traversee par le plan. Marche dont les deux flancs (plans y = 0 et y = 1) sont des
// faces a SIX sommets ; un eventail depuis (2,1) sortirait du L et rendrait
// des segments hors de la matiere.
TEST (TEST_mesh_slicing, non_convex_vertical_faces)
{
	const float prof[6][2] = { { 0, 0 }, { 2, 0 }, { 2, 1 }, { 1, 1 }, { 1, 2 }, { 0, 2 } };
	for (const unsigned int start : { 2u, 0u })
	{
		SCOPED_TRACE ("flancs depuis le sommet " + std::to_string (start));
		std::unique_ptr<Mesh> m (new Mesh (12, 8));
		for (unsigned int i = 0; i < 6; ++i)
		{
			m->SetVertex (i, prof[i][0], 0.f, prof[i][1]);
			m->SetVertex (6 + i, prof[i][0], 1.f, prof[i][1]);
		}
		for (unsigned int i = 0; i < 6; ++i)
		{
			const unsigned int a0 = i, b0 = (i + 1) % 6;
			m->FaceAt (i)->SetQuad (a0, 6 + a0, 6 + b0, b0);
		}
		auto front = m->FaceAt (6);   // y = 0, normale -Y : sens du profil dans (x, z)
		auto back = m->FaceAt (7);    // y = 1, normale +Y
		front->SetNVertices (6);
		back->SetNVertices (6);
		for (unsigned int k = 0; k < 6; ++k)
		{
			front->SetVertex (k, (start + k) % 6);
			back->SetVertex (k, 6 + (start + 6 - k) % 6);
		}
		EXPECT_NEAR (signedVolume (*m), 3., 1e-6);

		const float zs[5] = { 0.f, 0.5f, 1.f, 1.5f, 2.f };
		const double areas[5] = { 2., 2., 2., 1., 1. };
		for (int k = 0; k < 5; ++k)
		{
			SCOPED_TRACE (zs[k]);
			const SliceLayer l = sliceOne (*m, zs[k]);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_NEAR (layerArea (l), areas[k], 1e-6);
			expectO4 (l, "flancs non convexes");
			// L'aire seule ne suffit pas : un triangle d'eventail retourne rend un
			// aller-retour hors de la matiere, d'aire signee nulle -- une "pique"
			// dans le contour. Les sommets, eux, la voient.
			const float w = (zs[k] > 1.f) ? 1.f : 2.f;
			EXPECT_TRUE (sameVertexSet (l[0].contours[0].pts,
			                            { Vector2f (0, 0), Vector2f (w, 0), Vector2f (w, 1), Vector2f (0, 1) }))
				<< l[0].contours[0].pts.size () << " sommets";
		}
	}
}

// --- D6 (orientation combinatoire) et D2 (segments de longueur nulle) ---------

namespace
{
// Regle GEOMETRIQUE, gardee comme oracle de la regle combinatoire : le segment
// p0 -> p1 laisse la normale de face a gauche, (n x d) . z <= 0. Rend le
// nombre de segments recoupes (longueur > 1e-5) ; echoue sur tout desaccord.
size_t crossCheckOrientation (const Mesh& m, const std::vector<float>& zs, const char* what,
                              const Matrix4f& M = Matrix4f ())
{
	// Sous symetrie, les triangles plateau sont deja retournes : leur normale
	// (faceNormal) est exterieure, et la regle geometrique s'y applique telle quelle.
	const PlateMesh pm = buildPlateMesh (m, M);
	size_t checked = 0;
	for (const float z : zs)
	{
		std::vector<SliceSegment> segs;
		intersectAtZ (pm, z, segs);
		for (const SliceSegment& s : segs)
		{
			const double dx = (double)s.points[1].x - s.points[0].x;
			const double dy = (double)s.points[1].y - s.points[0].y;
			const double len = std::sqrt (dx * dx + dy * dy);
			if (len <= 1e-5) continue;
			const double cross = (double)s.faceNormal.x * dy - (double)s.faceNormal.y * dx;
			EXPECT_LE (cross, 1e-6 * len) << what << " z=" << z << " face " << s.faceId;
			checked++;
		}
	}
	return checked;
}

// Couche brute : au moins un segment de longueur EXACTEMENT nulle ?
bool hasZeroLengthSegment (const Mesh& m, float z)
{
	const PlateMesh pm = buildPlateMesh (m, Matrix4f ());
	std::vector<SliceSegment> segs;
	intersectAtZ (pm, z, segs);
	for (const SliceSegment& s : segs)
		if (s.length2 () == 0.f) return true;
	return false;
}
} // namespace

TEST (TEST_mesh_slicing, combinatorial_orientation_matches_geometric_rule)
{
	auto cube = unitCube (0, 0, 0, 1, 1, 1);
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);
	const Soup st = stepSoup ();
	auto step = meshFrom (st.v, st.f);
	Mesh octas;
	appendTo (octas, octahedronAt (0, 0, 0));
	appendTo (octas, octahedronAt (2, 0, 0));
	appendTo (octas, octahedronAt (5, 0, 0.3f));
	octas.MergeVertices ();
	std::vector<ExtrudeContour> contours;
	std::unique_ptr<Mesh> text = textSolid ("8oB@", 2.f, contours);
	ASSERT_TRUE (text);

	std::vector<float> zs01, zsTorus, zsStep, zsOcta;
	for (int k = 0; k <= 10; ++k) zs01.push_back (0.1f * k);          // sommets dans le plan compris
	for (int k = 0; k <= 40; ++k) zsTorus.push_back (0.1f * k);
	for (int k = 0; k <= 8; ++k) zsStep.push_back (0.25f * k);
	for (int k = -5; k <= 5; ++k) zsOcta.push_back (0.25f * k);

	EXPECT_GT (crossCheckOrientation (*cube, zs01, "cube"), 0u);
	EXPECT_GT (crossCheckOrientation (*torus, zsTorus, "tore"), 0u);
	EXPECT_GT (crossCheckOrientation (*step, zsStep, "marche"), 0u);
	EXPECT_GT (crossCheckOrientation (octas, zsOcta, "octaedres"), 0u);
	EXPECT_GT (crossCheckOrientation (*text, { 0.f, 0.5f, 1.f, 1.5f, 2.f }, "texte"), 0u);

	// matrice miroir : l'accord doit survivre au retournement des triangles
	const Matrix4f mirror = scaleMatrix (-1, 1, 1);
	EXPECT_GT (crossCheckOrientation (*cube, zs01, "cube miroir", mirror), 0u);
	EXPECT_GT (crossCheckOrientation (*step, zsStep, "marche miroir", mirror), 0u);
}

// D2 : contours faits de segments plus courts que le seuil absolu du retrait
// des segments nuls (length2 < FLT_EPSILON, longueur < 3,45e-4). Ce retrait,
// s'il s'appliquait, les absorberait un a un et le contour s'effondrerait en
// une boucle de deux points, d'aire nulle.
TEST (TEST_mesh_slicing, fine_cylinders_keep_their_section)
{
	struct Case { float r; unsigned int n; double tol; };
	// Tolerance relative 1e-3, sauf r = 0,1 : le nettoyage (0.01, test par axe)
	// ne garde qu'une cinquantaine de points sur un cercle de circonference
	// 0,63, et la corde coute 2,1e-3 relatif (6,7e-5 sur 0,0314, releve a la
	// premiere execution). Ce n'est pas le linker : c'est le nettoyage.
	const Case cases[3] = { { 1.f, 20000, 1e-3 }, { 0.3f, 5000, 1e-3 }, { 0.1f, 2500, 5e-3 } };
	for (const Case& c : cases)
	{
		SCOPED_TRACE ("r=" + std::to_string (c.r) + " n=" + std::to_string (c.n));
		std::unique_ptr<Mesh> cyl (CreateCylinder (2.f, c.r, c.n, true, true));
		cyl->MergeVertices ();
		// chaque cote rend DEUX segments (deux triangles), d'une demi-corde chacun
		ASSERT_LT (c.r * std::sin ((float)kPi / c.n), 3.45e-4f) << "demi-segments sous le seuil du retrait";

		const auto t0 = std::chrono::steady_clock::now ();
		const SliceLayer l = sliceOne (*cyl, 1.f);
		const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
		if (c.n == 20000)
		{
			EXPECT_LT (seconds, 5.) << "O(k^2) ?";
		}

		ASSERT_EQ (l.size (), 1u);
		EXPECT_GT (l[0].contours[0].pts.size (), 3u);
		const double expected = regularPolygonArea ((int)c.n, c.r);
		EXPECT_NEAR (regionArea (l[0]), expected, c.tol * expected);
		expectO4 (l, "cylindre fin");
	}
}

namespace
{
// Cube [0,1]^3 dont le mur x = 1 porte un sommet M = (1, 0, 0,5) sur son
// arete verticale avant, et un triangle DEGENERE (T, B, M), aligne, qui
// referme la topologie. Coupe a z = 0,3, il rend un segment de longueur nulle.
std::unique_ptr<Mesh> cubeWithCollinearSliver ()
{
	return meshFrom (
		{ 0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0,   0, 0, 1,   1, 0, 1,   1, 1, 1,   0, 1, 1,
		  1, 0, 0.5f },
		{ 0, 3, 2,   0, 2, 1,        // z = 0
		  4, 5, 6,   4, 6, 7,        // z = 1
		  0, 4, 7,   0, 7, 3,        // x = 0
		  3, 7, 6,   3, 6, 2,        // y = 1
		  0, 1, 5,   0, 5, 4,        // y = 0
		  8, 1, 2,   8, 2, 6,   8, 6, 5,   // x = 1, autour de M
		  5, 1, 8 });                // degenere : T, B, M alignes
}

// Cube [0,1]^3 dont le mur y = 0 est un eventail autour de C = (0,5, 0, 0,5),
// DANS le plan z = 0,5. C est duplique en C' (non soude) pour la moitie haute
// de l'eventail, et deux triangles degeneres (C, T, C') et (A, C, C') recousent
// la topologie : l'arete C - C' est de longueur nulle, dans le plan.
std::unique_ptr<Mesh> cubeWithUnweldedVertexOnPlane ()
{
	return meshFrom (
		{ 0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0,   0, 0, 1,   1, 0, 1,   1, 1, 1,   0, 1, 1,
		  0.5f, 0, 0.5f,   0.5f, 0, 0.5f },
		{ 0, 3, 2,   0, 2, 1,        // z = 0
		  4, 5, 6,   4, 6, 7,        // z = 1
		  0, 4, 7,   0, 7, 3,        // x = 0
		  3, 7, 6,   3, 6, 2,        // y = 1
		  1, 2, 6,   1, 6, 5,        // x = 1
		  0, 1, 8,   1, 5, 8,        // y = 0, moitie basse (C)
		  5, 4, 9,   4, 0, 9,        // y = 0, moitie haute (C')
		  8, 5, 9,   0, 8, 9 });     // coutures degenerees
}
} // namespace

TEST (TEST_mesh_slicing, zero_length_segments_keep_the_contour_closed)
{
	struct Case { const char* name; std::unique_ptr<Mesh> mesh; float z; };
	Case cases[2] = { { "triangle degenere aligne", cubeWithCollinearSliver (), 0.3f },
	                  { "sommet non soude dans le plan", cubeWithUnweldedVertexOnPlane (), 0.5f } };
	for (Case& c : cases)
	{
		SCOPED_TRACE (c.name);
		EXPECT_NEAR (signedVolume (*c.mesh), 1., 1e-6);
		size_t nm = 0, b = 0;
		topology (*c.mesh, nm, b);
		EXPECT_EQ (nm, 0u);
		EXPECT_EQ (b, 0u);
		EXPECT_TRUE (hasZeroLengthSegment (*c.mesh, c.z)) << "la fixture ne produit pas le cas vise";

		bool onIndices = false;
		unsigned int nmv = 99, borders = 99;
		rawManifold (*c.mesh, c.z, onIndices, nmv, borders);
		EXPECT_TRUE (onIndices);   // chemin nominal : le segment nul reste un connecteur
		EXPECT_EQ (nmv, 0u);
		EXPECT_EQ (borders, 0u);

		expectRawClockwise (*c.mesh, c.z);
		const PlateMesh pm = buildPlateMesh (*c.mesh, Matrix4f ());
		std::vector<SliceLoop> loops;
		sliceLoopsAtZ (pm, c.z, loops);
		ASSERT_EQ (loops.size (), 1u);
		EXPECT_EQ (loops[0].pts.front ().x, loops[0].pts.back ().x);
		EXPECT_EQ (loops[0].pts.front ().y, loops[0].pts.back ().y);

		for (const Nesting nesting : kBothModes)
		{
			SCOPED_TRACE (modeName (nesting));
			const SliceLayer l = sliceOne (*c.mesh, c.z, nesting);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_EQ (l[0].contours[0].pts.size (), 4u);   // points confondus retires par cleanContour
			EXPECT_NEAR (regionArea (l[0]), 1., 1e-6);
			expectO4 (l, "segment nul");
		}
	}
}

// D6, test direct : un segment de LONGUEUR NULLE est oriente par la seule
// combinatoire -- une regle geometrique normaliserait un vecteur nul (NaN) et
// le laisserait au hasard.
TEST (TEST_mesh_slicing, zero_length_segment_orientation)
{
	const auto triangleOfFace = [] (const PlateMesh& pm, int face) {
		for (int t = 0; t < pm.triangleCount (); ++t)
			if (pm.faceOfTriangle[t] == face) return t;
		return -1;
	};
	const auto sameKey = [] (const int e[2], int a, int b) {
		return (e[0] == a && e[1] == b) || (e[0] == b && e[1] == a);
	};

	// (a) triangle aligne (T=5, B=1, M=8), face 13 de cubeWithCollinearSliver, z = 0,3.
	// Ordre d'enroulement T -> B -> M : T dessus, B dessous, M dessus. T -> B
	// descend, B -> M MONTE : le segment part de l'arete {B, M} et finit sur {T, B}.
	{
		auto m = cubeWithCollinearSliver ();
		const PlateMesh pm = buildPlateMesh (*m, Matrix4f ());
		const int t = triangleOfFace (pm, 13);
		ASSERT_GE (t, 0);
		std::vector<SliceSegment> segs;
		intersectTriangles (pm, { t }, 0.3f, segs);
		ASSERT_EQ (segs.size (), 1u);
		EXPECT_EQ (segs[0].length2 (), 0.f);
		EXPECT_TRUE (sameKey (segs[0].edges[0], 1, 8)) << segs[0].edges[0][0] << "," << segs[0].edges[0][1];
		EXPECT_TRUE (sameKey (segs[0].edges[1], 5, 1)) << segs[0].edges[1][0] << "," << segs[0].edges[1][1];
	}

	// (b) coutures de cubeWithUnweldedVertexOnPlane, z = 0,5 : arete C (8) - C' (9)
	// dans le plan, de longueur nulle (nOn == 2).
	//   face 14 = (C, T, C') : ordre R, P, Q = T, C', C ; T DESSUS -> de Q vers P,
	//            donc de C vers C' ;
	//   face 15 = (A, C, C') : ordre R, P, Q = A, C, C' ; A DESSOUS -> de P vers Q,
	//            donc de C vers C'.
	{
		auto m = cubeWithUnweldedVertexOnPlane ();
		const PlateMesh pm = buildPlateMesh (*m, Matrix4f ());
		for (const int face : { 14, 15 })
		{
			SCOPED_TRACE (face);
			const int t = triangleOfFace (pm, face);
			ASSERT_GE (t, 0);
			std::vector<SliceSegment> segs;
			intersectTriangles (pm, { t }, 0.5f, segs);   // une seule entree : gardee
			ASSERT_EQ (segs.size (), 1u);
			EXPECT_EQ (segs[0].length2 (), 0.f);
			EXPECT_TRUE (sameKey (segs[0].edges[0], 8, 8));
			EXPECT_TRUE (sameKey (segs[0].edges[1], 9, 9));
		}
	}
}

// --- contours degeneres ecartes ---------------------------------------------------

// Juste sous la pointe haute (ou au-dessus de la pointe basse) d'un octaedre, la
// section est un losange de demi-diagonale delta : cleanContour (0.01) le reduit a
// moins de trois points, ecarte : aucune region, dans les deux modes (sans ce
// retrait, Containment rendrait une region a contour VIDE).
TEST (TEST_mesh_slicing, degenerate_contours_are_dropped_near_a_tip)
{
	auto octa = octahedronAt (0.f, 0.f, 1.f);   // pointes en z = 0 et z = 2
	for (const Nesting nesting : kBothModes)
	{
		SCOPED_TRACE (modeName (nesting));
		for (const float delta : { 3e-7f, 1e-5f, 1e-3f })
		{
			SCOPED_TRACE (delta);
			EXPECT_TRUE (sliceOne (*octa, 2.f - delta, nesting).empty ()) << "sous la pointe haute";
			EXPECT_TRUE (sliceOne (*octa, delta, nesting).empty ()) << "au-dessus de la pointe basse";
		}
		for (const float z : { 1.9f, 0.1f })   // delta = 0,1 : section de 4 points, aire 2 delta^2
		{
			SCOPED_TRACE (z);
			const SliceLayer l = sliceOne (*octa, z, nesting);
			ASSERT_EQ (l.size (), 1u);
			ASSERT_EQ (l[0].contours.size (), 1u);
			EXPECT_EQ (l[0].contours[0].pts.size (), 4u);
			EXPECT_NEAR (regionArea (l[0]), 2. * 0.1 * 0.1, 1e-6);
			expectO4 (l, "octaedre");
		}
	}
}

// Tore QUASI tangent : bande fine, geometriquement correcte, que le filtre ne
// touche pas (ses deux boucles gardent bien plus de trois points).
TEST (TEST_mesh_slicing, nearly_tangent_torus_keeps_its_thin_band)
{
	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);   // z dans [0, 4]
	for (const Nesting nesting : kBothModes)
	{
		SCOPED_TRACE (modeName (nesting));
		for (const float z : { 3e-7f, 1e-5f })
		{
			SCOPED_TRACE (z);
			const SliceLayer l = sliceOne (*torus, z, nesting);
			ASSERT_EQ (l.size (), 1u);
			EXPECT_EQ (l[0].contours.size (), 2u);
			for (const ExtrudeContour& c : l[0].contours) EXPECT_GE (c.pts.size (), 3u);
			EXPECT_GT (regionArea (l[0]), 0.);
			expectO4 (l, "tore quasi tangent");
		}
	}
}

// --- D8 : independance a l'ordre des faces ------------------------------------------

namespace
{
// Triangles decales de `shift`, et eventuellement dans l'ordre inverse.
Soup shiftedSoup (const Soup& s, size_t shift, bool reversed)
{
	const size_t nt = s.f.size () / 3;
	Soup o;
	o.v = s.v;
	o.f.reserve (s.f.size ());
	for (size_t k = 0; k < nt; ++k)
	{
		size_t t = (k + shift) % nt;
		if (reversed) t = nt - 1 - t;
		o.f.insert (o.f.end (), { s.f[3 * t], s.f[3 * t + 1], s.f[3 * t + 2] });
	}
	return o;
}

// Face d'origine de la face k de shiftedSoup (s, shift, reversed).
int referenceFace (size_t k, size_t nt, size_t shift, bool reversed)
{
	size_t t = (k + shift) % nt;
	if (reversed) t = nt - 1 - t;
	return (int)t;
}

// Regions, contours et POINTS identiques au bit pres ; faceIds ramenes a la
// numerotation de reference par `toRef`, puis compares tries (si toRef donne).
void expectSameGeometry (const SliceLayer& a, const SliceLayer& b, const std::string& what,
                         const std::function<int (int)>& toRef = nullptr)
{
	ASSERT_EQ (a.size (), b.size ()) << what;
	for (size_t r = 0; r < a.size (); ++r)
	{
		ASSERT_EQ (a[r].contours.size (), b[r].contours.size ()) << what << " region " << r;
		for (size_t c = 0; c < a[r].contours.size (); ++c)
		{
			const ExtrudeContour& ca = a[r].contours[c];
			const ExtrudeContour& cb = b[r].contours[c];
			EXPECT_EQ (ca.isHole, cb.isHole) << what;
			ASSERT_EQ (ca.pts.size (), cb.pts.size ()) << what << " region " << r << " contour " << c;
			for (size_t i = 0; i < ca.pts.size (); ++i)
			{
				EXPECT_EQ (ca.pts[i].x, cb.pts[i].x) << what << " point " << i;
				EXPECT_EQ (ca.pts[i].y, cb.pts[i].y) << what << " point " << i;
			}
		}
		if (toRef)
		{
			std::vector<int> mapped;
			for (const int f : b[r].faceIds) mapped.push_back (toRef (f));
			std::sort (mapped.begin (), mapped.end ());
			EXPECT_EQ (a[r].faceIds, mapped) << what << " faceIds, region " << r;
		}
	}
}
} // namespace

TEST (TEST_mesh_slicing, face_order_independence)
{
	// compareFaces : faux pour la seule face dupliquee. Les deux jumelles ont
	// des indices distincts dans la numerotation de reference, et la reparation
	// (removeDuplicateEdges) garde la premiere rencontree -- dependance
	// residuelle documentee dans mesh_slicing.h. Les points restent compares.
	struct Case { std::string name; Soup soup; std::vector<float> zs; bool compareFaces = true; };
	std::vector<Case> cases;

	auto torus = lyingTorus (20, 20, 5.f, 2.f, 2.f);
	cases.push_back ({ "tore", soupOf (*torus), { 1.5f } });

	std::unique_ptr<Mesh> cyl (CreateCylinder (2.f, 1.f, 2000, true, true));
	cyl->MergeVertices ();
	cases.push_back ({ "cylindre 2000", soupOf (*cyl), { 1.f } });

	std::vector<ExtrudeContour> contours;
	std::unique_ptr<Mesh> text = textSolid ("8oB@", 2.f, contours);
	ASSERT_TRUE (text);
	cases.push_back ({ "texte", soupOf (*text), { 1.f } });

	{   // tore + un triangle traversant z = 1,5 duplique (chemin de reparation)
		Soup s = soupOf (*torus);
		const PlateMesh pm = buildPlateMesh (*torus, Matrix4f ());
		int t = 0;
		while (t < pm.triangleCount () && !(pm.zmin[t] < 1.5f && pm.zmax[t] > 1.5f)) ++t;
		ASSERT_LT (t, pm.triangleCount ());
		s.f.insert (s.f.end (), { s.f[3 * t], s.f[3 * t + 1], s.f[3 * t + 2] });
		cases.push_back ({ "tore a face dupliquee", s, { 1.5f }, false });
	}
	{
		Mesh octas;
		appendTo (octas, octahedronAt (0, 0, 0));
		appendTo (octas, octahedronAt (2, 0, 0));
		octas.MergeVertices ();   // soudes en (1, 0, 0)
		cases.push_back ({ "octaedres pinces", soupOf (octas), { 0.f, 0.25f } });
	}

	for (const Case& c : cases)
	{
		auto ref = meshFrom (c.soup.v, c.soup.f);
		for (const Nesting nesting : kBothModes)
			for (const float z : c.zs)
			{
				const SliceLayer a = sliceOne (*ref, z, nesting);
				ASSERT_FALSE (a.empty ()) << c.name;
				for (const size_t shift : { (size_t)0, (size_t)1, (size_t)7, (size_t)333 })
					for (const bool reversed : { false, true })   // l'identite comprise
					{
						const Soup ps = shiftedSoup (c.soup, shift, reversed);
						auto perm = meshFrom (ps.v, ps.f);
						const size_t nt = c.soup.f.size () / 3;
						const std::string what = c.name + " " + modeName (nesting) + " z=" + std::to_string (z) +
						                         " decalage " + std::to_string (shift) + (reversed ? " envers" : "");
						std::function<int (int)> toRef;
						if (c.compareFaces)
							toRef = [=] (int k) { return referenceFace ((size_t)k, nt, shift, reversed); };
						expectSameGeometry (a, sliceOne (*perm, z, nesting), what, toRef);
					}
			}
	}
}

// D8 : liens ASYMETRIQUES (comme en laisse la branche n == 3 de
// removeNonManifoldVertices). Carre A -> B -> C -> D -> A, plus un segment E
// qui arrive sur l'origine de A : A.linked[0] = E, alors que D.linked[1] = A.
// La marche avant revient sur A, mais la marche arriere prolonge par E : le
// chemin n'est PAS ferme, et aucun point ne doit etre perdu.
TEST (TEST_mesh_slicing, trace_chains_asymmetric_links)
{
	std::vector<SliceSegment> s (5);
	const float p[6][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, { -1, 0 } };
	for (int k = 0; k < 4; ++k)
	{
		s[k].faceId = k;
		s[k].points[0] = Vector3f (p[k][0], p[k][1], 0.f);
		s[k].points[1] = Vector3f (p[k + 1][0], p[k + 1][1], 0.f);
	}
	s[4].faceId = 4;   // E : (-1, 0) -> (0, 0)
	s[4].points[0] = Vector3f (-1.f, 0.f, 0.f);
	s[4].points[1] = Vector3f (0.f, 0.f, 0.f);
	for (int k = 0; k < 4; ++k)
	{
		s[k].linked[1] = &s[(k + 1) % 4];
		s[(k + 1) % 4].linked[0] = &s[k];
	}
	s[0].linked[0] = &s[4];   // ecrase : D pointe toujours vers A, sans reciproque
	s[4].linked[1] = &s[0];
	s[4].linked[0] = &s[3];

	std::vector<std::vector<Vector3f>> paths;
	std::vector<std::vector<int>> faces;
	std::vector<bool> closed;
	traceChains (s, paths, faces, &closed);
	ASSERT_EQ (paths.size (), 1u);
	ASSERT_EQ (closed.size (), 1u);
	EXPECT_FALSE (closed[0]);
	// Six points : les cinq de la marche avant (carre et retour sur (0,0)), plus
	// celui que la marche arriere insere en tete : E.points[next] avec
	// next = 1 (E.linked[0] deja visite), soit (0,0) et non (-1,0). Ce chemin ne
	// doit pas etre marque FERME, sans quoi l'ancrage en retirerait le premier
	// point.
	ASSERT_EQ (paths[0].size (), 6u);
	EXPECT_EQ (paths[0].front ().x, 0.f);
	EXPECT_EQ (paths[0].front ().y, 0.f);
	EXPECT_EQ (faces[0].size (), 5u);

	// Et une chaine ferme SYMETRIQUE reste marquee fermee.
	for (int k = 0; k < 4; ++k) s[k].visited = false;
	s[0].linked[0] = &s[3];
	s.pop_back ();
	paths.clear (); faces.clear (); closed.clear ();
	traceChains (s, paths, faces, &closed);
	ASSERT_EQ (paths.size (), 1u);
	EXPECT_TRUE (closed[0]);
	EXPECT_EQ (paths[0].size (), 5u);

	// Dans la chaine complete, une boucle non fermee n'est pas ancree : buildRegions
	// ne retire aucun sommet d'un chemin ouvert.
	SliceLoop open;
	open.pts = { Vector2d (-1, 0), Vector2d (0, 0), Vector2d (1, 0), Vector2d (1, 1), Vector2d (0, 1), Vector2d (0, 0) };
	open.closed = false;
	std::vector<SliceLoop> loops = { open };
	const SliceLayer l = buildRegions (loops, 0, false, false, Nesting::Containment);
	ASSERT_EQ (l.size (), 1u);
	EXPECT_TRUE (sameVertexSet (l[0].contours[0].pts,
	                            { Vector2f (-1, 0), Vector2f (1, 0), Vector2f (1, 1), Vector2f (0, 1), Vector2f (0, 0) }))
		<< l[0].contours[0].pts.size () << " points";
}
