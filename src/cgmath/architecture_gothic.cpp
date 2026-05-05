#include <cmath>
#include <stdexcept>

#include "architecture_gothic.h"

namespace
{
	inline bool isFiniteVector (const Vector2d &v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y);
	}
}

ArchGeom buildArch (const ArchBasis &basis)
{
	// --- Input validation ------------------------------------------------
	if (!isFiniteVector(basis.pL))
		throw std::invalid_argument("buildArch: pL must have finite coordinates");
	if (!isFiniteVector(basis.pR))
		throw std::invalid_argument("buildArch: pR must have finite coordinates");
	if (!std::isfinite(basis.excess))
		throw std::invalid_argument("buildArch: excess must be finite");
	if (basis.excess <= 0.5)
		throw std::invalid_argument("buildArch: excess must be strictly greater than 0.5");

	double width = Vector2d::Distance(basis.pL, basis.pR);
	if (width < 1e-12)
		throw std::invalid_argument("buildArch: pL and pR must be distinct (width > 0)");

	// --- Construction ----------------------------------------------------
	ArchGeom g;
	g.width   = width;
	double r  = basis.excess * width;
	g.circleL = Circle(basis.pL, r);
	g.circleR = Circle(basis.pR, r);

	Circle::IntersectionResult inter = g.circleL.intersection(g.circleR);
	if (inter.count < 1)
		throw std::runtime_error("buildArch: construction circles do not intersect");

	g.apex   = inter.pts[0];                          // upper intersection
	g.height = g.apex.y - basis.pL.y;                 // assumes horizontal base

	// arcLeft  : on circleL, from the pR-direction (start) to the apex (end), CCW.
	//            Traces the right visible half of the arch.
	double aLstart = g.circleL.angleAt(basis.pR);
	double aLend   = g.circleL.angleAt(g.apex);
	g.arcLeft  = Arc(g.circleL, aLstart, aLend, true);

	// arcRight : on circleR, from the apex (start) to the pL-direction (end), CCW.
	//            Traces the left visible half of the arch.
	double aRstart = g.circleR.angleAt(g.apex);
	double aRend   = g.circleR.angleAt(basis.pL);
	g.arcRight = Arc(g.circleR, aRstart, aRend, true);

	return g;
}


//
// Pointed arch offset (fixed-centers method)
//

ArchOffsetGeom buildArchOffset (const ArchGeom &geom, const ArchOffsetParams &params)
{
	// --- Input validation ------------------------------------------------
	if (!std::isfinite(params.outer))
		throw std::invalid_argument("buildArchOffset: outer must be finite");
	if (!std::isfinite(params.inner))
		throw std::invalid_argument("buildArchOffset: inner must be finite");
	if (params.outer < 0.0)
		throw std::invalid_argument("buildArchOffset: outer must be non-negative");
	if (params.inner < 0.0)
		throw std::invalid_argument("buildArchOffset: inner must be non-negative");

	double r = geom.circleL.radius;
	double w = geom.width;

	// Spec : inner < radius (otherwise inner arch radius would be <= 0).
	if (params.inner >= r)
		throw std::invalid_argument("buildArchOffset: inner must be strictly less than the base radius");

	// Tightened : the inner arch must remain a valid pointed arch, which
	// requires r_in > w/2 strict (i.e. the inner construction circles must
	// intersect strictly above the base). Equivalently, inner < r - w/2 strict.
	// Without this, the inner arch would have excess <= 0.5, which buildArch
	// rejects with a misleading "excess" error message.
	if (params.inner >= r - 0.5 * w)
		throw std::invalid_argument("buildArchOffset: inner offset too large (would collapse the inner arch)");

	// --- Construction ----------------------------------------------------
	// Same basis (mL, mR) for both, only the excess (= radius/width) changes.
	ArchBasis innerBasis;
	innerBasis.pL     = geom.circleL.center;
	innerBasis.pR     = geom.circleR.center;
	innerBasis.excess = (r - params.inner) / w;

	ArchBasis outerBasis;
	outerBasis.pL     = geom.circleL.center;
	outerBasis.pR     = geom.circleR.center;
	outerBasis.excess = (r + params.outer) / w;

	ArchOffsetGeom result;
	result.inner = buildArch(innerBasis);
	result.outer = buildArch(outerBasis);
	result.stoneWidthActual = result.outer.apex.y - result.inner.apex.y;
	return result;
}


