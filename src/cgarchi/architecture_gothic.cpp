#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/architecture_gothic.h>
#include <cgarchi/architecture_gothic_region.h>
#include "contour_validation.h"

#include "clipper2/clipper.h"   // 2D polygon boolean (vendored, extern/clipper2)

namespace cgarchi
{

namespace
{
	using Clipper2Lib::PathD;
	using Clipper2Lib::PathsD;
	using Clipper2Lib::PointD;
	using Clipper2Lib::FillRule;
	using Clipper2Lib::JoinType;
	using Clipper2Lib::EndType;

	// --- Clipper2 <-> Vector2d bridges -------------------------------------
	PathD toPathD (const std::vector<Vector2d> &c)
	{
		PathD p; p.reserve(c.size());
		for (const auto &v : c) p.emplace_back(v.x, v.y);
		return p;
	}
	std::vector<Vector2d> fromPathD (const PathD &p)
	{
		std::vector<Vector2d> c; c.reserve(p.size());
		for (const auto &pt : p) c.emplace_back(pt.x, pt.y);
		return c;
	}
	// Sample a circle (or arc-disk) as a CCW closed PathD.
	PathD circlePathD (Vector2d c, double r, double maxAngleRad)
	{
		const double TWO_PI = 2.0 * 3.14159265358979323846;
		int n = (int)std::ceil(TWO_PI / maxAngleRad);
		if (n < 12) n = 12;
		PathD p; p.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			double t = TWO_PI * i / n;
			p.emplace_back(c.x + r * std::cos(t), c.y + r * std::sin(t));
		}
		return p;
	}
	// Tessellate the visible silhouette of an offset arch into a CCW polyline.
	// arcLeft and arcRight are sub-arcs of the offset arch, parameterized so
	// that arcLeft.pointAt(0) is the right foot and arcLeft.pointAt(1) = apex
	// = arcRight.pointAt(0), and arcRight.pointAt(1) is the left foot.
	std::vector<Vector2d> archOutlineCcw (const ArchGeom &arch, double maxAngleRad)
	{
		auto leftPts  = arch.arcLeft.tessellateAdaptive(maxAngleRad);    // foot-right -> apex
		auto rightPts = arch.arcRight.tessellateAdaptive(maxAngleRad);   // apex -> foot-left

		std::vector<Vector2d> outline;
		outline.reserve(leftPts.size() + rightPts.size());
		for (const auto &p : leftPts) outline.push_back(p);
		// Skip the first point of rightPts (= apex, duplicates last of leftPts).
		for (size_t i = 1; i < rightPts.size(); ++i)
			outline.push_back(rightPts[i]);
		return outline;
	}

	// Cusped ("foiled") pointed-arch outline — the Havemann thesis approach to a
	// foiled lancet head (§5.4.1, Fig 5.27 "pointed trefoil arch") : the lancet's
	// OWN arch is refended into nFoils lobes separated by inward cusps, instead of
	// inscribing a separate foiled disc in the head. The outline touches the plain
	// arch at each foil peak and dips inward (toward the head centre) at each cusp ;
	// the two feet stay on the arch so the body still joins cleanly. So the foils
	// ARE the opening, always part of a lancet — never a floating disc. nFoils < 2
	// or a degenerate arch returns the plain outline.
	std::vector<Vector2d> cuspedArchOutline (const ArchGeom &arch, int nFoils, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		std::vector<Vector2d> base = archOutlineCcw(arch, maxAngleRad);
		const int m = (int)base.size();
		if (nFoils < 2 || m < 6) return base;

		// Head centre to pull the cusps toward (largest circle inside the arch).
		Vector2d Hc(arch.apex.x, 0.5*(arch.apex.y + 0.5*(base.front().y + base.back().y)));
		try { Hc = inscribedCircleOfPointedArch(arch).center; } catch (...) {}

		// Normalised arc-length parameter along the outline (foot -> apex -> foot).
		std::vector<double> s(m, 0.0);
		for (int i = 1; i < m; ++i)
		{
			double dx = base[i].x - base[i-1].x, dy = base[i].y - base[i-1].y;
			s[i] = s[i-1] + std::sqrt(dx*dx + dy*dy);
		}
		const double total = s[m-1];
		if (total < 1e-6) return base;
		for (int i = 0; i < m; ++i) s[i] /= total;

		const double d = 0.32 * (total / nFoils);   // cusp depth ~ a third of a foil chord
		std::vector<Vector2d> out(m);
		for (int i = 0; i < m; ++i)
		{
			double ss = s[i] * nFoils;
			int    fi = (int)std::floor(ss);
			if (fi < 0) fi = 0;
			if (fi >= nFoils) fi = nFoils - 1;
			double u  = ss - fi;                              // 0..1 within this foil
			double cL = (fi == 0)          ? 0.0 : d;         // foot (no cusp) vs interior cusp
			double cR = (fi == nFoils - 1) ? 0.0 : d;
			double off = (u <= 0.5 ? cL : cR) * (1.0 - std::sin(PI * u));
			double vx = Hc.x - base[i].x, vy = Hc.y - base[i].y;
			double L  = std::sqrt(vx*vx + vy*vy);
			if (L < 1e-9) { out[i] = base[i]; continue; }
			double pull = std::min(off, 0.8 * L);
			out[i] = Vector2d(base[i].x + pull * vx / L, base[i].y + pull * vy / L);
		}
		return out;
	}

	// Pack a contour (vector of Vector2d) into a flat float array (x0 y0 x1 y1 ...)
	// suitable for Polygon2::add_contour.
	std::vector<float> packContour (const std::vector<Vector2d> &pts)
	{
		std::vector<float> out;
		out.reserve(pts.size() * 2);
		for (const auto &p : pts)
		{
			out.push_back(static_cast<float>(p.x));
			out.push_back(static_cast<float>(p.y));
		}
		return out;
	}

