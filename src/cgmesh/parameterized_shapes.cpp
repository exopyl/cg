#include "parameterized_shapes.h"

#include "surface_basic.h"
#include "surface_parametric.h"
#include "voxels_menger_sponge.h"
#include "import_svg.h"

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
