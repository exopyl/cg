#include <cgarchi/classical_acanthus_detail.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_volute.h>

namespace cgarchi::classical::detail
{
namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
using Vec = std::array<double, 3>;

Vec Add (const Vec &a, const Vec &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec Sub (const Vec &a, const Vec &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec Mul (double s, const Vec &a) { return {s * a[0], s * a[1], s * a[2]}; }
double Dot (const Vec &a, const Vec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec Cross (const Vec &a, const Vec &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double Norm (const Vec &a) { return std::sqrt (Dot (a, a)); }
Vec Unit (const Vec &a) { return Mul (1.0 / Norm (a), a); }

[[noreturn]] void Fail (const std::string &what)
{
	throw std::invalid_argument ("BuildVoluteRibbon : " + what);
}

double Smootherstep (double s)
{
	return s * s * s * (s * (6.0 * s - 15.0) + 10.0);
}

// Station du ruban : bord exterieur, direction vers le bord interieur, normale
// de la face vue, largeur.
struct Station
{
	Vec outer, m, n;
	double width = 0.0;
	double thickness = 1.0;   // part de la profondeur gardee, face interieure fixe
};

// Cote z de la section a l'epaisseur `thickness` : la face interieure (-depth/2) reste en place.
double Thinned (double z, double depth, double thickness)
{
	return thickness == 1.0 ? z : -0.5 * depth + (z + 0.5 * depth) * thickness;
}

// Section (s, z) : face vue de s = 0 a s = 1, puis dos (1, -b/2) et (0, -b/2).
std::vector<std::array<double, 2>> Section (const VoluteSpec &spec, double width)
{
	std::vector<double> s;
	const double bead = spec.beadShare, groove = spec.grooveShare;
	const double grooveStart = 0.5 - 0.5 * groove, grooveEnd = 0.5 + 0.5 * groove;
	for (int i = 0; i < spec.beadSegments; ++i)
		s.push_back (bead * i / spec.beadSegments);
	for (int i = 0; i < spec.grooveSegments; ++i)
		s.push_back (grooveStart + groove * i / spec.grooveSegments);
	for (int i = 0; i < spec.beadSegments; ++i)
		s.push_back (grooveEnd + bead * i / spec.beadSegments);
	s.push_back (1.0);
	const double half = 0.5 * spec.depth;
	std::vector<std::array<double, 2>> out;
	for (const double si : s)
	{
		double z = half;
		if (si > 0.0 && si < bead)
			z += spec.beadHeight * width * std::sin (kPi * si / bead);
		else if (si > grooveStart && si < grooveEnd)
			z -= spec.grooveDepth * std::sin (kPi * (si - grooveStart) / groove);
		else if (si > grooveEnd && si < 1.0)
			z += spec.beadHeight * width * std::sin (kPi * (1.0 - si) / bead);
		out.push_back ({si, z});
	}
	out.push_back ({1.0, -half});
	out.push_back ({0.0, -half});
	return out;
}

double SignedVolume (const std::vector<Vec> &p, const std::vector<std::array<unsigned, 3>> &t, std::size_t t0, std::size_t t1)
{
	double v = 0.0;
	for (std::size_t i = t0; i < t1; ++i)
		v += Dot (p[t[i][0]], Cross (p[t[i][1]], p[t[i][2]]));
	return v / 6.0;
}

void CheckSpec (const VoluteSpec &s)
{
	const auto ok = [] (bool c, const char *name) {
		if (!c)
			Fail (std::string ("forme hors domaine : ") + name);
	};
	ok (std::isfinite (s.scale) && s.scale > 0.0, "scale fini et > 0");
	ok (std::isfinite (s.eyeRadius) && s.eyeRadius > 0.0, "eyeRadius fini et > 0");
	ok (std::isfinite (s.eyeTheta) && std::isfinite (s.eyeHeight) && std::isfinite (s.planeAngle), "oeil et plan finis");
	ok (s.planeAngle > 0.0 && s.planeAngle < kPi && s.planeAngle != 0.5 * kPi, "planeAngle dans ]0, pi[, hors de pi/2");
	for (const double w : s.turnWidths)
		ok (std::isfinite (w) && w > 0.0, "turnWidths finies et > 0");
	ok (std::isfinite (s.minWidth) && s.minWidth > 0.0, "minWidth fini et > 0");
	ok (std::isfinite (s.depth) && s.depth > 0.0, "depth fini et > 0");
	ok (std::isfinite (s.clearance) && s.clearance > 0.0, "clearance fini et > 0");
	ok (std::isfinite (s.beadShare) && std::isfinite (s.grooveShare) && s.beadShare > 0.0 && s.grooveShare > 0.0 &&
	        std::fabs (2.0 * s.beadShare + s.grooveShare - 1.0) <= 1e-12,
	    "bourrelets et rainure finis, 2 beadShare + grooveShare = 1");
	ok (std::isfinite (s.beadHeight) && std::isfinite (s.grooveDepth) && s.beadHeight >= 0.0 && s.grooveDepth >= 0.0 &&
	        s.grooveDepth < 0.5 * s.depth,
	    "reliefs finis et >= 0, rainure < depth / 2");
	ok (std::isfinite (s.eyeExtraDepth) && s.eyeExtraDepth >= 0.0, "eyeExtraDepth fini et >= 0");
	ok (std::isfinite (s.stemTheta) && std::isfinite (s.stemFoot) && std::isfinite (s.stemBend) && s.stemBend > s.stemFoot,
	    "tige finie, stemBend > stemFoot");
	ok (std::isfinite (s.stemTangents) && s.stemTangents > 0.0, "stemTangents fini et > 0");
	ok (std::isfinite (s.stemLift) && s.stemLift >= 0.0, "stemLift fini et >= 0");
	ok (s.stemThin > 0.0 && s.stemThin <= 1.0, "stemThin dans ]0, 1]");
	ok (std::isfinite (s.stemThinLength) && s.stemThinLength > 0.0, "stemThinLength fini et > 0");
	ok (std::isfinite (s.maxTwistRate) && s.maxTwistRate > 0.0, "maxTwistRate fini et > 0");
	ok (s.samplesPerQuarter >= 1 && s.bellSegments >= 1 && s.bendSegments >= 2 && s.beadSegments >= 1 &&
	        s.grooveSegments >= 1 && s.eyeSegments >= 3,
	    "echantillonnage");
	ok (s.grooveSegments % 2 == 0, "grooveSegments pair (station au milieu de la rainure)");
}

// Volute de la tige 0, repere du tour en parties (x, y, h).
VolutePiece BuildStemZero (const CapitalEnvelope &env, const VoluteSpec &spec)
{
	const double k = spec.scale, c = spec.clearance, b = spec.depth;
	const double alpha = 0.5 * kPi - spec.planeAngle;
	const Vec U {std::cos (alpha), std::sin (alpha), 0.0}, Z {0.0, 0.0, 1.0}, N {std::sin (alpha), -std::cos (alpha), 0.0};
	// Face vue tournee vers le spectateur de la face de la tige 0 (axe theta = 0) : +1 si n a une composante x
	// positive, -1 sinon.
	const double face = N[0] > 0.0 ? 1.0 : -1.0;
	const Vec eye {spec.eyeRadius * std::cos (spec.eyeTheta), spec.eyeRadius * std::sin (spec.eyeTheta), spec.eyeHeight};
	const auto inPlane = [&] (double u, double v) { return Add (eye, Add (Mul (u, U), Mul (v, Z))); };

	// Spirale.
	IonicVoluteData data;
	data.eyeDiameter = 2.0 * k;
	data.centreFromTop = 9.0 * k;
	const VoluteSpiral spiral = BuildTwelveCentreVolute (data, VoluteJunction::CentreLine);
	const double eyeRadius = k;
	const auto schedule = [&] (double phi) {
		const std::array<double, 3> &w = spec.turnWidths;
		if (phi <= 2.0 * kPi)
			return w[0] + (w[1] - w[0]) * phi / (2.0 * kPi);
		if (phi <= 4.0 * kPi)
			return w[1] + (w[2] - w[1]) * (phi - 2.0 * kPi) / (2.0 * kPi);
		return w[2];
	};
	std::vector<Station> stations;
	std::vector<Station> spiralStations;
	double phi0 = 0.0, phiLast = 0.0;
	bool stop = false;
	for (std::size_t a = 0; a < spiral.outer.arcs.size () && !stop; ++a)
	{
		const VoluteArc &arc = spiral.outer.arcs[a];
		const double sweep = arc.a0 - arc.a1;
		const int n = std::max (1, static_cast<int> (std::ceil (sweep / (0.5 * kPi) * spec.samplesPerQuarter - 1e-9)));
		for (int i = a == 0 ? 0 : 1; i <= n; ++i)
		{
			const double angle = i == n ? arc.a1 : arc.a0 + (arc.a1 - arc.a0) * i / n;
			const VolutePoint o = arc.At (angle);
			const double mu = (arc.centre.u - o.u) / arc.radius, mv = (arc.centre.v - o.v) / arc.radius;
			const double phi = phi0 + (arc.a0 - angle);
			const double width = std::min (schedule (phi), std::hypot (o.u, o.v) - eyeRadius - c);
			if (width < spec.minWidth)
			{
				stop = true;
				break;
			}
			if (!(width < arc.radius - c))
				Fail ("largeur du ruban >= rayon local - c (" + std::to_string (width) + " contre " + std::to_string (arc.radius) + ")");
			spiralStations.push_back ({inPlane (o.u, o.v), Add (Mul (mu, U), Mul (mv, Z)), Mul (face, N), width});
			phiLast = phi;
		}
		phi0 += sweep;
	}
	if (phiLast < 2.0 * kPi)
		Fail ("oeil plus grand que la derniere spire (moins d'un tour de ruban)");

	// Tige.
	const Station &first = spiralStations.front ();
	const double w0 = first.width;
	const Vec junction = Add (first.outer, Mul (0.5 * w0, first.m));
	const double ct = std::cos (spec.stemTheta), st = std::sin (spec.stemTheta);
	const Vec radial {ct, st, 0.0};
	const double off = c + 0.5 * b + spec.stemLift;
	const auto bellPoint = [&] (double h) { const double r = env.CoreRadius (h) + off; return Vec {r * ct, r * st, h}; };
	const auto bellTangent = [&] (double h) {
		const double slope = (env.CoreRadius (h + 1e-3) - env.CoreRadius (h - 1e-3)) / 2e-3;
		return Unit (Vec {slope * ct, slope * st, 1.0});
	};
	const auto bellFrame = [&] (double h, Vec &t, Vec &m, Vec &n) {
		t = bellTangent (h);
		n = Unit (Sub (radial, Mul (Dot (radial, t), t)));
		m = Mul (face, Cross (t, n));
	};
	const std::vector<std::array<double, 2>> section = Section (spec, w0);
	// Plus bas point de la premiere section a stemFoot.
	double hb = spec.stemFoot;
	for (int it = 0; it < 3; ++it)
	{
		Vec t, m, n;
		bellFrame (hb, t, m, n);
		const Vec centre = bellPoint (hb);
		double low = 1e300;
		for (const std::array<double, 2> &q : section)
			low = std::min (low, Add (centre, Add (Mul ((q[0] - 0.5) * w0, m), Mul (Thinned (q[1], b, spec.stemThin), n)))[2]);
		hb += spec.stemFoot - low;
	}
	if (!(hb < spec.stemBend))
		Fail ("forme hors domaine : stemBend sous la naissance de la tige");
	std::vector<Vec> centres, tangents, ms;
	for (int i = 0; i <= spec.bellSegments; ++i)
	{
		const double h = hb + (spec.stemBend - hb) * i / spec.bellSegments;
		Vec t, m, n;
		bellFrame (h, t, m, n);
		centres.push_back (bellPoint (h));
		tangents.push_back (t);
		ms.push_back (m);
	}
	const Vec p1 = centres.back (), t1 = tangents.back ();
	const double chord = Norm (Sub (junction, p1)) * spec.stemTangents;
	for (int i = 1; i <= spec.bendSegments; ++i)
	{
		const double s = static_cast<double> (i) / spec.bendSegments;
		const double h00 = 2 * s * s * s - 3 * s * s + 1, h10 = s * s * s - 2 * s * s + s, h01 = -2 * s * s * s + 3 * s * s,
		             h11 = s * s * s - s * s;
		centres.push_back (i == spec.bendSegments ? junction
		                                          : Add (Add (Mul (h00, p1), Mul (h10 * chord, t1)), Add (Mul (h01, junction), Mul (h11 * chord, U))));
	}
	const std::size_t nb = static_cast<std::size_t> (spec.bellSegments), nc = centres.size ();
	for (std::size_t i = nb + 1; i < nc; ++i)
		tangents.push_back (i + 1 == nc ? U : Unit (Sub (centres[i + 1], centres[i - 1])));
	ms.resize (nc);
	for (std::size_t i = nb; i + 1 < nc; ++i)
	{
		const Vec v1 = Sub (centres[i + 1], centres[i]);
		const double c1 = Dot (v1, v1);
		const Vec rL = Sub (ms[i], Mul (2.0 / c1 * Dot (v1, ms[i]), v1));
		const Vec tL = Sub (tangents[i], Mul (2.0 / c1 * Dot (v1, tangents[i]), v1));
		const Vec v2 = Sub (tangents[i + 1], tL);
		const double c2 = Dot (v2, v2);
		ms[i + 1] = c2 > 1e-24 ? Sub (rL, Mul (2.0 / c2 * Dot (v2, rL), v2)) : rL;
	}
	const double delta = std::atan2 (Dot (Cross (ms.back (), first.m), U), Dot (ms.back (), first.m));
	double twistRate = 0.0, previous = 0.0;
	double developed = 0.0;
	for (std::size_t i = 0; i + 1 < nc; ++i)
	{
		const double twist = i <= nb ? 0.0 : delta * Smootherstep (static_cast<double> (i - nb) / static_cast<double> (nc - 1 - nb));
		if (i > 0)
			twistRate = std::max (twistRate, std::fabs (twist - previous) / Norm (Sub (centres[i], centres[i - 1])));
		previous = twist;
		const Vec &t = tangents[i];
		const Vec m = Add (Mul (std::cos (twist), ms[i]), Mul (std::sin (twist), Cross (t, ms[i])));
		const Vec n = Mul (face, Cross (m, t));
		if (i > 0)
			developed += Norm (Sub (centres[i], centres[i - 1]));
		const double thickness =
		    spec.stemThin + (1.0 - spec.stemThin) * Smootherstep (std::min (1.0, developed / spec.stemThinLength));
		stations.push_back ({Sub (centres[i], Mul (0.5 * w0, m)), m, n, w0, thickness});
	}
	twistRate = std::max (twistRate, std::fabs (delta - previous) / Norm (Sub (centres[nc - 1], centres[nc - 2])));
	if (spec.stemThin < 1.0 && developed < spec.stemThinLength)
		Fail ("rampe d'epaisseur plus longue que la tige (" + std::to_string (developed) + " < " + std::to_string (spec.stemThinLength) + " p)");
	if (!(twistRate <= spec.maxTwistRate))
		Fail ("torsion de la tige trop rapide (" + std::to_string (twistRate) + " rad/p)");
	// Courbure de la ligne centrale de la tige (cercle par trois stations).
	double beadMax = 0.0;
	for (const std::array<double, 2> &q : section)
		beadMax = std::max (beadMax, q[1]);
	const double halfDiagonal = std::hypot (0.5 * w0, std::max (beadMax, 0.5 * b));
	for (std::size_t i = 1; i + 1 < nc; ++i)
	{
		const Vec a = Sub (centres[i], centres[i - 1]), d = Sub (centres[i + 1], centres[i]), e = Sub (centres[i + 1], centres[i - 1]);
		const double area2 = Norm (Cross (a, d));
		if (area2 <= 0.0)
			continue;
		const double radius = Norm (a) * Norm (d) * Norm (e) / (2.0 * area2);
		if (!(radius >= halfDiagonal + c))
			Fail ("courbure de la tige : rayon " + std::to_string (radius) + " < demi-diagonale + c");
	}
	const unsigned stemCount = static_cast<unsigned> (stations.size ());
	stations.insert (stations.end (), spiralStations.begin (), spiralStations.end ());

	// Coque du ruban.
	VolutePiece piece;
	std::vector<Vec> pts;
	std::vector<std::array<unsigned, 3>> &tri = piece.triangles;
	unsigned nq = 0;
	for (const Station &s : stations)
	{
		const std::vector<std::array<double, 2>> ring = Section (spec, s.width);
		nq = static_cast<unsigned> (ring.size ());
		for (const std::array<double, 2> &q : ring)
			pts.push_back (Add (s.outer, Add (Mul (q[0] * s.width, s.m), Mul (Thinned (q[1], b, s.thickness), s.n))));
	}
	const unsigned ns = static_cast<unsigned> (stations.size ());
	for (unsigned j = 0; j + 1 < ns; ++j)
		for (unsigned i = 0; i < nq; ++i)
		{
			const unsigned a = j * nq + i, bq = j * nq + (i + 1) % nq, cq = (j + 1) * nq + (i + 1) % nq, d = (j + 1) * nq + i;
			tri.push_back ({a, bq, cq});
			tri.push_back ({a, cq, d});
		}
	// Bouchons : la face vue est monotone en s, au-dessus du dos ; chaque moitie est vue de son coin du dos.
	const unsigned mid = static_cast<unsigned> (spec.beadSegments + spec.grooveSegments / 2), front = nq - 2;
	for (const unsigned j : {0u, ns - 1})
	{
		const unsigned base = j * nq, back1 = base + nq - 2, back0 = base + nq - 1;
		for (unsigned i = 0; i < mid; ++i)
			tri.push_back ({back0, base + i, base + i + 1});
		for (unsigned i = mid; i + 1 < front; ++i)
			tri.push_back ({back1, base + i, base + i + 1});
		tri.push_back ({back0, base + mid, back1});
	}
	// Orientation : les deux bouchons doivent etre opposes ; le premier est retourne.
	const std::size_t capFirst = 2u * (ns - 1) * nq;
	for (std::size_t i = capFirst; i < capFirst + (nq - 2); ++i)
		std::swap (tri[i][1], tri[i][2]);
	if (SignedVolume (pts, tri, 0, tri.size ()) < 0.0)
		for (std::array<unsigned, 3> &t : tri)
			std::swap (t[1], t[2]);
	piece.ribbonVertices = static_cast<unsigned> (pts.size ());
	piece.ribbonTriangles = static_cast<unsigned> (tri.size ());

	// Oeil.
	const unsigned ne = static_cast<unsigned> (spec.eyeSegments), e0 = static_cast<unsigned> (pts.size ());
	const double halfEye = 0.5 * (b + spec.eyeExtraDepth);
	for (const double side : {1.0, -1.0})
		for (unsigned i = 0; i < ne; ++i)
		{
			const double a = 2.0 * kPi * i / ne;
			pts.push_back (Add (inPlane (eyeRadius * std::cos (a), eyeRadius * std::sin (a)), Mul (side * halfEye, N)));
		}
	const std::size_t eyeFirst = tri.size ();
	for (unsigned i = 0; i < ne; ++i)
	{
		const unsigned a = e0 + i, bq = e0 + (i + 1) % ne, cq = e0 + ne + (i + 1) % ne, d = e0 + ne + i;
		tri.push_back ({a, cq, bq});
		tri.push_back ({a, d, cq});
	}
	for (unsigned i = 1; i + 1 < ne; ++i)
	{
		tri.push_back ({e0, e0 + i, e0 + i + 1});
		tri.push_back ({e0 + ne, e0 + ne + i + 1, e0 + ne + i});
	}
	if (SignedVolume (pts, tri, eyeFirst, tri.size ()) < 0.0)
		for (std::size_t i = eyeFirst; i < tri.size (); ++i)
			std::swap (tri[i][1], tri[i][2]);

	// Gardes sur tous les sommets, une categorie apres l'autre : abaque, cone, abaque en plan, corbeille.
	const double top = env.BandTop () - c;
	for (const Vec &p : pts)
		if (p[2] > top)
			Fail ("sommet au-dessus de l'abaque - c (h " + std::to_string (p[2]) + ")");
	for (const Vec &p : pts)
		if (!(std::hypot (p[0], p[1]) < env.ConeRadius (p[2])))
			Fail ("sommet hors du cone (r " + std::to_string (std::hypot (p[0], p[1])) + " a h " + std::to_string (p[2]) + ")");
	if (spec.planBound)
		for (const Vec &p : pts)
		{
			const double theta = std::atan2 (p[1], p[0]);
			if (!(std::hypot (p[0], p[1]) <= env.abacusOutline.RadiusAt (theta) - c))
				Fail ("sommet hors de l'abaque en plan (r " + std::to_string (std::hypot (p[0], p[1])) + " a theta " +
				      std::to_string (theta * 180.0 / kPi) + " degres)");
		}
	for (const Vec &p : pts)
		if (std::hypot (p[0], p[1]) < env.CoreRadius (p[2]) + c - 1e-9)
			Fail ("sommet dans la corbeille (r " + std::to_string (std::hypot (p[0], p[1])) + " a h " + std::to_string (p[2]) + ")");
	piece.positions = std::move (pts);
	piece.stemStations = stemCount;
	piece.spiralStations = ns - stemCount;
	piece.sectionPoints = nq;
	piece.turns = phiLast / (2.0 * kPi);
	piece.eye = eye;
	return piece;
}

} // namespace

VoluteSpec DefaultVoluteSpec (OrnamentDetail detail)
{
	VoluteSpec s;
	s.eyeRadius = 27.2;
	s.eyeTheta = 40.0 * kDeg;
	s.planeAngle = 22.5 * kDeg;
	s.planBound = true;
	s.stemTheta = 28.5 * kDeg;
	s.stemThin = 0.8;
	s.stemThinLength = 1.0;
	if (detail == OrnamentDetail::Heavy)
	{
		s.samplesPerQuarter = 8;
		s.bellSegments = 8;
		s.bendSegments = 20;
		s.beadSegments = 4;
		s.grooveSegments = 4;
		s.eyeSegments = 32;
	}
	return s;
}

VoluteSpec DefaultHelixSpec (OrnamentDetail detail)
{
	VoluteSpec s = DefaultVoluteSpec (detail);
	s.scale = 0.36;
	const double x = 21.5, y = 3.45;
	s.eyeRadius = std::hypot (x, y);
	s.eyeTheta = std::atan2 (y, x);
	s.eyeHeight = 30.05;
	s.planeAngle = 135.0 * kDeg;
	s.turnWidths = {0.90, 0.44, 0.38};
	s.stemTheta = 12.0 * kDeg;
	s.stemFoot = 25.5;
	s.stemBend = 27.0;
	s.stemTangents = 0.8;
	s.stemLift = 0.03;
	s.stemThin = 1.0;
	s.planBound = false;
	return s;
}

VolutePiece BuildVoluteRibbon (const CapitalEnvelope &env, const VoluteSpec &spec, int stem)
{
	if (stem != 0 && stem != 1)
		Fail ("tige " + std::to_string (stem) + " hors de {0, 1}");
	CheckSpec (spec);
	VolutePiece piece = BuildStemZero (env, spec);
	if (stem == 1)
	{
		for (Vec &p : piece.positions)
			std::swap (p[0], p[1]);
		for (std::array<unsigned, 3> &t : piece.triangles)
			std::swap (t[1], t[2]);
		std::swap (piece.eye[0], piece.eye[1]);
	}
	const double s = env.scale;
	for (Vec &p : piece.positions)
		p = {p[0] * s, p[1] * s, (env.foot + p[2]) * s};
	return piece;
}

} // namespace cgarchi::classical::detail
