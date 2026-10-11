#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <cgmath/architecture_gothic.h>
#include <cgmath/architecture_gothic_io.h>

#include "polygon2.h"
#include "mesh.h"
#include "profile2d.h"
#include "bar_fusion.h"   // FusedBar

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
// Contours (outer + holes) :
//   - contour 0 : main inner offset outline (CCW), bounds the whole infill
//   - each opening (lancet / rosette) contributes glass voids as CW holes.
//
// Cusped-void rule (avoids stone islands) : GLU's NONZERO winding cannot nest
// "void inside void" — a hole cut inside an already-void area flips back to
// STONE. So for a FOILED opening we do NOT cut its base circle/arch ; we cut
// the FOILS themselves as the voids. Havemann foils are mutually tangent
// (touch only at cusp points), so independent foil holes reproduce the true
// multifoil : void petals + stone cusps + stone central boss, no islands.
//   - rosette with foils   -> its foil petals as holes (round circle / pointed leaf)
//   - rosette without foils -> its circle as a hole (plain oculus)
//
// Lancets are ALWAYS a plain tall pointed-arch void (the window opening). Foil
// head-cusping is intentionally NOT applied to lancets : the foil ring is
// inscribed in a circle interior to the lancet, so "tall arch void + cusped
// head" is a genuine 2D-boolean op (stone cusps must stay connected to the
// frame) that GLU alone cannot express without floating stone. The full
// Havemann reference (cusped lancet heads) would need a 2D polygon boolean
// library (e.g. Clipper2), not currently vendored.
//
// Phase 3 :
//   - Extrusion : if `GothicMeshParams::zHeight > 0`, the polygon is extruded
//     in z to produce a solid 3D mesh (top cap + bottom cap + side walls).
//
// NOT covered yet :
//   - Trefoils, mouchettes, fillets as voids.
//   - No B2 sweep (stone moldings extruded along curves).
//

// Feature thresholds : lengths, in the units of the geometry, below which the
// generator drops an element or floors a dimension. Defaults suit a bay of
// width 200. ParameterizedGothicWindow takes them in world units ; they follow
// the window scale only when scaled explicitly (Scaled). Degeneracy guards
// (tangent circle of radius <= 1, epsilons) are not part of it.
struct GothicFeatureThresholds
{
	double minRosetteRadius    = 1.0;   // rosette kept only if its rim and its inner (ring) radius exceed it
	double minRingWidth        = 3.0;   // floor of the rosette ring width (otherwise 0.10 x rim radius)
	double minEyeletInset      = 1.5;   // floor of the eyelet fillet inset (otherwise 0.28 x ring width)
	double minSubRosetteRadius = 3.0;   // recursion sub-rosette kept only if its radius exceeds it
	double minMouchetteRadius  = 1.5;   // smaller spandrel fields keep a plain fillet void

	// ParameterizedGothicWindow only (spandrel inset, Chamfer and bar profiles).
	double minFilletInnerOffset = 1.0;  // floor of the spandrel inset : inset = max(inner offset, this)
	double maxMouchetteInset    = 4.0;  // spandrel inset cap when the fields hold mouchettes
	double chamferWidthAdd      = 2.0;  // Chamfer width = min(0.6 x inset, 0.45 x inner + this)...
	double minChamferWidth      = 0.5;  // ...floored at this
	double barRadiusAdd         = 1.5;  // bar radius = min(0.5 x inset, 0.45 x inner + this)...
	double minBarRadius         = 0.8;  // ...floored at this
	double minBand              = 0.5;  // floor of the outer offset and of the Prototype mullion

	// Every length multiplied by k.
	GothicFeatureThresholds Scaled (double k) const;
};

struct GothicMeshParams
{
	double maxAngleRad = 3.14159265358979323846 / 180.0;   // 1 deg per arc segment (matches SVG renderer)

	// Extrusion : if zHeight > 0, the polygon is extruded in z to produce a
	// solid 3D mesh (top cap + bottom cap + side walls connecting outer and
	// hole contours). If zHeight == 0, the mesh is flat at z = 0.
	double zHeight = 0.0;

	// Straight vertical body below the pointed heads. When > 0, the main frame
	// and each lancet are extended down from their springline by `bodyHeight`
	// (a stone sill band is kept at the very bottom), turning bare pointed
	// arches into tall window / lancet silhouettes. 0 = pure arch.
	double bodyHeight = 0.0;

