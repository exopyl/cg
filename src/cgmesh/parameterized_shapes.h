#pragma once
#include "parameterized.h"
#include "mesh.h"
#include "surface_implicit_pointcloud.h"

//
// Common base for parameterized objects that own a Mesh*. Handles
// destruction and TakeMesh() boilerplate so subclasses only need to
// implement GetParameters(), Regenerate() and GetName().
//
class ParameterizedMesh : public IParameterized
{
public:
	~ParameterizedMesh() override { delete m_pMesh; }
	Mesh* GetMesh() { return m_pMesh; }
	Mesh* TakeMesh() override { Mesh *m = m_pMesh; m_pMesh = nullptr; return m; }

protected:
	Mesh *m_pMesh = nullptr;
};

// ---------------------------------------------------------------------------
// Basic shapes
// ---------------------------------------------------------------------------

class ParameterizedCube : public ParameterizedMesh
{
public:
	ParameterizedCube();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cube"; }
private:
	float m_edgeLength = 1.f;
	bool m_triangulated = true;
};

class ParameterizedSphere : public ParameterizedMesh
{
public:
	ParameterizedSphere();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Sphere"; }
private:
	int m_nu = 20, m_nv = 20;
};

class ParameterizedCylinder : public ParameterizedMesh
{
public:
	ParameterizedCylinder();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cylinder"; }
private:
	float m_height = 2.f, m_radius = 1.f;
	int m_nVertices = 32;
	bool m_cap = true;
};

class ParameterizedCone : public ParameterizedMesh
{
public:
	ParameterizedCone();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cone"; }
private:
	float m_height = 2.f, m_radius = 1.f;
	int m_nVertices = 32;
	bool m_cap = true;
};

class ParameterizedCapsule : public ParameterizedMesh
{
public:
	ParameterizedCapsule();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Capsule"; }
private:
	int m_n = 20;
	float m_height = 2.f, m_radius = 1.f;
};

class ParameterizedTorus : public ParameterizedMesh
{
public:
	ParameterizedTorus();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Torus"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_R = 5.f, m_r = 2.f;
};

// ---------------------------------------------------------------------------
// Parametric surfaces
// ---------------------------------------------------------------------------

class ParameterizedKleinBottle : public ParameterizedMesh
{
public:
	ParameterizedKleinBottle();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Klein Bottle"; }
private:
	int m_thetaRes = 20, m_phiRes = 20;
};

class ParameterizedHelicoid : public ParameterizedMesh
{
public:
	ParameterizedHelicoid();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Helicoid"; }
private:
	int m_nu = 20, m_nv = 20;
	float m_a = 1.f, m_b = 1.f, m_c = 0.2f;
};

class ParameterizedSeashell : public ParameterizedMesh
{
public:
	ParameterizedSeashell();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Seashell"; }
private:
	int m_nu = 50, m_nv = 50;
};

class ParameterizedSeashellVonSeggern : public ParameterizedMesh
{
public:
	ParameterizedSeashellVonSeggern();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Seashell (von Seggern)"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_a = 0.2f, m_b = 1.f, m_c = 0.1f, m_n = 2.f;
};

class ParameterizedCorkscrew : public ParameterizedMesh
{
public:
	ParameterizedCorkscrew();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Corkscrew"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_a = 1.f, m_b = 0.5f;
};

class ParameterizedMobiusStrip : public ParameterizedMesh
{
public:
	ParameterizedMobiusStrip();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Mobius Strip"; }
private:
	int m_nu = 50, m_nv = 10;
	float m_w = 0.1f, m_r = 0.5f;
};

class ParameterizedRadialWave : public ParameterizedMesh
{
public:
	ParameterizedRadialWave();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Radial Wave"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_radius = 10.f, m_height = 20.f, m_frequency = 0.6f;
};

class ParameterizedBreather : public ParameterizedMesh
{
public:
	ParameterizedBreather();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Breather"; }
private:
	int m_nu = 50, m_nv = 50;
};

