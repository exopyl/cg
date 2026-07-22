#include "parameterized_shapes.h"

#include "surface_basic.h"
#include "surface_parametric.h"
#include "voxels_menger_sponge.h"
#include "import_svg.h"
#include "surface_implicit.h"
#include "surface_implicit_tandem.h"
#include "lsysteminit.h"
#include "surface_architecture.h"
#include "architecture_gothic.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

// ===========================================================================
// Basic shapes
// ===========================================================================

// --- Cube ------------------------------------------------------------------
ParameterizedCube::ParameterizedCube()                  { Regenerate(); }
std::vector<Parameter> ParameterizedCube::GetParameters()
{
	return {
		Parameter::MakeFloat("Edge length", &m_edgeLength, 0.01f, 100.f),
		Parameter::MakeBool("Triangulated", &m_triangulated),
	};
}
void ParameterizedCube::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateCube(m_triangulated);
	float scale = m_edgeLength / 2.f;
	for (unsigned int i = 0; i < m_pMesh->m_nVertices; i++)
	{
		m_pMesh->m_pVertices[3*i]     *= scale;
		m_pMesh->m_pVertices[3*i + 1] *= scale;
		m_pMesh->m_pVertices[3*i + 2] *= scale;
	}
	m_pMesh->ComputeNormals();
}

// --- Sphere ----------------------------------------------------------------
ParameterizedSphere::ParameterizedSphere()              { Regenerate(); }
std::vector<Parameter> ParameterizedSphere::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
	};
}
void ParameterizedSphere::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new ParametricSphere(m_nu, m_nv);
	m_pMesh->ComputeNormals();
}

// --- Cylinder --------------------------------------------------------------
ParameterizedCylinder::ParameterizedCylinder()          { Regenerate(); }
std::vector<Parameter> ParameterizedCylinder::GetParameters()
{
	return {
		Parameter::MakeFloat("Height", &m_height, 0.1f, 50.f),
		Parameter::MakeFloat("Radius", &m_radius, 0.01f, 20.f),
		Parameter::MakeInt("nVertices", &m_nVertices, 3, 200),
		Parameter::MakeBool("Cap", &m_cap),
	};
}
void ParameterizedCylinder::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateCylinder(m_height, m_radius, (unsigned int)m_nVertices, m_cap);
	m_pMesh->ComputeNormals();
}

// --- Cone ------------------------------------------------------------------
ParameterizedCone::ParameterizedCone()                  { Regenerate(); }
std::vector<Parameter> ParameterizedCone::GetParameters()
{
	return {
		Parameter::MakeFloat("Height", &m_height, 0.1f, 50.f),
		Parameter::MakeFloat("Radius", &m_radius, 0.01f, 20.f),
		Parameter::MakeInt("nVertices", &m_nVertices, 3, 200),
		Parameter::MakeBool("Cap", &m_cap),
	};
}
void ParameterizedCone::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateCone(m_height, m_radius, (unsigned int)m_nVertices, m_cap);
	m_pMesh->ComputeNormals();
}

// --- Capsule ---------------------------------------------------------------
ParameterizedCapsule::ParameterizedCapsule()            { Regenerate(); }
std::vector<Parameter> ParameterizedCapsule::GetParameters()
{
	return {
		Parameter::MakeInt("n", &m_n, 4, 100),
		Parameter::MakeFloat("Height", &m_height, 0.1f, 50.f),
		Parameter::MakeFloat("Radius", &m_radius, 0.01f, 20.f),
	};
}
void ParameterizedCapsule::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateCapsule((unsigned int)m_n, m_height, m_radius);
	m_pMesh->ComputeNormals();
}

// --- Torus -----------------------------------------------------------------
ParameterizedTorus::ParameterizedTorus()                { Regenerate(); }
std::vector<Parameter> ParameterizedTorus::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("Major radius", &m_R, 0.1f, 50.f),
		Parameter::MakeFloat("Minor radius", &m_r, 0.01f, 20.f),
	};
}
void ParameterizedTorus::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new ParametricTorus(m_nu, m_nv, m_R, m_r);
	m_pMesh->ComputeNormals();
}

// ===========================================================================
// Parametric surfaces
// ===========================================================================

// --- Klein Bottle ----------------------------------------------------------
ParameterizedKleinBottle::ParameterizedKleinBottle()    { Regenerate(); }
std::vector<Parameter> ParameterizedKleinBottle::GetParameters()
{
	return {
		Parameter::MakeInt("Theta resolution", &m_thetaRes, 4, 200),
		Parameter::MakeInt("Phi resolution", &m_phiRes, 4, 200),
	};
}
void ParameterizedKleinBottle::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateKleinBottle(m_thetaRes, m_phiRes);
	m_pMesh->ComputeNormals();
}

