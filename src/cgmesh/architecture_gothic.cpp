#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "architecture_gothic.h"

#include "clipper2/clipper.h"   // 2D polygon boolean (vendored, extern/clipper2)

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

	// Fillet openings = region − solids, inset by `inset` (bdInner).
	PathsD filletPaths (const PathD &region, const PathsD &solids, double inset)
	{
		PathsD f = Difference(PathsD{ region }, Union(solids, FillRule::NonZero), FillRule::NonZero);
		if (inset > 0.0) f = InflatePaths(f, -inset, JoinType::Round, EndType::Polygon);
		return f;
	}

	// The ONE canonical rosette motif, shared by the main rosette and the recursion
	// sub-rosettes : a single CONNECTED multifoil flower (pointed or round) with an
	// open centre, plus one eyelet fillet per foil between the foils and the ring.
	// Returns the void contours (CCW). Cr = centre, Rr = outer/ring radius, n = foil
	// count, foilType (1 = pointed, 0 = round), pointedness for the pointed variant.
	std::vector<std::vector<Vector2d>> rosetteVoidContours (Vector2d Cr, double Rr, int n,
	                                                        int foilType, double pointedness,
	                                                        double maxAngleRad)
	{
		const double PI = 3.14159265358979323846;
		std::vector<std::vector<Vector2d>> out;
		if (n < 2 || Rr <= 1.0) return out;
		const double ringW = std::max(3.0, 0.10 * Rr);
		const double Rin   = Rr - ringW;
		if (Rin <= 1.0) return out;
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
		const double rinset = std::max(1.5, 0.28 * ringW);
		for (auto &f : filletPaths(circlePathD(Cr, Rin, maxAngleRad), PathsD{ flowerPath }, rinset))
			if (f.size() >= 3) out.push_back(fromPathD(f));   // one eyelet fillet per foil
		return out;
	}

	// Recursively collect every GLASS void contour of a window unit (base arch
	// `arch`). A unit is built like the whole window : sub-lancets (leaves get a
	// foiled head, or recurse), a crowning daisy rosette, and corner fillets.
	// The caller Differences these from the unit opening to get the stone.
	// Degenerate sub-geometry is skipped (the buildXxx may throw on tiny arches).
	void collectUnitVoids (const ArchGeom &arch, const ArchOffsetParams &off,
	                       const SubwindowParams &sub, double sillY,
	                       int remaining, int foilN, int foilType, double pointedness,
	                       bool headFoiled, int headFoils, double filletInset,
	                       double maxAngleRad, std::vector<PathD> &out)
	{
		ArchOffsetGeom o;
		try { o = buildArchOffset(arch, off); }
		catch (...) { return; }

		std::vector<Vector2d> openingV = archWithBody(archOutlineCcw(o.inner, maxAngleRad), sillY);

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
		catch (...) { out.push_back(toPathD(openingV)); return; }

		PathsD solids;   // rosette disk + sub-arch base arches, for this unit's fillets
		for (const auto &l : subs.lancets)
		{
			ArchOffsetParams lo = off;                       // thinner mullions deeper down
			const double r = l.arch.circleL.radius;
			lo.inner = std::min(off.inner, 0.30 * r);
			lo.outer = std::min(off.outer, 0.30 * r);
			solids.push_back(toPathD(archWithBody(archOutlineCcw(l.arch, maxAngleRad), sillY)));
			collectUnitVoids(l.arch, lo, sub, sillY, remaining - 1, foilN, foilType, pointedness,
			                 headFoiled, headFoils, filletInset * 0.7, maxAngleRad, out);
		}

		// Crowning sub-rosette : passes through the two sub-lancet apexes and is
		// tangent to this unit's arch. Uses the SAME motif as the main rosette
		// (connected multifoil flower + eyelet fillets). RULES : (a) the unit must
		// subdivide into exactly two sub-lancets ; (b) the rosette must REST ON them
		// — its centre ABOVE their apexes (else it would hang with no support below).
		if (subs.lancets.size() == 2)
		{
			const double axisX = 0.5*(o.inner.circleL.center.x + o.inner.circleR.center.x);
			const Vector2d subApex = subs.lancets.back().offset.inner.apex;
			Circle rc = rosetteTangentToLancets(o.inner, axisX, subApex);
			if (rc.radius > 3.0 && rc.center.y > subApex.y)
			{
				solids.push_back(circlePathD(rc.center, rc.radius, maxAngleRad));   // for corner fillets
				for (auto &v : rosetteVoidContours(rc.center, rc.radius, foilN,
				                                   foilType, pointedness, maxAngleRad))
					out.push_back(toPathD(v));
			}
		}

		// Corner fillets for this unit.
		if (filletInset > 0.0 && !solids.empty())
		{
			for (auto &f : filletPaths(toPathD(openingV), solids, filletInset))
				if (f.size() >= 3) out.push_back(std::move(f));
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

Polygon2 buildBayStonePolygon (const WindowGeometry &geom, const GothicMeshParams &params)
{
	// Main inner-offset outline (CCW) : the INNER edge of the frame. It bounds
	// the tracery infill and is the region in which the spandrel fillets are cut.
	// (The visible silhouette / contour 0 is the OUTER offset, `frameOuter` below,
	// so the `offset outer` slider grows a real stone frame band around it.)
	std::vector<Vector2d> mainOutline = archOutlineCcw(geom.mainOffset.inner, params.maxAngleRad);

	// Tall-window body : extend the main frame and the lancets straight down
	// below their springline. Without this, every opening is a bare pointed
	// arch (squat "dome" look). The window bottom is `sillMain` ; lancets stop
	// a `band` above it so a stone sill remains at the very bottom.
	double footYmain  = mainOutline.empty() ? 0.0
	                  : std::min(mainOutline.front().y, mainOutline.back().y);
	double sillMain   = footYmain - params.bodyHeight;
	double band       = 0.12 * params.bodyHeight;
	double sillLancet = sillMain + band;
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
	// opening becomes a mini-window — cut the opening as a void, then add back the
	// stone tracery (mullions + sub-frames + sub-rosette) computed as
	// Difference(opening, all-recursed-sub-voids) via Clipper2. The mullions stay
	// connected to the frame at the sill and the arch, so nothing floats.
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
			                 params.maxAngleRad, subVoids);
			if (!subVoids.empty())
			{
				PathsD allVoid = Union(PathsD(subVoids.begin(), subVoids.end()), FillRule::NonZero);
				PathsD stone   = Difference(PathsD{ toPathD(opening) }, allVoid, FillRule::NonZero);
				pushAsHole(holes, opening);                        // opening = void
				for (auto &p : stone) holes.push_back(fromPathD(p)); // mullions/frames/sub-rosette stone
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
			                                   params.rosettePointedness, params.maxAngleRad))
				pushAsHoleAnyOrientation(holes, std::move(v));
		}
		else
			pushAsHole(holes, circleOutlineCcw(geom.rosette.circle, params.maxAngleRad));
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
			pushAsHoleAnyOrientation(holes, fromPathD(f));   // fillet opening (void)
		}
	}

	// Pack contours into Polygon2.
	Polygon2 poly;
	poly.alloc_contours(1 + static_cast<int>(holes.size()));

	std::vector<float> outerPacked = packContour(frameOuter);
	poly.add_contour(0, static_cast<unsigned int>(frameOuter.size()), outerPacked.data());

	for (size_t i = 0; i < holes.size(); ++i)
	{
		std::vector<float> holePacked = packContour(holes[i]);
		poly.add_contour(static_cast<unsigned int>(i + 1),
		                 static_cast<unsigned int>(holes[i].size()),
		                 holePacked.data());
	}

	return poly;
}

