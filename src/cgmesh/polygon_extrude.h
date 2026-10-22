#pragma once

#include "polygon2.h"
#include "mesh.h"

//
// Multi-contour Polygon2 -> triangulated Mesh, flat or extruded.
//
// Contour convention : outer contours CCW, holes CW. The tessellation goes
// through Polygon2's GLU tessellator.
//

// Tessellate `polygon` and pack the result into the flat 3D mesh `out` at
// z. Replaces any previous content of `out`. Throws std::runtime_error if
// the tessellator produces no output.
void tessellateToMesh (Polygon2 &polygon, Mesh &out, double z = 0.0);

// Extrude `polygon` between zBottom and zTop into a solid 3D mesh `out`.
// The output contains :
//   - top cap (CCW triangles at z = zTop, normals +z)
//   - bottom cap (CW triangles at z = zBottom, normals -z)
//   - side walls : one quad (= 2 triangles) per polygon edge, normals
//     pointing OUTWARD from the filled region (so outer-contour walls face
//     out, hole-contour walls face into the holes).
// Throws std::runtime_error if the tessellator produces no output.
void extrudeToMesh (Polygon2 &polygon, Mesh &out, double zBottom, double zTop);

// Append `src` mesh's vertices and faces to `dest`. `src`'s face indices are
// shifted by `dest`'s current vertex count. Used to merge separately-built
// meshes (e.g. an extruded plate + swept mouldings) into a single Mesh.
void appendMesh (Mesh &dest, const Mesh &src);