// --- Helicoid --------------------------------------------------------------
ParameterizedHelicoid::ParameterizedHelicoid()          { Regenerate(); }
std::vector<Parameter> ParameterizedHelicoid::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("a", &m_a, 0.01f, 10.f),
		Parameter::MakeFloat("b", &m_b, 0.01f, 10.f),
		Parameter::MakeFloat("c", &m_c, 0.01f, 5.f),
	};
}
void ParameterizedHelicoid::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new EllipticHelicoid(m_nu, m_nv, m_a, m_b, m_c);
	m_pMesh->ComputeNormals();
}

// --- Seashell --------------------------------------------------------------
ParameterizedSeashell::ParameterizedSeashell()          { Regenerate(); }
std::vector<Parameter> ParameterizedSeashell::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
	};
}
void ParameterizedSeashell::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new SeaShell(m_nu, m_nv);
	m_pMesh->ComputeNormals();
}

// --- Seashell von Seggern --------------------------------------------------
ParameterizedSeashellVonSeggern::ParameterizedSeashellVonSeggern() { Regenerate(); }
std::vector<Parameter> ParameterizedSeashellVonSeggern::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("a", &m_a, 0.01f, 5.f),
		Parameter::MakeFloat("b", &m_b, 0.01f, 5.f),
		Parameter::MakeFloat("c", &m_c, 0.01f, 5.f),
		Parameter::MakeFloat("n", &m_n, 0.5f, 10.f),
	};
}
void ParameterizedSeashellVonSeggern::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new SeaShellVonSeggern(m_nu, m_nv, m_a, m_b, m_c, m_n);
	m_pMesh->ComputeNormals();
}

// --- Corkscrew -------------------------------------------------------------
ParameterizedCorkscrew::ParameterizedCorkscrew()        { Regenerate(); }
std::vector<Parameter> ParameterizedCorkscrew::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("a", &m_a, 0.01f, 10.f),
		Parameter::MakeFloat("b", &m_b, 0.01f, 5.f),
	};
}
void ParameterizedCorkscrew::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new CorkscrewSurface(m_nu, m_nv, m_a, m_b);
	m_pMesh->ComputeNormals();
}

// --- Mobius Strip ----------------------------------------------------------
ParameterizedMobiusStrip::ParameterizedMobiusStrip()    { Regenerate(); }
std::vector<Parameter> ParameterizedMobiusStrip::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 100),
		Parameter::MakeFloat("Width", &m_w, 0.01f, 1.f),
		Parameter::MakeFloat("Radius", &m_r, 0.1f, 5.f),
	};
}
void ParameterizedMobiusStrip::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new MobiusStrip(m_nu, m_nv, m_w, m_r);
	m_pMesh->ComputeNormals();
}

// --- Radial Wave -----------------------------------------------------------
ParameterizedRadialWave::ParameterizedRadialWave()      { Regenerate(); }
std::vector<Parameter> ParameterizedRadialWave::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("Radius", &m_radius, 1.f, 50.f),
		Parameter::MakeFloat("Height", &m_height, 0.1f, 50.f),
		Parameter::MakeFloat("Frequency", &m_frequency, 0.01f, 5.f),
	};
}
void ParameterizedRadialWave::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new RadialWave(m_nu, m_nv, m_radius, m_height, m_frequency);
	m_pMesh->ComputeNormals();
}

// --- Breather --------------------------------------------------------------
ParameterizedBreather::ParameterizedBreather()          { Regenerate(); }
std::vector<Parameter> ParameterizedBreather::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
	};
}
void ParameterizedBreather::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new Breather(m_nu, m_nv);
	m_pMesh->ComputeNormals();
}

// --- Hyperbolic Paraboloid -------------------------------------------------
ParameterizedHyperbolicParaboloid::ParameterizedHyperbolicParaboloid() { Regenerate(); }
std::vector<Parameter> ParameterizedHyperbolicParaboloid::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("xmin", &m_xmin, -50.f, 0.f),
		Parameter::MakeFloat("xmax", &m_xmax, 0.f, 50.f),
		Parameter::MakeFloat("ymin", &m_ymin, -50.f, 0.f),
		Parameter::MakeFloat("ymax", &m_ymax, 0.f, 50.f),
	};
}
void ParameterizedHyperbolicParaboloid::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new HyperbolicParaboloid(m_nu, m_nv, m_xmin, m_xmax, m_ymin, m_ymax);
	m_pMesh->ComputeNormals();
}