//
// Subwindows (lancets)
//

namespace
{
	void validateOffsetParams (const ArchOffsetParams &p)
	{
		if (!std::isfinite(p.outer))
			throw std::invalid_argument("buildSubwindows: offsetParams.outer must be finite");
		if (!std::isfinite(p.inner))
			throw std::invalid_argument("buildSubwindows: offsetParams.inner must be finite");
		if (p.outer < 0.0)
			throw std::invalid_argument("buildSubwindows: offsetParams.outer must be non-negative");
		if (p.inner < 0.0)
			throw std::invalid_argument("buildSubwindows: offsetParams.inner must be non-negative");
	}
}

SubwindowsGeom buildSubwindows (const ArchGeom         &mainArch,
                                const ArchOffsetParams &offsetParams,
                                const SubwindowParams  &params)
{
	// --- Input validation : structural -----------------------------------
	if (params.count < 1 || params.count > 6)
		throw std::invalid_argument("buildSubwindows: count must be in [1, 6]");

	if (!std::isfinite(params.drop))
		throw std::invalid_argument("buildSubwindows: drop must be finite");
	if (params.drop < 0.0)
		throw std::invalid_argument("buildSubwindows: drop must be non-negative");

	if (!std::isfinite(params.excess))
		throw std::invalid_argument("buildSubwindows: excess must be finite");
	if (params.excess <= 0.5)
		throw std::invalid_argument("buildSubwindows: excess must be strictly greater than 0.5");

	validateOffsetParams(offsetParams);

	// --- Input validation : gap ------------------------------------------
	double gapWidth;
	if (params.gap.mode == SubwindowParams::Gap::Mode::Fraction)
	{
		if (!std::isfinite(params.gap.gapFraction))
			throw std::invalid_argument("buildSubwindows: gap.gapFraction must be finite");
		if (params.gap.gapFraction <= 0.0)
			throw std::invalid_argument("buildSubwindows: gap.gapFraction must be strictly positive");
		gapWidth = params.gap.gapFraction * mainArch.width;
	}
	else  // Absolute
	{
		if (!std::isfinite(params.gap.absoluteWidth))
			throw std::invalid_argument("buildSubwindows: gap.absoluteWidth must be finite");
		if (params.gap.absoluteWidth <= 0.0)
			throw std::invalid_argument("buildSubwindows: gap.absoluteWidth must be strictly positive");
		gapWidth = params.gap.absoluteWidth;
	}

	const int    n        = params.count;
	const double totalGap = (n + 1) * gapWidth;
	const double subWidth = (mainArch.width - totalGap) / n;
	if (subWidth <= 0.0)
		throw std::invalid_argument("buildSubwindows: gaps consume the full width (subWidth <= 0)");

	// --- Construction ----------------------------------------------------
	SubwindowsGeom result;
	result.gapWidth = gapWidth;
	result.lancets.reserve(n);

	const double y0     = mainArch.circleL.center.y - params.drop;
	const double xOrig  = mainArch.circleL.center.x;     // pL_main.x

	for (int i = 0; i < n; ++i)
	{
		const double x0 = xOrig + (i + 1) * gapWidth + i * subWidth;
		const double x1 = x0 + subWidth;

		LancetGeom lancet;
		lancet.basis.pL     = Vector2d(x0, y0);
		lancet.basis.pR     = Vector2d(x1, y0);
		lancet.basis.excess = params.excess;
		lancet.arch         = buildArch(lancet.basis);
		lancet.offset       = buildArchOffset(lancet.arch, offsetParams);
		lancet.subWidth     = subWidth;
		lancet.center       = Vector2d((x0 + x1) / 2.0, y0);
		result.lancets.push_back(lancet);
	}
	return result;
}


