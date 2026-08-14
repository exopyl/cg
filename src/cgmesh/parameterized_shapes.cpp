#include "parameterized_shapes.h"

#include "surface_basic.h"
#include "surface_parametric.h"
#include "voxels_menger_sponge.h"
#include "import_svg.h"
#include "image_relief.h"
#include "surface_implicit.h"
#include "surface_implicit_tandem.h"
#include "lsysteminit.h"
#include "surface_architecture.h"
#include "architecture_gothic.h"
#include "extrude_contours.h"
#include "stroke_contours.h"
#include "text_extrude.h"
#include "../cgmath/font.h"
#include <nlohmann/json.hpp>
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
ParameterizedSvgExtrusion::ParameterizedSvgExtrusion(const std::string& filename)
	: m_filename(filename)
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
		Parameter::MakeEnum ("Lancet layout", &m_lancetLayout, {"Uniform","Prototype"}),
		// rosette + foils (les foils de LANCETTE ne sont pas exposes : les
		// lancettes sont de simples arcs brises hauts, cf. buildBayStonePolygon)
		Parameter::MakeBool ("Fillets",             &m_fillets),
		Parameter::MakeBool ("Mouchettes",          &m_mouchettes),
		Parameter::MakeEnum ("Mouchette type",      &m_mouchetteType, {"Vesica","Teardrop","Soufflet"}),
		Parameter::MakeBool ("Rosette",             &m_rosette),
		Parameter::MakeBool ("Rosette foils",       &m_rosetteFoils),
		Parameter::MakeInt  ("Rosette foil count",  &m_rosetteFoilCount, 3, 24),
		Parameter::MakeEnum ("Rosette foil type",   &m_rosetteFoilType, {"Round","Pointed"}),
		Parameter::MakeFloat("Rosette pointedness", &m_rosetteFoilPointed, 0.01f, 2.f),
		// extrusion + profil de moulure (trefoil/mouchettes non exposes)
		Parameter::MakeFloat("Extrusion (zHeight)", &m_zHeight, 0.f, 100.f),
		Parameter::MakeEnum ("Profile",             &m_profile, {"Flat","Chamfer","Roll bar","Keel bar","Ogee bar"}),
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
		wi.subwindowParams.layout = (m_lancetLayout == 1)
			? SubwindowParams::Layout::Prototype
			: SubwindowParams::Layout::Uniform;
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
		mp.maxAngleRad = (m_maxAngleRad > 1e-4) ? m_maxAngleRad : (3.14159265358979323846 / 180.0);
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
		// Flamboyant : shape the spandrel fields as mouchettes/soufflets (needs fillets on).
		// Use a THIN frame bar (small inset) so the field ≈ the full spandrel and the
		// inscribed mouchette nearly fills it, leaving only a slim stone bar around it.
		mp.mouchettes    = m_mouchettes;
		mp.mouchetteType = std::min(std::max(m_mouchetteType, 0), 2);
		mp.mouchetteSize = 0.92;
		if (m_mouchettes) mp.filletInset = std::min(mp.filletInset, 4.0);
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

			// Profile bars (Phase 2b, Havemann §5.4.1 "french style", Fig 5.28 d) : on
			// top of the flat plate, sweep a bar cross-section along the ACTUAL opening
			// outlines (main frame, lancets incl. foiled heads + jambs, recursion sub-
			// tracery, rosette rim) via buildBayMoulding. Profile library : 2 = Roll
			// (half-round bead), 3 = Keel (pointed ridge), 4 = Ogee (symmetric cyma).
			// Corner mitring is deferred (open joints for now).
			if (m_profile >= 2)
			{
				const double PI = 3.14159265358979323846;
				double rb = std::min(0.5 * mp.filletInset, 0.45 * (double)m_offsetInner + 1.5);
				if (rb < 0.8) rb = 0.8;
				const double zF = mp.zHeight;

				// Closed cross-section in (u = z, v = in-plane) : flat base on the front
				// face (z = zF), molding proud toward +z. Symmetric in v (a bar).
				std::vector<Vector2d> profile;
				if (m_profile == 3)          // Keel : pointed ridge
				{
					profile = { Vector2d(zF, rb), Vector2d(zF + 1.2 * rb, 0.0), Vector2d(zF, -rb) };
				}
				else if (m_profile == 4)     // Ogee : symmetric cyma (smoothstep flanks)
				{
					const int ns = 6;
					for (int k = 0; k <= ns; ++k)
					{ double s = (double)k/ns; double h = rb*(3*s*s - 2*s*s*s); profile.push_back(Vector2d(zF + h,  rb*(1.0-s))); }
					for (int k = ns - 1; k >= 0; --k)
					{ double s = (double)k/ns; double h = rb*(3*s*s - 2*s*s*s); profile.push_back(Vector2d(zF + h, -rb*(1.0-s))); }
				}
				else                          // Roll (m_profile==2) : half-round bead
				{
					const int ns = 12;
					for (int k = 0; k <= ns; ++k)
					{ double th = PI * (double)k/ns; profile.push_back(Vector2d(zF + rb*std::sin(th), rb*std::cos(th))); }
				}

				try
				{
					Mesh beads;
					buildBayMoulding(geom, mp, profile, beads);
					appendMesh(*m, beads);
				}
				catch (...) { /* degenerate outline : keep the flat plate only */ }
			}
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