	// Sample a circle into a CCW polyline. Step controls the angular resolution.
	std::vector<Vector2d> circleOutlineCcw (const Circle &c, double maxAngleRad)
	{
		const double TWO_PI = 2.0 * 3.14159265358979323846;
		int n = static_cast<int>(std::ceil(TWO_PI / maxAngleRad));
		if (n < 8) n = 8;
		std::vector<Vector2d> pts;
		pts.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			double theta = TWO_PI * i / n;
			pts.push_back(c.pointAt(theta));
		}
		return pts;
	}

	// Cusped multifoil oculus as a SINGLE CCW contour : n outward lobes meeting
	// at n inward cusps (classic gothic foiled circle : trefoil, quatrefoil,
	// sexfoil...). Unlike cutting each foil as an isolated circle-hole (which
	// leaves a big central stone boss and 6 "drilled holes"), this traces the
	// UNION boundary of the tangent foils : one connected flower with an open
	// centre, exactly the reference look.
	//
	// Lobes peak at radius R (the rim) ; cusps dip to radius rc. Each lobe is a
	// circular arc through cusp_i -> peak_i -> cusp_{i+1}, bulging outward. With
	// rc = (R - fr) * cos(pi/n) the lobe arc radius comes out to fr and its
	// centre to R-fr, i.e. it coincides with the Havemann foil circles.
	// (kept for reference : renders the rosette as a single connected cusped
	// flower. Superseded by the bar-tracery rosette below.)
	[[maybe_unused]] std::vector<Vector2d> cuspedOculusCcw (Vector2d C, double R, int n, double rc,
	                                       double phi0, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		if (n < 2) n = 2;
		if (rc < 0.0)        rc = 0.0;
		if (rc > 0.98 * R)   rc = 0.98 * R;
		const double alpha = 2.0 * PI / n;
		const double half  = alpha * 0.5;

		const double denom = (R - rc * std::cos(half));
		double dc  = (std::fabs(denom) < 1e-9) ? 0.0 : (R * R - rc * rc) / (2.0 * denom);
		double rho = R - dc;                    // lobe arc radius
		if (rho < 1e-6) return {};

		auto wrap = [&](double d){ while (d >  PI) d -= 2*PI; while (d < -PI) d += 2*PI; return d; };

		std::vector<Vector2d> pts;
		for (int i = 0; i < n; ++i)
		{
			const double m   = phi0 + i * alpha;          // lobe mid (aligned on a foil)
			const double aCl = m - half;                  // cusp before
			const double aCr = m + half;                  // cusp after
			const Vector2d lc(C.x + dc * std::cos(m), C.y + dc * std::sin(m));
			const Vector2d Pl(C.x + rc * std::cos(aCl), C.y + rc * std::sin(aCl));
			const Vector2d Pk(C.x + R  * std::cos(m),   C.y + R  * std::sin(m));
			const Vector2d Pr(C.x + rc * std::cos(aCr), C.y + rc * std::sin(aCr));

			auto ang = [&](const Vector2d &P){ return std::atan2(P.y - lc.y, P.x - lc.x); };
			const double a1 = ang(Pl), ak = ang(Pk), a2 = ang(Pr);
			const double akU = a1  + wrap(ak - a1);       // unwrap through the peak
			const double a2U = akU + wrap(a2 - ak);

			int per = std::max(4, (int)std::ceil(std::fabs(a2U - a1) / maxAngleRad));
			for (int k = 0; k < per; ++k)                 // [a1, a2U) : cusp shared with next lobe
			{
				const double a = a1 + (a2U - a1) * (double)k / per;
				pts.push_back(Vector2d(lc.x + rho * std::cos(a), lc.y + rho * std::sin(a)));
			}
		}
		return pts;
	}

	// SEPARATE teardrop petals (the reference "daisy") : n distinct void lobes,
	// each bulging to the rim (radius R) and narrowing to a point near a central
	// hub (radius rh). Returned as n CCW contours -> cut each as its own void, so
	// the stone left between them (thin cusps) and the central hub read as real
	// tracery : "a circular arrangement of holes with stone between", not one
	// merged flower. Same lobe geometry as cuspedOculusCcw (rc = foil tangency).
	std::vector<std::vector<Vector2d>> cuspedPetals (Vector2d C, double R, int n, double rc,
	                                                 double rh, double phi0, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		if (n < 2) n = 2;
		if (rc < 0.0)      rc = 0.0;
		if (rc > 0.98 * R) rc = 0.98 * R;
		if (rh < 0.0)      rh = 0.0;
		if (rh > rc)       rh = rc * 0.9;
		const double alpha = 2.0 * PI / n;
		const double half  = alpha * 0.5;
		const double denom = (R - rc * std::cos(half));
		double dc  = (std::fabs(denom) < 1e-9) ? 0.0 : (R * R - rc * rc) / (2.0 * denom);
		double rho = R - dc;
		std::vector<std::vector<Vector2d>> petals;
		if (rho < 1e-6) return petals;
		auto wrap = [&](double d){ while (d >  PI) d -= 2*PI; while (d < -PI) d += 2*PI; return d; };

		for (int i = 0; i < n; ++i)
		{
			const double m   = phi0 + i * alpha;
			const Vector2d lc(C.x + dc * std::cos(m), C.y + dc * std::sin(m));
			const Vector2d Pl(C.x + rc * std::cos(m - half), C.y + rc * std::sin(m - half));
			const Vector2d Pk(C.x + R  * std::cos(m),        C.y + R  * std::sin(m));
			const Vector2d Pr(C.x + rc * std::cos(m + half), C.y + rc * std::sin(m + half));
			auto ang = [&](const Vector2d &P){ return std::atan2(P.y - lc.y, P.x - lc.x); };
			const double a1 = ang(Pl), ak = ang(Pk), a2 = ang(Pr);
			const double akU = a1  + wrap(ak - a1);
			const double a2U = akU + wrap(a2 - ak);

			std::vector<Vector2d> petal;
			int per = std::max(4, (int)std::ceil(std::fabs(a2U - a1) / maxAngleRad));
			for (int k = 0; k <= per; ++k)                // full outer lobe arc, cusp_left -> cusp_right
			{
				const double a = a1 + (a2U - a1) * (double)k / per;
				petal.push_back(Vector2d(lc.x + rho * std::cos(a), lc.y + rho * std::sin(a)));
			}
			// close toward a hub point near the centre -> teardrop tip
			petal.push_back(Vector2d(C.x + rh * std::cos(m), C.y + rh * std::sin(m)));
			petals.push_back(std::move(petal));
		}
		return petals;
	}

	// Full rosette STONE tracery inside the oculus (radius R), via Clipper2 :
	//   - a distinct outer stone RING [Ri, Ro], held to the surrounding head
	//     stone by n slender connector SPOKES across a void groove [Ro, R] ;
	//   - the tracery web inside the ring = disk(Ri) minus the teardrop petals
	//     (leaves the thin cusps between petals + the central hub as stone).
	// Returned as Clipper2 paths (outers CCW / holes CW). The caller cuts the
	// oculus as a void and adds these back as stone islands -> a real foiled
	// oculus : ring + petals + cusps + hub, matching the reference. Booleans
	// (Union/Difference of overlapping primitives) are exactly what Clipper2 is
	// for and what plain GLU winding could not express.
	[[maybe_unused]] PathsD buildRosetteStone (Vector2d C, double R, int n, double phi0, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		if (n < 2) n = 2;
		const double gw = R * 0.06;            // void groove width (ring <-> head)
		const double Ro = R - gw;              // ring outer edge
		const double Ri = R * 0.72;            // ring inner edge = petal tips
		const double rh = R * 0.16;            // central hub radius
		const double fr = std::sin(PI / n) / (1.0 + std::sin(PI / n)) * Ri;
		const double rc = (Ri - fr) * std::cos(PI / n);

		// Teardrop petals inscribed in [rh, Ri].
		PathsD petals;
		for (auto &pv : cuspedPetals(C, Ri, n, rc, rh, phi0, maxAngleRad))
			petals.push_back(toPathD(pv));
		PathsD petalsU = Union(petals, FillRule::NonZero);

		// Inner web = disk(Ri) - petals  (cusps + hub + spandrels).
		PathsD innerWeb = Difference(PathsD{ circlePathD(C, Ri, maxAngleRad) }, petalsU,
		                             FillRule::NonZero);
		// Ring band = disk(Ro) - disk(Ri).
		PathsD ringBand = Difference(PathsD{ circlePathD(C, Ro, maxAngleRad) },
		                             PathsD{ circlePathD(C, Ri, maxAngleRad) }, FillRule::NonZero);
		// n connector spokes bridging the groove [Ri, R], between petals.
		PathsD spokes;
		const double alpha = 2.0 * PI / n, hw = alpha * 0.10;
		for (int i = 0; i < n; ++i)
		{
			const double a = phi0 + (i + 0.5) * alpha;
			const double r0 = Ri - 1.0, r1 = R + 2.0;
			PathD sp;
			sp.emplace_back(C.x + r0 * std::cos(a - hw), C.y + r0 * std::sin(a - hw));
			sp.emplace_back(C.x + r1 * std::cos(a - hw), C.y + r1 * std::sin(a - hw));
			sp.emplace_back(C.x + r1 * std::cos(a + hw), C.y + r1 * std::sin(a + hw));
			sp.emplace_back(C.x + r0 * std::cos(a + hw), C.y + r0 * std::sin(a + hw));
			spokes.push_back(sp);
		}

		PathsD stone = Union(innerWeb, ringBand, FillRule::NonZero);
		stone       = Union(stone, spokes, FillRule::NonZero);
		return stone;
	}

	double signedAreaShoelace (const std::vector<Vector2d> &polygon)
	{
		double sum = 0.0;
		size_t n = polygon.size();
		for (size_t i = 0; i < n; ++i)
		{
			size_t j = (i + 1) % n;
			sum += polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
		}
		return 0.5 * sum;
	}

	// Append a contour as a CW hole. If the input is CCW, reverse it first.
	// If already CW, push as is.
	void pushAsHoleAnyOrientation (std::vector<std::vector<Vector2d>> &holes,
	                                std::vector<Vector2d> contour)
	{
		if (signedAreaShoelace(contour) > 0.0)
			std::reverse(contour.begin(), contour.end());   // CCW -> CW
		holes.push_back(std::move(contour));
	}

	// Append a contour as a CW hole (input is CCW, will be reversed).
	void pushAsHole (std::vector<std::vector<Vector2d>> &holes,
	                  std::vector<Vector2d> ccwOutline)
	{
		std::reverse(ccwOutline.begin(), ccwOutline.end());
		holes.push_back(std::move(ccwOutline));
	}

	// Append a contour as CCW (a STONE island : positive winding). Used to place
	// solid tracery back INSIDE a void (e.g. the foil rings inside the oculus).
	// Stone-in-void is representable under GLU NONZERO (unlike void-in-void).
	[[maybe_unused]] void pushAsStone (std::vector<std::vector<Vector2d>> &out,
	                   std::vector<Vector2d> contour)
	{
		if (signedAreaShoelace(contour) < 0.0)
			std::reverse(contour.begin(), contour.end());   // CW -> CCW
		out.push_back(std::move(contour));
	}

	// Build the closed boundary of a pointed foil. The boundary is :
	//   arcLeft  : baseLeft -> apex
	//   arcRight : apex -> baseRight  (skip first to avoid duplicating apex)
	//   (implicit closure from baseRight back to baseLeft : chord)
	//
	// We deliberately use a CHORD (straight line) rather than the rim arc on
	// the foil ring's outer circle. Reasons :
	//   1. The rim arc would coincide with edges of the rosette / lancet head
	//      circle if both are added as holes, which causes GLU's tessellator
	//      to assert (coincident edges). Chord closure avoids this.
	//   2. The chord-closed shape is the geometrically meaningful "foil leaf",
	//      excluding the small sliver between the foil and the rosette rim.
	std::vector<Vector2d> pointedFoilContour (const PointedFoil &pf,
	                                            double maxAngleRad)
	{
		std::vector<Vector2d> contour;
		auto leftPts  = pf.arcLeft.tessellateAdaptive(maxAngleRad);
		auto rightPts = pf.arcRight.tessellateAdaptive(maxAngleRad);

		for (const auto &p : leftPts) contour.push_back(p);
		for (size_t i = 1; i < rightPts.size(); ++i) contour.push_back(rightPts[i]);

		return contour;
	}

	// Extend a CCW pointed-arch outline (foot-right ... apex ... foot-left) into
	// a tall window silhouette by dropping both feet straight down to y = sillY
	// and closing along the sill. Preserves CCW orientation. No-op (returns the
	// arch as-is) when sillY is not below the feet.
	std::vector<Vector2d> archWithBody (std::vector<Vector2d> outline, double sillY)
	{
		if (outline.size() < 2) return outline;
		const Vector2d footRight = outline.front();
		const Vector2d footLeft  = outline.back();
		const double   footY     = std::min(footRight.y, footLeft.y);
		if (sillY >= footY - 1e-6) return outline;   // body would be zero/negative
		outline.push_back(Vector2d(footLeft.x,  sillY));   // left jamb, down
		outline.push_back(Vector2d(footRight.x, sillY));   // sill, right-ward
		return outline;                                     // closes up the right jamb
	}

	// --- Shared tracery-field helpers (used by both the main window and the
	//     recursive sub-windows, so every level looks the same) ---------------

	// n ROUND foils (Havemann Fig 5.27 1a) inscribed in radius R : circles of
	// radius fr = sin(a/2)/(1+sin(a/2))*R, tangent to the rim and to each other,
	// one centred at angle phi0 (pi/2 = top). The classic round sexfoil.
	[[maybe_unused]] std::vector<Circle> roundFoilCircles (Vector2d C, double R, int n, double phi0)
	{
		const double PI = 3.14159265358979323846;
		if (n < 3) n = 3;
		const double a  = 2.0 * PI / n;
		const double fr = std::sin(a/2.0) / (1.0 + std::sin(a/2.0)) * R;
		const double Rm = R - fr;
		std::vector<Circle> out;
		for (int i = 0; i < n; ++i)
		{
			const double t = phi0 + i * a;
			out.emplace_back(Vector2d(C.x + Rm*std::cos(t), C.y + Rm*std::sin(t)), fr);
		}
		return out;
	}

	// Points along the circular arc through P0 -> Pm -> P1 (circumcircle of the
	// three). Falls back to a straight segment if they are collinear.
	std::vector<Vector2d> arcThrough3 (Vector2d P0, Vector2d Pm, Vector2d P1, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		const double ax=P0.x, ay=P0.y, bx=Pm.x, by=Pm.y, cx=P1.x, cy=P1.y;
		const double d = 2.0*(ax*(by-cy) + bx*(cy-ay) + cx*(ay-by));
		std::vector<Vector2d> out;
		if (std::fabs(d) < 1e-9)
		{
			const int k=8;
			for (int i=0;i<=k;++i){ double t=(double)i/k; out.emplace_back(P0.x+(P1.x-P0.x)*t, P0.y+(P1.y-P0.y)*t); }
			return out;
		}
		const double a2=ax*ax+ay*ay, b2=bx*bx+by*by, c2=cx*cx+cy*cy;
		const double ux=(a2*(by-cy)+b2*(cy-ay)+c2*(ay-by))/d;
		const double uy=(a2*(cx-bx)+b2*(ax-cx)+c2*(bx-ax))/d;
		const double r = std::sqrt((ax-ux)*(ax-ux)+(ay-uy)*(ay-uy));
		auto wrap=[&](double x){ while(x>PI)x-=2*PI; while(x<-PI)x+=2*PI; return x; };
		const double a0=std::atan2(ay-uy,ax-ux), am=std::atan2(by-uy,bx-ux), a1=std::atan2(cy-uy,cx-ux);
		const double amU=a0+wrap(am-a0), a1U=amU+wrap(a1-am);
		const int seg=std::max(3,(int)std::ceil(std::fabs(a1U-a0)/maxAngleRad));
		for (int i=0;i<=seg;++i){ double t=(double)i/seg, a=a0+(a1U-a0)*t; out.emplace_back(ux+r*std::cos(a), uy+r*std::sin(a)); }
		return out;
	}

	// n POINTED foils (Havemann Fig 5.27 1b) as separate almond/vesica petals :
	// each pointed at the centre (inner tip) AND at the rim (outer tip), bounded
	// by two circular arcs bulging sideways ; one petal centred at phi0. The
	// classic pointed sexfoil. Adjacent petals leave thin stone cusps between them.
	std::vector<std::vector<Vector2d>> pointedFoilPetals (Vector2d C, double R, int n,
	                                                      double phi0, double maxAngleRad,
	                                                      double pointedness)
	{
		const double PI = 3.14159265358979323846;
		if (n < 3) n = 3;
		const double alpha = 2.0*PI/n;
		// pointedness in [0,2] : 0 = fat / almost round petals, 2 = slender sharp
		// petals. Controls the sideways bulge, the bulge radius and the inner tip.
		const double t     = std::min(std::max(pointedness, 0.0), 2.0) / 2.0;   // 0..1
		const double r_in  = R * (0.18 - 0.13*t);   // sharper centre point at high t
		const double r_mid = R * (0.64 - 0.12*t);   // bulge nearer the rim at low t
		const double beta  = alpha * (0.48 - 0.16*t);  // narrower petals at high t
		std::vector<std::vector<Vector2d>> out;
		for (int i = 0; i < n; ++i)
		{
			const double m = phi0 + i*alpha;
			const Vector2d Pi(C.x + r_in*std::cos(m),        C.y + r_in*std::sin(m));         // inner tip
			const Vector2d Po(C.x + R   *std::cos(m),        C.y + R   *std::sin(m));         // outer tip
			const Vector2d Ml(C.x + r_mid*std::cos(m-beta),  C.y + r_mid*std::sin(m-beta));   // left bulge
			const Vector2d Mr(C.x + r_mid*std::cos(m+beta),  C.y + r_mid*std::sin(m+beta));   // right bulge
			std::vector<Vector2d> petal = arcThrough3(Pi, Ml, Po, maxAngleRad);               // inner->left->outer
			std::vector<Vector2d> rgt   = arcThrough3(Po, Mr, Pi, maxAngleRad);               // outer->right->inner
			for (size_t k = 1; k < rgt.size(); ++k) petal.push_back(rgt[k]);
			out.push_back(std::move(petal));
		}
		return out;
	}

	// Single CONNECTED pointed multifoil as ONE closed contour : n outward pointed
	// lobe tips at radius R (touching the ring) and n inward cusps at radius rc,
	// all lobes sharing one open centre (a void of radius ~rc). pointedness in
	// [0,2] : fat convex lobes (0) .. slender spikes (2), pointed tip & cusp both.
	//
	// Built as a POLAR profile r(theta) with theta sweeping monotonically around
	// the full circle and r > 0 everywhere : such a curve is provably SIMPLE (no
	// self-intersection) for ANY n. The previous arc-through-3-points construction
	// produced a small loop inside every lobe (total turning grew as ~n*2pi instead
	// of 2pi), which the flat GLU cap hid but the 3D chamfer turned into "drops" —
	// visible as soon as the foil count climbed (n > 11).
	std::vector<Vector2d> cuspedFlowerPointed (Vector2d C, double R, int n, double phi0,
	                                           double pointedness, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		if (n < 3) n = 3;
		const double alpha = 2.0*PI/n;
		const double t  = std::min(std::max(pointedness, 0.0), 2.0) / 2.0;   // 0..1
		const double rc = R * (0.50 - 0.18*t);        // valley radius = open central void
		// Lobe profile exponent : g<1 -> fat convex lobes, g>1 -> slender ; the tip
		// (u=1) and cusp (u=0) stay pointed corners for any g>0.
		const double g  = 0.45 + 0.9*t;
		const int per = std::max(8, (int)std::ceil(alpha / std::max(maxAngleRad, 1e-3)));
		std::vector<Vector2d> out;
		out.reserve((size_t)n * per);
		for (int i = 0; i < n; ++i)
		{
			const double m = phi0 + (i + 1.0)*alpha;   // this lobe's tip angle
			for (int j = 0; j < per; ++j)              // theta over [m-alpha/2, m+alpha/2)
			{
				const double th = m - 0.5*alpha + alpha * ((double)j / per);
				const double u  = 1.0 - std::fabs(th - m) / (0.5*alpha);   // 0 valley .. 1 tip
				const double r  = rc + (R - rc) * std::pow(std::max(0.0, u), g);
				out.emplace_back(C.x + r*std::cos(th), C.y + r*std::sin(th));
			}
		}
		return out;
	}

	// Rosette round foils as VOID paths, inscribed at 0.86*R so a stone ring
	// remains around them. (Superseded by rosetteVoidContours ; kept for reference.)
	[[maybe_unused]] PathsD daisyPetalPaths (Vector2d C, double R, int n, double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		if (n < 3) n = 3;
		PathsD out;
		for (auto &c : roundFoilCircles(C, R * 0.86, n, PI/2.0))
			out.push_back(circlePathD(c.center, c.radius, maxAngleRad));
		return out;
	}

	// (A foiled lancet head is now the lancet's own arch refended into a cusped /
	// trefoil arch via cuspedArchOutline — the thesis approach — so the old
	// "inscribe a separate foiled disc in the head" construction was removed.)

	void record (GothicBuildReport *report, GothicBuildWarning kind)
	{
		if (report) report->Record(kind);
	}

	void record (GothicBuildReport *report, GothicBuildWarning kind, const std::string &message)
	{
		if (report) report->Record(kind, std::string(), message);
	}

	// Text of the exception being handled, prefixed by `where`. Call from a
	// catch block only.
	std::string currentExceptionText (const char *where)
	{
		try { throw; }
		catch (const std::exception &e) { return std::string(where) + ": " + e.what(); }
		catch (...) { return std::string(where) + ": unknown exception"; }
	}

	// Why a crowning rosette (main or sub) that rests on two lancets is not built.
	std::string rosetteDropReason (const Circle &rc, double minRadius, const Vector2d &apex)
	{
		std::ostringstream os;
		if (rc.radius <= 0.0)
			os << "no circle tangent to the arch through the lancet apexes";
		else if (rc.radius <= minRadius)
			os << "radius " << rc.radius << " <= threshold " << minRadius;
		else
			os << "centre " << rc.center.y << " not above the lancet apexes " << apex.y;
		return os.str();
	}

	// Offsets of a sub-lancet one level down : the unit's own offsets, capped at
	// 0.30 x the sub-arch radius so that the mullions thin out with depth.
	ArchOffsetParams childOffset (const ArchOffsetParams &off, const ArchGeom &childArch)
	{
		ArchOffsetParams lo = off;
		const double r = childArch.circleL.radius;
		lo.inner = std::min(off.inner, 0.30 * r);
		lo.outer = std::min(off.outer, 0.30 * r);
		return lo;
	}

	// Inner apex of a sub-lancet opening, computed with the offsets the sub-lancet
	// is actually cut with (childOffset), so that the crowning sub-rosette rests on
	// the drawn apexes. Falls back to the apex of `lancet.offset` (unit offsets)
	// when the child offset cannot be built, recorded as
	// GothicBuildWarning::DepthFallback.
	Vector2d childInnerApex (const LancetGeom &lancet, const ArchOffsetParams &off,
	                         GothicBuildReport *report)
	{
		try { return buildArchOffset(lancet.arch, childOffset(off, lancet.arch)).inner.apex; }
		catch (...)
		{
			record(report, GothicBuildWarning::DepthFallback, currentExceptionText("sub-rosette apex"));
			return lancet.offset.inner.apex;
		}
	}

	// Fillet openings = region − solids, inset by `inset` (bdInner).
	PathsD filletPaths (const PathD &region, const PathsD &solids, double inset)
	{
		PathsD f = Difference(PathsD{ region }, Union(solids, FillRule::NonZero), FillRule::NonZero);
		if (inset > 0.0) f = InflatePaths(f, -inset, JoinType::Round, EndType::Polygon);
		return f;
	}

	// Width of the stone ring between a rosette rim (radius Rr) and its voids,
	// shared by the lobed motif and the plain oculus.
	double rosetteRingWidth (double Rr, const GothicFeatureThresholds &thr)
	{
		return std::max(thr.minRingWidth, 0.10 * Rr);
	}

	// Void of a plain (unlobed) oculus : the rim shrunk by the ring width of the
	// lobed motif, so that the void keeps off the lancet apexes the rim passes
	// through (Havemann's contiguous border plane). Radius -1 when the ring leaves
	// no void, by the same rule as the lobed motif.
	Circle plainOculusVoid (const Circle &rim, const GothicFeatureThresholds &thr)
	{
		const double Rin  = rim.radius - rosetteRingWidth(rim.radius, thr);
		const bool   kept = rim.radius > thr.minRosetteRadius && Rin > thr.minRosetteRadius;
		return Circle(rim.center, kept ? Rin : -1.0);
	}

	// The ONE canonical rosette motif, shared by the main rosette and the recursion
	// sub-rosettes : a single CONNECTED multifoil flower (pointed or round) with an
	// open centre, plus one eyelet fillet per foil between the foils and the ring.
	// Returns the void contours (CCW). Cr = centre, Rr = outer/ring radius, n = foil
	// count, foilType (1 = pointed, 0 = round), pointedness for the pointed variant.
	// A rosette whose rim or inner radius is within thr.minRosetteRadius has no
	// void, recorded as GothicBuildWarning::FeatureDropped.
	std::vector<std::vector<Vector2d>> rosetteVoidContours (Vector2d Cr, double Rr, int n,
	                                                        int foilType, double pointedness,
	                                                        double maxAngleRad,
	                                                        const GothicFeatureThresholds &thr,
	                                                        GothicBuildReport *report)
	{
		const double PI = 3.14159265358979323846;
		std::vector<std::vector<Vector2d>> out;
		if (n < 2) return out;
		const double ringW = rosetteRingWidth(Rr, thr);
		const double Rin   = Rr - ringW;
		if (Rr <= thr.minRosetteRadius || Rin <= thr.minRosetteRadius)
		{
			record(report, GothicBuildWarning::FeatureDropped, "rosette motif: ring leaves no void");
			return out;
		}
		const double Rp    = Rin;                       // foil tips reach the ring

		std::vector<Vector2d> flower;
		if (foilType == 1)                              // pointed connected multifoil
			flower = cuspedFlowerPointed(Cr, Rp, n, PI/2.0, pointedness, maxAngleRad);
		else                                           // round connected multifoil
		{
			const double fr = std::sin(PI/n)/(1.0+std::sin(PI/n))*Rp;
			const double rc = (Rp - fr)*std::cos(PI/n);
			flower = cuspedOculusCcw(Cr, Rp, n, rc, PI/2.0, maxAngleRad);
		}
		if (flower.size() < 3) return out;

		PathD flowerPath = toPathD(flower);
		out.push_back(std::move(flower));               // the multifoil void
		const double rinset = std::max(thr.minEyeletInset, 0.28 * ringW);
		for (auto &f : filletPaths(circlePathD(Cr, Rin, maxAngleRad), PathsD{ flowerPath }, rinset))
			if (f.size() >= 3) out.push_back(fromPathD(f));   // one eyelet fillet per foil
		return out;
	}

	// Forward decl (defined below, near buildBayStonePolygon) : shape a spandrel field
	// as an inscribed mouchette clipped to it. Same anonymous namespace ⇒ same entity.
	PathsD mouchetteInField (const PathD &field, int mType, double sizeFactor,
	                         Vector2d focus, double maxAngleRad, double minRadius,
	                         GothicBuildReport *report);

	// Recursively collect every GLASS void contour of a window unit (base arch
	// `arch`). A unit is built like the whole window : sub-lancets (leaves get a
	// foiled head, or recurse), a crowning daisy rosette, and corner fillets.
	// The caller Differences these from the unit opening to get the stone.
	// A unit whose offset cannot be built (the buildXxx may throw on tiny arches)
	// has no void, recorded as GothicBuildWarning::FeatureDropped, like a
	// sub-rosette that is not built. When `mouchettes`, the unit's corner
	// fillets are shaped as inscribed mouchettes/soufflets (flamboyant), like
	// the top-level ones.
	// A unit that cannot subdivide falls back to its whole opening as one void,
	// recorded as GothicBuildWarning::DepthFallback.
	void collectUnitVoids (const ArchGeom &arch, const ArchOffsetParams &off,
	                       const SubwindowParams &sub, double sillY,
	                       int remaining, int foilN, int foilType, double pointedness,
	                       bool headFoiled, int headFoils, double filletInset,
	                       bool mouchettes, int mouchetteType, double mouchetteSize,
	                       double maxAngleRad, const GothicFeatureThresholds &thr,
	                       GothicBuildReport *report, std::vector<PathD> &out)
	{
		ArchOffsetGeom o;
		try { o = buildArchOffset(arch, off); }
		catch (...)
		{
			record(report, GothicBuildWarning::FeatureDropped, currentExceptionText("recursive unit offset"));
			return;
		}

		std::vector<Vector2d> openingV = archWithBody(archOutlineCcw(o.inner, maxAngleRad), sillY);
		Vector2d filletFocus = o.inner.apex;   // mouchette apex points here (updated to the sub-oculus below)

		if (remaining <= 0)
		{
			// Leaf lancet : plain opening, or the lancet's arch refended into a
			// cusped (trefoil) arch — the foils ARE part of the opening.
			if (headFoiled)
			{
				std::vector<Vector2d> fov =
					archWithBody(cuspedArchOutline(o.inner, headFoils, maxAngleRad), sillY);
				out.push_back(toPathD(fov));
			}
			else
				out.push_back(toPathD(openingV));
			return;
		}

		SubwindowsGeom subs;
		try { subs = buildSubwindows(arch, off, sub); }
		catch (...)
		{
			record(report, GothicBuildWarning::DepthFallback, currentExceptionText("recursive unit subdivision"));
			out.push_back(toPathD(openingV));
			return;
		}

		PathsD solids;   // rosette disk + sub-arch base arches, for this unit's fillets
		for (const auto &l : subs.lancets)
		{
			solids.push_back(toPathD(archWithBody(archOutlineCcw(l.arch, maxAngleRad), sillY)));
			collectUnitVoids(l.arch, childOffset(off, l.arch), sub, sillY, remaining - 1,
			                 foilN, foilType, pointedness, headFoiled, headFoils, filletInset * 0.7,
			                 mouchettes, mouchetteType, mouchetteSize, maxAngleRad, thr, report, out);
		}

		// Crowning sub-rosette : passes through the two sub-lancet apexes and is
		// tangent to this unit's arch. Uses the SAME motif as the main rosette
		// (connected multifoil flower + eyelet fillets). RULES : (a) the unit must
		// subdivide into exactly two sub-lancets ; (b) the rosette must REST ON them
		// — its centre ABOVE their apexes (else it would hang with no support below).
		if (subs.lancets.size() == 2)
		{
			const double axisX = 0.5*(o.inner.circleL.center.x + o.inner.circleR.center.x);
			const Vector2d subApex = childInnerApex(subs.lancets.back(), off, report);
			Circle rc = rosetteTangentToLancets(o.inner, axisX, subApex);
			if (!(rc.radius > thr.minSubRosetteRadius && rc.center.y > subApex.y))
				record(report, GothicBuildWarning::FeatureDropped,
				       "sub-rosette: " + rosetteDropReason(rc, thr.minSubRosetteRadius, subApex));
			else
			{
				filletFocus = rc.center;                                            // mouchettes point at the sub-oculus
				solids.push_back(circlePathD(rc.center, rc.radius, maxAngleRad));   // for corner fillets
				for (auto &v : rosetteVoidContours(rc.center, rc.radius, foilN, foilType, pointedness,
				                                   maxAngleRad, thr, report))
					out.push_back(toPathD(v));
			}
		}

		// Corner fillets for this unit (shaped as mouchettes when enabled).
		if (filletInset > 0.0 && !solids.empty())
		{
			for (auto &f : filletPaths(toPathD(openingV), solids, filletInset))
			{
				if (f.size() < 3) continue;
				if (mouchettes)
				{
					for (auto &s : mouchetteInField(f, mouchetteType, mouchetteSize, filletFocus, maxAngleRad,
					                                thr.minMouchetteRadius, report))
						if (s.size() >= 3) out.push_back(std::move(s));
				}
				else
					out.push_back(std::move(f));
			}
		}
	}
}

