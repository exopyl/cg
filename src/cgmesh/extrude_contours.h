#pragma once

// ============================================================================
//  2D contours -> extruded solid (tessellation + caps + walls)
// ============================================================================
//
// The primitive behind import_svg_extruded() and image_to_relief(): take a set
// of closed 2D contours (outer boundaries + holes), tessellate the filled
// region with glutess, and emit a closed solid between two Z planes — bottom
// cap, top cap and side walls with outward normals.
//
// Contours are expected in FINAL world XY: any recentering, scaling or Y flip
// is the caller's job, so this file stays a pure geometry primitive. Several
// contour sets can be appended to one builder (each with its own Z range and
// material id) and baked into a single multi-material Mesh.
//
// ============================================================================

#include <functional>
#include <vector>

#include "../cgmath/cgmath.h"

class Mesh;

// One closed contour. The closing edge (last point -> first point) is implicit.
struct ExtrudeContour
{
	std::vector<Vector2f> pts;
	// Marks a contour that SUBTRACTS from the region it sits inside. Only used
	// when ExtrudeAppendOptions::normalizeOrientation is set (see there);
	// otherwise the winding rule alone decides what is filled.
	bool isHole = false;
};

// Tessellator fill rule. NonZero: a contour subtracts only if wound opposite to
// the one containing it (the usual SVG / authoring convention). EvenOdd: any
// nesting alternates fill, whatever the orientations.
enum class ExtrudeWinding { NonZero, EvenOdd };

struct ExtrudeAppendOptions
{
	float zBottom = 0.f;
	float zTop    = 1.f;

	// Stamped on every face emitted by this Append (see Mesh::Material_Add).
	// The default matches MaterialType::MATERIAL_NONE.
	unsigned int materialId = (unsigned int)-1;

	bool emitWalls = true;

	ExtrudeWinding winding = ExtrudeWinding::NonZero;

	// Rewind each contour from its isHole flag (outer CCW / hole CW) before
	// tessellating, so a NonZero fill behaves as intended whatever orientation
	// the caller built its points in.
	// Keep it FALSE for SVG: there, holes are expressed by the orientation the
	// document AUTHORED, and isHole is unknown — rewinding would fill holes in.
	bool normalizeOrientation = false;

	// Optional per-edge veto for wall quads: return false to skip the wall on
	// the contour edge (a, b). Used to drop the coincident internal walls
	// between two blocks that share a boundary. Empty = emit every wall.
	std::function<bool(const Vector2f& a, const Vector2f& b)> wallFilter;
};

// Accumulates several extruded contour sets, then bakes them into one Mesh.
//
// Vertices are emitted as four DISJOINT blocks per Append (bottom cap, top cap,
// bottom wall, top wall) so Mesh::ComputeNormals() never averages a cap normal
// with a wall normal: the VBO path feeds per-vertex normals to the GPU even for
// GL_FLAT, so sharing cap and wall corners would tilt every cap-boundary normal
// toward the local wall direction and give visibly non-uniform top/bottom
// shading.
class ExtrudedMeshBuilder
{
public:
	// Tessellate `contours` and append caps (+ walls) between opt.zBottom and
	// opt.zTop. Returns false when nothing could be emitted (fewer than three
	// points, tessellation failure).
	bool Append (const std::vector<ExtrudeContour>& contours,
		     const ExtrudeAppendOptions& opt);

	bool Empty (void) const { return m_faces.empty(); }
	unsigned int GetNVertices (void) const { return (unsigned int)(m_verts.size() / 3); }
	unsigned int GetNFaces (void) const { return (unsigned int)m_faces.size(); }

	// Heap-allocated Mesh (caller owns it) with normals computed and the
	// revision bumped; nullptr when the builder is empty. Materials are NOT
	// added here — the caller owns the material table and passed the ids to
	// Append(). The builder is left untouched and can be baked again.
	Mesh* Build (void);

private:
	struct Tri
	{
		unsigned int a, b, c;
		unsigned int materialId;
	};

	std::vector<float> m_verts;   // 3 floats per vertex
	std::vector<Tri>   m_faces;
};