void tessellateToMesh (Polygon2 &polygon, Mesh &out, double z)
{
	float        *pV = nullptr;
	unsigned int  nV = 0;
	unsigned int *pF = nullptr;
	unsigned int  nF = 0;

	polygon.tesselate(&pV, &nV, &pF, &nF);

	if (nV == 0 || nF == 0)
	{
		if (pV) free(pV);
		if (pF) free(pF);
		throw std::runtime_error("tessellateToMesh: tessellation produced no output");
	}

	// Polygon2 packs vertices as (x, y, 0). Override z if non-zero.
	if (z != 0.0)
	{
		const float zf = static_cast<float>(z);
		for (unsigned int i = 0; i < nV; ++i)
			pV[3*i + 2] = zf;
	}

	out.SetVertices(nV, pV);
	out.SetFaces(nF, 3, pF);

	free(pV);
	free(pF);
}

std::vector<Vector2d> rectangularProfile ()
{
	// CCW from path-tangent perspective (= when looking along +T, with B = "up",
	// N = "right") :
	//   (0, 0) bottom-left  -> (1, 0) top-left  -> (1, 1) top-right  -> (0, 1) bottom-right
	return { Vector2d(0.0, 0.0), Vector2d(1.0, 0.0),
	         Vector2d(1.0, 1.0), Vector2d(0.0, 1.0) };
}