//
// Rosette
//

RosetteGeom buildRosette (const ArchGeom &mainArch, const SubwindowsGeom &subwindows)
{
	if (subwindows.lancets.empty())
		throw std::invalid_argument("buildRosette: subwindows must contain at least one lancet");

	// Foci of the focal-sum ellipse :
	//   F1 = center of the main arch's right foot circle
	//   F2 = center of the rightmost sub-arch's left foot circle (adjacent to the symmetry axis)
	const LancetGeom &rightLancet = subwindows.lancets.back();
	Vector2d F1 = mainArch.circleR.center;
	Vector2d F2 = rightLancet.arch.circleL.center;

	double rMain = mainArch.circleR.radius;
	double rSub  = rightLancet.arch.circleL.radius;
	double sumDist = rMain + rSub;

	Ellipse ellipse(F1, F2, sumDist);

	// Symmetry axis : midpoint of the main arch construction centers.
	double cx = (mainArch.circleL.center.x + mainArch.circleR.center.x) / 2.0;

	Ellipse::IntersectionResult inter = ellipse.verticalIntersection(cx);
	if (inter.count < 2)
		throw std::runtime_error("buildRosette: ellipse does not intersect the symmetry axis at 2 points");

	// Pick the candidate whose y lies between the rightmost sub-arch apex (lower
	// bound) and the main arch apex (upper bound). For a well-formed input the
	// upper intersection (pts[0]) is the right one, but we check explicitly for
	// robustness.
	double yLo = rightLancet.arch.apex.y;
	double yHi = mainArch.apex.y;

	Vector2d mC;
	bool found = false;
	for (int i = 0; i < inter.count; ++i)
	{
		Vector2d p = inter.pts[i];
		if (p.y >= yLo && p.y <= yHi)
		{
			mC = p;
			found = true;
			break;
		}
	}
	if (!found)
		throw std::runtime_error("buildRosette: no ellipse intersection lies between the sub-arch apex and the main-arch apex");

	double r_ros = rMain - Vector2d::Distance(mC, F1);
	if (r_ros <= 0.0)
		throw std::runtime_error("buildRosette: computed rosette radius is non-positive");

	RosetteGeom result;
	result.center = mC;
	result.radius = r_ros;
	result.circle = Circle(mC, r_ros);
	return result;
}


//
// Foil ring
//

namespace
{
	// Pick ccw such that the arc from `from` to `to` on `c` is the shorter one
	// (i.e. delta angle has magnitude < pi).
	bool pickShorterArcDirection (const Circle &c, Vector2d from, Vector2d to)
	{
		const double TWO_PI = 2.0 * M_PI;
		double a0 = c.angleAt(from);
		double a1 = c.angleAt(to);
		double delta = a1 - a0;
		while (delta >  M_PI) delta -= TWO_PI;
		while (delta < -M_PI) delta += TWO_PI;
		return delta >= 0.0;
	}
}

