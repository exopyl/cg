#pragma once

#include <string>

#include "../cgmath/architecture_gothic.h"
#include "../cgmath/architecture_gothic_io.h"

#include "polygon2.h"
#include "mesh.h"

//
// Gothic architecture mesh : 2D contour -> 3D triangulated Mesh
//
// Reference : extends the cgmath gothic pipeline (architecture_gothic / _io)
// into a 3D mesh suitable for OBJ / STL / PLY / OFF / DAE export via the
// existing Mesh::save mechanism.
//
// Scope :
//   - The bay's stone region is built as a multi-contour polygon (outer + holes),
//     tessellated via Polygon2's GLU tessellator into a flat 3D Mesh at z = 0,
//     saved via Mesh::save (extension dispatches : .obj / .stl / .ply / .off / .dae).
//
// Phase 1 holes :
//   - main inner offset outline (CCW outer)
//   - each lancet's INNER offset outline (CW)
//
// Phase 2 holes (additional voids for richer tracery detail) :
//   - rosette circle (CW)
//   - each round rosette foil circle (CW)
//   - each pointed rosette foil contour : 2 arcs + short rim arc on outer circle (CW)
//   - each round / pointed lancet foil (CW)
//
// Caveat : GLU's NONZERO winding does not nest "void inside void". A foil hole
// placed inside an already-void area (lancet inner / rosette) becomes a STONE
// ISLAND at the foil position rather than a deeper void. This is acceptable
// for visualization (the foils' positions are still visible) but does not
// match the strict architectural meaning where foils are stone arches around
// glass voids.
//
// Phase 3 :
//   - Extrusion : if `GothicMeshParams::zHeight > 0`, the polygon is extruded
//     in z to produce a solid 3D mesh (top cap + bottom cap + side walls).
//
// NOT covered yet :
//   - Trefoils, mouchettes, fillets as voids.
//   - No B2 sweep (stone moldings extruded along curves).
//

struct GothicMeshParams
{
	double maxAngleRad = 3.14159265358979323846 / 180.0;   // 1 deg per arc segment (matches SVG renderer)

	// Extrusion : if zHeight > 0, the polygon is extruded in z to produce a
	// solid 3D mesh (top cap + bottom cap + side walls connecting outer and
	// hole contours). If zHeight == 0, the mesh is flat at z = 0.
	double zHeight = 0.0;
};

// Build a multi-contour Polygon2 representing the stone region of the bay :
//   contour 0 : main inner offset outline, CCW
//   contour i+1 (1..n) : lancet i inner offset outline, reversed to CW (= hole)
//
// The total filled area (signed sum) equals the stone region : outer area
// minus the lancet glass voids.
Polygon2 buildBayStonePolygon (const WindowGeometry &geom,
                                const GothicMeshParams &params = {});

// Tessellate `polygon` and pack the result into the flat 3D mesh `out` at
// z. Replaces any previous content of `out`. Throws std::runtime_error if
// the tessellator produces no output.
void tessellateToMesh (Polygon2 &polygon, Mesh &out, double z = 0.0);

// Extrude `polygon` between zBottom and zTop into a solid 3D mesh `out`.
// The output contains :
//   - top cap (CCW triangles at z = zTop, normals +z)
//   - bottom cap (CW triangles at z = zBottom, normals -z)
//   - side walls : one quad (= 2 triangles) per polygon edge, normals
//     pointing OUTWARD from the stone region (so outer-contour walls face
//     out of the bay, hole-contour walls face into the holes/voids).
// Throws std::runtime_error if the tessellator produces no output.
void extrudeToMesh (Polygon2 &polygon, Mesh &out, double zBottom, double zTop);


//
// B2 — Profile sweep along an arc
//
// Reference : Havemann thesis section 2.7. A 2D profile (cross-section of a
// stone molding) is swept along a 3D path (typically a gothic arc lifted into
// the wall plane) to produce a tube-like 3D mesh.
//
// Profile coordinates :
//   x = u  : depth into the wall  (z direction in our flat config)
//   y = v  : offset perpendicular to the path (in-plane, "outward" of the
//            traversal direction = right of CCW arc walking)
//
// Each profile point (u, v) maps to a 3D position :
//   3D = pathPos + v * scale_v * N + u * scale_u * (0, 0, 1)
// where N is the in-plane normal at pathPos, computed as (T.y, -T.x) (= right
// of walk direction for CCW arcs).
//
// The profile is treated as a CLOSED loop (last vertex connects back to first).

// Sweep a closed 2D profile along an arc, producing a tube mesh of triangulated
// quads. `profile` is a vector of (u, v) points. `scale_u` maps profile u to
// world wall depth (z axis). `scale_v` maps profile v to in-plane offset.
// Result written into `out` (replaces previous content).
//
// Throws std::invalid_argument if `profile` has fewer than 3 points or
// `arc.length()` is non-positive.
void sweepProfileAlongArc (const Arc &arc,
                            const std::vector<Vector2d> &profile,
                            double scale_u,
                            double scale_v,
                            Mesh &out,
                            double maxAngleRad = 3.14159265358979323846 / 180.0);

// Sweep a closed 2D profile along multiple arcs (concatenated tube sections).
// Each arc produces its own tube section ; sections are merged into `out`
// with a single Mesh::SetVertices / SetFaces call. Junctions between arcs are
// "open" (sharp corners), no smoothing.
void sweepProfileAlongArcs (const std::vector<Arc> &arcs,
                             const std::vector<Vector2d> &profile,
                             double scale_u,
                             double scale_v,
                             Mesh &out,
                             double maxAngleRad = 3.14159265358979323846 / 180.0);

// Append `src` mesh's vertices and faces to `dest`. `src`'s face indices are
// shifted by `dest`'s current vertex count. Used to merge separately-built
// meshes (e.g. bay tessellation + sweep moldings) into a single Mesh.
void appendMesh (Mesh &dest, const Mesh &src);

//
// Profile factories : closed cross-sections in (u, v) coords, CCW from
// path-tangent perspective.
//

// Convenience : a closed unit-square profile in (u, v) coords.
// Useful to demonstrate B2 with a rectangular cross-section.
std::vector<Vector2d> rectangularProfile ();

// Chamfered profile : the outside-bottom corner of the unit square is replaced
// by a 45-deg bevel of `depth` (in normalized [0, 1] coords). Returns 5 points.
std::vector<Vector2d> chamferProfile (double depth = 0.3);

// Cavetto profile : concave quarter-circle replacing the outside-bottom corner.
// Returns `nCurveSegments + 4` points (the quarter-arc samples + 3 corners).
std::vector<Vector2d> cavettoProfile (double depth = 0.3, int nCurveSegments = 6);

// One-shot helper : build polygon, tessellate, save (file-extension dispatched
// by Mesh::save : .obj, .stl, .ply, .off, .dae, ...).
// Throws std::runtime_error if the underlying save fails.
void writeBayMesh (const WindowGeometry &geom, const std::string &filePath,
                   const GothicMeshParams &params = {});