std::vector<Vector2d> chamferProfile (double depth)
{
	// 5-point profile : the outside-bottom corner of a unit square is replaced
	// by a 45-deg bevel of size `depth` (in normalized [0,1] coords).
	return {
		Vector2d(0.0,   depth),    // outside, just above chamfer
		Vector2d(depth, 0.0),      // bottom, at end of chamfer
		Vector2d(1.0,   0.0),      // inner-bottom corner
		Vector2d(1.0,   1.0),      // inner-top corner
		Vector2d(0.0,   1.0)       // outside-top corner
	};
}

std::vector<Vector2d> cavettoProfile (double depth, int nCurveSegments)
{
	// Concave quarter-circle replacing the outside-bottom corner. The curve is
	// centered at (depth, depth) with radius `depth`, sampled CCW from angle pi
	// (point (0, depth)) to angle 3*pi/2 (point (depth, 0)).
	if (nCurveSegments < 2) nCurveSegments = 2;
	std::vector<Vector2d> profile;
	profile.reserve(nCurveSegments + 4);
	const double PI = 3.14159265358979323846;
	for (int i = 0; i <= nCurveSegments; ++i)
	{
		double t = static_cast<double>(i) / nCurveSegments;
		double angle = PI + t * (PI / 2.0);
		double x = depth + depth * std::cos(angle);
		double y = depth + depth * std::sin(angle);
		profile.push_back(Vector2d(x, y));
	}
	profile.push_back(Vector2d(1.0, 0.0));
	profile.push_back(Vector2d(1.0, 1.0));
	profile.push_back(Vector2d(0.0, 1.0));
	return profile;
}