FoilRing buildFoilRing (const Circle &outerCircle, const FoilsParams &params)
{
	// --- Input validation ------------------------------------------------
	if (params.count < 3)
		throw std::invalid_argument("buildFoilRing: count must be >= 3");
	if (params.count > 24)
		throw std::invalid_argument("buildFoilRing: count must be <= 24");
	if (!std::isfinite(params.phi0))
		throw std::invalid_argument("buildFoilRing: phi0 must be finite");
	if (outerCircle.radius <= 0.0)
		throw std::invalid_argument("buildFoilRing: outerCircle.radius must be strictly positive");

	if (params.type == FoilType::Pointed)
	{
		if (!std::isfinite(params.pointedness))
			throw std::invalid_argument("buildFoilRing: pointedness must be finite");
		if (params.pointedness <= 0.0)
			throw std::invalid_argument("buildFoilRing: pointedness must be > 0 for Pointed type (use type=Round for round foils)");
		if (params.pointedness > 2.0)
			throw std::invalid_argument("buildFoilRing: pointedness must be <= 2");
	}
	// For type=Round, pointedness is silently ignored.

	const int    n            = params.count;
	const double R            = outerCircle.radius;
	const double TWO_PI       = 2.0 * M_PI;
	const double alpha        = TWO_PI / n;
	const double halfAlpha    = alpha / 2.0;
	const double sinHalfAlpha = std::sin(halfAlpha);
	const double fr           = sinHalfAlpha / (1.0 + sinHalfAlpha) * R;
	const double Rm           = R - fr;

	// |a_i - m| is constant for all foils by symmetry. Compute once.
	// |a_i - m|^2 = R^2 + Rm^2 - 2*R*Rm*cos(alpha/2)
	const double amDist2 = R * R + Rm * Rm - 2.0 * R * Rm * std::cos(halfAlpha);
	const double amDist  = std::sqrt(amDist2);

	if (params.type == FoilType::Pointed)
	{
		const double disp = params.pointedness * fr;
		if (disp >= amDist)
			throw std::invalid_argument("buildFoilRing: pointedness too large for given count (would yield non-positive arc radius)");
	}

	const double phi0 = params.phi0 + (params.orientLying ? halfAlpha : 0.0);
	const Vector2d cc = outerCircle.center;

	FoilRing ring;
	ring.outerCircle = outerCircle;
	ring.foilRadius  = fr;
	ring.foils.reserve(n);

	for (int i = 0; i < n; ++i)
	{
		const double thetaI = phi0 + i * alpha;
		const double cosI   = std::cos(thetaI);
		const double sinI   = std::sin(thetaI);
		const Vector2d m(cc.x + Rm * cosI, cc.y + Rm * sinI);

		if (params.type == FoilType::Round)
		{
			ring.foils.emplace_back(RoundFoil{ Circle(m, fr) });
			continue;
		}

		// --- Pointed foil --------------------------------------------------
		const double thetaA = phi0 + (i - 0.5) * alpha;
		const double thetaB = phi0 + (i + 0.5) * alpha;
		const Vector2d aI(cc.x + R * std::cos(thetaA), cc.y + R * std::sin(thetaA));
		const Vector2d bI(cc.x + R * std::cos(thetaB), cc.y + R * std::sin(thetaB));

		const double disp = params.pointedness * fr;

		// m' moves m toward a_i by `disp`.
		const double dxA = aI.x - m.x, dyA = aI.y - m.y;
		const double mPrimeX = m.x + disp * dxA / amDist;
		const double mPrimeY = m.y + disp * dyA / amDist;
		const double rPrime  = amDist - disp;
		const Circle circleL(Vector2d(mPrimeX, mPrimeY), rPrime);

		// m'' moves m toward b_i by `disp`.
		const double dxB = bI.x - m.x, dyB = bI.y - m.y;
		const double mDoubleX = m.x + disp * dxB / amDist;
		const double mDoubleY = m.y + disp * dyB / amDist;
		const double rDouble  = amDist - disp;
		const Circle circleR(Vector2d(mDoubleX, mDoubleY), rDouble);

		Circle::IntersectionResult inter = circleL.intersection(circleR);
		if (inter.count < 2)
			throw std::runtime_error("buildFoilRing: pointed foil arc circles do not intersect at 2 points");

		// Pick the inner intersection (closer to outerCircle.center).
		const double d0 = Vector2d::Distance(inter.pts[0], cc);
		const double d1 = Vector2d::Distance(inter.pts[1], cc);
		const Vector2d apex = (d0 < d1) ? inter.pts[0] : inter.pts[1];

		PointedFoil pf;
		pf.circleLeft  = circleL;
		pf.circleRight = circleR;
		pf.baseLeft    = aI;
		pf.baseRight   = bI;
		pf.apex        = apex;

		// Build the arcs taking the shorter direction on each circle.
		const bool ccwL = pickShorterArcDirection(circleL, aI, apex);
		pf.arcLeft = Arc(circleL, aI, apex, ccwL);

		const bool ccwR = pickShorterArcDirection(circleR, apex, bI);
		pf.arcRight = Arc(circleR, apex, bI, ccwR);

		ring.foils.emplace_back(pf);
	}

	return ring;
}