// --- Monkey Saddle ---------------------------------------------------------
ParameterizedMonkeySaddle::ParameterizedMonkeySaddle()  { Regenerate(); }
std::vector<Parameter> ParameterizedMonkeySaddle::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("xmin", &m_xmin, -50.f, 0.f),
		Parameter::MakeFloat("xmax", &m_xmax, 0.f, 50.f),
		Parameter::MakeFloat("ymin", &m_ymin, -50.f, 0.f),
		Parameter::MakeFloat("ymax", &m_ymax, 0.f, 50.f),
	};
}
void ParameterizedMonkeySaddle::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new MonkeySaddle(m_nu, m_nv, m_xmin, m_xmax, m_ymin, m_ymax);
	m_pMesh->ComputeNormals();
}

// --- Blobs -----------------------------------------------------------------
ParameterizedBlobs::ParameterizedBlobs()                { Regenerate(); }
std::vector<Parameter> ParameterizedBlobs::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("xmin", &m_xmin, -50.f, 0.f),
		Parameter::MakeFloat("xmax", &m_xmax, 0.f, 50.f),
		Parameter::MakeFloat("ymin", &m_ymin, -50.f, 0.f),
		Parameter::MakeFloat("ymax", &m_ymax, 0.f, 50.f),
	};
}
void ParameterizedBlobs::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new Blobs(m_nu, m_nv, m_xmin, m_xmax, m_ymin, m_ymax);
	m_pMesh->ComputeNormals();
}

// --- Drop ------------------------------------------------------------------
ParameterizedDrop::ParameterizedDrop()                  { Regenerate(); }
std::vector<Parameter> ParameterizedDrop::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("xmin", &m_xmin, -50.f, 0.f),
		Parameter::MakeFloat("xmax", &m_xmax, 0.f, 50.f),
		Parameter::MakeFloat("ymin", &m_ymin, -50.f, 0.f),
		Parameter::MakeFloat("ymax", &m_ymax, 0.f, 50.f),
	};
}
void ParameterizedDrop::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new Drop(m_nu, m_nv, m_xmin, m_xmax, m_ymin, m_ymax);
	m_pMesh->ComputeNormals();
}

// --- Guimard ---------------------------------------------------------------
ParameterizedGuimard::ParameterizedGuimard()            { Regenerate(); }
std::vector<Parameter> ParameterizedGuimard::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 4, 200),
		Parameter::MakeInt("nv", &m_nv, 4, 200),
		Parameter::MakeFloat("a", &m_a, 0.1f, 10.f),
		Parameter::MakeFloat("b", &m_b, 0.1f, 10.f),
		Parameter::MakeFloat("c", &m_c, 0.1f, 10.f),
	};
}
void ParameterizedGuimard::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new Guimard(m_nu, m_nv, m_a, m_b, m_c);
	m_pMesh->ComputeNormals();
}

// ===========================================================================
// Knots
// ===========================================================================

// --- Torus Knot ------------------------------------------------------------
ParameterizedTorusKnot::ParameterizedTorusKnot()        { Regenerate(); }
std::vector<Parameter> ParameterizedTorusKnot::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 10, 500),
		Parameter::MakeInt("nv", &m_nv, 4, 100),
		Parameter::MakeInt("p", &m_a, 1, 20),
		Parameter::MakeInt("q", &m_b, 1, 20),
	};
}
void ParameterizedTorusKnot::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new TorusKnot(m_nu, m_nv, (unsigned int)m_a, (unsigned int)m_b);
	m_pMesh->ComputeNormals();
}

// --- Cinquefoil Knot -------------------------------------------------------
ParameterizedCinquefoilKnot::ParameterizedCinquefoilKnot() { Regenerate(); }
std::vector<Parameter> ParameterizedCinquefoilKnot::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 10, 500),
		Parameter::MakeInt("nv", &m_nv, 4, 100),
		Parameter::MakeInt("a", &m_a, 1, 10),
	};
}
void ParameterizedCinquefoilKnot::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new CinquefoilKnot(m_nu, m_nv, (unsigned int)m_a);
	m_pMesh->ComputeNormals();
}

// --- Trefoil Knot ----------------------------------------------------------
ParameterizedTrefoilKnot::ParameterizedTrefoilKnot()    { Regenerate(); }
std::vector<Parameter> ParameterizedTrefoilKnot::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 10, 500),
		Parameter::MakeInt("nv", &m_nv, 4, 100),
	};
}
void ParameterizedTrefoilKnot::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new TrefoilKnot1(m_nu, m_nv);
	m_pMesh->ComputeNormals();
}