namespace
{
	// Build the (vertices, faces) buffers of one tube section for `arc`.
	// Vertex layout : index = i * M + j with i = path sample, j = profile point.
	void sweepArcBuffers (const Arc &arc,
	                       const std::vector<Vector2d> &profile,
	                       double scale_u,
	                       double scale_v,
	                       double maxAngleRad,
	                       std::vector<float> &verts,
	                       std::vector<unsigned int> &faces)
	{
		std::vector<Vector2d> pathPos = arc.tessellateAdaptive(maxAngleRad);
		const int N = static_cast<int>(pathPos.size());
		const int M = static_cast<int>(profile.size());

		verts.resize(static_cast<size_t>(3) * N * M);
		for (int i = 0; i < N; ++i)
		{
			const Vector2d &P = pathPos[i];
			const double t = (N == 1) ? 0.0 : static_cast<double>(i) / (N - 1);
			Vector2d T = arc.tangentAt(t);
			const double Nx =  T.y;
			const double Ny = -T.x;

			for (int j = 0; j < M; ++j)
			{
				const double u = profile[j].x;
				const double v = profile[j].y;
				const int idx = i * M + j;
				verts[3*idx + 0] = static_cast<float>(P.x + v * scale_v * Nx);
				verts[3*idx + 1] = static_cast<float>(P.y + v * scale_v * Ny);
				verts[3*idx + 2] = static_cast<float>(u * scale_u);
			}
		}

		faces.reserve(faces.size() + static_cast<size_t>(3) * 2 * (N - 1) * M);
		for (int i = 0; i < N - 1; ++i)
		{
			for (int j = 0; j < M; ++j)
			{
				const int j1 = (j + 1) % M;
				const unsigned int v00 = i       * M + j;
				const unsigned int v10 = (i + 1) * M + j;
				const unsigned int v11 = (i + 1) * M + j1;
				const unsigned int v01 = i       * M + j1;

				faces.push_back(v00);
				faces.push_back(v10);
				faces.push_back(v11);

				faces.push_back(v00);
				faces.push_back(v11);
				faces.push_back(v01);
			}
		}
	}
}