//
// Helpers
//

Circle inscribedCircleOfPointedArch (const ArchGeom &arch)
{
	double r = arch.circleL.radius;
	double halfW = arch.width / 2.0;
	if (r <= halfW)
		throw std::invalid_argument("inscribedCircleOfPointedArch: arch is too flat (r <= width/2)");

	double rk = (r * r - halfW * halfW) / (2.0 * r);
	double cx = (arch.circleL.center.x + arch.circleR.center.x) / 2.0;
	double cy = arch.circleL.center.y + rk;
	return Circle(Vector2d(cx, cy), rk);
}


//
// Trefoil arch
//

namespace
{
	// Compute one trefoil "side" (foil arc + upper main arc).
	//
	// `splitT` is measured FROM THE FOOT toward the apex on both sides
	// (uniform convention preserving left/right symmetry of the result).
	// `downDir` is a unit vector pointing FROM the apex TOWARD the base
	// midpoint (i.e. the "downward" direction in the arch's local frame).
	// It is used to orient the foil semicircle so that it bulges away from
	// the apex (toward the foot of the arch), independently of the base's
	// orientation in world coordinates.
	// `upperFromSplitToApex` selects how the upper main arc is oriented :
	//   - true  : upper goes from splitPoint to apex (LEFT side : matches arcLeft direction)
	//   - false : upper goes from apex to splitPoint (RIGHT side : matches arcRight direction)
	void buildTrefoilSide (const Circle &mainCircle,
	                       double angFoot, double angApex, double splitT,
	                       double r_f,
	                       Vector2d downDir,
	                       Arc &upperOut, TrefoilFoilArc &foilOut,
	                       bool upperFromSplitToApex)
	{
		double angSplit = angFoot + splitT * (angApex - angFoot);
		Vector2d splitPoint = mainCircle.pointAt(angSplit);

		// Outward radial direction at splitPoint.
		double dx = splitPoint.x - mainCircle.center.x;
		double dy = splitPoint.y - mainCircle.center.y;
		double len = std::sqrt(dx * dx + dy * dy);
		double dirX = dx / len;
		double dirY = dy / len;

		Vector2d mf(splitPoint.x + dirX * r_f, splitPoint.y + dirY * r_f);
		Vector2d footPoint(splitPoint.x + 2.0 * dirX * r_f, splitPoint.y + 2.0 * dirY * r_f);

		Circle foilCircle(mf, r_f);

		// Choose ccw such that the semicircle bulges along `downDir`
		// (= toward the base of the arch, away from the apex).
		//
		// On foilCircle, splitPoint is at angle theta with (cos theta, sin theta)
		// = (-dirX, -dirY). The CCW tangent there is (-sin theta, cos theta)
		// = (dirY, -dirX). We pick ccw=true iff this CCW-tangent has positive
		// dot product with downDir (the arc starts moving in the down direction).
		double tangentDotDown = dirY * downDir.x + (-dirX) * downDir.y;
		bool ccw = (tangentDotDown > 0.0);
		Arc foilArc(foilCircle, splitPoint, footPoint, ccw);

		// Upper part of the main arc, on `mainCircle`, CCW.
		if (upperFromSplitToApex)
			upperOut = Arc(mainCircle, angSplit, angApex, true);   // splitL -> apex
		else
			upperOut = Arc(mainCircle, angApex, angSplit, true);   // apex -> splitR

		foilOut.circle     = foilCircle;
		foilOut.arc        = foilArc;
		foilOut.splitPoint = splitPoint;
		foilOut.footPoint  = footPoint;
	}
}