// --- Borromean Rings -------------------------------------------------------
ParameterizedBorromeanRings::ParameterizedBorromeanRings() { Regenerate(); }
std::vector<Parameter> ParameterizedBorromeanRings::GetParameters()
{
	return {
		Parameter::MakeInt("nu", &m_nu, 10, 500),
		Parameter::MakeInt("nv", &m_nv, 4, 100),
	};
}
void ParameterizedBorromeanRings::Regenerate()
{
	delete m_pMesh;
	m_pMesh = new BorromeanRings(m_nu, m_nv);
	m_pMesh->ComputeNormals();
}

// ===========================================================================
// Fractal shapes
// ===========================================================================

// --- Menger Sponge ---------------------------------------------------------
ParameterizedMengerSponge::ParameterizedMengerSponge()  { Regenerate(); }
std::vector<Parameter> ParameterizedMengerSponge::GetParameters()
{
	return {
		Parameter::MakeInt("Level", &m_level, 1, 4),
	};
}
void ParameterizedMengerSponge::Regenerate()
{
	delete m_pMesh;
	MengerSponge sponge((unsigned int)m_level);
	m_pMesh = sponge.ToMesh();
	if (m_pMesh)
		m_pMesh->ComputeNormals();
}

// ===========================================================================
// SVG-based shapes
// ===========================================================================

// --- SVG extrusion ---------------------------------------------------------
ParameterizedSvgExtrusion::ParameterizedSvgExtrusion(const std::string& filename)
	: m_filename(filename)
{
	Regenerate();
}
std::vector<Parameter> ParameterizedSvgExtrusion::GetParameters()
{
	return {
		Parameter::MakeFloat("Height",         &m_height,     0.001f, 10.f),
		Parameter::MakeFloat("Flatten Tol",    &m_flattenTol, 0.05f,  10.f),
	};
}
void ParameterizedSvgExtrusion::Regenerate()
{
	delete m_pMesh;
	SvgExtrudeOptions opt;
	opt.height       = m_height;
	opt.flattenTol   = m_flattenTol;
	opt.centerAndFit = true;
	opt.invertY      = true;
	m_pMesh = import_svg_extruded(m_filename, opt);
	if (m_pMesh)
		m_pMesh->ComputeNormals();
}

// ===========================================================================
// Implicit surface from a point cloud
// ===========================================================================

ParameterizedImplicitFromPoints::ParameterizedImplicitFromPoints(const std::string& plyPath)
	: m_filename(plyPath)
{
	// Load the cloud once at construction; Mesh::load() dispatches on the .ply
	// extension and fills m_pVertices even for a face-less point cloud.
	Mesh tmp;
	if (tmp.load(m_filename.c_str()) == 0 && tmp.GetNVertices() > 0)
		m_field.Build(tmp.m_pVertices.data(), (int)tmp.GetNVertices());

	Regenerate();
}

std::vector<Parameter> ParameterizedImplicitFromPoints::GetParameters()
{
	return {
		Parameter::MakeInt  ("Resolution",       &m_resolution,  4, 200),
		Parameter::MakeFloat("Iso distance",      &m_isoDistance, 0.001f, 10.f),
		Parameter::MakeBool ("Simplify (tandem)", &m_simplify),
	};
}

void ParameterizedImplicitFromPoints::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;

	if (m_field.NPoints() == 0)
		return; // no cloud loaded -> no mesh (the caller reports the error)

	m_field.SetIsoDistance(m_isoDistance);

	float vmin[3], vmax[3];
	m_field.GetPaddedAABB(vmin, vmax);

	// Interpret "Resolution" as the cell count along the largest axis, so the
	// grid density is independent of the cloud's world scale. Guard against a
	// zero divisor / zero resolution for tiny or degenerate clouds.
	float extMax = vmax[0] - vmin[0];
	for (int i = 1; i < 3; i++)
		extMax = (vmax[i] - vmin[i] > extMax) ? (vmax[i] - vmin[i]) : extMax;
	if (extMax <= 0.f)
		extMax = 1.f;
	int perUnit = (int)((float)m_resolution / extMax + 0.5f);
	if (perUnit < 1)
		perUnit = 1;

	// The tandem extractor decimates the iso-surface in tandem with extraction
	// (quadric-error edge contraction). get_triangulation() dispatches to the
	// right pre/post hooks through the base pointer, so the rest is identical.
	ImplicitSurface* surf = m_simplify ? new ImplicitSurfaceTandem()
	                                   : new ImplicitSurface();
	surf->set_bbox(vmin[0], vmin[1], vmin[2], vmax[0], vmax[1], vmax[2]);
	surf->set_resolution_per_unit(perUnit);
	surf->set_orientation(1);
	surf->set_eval_func(&PointCloudField::Eval);
	surf->set_eval_data(&m_field);
	surf->set_value(m_field.GetIsoLevel());

	int nv = 0, nf = 0;
	float* verts = nullptr;
	unsigned int* faces = nullptr;
	surf->get_triangulation(&nv, &verts, &nf, &faces);
	delete surf;

	m_pMesh = new Mesh();
	if (nv > 0)
		m_pMesh->SetVertices((unsigned int)nv, verts);
	if (nf > 0)
		m_pMesh->SetFaces((unsigned int)nf, 3, faces);
	m_pMesh->ComputeNormals();

	free(verts);
	free(faces);
}