	// Recursion : when recursionDepth > 0, each main lancet opening becomes a
	// mini-window — subdivided by thin stone mullions into `recursionSub.count`
	// sub-lancets crowned by a small rosette, recursed `recursionDepth` times
	// (right image of the Havemann-Fellner reference). Needs the offset and
	// subdivision params to re-run buildArchOffset/buildSubwindows/buildRosette
	// on each sub-arch. recursionFoils = sub-rosette foil count.
	int              recursionDepth = 0;
	ArchOffsetParams recursionOffset;
	SubwindowParams  recursionSub;
	int              recursionFoils = 3;

	// Fillets : the corner FIELDS between the rosette, the sub-arches and the
	// main arch (Havemann §5.4, Fig 5.25). Computed as region − rosette − sub-
	// arches via Clipper2 (the 2D-CSG the thesis wanted, p.254), then inset by
	// `filletInset` (= bdInner) so a stone bar frames them.
	bool             fillets     = false;
	double           filletInset = 8.0;

	// Flamboyant mouchettes (Havemann §5.4, Fig 5.22 row 3) : when `mouchettes` is
	// set (and `fillets` on), each spandrel FIELD is voided as an inscribed
	// mouchette/soufflet clipped to the field, instead of a plain fillet void — the
	// curvilinear late-Gothic look. mouchetteType : 0 = Vesica, 1 = Teardrop,
	// 2 = Soufflet (flamboyant curvilinear triangle). mouchetteSize scales the shape
	// within its field, in (0, 1].
	bool             mouchettes    = false;
	int              mouchetteType = 1;
	double           mouchetteSize = 0.85;

	// Foiled lancet heads (Phase 3, Havemann §5.4 / gothic1.png middle panel) :
	// a small foiled circle (mini-rosette) set into the head of each lancet,
	// tangent to the lancet arch, the light open below it. `lancetHeadFoils` =
	// number of foils. Applied at recursion depth 0.
	bool             lancetHeadFoiled = false;
	int              lancetHeadFoils  = 4;

	// Rosette foil shape : 0 = round foils (circles), 1 = pointed foils (almond
	// petals pointed at both ends, the reference gothic1.png look).
	int              rosetteFoilType = 1;
	// Pointedness of the pointed foils : 0 = fat/almost round, 2 = slender sharp.
	double           rosettePointedness = 0.5;

	// Feature thresholds, in the units of the geometry.
	GothicFeatureThresholds thresholds;
};

// Build warnings : events that do not make the construction fail but change the
// bay it returns. Recorded only ; the construction is the same whether or not a
// report is collected. Categories are stable : new ones are appended.
enum class GothicBuildWarning
{
	DepthFallback,        // a recursive unit could not subdivide : its whole opening is cut
	FeatureDropped,       // an element dropped (threshold, no support, construction that throws)
	MorphologicalGuard,   // the morphological guard was applied to the stone region
	GuardClosingFallback, // the guard kept the closing alone (opening + closing left a defect)
	GuardInsufficient,    // the region keeps a contour defect after the guard
	ParameterClamped,     // a parameter was brought back into its domain
	RegionInvalid,        // the level-0 region keeps a contour defect after the boolean and the guard
	ProfileFallback,      // the splay profile could not be extruded : straight walls instead
	MouldingDropped,      // a moulding bar (or the whole moulding) could not be built
	FieldIgnored,         // a loaded field has no effect on the bay
	FieldConverted,       // a loaded field was converted to its nearest equivalent
	ProfileReduced,       // the splay profile was narrowed on a contour whose final ring was invalid
	RegionRepaired,       // the level-0 assembly had a contour defect : the region is one boolean instead
	MultipleShells,       // the mesh is several closed shells : moulding bars laid on the plate, not fused
	MouldingNotFused,     // the bars could not be fused with the plate : laid on it instead (MultipleShells)
	MouldingSimplified,   // fused bars : level steps skipped by the ring guard (fewer sloped steps)
};

constexpr std::size_t kGothicBuildWarningCount = static_cast<std::size_t>(GothicBuildWarning::MouldingSimplified) + 1;

