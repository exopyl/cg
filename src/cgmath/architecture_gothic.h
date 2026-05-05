#pragma once

#include <variant>

#include "TVector2.h"
#include "geometry.h"

//
// Gothic architecture primitives
//
// Reference : Havemann & Fellner, "Generative Parametric Design of Gothic Window
// Tracery", VAST 2004 ; Havemann, "Generative Mesh Modeling", TU Braunschweig,
// 2005, section 2.
//
// The pointed arch is the fundamental operator of the Havemann pipeline: every
// other gothic primitive (offsets, sub-windows, rosette, foils, fillets, ...) is
// derived from it.
//

//
// Pointed arch
//

// Inputs of the pointed arch construction.
//
//   pL, pR : "base points" = centers of the two construction circles. NOT the
//            visible foot points of the arch in general (they coincide only for
//            excess == 1, the equilateral case). For excess > 1 the arch's
//            ground footprint extends beyond pL and pR; for excess < 1 it stays
//            inside.
//   excess : ratio r / dist(pL, pR). Must be > 0.5 strict (boundary case 0.5
//            gives two tangent circles, an apex of height 0, and is excluded as
//            a degenerate input).
//
struct ArchBasis
{
	Vector2d pL;
	Vector2d pR;
	double   excess = 1.0;
};

// Result of the pointed arch construction.
//
// Naming convention :
//   arcLeft  is drawn on circleL (centered at pL) and traces the *right* visible
//            half of the arch (from the pR-direction up to the apex).
//   arcRight is drawn on circleR (centered at pR) and traces the *left*  visible
//            half of the arch (from the apex down to the pL-direction).
// "Left/Right" refers to which construction circle, not which side of the arch.
// Convention faithful to the Havemann thesis.
//
struct ArchGeom
{
	Circle   circleL;            // centered at pL, radius excess * width
	Circle   circleR;            // centered at pR, same radius
	Vector2d apex;                // upper intersection of circleL and circleR
	double   width  = 0.0;        // dist(pL, pR)
	double   height = 0.0;        // apex.y - pL.y (assumes a horizontal base)
	Arc      arcLeft;             // on circleL, traces the right visible half (CCW)
	Arc      arcRight;            // on circleR, traces the left  visible half (CCW)
};

// Build the pointed arch geometry from an ArchBasis.
//
// Throws std::invalid_argument when :
//   - pL or pR contain non-finite coordinates,
//   - excess is non-finite,
//   - excess <= 0.5 (strict : excludes the degenerate tangent case),
//   - dist(pL, pR) is below a small tolerance (pL ~ pR).
//
// Throws std::runtime_error when the construction circles fail to intersect.
// This indicates either a numerical issue or a precondition violation that
// escaped the input checks above.
//
ArchGeom buildArch (const ArchBasis &basis);


//
// Pointed arch offset (fixed-centers method)
//
// Reference : Havemann thesis section 2.4. Builds an inner and an outer arch
// parallel to a base arch, by keeping the construction centers (mL, mR) fixed
// and only changing the radii. The construction guarantees C1 continuity at
// the foot points (tangents are radial from the shared center).
//

// Offsets are non-negative thicknesses. inner shrinks the radius (toward the
// inside of the bay), outer grows it (toward the stone face).
struct ArchOffsetParams
{
	double outer = 0.0;     // delta_out > 0 -> r_out = r + outer
	double inner = 0.0;     // delta_in  > 0 -> r_in  = r - inner
};

struct ArchOffsetGeom
{
	ArchGeom inner;                 // inner arch (bay void)
	ArchGeom outer;                 // outer arch (stone face)
	double   stoneWidthActual = 0.0; // outer.apex.y - inner.apex.y (vertical band thickness at apex)
};

// Build the inner/outer offset arches from a base arch.
//
// Precondition : `geom` must come from a valid buildArch() call. Hand-built
// ArchGeom with inconsistent fields is undefined behavior.
//
// Throws std::invalid_argument when :
//   - outer or inner is non-finite,
//   - outer < 0 or inner < 0,
//   - inner >= geom.circleL.radius (inner arch radius would be <= 0),
//   - inner > geom.circleL.radius - geom.width / 2 (inner arch collapses :
//     its construction circles cannot meet above the base).
//
ArchOffsetGeom buildArchOffset (const ArchGeom &geom, const ArchOffsetParams &params);


