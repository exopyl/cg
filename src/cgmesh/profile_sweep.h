#pragma once

#include <vector>

#include <cgmath/TVector2.h>
#include <cgmath/geometry.h>   // Arc

#include "mesh.h"

//
// Profile sweep along an arc
//
// Reference : Havemann thesis section 2.7. A 2D profile (cross-section of a
// moulding) is swept along a 3D path (typically an arc lifted into the wall
// plane) to produce a tube-like 3D mesh.
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

//
// Profile factories : closed cross-sections in (u, v) coords, CCW from
// path-tangent perspective.
//

// Convenience : a closed unit-square profile in (u, v) coords.
std::vector<Vector2d> rectangularProfile ();

// Chamfered profile : the outside-bottom corner of the unit square is replaced
// by a 45-deg bevel of `depth` (in normalized [0, 1] coords). Returns 5 points.
std::vector<Vector2d> chamferProfile (double depth = 0.3);

// Cavetto profile : concave quarter-circle replacing the outside-bottom corner.
// Returns `nCurveSegments + 4` points (the quarter-arc samples + 3 corners).
std::vector<Vector2d> cavettoProfile (double depth = 0.3, int nCurveSegments = 6);