// Build errors : the bay is not usable as built, or a description could not be
// loaded. InvalidParameter and ConstructionFailed come with the generator's
// placeholder mesh ; MeshNotStrict keeps the real mesh, whose stone is not
// strict. Any error refuses the export (CheckGothicExport).
enum class GothicBuildError
{
	InvalidParameter,     // a setting outside any usable domain (non-finite value)
	ConstructionFailed,   // the construction threw ; the message is the exception text
	JsonParse,
	JsonSchema,
	JsonType,
	JsonValue,
	JsonMissing,
	MeshNotStrict,        // the level-0 region kept a contour defect and the extruded stone is not strict
};

constexpr std::size_t kGothicBuildErrorCount = static_cast<std::size_t>(GothicBuildError::MeshNotStrict) + 1;

// Stable snake_case names (« depth_fallback », « invalid_parameter », ...).
const char *GothicBuildWarningName (GothicBuildWarning kind);
const char *GothicBuildErrorName   (GothicBuildError kind);

// One recorded event. `code` is the GothicBuildWarning or GothicBuildError value,
// according to `error`. `parameter` names the setting : its UI name for a build
// event, its JSON path for a load event (LoadFromJson) ; empty when the event has
// none.
struct GothicDiagnostic
{
	bool        error = false;
	int         code  = 0;
	std::string parameter;
	std::string message;
};

// Entries kept per report ; the counters count every event.
constexpr std::size_t kGothicBuildReportMaxEntries = 64;

struct GothicBuildReport
{
	std::array<std::size_t, kGothicBuildWarningCount> counts {};
	std::array<std::size_t, kGothicBuildErrorCount>   errorCounts {};
	std::vector<GothicDiagnostic>                     entries;   // at most kGothicBuildReportMaxEntries

	// Count only, no entry.
	void        Record (GothicBuildWarning kind)      { ++counts[static_cast<std::size_t>(kind)]; }
	// Count and entry (while the entries are below the cap).
	void        Record (GothicBuildWarning kind, std::string parameter, std::string message);
	void        Record (GothicBuildError kind, std::string parameter, std::string message);

	std::size_t Count  (GothicBuildWarning kind) const { return counts[static_cast<std::size_t>(kind)]; }
	std::size_t Count  (GothicBuildError kind) const   { return errorCounts[static_cast<std::size_t>(kind)]; }
	bool        HasErrors () const;
	bool        HasWarnings () const;

	// Adds the counters of `other` and appends its entries, within the cap.
	void        Merge (const GothicBuildReport &other);
};

// JSON object : {"errors": {name: count}, "warnings": {name: count}, "entries":
// [{"severity": "error" | "warning", "code": name, "parameter": ..., "message": ...}]}.
// Every category appears in its counter object, zero included.
std::string GothicBuildReportToJson (const GothicBuildReport &report);

// Oculus (rosette) circle centred on the symmetry axis x = axisX, internally
// tangent to the main arch AND passing through the lancet apex point, so it
// touches the DRAWN lancet tops (cf. gothic1.png) rather than the phantom
// extension of the arc circle (which buildRosette's construction-circle tangency
// lands on, leaving a visible gap above the apex). Returns a circle with
// radius <= 0 if no valid solution exists (caller should fall back).
Circle rosetteTangentToLancets (const ArchGeom &mainInner, double axisX,
                                const Vector2d &lancetApex);

// Build a multi-contour Polygon2 representing the stone region of the bay :
//   contour 0 : main inner offset outline, CCW
//   contour i+1 (1..n) : lancet i inner offset outline, reversed to CW (= hole)
//
// The total filled area (signed sum) equals the stone region : outer area
// minus the lancet glass voids.
//
// With recursion (recursionDepth >= 1), the stone is one 2D boolean,
// buildGothicStoneRegion (architecture_gothic_region.h) : one outer contour
// and disjoint holes, on the Clipper2 grid, the sub-tracery joined to the frame.
//
// Without recursion, the stone is buildLevel0StoneRegion.
//
// `report`, when given, receives the build warnings (GothicBuildWarning). The
// returned polygon does not depend on `report`.
Polygon2 buildBayStonePolygon (const WindowGeometry &geom,
                                const GothicMeshParams &params = {},
                                GothicBuildReport *report = nullptr);