//
// Subwindows (lancets)
//
// Reference : Havemann thesis section 2.2, Fig. 5b. Subdivides the main bay
// into `count` lancets separated by `count + 1` mullions (the gaps between
// lancets, including at both ends). Each lancet is a small pointed arch with
// its own offset, anchored at a level dropped vertically below the main arch
// base.
//

// Subwindow input parameters.
struct SubwindowParams
{
	struct Gap
	{
		// Gap (mullion) width can be specified either as a fraction of the
		// main arch width, or as an absolute distance.
		enum class Mode { Fraction, Absolute };
		Mode   mode          = Mode::Fraction;
		double gapFraction   = 0.0;     // active when mode == Fraction (in (0, 1/(count+1)) )
		double absoluteWidth = 0.0;     // active when mode == Absolute (in (0, mainWidth/(count+1)) )
	};

	int    count  = 1;          // number of lancets, in [1, 6]
	double drop   = 0.0;        // vertical descent of lancet bases below the main arch base, >= 0
	double excess = 1.0;        // excess of each sub-arch
	Gap    gap;
};

// Geometry of a single lancet.
struct LancetGeom
{
	ArchBasis      basis;       // (pL_i, pR_i, params.excess)
	ArchGeom       arch;        // buildArch(basis)
	ArchOffsetGeom offset;      // buildArchOffset(arch, offsetParams)
	double         subWidth = 0.0;     // arc base width (= dist(pL_i, pR_i))
	Vector2d       center;             // midpoint of the base, at y = mainArch.foot.y - drop
};

// Geometry of the full subwindow row.
struct SubwindowsGeom
{
	std::vector<LancetGeom> lancets;   // ordered left-to-right
	double                  gapWidth = 0.0;   // actual mullion width used
};

// Build the subwindow row.
//
// Throws std::invalid_argument when :
//   - count is not in [1, 6],
//   - drop is non-finite or negative,
//   - excess is non-finite or <= 0.5 (pre-validated, identical to buildArch),
//   - offsetParams contain non-finite or negative values (pre-validated),
//   - gap parameters are non-finite or non-positive,
//   - the resulting gapWidth or subWidth would be non-positive.
//
// May propagate std::invalid_argument from buildArchOffset on a per-lancet
// basis when the offset would collapse a sub-arch (for example when `inner`
// is too large for the lancet's smaller radius). The error message comes from
// the underlying buildArchOffset call.
//
SubwindowsGeom buildSubwindows (const ArchGeom         &mainArch,
                                const ArchOffsetParams &offsetParams,
                                const SubwindowParams  &params);


//
// Rosette (crowning circle)
//
// Reference : Havemann thesis section 2.3. The rosette circle that crowns the
// main arch is positioned by intersecting an ellipse (foci at the centers of
// the adjacent main- and sub-arch construction circles, focal sum equal to the
// sum of those radii) with the symmetry axis of the main arch. The resulting
// circle is internally tangent to the main arch's foot circle and externally
// tangent to the right sub-arch's foot circle.
//

struct RosetteGeom
{
	Circle   circle;             // = Circle(center, radius), kept for completeness
	Vector2d center;             // mC : intersection of focal-sum ellipse with the symmetry axis
	double   radius = 0.0;       // r_ros = mainArch.circleR.radius - dist(mC, F1)
};

// Build the rosette geometry from the main arch and the subwindow row.
//
// Throws std::invalid_argument when :
//   - subwindows.lancets is empty.
//
// Throws std::runtime_error when the geometric construction fails :
//   - the focal-sum ellipse does not intersect the symmetry axis at 2 points,
//   - none of the candidate intersections lies between the main-arch apex and
//     the rightmost sub-arch's apex (incoherent input geometry),
//   - the resulting rosette radius would be non-positive.
//
RosetteGeom buildRosette (const ArchGeom &mainArch, const SubwindowsGeom &subwindows);