bool ParameterizedGothicWindow::LoadFromJson(const std::string &jsonText)
{
	using nlohmann::json;
	json j;
	try { j = json::parse(jsonText); }
	catch (...) { return false; }

	auto profileIndex = [](const std::string &t) -> int {
		if (t == "flat")    return 0;
		if (t == "chamfer") return 1;
		if (t == "roll")    return 2;
		if (t == "keel")    return 3;
		if (t == "ogee")    return 4;
		return 2;   // default : roll bar
	};

	try
	{
		// --- geometry (Fig 5.30) -----------------------------------------
		if (j.contains("geometry") && j["geometry"].is_object())
		{
			const json &g = j["geometry"];
			m_width       = (float) g.value("width",   (double)m_width);
			m_excess      = (float) g.value("excess",  (double)m_excess);
			m_offsetOuter = (float) g.value("bdOuter", (double)m_offsetOuter);
			m_offsetInner = (float) g.value("bdInner", (double)m_offsetInner);

			// heightBott = niveau Y du bas (springline a y=0) -> body height = -heightBott.
			if (g.contains("heightBott") && g["heightBott"].is_number())
			{
				double body = -g["heightBott"].get<double>();
				m_bodyHeight = (float)(body > 0.0 ? body : 0.0);
			}
			// kseg = segments par cercle complet -> pas angulaire.
			if (g.contains("kseg") && g["kseg"].is_number())
			{
				double k = g["kseg"].get<double>();
				if (k >= 3.0) m_maxAngleRad = 2.0 * 3.14159265358979323846 / k;
			}
			if (g.contains("subwindows") && g["subwindows"].is_object())
			{
				const json &s = g["subwindows"];
				m_subCount  =        s.value("count",   m_subCount);
				m_subDrop   = (float) s.value("arcDown", (double)m_subDrop);
				m_subExcess = (float) s.value("excess",  (double)m_subExcess);
				m_lancetLayout = (s.value("layout", std::string("prototype")) == "uniform") ? 0 : 1;
				if (s.contains("gap") && s["gap"].is_object())
					m_gapFraction = (float) s["gap"].value("gapFraction", (double)m_gapFraction);
			}
		}

		// --- style (dict de 4 fonctions de champ) ------------------------
		if (j.contains("style") && j["style"].is_object())
		{
			const json &st = j["style"];
			// Profil : aujourd'hui GLOBAL -> pris sur mainArch (§5.2 pour le par-champ).
			if (st.contains("mainArch") && st["mainArch"].is_object())
			{
				const json &ma = st["mainArch"];
				if (ma.contains("profile") && ma["profile"].is_object())
					m_profile = profileIndex(ma["profile"].value("type", std::string("roll")));
				if (ma.contains("trefoil") && ma["trefoil"].is_object())
				{
					const json &tf = ma["trefoil"];
					m_archTrefoil       =        tf.value("enabled", m_archTrefoil);
					m_trefoilSplit      = (float) tf.value("splitParameter",   (double)m_trefoilSplit);
					m_trefoilFoilRadius = (float) tf.value("foilRadiusFactor", (double)m_trefoilFoilRadius);
				}
			}
			if (st.contains("subArch") && st["subArch"].is_object()
			    && st["subArch"].contains("head") && st["subArch"]["head"].is_object())
			{
				const json &h = st["subArch"]["head"];
				m_lancetHead      = (h.value("type", std::string("foiled")) == "plain") ? 0 : 1;
				m_lancetHeadFoils = h.value("foils", m_lancetHeadFoils);
			}
			if (st.contains("rosette") && st["rosette"].is_object())
			{
				const json &r = st["rosette"];
				m_rosette = r.value("present", m_rosette);
				if (r.contains("foils") && r["foils"].is_object())
				{
					const json &f = r["foils"];
					m_rosetteFoils       = true;
					m_rosetteFoilCount   = f.value("count", m_rosetteFoilCount);
					m_rosetteFoilType    = (f.value("type", std::string("round")) == "pointed") ? 1 : 0;
					m_rosetteFoilPointed = (float) f.value("pointedness", (double)m_rosetteFoilPointed);
				}
				else m_rosetteFoils = false;
			}
			if (st.contains("fillet") && st["fillet"].is_object())
			{
				const json &fi = st["fillet"];
				m_fillets    = true;
				m_mouchettes = (fi.value("fill", std::string("plain")) == "mouchette");
				if (fi.contains("mouchette") && fi["mouchette"].is_object())
				{
					std::string mt = fi["mouchette"].value("type", std::string("soufflet"));
					m_mouchetteType = (mt == "vesica") ? 0 : (mt == "teardrop") ? 1 : 2;
				}
			}
		}

		// --- recursion / extrusion --------------------------------------
		if (j.contains("recursion") && j["recursion"].is_object())
			m_recursion = j["recursion"].value("depth", m_recursion);
		if (j.contains("extrusion") && j["extrusion"].is_object())
			m_zHeight = (float) j["extrusion"].value("zHeight", (double)m_zHeight);
	}
	catch (...) { return false; }

	return true;
}