TrefoilArchGeom buildTrefoilArch (const ArchGeom &arch, const TrefoilParams &params)
{
	// --- Input validation ------------------------------------------------
	if (!std::isfinite(params.splitParameter))
		throw std::invalid_argument("buildTrefoilArch: splitParameter must be finite");
	if (params.splitParameter <= 0.0 || params.splitParameter >= 1.0)
		throw std::invalid_argument("buildTrefoilArch: splitParameter must be in (0, 1)");

	if (!std::isfinite(params.foilRadiusFactor))
		throw std::invalid_argument("buildTrefoilArch: foilRadiusFactor must be finite");
	if (params.foilRadiusFactor <= 0.0)
		throw std::invalid_argument("buildTrefoilArch: foilRadiusFactor must be strictly positive");
	if (params.foilRadiusFactor > 0.5)
		throw std::invalid_argument("buildTrefoilArch: foilRadiusFactor must be <= 0.5");

	TrefoilArchGeom result;
	result.base = arch;

	double r_f = params.foilRadiusFactor * arch.circleL.radius;

	// "Downward" direction = unit vector from apex to base midpoint. This is the
	// direction in which the foil semicircle should bulge (toward the foot of
	// the arch, away from the apex), independent of the base's orientation.
	double midX = (arch.circleL.center.x + arch.circleR.center.x) / 2.0;
	double midY = (arch.circleL.center.y + arch.circleR.center.y) / 2.0;
	double dx   = midX - arch.apex.x;
	double dy   = midY - arch.apex.y;
	double dlen = std::sqrt(dx * dx + dy * dy);
	if (dlen < 1e-12)
		throw std::runtime_error("buildTrefoilArch: base midpoint coincides with apex (degenerate arch)");
	Vector2d downDir(dx / dlen, dy / dlen);

	// Left side (using circleL) : foot at angle to pR, apex above.
	double angFootL = arch.circleL.angleAt(arch.circleR.center);   // direction toward pR
	double angApexL = arch.circleL.angleAt(arch.apex);
	buildTrefoilSide(arch.circleL, angFootL, angApexL, params.splitParameter,
	                 r_f, downDir, result.upperLeft, result.foilLeft,
	                 /*upperFromSplitToApex=*/true);

	// Right side (using circleR) : foot at angle to pL, apex above. splitParameter
	// is measured from the foot on both sides for symmetry.
	double angFootR = arch.circleR.angleAt(arch.circleL.center);   // direction toward pL
	double angApexR = arch.circleR.angleAt(arch.apex);
	buildTrefoilSide(arch.circleR, angFootR, angApexR, params.splitParameter,
	                 r_f, downDir, result.upperRight, result.foilRight,
	                 /*upperFromSplitToApex=*/false);

	return result;
}


//
// Fillets
//

namespace
{
	// Pick the intersection point closest to refCircle's BOUNDARY.
	Vector2d pickClosestToBoundary (const Circle::IntersectionResult &ir, const Circle &refCircle)
	{
		if (ir.count == 0)
			throw std::runtime_error("buildFillets: required circle intersection has no points");
		if (ir.count == 1)
			return ir.pts[0];
		double d0 = std::fabs(Vector2d::Distance(ir.pts[0], refCircle.center) - refCircle.radius);
		double d1 = std::fabs(Vector2d::Distance(ir.pts[1], refCircle.center) - refCircle.radius);
		return (d0 < d1) ? ir.pts[0] : ir.pts[1];
	}

	FilletGeom buildOneLateralFillet (const Circle &circOuter,
	                                   const Circle &circSub,
	                                   const Circle &circRosette)
	{
		FilletGeom result;
		result.cornerA = pickClosestToBoundary(circOuter.intersection(circSub),     circRosette);
		result.cornerB = pickClosestToBoundary(circSub.intersection(circRosette),   circOuter);
		result.cornerC = pickClosestToBoundary(circRosette.intersection(circOuter), circSub);

		bool ccwOuter   = pickShorterArcDirection(circOuter,   result.cornerC, result.cornerA);
		bool ccwSub     = pickShorterArcDirection(circSub,     result.cornerA, result.cornerB);
		bool ccwRosette = pickShorterArcDirection(circRosette, result.cornerB, result.cornerC);

		result.arcOuter   = Arc(circOuter,   result.cornerC, result.cornerA, ccwOuter);
		result.arcSub     = Arc(circSub,     result.cornerA, result.cornerB, ccwSub);
		result.arcRosette = Arc(circRosette, result.cornerB, result.cornerC, ccwRosette);
		return result;
	}
}