// ===========================================================================
//  ParameterizedLSystem : L-systeme -> tube 3D le long de la marche tortue
// ===========================================================================

namespace {

// Noms des L-systemes indexes par valeur d'enum (construits une seule fois).
const std::vector<std::string>& LSystemCatalogueNames()
{
	static std::vector<std::string> names;
	if (names.empty())
	{
		std::map<int, LSystemData*> cat;
		InitLSystems(cat);
		int n = 0;
		for (auto& kv : cat) n = std::max(n, kv.first + 1);
		names.assign(n, std::string());
		for (auto& kv : cat)
		{
			if (kv.first >= 0 && kv.first < n && kv.second && kv.second->pLSystem)
				names[kv.first] = kv.second->pLSystem->GetName();
			delete kv.second->pLSystem;
			delete kv.second;
		}
	}
	return names;
}

} // namespace

ParameterizedLSystem::ParameterizedLSystem() { Regenerate(); }

std::vector<Parameter> ParameterizedLSystem::GetParameters()
{
	return {
		Parameter::MakeEnum("System", &m_system, LSystemCatalogueNames()),
		Parameter::MakeInt("Iterations", &m_iterations, 0, 6),
		Parameter::MakeFloat("Tube radius", &m_thickness, 0.005f, 0.3f),
	};
}

void ParameterizedLSystem::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;

	std::map<int, LSystemData*> cat;
	InitLSystems(cat);
	auto cleanup = [&]() { for (auto& kv : cat) { delete kv.second->pLSystem; delete kv.second; } };

	auto it = cat.find(m_system);
	if (it == cat.end() || !it->second || !it->second->pLSystem)
	{
		cleanup();
		m_pMesh = CreateTubes({}, m_thickness); // mesh vide mais valide
		return;
	}

	LSystem* ls = it->second->pLSystem;
	int iters = m_iterations;
	if (iters < 0) iters = 0;
	if (iters > 6) iters = 6; // borne l'expansion exponentielle de Next()
	for (int k = 0; k < iters; ++k) ls->Next();

	const bool is3D = (m_system >= LSYSTEM_HILBERT_CURVE_3D);
	if (is3D) ls->ComputeGraphicalInterpretation3D();
	else      ls->ComputeGraphicalInterpretation2D();

	// --- walk -> polylignes : rupture a chaque saut (m_bDrawable[i] == false) ---
	const int dim = ls->m_iDimension;
	const int np  = ls->m_iNumberPoints;
	auto P = [&](int i) -> Vector3f {
		return (dim == 3) ? Vector3f(ls->m_walk[3*i], ls->m_walk[3*i+1], ls->m_walk[3*i+2])
		                  : Vector3f(ls->m_walk[2*i], ls->m_walk[2*i+1], 0.f);
	};

	const size_t MAX_SEG = 60000;
	std::vector<std::vector<Vector3f>> chains;
	std::vector<Vector3f> cur;
	size_t segCount = 0;
	for (int i = 1; i < np && segCount < MAX_SEG; ++i)
	{
		bool drawn = (i < (int)ls->m_bDrawable.size()) && ls->m_bDrawable[i];
		if (drawn)
		{
			if (cur.empty()) cur.push_back(P(i-1));
			cur.push_back(P(i));
			++segCount;
		}
		else
		{
			if (cur.size() >= 2) chains.push_back(std::move(cur));
			cur.clear();
		}
	}
	if (cur.size() >= 2) chains.push_back(std::move(cur));

	cleanup();

	if (chains.empty()) { m_pMesh = CreateTubes({}, m_thickness); return; }

	// --- normalisation (presentation) : centrer + echelle diagonale bbox -> 4 ---
	float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
	for (auto& ch : chains) for (auto& q : ch) for (int c = 0; c < 3; ++c)
	{ float v = q[c]; if (v < mn[c]) mn[c] = v; if (v > mx[c]) mx[c] = v; }
	float ctr[3] = {(mn[0]+mx[0])*0.5f,(mn[1]+mx[1])*0.5f,(mn[2]+mx[2])*0.5f};
	float ex=mx[0]-mn[0], ey=mx[1]-mn[1], ez=mx[2]-mn[2];
	float diag = sqrtf(ex*ex+ey*ey+ez*ez);
	float scale = (diag > 1e-6f) ? (4.0f / diag) : 1.0f;
	for (auto& ch : chains) for (auto& q : ch)
		q.Set((q.x-ctr[0])*scale, (q.y-ctr[1])*scale, (q.z-ctr[2])*scale);

	// --- tube generique (cf. CreateTubes dans surface_basic) ---
	m_pMesh = CreateTubes(chains, m_thickness, 6);
	m_pMesh->ComputeNormals();
}

