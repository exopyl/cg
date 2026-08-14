#pragma once
#include <memory>

#include "parameterized.h"
#include "mesh.h"
#include "image_pixel_blocks.h"   // ImagePixelBlocksOptions
#include "surface_implicit_pointcloud.h"

class Font;   // cgmath/font.h -- garde stb_truetype hors de cet en-tete

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
	// 0.01 : le maillage etant normalise a 1.0 dans sa plus grande dimension, c'est
	// un centieme de cette taille -- l'epaisseur d'une plaque gravee. A 0.2 le
	// resultat etait un pave dont le relief se lisait mal.
	float       m_height     = 0.01f;
	// 0.005 : un 200e de la taille normalisee, dans les unites du maillage produit
	// et non dans celles du document (cf. SvgExtrudeOptions::flattenTol). C'est ce
	// que valait l'ancien 0.5 sur un canevas de 100 -- soit les fichiers d'essai du
	// depot -- de sorte que le rendu habituel ne change pas.
	float       m_flattenTol = 0.005f;
};

// ---------------------------------------------------------------------------
// Image -> coloured relief
// ---------------------------------------------------------------------------

// Quantizes a raster image to a few colours, vectorizes each colour region and
// extrudes it as a block of uniform height on a base plate framed by a wall
// (see image_relief.h). The produced Mesh carries one Material per colour plus
// base and wall, so a renderer that honours face material ids shows the picture.
//
// NOTE: Regenerate() replays the WHOLE chain (load, quantize, vectorize,
// extrude). The vectorization dominates and is not cheap on a large image, so
// every parameter edit costs a full pass -- keep `Max colors` low while
// exploring.
class ParameterizedImageRelief : public ParameterizedMesh
{
public:
	explicit ParameterizedImageRelief(const std::string& filename);
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Image quantification"; }
private:
	std::string m_filename;
	int   m_maxColors     = 8;
	int   m_algo          = 0;      // 0 = Wu, 1 = Heckbert
	float m_simplifyErr   = 1.0f;
	int   m_preSmooth     = 1;       // passes de bilateral avant quantification
	int   m_refine        = 3;       // iterations de raffinement k-means de la palette
	int   m_despeckle     = 1;       // passes de filtre majoritaire 3x3
	int   m_minRegionArea = 12;      // px, absorption des petites regions
	// px, offset negatif sur chaque contour. Non nul par defaut : les regions
	// cessent d'etre jointives et se detachent en pieces separables -- c'est ce
	// que la page « Image to puzzle » cherche a produire.
	float m_shrink        = 0.3f;
	float m_fitSize       = 1.0f;
	float m_blockHeight   = 0.10f;
	float m_baseThickness = 0.05f;
	float m_margin        = 0.05f;
	float m_wallThickness = 0.03f;
	float m_wallHeight    = 0.10f;
	bool  m_internalWalls = true;
	bool  m_emitBase      = true;    // plaque de support
	bool  m_emitWall      = true;    // mur perimetrique
};

// Variante pixel art du relief : l'image est PIXELISEE (vote majoritaire) avant
// segmentation, et les contours sont traces sans lissage -- des blocs carrés au
// lieu de formes suivies. Chaque zone contiguë d'une même couleur devient un
// solide séparable (cf. image_pixel_blocks.h).
class ParameterizedImagePixelBlocks : public ParameterizedMesh
{
public:
	explicit ParameterizedImagePixelBlocks(const std::string& filename);
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Image pixel blocks"; }

	// Chemin de l'image source, pour l'export par blocs (qui doit rejouer la
	// chaine avec les memes reglages).
	const std::string& GetFilename() const { return m_filename; }
	// Options courantes, telles que Regenerate() les construit.
	ImagePixelBlocksOptions GetOptions() const;

private:
	std::string m_filename;
	int   m_pixelWidth    = 64;
	int   m_maxColors     = 8;
	int   m_algo          = 0;      // 0 = Wu, 1 = Heckbert
	int   m_preSmooth     = 1;
	int   m_refine        = 3;
	int   m_despeckle     = 0;
	int   m_minRegionArea = 0;
	// En CELLULES de sortie. Non nul par defaut : le sillon de 2*shrink separe
	// les blocs voisins, qui deviennent des pieces distinctes plutot qu'un
	// pavage jointif.
	float m_shrink        = 0.1f;
	float m_fitSize       = 1.0f;
	float m_blockHeight   = 0.10f;
	float m_baseThickness = 0.05f;
	float m_margin        = 0.05f;
	float m_wallThickness = 0.03f;
	float m_wallHeight    = 0.10f;
	bool  m_internalWalls = true;
	bool  m_emitBase      = true;    // plaque de support
	bool  m_emitWall      = true;    // mur perimetrique
};

// ---------------------------------------------------------------------------
// Texte 3D
// ---------------------------------------------------------------------------

// Charge une police a contours (TTF / OTF / TTC) et extrude une chaine de
// caracteres (cf. text_extrude.h).
//
// Le TEXTE est un Parameter a part entiere (Parameter::STRING) : c'est meme LE
// parametre de cette forme, et il se modifie en direct comme n'importe quel
// curseur. La POLICE, elle, reste fixee a la construction -- c'est un FICHIER,
// du meme ordre que le SVG de ParameterizedSvgExtrusion ou l'image de
// ParameterizedImageRelief. D'ou l'ergonomie retenue cote applications :
// « importer une police » cree l'objet, puis tout se regle dans le panneau.
//
// La police est chargee UNE fois, a la construction, et conservee : contrairement
// a ParameterizedImageRelief qui relit son fichier a chaque Regenerate(),
// re-parser une police a chaque frappe serait du gaspillage pur. Seuls la mise
// en page, l'aplatissement et la tessellation sont rejoues.
class ParameterizedText3D : public ParameterizedMesh
{
public:
	// `text` n'est que la valeur INITIALE du parametre du meme nom.
	ParameterizedText3D(const std::string& fontFilename,
	                    const std::string& text = "Text");
	~ParameterizedText3D() override;

	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Text 3D"; }