//
// Foil ring (rosette or lancette head)
//
// Reference : Havemann thesis, Fig. 9 (round foils) and Fig. 10 (pointed foils).
// Builds n foils inscribed in a circle of radius R. The foil ring tessellates
// the rim of the outer circle into n congruent foils (round = circles, pointed
// = pairs of arcs forming a cusp).
//
// Round foils :
//   alpha = 2*pi / n
//   fr    = sin(alpha/2) / (1 + sin(alpha/2)) * R       (foil radius)
//   Rm    = R - fr                                       (foil center distance)
//   foil i center fc_i = outerCircle.center + Rm * (cos(theta_i), sin(theta_i))
//   theta_i = phi0 + i*alpha   (+ pi/n if orientLying)
//
// Pointed foils :
//   For each foil i, two arcs join at an apex (cusp) :
//     a_i = outerCircle.center + R*(cos(phi0 + (i-0.5)*alpha), ...)   on outer rim
//     b_i = outerCircle.center + R*(cos(phi0 + (i+0.5)*alpha), ...)   on outer rim
//     m   = outerCircle.center + Rm*(cos(phi0 + i*alpha), ...)
//     m'  = m + disp * normalize(a_i - m),   r' = |a_i - m| - disp
//     m'' = m + disp * normalize(b_i - m),   r'' = |b_i - m| - disp
//     disp = pointedness * fr
//     apex = inner intersection of Circle(m', r') and Circle(m'', r'')
//            (i.e. the one closer to outerCircle.center)
//

enum class FoilType
{
	Round,
	Pointed
};

struct FoilsParams
{
	int      count       = 6;            // n in [3, 24]
	FoilType type        = FoilType::Round;
	double   pointedness = 0.0;          // for Pointed : > 0 strict ; ignored for Round
	double   phi0        = 0.0;          // initial rotation in radians
	bool     orientLying = false;        // true => phi0 += pi/n (Fig. 8)
};

// Round foil : a single circle inscribed in the rosette.
struct RoundFoil
{
	Circle circle;
};

// Pointed foil : two arcs forming a cusp pointing toward the rosette center.
struct PointedFoil
{
	Circle   circleLeft;     // circle hosting arcLeft (centered at m')
	Circle   circleRight;    // circle hosting arcRight (centered at m'')
	Arc      arcLeft;        // from baseLeft to apex on circleLeft
	Arc      arcRight;       // from apex to baseRight on circleRight
	Vector2d apex;           // cusp tip (closer to outerCircle.center than m)
	Vector2d baseLeft;       // = a_i, on outerCircle
	Vector2d baseRight;      // = b_i, on outerCircle
};

struct FoilRing
{
	Circle outerCircle;                                                  // R
	double foilRadius = 0.0;                                             // fr
	std::vector<std::variant<RoundFoil, PointedFoil>> foils;             // ordered i = 0..count-1
};

// Build the foil ring inscribed in `outerCircle`.
//
// Throws std::invalid_argument when :
//   - count < 3 or count > 24,
//   - phi0 is non-finite,
//   - outerCircle.radius <= 0,
//   - type is Pointed and pointedness <= 0 (degenerate ; for Round, use type=Round),
//   - type is Pointed and pointedness > 2 (schema bound),
//   - type is Pointed and pointedness * fr >= |a_i - m| (would yield a
//     non-positive arc radius). The actual geometric upper bound depends on n.
//
// For type=Round, the `pointedness` field is silently ignored (the schema
// already constrains it to 0 for Round in the JSON layer).
//
FoilRing buildFoilRing (const Circle &outerCircle, const FoilsParams &params);


//
// Helpers
//