std::string ParameterizedGothicWindow::ExportJson() const
{
	using nlohmann::json;
	auto R = [](double v) { return std::round(v * 1e4) / 1e4; };   // arrondi lisible
	auto profileName = [](int p) -> std::string {
		switch (p) { case 0: return "flat"; case 1: return "chamfer";
		             case 3: return "keel"; case 4: return "ogee"; default: return "roll"; }
	};
	const std::string prof = profileName(m_profile);   // profil GLOBAL (émis par champ)

	json j;
	j["schema"] = "gothic-window/v2";

	json &g = j["geometry"];
	g["width"]      = R(m_width);
	g["excess"]     = R(m_excess);
	g["bdOuter"]    = R(m_offsetOuter);
	g["bdInner"]    = R(m_offsetInner);
	g["heightBott"] = R(-m_bodyHeight);   // inverse de LoadFromJson (bodyHeight = -heightBott)
	if (m_maxAngleRad > 1e-6)
		g["kseg"] = (int) std::lround(2.0 * 3.14159265358979323846 / m_maxAngleRad);

	json &sw = g["subwindows"];
	sw["count"]   = m_subCount;
	sw["arcDown"] = R(m_subDrop);
	sw["excess"]  = R(m_subExcess);
	sw["layout"]  = (m_lancetLayout == 0) ? "uniform" : "prototype";
	if (m_lancetLayout == 0)
		sw["gap"] = json{ {"mode", "fraction"}, {"gapFraction", R(m_gapFraction)} };

	json &st = j["style"];
	st["mainArch"]["profile"]["type"] = prof;
	st["mainArch"]["trefoil"] = json{
		{"enabled", m_archTrefoil},
		{"splitParameter", R(m_trefoilSplit)},
		{"foilRadiusFactor", R(m_trefoilFoilRadius)} };
	st["subArch"]["profile"]["type"] = prof;
	st["subArch"]["head"] = json{
		{"type", (m_lancetHead == 0) ? "plain" : "foiled"},
		{"foils", m_lancetHeadFoils} };
	json &ros = st["rosette"];
	ros["present"] = m_rosette;
	ros["profile"]["type"] = prof;
	if (m_rosetteFoils)
		ros["foils"] = json{
			{"count", m_rosetteFoilCount},
			{"type", (m_rosetteFoilType == 1) ? "pointed" : "round"},
			{"pointedness", R(m_rosetteFoilPointed)},
			{"orientation", "standing"} };
	json &fi = st["fillet"];
	fi["fill"] = m_mouchettes ? "mouchette" : "plain";
	fi["mouchette"] = json{
		{"type", (m_mouchetteType == 0) ? "vesica" : (m_mouchetteType == 1) ? "teardrop" : "soufflet"},
		{"size", 0.85} };

	j["recursion"]["depth"] = m_recursion;
	j["extrusion"]["zHeight"] = R(m_zHeight);

	return j.dump(2);
}