FilletsGeom buildFillets (const ArchOffsetGeom &mainOffset,
                          const SubwindowsGeom &subwindows,
                          const RosetteGeom    &rosette,
                          double                stoneBandWidth)
{
	if (!std::isfinite(stoneBandWidth))
		throw std::invalid_argument("buildFillets: stoneBandWidth must be finite");
	if (stoneBandWidth <= 0.0)
		throw std::invalid_argument("buildFillets: stoneBandWidth must be strictly positive");
	if (subwindows.lancets.empty())
		throw std::invalid_argument("buildFillets: subwindows must not be empty");

	FilletsGeom result;
	result.fillets.reserve(2);

	// LEFT lateral : main inner left side, leftmost lancet left side, rosette.
	{
		Circle circOuter = mainOffset.inner.circleR;                       // LEFT visible side of main inner
		Circle circSub   = subwindows.lancets.front().offset.outer.circleR; // LEFT visible side of leftmost lancet
		Circle circRos   = rosette.circle;
		result.fillets.push_back(buildOneLateralFillet(circOuter, circSub, circRos));
	}

	// RIGHT lateral : main inner right side, rightmost lancet right side, rosette.
	{
		Circle circOuter = mainOffset.inner.circleL;                       // RIGHT visible side of main inner
		Circle circSub   = subwindows.lancets.back().offset.outer.circleL;  // RIGHT visible side of rightmost lancet
		Circle circRos   = rosette.circle;
		result.fillets.push_back(buildOneLateralFillet(circOuter, circSub, circRos));
	}

	return result;
}


//
// Mouchettes
//

namespace
{
	MouchetteGeom buildVesicaMouchette (Vector2d center, double radius, double rotation)
	{
		MouchetteGeom result;
		result.center = center;
		result.radius = radius;

		double cosR = std::cos(rotation);
		double sinR = std::sin(rotation);
		// Lens long axis along `direction` ; foci along the perpendicular direction.
		double perpX = -sinR;
		double perpY =  cosR;

		Vector2d A(center.x - 0.5 * radius * perpX, center.y - 0.5 * radius * perpY);
		Vector2d B(center.x + 0.5 * radius * perpX, center.y + 0.5 * radius * perpY);

		Circle circA(A, radius);
		Circle circB(B, radius);

		Circle::IntersectionResult inter = circA.intersection(circB);
		if (inter.count < 2)
			throw std::runtime_error("buildVesicaMouchette: degenerate intersection");

		// Pick P1 = lens tip in `+direction`, P2 = lens tip in `-direction`.
		double d0 = (inter.pts[0].x - center.x) * cosR + (inter.pts[0].y - center.y) * sinR;
		Vector2d P1 = (d0 > 0.0) ? inter.pts[0] : inter.pts[1];
		Vector2d P2 = (d0 > 0.0) ? inter.pts[1] : inter.pts[0];

		// Each arc spans 2*pi/3 (< pi), so pickShorterArcDirection selects the lens-side arc.
		bool ccwA = pickShorterArcDirection(circA, P1, P2);
		bool ccwB = pickShorterArcDirection(circB, P2, P1);

		result.contour.push_back(Arc(circA, P1, P2, ccwA));
		result.contour.push_back(Arc(circB, P2, P1, ccwB));
		return result;
	}