Circle rosetteTangentToLancets (const ArchGeom &mainInner, double axisX,
                                const Vector2d &lancetApex)
{
	const Vector2d mCR = mainInner.circleR.center;     // main arch construction circle
	const double   mR  = mainInner.circleR.radius;
	const double   ax  = lancetApex.x - axisX;         // apex offset from the axis
	const double   ay  = lancetApex.y;
	// r(cy) : radius so the circle stays internally tangent to the main arch.
	auto rOf = [&](double cy){
		return mR - std::sqrt((mCR.x-axisX)*(mCR.x-axisX) + (cy-mCR.y)*(cy-mCR.y));
	};
	// g(cy) : 0 when the circle also passes through the apex. Monotone in cy over
	// [apexY, mainApexY] (g<0 low, g>0 high) -> bisection.
	auto g = [&](double cy){ double r = rOf(cy); return ax*ax + (cy-ay)*(cy-ay) - r*r; };
	double lo = ay, hi = mainInner.apex.y;
	if (!(hi > lo + 1e-6)) return Circle(Vector2d(axisX, ay), -1.0);
	double glo = g(lo), ghi = g(hi);
	if (glo * ghi > 0.0) return Circle(Vector2d(axisX, ay), -1.0);   // no bracketed root
	for (int it = 0; it < 60; ++it)
	{
		double mid = 0.5*(lo+hi), gm = g(mid);
		if (glo * gm <= 0.0) { hi = mid; ghi = gm; }
		else                 { lo = mid; glo = gm; }
	}
	double cy = 0.5*(lo+hi), r = rOf(cy);
	if (r <= 1.0) return Circle(Vector2d(axisX, ay), -1.0);
	return Circle(Vector2d(axisX, cy), r);
}

