#pragma once

// ============================================================================
//  SVG → extruded Mesh
// ============================================================================
//
// Parse an SVG file (via extern/nanosvg), flatten cubic Bezier paths into
// polylines, tessellate the resulting 2D filled regions with glutess, then
// extrude along Z to produce a 3D solid Mesh.
//
//   - SVG Y axis points down by convention; the importer flips Y so the
//     result is upright in a standard right-handed +Y-up viewer.
//   - When centerXY is true, the produced mesh is recentered on its XY
//     bounding box and uniformly scaled so the longest XY dimension equals
//     1.0 (consistent with the other parameterized geometries in sinaia).
//   - Each <path> is treated as a contour; multiple contours within one
//     <shape> are passed to the tessellator together (NONZERO winding so
//     internal holes are subtracted as expected by most SVG authors).
//
// ============================================================================

#include <string>

class Mesh;

struct SvgExtrudeOptions
{
    float height       = 1.0f;  // extrusion depth along +Z
    float flattenTol   = 0.5f;  // pixel-space tolerance for bezier flattening
    bool  centerAndFit = true;  // recenter on XY bbox and normalize size
    bool  invertY      = true;  // SVG Y points down; flip it
};

// Parse `filename` and return a heap-allocated extruded Mesh, or nullptr on
// failure (file missing, parse error, no fillable shape).
Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt);