	MouchetteGeom buildTeardropMouchette (Vector2d center, double radius, double rotation)
	{
		MouchetteGeom result;
		result.center = center;
		result.radius = radius;

		const double r = radius;
		const double cosR = std::cos(rotation);
		const double sinR = std::sin(rotation);

		Vector2d apex(center.x + 2.0 * r * cosR, center.y + 2.0 * r * sinR);

		// Tangent points at angles (rotation +/- pi/2) on the base circle.
		const double angT1 = rotation - 0.5 * M_PI;
		const double angT2 = rotation + 0.5 * M_PI;
		Vector2d T1(center.x + r * std::cos(angT1), center.y + r * std::sin(angT1));
		Vector2d T2(center.x + r * std::cos(angT2), center.y + r * std::sin(angT2));

		// Side circles : tangent to base at T_i, passing through apex.
		// With apex at distance 2r and tangent at angle pi/2 from apex direction :
		//   s = (r^2 - (2r)^2) / (2r) = -1.5 r        (along the radial line outward)
		//   side radius = |s - r| = 2.5 r
		const double s          = -1.5 * r;
		const double sideRadius =  2.5 * r;

		const double u1x = std::cos(angT1), u1y = std::sin(angT1);
		const double u2x = std::cos(angT2), u2y = std::sin(angT2);

		Vector2d sideCenter1(center.x + s * u1x, center.y + s * u1y);
		Vector2d sideCenter2(center.x + s * u2x, center.y + s * u2y);

		Circle baseCircle(center, r);
		Circle sideCircle1(sideCenter1, sideRadius);
		Circle sideCircle2(sideCenter2, sideRadius);

		// Base back arc : from T1 to T2 going AWAY from apex direction (through angle rotation+pi).
		// CW (ccw=false) gives this 180-degree arc since angT2 - angT1 = +pi.
		Arc baseArc(baseCircle, angT1, angT2, /*ccw=*/false);

		// Side arcs : each is the SHORTER arc on its side circle.
		bool ccwS1 = pickShorterArcDirection(sideCircle1, T1, apex);
		bool ccwS2 = pickShorterArcDirection(sideCircle2, apex, T2);
		Arc sideArc1(sideCircle1, T1, apex, ccwS1);
		Arc sideArc2(sideCircle2, apex, T2, ccwS2);

		result.contour.push_back(baseArc);
		result.contour.push_back(sideArc1);
		result.contour.push_back(sideArc2);
		return result;
	}
}

std::vector<MouchetteGeom> buildMouchettes (const SubwindowsGeom  &subwindows,
                                             const MouchetteParams &params)
{
	if (subwindows.lancets.empty())
		throw std::invalid_argument("buildMouchettes: subwindows must not be empty");
	if (!std::isfinite(params.radiusFactor))
		throw std::invalid_argument("buildMouchettes: radiusFactor must be finite");
	if (params.radiusFactor <= 0.0)
		throw std::invalid_argument("buildMouchettes: radiusFactor must be strictly positive");
	if (params.radiusFactor > 0.5)
		throw std::invalid_argument("buildMouchettes: radiusFactor must be <= 0.5");
	if (!std::isfinite(params.rotation))
		throw std::invalid_argument("buildMouchettes: rotation must be finite");

	std::vector<MouchetteGeom> result;
	const int n = (int) subwindows.lancets.size();
	if (n < 2)
		return result;     // no gaps : empty

	for (int i = 0; i < n - 1; ++i)
	{
		const LancetGeom &left  = subwindows.lancets[i];
		const LancetGeom &right = subwindows.lancets[i + 1];

		double centerX = (left.basis.pR.x + right.basis.pL.x) / 2.0;
		double maxApexY = std::max(left.arch.apex.y, right.arch.apex.y);
		double radius = params.radiusFactor * left.subWidth;
		double centerY = maxApexY + radius;
		Vector2d center(centerX, centerY);

		switch (params.type)
		{
			case MouchetteType::Vesica:
				result.push_back(buildVesicaMouchette(center, radius, params.rotation));
				break;
			case MouchetteType::Teardrop:
			case MouchetteType::Soufflet:    // alias for now
				result.push_back(buildTeardropMouchette(center, radius, params.rotation));
				break;
		}
	}
	return result;
}