// Largest inscribed circle of a pointed arch.
//
// The returned circle is tangent to the base line (y = pL.y) AND internally
// tangent to both foot-circle arcs. Center on the symmetry axis :
//   center = ((pL.x + pR.x)/2,  pL.y + rk)
//   rk     = (r^2 - (width/2)^2) / (2 r)
// where r = arch.circleL.radius, width = arch.width.
//
// Used as the "head circle" hosting foils inscribed in a lancet.
//
// Throws std::invalid_argument if the arch is too flat (r <= width/2). For
// arches built with excess > 0.5 (the usual constraint of buildArch), this
// case never triggers.
//
Circle inscribedCircleOfPointedArch (const ArchGeom &arch);


//
// Trefoil arch
//
// Reference : Havemann thesis section 2.6, Fig. 12. Modifies a pointed arch by
// cutting each half-arc at `splitParameter` (a fraction along the arc from
// foot to apex) and inserting a small foil arc, tangent (C1) to the main arc
// at the cut point.
//
// Convention used here :
//   - The foil center is OUTWARD from the main circle :
//       mf = splitPoint + dir * r_f                with dir = normalize(splitPoint - mainCircle.center)
//     so the foil circle sits outside the main silhouette and is tangent to
//     the main arc only at splitPoint (single contact). This matches the
//     spec's literal formulas.
//   - The foil arc is a SEMICIRCLE (180 deg) around mf, starting at splitPoint
//     and ending at the antipodal point on the foil circle :
//       footPoint = splitPoint + 2 * dir * r_f
//     The arc orientation (ccw flag) is chosen so that the arc bumps downward
//     (assumes a horizontal-base arch).
//
// IMPORTANT divergence from spec text : the foil circle does NOT pass through
// pL/pR in general (it cannot, with a free `foilRadiusFactor`). The
// `footPoint` field is the END of the semicircle, not the foot of the original
// arch. C1 continuity at splitPoint is preserved by the radial-line
// construction.
//

struct TrefoilParams
{
	double splitParameter   = 0.45;   // in (0, 1) strict, position of the cut along each main half-arc
	double foilRadiusFactor = 0.30;   // in (0, 0.5], r_f = factor * arch.circleL.radius
};

struct TrefoilFoilArc
{
	Circle   circle;       // foil circle (centered at mf, radius r_f)
	Arc      arc;          // semicircle from splitPoint to footPoint
	Vector2d splitPoint;   // tangent contact with the main arc (on main circle)
	Vector2d footPoint;    // antipodal endpoint of the semicircle (NOT pL/pR)
};

struct TrefoilArchGeom
{
	ArchGeom       base;          // the original arch (unchanged)
	Arc            upperLeft;     // upper part of arcLeft : from splitL to apex (CCW on circleL)
	Arc            upperRight;    // upper part of arcRight : from apex to splitR (CCW on circleR)
	TrefoilFoilArc foilLeft;      // small foil tangent at splitL (on circleL)
	TrefoilFoilArc foilRight;     // small foil tangent at splitR (on circleR)
};

// Build the trefoil arch from a base pointed arch.
//
// Throws std::invalid_argument when :
//   - splitParameter is non-finite or not in the open interval (0, 1),
//   - foilRadiusFactor is non-finite, <= 0, or > 0.5.
//
TrefoilArchGeom buildTrefoilArch (const ArchGeom &arch, const TrefoilParams &params);


//
// Fillets (curved triangular regions)
//
// Reference : Havemann thesis section 3.1. The fillet regions are bounded by
// three concurrent arc segments. Each fillet has 3 corners (pairwise arc
// intersections) and 3 arcs trimmed corner-to-corner.
//
// Scope of this minimal implementation : only LATERAL fillets are built.
// There are exactly 2 of them (left and right) regardless of the lancet count :
//   - LEFT  lateral : main_inner_R ∩ leftmost_lancet_outer_R ∩ rosette
//   - RIGHT lateral : main_inner_L ∩ rightmost_lancet_outer_L ∩ rosette
//
// CENTRAL fillets (between adjacent lancets) are NOT built : in many natural
// configurations the relevant lancet outer-offset circles do not intersect
// the rosette (the rosette sits above the inter-lancet gap with no direct
// boundary), making the 3-arc construction non-trivial / config-dependent.
// Future extension : build central fillets bounded by 4 arcs (main inner +
// 2 lancet sides + rosette) when the rosette does not reach down, or extend
// the data model to support 4-sided fillets.
//
// Corner selection heuristic : for each pairwise circle intersection (which
// has 2 candidate points generally), pick the candidate closest to the THIRD
// circle's BOUNDARY. This selects the corner facing the fillet region.
//