void sweepProfileAlongArc (const Arc &arc,
                            const std::vector<Vector2d> &profile,
                            double scale_u,
                            double scale_v,
                            Mesh &out,
                            double maxAngleRad)
{
	if (profile.size() < 3)
		throw std::invalid_argument("sweepProfileAlongArc: profile must have at least 3 points");
	if (arc.length() <= 0.0)
		throw std::invalid_argument("sweepProfileAlongArc: arc has non-positive length");

	std::vector<float> verts;
	std::vector<unsigned int> faces;
	sweepArcBuffers(arc, profile, scale_u, scale_v, maxAngleRad, verts, faces);

	if (verts.empty() || faces.empty())
		throw std::invalid_argument("sweepProfileAlongArc: arc tessellation produced empty mesh");

	out.SetVertices(static_cast<unsigned int>(verts.size() / 3), verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
}

void sweepProfileAlongArcs (const std::vector<Arc> &arcs,
                             const std::vector<Vector2d> &profile,
                             double scale_u,
                             double scale_v,
                             Mesh &out,
                             double maxAngleRad)
{
	if (arcs.empty())
		throw std::invalid_argument("sweepProfileAlongArcs: arcs must not be empty");
	if (profile.size() < 3)
		throw std::invalid_argument("sweepProfileAlongArcs: profile must have at least 3 points");

	std::vector<float> totalVerts;
	std::vector<unsigned int> totalFaces;

	for (const Arc &arc : arcs)
	{
		if (arc.length() <= 0.0)
			continue;     // skip degenerate arcs silently

		std::vector<float> sectionVerts;
		std::vector<unsigned int> sectionFaces;
		sweepArcBuffers(arc, profile, scale_u, scale_v, maxAngleRad,
		                sectionVerts, sectionFaces);

		const unsigned int offset = static_cast<unsigned int>(totalVerts.size() / 3);
		totalVerts.insert(totalVerts.end(), sectionVerts.begin(), sectionVerts.end());
		for (unsigned int idx : sectionFaces)
			totalFaces.push_back(idx + offset);
	}

	if (totalVerts.empty() || totalFaces.empty())
		throw std::invalid_argument("sweepProfileAlongArcs: all arcs were degenerate");

	out.SetVertices(static_cast<unsigned int>(totalVerts.size() / 3), totalVerts.data());
	out.SetFaces(static_cast<unsigned int>(totalFaces.size() / 3), 3, totalFaces.data());
}

void appendMesh (Mesh &dest, const Mesh &src)
{
	// Read dest's current vertex/face count.
	Mesh &destNc = const_cast<Mesh &>(dest);  // GetNVertices / GetVertex are non-const
	Mesh &srcNc  = const_cast<Mesh &>(src);

	const unsigned int destNV = destNc.GetNVertices();
	const unsigned int destNF = destNc.GetNFaces();
	const unsigned int srcNV  = srcNc.GetNVertices();
	const unsigned int srcNF  = srcNc.GetNFaces();

	// Build a combined vertex array.
	std::vector<float> verts;
	verts.reserve(3 * (destNV + srcNV));
	for (unsigned int i = 0; i < destNV; ++i)
	{
		float v[3];
		destNc.GetVertex(i, v);
		verts.push_back(v[0]); verts.push_back(v[1]); verts.push_back(v[2]);
	}
	for (unsigned int i = 0; i < srcNV; ++i)
	{
		float v[3];
		srcNc.GetVertex(i, v);
		verts.push_back(v[0]); verts.push_back(v[1]); verts.push_back(v[2]);
	}

	// Build a combined face array, shifting src's indices by destNV.
	std::vector<unsigned int> faces;
	faces.reserve(3 * (destNF + srcNF));
	for (unsigned int i = 0; i < destNF; ++i)
		for (unsigned int j = 0; j < 3; ++j)
			faces.push_back(static_cast<unsigned int>(destNc.GetFaceVertex(i, j)));
	for (unsigned int i = 0; i < srcNF; ++i)
		for (unsigned int j = 0; j < 3; ++j)
			faces.push_back(static_cast<unsigned int>(srcNc.GetFaceVertex(i, j)) + destNV);

	dest.SetVertices(destNV + srcNV, verts.data());
	dest.SetFaces(destNF + srcNF, 3, faces.data());
}

void extrudeToMesh (Polygon2 &polygon, Mesh &out, double zBottom, double zTop)
{
	float        *pV = nullptr;
	unsigned int  nV = 0;
	unsigned int *pF = nullptr;
	unsigned int  nF = 0;

	polygon.tesselate(&pV, &nV, &pF, &nF);

	if (nV == 0 || nF == 0)
	{
		if (pV) free(pV);
		if (pF) free(pF);
		throw std::runtime_error("extrudeToMesh: tessellation produced no output");
	}

	// Vertex layout : caps and walls get DISTINCT vertices so per-vertex normals
	// stay correct at the hard front/back-to-wall edges. If caps and walls
	// shared vertices, normal averaging would tilt the flat faces near every
	// contour edge (visible shading gradient / facets). Layout :
	//   [0,    N)   : cap top    (z = zTop)    — used only by the top cap
	//   [N,   2N)   : cap bottom (z = zBottom) — used only by the bottom cap
	//   [2N,  3N)   : wall top   (z = zTop)    — used only by side walls
	//   [3N,  4N)   : wall bottom(z = zBottom) — used only by side walls
	// Wall vertices are shared BETWEEN adjacent wall quads of the same contour,
	// so the walls smooth-shade nicely around the curved holes and arcs.
	const unsigned int N = nV;
	const float zT = static_cast<float>(zTop);
	const float zB = static_cast<float>(zBottom);

	std::vector<float> verts(4 * N * 3);
	for (unsigned int i = 0; i < N; ++i)
	{
		const float x = pV[3*i + 0];
		const float y = pV[3*i + 1];
		verts[3*(i      ) + 0] = x; verts[3*(i      ) + 1] = y; verts[3*(i      ) + 2] = zT; // cap top
		verts[3*(i +   N) + 0] = x; verts[3*(i +   N) + 1] = y; verts[3*(i +   N) + 2] = zB; // cap bottom
		verts[3*(i + 2*N) + 0] = x; verts[3*(i + 2*N) + 1] = y; verts[3*(i + 2*N) + 2] = zT; // wall top
		verts[3*(i + 3*N) + 0] = x; verts[3*(i + 3*N) + 1] = y; verts[3*(i + 3*N) + 2] = zB; // wall bottom
	}

	std::vector<unsigned int> faces;
	faces.reserve(3 * (2 * nF + 4 * 200));   // upper bound for typical configs

	// Top cap : keep original CCW triangles (normals +z).
	for (unsigned int i = 0; i < nF; ++i)
	{
		faces.push_back(pF[3*i + 0]);
		faces.push_back(pF[3*i + 1]);
		faces.push_back(pF[3*i + 2]);
	}

	// Bottom cap : same triangles, indices shifted by N, winding REVERSED so
	// that the normals point -z (outward, downward).
	for (unsigned int i = 0; i < nF; ++i)
	{
		faces.push_back(N + pF[3*i + 0]);
		faces.push_back(N + pF[3*i + 2]);
		faces.push_back(N + pF[3*i + 1]);
	}

	// Side walls : for each contour edge (P_i -> P_j), emit two triangles whose
	// outward normal points away from the stone region. Uses the dedicated wall
	// vertices (regions 2 = top, 3 = bottom).
	//
	// Convention : in a CCW outer contour, walking P_i -> P_j has the polygon
	// stone on the LEFT, the outside on the RIGHT. In a CW hole, walking
	// P_i -> P_j has the polygon stone on the LEFT, the hole on the RIGHT.
	// In both cases, "right of walk" = outward-of-stone, so the SAME triangle
	// ordering produces correctly-oriented walls :
	//   tri 1 = (P_i_top, P_i_bot, P_j_bot)
	//   tri 2 = (P_i_top, P_j_bot, P_j_top)
	unsigned int contourStart = 0;
	for (int ci = 0; ci < polygon.get_n_contours(); ++ci)
	{
		const unsigned int nPts = polygon.get_n_points(ci);
		for (unsigned int i = 0; i < nPts; ++i)
		{
			const unsigned int j        = (i + 1) % nPts;
			const unsigned int p_i_top  = 2*N + contourStart + i;
			const unsigned int p_j_top  = 2*N + contourStart + j;
			const unsigned int p_i_bot  = N + p_i_top;   // region 3 = region 2 + N
			const unsigned int p_j_bot  = N + p_j_top;

			faces.push_back(p_i_top);
			faces.push_back(p_i_bot);
			faces.push_back(p_j_bot);

			faces.push_back(p_i_top);
			faces.push_back(p_j_bot);
			faces.push_back(p_j_top);
		}
		contourStart += nPts;
	}

	out.SetVertices(4 * N, verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());

	free(pV);
	free(pF);
}

namespace
{
	// Per-vertex "into the stone" normal for a contour stored with the stone on
	// its LEFT (outer CCW, holes CW). = normalize(avg of rot90(edge)) with
	// rot90(dx,dy) = (-dy,dx) (points left of the walk).
	std::vector<Vector2d> intoStoneNormals (const std::vector<Vector2d> &pts)
	{
		const size_t n = pts.size();
		std::vector<Vector2d> en(n);   // per-edge normal (edge i -> i+1)
		for (size_t i = 0; i < n; ++i)
		{
			const Vector2d &a = pts[i], &b = pts[(i + 1) % n];
			double dx = b.x - a.x, dy = b.y - a.y;
			double L = std::sqrt(dx*dx + dy*dy);
			en[i] = (L < 1e-12) ? Vector2d(0,0) : Vector2d(-dy / L, dx / L);
		}
		std::vector<Vector2d> vn(n);
		for (size_t i = 0; i < n; ++i)
		{
			Vector2d s = en[(i + n - 1) % n] + en[i];
			double L = std::sqrt(s.x*s.x + s.y*s.y);
			vn[i] = (L < 1e-9) ? en[i] : Vector2d(s.x / L, s.y / L);
		}
		return vn;
	}
}

void extrudeProfiledToMesh (Polygon2 &polygon, Mesh &out,
                            double zBottom, double zTop, double chamW, double chamD)
{
	const int nc = polygon.get_n_contours();
	if (nc <= 0) throw std::runtime_error("extrudeProfiledToMesh: empty polygon");
	if (chamD > (zTop - zBottom)) chamD = zTop - zBottom;   // chamfer can't exceed depth

	// Base (v=0) and offset (into-stone by a PER-CONTOUR chamfer) point rings.
	// Small openings (rosette foils, fillet bars) get a proportionally smaller
	// chamfer so the moulding never eats the thin bars nor merges neighbouring
	// foils ; large openings (lancets) keep the full chamfer. chamW_c is capped
	// by the contour's characteristic radius sqrt(|area|/pi).
	const double PI = 3.14159265358979323846;
	std::vector<std::vector<Vector2d>> base(nc), off(nc);
	std::vector<double> zChfC(nc);
	for (int ci = 0; ci < nc; ++ci)
	{
		const unsigned int n = polygon.get_n_points(ci);
		const float *p = polygon.get_points(ci);
		base[ci].resize(n);
		for (unsigned int i = 0; i < n; ++i) base[ci][i] = Vector2d(p[2*i], p[2*i+1]);
		const double area  = signedAreaShoelace(base[ci]);
		const double charR = std::sqrt(std::fabs(area) / PI);
		double chamW_c = std::min(chamW, 0.32 * charR);
		if (chamW_c < 0.3) chamW_c = std::min(chamW, 0.3);
		const double chamD_c = std::min(chamD, 2.5 * chamW_c);

		std::vector<Vector2d> nrm = intoStoneNormals(base[ci]);
		off[ci].resize(n);
		for (unsigned int i = 0; i < n; ++i)
			off[ci][i] = Vector2d(base[ci][i].x + chamW_c * nrm[i].x,
			                      base[ci][i].y + chamW_c * nrm[i].y);

		// Skip the chamfer (vertical wall) when it would produce spurious bowls /
		// "drops" : (a) SMALL contours — foils, foiled heads — where the moulding
		// is proportionally too big and overlaps the thin stone ; (b) CUSPED
		// contours (rosette flower, foiled heads) whose inward offset self-crosses
		// at the sharp concave cusps. Only large convex openings (lancets, main
		// arch) keep the moulding.
		//
		// (b) is measured as the total CONCAVE turning : the sum of per-vertex turn
		// angles that run OPPOSITE the contour's overall orientation (= reflex /
		// cusp vertices). This is sampling-invariant — splitting an edge adds zero
		// turn — so it catches finely-tessellated SHARP cusps (pointed flower) that
		// the old per-edge "offset folded back" test missed (each short edge's
		// offset never fully reversed). Convex openings have ~zero concave turning.
		bool skip = (charR < std::max(24.0, 4.0 * chamW));
		if (!skip)
		{
			const double orient = (area >= 0.0) ? 1.0 : -1.0;
			double concaveTurn = 0.0;
			for (unsigned int i = 0; i < n; ++i)
			{
				const Vector2d &a = base[ci][(i + n - 1) % n];
				const Vector2d &b = base[ci][i];
				const Vector2d &c = base[ci][(i + 1) % n];
				const double e1x = b.x - a.x, e1y = b.y - a.y;
				const double e2x = c.x - b.x, e2y = c.y - b.y;
				const double cross = e1x*e2y - e1y*e2x;
				const double dot   = e1x*e2x + e1y*e2y;
				const double turn  = std::atan2(cross, dot);   // signed turn at b
				if (turn * orient < 0.0) concaveTurn += -turn * orient;
			}
			if (concaveTurn > 0.8) skip = true;                // ~45 deg of cusps
		}
		if (skip) { off[ci] = base[ci]; zChfC[ci] = zTop; }        // vertical wall
		else      { zChfC[ci] = zTop - chamD_c; }
	}

	std::vector<float>        V;
	std::vector<unsigned int> F;
	auto addV = [&](double x, double y, double z) -> unsigned int
	{ unsigned int id = (unsigned int)(V.size() / 3); V.push_back((float)x); V.push_back((float)y); V.push_back((float)z); return id; };

	// --- front cap : stone face at zTop (v=0) ---
	{
		float *pV=nullptr; unsigned int nV=0,*pF=nullptr,nF=0;
		polygon.tesselate(&pV,&nV,&pF,&nF);
		if (!nV||!nF){ if(pV)free(pV); if(pF)free(pF); throw std::runtime_error("extrudeProfiledToMesh: front tess empty"); }
		unsigned int b=(unsigned int)(V.size()/3);
		for (unsigned int i=0;i<nV;++i) addV(pV[3*i],pV[3*i+1],zTop);
		for (unsigned int i=0;i<nF;++i){ F.push_back(b+pF[3*i]); F.push_back(b+pF[3*i+1]); F.push_back(b+pF[3*i+2]); }
		free(pV); free(pF);
	}

	// --- side moulding : sweep the chamfer profile along every contour ---
	// Levels : L0 (base, zTop) -> L1 (off, zTop-chamD) -> L2 (off, zBottom).
	const double zChf = zTop - chamD;
	for (int ci = 0; ci < nc; ++ci)
	{
		const size_t n = base[ci].size();
		if (n < 2) continue;
		unsigned int L0 = (unsigned int)(V.size()/3);
		for (size_t i=0;i<n;++i) addV(base[ci][i].x, base[ci][i].y, zTop);
		unsigned int L1 = (unsigned int)(V.size()/3);
		for (size_t i=0;i<n;++i) addV(off[ci][i].x,  off[ci][i].y,  zChf);
		unsigned int L2 = (unsigned int)(V.size()/3);
		for (size_t i=0;i<n;++i) addV(off[ci][i].x,  off[ci][i].y,  zBottom);
		auto strip = [&](unsigned int A, unsigned int B){   // A=front level, B=back level
			for (size_t i=0;i<n;++i){
				unsigned int j=(unsigned int)((i+1)%n);
				unsigned int ai=A+(unsigned int)i, aj=A+j, bi=B+(unsigned int)i, bj=B+j;
				F.push_back(ai); F.push_back(bi); F.push_back(bj);
				F.push_back(ai); F.push_back(bj); F.push_back(aj);
			}
		};
		strip(L0, L1);            // the chamfer
		if (zChf > zBottom + 1e-6) strip(L1, L2);   // straight part
	}

	// --- back cap : offset stone face at zBottom ---
	Polygon2 backPoly;
	backPoly.alloc_contours(nc);
	std::vector<std::vector<float>> packed(nc);
	for (int ci=0; ci<nc; ++ci){
		packed[ci].resize(off[ci].size()*2);
		for (size_t i=0;i<off[ci].size();++i){ packed[ci][2*i]=(float)off[ci][i].x; packed[ci][2*i+1]=(float)off[ci][i].y; }
		backPoly.add_contour(ci, (unsigned int)off[ci].size(), packed[ci].data());
	}
	{
		float *pV=nullptr; unsigned int nV=0,*pF=nullptr,nF=0;
		backPoly.tesselate(&pV,&nV,&pF,&nF);
		if (nV&&nF){
			unsigned int b=(unsigned int)(V.size()/3);
			for (unsigned int i=0;i<nV;++i) addV(pV[3*i],pV[3*i+1],zBottom);
			for (unsigned int i=0;i<nF;++i){ F.push_back(b+pF[3*i]); F.push_back(b+pF[3*i+2]); F.push_back(b+pF[3*i+1]); } // reversed -> -z
		}
		if(pV)free(pV); if(pF)free(pF);
	}

	out.SetVertices((unsigned int)(V.size()/3), V.data());
	out.SetFaces   ((unsigned int)(F.size()/3), 3, F.data());
}

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