class ParameterizedHyperbolicParaboloid : public ParameterizedMesh
{
public:
	ParameterizedHyperbolicParaboloid();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Hyperbolic Paraboloid"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedMonkeySaddle : public ParameterizedMesh
{
public:
	ParameterizedMonkeySaddle();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Monkey Saddle"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedBlobs : public ParameterizedMesh
{
public:
	ParameterizedBlobs();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Blobs"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedDrop : public ParameterizedMesh
{
public:
	ParameterizedDrop();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Drop"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedGuimard : public ParameterizedMesh
{
public:
	ParameterizedGuimard();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Guimard"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_a = 2.f, m_b = 3.f, m_c = 1.f;
};

// ---------------------------------------------------------------------------
// Knots
// ---------------------------------------------------------------------------

class ParameterizedTorusKnot : public ParameterizedMesh
{
public:
	ParameterizedTorusKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Torus Knot"; }
private:
	int m_nu = 100, m_nv = 20;
	int m_a = 3, m_b = 4;
};

class ParameterizedCinquefoilKnot : public ParameterizedMesh
{
public:
	ParameterizedCinquefoilKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cinquefoil Knot"; }
private:
	int m_nu = 100, m_nv = 20;
	int m_a = 3;
};

class ParameterizedTrefoilKnot : public ParameterizedMesh
{
public:
	ParameterizedTrefoilKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Trefoil Knot"; }
private:
	int m_nu = 100, m_nv = 20;
};

class ParameterizedBorromeanRings : public ParameterizedMesh
{
public:
	ParameterizedBorromeanRings();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Borromean Rings"; }
private:
	int m_nu = 100, m_nv = 20;
};

// ---------------------------------------------------------------------------
// Fractal shapes
// ---------------------------------------------------------------------------

class ParameterizedMengerSponge : public ParameterizedMesh
{
public:
	ParameterizedMengerSponge();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Menger Sponge"; }
private:
	int m_level = 2;
};

// ---------------------------------------------------------------------------
// SVG-based shapes
// ---------------------------------------------------------------------------

// Loads an SVG file and produces an extruded 3D mesh. The filename is set
// at construction (typically from a wxFileDialog) and is NOT exposed as a
// Parameter — only the extrusion height and the bezier-flatten tolerance
// are user-editable from the property panel.
class ParameterizedSvgExtrusion : public ParameterizedMesh
{
public:
	explicit ParameterizedSvgExtrusion(const std::string& filename);
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "SVG extrusion"; }
private:
	std::string m_filename;
	float       m_height     = 0.2f;
	float       m_flattenTol = 0.5f;
};

// ---------------------------------------------------------------------------
// L-systems (fractal curves rendered as 3D tubes)
// ---------------------------------------------------------------------------

// Runs one of the built-in L-systems (see lsysteminit.h) for a chosen number of
// iterations, then sweeps a tube of the given radius along every drawn segment
// of its turtle walk to produce a renderable 3D Mesh. The system is picked from
// an ENUM exposing the whole catalogue; 3D systems (Hilbert 3D, plants) use the
// 3D turtle interpretation automatically.
class ParameterizedLSystem : public ParameterizedMesh
{
public:
	ParameterizedLSystem();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "L-system"; }
private:
	int   m_system     = 9;      // default: Hilbert (nice and bounded)
	int   m_iterations = 4;
	float m_thickness  = 0.04f;
};

// ---------------------------------------------------------------------------
// Gothic architecture
// ---------------------------------------------------------------------------

// Simple beveled masonry block (CreateBlock).
class ParameterizedGothicBlock : public ParameterizedMesh
{
public:
	ParameterizedGothicBlock();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Gothic Block"; }
private:
	float m_width = 1.618f, m_height = 1.f, m_depth = 1.f, m_bevel = 0.1f;
};

// Full parametric Gothic window tracery (Havemann pipeline) : pointed arch +
// offset + lancets + rosette + foils + trefoil + mouchettes + fillets, then the
// stone region is tessellated and (optionally) extruded to a 3D Mesh. All the
// buildXxx() steps throw on invalid inputs, so Regenerate() clamps parameters
// and wraps the pipeline in try/catch with a placeholder fallback.
class ParameterizedGothicWindow : public ParameterizedMesh
{
public:
	ParameterizedGothicWindow();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Gothic Window"; }
private:
	// arch
	float m_width = 200.f, m_excess = 1.0f, m_offsetOuter = 16.f, m_offsetInner = 10.f;
	// straight body below the pointed heads (main frame + lancets) : turns the
	// bare pointed arch into a tall window silhouette. 0 = pure arch (old look).
	float m_bodyHeight = 260.f;
	// lancets
	int   m_subCount = 2; float m_subDrop = 0.f, m_subExcess = 1.0f, m_gapFraction = 0.11f;
	// recursion : each lancet -> mini-window (sub-lancets + small rosette), 0..2
	int   m_recursion = 0;
	// lancet head : 0 = plain pointed, 1 = foiled (small foiled circle in the head)
	int   m_lancetHead = 1; int m_lancetHeadFoils = 4;
	// rosette + foils
	bool  m_rosette = true;
	bool  m_rosetteFoils = true; int m_rosetteFoilCount = 6; int m_rosetteFoilType = 1; float m_rosetteFoilPointed = 0.5f;
	bool  m_subFoils = true;     int m_subFoilCount = 3;     int m_subFoilType = 0;     float m_subFoilPointed = 0.5f;
	// trefoil (on the main arch)
	bool  m_archTrefoil = false; float m_trefoilSplit = 0.45f, m_trefoilFoilRadius = 0.30f;
	// mouchettes
	bool  m_mouchettes = false; int m_mouchetteType = 0; float m_mouchetteRadius = 0.18f;
	// fillets + mesh
	bool  m_fillets = true;
	float m_zHeight = 20.f;
	// 3D moulding profile on the field borders : 0 = flat (straight walls),
	// 1 = chamfer (bevelled/splayed openings). Phase 2.
	int   m_profile = 1;
};

// ---------------------------------------------------------------------------
// Implicit surface from a point cloud
// ---------------------------------------------------------------------------

// Loads a point cloud (PLY) and reconstructs an implicit "blobby" surface from
// it via marching cubes. The filename is set at construction (typically from a
// wxFileDialog) and is NOT exposed as a Parameter -- only the marching-cubes
// resolution and the iso-surface offset distance are user-editable.
class ParameterizedImplicitFromPoints : public ParameterizedMesh
{
public:
	explicit ParameterizedImplicitFromPoints(const std::string& plyPath);
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Implicit (point cloud)"; }
private:
	std::string     m_filename;
	PointCloudField m_field;
	int             m_resolution  = 32;
	float           m_isoDistance = 0.05f;
	bool            m_simplify    = false; // use the tandem extractor (decimated output)
};