// One fillet : 3 corners + 3 arcs trimmed corner-to-corner.
//
// Convention :
//   cornerA = arcOuter   ∩ arcSub
//   cornerB = arcSub     ∩ arcRosette
//   cornerC = arcRosette ∩ arcOuter
//
// Arc orientations are chosen so each is the SHORTER of the two possible arcs
// between its corner pair (|spanAngle| < pi).
//
struct FilletGeom
{
	Arc      arcOuter;     // on main_inner circle (or lancet outer for centrals when implemented)
	Arc      arcSub;       // on adjacent lancet outer circle
	Arc      arcRosette;   // on rosette circle
	Vector2d cornerA;      // arcOuter ∩ arcSub
	Vector2d cornerB;      // arcSub ∩ arcRosette
	Vector2d cornerC;      // arcRosette ∩ arcOuter
};

struct FilletsGeom
{
	std::vector<FilletGeom> fillets;   // size 2 in this minimal implementation (left + right lateral)
};

// Build the lateral fillet regions.
//
// Throws std::invalid_argument when :
//   - subwindows.lancets is empty,
//   - stoneBandWidth is non-finite or non-positive (validated, not yet used).
//
// Throws std::runtime_error when a required pairwise circle intersection has
// no points (incoherent geometry).
//
FilletsGeom buildFillets (const ArchOffsetGeom &mainOffset,
                          const SubwindowsGeom &subwindows,
                          const RosetteGeom    &rosette,
                          double                stoneBandWidth);


//
// Mouchettes (flamboyant Gothic decorative shapes)
//
// Reference : Havemann thesis section 3.6, Fig. 20 e-f. Decorative shapes
// placed in the inter-lancet gaps (one per gap, above the lancet apexes).
//
// Conventions in this minimal implementation :
//   - Placement : one mouchette per gap between consecutive lancets
//                 (size = lancets.size() - 1). For a single lancet : empty.
//                 Center : (midpoint of gap, max(adjacent apex .y) + radius).
//   - Size      : radius = radiusFactor * subwindow width.
//   - Rotation  : applied as a rotation of the shape around its center.
//   - Shapes    :
//       * Vesica   : the classical vesica piscis (lens of two equal circles
//                    whose centers are r apart). Contour : 2 arcs (span 2*pi/3 each).
//       * Teardrop : circle base (radius r) with two tangent side arcs joining
//                    at an apex at distance 2r from the center. Tangent points
//                    at angles (rotation +/- pi/2) on the base circle.
//                    Contour : 3 arcs (base back arc + 2 side arcs).
//       * Soufflet : aliased to Teardrop in this minimal implementation. The
//                    proper flamboyant "bellows" curl is not yet implemented.
//

enum class MouchetteType
{
	Vesica,
	Teardrop,
	Soufflet     // currently aliased to Teardrop
};

struct MouchetteParams
{
	MouchetteType type         = MouchetteType::Vesica;
	double        radiusFactor = 0.18;     // in (0, 0.5], r = factor * subWidth
	double        rotation     = 0.0;
};

struct MouchetteGeom
{
	Vector2d         center;
	double           radius = 0.0;
	std::vector<Arc> contour;            // 2 arcs (Vesica) or 3 arcs (Teardrop / Soufflet)
};

// Build the mouchettes inscribed between consecutive lancets.
//
// Returns an empty vector when subwindows has fewer than 2 lancets.
//
// Throws std::invalid_argument when :
//   - subwindows.lancets is empty,
//   - radiusFactor is non-finite, <= 0, or > 0.5,
//   - rotation is non-finite.
//
std::vector<MouchetteGeom> buildMouchettes (const SubwindowsGeom  &subwindows,
                                             const MouchetteParams &params);
