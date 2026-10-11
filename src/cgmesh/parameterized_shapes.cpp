#include "parameterized_shapes.h"

#include "surface_basic.h"
#include "surface_parametric.h"
#include "voxels_menger_sponge.h"
#include "import_svg.h"
#include "image_relief.h"
#include "surface_implicit.h"
#include "surface_implicit_tandem.h"
#include "lsysteminit.h"
#include "extrude_contours.h"
#include "stroke_contours.h"
#include "text_extrude.h"
#include "mesh_solidify.h"
#include <cgmath/font.h>
#include <cmath>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <map>
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
	for (unsigned int i = 0; i < m_pMesh->GetNVertices (); i++)
	{
		m_pMesh->SetVertexComponent (i, 0, m_pMesh->GetVertices ()[3*i+0] * (scale));
		m_pMesh->SetVertexComponent (i, 1, m_pMesh->GetVertices ()[3*i+1] * (scale));
		m_pMesh->SetVertexComponent (i, 2, m_pMesh->GetVertices ()[3*i+2] * (scale));
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

// --- Seashell ----------------------------------------------------------------
ParameterizedSeashell::ParameterizedSeashell() { Regenerate(); }
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

// --- Seashell von Seggern ----------------------------------------------------
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

// --- Klein Bottle ------------------------------------------------------------
ParameterizedKleinBottle::ParameterizedKleinBottle() { Regenerate(); }
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

// --- Breather ----------------------------------------------------------------
ParameterizedBreather::ParameterizedBreather() { Regenerate(); }
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

// --- Hyperbolic Paraboloid ---------------------------------------------------
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

// --- Monkey Saddle -----------------------------------------------------------
ParameterizedMonkeySaddle::ParameterizedMonkeySaddle() { Regenerate(); }
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

// --- Blobs -------------------------------------------------------------------
ParameterizedBlobs::ParameterizedBlobs() { Regenerate(); }
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

// --- Drop --------------------------------------------------------------------
ParameterizedDrop::ParameterizedDrop() { Regenerate(); }
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

// --- Torus Knot --------------------------------------------------------------
ParameterizedTorusKnot::ParameterizedTorusKnot() { Regenerate(); }
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

// --- Cinquefoil Knot ---------------------------------------------------------
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

// --- Trefoil Knot ------------------------------------------------------------
ParameterizedTrefoilKnot::ParameterizedTrefoilKnot() { Regenerate(); }
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

// --- Borromean Rings ---------------------------------------------------------
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
ParameterizedSvgExtrusion::ParameterizedSvgExtrusion(const std::string& filename,
                                                     bool perShapeMaterials)
	: m_filename(filename), m_perShapeMaterials(perShapeMaterials)
{
	Regenerate();
}
std::vector<Parameter> ParameterizedSvgExtrusion::GetParameters()
{
	return {
		// Plage resserree a 0.01 .. 0.1. Le maillage etant recentre et normalise a
		// 1.0 dans sa plus grande dimension (SvgExtrudeOptions::centerAndFit), une
		// hauteur se lit comme une FRACTION de cette taille : au-dela de 0.1 on
		// obtient une dalle, pas une plaque gravee. L'ancien maximum de 10 donnait
		// une course de curseur ou toute la plage utile tenait dans le premier
		// centieme.
		Parameter::MakeFloat("Height",         &m_height,     0.01f, 0.1f),
		// Meme cause, meme remede que ci-dessus, et meme plage que le texte 3D :
		// la tolerance est desormais exprimee dans les unites du maillage produit
		// (SvgExtrudeOptions::flattenTol), donc en fraction de l'objet normalise.
		// L'ancienne plage 0.05 .. 10 etait en unites du DOCUMENT : selon le
		// fichier importe, la meme course de curseur allait de « toujours trop
		// fin » a « 42 % d'ecart des le milieu ».
		Parameter::MakeFloat("Flatten Tol",    &m_flattenTol, 0.001f, 0.1f),
		Parameter::MakeBool ("Per-shape materials", &m_perShapeMaterials),
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
	opt.perShapeMaterials = m_perShapeMaterials;
	m_pMesh = import_svg_extruded(m_filename, opt);
	if (m_pMesh)
		m_pMesh->ComputeNormals();
}

// ===========================================================================
// Image -> coloured relief
// ===========================================================================

ParameterizedImageRelief::ParameterizedImageRelief(const std::string& filename)
	: m_filename(filename)
{
	Regenerate();
}

std::vector<Parameter> ParameterizedImageRelief::GetParameters()
{
	return {
		// Borne haute a 10 et non a la limite technique de la palette : au-dela, les
		// couleurs supplementaires se depensent en degrades du fond et en regions
		// minuscules, que la simplification de contour et l anti-mouchetis effacent
		// ensuite. Du temps de calcul paye pour du detail jete.
		Parameter::MakeInt  ("Max colors",      &m_maxColors, 2, 10),
		Parameter::MakeEnum ("Quantization",    &m_algo, { "Wu", "Heckbert" }),
		Parameter::MakeFloat("Simplify err",    &m_simplifyErr,   0.1f,  5.f),
		Parameter::MakeInt  ("Pre-smooth",      &m_preSmooth,     0,     4),
		Parameter::MakeInt  ("Refine palette",  &m_refine,        0,     12),
		Parameter::MakeInt  ("Despeckle",       &m_despeckle,     0,     6),
		Parameter::MakeInt  ("Min region area", &m_minRegionArea, 0,     400),
		// En pixels de l image REDUITE (512 px de cote au plus) : un pixel y vaut deja
		// plusieurs pixels source, donc 1 suffit a degager le jeu entre regions.
		Parameter::MakeFloat("Shrink (px)",     &m_shrink,        0.f,   1.f),
		Parameter::MakeFloat("Fit size",        &m_fitSize,       0.1f,  100.f),
		// Emprise du contenu ramenee a `fitSize` (1 par defaut) : au-dela de 1 le
		// relief est plus haut que large, ce qui n a plus de sens pour une plaque.
		Parameter::MakeFloat("Block height",    &m_blockHeight,   0.001f, 1.f),
		Parameter::MakeFloat("Base thickness",  &m_baseThickness, 0.001f, 20.f),
		Parameter::MakeFloat("Margin",          &m_margin,        0.f,    20.f),
		Parameter::MakeFloat("Wall thickness",  &m_wallThickness, 0.001f, 20.f),
		// Meme raison que Block height : borne a l emprise du contenu.
		Parameter::MakeFloat("Wall height",     &m_wallHeight,    0.001f, 1.f),
		Parameter::MakeBool ("Internal walls",  &m_internalWalls),
		// Le cadre est un accessoire de presentation : decochable, pour qu'un export
		// ne contienne que les regions extrudees.
		Parameter::MakeBool ("Base plate",      &m_emitBase),
		Parameter::MakeBool ("Perimeter wall",  &m_emitWall),
	};
}

void ParameterizedImageRelief::Regenerate()
{
	delete m_pMesh;

	ImageReliefOptions opt;
	opt.maxColors     = m_maxColors;
	opt.algo          = (m_algo == 1) ? QuantAlgo::Heckbert : QuantAlgo::Wu;
	opt.simplifyErr      = m_simplifyErr;
	opt.preSmoothPasses  = m_preSmooth;
	opt.refineIterations = m_refine;
	opt.despecklePasses  = m_despeckle;
	opt.minRegionArea    = m_minRegionArea;
	opt.shrink           = m_shrink;
	opt.fitSize          = m_fitSize;
	opt.blockHeight   = m_blockHeight;
	opt.baseThickness = m_baseThickness;
	opt.margin        = m_margin;
	opt.wallThickness = m_wallThickness;
	opt.wallHeight    = m_wallHeight;
	opt.emitInternalWalls = m_internalWalls;
	opt.emitBase      = m_emitBase;
	opt.emitWall      = m_emitWall;

	// image_to_relief() already computes the normals and the bbox.
	m_pMesh = image_to_relief(m_filename, opt);
}

// ===========================================================================
// Texte 3D
// ===========================================================================

ParameterizedText3D::ParameterizedText3D(const std::string& fontFilename,
                                         const std::string& text)
	: m_pFont(new Font)
	, m_text(text)
{
	// Un echec de chargement n'est PAS fatal ici : l'objet existe, Regenerate()
	// ne produira simplement aucun maillage. L'appelant le constate par
	// TakeMesh() == nullptr, comme pour un SVG illisible.
	m_pFont->loadFromFile(fontFilename);
	Regenerate();
}

// Hors-ligne parce que Font est incomplet dans l'en-tete : le destructeur de
// unique_ptr<Font> a besoin du type complet, et c'est ici qu'il l'a.
ParameterizedText3D::~ParameterizedText3D() = default;

bool ParameterizedText3D::IsFontLoaded() const { return m_pFont && m_pFont->isValid(); }

int ParameterizedText3D::GetKerningStatus() const
{
	return m_pFont ? (int)m_pFont->kerningStatus() : (int)KerningStatus::None;
}

std::vector<Parameter> ParameterizedText3D::GetParameters()
{
	return {
		// LE parametre de cette forme : la chaine a extruder. Multiligne, un
		// retour a la ligne ouvrant une nouvelle ligne de texte (text_layout.cpp).
		// La police, elle, reste fixee a la construction -- c'est un FICHIER, du
		// meme ordre que le SVG de ParameterizedSvgExtrusion, pas une valeur que
		// l'on regle au curseur.
		Parameter::MakeString("Text",          &m_text,          /*multiline*/ true),
		// Le corps typographique, en unites du maillage.
		Parameter::MakeFloat("Size",           &m_size,          0.1f,  10.f),
		// Borne haute a une demi-hauteur d'em : au-dela le texte devient une
		// barre dont on ne lit plus la lettre de face.
		Parameter::MakeFloat("Depth",          &m_depth,         0.01f, 0.5f),
		// En unites MONDE, et non en unites de police : la finesse ne depend donc
		// ni de l'em de la police ni de la taille demandee. A 1/100e du corps un
		// contour est deja visuellement lisse ; en dessous de 1/1000e on paie des
		// sommets que l'oeil ne distingue plus.
		Parameter::MakeFloat("Flatten tol",    &m_flattenTol,    0.001f, 0.1f),
		Parameter::MakeFloat("Letter spacing", &m_letterSpacing, -0.5f, 0.5f),
		Parameter::MakeFloat("Line spacing",   &m_lineSpacing,    0.5f, 3.f),
		Parameter::MakeEnum ("Align",          &m_align, { "Left", "Center", "Right" }),
		// Sans effet quand la police porte son crenage dans un format que nous ne
		// savons pas lire (cf. GetKerningStatus).
		Parameter::MakeBool ("Kerning",        &m_kerning),
		// Couteux : ne sert qu'aux interlettrages tres serres, ou les glyphes se
		// recouvrent reellement.
		Parameter::MakeBool ("Union overlaps", &m_unionOverlaps),
		Parameter::MakeBool ("Center",         &m_centerOnOrigin),
	};
}

void ParameterizedText3D::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;
	if (!IsFontLoaded()) return;

	TextExtrudeOptions opt;
	opt.size           = m_size;
	opt.depth          = m_depth;
	opt.flattenTol     = m_flattenTol;
	opt.letterSpacing  = m_letterSpacing;
	opt.lineSpacing    = m_lineSpacing;
	opt.align          = (m_align == 1) ? TextAlign::Center
	                   : (m_align == 2) ? TextAlign::Right
	                                    : TextAlign::Left;
	opt.kerning        = m_kerning;
	opt.unionOverlaps  = m_unionOverlaps;
	opt.centerOnOrigin = m_centerOnOrigin;

	// text_to_extruded_mesh() calcule deja les normales (ExtrudedMeshBuilder::Build).
	m_pMesh = text_to_extruded_mesh(*m_pFont, m_text, opt);
}

// ===========================================================================
// Image -> blocs pixelises
// ===========================================================================

ParameterizedImagePixelBlocks::ParameterizedImagePixelBlocks(const std::string& filename)
	: m_filename(filename)
{
	Regenerate();
}

std::vector<Parameter> ParameterizedImagePixelBlocks::GetParameters()
{
	return {
		// En tete : c'est LE reglage de la brique. Borne haute a 256 pour garder le
		// nombre de blocs -- donc le poids du maillage -- dans des eaux navigables.
		Parameter::MakeInt  ("Pixel width",     &m_pixelWidth,    4,     256),
		Parameter::MakeInt  ("Max colors",      &m_maxColors,     2,     64),
		Parameter::MakeEnum ("Quantization",    &m_algo, { "Wu", "Heckbert" }),
		Parameter::MakeInt  ("Pre-smooth",      &m_preSmooth,     0,     4),
		Parameter::MakeInt  ("Refine palette",  &m_refine,        0,     12),
		// En CELLULES de sortie, pas en pixels source.
		Parameter::MakeInt  ("Despeckle",       &m_despeckle,     0,     6),
		Parameter::MakeInt  ("Min region area", &m_minRegionArea, 0,     64),
		// Borne haute a 0.2 cellule : au-dela le sillon (2*shrink) devore les
		// blocs les plus fins, qui disparaissent du maillage.
		Parameter::MakeFloat("Shrink (cells)",  &m_shrink,        0.f,   0.2f),
		Parameter::MakeFloat("Fit size",        &m_fitSize,       0.1f,  100.f),
		Parameter::MakeFloat("Block height",    &m_blockHeight,   0.001f, 20.f),
		Parameter::MakeFloat("Base thickness",  &m_baseThickness, 0.001f, 20.f),
		Parameter::MakeFloat("Margin",          &m_margin,        0.f,    20.f),
		Parameter::MakeFloat("Wall thickness",  &m_wallThickness, 0.001f, 20.f),
		Parameter::MakeFloat("Wall height",     &m_wallHeight,    0.001f, 20.f),
		Parameter::MakeBool ("Internal walls",  &m_internalWalls),
		// Decocher pour n'exporter que les blocs, sans le plateau qui les encadre.
		Parameter::MakeBool ("Base plate",      &m_emitBase),
		Parameter::MakeBool ("Perimeter wall",  &m_emitWall),
	};
}

ImagePixelBlocksOptions ParameterizedImagePixelBlocks::GetOptions() const
{
	ImagePixelBlocksOptions opt;
	opt.pixelWidth       = m_pixelWidth;
	opt.maxColors        = m_maxColors;
	opt.algo             = (m_algo == 1) ? QuantAlgo::Heckbert : QuantAlgo::Wu;
	opt.preSmoothPasses  = m_preSmooth;
	opt.refineIterations = m_refine;
	opt.despecklePasses  = m_despeckle;
	opt.minRegionArea    = m_minRegionArea;
	opt.shrink           = m_shrink;
	opt.fitSize          = m_fitSize;
	opt.blockHeight      = m_blockHeight;
	opt.baseThickness    = m_baseThickness;
	opt.margin           = m_margin;
	opt.wallThickness    = m_wallThickness;
	opt.wallHeight       = m_wallHeight;
	opt.emitInternalWalls = m_internalWalls;
	// Ces deux-la existaient dans les options mais n'etaient jamais cablees : le
	// cadre etait donc toujours emis, y compris a l'export.
	opt.emitBase         = m_emitBase;
	opt.emitWall         = m_emitWall;
	// workingMaxDim garde sa valeur par defaut (1024) : c'est ce qui borne le cout
	// du lissage et de la quantification sur une grande source.
	return opt;
}

void ParameterizedImagePixelBlocks::Regenerate()
{
	delete m_pMesh;
	// Maillage d'AFFICHAGE : un materiau par couleur de palette, l'image reste
	// lisible. La decoupe en blocs separables passe par
	// image_to_pixel_blocks_per_component() (export).
	m_pMesh = image_to_pixel_blocks(m_filename, GetOptions());
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
		m_field.Build(tmp.GetVertices ().data(), (int)tmp.GetNVertices());

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

// Plafond de recursions, par systeme.
//
// Le nombre de segments croit comme k^n, ou k est le facteur de reecriture de la
// regle, et k va de 2 a une vingtaine selon le systeme. Une valeur unique ne peut
// donc pas convenir : elle tronquerait les uns et brimerait les autres. Deux
// contraintes fixent chaque plafond.
//
// 1. MAX_SEG. Regenerate() arrete la marche a 60 000 segments, et le fait EN
//    SILENCE : la figure sort incomplete sans que rien ne le signale. Sur un axiome
//    "F+F+F+F" le trace compte 4 x k^n segments, et pour les regles les plus
//    prolifiques la recursion suivante depasse deja le plafond :
//
//        Cross B                 k =  5 -> 12 500 a n=5,   62 500 a n=6
//        Board                   k =  7 ->  9 604 a n=4,   67 228 a n=5
//        Koch Curve              k =  8 -> 16 384 a n=4,  131 072 a n=5
//        Rings                   k =  8 -> 16 384 a n=4,  131 072 a n=5
//        Quadratic Koch island A k =  9 -> 26 244 a n=4,  236 196 a n=5
//        Quadratic Koch island B k = 18 -> 23 328 a n=3,  419 904 a n=4
//
//    Quadratic Gosper deborde de meme, avec une vingtaine de non-terminaux par
//    regle : n=3 frole le plafond, n=4 le depasse d'un ordre de grandeur.
//
// 2. La lisibilite. Cross A, Peano curve, Hexagonal Gosper, les buissons 1 a 3 et
//    les plantes 1 et 3 tiendraient une recursion de plus sous MAX_SEG -- Bush3 par
//    exemple monte a 59 049 a n=5 --, mais leur trace remplit alors son enveloppe au
//    point de ne plus rien montrer. Plant1, dont la regle porte SEIZE F, sature des
//    la troisieme.
//
//    Plant2 est a l'oppose : sa regle reconduit un seul X et ajoute dix-huit F, donc
//    une croissance LINEAIRE -- une centaine de segments a six recursions. Elle
//    reste au defaut, faute de raison de la brider.
//
// Hilbert curve 3D est le cas extreme, plafonne a UNE recursion : ses quatre regles
// comptent une dizaine de F chacune, et le cube s'y remplit d'un coup. En mode
// Extrusion la marche repliee sur elle-meme coute en plus tres cher a l'union
// Clipper2 -- une quinzaine de secondes des la deuxieme recursion.
//
// A l'inverse, deux systemes croissent LENTEMENT et ont besoin de plus de
// recursions pour prendre forme, d'ou un plafond releve au-dessus du defaut :
//
//        Sierpinski Arrowhead    X -> YF+XF+Y, Y -> XF-YF-X : 3^n
//        Dragon curve            X -> X+YF+,   Y -> -FX-Y   : 2^n
int LSystemMaxIterations (int system)
{
	switch (system)
	{
	case LSYSTEM_HILBERT_CURVE_3D:        return 1;
	case LSYSTEM_PLANT1:                  return 2;
	case LSYSTEM_QUADRATIC_KOCH_ISLAND_B: return 3;
	case LSYSTEM_PLANT3:                  return 3;
	case LSYSTEM_QUADRATIC_GOSPER:        return 3;
	case LSYSTEM_BOARD:                   return 4;
	case LSYSTEM_KOCH_CURVE:              return 4;
	case LSYSTEM_QUADRATIC_KOCH_ISLAND_A: return 4;
	case LSYSTEM_PEANO_CURVE:             return 4;
	case LSYSTEM_HEXAGONAL_GOSPER:        return 4;
	case LSYSTEM_CROSS_A:                 return 4;
	case LSYSTEM_RINGS:                   return 4;
	case LSYSTEM_BUSH1:                   return 4;
	case LSYSTEM_BUSH2:                   return 4;
	case LSYSTEM_BUSH3:                   return 4;
	case LSYSTEM_CROSS_B:                 return 5;
	case LSYSTEM_BUSH4:                   return 6;   // le defaut, explicites pour memoire
	case LSYSTEM_PLANT2:                  return 6;
	case LSYSTEM_SIERPINSKI_ARROWHEAD:    return 7;
	case LSYSTEM_DRAGON_CURVE:            return 9;
	default:                              return 6;
	}
}

} // namespace

ParameterizedLSystem::ParameterizedLSystem() { Regenerate(); }

std::vector<Parameter> ParameterizedLSystem::GetParameters()
{
	return {
		Parameter::MakeEnum("Mode", &m_mode, {"Tube", "Extrusion"}),
		Parameter::MakeEnum("System", &m_system, LSystemCatalogueNames()),
		// Borne haute dependante du systeme choisi. Elle n'est lue qu'a la
		// construction du panneau, qui ne se reconstruit pas au changement de
		// systeme : c'est le meme plafond, applique dans Regenerate(), qui fait
		// vraiment foi.
		Parameter::MakeInt("Iterations", &m_iterations, 0, LSystemMaxIterations(m_system)),
		// Rayon du tube, ou demi-largeur du trait epaissi : la meme grandeur
		// geometrique dans les deux modes, en unites du maillage normalise.
		Parameter::MakeFloat("Thickness", &m_thickness, 0.005f, 0.3f),
		// Mode Extrusion : sans effet sur le tube, mais laisses visibles plutot
		// que masques -- le panneau est construit une fois a la liaison, il ne se
		// reconstruit pas au changement de mode.
		Parameter::MakeFloat("Height", &m_height, 0.01f, 0.5f),
		Parameter::MakeEnum("Join", &m_join, {"Round", "Miter", "Bevel"}),
		Parameter::MakeEnum("Cap",  &m_cap,  {"Round", "Square", "Butt"}),
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
	// Borne l'expansion exponentielle de Next(). On plafonne la COPIE, pas le
	// membre : le reglage de l'utilisateur reste celui qu'il a pose, et redescendre
	// vers un systeme moins prolifique le retrouve.
	int iters = m_iterations;
	if (iters < 0) iters = 0;
	const int maxIters = LSystemMaxIterations(m_system);
	if (iters > maxIters) iters = maxIters;
	for (int k = 0; k < iters; ++k) ls->Next();

	// A lire MAINTENANT : cleanup() detruit le catalogue avant la construction du
	// maillage.
	const bool traceIsClosed = it->second->bClosed;

	// Une extrusion est PLANE : on garde l'interpretation 2D meme pour un systeme
	// 3D, projeter la marche tortue n'aurait pas de sens.
	const bool is3D = (m_system >= LSYSTEM_HILBERT_CURVE_3D) && (m_mode == 0);
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

	if (m_mode == 1)
	{
		m_pMesh = BuildExtrudedTrace(chains, traceIsClosed);
		m_pMesh->ComputeNormals();
		return;
	}

	// --- tube generique (cf. CreateTubes dans surface_basic) ---
	m_pMesh = CreateTubes(chains, m_thickness, 6);
	m_pMesh->ComputeNormals();
}

// Mode Extrusion : le trace, deja normalise (diagonale de bbox = 4), est ramene
// au plan XY puis extrude sur `Height`.
//
// `m_thickness` est la DEMI-largeur du trait -- exactement ce qu'il est comme
// rayon du tube dans l'autre mode --, donc une largeur de 2*m_thickness. Il est
// applique APRES la normalisation, l'echelle dans laquelle le parametre est
// defini.
Mesh* ParameterizedLSystem::BuildExtrudedTrace (const std::vector<std::vector<Vector3f>>& chains,
                                                bool traceIsClosed)
{
	std::vector<std::vector<std::array<float, 2>>> polylines;
	polylines.reserve(chains.size());
	for (const auto& ch : chains)
	{
		if (ch.size() < 2) continue;
		std::vector<std::array<float, 2>> pl;
		pl.reserve(ch.size());
		for (const auto& q : ch) pl.push_back({ q.x, q.y });
		polylines.push_back(std::move(pl));
	}

	std::vector<std::vector<std::array<float, 2>>> contours;
	if (traceIsClosed)
	{
		// Le trace delimite deja une surface : la tesseler telle quelle.
		// L'epaissir en donnerait le contour creux au lieu du plein.
		contours = std::move(polylines);
	}
	else
	{
		const StrokeJoin join = (m_join == 1) ? StrokeJoin::Miter
		                      : (m_join == 2) ? StrokeJoin::Bevel
		                                      : StrokeJoin::Round;
		const StrokeCap  cap  = (m_cap == 1) ? StrokeCap::Square
		                      : (m_cap == 2) ? StrokeCap::Butt
		                                     : StrokeCap::Round;
		contours = strokeToContours(polylines, 2.f * m_thickness, join, cap);
	}

	std::vector<ExtrudeContour> ec;
	ec.reserve(contours.size());
	for (const auto& c : contours)
	{
		if (c.size() < 3) continue;
		ExtrudeContour e;
		e.pts.reserve(c.size());
		for (const auto& p : c) e.pts.emplace_back(p[0], p[1]);
		ec.push_back(std::move(e));
	}
	if (ec.empty()) return new Mesh();  // vide mais valide, comme le repli du tube

	ExtrudeAppendOptions ao;
	ao.zBottom = 0.f;
	ao.zTop    = (m_height > 0.f) ? m_height : 0.01f;
	ao.winding = ExtrudeWinding::NonZero;
	// Orientation prise telle quelle : celle de Clipper2 pour un trace epaissi
	// (enveloppes positives, trous inverses), celle du parcours pour un trace
	// ferme. La renormaliser remplirait les trous.
	ao.normalizeOrientation = false;

	ExtrudedMeshBuilder builder;
	if (!builder.Append(ec, ao) || builder.Empty()) return new Mesh();
	return builder.Build();
}

// --- Extrusion (decorateur) ------------------------------------------------
ParameterizedExtruded::ParameterizedExtruded(std::unique_ptr<IParameterized> inner)
	: m_inner(std::move(inner))
{
	Regenerate();
}

std::vector<Parameter> ParameterizedExtruded::GetParameters()
{
	std::vector<Parameter> params = m_inner ? m_inner->GetParameters() : std::vector<Parameter>();
	params.push_back(Parameter::MakeBool("Extrude", &m_enabled));
	params.push_back(Parameter::MakeFloat("Extrude distance", &m_distance, 0.001f, 5.f));
	return params;
}

// Valeurs courantes des parametres du generateur, en texte : un seul type pour
// les cinq genres de Parameter, et une egalite exacte (pas de tolerance -- une
// valeur ecrite par l'UI puis relue est bit a bit la meme).
std::vector<std::string> ParameterizedExtruded::InnerSnapshot()
{
	std::vector<std::string> values;
	if (!m_inner) return values;
	for (const Parameter &p : m_inner->GetParameters())
	{
		switch (p.GetType())
		{
		case Parameter::INT:
		case Parameter::ENUM:   values.push_back(std::to_string(p.GetInt())); break;
		case Parameter::FLOAT:  values.push_back(std::to_string(p.GetFloat())); break;
		case Parameter::BOOL:   values.push_back(p.GetBool() ? "1" : "0"); break;
		case Parameter::STRING: values.push_back(p.GetString()); break;
		}
	}
	return values;
}

void ParameterizedExtruded::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;
	ParameterizedMesh *inner = dynamic_cast<ParameterizedMesh*>(m_inner.get());
	if (!inner) return;

	// Le constructeur de chaque forme a deja appele Regenerate() : au premier
	// passage, le maillage du generateur est a jour tel quel.
	std::vector<std::string> snapshot = InnerSnapshot();
	if (m_innerBuilt && snapshot != m_lastInner)
		inner->Regenerate();
	m_lastInner = std::move(snapshot);
	m_innerBuilt = true;

	const Mesh *src = inner->GetMesh();
	if (!src) return;
	m_pMesh = new Mesh();
	if (m_enabled)
		SolidifyMesh(*src, m_distance, *m_pMesh);   // recopie src s'il echoue
	else
		*m_pMesh = *src;
}