// ===========================================================================
//  Architecture gothique
// ===========================================================================

// --- Gothic Block ----------------------------------------------------------
ParameterizedGothicBlock::ParameterizedGothicBlock() { Regenerate(); }
std::vector<Parameter> ParameterizedGothicBlock::GetParameters()
{
	return {
		Parameter::MakeFloat("Width",  &m_width,  0.1f, 20.f),
		Parameter::MakeFloat("Height", &m_height, 0.1f, 20.f),
		Parameter::MakeFloat("Depth",  &m_depth,  0.1f, 20.f),
		Parameter::MakeFloat("Bevel",  &m_bevel,  0.f,  0.5f),
	};
}
void ParameterizedGothicBlock::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateBlock(m_width, m_height, m_depth, m_bevel);
	m_pMesh->ComputeNormals();
}

// --- Gothic Window (remplage parametrique complet) -------------------------
ParameterizedGothicWindow::ParameterizedGothicWindow() { Regenerate(); }

std::vector<Parameter> ParameterizedGothicWindow::GetParameters()
{
	return {
		// arc + corps
		Parameter::MakeFloat("Excess (arch)", &m_excess, 0.7f, 2.5f),
		Parameter::MakeFloat("Width",         &m_width, 20.f, 600.f),
		Parameter::MakeFloat("Body height",   &m_bodyHeight, 0.f, 600.f),
		Parameter::MakeFloat("Offset outer",  &m_offsetOuter, 0.f, 40.f),
		Parameter::MakeFloat("Offset inner",  &m_offsetInner, 0.f, 40.f),
		// lancettes : figé à 2 pour l'instant (le modèle de la these ne definit la
		// rosette/tangence que pour deux sous-fenetres) -> pas exposé dans l'UI.
		Parameter::MakeEnum ("Lancet head",   &m_lancetHead, {"Plain","Foiled"}),
		Parameter::MakeInt  ("Lancet head foils", &m_lancetHeadFoils, 3, 8),
		Parameter::MakeInt  ("Recursion",     &m_recursion, 0, 2),
		Parameter::MakeFloat("Lancet drop",   &m_subDrop, 0.f, 100.f),
		Parameter::MakeFloat("Lancet excess", &m_subExcess, 0.7f, 2.5f),
		Parameter::MakeFloat("Gap fraction",  &m_gapFraction, 0.01f, 0.2f),
		// rosette + foils (les foils de LANCETTE ne sont pas exposes : les
		// lancettes sont de simples arcs brises hauts, cf. buildBayStonePolygon)
		Parameter::MakeBool ("Fillets",             &m_fillets),
		Parameter::MakeBool ("Rosette",             &m_rosette),
		Parameter::MakeBool ("Rosette foils",       &m_rosetteFoils),
		Parameter::MakeInt  ("Rosette foil count",  &m_rosetteFoilCount, 3, 24),
		Parameter::MakeEnum ("Rosette foil type",   &m_rosetteFoilType, {"Round","Pointed"}),
		Parameter::MakeFloat("Rosette pointedness", &m_rosetteFoilPointed, 0.01f, 2.f),
		// extrusion + profil de moulure (trefoil/mouchettes non exposes)
		Parameter::MakeFloat("Extrusion (zHeight)", &m_zHeight, 0.f, 100.f),
		Parameter::MakeEnum ("Profile",             &m_profile, {"Flat","Chamfer"}),
	};
}