	// Ce que l'on a su faire du crenage de CETTE police, pour qu'une interface
	// puisse le signaler plutot que de laisser decouvrir la limite. Valeurs :
	// cf. KerningStatus (cgmath/font.h) -- 0 aucun, 1 applique, 2 non lisible.
	int GetKerningStatus() const;
	bool IsFontLoaded() const;

private:
	// unique_ptr et non un Font membre : cela garde font.h -- et donc
	// stb_truetype -- hors de cet en-tete, inclus par des dizaines de fichiers.
	std::unique_ptr<Font> m_pFont;
	std::string m_text;

	float m_size          = 1.f;
	float m_depth         = 0.2f;
	float m_flattenTol    = 0.01f;
	float m_letterSpacing = 0.f;
	float m_lineSpacing   = 1.f;
	int   m_align         = 0;      // 0 = Left, 1 = Center, 2 = Right
	bool  m_kerning       = true;
	bool  m_unionOverlaps = false;
	bool  m_centerOnOrigin = true;
};

// ---------------------------------------------------------------------------
// L-systems (fractal curves rendered as 3D tubes)
// ---------------------------------------------------------------------------

// Runs one of the built-in L-systems (see lsysteminit.h) for a chosen number of
// iterations, then gives its turtle walk a volume. The system is picked from an
// ENUM exposing the whole catalogue.
//
// DEUX MODES, car un trace peut prendre corps de deux facons distinctes :
//
//   Tube      : un tube de rayon `Thickness` est balaye le long de chaque segment
//               dessine. Rendu filaire epais, en 3D. Les systemes 3D (Hilbert 3D,
//               plantes) utilisent alors l'interpretation 3D de la tortue.
//   Extrusion : le trace est EPAISSI dans son plan (stroke_contours.h) puis
//               extrude sur `Height`, ce qui donne une plaque gravee. Le
//               traitement des coins et des extremites devient reglable, et un
//               trace FERME -- le catalogue en marque cinq -- est tessele
//               directement au lieu d'etre epaissi, puisqu'il delimite deja une
//               surface.
//
// En mode Extrusion l'interpretation est toujours 2D : une extrusion est plane,
// projeter un trace 3D n'aurait pas de sens.
//
// `Thickness` sert aux deux modes -- rayon du tube, ou demi-largeur du trait --
// et s'exprime dans les unites du maillage normalise (diagonale de la boite
// englobante ramenee a 4), donc apres la mise a l'echelle et non avant.
class ParameterizedLSystem : public ParameterizedMesh
{
public:
	ParameterizedLSystem();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "L-system"; }
private:
	// Trace normalise -> maillage extrude. Sorti de Regenerate() pour en garder le
	// corps lisible ; ne renvoie jamais nullptr (Mesh vide si rien a extruder).
	Mesh* BuildExtrudedTrace (const std::vector<std::vector<Vector3f>>& chains,
	                          bool traceIsClosed);

	int   m_mode       = 0;      // 0 = Tube, 1 = Extrusion
	int   m_system     = 9;      // default: Hilbert (nice and bounded)
	int   m_iterations = 4;
	float m_thickness  = 0.04f;

	// Mode Extrusion uniquement.
	float m_height     = 0.05f;
	int   m_join       = 0;      // Round / Miter / Bevel
	int   m_cap        = 0;      // Round / Square / Butt
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

	// Load a v2 description file (src/cgmath/gothic-window-v2.schema) into the
	// members : geometry (Fig 5.30) + style (per-field) + recursion + extrusion.
	// Absent fields keep their current value. Returns false on parse error (the
	// object is left usable — partially applied fields are harmless). Does NOT
	// regenerate ; the caller runs Regenerate() as usual.
	bool LoadFromJson(const std::string &jsonText);

	// Serialize the current member values to a v2 description
	// (src/cgmath/gothic-window-v2.schema) — the inverse of LoadFromJson, so the
	// result re-imports to the same window. Pretty-printed JSON string.
	std::string ExportJson() const;
private:
	// arch
	float m_width = 200.f, m_excess = 1.0f, m_offsetOuter = 16.f, m_offsetInner = 10.f;
	// straight body below the pointed heads (main frame + lancets) : turns the
	// bare pointed arch into a tall window silhouette. 0 = pure arch (old look).
	float m_bodyHeight = 260.f;
	// lancets
	int   m_subCount = 2; float m_subDrop = 0.f, m_subExcess = 1.0f, m_gapFraction = 0.11f;
	int   m_lancetLayout = 0;   // 0 = Uniform (count+1 equal mullions), 1 = Prototype (Havemann Fig 5.39)
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
	// arc tessellation step (rad/segment) ; default 1°. Set from `kseg` on JSON load.
	double m_maxAngleRad = 3.14159265358979323846 / 180.0;
	// 3D moulding profile on the field borders. Index dans la liste exposee par
	// GetParameters : 0 = Flat (parois droites), 1 = Chamfer (ebrasement biseaute),
	// 2 = Roll bar, 3 = Keel bar, 4 = Ogee bar. Phase 2.
	//
	// FLAT par defaut : c'est la forme la plus simple, celle qui laisse lire la
	// geometrie de la baie sans le relief des moulures, et le point de depart
	// naturel avant d'enrichir. Le defaut etait Chamfer.
	int   m_profile = 0;
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