namespace
{
	// Closed outline through the arcs of a mouchette, joined end to end. The arcs
	// of a cgmath mouchette need not follow one another : the teardrop is base
	// T1 -> T2, side T1 -> apex, side apex -> T2. Starting from the first arc, the
	// next one is the unused arc with an endpoint nearest to the current end,
	// reversed when that endpoint is its end ; its first point (the shared
	// endpoint) is dropped.
	std::vector<Vector2d> chainArcs (const std::vector<Arc> &arcs, double maxAngleRad)
	{
		std::vector<std::vector<Vector2d>> pieces;
		pieces.reserve(arcs.size());
		for (const Arc &a : arcs)
		{
			std::vector<Vector2d> pts = a.tessellateAdaptive(maxAngleRad);
			if (!pts.empty()) pieces.push_back(std::move(pts));
		}
		if (pieces.empty()) return {};

		auto dist2 = [](const Vector2d &p, const Vector2d &q) {
			return (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
		};
		std::vector<Vector2d> contour = pieces[0];
		std::vector<bool> used(pieces.size(), false);
		used[0] = true;
		for (size_t step = 1; step < pieces.size(); ++step)
		{
			size_t best = 0;
			bool   reversed = false;
			double bestD = std::numeric_limits<double>::infinity();
			for (size_t k = 0; k < pieces.size(); ++k)
			{
				if (used[k]) continue;
				const double dStart = dist2(contour.back(), pieces[k].front());
				const double dEnd   = dist2(contour.back(), pieces[k].back());
				if (dStart < bestD) { bestD = dStart; best = k; reversed = false; }
				if (dEnd   < bestD) { bestD = dEnd;   best = k; reversed = true; }
			}
			used[best] = true;
			std::vector<Vector2d> &next = pieces[best];
			if (reversed) std::reverse(next.begin(), next.end());
			contour.insert(contour.end(), next.begin() + 1, next.end());
		}
		return contour;
	}

	// Inscribe a flamboyant mouchette into a spandrel FIELD polygon, clipped to the
	// field so the resulting void never breaks the surrounding stone frame. Returns
	// the plain field when it cannot carry a shape (inscribed radius below
	// minRadius, mouchette that throws, degenerate or clipped away), recorded as
	// GothicBuildWarning::FeatureDropped. `focus` is the point the teardrop apex
	// points at (the oculus centre).
	PathsD mouchetteInField (const PathD &field, int mType, double sizeFactor,
	                         Vector2d focus, double maxAngleRad, double minRadius,
	                         GothicBuildReport *report)
	{
		std::vector<Vector2d> poly = fromPathD(field);
		const int n = static_cast<int>(poly.size());
		if (n < 3) return PathsD{ field };
		const double area = std::fabs(signedAreaShoelace(poly));
		const double PI = 3.14159265358979323846;
		const double charR = std::sqrt(area / PI);
		const double rm = sizeFactor * 0.9 * charR;
		if (rm < minRadius)                                   // field too small to shape
		{
			record(report, GothicBuildWarning::FeatureDropped, "mouchette: field too small");
			return PathsD{ field };
		}

		Vector2d C(0.0, 0.0);
		for (const auto &p : poly) { C.x += p.x; C.y += p.y; }
		C.x /= n; C.y /= n;

		const double rot = std::atan2(focus.y - C.y, focus.x - C.x);   // apex toward the oculus
		MouchetteType mt = (mType == 0) ? MouchetteType::Vesica
		                 : (mType == 2) ? MouchetteType::Soufflet
		                                : MouchetteType::Teardrop;
		MouchetteGeom mg;
		try { mg = buildMouchette(mt, C, rm, rot); }
		catch (...)
		{
			record(report, GothicBuildWarning::FeatureDropped, currentExceptionText("mouchette"));
			return PathsD{ field };
		}

		std::vector<Vector2d> contour = chainArcs(mg.contour, maxAngleRad);
		if (contour.size() < 3)
		{
			record(report, GothicBuildWarning::FeatureDropped, "mouchette: outline has fewer than 3 points");
			return PathsD{ field };
		}

		PathsD clipped = Intersect(PathsD{ toPathD(contour) }, PathsD{ field }, FillRule::NonZero);
		if (clipped.empty())
		{
			record(report, GothicBuildWarning::FeatureDropped, "mouchette: empty once clipped to its field");
			return PathsD{ field };
		}
		return clipped;
	}
}

GothicFeatureThresholds GothicFeatureThresholds::Scaled (double k) const
{
	GothicFeatureThresholds t = *this;
	t.minRosetteRadius     *= k;
	t.minRingWidth         *= k;
	t.minEyeletInset       *= k;
	t.minSubRosetteRadius  *= k;
	t.minMouchetteRadius   *= k;
	t.minFilletInnerOffset *= k;
	t.maxMouchetteInset    *= k;
	t.chamferWidthAdd      *= k;
	t.minChamferWidth      *= k;
	t.barRadiusAdd         *= k;
	t.minBarRadius         *= k;
	t.minBand              *= k;
	return t;
}

const char *GothicBuildWarningName (GothicBuildWarning kind)
{
	switch (kind)
	{
		case GothicBuildWarning::DepthFallback:        return "depth_fallback";
		case GothicBuildWarning::FeatureDropped:       return "feature_dropped";
		case GothicBuildWarning::MorphologicalGuard:   return "morphological_guard";
		case GothicBuildWarning::GuardClosingFallback: return "guard_closing_fallback";
		case GothicBuildWarning::GuardInsufficient:    return "guard_insufficient";
		case GothicBuildWarning::ParameterClamped:     return "parameter_clamped";
		case GothicBuildWarning::RegionInvalid:        return "region_invalid";
		case GothicBuildWarning::ProfileFallback:      return "profile_fallback";
		case GothicBuildWarning::MouldingDropped:      return "moulding_dropped";
		case GothicBuildWarning::FieldIgnored:         return "field_ignored";
		case GothicBuildWarning::FieldConverted:       return "field_converted";
		case GothicBuildWarning::ProfileReduced:       return "profile_reduced";
		case GothicBuildWarning::RegionRepaired:       return "region_repaired";
		case GothicBuildWarning::MultipleShells:       return "multiple_shells";
		case GothicBuildWarning::MouldingNotFused:     return "moulding_not_fused";
		case GothicBuildWarning::MouldingSimplified:   return "moulding_simplified";
	}
	return "unknown";
}

const char *GothicBuildErrorName (GothicBuildError kind)
{
	switch (kind)
	{
		case GothicBuildError::InvalidParameter:   return "invalid_parameter";
		case GothicBuildError::ConstructionFailed: return "construction_failed";
		case GothicBuildError::JsonParse:          return "json_parse";
		case GothicBuildError::JsonSchema:         return "json_schema";
		case GothicBuildError::JsonType:           return "json_type";
		case GothicBuildError::JsonValue:          return "json_value";
		case GothicBuildError::JsonMissing:        return "json_missing";
		case GothicBuildError::MeshNotStrict:      return "mesh_not_strict";
	}
	return "unknown";
}

namespace
{
	void appendEntry (std::vector<GothicDiagnostic> &entries, GothicDiagnostic d)
	{
		if (entries.size() < kGothicBuildReportMaxEntries)
			entries.push_back(std::move(d));
	}
}

void GothicBuildReport::Record (GothicBuildWarning kind, std::string parameter, std::string message)
{
	++counts[static_cast<std::size_t>(kind)];
	appendEntry(entries, GothicDiagnostic{ false, static_cast<int>(kind), std::move(parameter), std::move(message) });
}

void GothicBuildReport::Record (GothicBuildError kind, std::string parameter, std::string message)
{
	++errorCounts[static_cast<std::size_t>(kind)];
	appendEntry(entries, GothicDiagnostic{ true, static_cast<int>(kind), std::move(parameter), std::move(message) });
}

bool GothicBuildReport::HasErrors () const
{
	return std::any_of(errorCounts.begin(), errorCounts.end(), [](std::size_t n) { return n > 0; });
}

bool GothicBuildReport::HasWarnings () const
{
	return std::any_of(counts.begin(), counts.end(), [](std::size_t n) { return n > 0; });
}

void GothicBuildReport::Merge (const GothicBuildReport &other)
{
	for (std::size_t k = 0; k < counts.size(); ++k)      counts[k]      += other.counts[k];
	for (std::size_t k = 0; k < errorCounts.size(); ++k) errorCounts[k] += other.errorCounts[k];
	for (const GothicDiagnostic &d : other.entries)
		appendEntry(entries, d);
}

std::string GothicBuildReportToJson (const GothicBuildReport &report)
{
	using nlohmann::json;
	json j;
	json &errors = j["errors"];
	for (std::size_t k = 0; k < kGothicBuildErrorCount; ++k)
		errors[GothicBuildErrorName(static_cast<GothicBuildError>(k))] = report.errorCounts[k];
	json &warnings = j["warnings"];
	for (std::size_t k = 0; k < kGothicBuildWarningCount; ++k)
		warnings[GothicBuildWarningName(static_cast<GothicBuildWarning>(k))] = report.counts[k];
	json entries = json::array();
	for (const GothicDiagnostic &d : report.entries)
		entries.push_back(json{
			{"severity",  d.error ? "error" : "warning"},
			{"code",      d.error ? GothicBuildErrorName(static_cast<GothicBuildError>(d.code))
			                      : GothicBuildWarningName(static_cast<GothicBuildWarning>(d.code))},
			{"parameter", d.parameter},
			{"message",   d.message} });
	j["entries"] = std::move(entries);
	// Exception texts are not guaranteed to be UTF-8 : invalid bytes are replaced.
	return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

Polygon2 buildBayStonePolygon (const WindowGeometry &geom, const GothicMeshParams &params,
                               GothicBuildReport *report)
{
	// Main inner-offset outline (CCW) : the INNER edge of the frame. It bounds
	// the tracery infill and is the region in which the spandrel fillets are cut.
	// (The visible silhouette / contour 0 is the OUTER offset, `frameOuter` below,
	// so the `offset outer` slider grows a real stone frame band around it.)
	std::vector<Vector2d> mainOutline = archOutlineCcw(geom.mainOffset.inner, params.maxAngleRad);

	// Tall-window body : extend the main frame and the lancets straight down
	// below their springline. Without this, every opening is a bare pointed
	// arch (squat "dome" look). The window bottom is `sillMain` ; lancets stop
	// a stone sill band above it so a stone sill remains at the very bottom.
	//
	// Havemann §5.4.2 (thesis Fig. 5.36, l.45) : the main frame bottom is at
	// `heightBott`, and the sub-arches (lancets) bottom is `heightBott + bdOuter`,
	// where bdOuter is the outer field offset (the single contiguous border-plane
	// width, §5.4.1). So the sill band is one border-plane thick — a CONSTANT
	// stone liseré aligned with the frame, NOT a fraction of the body height.
	// bdOuter is recovered from the already-built offset geometry.
	double footYmain  = mainOutline.empty() ? 0.0
	                  : std::min(mainOutline.front().y, mainOutline.back().y);
	double sillMain   = footYmain - params.bodyHeight;
	double bdOuter    = geom.mainOffset.outer.circleL.radius
	                  - geom.mainArch.circleL.radius;   // = window.arch.offset.outer
	double sillLancet = sillMain + std::max(0.0, bdOuter);
	bool   wantBody   = params.bodyHeight > 1e-6;

	std::vector<Vector2d> outer = wantBody ? archWithBody(mainOutline, sillMain)
	                                       : mainOutline;

	// Silhouette (contour 0) : the OUTER offset arch, so a stone frame band of
	// thickness ~(outer offset) frames the whole tracery. Everything (tracery,
	// fillets) lives inside `outer` ⊆ `frameOuter`, so the band is solid stone.
	std::vector<Vector2d> frameOuter = archOutlineCcw(geom.mainOffset.outer, params.maxAngleRad);
	if (wantBody) frameOuter = archWithBody(frameOuter, sillMain);
	if (frameOuter.size() < 3) frameOuter = outer;   // degenerate guard

	std::vector<std::vector<Vector2d>> holes;

	// --- Cusped-void rule (no nested voids) ---------------------------------
	// GLU's NONZERO winding rule cannot represent "void inside void" : a hole
	// cut inside an already-void region flips back to STONE (an island). The
	// old code cut each opening (lancet inner, rosette circle) as a void AND
	// then cut the foils inside it -> the foils became solid stone islands
	// (the "pillars" / "pegs" defect).
	//
	// Fix : for any FOILED opening, do NOT cut its base circle/arch ; cut the
	// FOILS themselves as the voids. Havemann's foils are mutually tangent
	// (they touch only at isolated cusp points), so a set of independent foil
	// holes reproduces the true multifoil opening : void petals, stone cusps
	// between them, and a stone central boss / spandrels — no nesting, no
	// islands. An UN-foiled opening keeps its plain circle/arch void.

	// (kept for reference : cut each foil as an isolated hole. The rosette now
	// uses cuspedOculusCcw for a connected flower instead.)
	[[maybe_unused]] auto pushFoils = [&](const FoilRing &ring) {
		for (const auto &foil : ring.foils)
		{
			if (std::holds_alternative<RoundFoil>(foil))
			{
				const RoundFoil &rf = std::get<RoundFoil>(foil);
				pushAsHole(holes, circleOutlineCcw(rf.circle, params.maxAngleRad));
			}
			else
			{
				const PointedFoil &pf = std::get<PointedFoil>(foil);
				std::vector<Vector2d> contour = pointedFoilContour(pf, params.maxAngleRad);
				// The natural traversal order may be CCW or CW depending on geometry ;
				// push with auto-orientation correction.
				pushAsHoleAnyOrientation(holes, std::move(contour));
			}
		}
	};

	// Lancets. Depth 0 : a plain tall pointed-arch void. Depth >= 1 : each lancet
	// opening becomes a mini-window whose VOIDS are its recursed sub-voids, kept
	// only inside the opening. The opening itself is never cut : the stone is
	// computed once, as frame minus the union of all voids (see the end of this
	// function), so the sub-tracery is part of the frame stone instead of islands
	// sharing the opening's edges.
	std::vector<GothicClippedVoids> recursedOpenings;
	for (const auto &l : geom.subwindows.lancets)
	{
		// Foiled head (thesis approach) : refend the lancet's own arch into a cusped
		// (trefoil) arch, so the foils are part of the opening — no floating disc.
		// Only at recursion depth 0 (deeper lancets become mini-windows instead).
		std::vector<Vector2d> opening =
			(params.lancetHeadFoiled && params.recursionDepth < 1)
				? cuspedArchOutline(l.offset.inner, params.lancetHeadFoils, params.maxAngleRad)
				: archOutlineCcw(l.offset.inner, params.maxAngleRad);
		if (wantBody) opening = archWithBody(opening, sillLancet);

		if (params.recursionDepth >= 1)
		{
			std::vector<PathD> subVoids;
			collectUnitVoids(l.arch, params.recursionOffset, params.recursionSub, sillLancet,
			                 params.recursionDepth, params.recursionFoils,
			                 params.rosetteFoilType, params.rosettePointedness,
			                 params.lancetHeadFoiled, params.lancetHeadFoils, params.filletInset,
			                 params.mouchettes, params.mouchetteType, params.mouchetteSize,
			                 params.maxAngleRad, params.thresholds, report, subVoids);
			if (!subVoids.empty())
			{
				GothicClippedVoids group;
				group.window = std::move(opening);
				group.voids.reserve(subVoids.size());
				for (const PathD &v : subVoids) group.voids.push_back(fromPathD(v));
				recursedOpenings.push_back(std::move(group));
				continue;
			}
		}

		pushAsHole(holes, opening);   // plain tall lancet, or cusped-head lancet
	}

	// Rosette : BAR TRACERY (reference, gothic1.png). The oculus is one big void ;
	// the stone is only THIN BARS tracing the foil outlines + an outer ring. So
	// the foil interiors are void, the CENTRE is a void hole, and there is a void
	// FILLET between the foils and the ring. Bars = each foil outline inflated to
	// a band (Clipper2 InflatePaths), unioned with the ring band, added as stone
	// islands inside the oculus void.
	if (geom.hasRosette)
	{
		const FoilRing &ring = geom.rosetteFoils;
		if (geom.hasRosetteFoils && ring.foils.size() >= 2)
		{
			// Canonical rosette motif (connected multifoil flower + eyelet fillets),
			// shared verbatim with the recursion sub-rosettes.
			for (auto &v : rosetteVoidContours(ring.outerCircle.center, ring.outerCircle.radius,
			                                   (int)ring.foils.size(), params.rosetteFoilType,
			                                   params.rosettePointedness, params.maxAngleRad,
			                                   params.thresholds, report))
				pushAsHoleAnyOrientation(holes, std::move(v));
		}
		else
		{
			const Circle oculus = plainOculusVoid(geom.rosette.circle, params.thresholds);
			if (oculus.radius > 0.0)
				pushAsHole(holes, circleOutlineCcw(oculus, params.maxAngleRad));
			else
				record(report, GothicBuildWarning::FeatureDropped, "oculus: ring leaves no void");
		}
	}

	// Fillets : the corner FIELDS = region − rosette − sub-arches, inset by
	// `filletInset` so a stone bar frames them (Havemann §5.4 ; the 2D-CSG the
	// thesis flagged as future work, p.254 — now done with Clipper2). Opens up
	// the head spandrels that a plain window leaves as solid stone.
	if (params.fillets && geom.hasRosette && !geom.subwindows.lancets.empty())
	{
		PathsD solids;
		solids.push_back(circlePathD(geom.rosette.circle.center,
		                             geom.rosette.circle.radius, params.maxAngleRad));
		for (const auto &l : geom.subwindows.lancets)
		{
			std::vector<Vector2d> a = archOutlineCcw(l.arch, params.maxAngleRad);
			if (wantBody) a = archWithBody(a, sillLancet);
			solids.push_back(toPathD(a));
		}
		PathsD solidsU = Union(solids, FillRule::NonZero);
		PathsD fillets  = Difference(PathsD{ toPathD(outer) }, solidsU, FillRule::NonZero);
		if (params.filletInset > 0.0)
			fillets = InflatePaths(fillets, -params.filletInset, JoinType::Round, EndType::Polygon);
		for (auto &f : fillets)
		{
			if (f.size() < 3) continue;
			if (params.mouchettes)
			{
				// Flamboyant : void the field as an inscribed mouchette/soufflet
				// (clipped to the field) instead of a plain fillet.
				PathsD shaped = mouchetteInField(f, params.mouchetteType, params.mouchetteSize,
				                                 geom.rosette.circle.center, params.maxAngleRad,
				                                 params.thresholds.minMouchetteRadius, report);
				for (auto &s : shaped)
					if (s.size() >= 3) pushAsHoleAnyOrientation(holes, fromPathD(s));
			}
			else
				pushAsHoleAnyOrientation(holes, fromPathD(f));   // plain fillet opening (void)
		}
	}

	// A bay with recursed openings is one 2D boolean : stone = frame minus the
	// union of every void, the level-0 holes included. A bay without them keeps
	// the direct assembly (frame + holes), off the Clipper2 grid, unless it has a
	// contour defect (buildLevel0StoneRegion).
	if (!recursedOpenings.empty())
	{
		GothicStoneInput input;
		input.frame        = std::move(frameOuter);
		input.voids        = std::move(holes);
		input.clippedVoids = std::move(recursedOpenings);
		bool guardApplied = false;
		GothicGuardOutcome outcome = GothicGuardOutcome::NotNeeded;
		const std::vector<GothicRing> region =
			buildGothicStoneRegion(input, kGothicClipperPrecision, kGothicGuardGridSteps,
			                       &guardApplied, &outcome);
		if (guardApplied)
			record(report, GothicBuildWarning::MorphologicalGuard);
		if (outcome == GothicGuardOutcome::ClosingFallback)
			record(report, GothicBuildWarning::GuardClosingFallback,
			       "guard: opening + closing left a contour defect, closing alone kept");
		else if (outcome == GothicGuardOutcome::Insufficient)
			record(report, GothicBuildWarning::GuardInsufficient,
			       "guard: the stone region keeps a contour defect");
		return gothicRegionToPolygon2(region);
	}

	return buildLevel0StoneRegion(frameOuter, holes, report);
}

Polygon2 buildLevel0StoneRegion (const std::vector<Vector2d> &frame,
                                 const std::vector<std::vector<Vector2d>> &holes,
                                 GothicBuildReport *report)
{
	// Pack contours into Polygon2.
	Polygon2 poly;
	poly.alloc_contours(1 + static_cast<int>(holes.size()));

	std::vector<float> outerPacked = packContour(frame);
	poly.add_contour(0, static_cast<unsigned int>(frame.size()), outerPacked.data());

	for (size_t i = 0; i < holes.size(); ++i)
	{
		std::vector<float> holePacked = packContour(holes[i]);
		poly.add_contour(static_cast<unsigned int>(i + 1),
		                 static_cast<unsigned int>(holes[i].size()),
		                 holePacked.data());
	}

	auto defects = [](const ContourValidationReport &check) {
		std::ostringstream os;
		for (std::size_t k = 0; k < kContourDefectCount; ++k)
			if (check.counts[k])
				os << ' ' << ContourDefectName(static_cast<ContourDefect>(k)) << '=' << check.counts[k];
		if (check.truncated)
			os << " (validation truncated)";
		return os.str();
	};

	const ContourValidationReport direct = ValidateContours(poly);
	if (direct.IsValid())
		return poly;

	GothicStoneInput input;
	input.frame = frame;
	input.voids = holes;
	bool guardApplied = false;
	GothicGuardOutcome outcome = GothicGuardOutcome::NotNeeded;
	const std::vector<GothicRing> region =
		buildGothicStoneRegion(input, kGothicClipperPrecision, kGothicGuardGridSteps,
		                       &guardApplied, &outcome);
	if (region.empty())
	{
		record(report, GothicBuildWarning::RegionInvalid,
		       "level-0 stone region: the boolean left no stone, direct assembly kept:" + defects(direct));
		return poly;
	}
	Polygon2 repaired = gothicRegionToPolygon2(region);
	const ContourValidationReport check = ValidateContours(repaired);
	if (guardApplied)
		record(report, GothicBuildWarning::MorphologicalGuard);
	if (outcome == GothicGuardOutcome::ClosingFallback)
		record(report, GothicBuildWarning::GuardClosingFallback,
		       "guard: opening + closing left a contour defect, closing alone kept");
	if (check.IsValid())
		record(report, GothicBuildWarning::RegionRepaired, "level-0 assembly:" + defects(direct));
	else
		record(report, GothicBuildWarning::RegionInvalid,
		       "level-0 stone region, after the boolean and the guard:" + defects(check));
	return repaired;
}

namespace
{
	// Sweep a closed profile along a POLYLINE path (line-segment analogue of
	// sweepArcBuffers in profile_sweep.cpp). Tangent at each vertex = central difference of the
	// neighbours ; in-plane normal N = (T.y, -T.x). Profile point (u, v) maps to
	// 3D = (P + v*scale_v*N, u*scale_u). Buffers are APPENDED (so several paths can
	// be merged into one mesh). `closedPath` wraps the last vertex back to the first.
	void sweepPolylineBuffers (const std::vector<Vector2d> &path, bool closedPath,
	                           const std::vector<Vector2d> &profile,
	                           double scale_u, double scale_v,
	                           std::vector<float> &verts, std::vector<unsigned int> &faces)
	{
		const int N = static_cast<int>(path.size());
		const int M = static_cast<int>(profile.size());
		if (N < 2 || M < 3) return;
		const size_t base = verts.size() / 3;

		for (int i = 0; i < N; ++i)
		{
			Vector2d a = path[(i - 1 + N) % N];
			Vector2d b = path[(i + 1) % N];
			if (!closedPath) { if (i == 0) a = path[0]; if (i == N - 1) b = path[N - 1]; }
			double tx = b.x - a.x, ty = b.y - a.y;
			double L = std::sqrt(tx*tx + ty*ty);
			if (L < 1e-12) { tx = 1.0; ty = 0.0; L = 1.0; }
			tx /= L; ty /= L;
			const double Nx = ty, Ny = -tx;
			const Vector2d &P = path[i];
			for (int j = 0; j < M; ++j)
			{
				verts.push_back(static_cast<float>(P.x + profile[j].y * scale_v * Nx));
				verts.push_back(static_cast<float>(P.y + profile[j].y * scale_v * Ny));
				verts.push_back(static_cast<float>(profile[j].x * scale_u));
			}
		}

		const int segs = closedPath ? N : (N - 1);
		for (int i = 0; i < segs; ++i)
		{
			const int i1 = (i + 1) % N;
			for (int j = 0; j < M; ++j)
			{
				const int j1 = (j + 1) % M;
				const unsigned int v00 = static_cast<unsigned int>(base + static_cast<size_t>(i)  * M + j);
				const unsigned int v10 = static_cast<unsigned int>(base + static_cast<size_t>(i1) * M + j);
				const unsigned int v11 = static_cast<unsigned int>(base + static_cast<size_t>(i1) * M + j1);
				const unsigned int v01 = static_cast<unsigned int>(base + static_cast<size_t>(i)  * M + j1);
				faces.push_back(v00); faces.push_back(v10); faces.push_back(v11);
				faces.push_back(v00); faces.push_back(v11); faces.push_back(v01);
			}
		}
	}
}

namespace
{
	// Recursively collect the STRUCTURAL opening outlines of a window unit for
	// moulding : every sub-lancet frame (cusped/foiled head at the leaves) and each
	// crowning sub-rosette rim, at every recursion level. Mirrors the structural
	// part of collectUnitVoids so the beads track the sub-tracery exactly.
	// Decorative voids (foils, fillets, rosette flower) are NOT moulded. An outline
	// that cannot be built is skipped, recorded as GothicBuildWarning::MouldingDropped.
	void collectMouldingOutlines (const ArchGeom &arch, const ArchOffsetParams &off,
	                              const SubwindowParams &sub, double sillY, int remaining,
	                              bool headFoiled, int headFoils, double maxAngleRad,
	                              const GothicFeatureThresholds &thr,
	                              GothicBuildReport *report,
	                              std::vector<std::vector<Vector2d>> &out)
	{
		if (remaining <= 0) return;
		ArchOffsetGeom o;
		try { o = buildArchOffset(arch, off); }
		catch (...)
		{
			record(report, GothicBuildWarning::MouldingDropped, currentExceptionText("moulding unit offset"));
			return;
		}
		SubwindowsGeom subs;
		try { subs = buildSubwindows(arch, off, sub); }
		catch (...)
		{
			record(report, GothicBuildWarning::MouldingDropped, currentExceptionText("moulding unit subdivision"));
			return;
		}

		for (const auto &l : subs.lancets)
		{
			const ArchOffsetParams lo = childOffset(off, l.arch);
			ArchOffsetGeom lOff;
			try { lOff = buildArchOffset(l.arch, lo); }
			catch (...)
			{
				record(report, GothicBuildWarning::MouldingDropped, currentExceptionText("moulding sub-lancet offset"));
				continue;
			}
			const bool subLeaf = (remaining - 1 <= 0);
			std::vector<Vector2d> opening =
				(headFoiled && subLeaf)
					? cuspedArchOutline(lOff.inner, headFoils, maxAngleRad)
					: archOutlineCcw(lOff.inner, maxAngleRad);
			opening = archWithBody(opening, sillY);
			out.push_back(std::move(opening));
			collectMouldingOutlines(l.arch, lo, sub, sillY, remaining - 1,
			                        headFoiled, headFoils, maxAngleRad, thr, report, out);
		}

		if (subs.lancets.size() == 2)
		{
			const double axisX = 0.5 * (o.inner.circleL.center.x + o.inner.circleR.center.x);
			const Vector2d subApex = childInnerApex(subs.lancets.back(), off, nullptr);
			Circle rc = rosetteTangentToLancets(o.inner, axisX, subApex);
			if (rc.radius > thr.minSubRosetteRadius && rc.center.y > subApex.y)
				out.push_back(circleOutlineCcw(rc, maxAngleRad));
		}
	}

	// Distance from (x, y) to the closed ring `ring` when the point is outside
	// it, 0 inside.
	double distanceOutside (const std::vector<Vector2d> &ring, double x, double y)
	{
		const std::size_t n = ring.size();
		bool inside = false;
		double d = std::numeric_limits<double>::infinity();
		for (std::size_t i = 0, j = n - 1; i < n; j = i++)
		{
			const Vector2d &a = ring[i], &b = ring[j];
			if (((a.y > y) != (b.y > y)) && (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x))
				inside = !inside;
			const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
			const double t = l2 > 0.0 ? std::min(1.0, std::max(0.0, ((x - a.x) * dx + (y - a.y) * dy) / l2)) : 0.0;
			d = std::min(d, std::hypot(a.x + t * dx - x, a.y + t * dy - y));
		}
		return inside ? 0.0 : d;
	}

	// A recursive moulding outline bounded by the window of its unit, as the
	// stone bounds the sub-voids (buildGothicStoneRegion, clippedVoids) : the
	// outline as is when every vertex lies in the window (to 1e-7 x the window
	// extent), otherwise its intersection with the window, on the Clipper2 grid.
	std::vector<std::vector<Vector2d>> boundedByWindow (const std::vector<Vector2d> &outline,
	                                                    const std::vector<Vector2d> &window)
	{
		if (window.size() < 3)
			return { outline };
		double xMin = window[0].x, xMax = xMin, yMin = window[0].y, yMax = yMin;
		for (const Vector2d &v : window)
		{
			xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x);
			yMin = std::min(yMin, v.y); yMax = std::max(yMax, v.y);
		}
		const double tol = 1e-7 * std::hypot(xMax - xMin, yMax - yMin);
		if (std::all_of(outline.begin(), outline.end(),
		                [&](const Vector2d &v) { return distanceOutside(window, v.x, v.y) <= tol; }))
			return { outline };
		std::vector<std::vector<Vector2d>> pieces;
		for (const PathD &p : Intersect(PathsD{ toPathD(outline) }, PathsD{ toPathD(window) },
		                                FillRule::NonZero, kGothicClipperPrecision))
			if (p.size() >= 3)
				pieces.push_back(fromPathD(p));
		return pieces;
	}
}

std::vector<FusedBar> collectBayMouldingOutlines (const WindowGeometry &geom, const GothicMeshParams &params,
                                                  const std::vector<Vector2d> &profile,
                                                  GothicBuildReport *report)
{
	std::vector<FusedBar> bars;
	if (profile.size() < 3) return bars;

	// Sill levels : recomputed identically to buildBayStonePolygon so the moulding
	// tracks the EXACT opening outlines (incl. foiled heads and straight jambs).
	std::vector<Vector2d> mainInner = archOutlineCcw(geom.mainOffset.inner, params.maxAngleRad);
	const bool wantBody = params.bodyHeight > 1e-6;
	const double footYmain = mainInner.empty() ? 0.0
	                       : std::min(mainInner.front().y, mainInner.back().y);
	const double sillMain   = footYmain - params.bodyHeight;
	const double bdOuter    = geom.mainOffset.outer.circleL.radius - geom.mainArch.circleL.radius;
	const double sillLancet = sillMain + std::max(0.0, bdOuter);

	// Profile half-width, used to rescale per outline.
	const double PI = 3.14159265358979323846;
	double halfW = 0.0;
	for (const auto &p : profile) halfW = std::max(halfW, std::fabs(p.y));

	// One moulded bar along `outline`, with a pragmatic "stable extrusion"
	// guard : the bar half-width may not exceed ~0.6 x the outline's characteristic
	// radius, otherwise the swept section self-intersects on tight curvature (small
	// recursion sub-tracery, tiny sub-rosettes). Below a fifth of that the feature
	// is too small to carry the bar at all, so it is skipped rather than crushed.
	auto emit = [&](const std::vector<Vector2d> &outline)
	{
		if (outline.size() < 3 || halfW < 1e-9) return;
		const double charR = std::sqrt(std::fabs(signedAreaShoelace(outline)) / PI);
		const double f = 0.6 * charR / halfW;
		if (f < 0.2) return;                     // feature too small to mould
		bars.push_back({ outline, f >= 0.999 ? 1.0 : f });
	};

	// Main frame inner edge (whole silhouette when a body is present).
	emit(wantBody ? archWithBody(mainInner, sillMain) : mainInner);

	// Lancets : follow the ACTUAL opening outline — cusped (foiled) head at depth 0,
	// plain arch otherwise — exactly as buildBayStonePolygon cuts them. When a lancet
	// recurses into a mini-window, also mould its sub-tracery frames + sub-rosettes.
	for (const auto &l : geom.subwindows.lancets)
	{
		std::vector<Vector2d> opening =
			(params.lancetHeadFoiled && params.recursionDepth < 1)
				? cuspedArchOutline(l.offset.inner, params.lancetHeadFoils, params.maxAngleRad)
				: archOutlineCcw(l.offset.inner, params.maxAngleRad);
		if (wantBody) opening = archWithBody(opening, sillLancet);
		emit(opening);

		if (params.recursionDepth >= 1)
		{
			std::vector<std::vector<Vector2d>> subOutlines;
			collectMouldingOutlines(l.arch, params.recursionOffset, params.recursionSub,
			                        sillLancet, params.recursionDepth,
			                        params.lancetHeadFoiled, params.lancetHeadFoils,
			                        params.maxAngleRad, params.thresholds, report, subOutlines);
			for (const auto &so : subOutlines)
			{
				const std::vector<std::vector<Vector2d>> pieces = boundedByWindow(so, opening);
				if (pieces.empty())
					record(report, GothicBuildWarning::MouldingDropped, "moulding outline: outside its window");
				for (const auto &piece : pieces)
					emit(piece);
			}
		}
	}

	// Rosette : the rim of the lobed motif, the void edge of a plain oculus.
	if (geom.hasRosette)
	{
		if (geom.hasRosetteFoils && geom.rosetteFoils.foils.size() >= 2)
			emit(circleOutlineCcw(geom.rosette.circle, params.maxAngleRad));
		else
		{
			const Circle oculus = plainOculusVoid(geom.rosette.circle, params.thresholds);
			if (oculus.radius > 0.0)
				emit(circleOutlineCcw(oculus, params.maxAngleRad));
		}
	}
	return bars;
}

void buildBayMoulding (const WindowGeometry &geom, const GothicMeshParams &params,
                       const std::vector<Vector2d> &profile, Mesh &out,
                       GothicBuildReport *report)
{
	if (profile.size() < 3) return;
	sweepBayMoulding(collectBayMouldingOutlines(geom, params, profile, report), profile, out, report);
}

void recordFusedBarsOutcome (const FusedBarsResult &result, GothicBuildReport *report)
{
	if (report == nullptr)
		return;
	if (result.failed)
		report->Record(GothicBuildWarning::MouldingNotFused, "Profile",
		               std::string("bars laid on the plate instead: ")
		               + (result.failure ? result.failure : "fusion failed"));
	else if (result.skippedLevels > 0)
		report->Record(GothicBuildWarning::MouldingSimplified, "Profile",
		               std::to_string(result.skippedLevels) + " level step(s) skipped by the ring guard");
}

void sweepBayMoulding (const std::vector<FusedBar> &bars, const std::vector<Vector2d> &profile, Mesh &out,
                       GothicBuildReport *report)
{
	if (profile.size() < 3) return;

	// Base plane of the profile : a scaled bar keeps its foot on it.
	double zBase = profile[0].x;
	for (const auto &p : profile) zBase = std::min(zBase, p.x);

	std::vector<float>        V;
	std::vector<unsigned int> F;
	for (const FusedBar &bar : bars)
	{
		if (bar.scale == 1.0) { sweepPolylineBuffers(bar.outline, true, profile, 1.0, 1.0, V, F); continue; }
		std::vector<Vector2d> scaled;
		scaled.reserve(profile.size());
		for (const auto &q : profile)
			scaled.push_back(Vector2d(zBase + (q.x - zBase) * bar.scale, q.y * bar.scale));
		sweepPolylineBuffers(bar.outline, true, scaled, 1.0, 1.0, V, F);
	}

	if (V.empty() || F.empty()) return;
	out.SetVertices(static_cast<unsigned int>(V.size() / 3), V.data());
	out.SetFaces   (static_cast<unsigned int>(F.size() / 3), 3, F.data());
	if (report)
		report->Record(GothicBuildWarning::MultipleShells, "Profile",
		               std::to_string(bars.size()) + " moulding bar(s), each a closed shell laid on the plate, not fused to it");
}

// L'extrusion a moulure (extrudeProfiledToMesh, parametree par un Profile2D, avec
// son garde-fou d'auto-intersection) est une primitive generique de cgmesh
// (profile2d.{h,cpp}) ; ce fichier l'appelle comme tout autre consommateur.

void writeBayMesh (const WindowGeometry &geom, const std::string &filePath,
                   const GothicMeshParams &params)
{
	Polygon2 poly = buildBayStonePolygon(geom, params);
	Mesh mesh;

	if (params.zHeight > 0.0)
		extrudeToMesh(poly, mesh, 0.0, params.zHeight);
	else
		tessellateToMesh(poly, mesh, 0.0);

	int rc = mesh.save(filePath.c_str());
	if (rc < 0)
		throw std::runtime_error("writeBayMesh: Mesh::save failed for " + filePath);
}

} // namespace cgarchi