void ParameterizedGothicWindow::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;

	auto placeholder = [&]() -> Mesh* {
		std::vector<float>        v = {0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f};
		std::vector<unsigned int> f = {0u,1u,2u};
		Mesh* m = new Mesh();
		m->SetVertices(3, v.data());
		m->SetFaces(1, 3, f.data());
		return m;
	};

	try
	{
		WindowInstance wi;

		// --- arc : pL/pR centres a la base, largeur = m_width ---
		double w = (m_width < 1.0) ? 1.0 : (double)m_width;
		wi.archBasis.pL = Vector2d(-w * 0.5, 0.0);
		wi.archBasis.pR = Vector2d( w * 0.5, 0.0);
		double excess = std::min(std::max((double)m_excess, 0.7), 2.5);
		wi.archBasis.excess = excess;

		wi.archOffset.outer = (m_offsetOuter < 0.f) ? 0.0 : (double)m_offsetOuter;

		// --- lancettes (calculees AVANT l'offset : elles bornent l'epaisseur inner) ---
		// Figé à 2 pour l'instant (cf. these : rosette/tangence definies pour deux
		// sous-fenetres seulement). Le membre m_subCount n'est plus exposé dans l'UI.
		int count = 2;
		wi.subwindowParams.count  = count;
		wi.subwindowParams.drop   = (m_subDrop < 0.f) ? 0.0 : (double)m_subDrop;
		double subExcess = std::min(std::max((double)m_subExcess, 0.7), 2.5);
		wi.subwindowParams.excess = subExcess;
		wi.subwindowParams.gap.mode = SubwindowParams::Gap::Mode::Fraction;
		double gapMax = 0.6 / (count + 1);   // conservateur : garde >= 40% de largeur aux lancettes
		double gf = (double)m_gapFraction;
		if (gf < 0.001) gf = 0.001;
		if (gf > gapMax) gf = gapMax;
		wi.subwindowParams.gap.gapFraction = gf;

		// --- offset inner : borne par l'arc principal ET par les lancettes ---
		// Un inner trop grand fait "collapser" un sous-arc -> contour degenere qui
		// fait BOUCLER la tessellation GLU (non rattrapable par try/catch). On borne
		// donc contre le rayon de lancette (le plus petit), avec marge 0.8.
		// Thesis convention : the concentric inner offset stays a valid arch while
		// inner < openingWidth/2 (main) and < lancetWidth/2 (each lancet). Margin 0.8.
		double lancetWidth    = w * (1.0 - (count + 1) * gf) / (double)count;
		double innerMaxMain   = 0.8 * 0.5 * w;
		double innerMaxLancet = 0.8 * 0.5 * lancetWidth;
		double innerMax = std::min(innerMaxMain, innerMaxLancet);
		double inner    = (m_offsetInner < 0.f) ? 0.0 : (double)m_offsetInner;
		if (innerMax > 0.0 && inner > innerMax) inner = innerMax;
		if (inner < 0.0) inner = 0.0;
		wi.archOffset.inner = inner;

		// --- rosette + foils ---
		// REGLE : une rosette n'existe QUE si elle couronne exactement DEUX lancets
		// (elle est construite tangente a ces deux lancets, cf. gothic1.png). Donc
		// pas de rosette a count != 2 : a count==1 la construction degenere / boucle,
		// et a count>=3 il n'y a pas la paire de lancets a laquelle etre tangente.
		// (La case "Rosette" reste sans effet tant qu'on n'a pas 2 lancets.)
		bool wantRosette = m_rosette && (count == 2);
		wi.hasRosette = wantRosette;
		if (m_rosetteFoils && wantRosette)
		{
			wi.hasRosetteFoils = true;
			wi.rosetteFoils.count = std::min(std::max(m_rosetteFoilCount, 3), 24);
			// Always build the cgmath foil ring as ROUND : the maker rendering
			// builds its own pointed petals (pointedFoilPetals), so we only need
			// count + outerCircle from cgmath. Forcing Round avoids buildFoilRing
			// throwing on extreme pointedness (which made the whole window vanish).
			wi.rosetteFoils.type  = FoilType::Round;
			// phi0 = +pi/2 : premier foil mesure depuis l'axe +Y -> un foil en HAUT
			// (trilobe = 1 en haut + 2 en bas, orientation gothique classique).
			wi.rosetteFoils.phi0 = 1.57079632679489661923;
		}
		// Foils de LANCETTE : plus construits ni exposes. Les lancettes sont de
		// simples arcs brises hauts (fut + tete pointue) ; le remplage folie
		// d'une lancette (grand vide + tete festonnee) exigerait un vrai booleen
		// 2D, cf. buildBayStonePolygon. m_subFoils/... restent en membres.
		wi.hasSubwindowFoils = false;

		// NB : trefoil / mouchettes / fillets NE SONT PAS construits ici. Le
		// constructeur de maillage (buildBayStonePolygon) ne les decoupe pas comme
		// vides (limitation documentee : "NOT covered yet") -> aucun effet visuel,
		// et leur construction geometrique peut boucler sur certains parametres.
		// Ils restent en membres (m_archTrefoil, ...) pour un branchement futur si
		// buildBayStonePolygon apprend a les rendre.

		// --- geometrie -> polygone pierre -> mesh (extrude si zHeight>0) ---
		WindowGeometry geom = buildGeometryFromInstance(wi);

		// La rosette de cgmath est tangente aux arcs de BASE, mais l'ouverture
		// reelle en pierre est l'offset INTERIEUR (mainOffset.inner + sous-arcs
		// offset.inner), plus petit : la rosette de base debordait donc du cadre.
		// On la recalcule tangente a CES arcs interieurs, pour qu'elle touche le
		// lancet principal ET les lancets inferieurs (cf. gothic1.png) au lieu de
		// flotter avec un jour tout autour. Repli sur l'ancien retrecit-a-centre-
		// fixe si la construction tangente degenere.
		if (geom.hasRosette && !geom.subwindows.lancets.empty())
		{
			// Oculus passing through the lancet apexes (touches the drawn lancet
			// tops) and tangent to the main arch. RULE : it exists only if it can
			// REST ON the two lancets -> its centre must be ABOVE their apexes. If
			// the head is too shallow (centre below the apexes, it would hang wedged
			// between the lancets with nothing supporting it), drop the rosette.
			Vector2d apex = geom.subwindows.lancets.back().offset.inner.apex;
			Circle rc = rosetteTangentToLancets(geom.mainOffset.inner, 0.0, apex);
			if (rc.radius > 1.0 && rc.center.y > apex.y)
			{
				geom.rosette.center = rc.center;
				geom.rosette.radius = rc.radius;
				geom.rosette.circle = rc;
				if (geom.hasRosetteFoils)
					geom.rosetteFoils = buildFoilRing(rc, wi.rosetteFoils);
			}
			else
			{
				geom.hasRosette      = false;   // ne peut pas prendre appui
				geom.hasRosetteFoils = false;
			}
		}

		GothicMeshParams mp;
		mp.zHeight    = (m_zHeight    < 0.f) ? 0.0 : (double)m_zHeight;
		mp.bodyHeight = (m_bodyHeight < 0.f) ? 0.0 : (double)m_bodyHeight;
		// Recursion : reuse the same offset + subdivision on each sub-lancet.
		mp.recursionDepth  = std::min(std::max(m_recursion, 0), 2);
		mp.recursionOffset = wi.archOffset;
		mp.recursionSub    = wi.subwindowParams;
		// Sub-rosettes use the SAME motif AND foil count as the main rosette
		// (a sub-window gets one only if it subdivides into exactly two sub-lancets,
		// enforced in collectUnitVoids).
		mp.recursionFoils  = std::min(std::max(m_rosetteFoilCount, 3), 24);
		// Fillets (Phase 1) : corner fields via Clipper2, inset by the inner offset.
		mp.fillets     = m_fillets;
		mp.filletInset = (m_offsetInner < 1.f) ? 4.0 : (double)m_offsetInner;
		// Foiled lancet heads (Phase 3).
		mp.lancetHeadFoiled = (m_lancetHead == 1);
		mp.lancetHeadFoils  = std::min(std::max(m_lancetHeadFoils, 3), 8);
		// Rosette foil shape (Round/Pointed) + pointedness of the pointed petals.
		mp.rosetteFoilType    = m_rosetteFoilType;
		mp.rosettePointedness = std::min(std::max((double)m_rosetteFoilPointed, 0.0), 2.0);
		Polygon2 poly = buildBayStonePolygon(geom, mp);

		Mesh* m = new Mesh();
		if (mp.zHeight > 0.0)
		{
			// Profile "Chamfer" (Phase 2) : bevelled/splayed openings for the 3D
			// carved-stone look. Falls back to a flat extrusion if the per-vertex
			// offset degenerates (thin bars / sharp corners).
			bool profiled = false;
			if (m_profile == 1)
			{
				double chamW = std::min(0.6 * mp.filletInset, 0.45 * (double)m_offsetInner + 2.0);
				if (chamW < 0.5) chamW = 0.5;
				double chamD = std::min(0.5 * mp.zHeight, 3.0 * chamW);
				try { extrudeProfiledToMesh(poly, *m, 0.0, mp.zHeight, chamW, chamD); profiled = true; }
				catch (...) { profiled = false; }
			}
			if (!profiled) extrudeToMesh(poly, *m, 0.0, mp.zHeight);
		}
		else tessellateToMesh(poly, *m, 0.0);
		m->ComputeNormals();
		m_pMesh = m;
	}
	catch (...)
	{
		// Combinaison de parametres invalide (les buildXxx lancent) -> placeholder.
		m_pMesh = placeholder();
	}
}