// Level-0 stone region : the outer silhouette `frame` (contour 0, CCW) and the
// voids `holes` (CW), each a simple ring. The direct assembly (frame + holes as
// they are) is returned when ValidateContours accepts it. Otherwise (a void
// tangent to the frame or to another void, a self-touching motif) the region is
// one boolean, buildGothicStoneRegion with its morphological guard, recorded as
// GothicBuildWarning::RegionRepaired (plus MorphologicalGuard /
// GuardClosingFallback when the guard ran). When that region still has a
// contour defect, it is returned and recorded as RegionInvalid (its defects ;
// « validation truncated » when the validator ran out of budget) ; when the
// boolean leaves no stone, the direct assembly is returned, RegionInvalid. Never
// throws for an invalid region : ParameterizedGothicWindow::Regenerate decides
// on the extruded mesh. The returned region does not depend on `report`.
Polygon2 buildLevel0StoneRegion (const std::vector<Vector2d> &frame,
                                 const std::vector<std::vector<Vector2d>> &holes,
                                 GothicBuildReport *report = nullptr);

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

// extrudeProfiledToMesh -- l'extrusion a moulure -- vit dans profile2d.h, ou
// elle est parametree par un Profile2D au lieu du couple (chamW, chamD). Elle
// n'est plus une fonction du gothique : trois consommateurs la demandaient, et
// le gothique n'est que l'un d'eux. L'en-tete est inclus ci-dessus pour que les
// appelants historiques -- qui incluent celui-ci -- continuent de la voir.


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

// "French style" bar moulding (Havemann §5.4.1, Fig. 5.28 d) : sweeps the closed
// cross-section `profile` along the ACTUAL field-border outlines of the bay — the
// main frame inner edge, each lancet opening (cusped/foiled head AND straight
// jambs when a body is present), each recursion sub-tracery frame + sub-rosette
// rim, and the rosette (rim of the lobed motif, void edge of a plain oculus) —
// reproducing the exact curves that `buildBayStonePolygon` cuts as voids.
//
// `profile` is a CLOSED loop in (u, v), in the units of `geom` : u = z (depth ; flat side on
// the front face, molding proud toward +z), v = in-plane offset perpendicular to
// the path. See the barProfile factories in parameterized_shapes (Roll / Keel /
// Ogee). Meant to be appended (appendMesh) onto a flat extruded bay plate.
//
// Result written into `out` (replaces previous content). `out` is left empty when
// the geometry yields no sweepable outline. Corner joints are left "open" (no
// mitre / bisector plane yet — thesis §5.4.1, deferred).
//
// The recursive outlines are bounded by the opening of their lancet, as the
// stone bounds the recursive sub-voids : an outline that leaves it is replaced
// by its intersection with it, so no bar runs off the plate.
//
// `report`, when given, receives GothicBuildWarning::MouldingDropped for each
// recursive outline that cannot be built, and MultipleShells once when `out`
// holds bars (each bar is a closed shell, laid on the plate without a boolean).
void buildBayMoulding (const WindowGeometry &geom, const GothicMeshParams &params,
                       const std::vector<Vector2d> &profile, Mesh &out,
                       GothicBuildReport *report = nullptr);

// The outlines buildBayMoulding sweeps, in sweep order, each with the profile
// scale it applies : f = 0.6 x (characteristic radius of the outline) / (profile
// half-width), 1 when f >= 0.999 ; an outline with f < 0.2 (too small to carry
// the bar) or fewer than 3 points is left out. `profile` is only read for its
// half-width (max |v|). `report` receives the same MouldingDropped warnings as
// buildBayMoulding. Empty for a profile of fewer than 3 points.
std::vector<FusedBar> collectBayMouldingOutlines (const WindowGeometry &geom, const GothicMeshParams &params,
                                                  const std::vector<Vector2d> &profile,
                                                  GothicBuildReport *report = nullptr);

// The sweep half of buildBayMoulding : one closed shell per outline of `bars`
// (as collected above), the profile scaled about its base plane by each bar's
// scale. Same output and same MultipleShells warning as buildBayMoulding.
void sweepBayMoulding (const std::vector<FusedBar> &bars, const std::vector<Vector2d> &profile, Mesh &out,
                       GothicBuildReport *report = nullptr);

// Report entries of a fusion (fuseBarsOnPlate), parameter "Profile" :
// MouldingNotFused when it failed, with its cause ; MouldingSimplified when the
// ring guard skipped level steps, with their number. Nothing otherwise.
void recordFusedBarsOutcome (const FusedBarsResult &result, GothicBuildReport *report);

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
