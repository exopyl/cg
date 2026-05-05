#pragma once

#include <string>

#include "architecture_gothic.h"

//
// Gothic architecture I/O : JSON parsing + SVG export
//
// Reference schema : src/cgmath/gothic-tracery.schema (JSON Schema Draft 2020-12).
//
// Coverage of the schema in the current minimal implementation :
//   - window.basis (origin, archRise, pL, pR)
//   - window.arch.{width, excess, offset.{outer, inner}}
//   - window.subwindows.{count, drop, excess, gap.{mode, gapFraction|absoluteWidth}}
//   - presence flag for window.rosette
//
// Ignored (silently) : foils, trefoil, fillets, mouchettes, profile, style,
// tessellation, recursive style, wall, variants. These will be wired in as the
// corresponding A5..A8 / B / C tasks land.
//
// Sub-arches use the same offset as the main arch (the schema is silent on a
// per-subwindow offset).
//

// Parsed parameters from a JSON instance (subset relevant for current tasks).
struct WindowInstance
{
	ArchBasis        archBasis;
	ArchOffsetParams archOffset;
	SubwindowParams  subwindowParams;
	bool             hasRosette = false;

	// Foils (parsed from JSON when present)
	FoilsParams      rosetteFoils;            // valid iff hasRosetteFoils
	bool             hasRosetteFoils = false;
	FoilsParams      subwindowFoils;          // applied uniformly to each lancet
	bool             hasSubwindowFoils = false;

	// Trefoil (optional, gated by `enabled` field in JSON)
	TrefoilParams    archTrefoil;             // valid iff hasArchTrefoil
	bool             hasArchTrefoil = false;
	TrefoilParams    subwindowTrefoil;        // applied uniformly to each lancet
	bool             hasSubwindowTrefoil = false;

	// Fillets (optional, gated by `enabled` field in JSON)
	bool             hasFillets = false;
	double           filletsStoneBandWidth = 10.0;

	// Mouchettes (optional, gated by `enabled` field in JSON)
	bool             hasMouchettes = false;
	MouchetteParams  mouchettes;

	// Optional metadata
	std::string id;
	std::string label;
	std::string period;
};

// Geometry produced by orchestrating buildArch / buildArchOffset /
// buildSubwindows / buildRosette / buildFoilRing from a WindowInstance.
struct WindowGeometry
{
	ArchGeom       mainArch;
	ArchOffsetGeom mainOffset;
	SubwindowsGeom subwindows;
	RosetteGeom    rosette;                   // valid iff hasRosette
	bool           hasRosette = false;

	// Foils. rosetteFoils is inscribed in rosette.circle ; subwindowFoils[i] is
	// inscribed in the inner offset of lancets[i] (computed via
	// inscribedCircleOfPointedArch).
	FoilRing                rosetteFoils;     // valid iff hasRosetteFoils
	bool                    hasRosetteFoils = false;
	std::vector<FoilRing>   subwindowFoils;   // one per lancet, valid iff hasSubwindowFoils
	bool                    hasSubwindowFoils = false;

	// Trefoil arches. Built from the *base* arch silhouette (offsets are not
	// trefoiled in this minimal renderer). The original mainOffset / lancet
	// offsets are still drawn ; the trefoil adds the foil decoration on top.
	TrefoilArchGeom               archTrefoil;             // valid iff hasArchTrefoil
	bool                          hasArchTrefoil = false;
	std::vector<TrefoilArchGeom>  subwindowTrefoils;       // one per lancet
	bool                          hasSubwindowTrefoils = false;

	// Fillets : 2 lateral fillets if rosette is present and instance enabled them.
	FilletsGeom                   fillets;
	bool                          hasFillets = false;

	// Mouchettes : one per inter-lancet gap when enabled.
	std::vector<MouchetteGeom>    mouchettes;
	bool                          hasMouchettes = false;
};

//
// JSON loading
//

// Parse a JSON instance string into a WindowInstance.
// Throws std::invalid_argument on missing required fields or malformed values,
// std::runtime_error on JSON parse error.
WindowInstance loadInstanceFromJson (const std::string &jsonText);

// Read a file and parse it as a JSON instance.
// Throws std::runtime_error if the file cannot be opened.
WindowInstance loadInstanceFromFile (const std::string &filePath);

//
// Geometry construction
//

// Build the WindowGeometry by orchestrating the buildXxx() functions.
// May propagate any exception raised by those (typically std::invalid_argument).
WindowGeometry buildGeometryFromInstance (const WindowInstance &instance);

//
// SVG export
//

// Render a WindowGeometry as a self-contained SVG document string.
// The current renderer emits polylines for arcs (sampled adaptively) and a
// circle element for the rosette. Y axis is flipped relative to the math frame
// so that the gothic window appears upright on screen.
std::string toSvg (const WindowGeometry &geom);

// Write the SVG produced by toSvg() to a file. Creates parent directories as
// needed. Throws std::runtime_error if the file cannot be opened for writing.
void writeSvgToFile (const WindowGeometry &geom, const std::string &filePath);
