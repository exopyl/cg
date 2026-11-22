#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cfloat>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/classical_volute.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Volutes (C : volute d'angle) dans l'ornement : K29 a K44 pour les 8 V
// ===========================================================================
//
// Pieces 40 a 47 : tige k croissante (registre AC-46) ; les 8 helices suivent (48 a 55). Cotes ecrites d'apres le
// relevé des pl. XXV et XXVI (registre AC-59 a AC-63 ; pose de l'oeil AC-81), non lues dans le produit : spirale de Vignole a 12 centres x 0,470 ; oeil a R 27,2, a 5 degres de la
// corne du cote de sa tige, h 31,5 ; plan vertical a 22,5 degres du plan de la face ; largeur du ruban au sommet des
// tours 1,43 / 0,70 / 0,60 p (+-0,3), bornee par l'oeil ; profondeur 1,0 ; tige nee a h 24,5, cachee derriere le D
// exterieur.

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kTau = 1e-4;
constexpr double kC = 0.05;
constexpr std::size_t kFirstVolute = 40;
constexpr std::size_t kVolutes = 8;
using Point = std::array<double, 3>;

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

struct Case
{
	OrnamentDetail detail;
	double scale;
	detail::CapitalEnvelope env;
	CapitalOrnament ornament;
};

const std::vector<Case> &Cases ()
{
	static const std::vector<Case> cases = [] {
		std::vector<Case> out;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		ColumnOptions options;
		options.order = Order::Corinthian;
		const ColumnProfile profile = BuildColumnProfile (order, options);
		for (const OrnamentDetail detail : {OrnamentDetail::Light, OrnamentDetail::Heavy})
			for (const double scale : {1.0, 2.0})
			{
				AcanthusOptions o;
				o.detail = detail;
				o.scale = scale;
				out.push_back ({detail, scale, detail::ReadCapitalEnvelope (order, profile, scale), BuildCorinthianOrnament (order, profile, o)});
			}
		return out;
	}();
	return cases;
}

#define CASE_TRACE(c) SCOPED_TRACE (std::string (LevelName ((c).detail)) + ", echelle " + std::to_string ((c).scale))

Point Parts (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned v)
{
	float p[3] {};
	mesh.GetVertex (v, p);
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

Point PartsOf (const std::array<double, 3> &p, const detail::CapitalEnvelope &env)
{
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

double Corner (std::size_t k)
{
	return kPi / 4.0 + 0.5 * kPi * static_cast<double> (k / 2);
}

// Sens de la tige k par rapport a sa corne : -1 si la tige est avant la corne (theta croissants), +1 sinon.
double Side (std::size_t k)
{
	return k % 2 == 0 ? -1.0 : 1.0;
}

::testing::AssertionResult HasVolutes (const Case &c)
{
	if (c.ornament.pieces.size () != kFirstVolute + 2 * kVolutes + detail::kFleurons)
		return ::testing::AssertionFailure () << c.ornament.pieces.size () << " pieces, 60 attendues";
	for (std::size_t i = kFirstVolute; i < kFirstVolute + kVolutes; ++i)
		if (c.ornament.pieces[i].kind != CapitalPieceKind::Volute)
			return ::testing::AssertionFailure () << "piece " << i << " : pas une volute";
	return ::testing::AssertionSuccess ();
}

// Centre et axe de l'oeil d'une piece : les 2 n_e derniers sommets (anneau avant, puis arriere).
void Eye (const Case &c, std::size_t i, unsigned ne, Point &centre, Point &axis)
{
	const CapitalPiece &p = c.ornament.pieces[i];
	ASSERT_GE (p.vertexEnd - p.vertexBegin, 2 * ne);
	const unsigned e0 = p.vertexEnd - 2 * ne;
	Point front {}, back {};
	for (unsigned j = 0; j < ne; ++j)
	{
		const Point a = Parts (*c.ornament.mesh, c.env, e0 + j), b = Parts (*c.ornament.mesh, c.env, e0 + ne + j);
		for (int q = 0; q < 3; ++q)
		{
			front[q] += a[q] / ne;
			back[q] += b[q] / ne;
		}
	}
	for (int q = 0; q < 3; ++q)
		centre[q] = 0.5 * (front[q] + back[q]);
	const double len = std::hypot (front[0] - back[0], front[1] - back[1], front[2] - back[2]);
	axis = {(front[0] - back[0]) / len, (front[1] - back[1]) / len, (front[2] - back[2]) / len};
}

void ExpectInvalid (const std::function<void ()> &call, const std::string &fragment)
{
	try
	{
		call ();
		ADD_FAILURE () << "aucune exception, attendu : " << fragment;
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find (fragment), std::string::npos) << e.what ();
	}
}

// Repere du plan de la representante de la tige 0 (corne pi/4) : u vers la corne, v vers le haut.
struct SpiralPlane
{
	Point eye, u, n;
	double U (const Point &p) const { return (p[0] - eye[0]) * u[0] + (p[1] - eye[1]) * u[1]; }
	double V (const Point &p) const { return p[2] - eye[2]; }
};

SpiralPlane PlaneZero ()
{
	const double a = 67.5 * kDeg;
	return {{27.2 * std::cos (40.0 * kDeg), 27.2 * std::sin (40.0 * kDeg), 31.5}, {std::cos (a), std::sin (a), 0.0}, {std::sin (a), -std::cos (a), 0.0}};
}

} // namespace

// K29 : comptes. Ruban : stations n_s (tige puis spirale), n_q points par section : 2 (n_s - 1) n_q + 2 (n_q - 2)
// triangles, n_s n_q sommets ; oeil : 4 n_e - 4 triangles, 2 n_e sommets. Comptes attendus : Light 14 + 34 stations,
// n_q 9, n_e 12 ; Heavy 28 + 88, 15, 32. Ornement : 8 F, 8 E, 8 C, 16 D, puis 8 V.
TEST (TEST_cgarchi_classical_acanthus_volute, the_volutes_have_their_counts_and_their_place_in_the_ornament)
{
	struct Expected
	{
		OrnamentDetail detail;
		unsigned stem, spiral, nq, ne, faces, vertices;
	};
	for (const Expected &e : {Expected {OrnamentDetail::Light, 14, 34, 9, 12, 904, 456}, Expected {OrnamentDetail::Heavy, 28, 88, 15, 32, 3600, 1804}})
	{
		SCOPED_TRACE (LevelName (e.detail));
		const Case &c = Cases ()[e.detail == OrnamentDetail::Light ? 0 : 2];
		for (const int stem : {0, 1})
		{
			const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultVoluteSpec (e.detail), stem);
			EXPECT_EQ (v.stemStations, e.stem);
			EXPECT_EQ (v.spiralStations, e.spiral);
			EXPECT_EQ (v.sectionPoints, e.nq);
			const unsigned ns = v.stemStations + v.spiralStations;
			EXPECT_EQ (v.ribbonTriangles, 2 * (ns - 1) * v.sectionPoints + 2 * (v.sectionPoints - 2));
			EXPECT_EQ (v.ribbonVertices, ns * v.sectionPoints);
			EXPECT_EQ (v.triangles.size () - v.ribbonTriangles, 4u * e.ne - 4u);
			EXPECT_EQ (v.positions.size () - v.ribbonVertices, 2u * e.ne);
			EXPECT_EQ (v.triangles.size (), e.faces);
			EXPECT_EQ (v.positions.size (), e.vertices);
			// deux coques : ruban et oeil
			MeshValidationOptions o;
			o.expectedComponents = 2u;
			const MeshValidationReport r = ValidateMesh (*MakeTriangleMesh (v.positions, v.triangles), o);
			EXPECT_TRUE (r.IsValid ());
			EXPECT_EQ (r.components, 2u);
		}
		ASSERT_TRUE (HasVolutes (c));
		for (std::size_t i = kFirstVolute; i < kFirstVolute + kVolutes; ++i)
		{
			EXPECT_EQ (c.ornament.pieces[i].faceEnd - c.ornament.pieces[i].faceBegin, e.faces) << i;
			EXPECT_EQ (c.ornament.pieces[i].vertexEnd - c.ornament.pieces[i].vertexBegin, e.vertices) << i;
		}
	}
}

// K30, K39 : oeil de chaque V a R 27,2, a 5 degres de sa corne du cote de sa tige, h 31,5 (pose C1, registre AC-81) ; axe de l'oeil (normale du
// plan de la spirale) a 22,5 degres de la normale de la face de sa tige. Sommets float : 1e-5 p, 1e-4 rad.
TEST (TEST_cgarchi_classical_acanthus_volute, each_volute_eye_and_plane_stand_on_the_plate_values)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		const unsigned ne = c.detail == OrnamentDetail::Light ? 12u : 32u;
		double worstR = 0.0, worstT = 0.0, worstH = 0.0, worstA = 0.0;
		for (std::size_t k = 0; k < kVolutes; ++k)
		{
			Point centre {}, axis {};
			Eye (c, kFirstVolute + k, ne, centre, axis);
			const double theta = std::atan2 (centre[1], centre[0]);
			worstR = std::max (worstR, std::fabs (std::hypot (centre[0], centre[1]) - 27.2));
			worstT = std::max (worstT, std::fabs (std::remainder (theta - (Corner (k) + Side (k) * 5.0 * kDeg), 2.0 * kPi)));
			worstH = std::max (worstH, std::fabs (centre[2] - 31.5));
			const double faceNormal = Corner (k) + Side (k) * kPi / 4.0;
			const double cosA = std::fabs (axis[0] * std::cos (faceNormal) + axis[1] * std::sin (faceNormal));
			worstA = std::max (worstA, std::fabs (std::acos (std::min (1.0, cosA)) - 22.5 * kDeg));
		}
		EXPECT_LE (worstR, 1e-5);
		EXPECT_LE (worstT, 1e-5);
		EXPECT_LE (worstH, 1e-5);
		EXPECT_LE (worstA, 1e-4);
		std::printf ("%s, echelle %g : K30 oeil : R %.1e, theta %.1e rad, h %.1e p ; plan %.1e rad\n", LevelName (c.detail), c.scale,
		             worstR, worstT, worstH, worstA);
	}
}

// K31 : chaque V est la representante de sa parite de tige tournee de pi/2 (k / 2), a 4 ulp float pres ; les deux
// premieres sont la construction elle-meme, a l'octet apres conversion en float.
TEST (TEST_cgarchi_classical_acanthus_volute, each_volute_copy_is_the_volute_built_at_its_place)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		double worst = 0.0;
		bool exact = true;
		for (std::size_t k = 0; k < kVolutes; ++k)
		{
			const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultVoluteSpec (c.detail), static_cast<int> (k % 2));
			const CapitalPiece &piece = c.ornament.pieces[kFirstVolute + k];
			ASSERT_EQ (piece.vertexEnd - piece.vertexBegin, v.positions.size ());
			const double a = 0.5 * kPi * static_cast<double> (k / 2), ca = std::cos (a), sa = std::sin (a);
			for (std::size_t j = 0; j < v.positions.size (); ++j)
			{
				float q[3] {};
				c.ornament.mesh->GetVertex (piece.vertexBegin + static_cast<unsigned> (j), q);
				const std::array<double, 3> &p = v.positions[j];
				const double x = k < 2 ? p[0] : ca * p[0] - sa * p[1], y = k < 2 ? p[1] : sa * p[0] + ca * p[1];
				if (k < 2)
					exact = exact && q[0] == static_cast<float> (x) && q[1] == static_cast<float> (y) && q[2] == static_cast<float> (p[2]);
				worst = std::max (worst, std::hypot (q[0] - x, q[1] - y, q[2] - p[2]) / c.env.scale);
			}
		}
		EXPECT_TRUE (exact);
		// 4 ulp float a la plus grande coordonnee du tour (z = (pied + 36) echelle), ramenes en parties
		EXPECT_LE (worst, 4.0 * std::numeric_limits<float>::epsilon () * (c.env.foot + 36.0));
		std::printf ("%s, echelle %g : K31 copie / construction des V %.2e p\n", LevelName (c.detail), c.scale, worst);
	}
}

// K32 : les 8 V ont la symetrie D4 de l'abaque (reflexions d'axes 0 et pi/4, rotation de pi/2), sans les indices ; pas
// celle de la rotation de pi/4 (controle negatif, mesure exhaustive sur les oeils).
TEST (TEST_cgarchi_classical_acanthus_volute, the_volutes_have_the_symmetry_of_the_abacus)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		std::vector<Point> pts;
		const CapitalPiece &first = c.ornament.pieces[kFirstVolute];
		for (unsigned v = first.vertexBegin; v < c.ornament.pieces[kFirstVolute + kVolutes - 1].vertexEnd; ++v)
			pts.push_back (Parts (*c.ornament.mesh, c.env, v));
		const auto nearest = [&] (const Point &q) {
			double best = std::numeric_limits<double>::infinity ();
			for (const Point &p : pts)
				best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
			return best;
		};
		double m0 = 0.0, m45 = 0.0, quarter = 0.0, eighth = 0.0;
		const unsigned ne = c.detail == OrnamentDetail::Light ? 12u : 32u;
		for (std::size_t i = 0; i < pts.size (); i += c.detail == OrnamentDetail::Light ? 1 : 7)
		{
			const Point &p = pts[i];
			m0 = std::max (m0, nearest ({p[0], -p[1], p[2]}));
			m45 = std::max (m45, nearest ({p[1], p[0], p[2]}));
			quarter = std::max (quarter, nearest ({-p[1], p[0], p[2]}));
		}
		const double cr = std::cos (kPi / 4.0), sr = std::sin (kPi / 4.0);
		for (std::size_t k = 0; k < kVolutes; ++k)
		{
			Point centre {}, axis {};
			Eye (c, kFirstVolute + k, ne, centre, axis);
			eighth = std::max (eighth, nearest ({cr * centre[0] - sr * centre[1], sr * centre[0] + cr * centre[1], centre[2]}));
		}
		EXPECT_LE (m0, 1e-5);
		EXPECT_LE (m45, 1e-5);
		EXPECT_LE (quarter, 1e-5);
		EXPECT_GT (eighth, 1.0);
		std::printf ("%s, echelle %g : K32 D4 des V : %.2e / %.2e / %.2e p ; pi/4 %.3f p (negatif)\n", LevelName (c.detail), c.scale, m0,
		             m45, quarter, eighth);
	}
}

// K33 : spirale G1 (MaxTangentBreak <= 1e-9 rad pour les cotes a l'echelle) ; tige G1 a sa jonction avec la spirale :
// a echantillonnage fin, l'angle de la ligne des centres de section a la jonction est du meme ordre que chez ses
// voisines (pas de cassure).
TEST (TEST_cgarchi_classical_acanthus_volute, the_spiral_and_its_stem_are_smooth)
{
	IonicVoluteData data;
	data.eyeDiameter = 2.0 * 0.470;
	data.centreFromTop = 9.0 * 0.470;
	EXPECT_LE (MaxTangentBreak (BuildTwelveCentreVolute (data, VoluteJunction::CentreLine).outer), 1e-9);

	const Case &c = Cases ()[0];
	detail::VoluteSpec spec = detail::DefaultVoluteSpec (OrnamentDetail::Light);
	spec.samplesPerQuarter = 64;
	spec.bendSegments = 400;
	const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, spec, 0);
	const unsigned nq = v.sectionPoints;
	const auto centre = [&] (unsigned station) {
		Point m {};
		for (unsigned i = 0; i < nq; ++i)
			for (int q = 0; q < 3; ++q)
				m[q] += PartsOf (v.positions[station * nq + i], c.env)[q] / nq;
		return m;
	};
	const auto turning = [&] (unsigned j) {
		const Point a = centre (j - 1), b = centre (j), d = centre (j + 1);
		const Point u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {d[0] - b[0], d[1] - b[1], d[2] - b[2]};
		const double cosA = (u[0] * w[0] + u[1] * w[1] + u[2] * w[2]) / (std::hypot (u[0], u[1], u[2]) * std::hypot (w[0], w[1], w[2]));
		return std::acos (std::clamp (cosA, -1.0, 1.0));
	};
	ASSERT_GE (v.stemStations, 2u);
	ASSERT_GE (v.spiralStations, 3u);
	ASSERT_GE (v.positions.size (), static_cast<std::size_t> (v.stemStations + v.spiralStations) * nq);
	const unsigned j = v.stemStations;
	const double atJunction = turning (j), before = turning (j - 1), after = turning (j + 1);
	EXPECT_LE (atJunction, std::max (before, after) + 0.5 * kDeg);
	// Spirale du ruban G1, raccords entre arcs compris : le bord exterieur tourne a chaque station de l'angle d'un pas
	// d'arc ; au raccord de deux arcs, de la demi-somme des pas voisins, donc entre les angles des deux stations
	// voisines. Une cassure (G0) s'y ajoute : depassement de l'angle d'une station hors de l'intervalle de ses voisines.
	const auto outerAt = [&] (unsigned station) { return PartsOf (v.positions[station * nq], c.env); };
	std::vector<double> turn;
	for (unsigned s = v.stemStations + 1; s + 1 < v.stemStations + v.spiralStations; ++s)
	{
		const Point a = outerAt (s - 1), b = outerAt (s), d = outerAt (s + 1);
		const Point u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {d[0] - b[0], d[1] - b[1], d[2] - b[2]};
		const double cosA = (u[0] * w[0] + u[1] * w[1] + u[2] * w[2]) / (std::hypot (u[0], u[1], u[2]) * std::hypot (w[0], w[1], w[2]));
		turn.push_back (std::acos (std::clamp (cosA, -1.0, 1.0)));
	}
	ASSERT_GE (turn.size (), 3u);
	double spiralBreak = 0.0;
	for (std::size_t s = 1; s + 1 < turn.size (); ++s)
		spiralBreak = std::max ({spiralBreak, turn[s] - std::max (turn[s - 1], turn[s + 1]), std::min (turn[s - 1], turn[s + 1]) - turn[s]});
	EXPECT_LE (spiralBreak, 1e-6);
	std::printf ("K33 jonction tige / spirale : %.3f deg (voisines %.3f / %.3f) ; spirale du ruban : cassure %.1e rad\n", atJunction / kDeg,
	             before / kDeg, after / kDeg, spiralBreak);
}

// K34, K35 : chaque V hors de la corbeille (jeu >= c), sous l'abaque (h <= 36 - c), dans le cone (garde stricte,
// EnvelopeExcess par piece < 0) et, en plan, sous l'arete superieure de l'abaque construit moins c (registre AC-2, sommets
// float : 1e-4 p) ; ecart a l'abaque des donnees (B) journalise.
TEST (TEST_cgarchi_classical_acanthus_volute, the_volutes_keep_off_the_bell_under_the_abacus_and_in_the_cone)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		double clearance = std::numeric_limits<double>::infinity (), cone = -clearance, abacus = -clearance, top = -clearance;
		double plan = -clearance;
		for (std::size_t i = kFirstVolute; i < kFirstVolute + kVolutes; ++i)
		{
			const CapitalPiece &p = c.ornament.pieces[i];
			clearance = std::min (clearance, detail::CoreClearance (*c.ornament.mesh, c.env, p));
			cone = std::max (cone, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p));
			abacus = std::max (abacus, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p.vertexBegin, p.vertexEnd));
			for (unsigned v = p.vertexBegin; v < p.vertexEnd; ++v)
			{
				const Point q = Parts (*c.ornament.mesh, c.env, v);
				top = std::max (top, q[2]);
				plan = std::max (plan, std::hypot (q[0], q[1]) - c.env.abacusOutline.RadiusAt (std::atan2 (q[1], q[0])));
			}
		}
		EXPECT_GE (clearance, kC - kTau);
		EXPECT_LT (cone, 0.0);
		EXPECT_LE (top, 36.0 - kC + kTau);
		EXPECT_LE (plan, -kC + kTau);
		std::printf ("%s, echelle %g : K34 jeu au noyau %.4f p, h max %.4f ; K35 cone %.4f p, arete superieure de l'abaque %+.4f p, "
		             "abaque des donnees (journal) %+.4f p\n",
		             LevelName (c.detail), c.scale, clearance, top, cone, plan, abacus);
	}
}

// K36 : ordre radial. La tige de chaque V passe derriere le D exterieur de sa tige de caulicole (aux noeuds (theta, h)
// couverts par les deux : rayon minimal du D - c - rayon maximal de la V >= 0) ; jour de la paire sous la corne, et
// jour entre chaque V et les F, E, C, D, >= c. Les jours sont mesures sur la volute 0 seule ; les autres s'en deduisent
// par la symetrie D4 (K32).
TEST (TEST_cgarchi_classical_acanthus_volute, each_stem_passes_behind_its_outer_minor_leaf_and_the_volutes_keep_their_distances)
{
	using acanthus_test::RadiusGrid;
	for (const Case &c : Cases ())
	{
		if (c.scale != 1.0)
			continue;
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		const Mesh &mesh = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		double behind = std::numeric_limits<double>::infinity ();
		unsigned common = 0;
		for (std::size_t k = 0; k < kVolutes; ++k)
		{
			const CapitalPiece &outerD = P[24 + 2 * k + 1];
			ASSERT_EQ (outerD.kind, CapitalPieceKind::MinorLeaf);
			const double stemAxis = kPi / 8.0 + k * kPi / 4.0;
			RadiusGrid vOut, dIn;
			acanthus_test::Rasterize (vOut, mesh, c.env, P[kFirstVolute + k].faceBegin, P[kFirstVolute + k].faceEnd, true, stemAxis);
			acanthus_test::Rasterize (dIn, mesh, c.env, outerD.faceBegin, outerD.faceEnd, false, stemAxis);
			for (int j = 0; j < RadiusGrid::kNH; ++j)
				for (int ii = 0; ii < RadiusGrid::kNTheta; ++ii)
					if (!std::isnan (vOut.At (ii, j)) && !std::isnan (dIn.At (ii, j)))
					{
						++common;
						behind = std::min (behind, dIn.At (ii, j) - kC - vOut.At (ii, j));
					}
		}
		EXPECT_GT (common, 0u);
		EXPECT_GE (behind, 0.0);
		// jours : sommets contre triangles, dans les deux sens, a la paire et aux pieces voisines (boite elargie de 5 et 1 p)
		const double pair = acanthus_test::PieceGap (mesh, c.env, P[kFirstVolute], P[kFirstVolute + 1], 5.0);
		double others = std::numeric_limits<double>::infinity ();
		std::size_t closest = 0;
		Point where {}, at {};
		for (std::size_t i = 0; i < kFirstVolute; ++i)
		{
			const double d = acanthus_test::PieceGap (mesh, c.env, P[kFirstVolute], P[i], 1.0, &at);
			if (d < others)
			{
				others = d;
				closest = i;
				where = at;
			}
		}
		EXPECT_GE (pair, kC);
		EXPECT_GE (others, kC) << "piece " << closest << ", sommet r " << std::hypot (where[0], where[1]) << " theta " << std::atan2 (where[1], where[0]) / kDeg << " deg h " << where[2];
		std::printf ("%s : K36 tige derriere le D e. %.4f p (%u noeuds) ; paire sous la corne %.4f p ; V / autres pieces %.4f p (piece %zu, sommet r %.3f theta %.2f deg h %.3f ; sommets contre triangles)\n",
		             LevelName (c.detail), behind, common, pair, others, closest, std::hypot (where[0], where[1]), std::atan2 (where[1], where[0]) / kDeg, where[2]);
	}
}

// K39 (couverture du pied) : vu radialement, part des noeuds (theta, h) de la tige de chaque V entre h 24,5 et 24,6
// devant lesquels passe une piece F, E, C ou D (rayon minimal de la piece > rayon maximal de la V). La gravure ne montre
// la tige qu'au-dessus de h 24,5 ; le pied n'est cache qu'en partie : borne 0,63, part mesuree 0,649. Parts a h 24,75
// et 25,0 journalisees.
TEST (TEST_cgarchi_classical_acanthus_volute, the_stem_foot_is_partly_hidden_behind_the_leaves)
{
	using acanthus_test::RadiusGrid;
	for (const Case &c : Cases ())
	{
		if (c.scale != 1.0)
			continue;
		CASE_TRACE (c);
		ASSERT_TRUE (HasVolutes (c));
		const Mesh &mesh = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		const auto rowOf = [] (double h) { return static_cast<int> (std::lround (h / RadiusGrid::kDH)); };
		std::array<unsigned, 3> seen {}, hidden {};
		const std::array<std::array<int, 2>, 3> bands {{{rowOf (24.5), rowOf (24.6)}, {rowOf (24.75), rowOf (24.75)}, {rowOf (25.0), rowOf (25.0)}}};
		for (std::size_t k = 0; k < kVolutes; ++k)
		{
			const double stemAxis = kPi / 8.0 + static_cast<double> (k) * kPi / 4.0;
			RadiusGrid volute, front;
			acanthus_test::Rasterize (volute, mesh, c.env, P[kFirstVolute + k].faceBegin, P[kFirstVolute + k].faceEnd, true, stemAxis);
			for (std::size_t i = 0; i < kFirstVolute; ++i)
				acanthus_test::Rasterize (front, mesh, c.env, P[i].faceBegin, P[i].faceEnd, false, stemAxis);
			for (std::size_t b = 0; b < bands.size (); ++b)
				for (int j = bands[b][0]; j <= bands[b][1]; ++j)
					for (int i = 0; i < RadiusGrid::kNTheta; ++i)
						if (!std::isnan (volute.At (i, j)))
						{
							++seen[b];
							if (!std::isnan (front.At (i, j)) && front.At (i, j) > volute.At (i, j))
								++hidden[b];
						}
		}
		ASSERT_GT (seen[0], 0u);
		const auto share = [&] (std::size_t b) { return seen[b] ? static_cast<double> (hidden[b]) / seen[b] : 0.0; };
		EXPECT_GE (share (0), 0.63);
		std::printf ("%s : K39 pied cache h 24,5-24,6 : %.3f (%u / %u noeuds) ; h 24,75 : %.3f (%u) ; h 25,0 : %.3f (%u)\n", LevelName (c.detail),
		             share (0), hidden[0], seen[0], share (1), seen[1], share (2), seen[2]);
	}
}

// K39 : diametres de la spirale dans son plan (H 6,60 +- 1, V 7,50 +- 1, contour exterieur), raccourci vu de face
// cos 22,5 (0,92 ; releve 1,0 +- 0,1) ; tige : plus bas sommet a h 24,5 (+0,05), cachee en dessous.
TEST (TEST_cgarchi_classical_acanthus_volute, the_volute_meets_the_plate_dimensions)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultVoluteSpec (c.detail), 0);
		const SpiralPlane plane = PlaneZero ();
		double uLo = 1e9, uHi = -1e9, vLo = 1e9, vHi = -1e9, yLo = 1e9, yHi = -1e9, low = 1e9;
		ASSERT_LE (v.ribbonVertices, v.positions.size ());
		for (std::size_t j = 0; j < v.ribbonVertices; ++j)
		{
			const Point p = PartsOf (v.positions[j], c.env);
			low = std::min (low, p[2]);
			if (j < static_cast<std::size_t> (v.stemStations) * v.sectionPoints)
				continue;
			uLo = std::min (uLo, plane.U (p));
			uHi = std::max (uHi, plane.U (p));
			vLo = std::min (vLo, plane.V (p));
			vHi = std::max (vHi, plane.V (p));
			yLo = std::min (yLo, p[1]);
			yHi = std::max (yHi, p[1]);
		}
		const double H = uHi - uLo, V = vHi - vLo, front = (yHi - yLo) / H;
		EXPECT_NEAR (H, 6.60, 1.0);
		EXPECT_NEAR (V, 7.50, 1.0);
		EXPECT_GE (front, 0.9);
		EXPECT_LE (front, 1.1);
		EXPECT_GE (low, 24.5 - kTau);
		EXPECT_LE (low, 24.55);
		std::printf ("%s, echelle %g : K39 spirale H %.3f, V %.3f p ; vue de face %.3f ; tige des h %.3f\n", LevelName (c.detail), c.scale, H,
		             V, (yHi - yLo) / H, low);
	}
}

// K40 : le prefixe F + E + C + D de l'ornement (56 256 / 243 008 premieres faces, 28 208 / 121 584 premiers sommets)
// a ces empreintes. Topologie (comptes, arite et indices de chaque face) a l'egalite ; sommets : somme ponderee de
// i |v_i|^2 a 1e-8 pres (relatif) et boite a 4 ulp float ; avec MSVC, sommets float a l'octet (FNV-1a ; un autre
// compilateur ou une autre bibliotheque mathematique peut en changer un dernier bit).
TEST (TEST_cgarchi_classical_acanthus_volute, the_leaves_and_stems_are_unchanged)
{
	struct Expected
	{
		OrnamentDetail detail;
		double scale;
		unsigned vertices, faces;
		std::uint64_t topology, bits;
		double weighted;
		std::array<double, 3> lo, hi;
	};
	const Expected expected[] = {{OrnamentDetail::Light, 1.0, 28208, 56256, 0x6b4b3be47707818eULL, 0x981e4622b83382c4ULL, 45593370587240.438, {-26.9386864, -26.9386864, 318.049988}, {26.9386864, 26.9386864, 345.824646}},
	                             {OrnamentDetail::Light, 2.0, 28208, 56256, 0x6b4b3be47707818eULL, 0x84b33c4dec0a4b0dULL, 182373482348961.75, {-53.8773727, -53.8773727, 636.099976}, {53.8773727, 53.8773727, 691.649292}},
	                             {OrnamentDetail::Heavy, 1.0, 121584, 243008, 0x45297b670fea13b5ULL, 0x054c71da57a9602cULL, 836756141614218.5, {-26.99617, -26.99617, 318.049988}, {26.99617, 26.99617, 345.887268}},
	                             {OrnamentDetail::Heavy, 2.0, 121584, 243008, 0x45297b670fea13b5ULL, 0x7d666a0d4e11dc49ULL, 3347024566456874.0, {-53.9923401, -53.9923401, 636.099976}, {53.9923401, 53.9923401, 691.774536}}};
	for (const Expected &e : expected)
	{
		const Case *c = nullptr;
		for (const Case &k : Cases ())
			if (k.detail == e.detail && k.scale == e.scale)
				c = &k;
		ASSERT_NE (c, nullptr);
		CASE_TRACE (*c);
		ASSERT_TRUE (HasVolutes (*c));
		EXPECT_EQ (c->ornament.pieces[kFirstVolute].vertexBegin, e.vertices);
		EXPECT_EQ (c->ornament.pieces[kFirstVolute].faceBegin, e.faces);
		ASSERT_GE (c->ornament.mesh->GetNVertices (), static_cast<int> (e.vertices));
		ASSERT_GE (c->ornament.mesh->GetNFaces (), static_cast<int> (e.faces));
		const acanthus_test::PrefixPrint p = acanthus_test::MeasurePrefix (*c->ornament.mesh, e.vertices, e.faces);
		EXPECT_EQ (p.topology, e.topology);
		EXPECT_NEAR (p.weighted, e.weighted, 1e-8 * e.weighted);
		for (std::size_t k = 0; k < 3; ++k)
		{
			EXPECT_NEAR (p.lo[k], e.lo[k], 4.0 * FLT_EPSILON * std::fabs (e.lo[k])) << k;
			EXPECT_NEAR (p.hi[k], e.hi[k], 4.0 * FLT_EPSILON * std::fabs (e.hi[k])) << k;
		}
#if defined (_MSC_VER)
		EXPECT_EQ (p.bits, e.bits);
#endif
		std::printf ("K40 volute %s %.1f topo=%016llx bits=%016llx weighted=%.17g lo=%.9g,%.9g,%.9g hi=%.9g,%.9g,%.9g\n", LevelName (e.detail), e.scale,
		             static_cast<unsigned long long> (p.topology), static_cast<unsigned long long> (p.bits), p.weighted, p.lo[0], p.lo[1], p.lo[2],
		             p.hi[0], p.hi[1], p.hi[2]);
	}
}

// K43 : gardes de la construction, vues par fragment de message. La garde « largeur >= rayon local - c » n'a pas de
// scenario : la borne de l'oeil (w <= |O| - k - c) la tient, les centres des arcs etant a moins de k de l'oeil. Borne en
// plan : la pose d'oeil (27,6 ; 39 degres) sort de l'arete superieure de l'abaque et n'est acceptee que sans planBound ;
// planBound vrai pour les volutes, faux pour les helices.
TEST (TEST_cgarchi_classical_acanthus_volute, the_volute_guards_are_seen)
{
	const detail::CapitalEnvelope &env = Cases ()[0].env;
	const detail::VoluteSpec base = detail::DefaultVoluteSpec (OrnamentDetail::Light);
	const auto with = [&] (const std::function<void (detail::VoluteSpec &)> &change) {
		detail::VoluteSpec s = base;
		change (s);
		return s;
	};
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, base, 2); }, "tige 2");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.scale = 0.0; }), 0); }, "forme hors domaine");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.grooveSegments = 3; }), 0); }, "grooveSegments pair");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.beadShare = 0.30; }), 0); },
	               "2 beadShare + grooveShare = 1");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.beadHeight = std::numeric_limits<double>::infinity (); }), 0); },
	               "reliefs finis");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.stemThinLength = 50.0; }), 0); },
	               "rampe d'epaisseur plus longue que la tige");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.minWidth = 1.0; }), 0); }, "derniere spire");
	ExpectInvalid ([&] {
		detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) {
			                           s.stemTangents = 0.001;
			                           s.bellSegments = 20;
			                           s.bendSegments = 40;
		                           }),
		                           0);
	},
	               "courbure de la tige");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.maxTwistRate = 0.01; }), 0); }, "torsion de la tige");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.stemThin = 0.0; }), 0); }, "stemThin dans ]0, 1]");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.stemThin = 1.2; }), 0); }, "stemThin dans ]0, 1]");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.stemThinLength = 0.0; }), 0); }, "stemThinLength fini");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.eyeHeight = 33.0; }), 0); }, "au-dessus de l'abaque");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.eyeRadius = 27.8; s.planBound = false; }), 0); },
	               "hors du cone");
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.eyeRadius = 27.6; s.eyeTheta = 39.0 * kDeg; }), 0); },
	               "hors de l'abaque en plan");
	EXPECT_NO_THROW (detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) {
		s.eyeRadius = 27.6;
		s.eyeTheta = 39.0 * kDeg;
		s.planBound = false;
	}), 0));
	EXPECT_TRUE (detail::DefaultVoluteSpec (OrnamentDetail::Light).planBound);
	EXPECT_FALSE (detail::DefaultHelixSpec (OrnamentDetail::Light).planBound);
	EXPECT_FALSE (detail::DefaultHelixSpec (OrnamentDetail::Heavy).planBound);
	ExpectInvalid ([&] { detail::BuildVoluteRibbon (env, with ([] (detail::VoluteSpec &s) { s.stemFoot = 30.0; s.stemBend = 31.0; }), 0); },
	               "dans la corbeille");
}

// K44 : ruban. Bord exterieur sur le contour de la construction a 1e-9 ; largeur au sommet des tours (coupe
// au-dessus de l'oeil) 1,43 / 0,70 / 0,60 +- 0,3 ; jour entre un tour et le suivant >= c sur tout le trace.
TEST (TEST_cgarchi_classical_acanthus_volute, the_ribbon_follows_the_contour_with_its_widths_and_gaps)
{
	IonicVoluteData data;
	data.eyeDiameter = 2.0 * 0.470;
	data.centreFromTop = 9.0 * 0.470;
	const VoluteSpiral spiral = BuildTwelveCentreVolute (data, VoluteJunction::CentreLine);
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		const detail::VolutePiece v = detail::BuildVoluteRibbon (c.env, detail::DefaultVoluteSpec (c.detail), 0);
		const SpiralPlane plane = PlaneZero ();
		const unsigned nq = v.sectionPoints, nf = nq - 2, first = v.stemStations;
		ASSERT_GE (nq, 3u);
		ASSERT_GE (v.positions.size (), static_cast<std::size_t> (first + v.spiralStations) * nq);
		std::vector<std::array<double, 2>> outer, inner;
		for (unsigned j = first; j < first + v.spiralStations; ++j)
		{
			const Point o = PartsOf (v.positions[j * nq], c.env), i = PartsOf (v.positions[j * nq + nf - 1], c.env);
			outer.push_back ({plane.U (o), plane.V (o)});
			inner.push_back ({plane.U (i), plane.V (i)});
		}
		// contour : chaque point exterieur sur un cercle d'arc de la construction
		double onContour = 0.0;
		for (const std::array<double, 2> &o : outer)
		{
			double best = std::numeric_limits<double>::infinity ();
			for (const VoluteArc &a : spiral.outer.arcs)
				best = std::min (best, std::fabs (std::hypot (o[0] - a.centre.u, o[1] - a.centre.v) - a.radius));
			onContour = std::max (onContour, best);
		}
		EXPECT_LE (onContour, c.scale == 1.0 ? 1e-9 : 1e-8);
		// largeur au sommet de chaque tour : station exterieure au-dessus de l'oeil la plus proche de u = 0
		const auto widthAt = [&] (std::size_t j) { return std::hypot (outer[j][0] - inner[j][0], outer[j][1] - inner[j][1]); };
		std::vector<double> widths {widthAt (0)};
		for (std::size_t j = 1; j + 1 < outer.size (); ++j)
			if (outer[j][1] > 0.0 && outer[j][0] <= 0.0 && outer[j + 1][0] > 0.0)
				widths.push_back (widthAt (std::fabs (outer[j][0]) <= std::fabs (outer[j + 1][0]) ? j : j + 1));
		ASSERT_GE (widths.size (), 3u);
		EXPECT_NEAR (widths[0], 1.43, 0.3);
		EXPECT_NEAR (widths[1], 0.70, 0.3);
		EXPECT_NEAR (widths[2], 0.60, 0.3);
		// jour : bord interieur de la station j contre le bord exterieur des stations a plus de 3/4 de tour plus loin
		const std::size_t lag = static_cast<std::size_t> (3 * detail::DefaultVoluteSpec (c.detail).samplesPerQuarter);
		double gap = std::numeric_limits<double>::infinity ();
		for (std::size_t j = 0; j < inner.size (); ++j)
			for (std::size_t q = j + lag; q + 1 < outer.size (); ++q)
			{
				// distance du point interieur au segment exterieur [q, q + 1]
				const double ax = outer[q][0], ay = outer[q][1], bx = outer[q + 1][0], by = outer[q + 1][1];
				const double dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy;
				const double t = L > 0.0 ? std::clamp (((inner[j][0] - ax) * dx + (inner[j][1] - ay) * dy) / L, 0.0, 1.0) : 0.0;
				gap = std::min (gap, std::hypot (inner[j][0] - ax - t * dx, inner[j][1] - ay - t * dy));
			}
		EXPECT_GE (gap, kC - kTau);
		std::printf ("%s, echelle %g : K44 contour %.1e p ; largeurs %.3f / %.3f / %.3f p ; jour entre tours %.4f p ; %.3f tours\n",
		             LevelName (c.detail), c.scale, onContour, widths[0], widths[1], widths[2], gap, v.turns);
	}
}

namespace
{

// Dossier de sortie du balayage (CG_VOLUTE_SWEEP_DIR) ; vide : sortie standard seule.
std::string SweepDir ()
{
#if defined (_MSC_VER)
	char *value = nullptr;
	std::size_t length = 0;
	if (_dupenv_s (&value, &length, "CG_VOLUTE_SWEEP_DIR") != 0 || value == nullptr)
		return {};
	const std::string dir (value);
	std::free (value);
	return dir;
#else
	const char *value = std::getenv ("CG_VOLUTE_SWEEP_DIR");
	return value ? std::string (value) : std::string ();
#endif
}

// Anneau du sommet le plus haut d'un membre du chapiteau dans le profil.
Ring TopRingOf (const ColumnProfile &profile, const std::string &member)
{
	int index = -1;
	for (std::size_t i = 0; i < profile.members.size (); ++i)
		if (profile.members[i].block == "chapiteau" && profile.members[i].name == member)
			index = static_cast<int> (i);
	EXPECT_GE (index, 0) << member;
	Ring ring;
	double top = -std::numeric_limits<double>::infinity ();
	for (const ProfileVertex &v : profile.vertices)
		if (v.member == index && v.y > top)
		{
			top = v.y;
			ring = v.ring;
		}
	return ring;
}

// Pieces d'un maillage d'essai : sommets en double, triangles, plages.
struct GapScene
{
	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	std::vector<CapitalPiece> pieces;

	void Add (CapitalPieceKind kind, const std::vector<std::array<double, 3>> &pos, const std::vector<std::array<unsigned, 3>> &tri)
	{
		CapitalPiece p;
		p.kind = kind;
		p.vertexBegin = static_cast<unsigned> (positions.size ());
		p.faceBegin = static_cast<unsigned> (triangles.size ());
		for (const std::array<unsigned, 3> &t : tri)
			triangles.push_back ({t[0] + p.vertexBegin, t[1] + p.vertexBegin, t[2] + p.vertexBegin});
		positions.insert (positions.end (), pos.begin (), pos.end ());
		p.vertexEnd = static_cast<unsigned> (positions.size ());
		p.faceEnd = static_cast<unsigned> (triangles.size ());
		pieces.push_back (p);
	}

	void AddFrom (const Mesh &mesh, const CapitalPiece &piece)
	{
		std::vector<std::array<double, 3>> pos;
		std::vector<std::array<unsigned, 3>> tri;
		for (unsigned v = piece.vertexBegin; v < piece.vertexEnd; ++v)
		{
			float q[3] {};
			mesh.GetVertex (v, q);
			pos.push_back ({q[0], q[1], q[2]});
		}
		for (unsigned f = piece.faceBegin; f < piece.faceEnd; ++f)
			tri.push_back ({static_cast<unsigned> (mesh.GetFaceVertex (f, 0)) - piece.vertexBegin, static_cast<unsigned> (mesh.GetFaceVertex (f, 1)) - piece.vertexBegin,
			                static_cast<unsigned> (mesh.GetFaceVertex (f, 2)) - piece.vertexBegin});
		Add (piece.kind, pos, tri);
	}
};

} // namespace

// Instrument, hors suite (--gtest_also_run_disabled_tests) : balayage de la pose des volutes V (paire des tiges 0 et 1
// sous la corne pi/4), echelle 1, Light et Heavy. Grille : planeAngle 22,5 a 36 degres (pas 1,5) x eyeRadius 26,8 a
// 27,6 p (pas 0,2) a eyeTheta 39 degres ; eyeTheta 39 a 41 degres (pas 0,5) a planeAngle 22,5 et eyeRadius 27,6 ;
// puis planeAngle 12 a 22,5 degres (pas 1,5) x eyeRadius 27,0 a 27,6 p (pas 0,2) x eyeTheta 39 a 40,5 degres (pas 0,5).
// Par point, borne en plan levee (planBound faux : le balayage mesure le debord) : construction ou message de garde ; V
// contre l'arete superieure de l'abaque construit (anneau du sommet le plus haut de « A · cimatio dell'abaco (talon) »
// dans le profil), contre B (EnvelopeRadius, comme K60) ; cone
// (LeafBoundRadius) ; jeu au noyau ; h maximal ; jour de la paire ; jour aux D exterieurs (pieces 25 et 27) et aux
// autres pieces voisines de l'ornement par defaut (boite de V elargie de 3 p) ; ecarts de l'oeil aux releves d'AC-63
// (R cos (theta - 45) = 27,1 ; R sin theta = 17,4 ; h 31,5) ; raccourci vu de face (etendue en y de la spirale sur son
// etendue horizontale dans son plan). Criteres : V contre A <= -c, cone < 0, jeu >= c, h <= 36 - c, jours >= c, ecarts
// de l'oeil <= 0,5 p, raccourci >= 0,90. Controle positif : au point (22,5 ; 27,6 ; 39) V contre A +1,22 et contre B
// +3,65 a 0,01. Controles de l'anneau : celui lu par le test est le carre concave de corne 36 et
// de face 18,64 (cotes de l'arete superieure de A, comme K60) ; aucun anneau de A ou de B ne sort de l'arete superieure de A.
// CG_VOLUTE_SWEEP_DIR : CSV du balayage et maillages de la paire en Light (v_<planeAngle>_<eyeRadius>_<eyeTheta>.csv).
TEST (TEST_cgarchi_classical_acanthus_volute, DISABLED_volute_pose_sweep)
{
	const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
	ColumnOptions options;
	options.order = Order::Corinthian;
	const ColumnProfile profile = BuildColumnProfile (order, options);
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (order, profile);
	const Ring top = TopRingOf (profile, "A · cimatio dell'abaco (talon)");
	EXPECT_TRUE (top == Ring::MakeConcaveSquare (36.0, 18.64));
	std::printf ("anneau de l'arete superieure de A : milieu de face %.4f p, corne %.4f p\n", top.RadiusAt (0.0), top.RadiusAt (kPi / 4.0));
	{
		double worst = -std::numeric_limits<double>::infinity ();
		std::string worstMember;
		for (const ProfileVertex &v : profile.vertices)
		{
			if (v.member < 0 || profile.members[static_cast<std::size_t> (v.member)].block != "chapiteau")
				continue;
			const std::string &name = profile.members[static_cast<std::size_t> (v.member)].name;
			if (name.rfind ("A · ", 0) != 0 && name != "B · abaco")
				continue;
			for (int k = 0; k <= 90; ++k)
			{
				const double t = k * 0.5 * kDeg;
				if (v.ring.RadiusAt (t) - top.RadiusAt (t) > worst)
				{
					worst = v.ring.RadiusAt (t) - top.RadiusAt (t);
					worstMember = name;
				}
			}
		}
		EXPECT_LE (worst, 1e-9) << worstMember;
		std::printf ("RA3 : plus grand ecart d'un anneau de A ou de B a l'arete superieure de A %.3e p (%s)\n", worst, worstMember.c_str ());
	}

	const std::string dir = SweepDir ();
	std::ostringstream csv;
	csv << "niveau,planeAngle,eyeRadius,eyeTheta,valide,message,V_contre_A,V_contre_B,cone,jeu_noyau,h_max,jour_paire,jour_D_ext,"
	       "jour_autres,piece_proche,oeil_d27_5,oeil_d17_4,oeil_dh,raccourci,H,V,realisable\n";
	struct Pose
	{
		double planeAngle, eyeRadius, eyeTheta;
	};
	std::vector<Pose> grid;
	for (int a = 0; a < 10; ++a)
		for (int r = 0; r < 5; ++r)
			grid.push_back ({(225 + 15 * a) / 10.0, (268 + 2 * r) / 10.0, 39.0});
	for (int t = 1; t < 5; ++t)
		grid.push_back ({22.5, 27.6, (78 + t) / 2.0});
	for (int a = 0; a < 8; ++a)
		for (int r = 0; r < 4; ++r)
			for (int t = 0; t < 4; ++t)
			{
				const Pose pose {(120 + 15 * a) / 10.0, (270 + 2 * r) / 10.0, (78 + t) / 2.0};
				const bool known = pose.planeAngle == 22.5 && (pose.eyeTheta == 39.0 || pose.eyeRadius == 27.6);
				if (!known)
					grid.push_back (pose);
			}

	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		AcanthusOptions o;
		o.detail = level;
		const CapitalOrnament ornament = BuildCorinthianOrnament (order, profile, o);
		const Mesh &mesh = *ornament.mesh;
		const std::vector<CapitalPiece> &P = ornament.pieces;
		ASSERT_EQ (P.size (), 60u);
		std::array<double, 3> lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
		for (const std::size_t i : {kFirstVolute, kFirstVolute + 1})
			for (unsigned v = P[i].vertexBegin; v < P[i].vertexEnd; ++v)
			{
				const Point p = Parts (mesh, env, v);
				for (int q = 0; q < 3; ++q)
				{
					lo[q] = std::min (lo[q], p[q] - 3.0);
					hi[q] = std::max (hi[q], p[q] + 3.0);
				}
			}
		std::vector<std::size_t> near;
		for (std::size_t i = 0; i < P.size (); ++i)
		{
			if (P[i].kind == CapitalPieceKind::Volute)
				continue;
			bool hit = false;
			for (unsigned v = P[i].vertexBegin; v < P[i].vertexEnd && !hit; ++v)
			{
				const Point p = Parts (mesh, env, v);
				hit = p[0] >= lo[0] && p[0] <= hi[0] && p[1] >= lo[1] && p[1] <= hi[1] && p[2] >= lo[2] && p[2] <= hi[2];
			}
			if (hit)
				near.push_back (i);
		}
		std::printf ("%s : %zu pieces voisines de la paire de V\n", LevelName (level), near.size ());

		for (const Pose &pose : grid)
		{
			detail::VoluteSpec spec = detail::DefaultVoluteSpec (level);
			spec.planeAngle = pose.planeAngle * kDeg;
			spec.eyeRadius = pose.eyeRadius;
			spec.eyeTheta = pose.eyeTheta * kDeg;
			spec.planBound = false;
			char head[96];
			std::snprintf (head, sizeof head, "%s,%.1f,%.1f,%.1f", LevelName (level), pose.planeAngle, pose.eyeRadius, pose.eyeTheta);
			std::array<detail::VolutePiece, 2> pair;
			try
			{
				pair[0] = detail::BuildVoluteRibbon (env, spec, 0);
				pair[1] = detail::BuildVoluteRibbon (env, spec, 1);
			}
			catch (const std::invalid_argument &e)
			{
				std::string message (e.what ());
				for (char &ch : message)
					if (ch == ',' || ch == '\n')
						ch = ';';
				csv << head << ",0," << message << ",,,,,,,,,,,,,,,,0\n";
				std::printf ("BALAYAGE %s : refuse (%s)\n", head, e.what ());
				continue;
			}
			double vsA = -1e300, vsB = -1e300, cone = -1e300, core = 1e300, hTop = -1e300;
			std::size_t worstIndex = 0;
			for (const detail::VolutePiece &v : pair)
				for (std::size_t j = 0; j < v.positions.size (); ++j)
				{
					const Point p = PartsOf (v.positions[j], env);
					const double r = std::hypot (p[0], p[1]), t = std::atan2 (p[1], p[0]);
					if (r - top.RadiusAt (t) > vsA && &v == &pair[0])
						worstIndex = j;
					vsA = std::max (vsA, r - top.RadiusAt (t));
					vsB = std::max (vsB, r - env.EnvelopeRadius (p[2], t));
					cone = std::max (cone, r - env.LeafBoundRadius (p[2]));
					core = std::min (core, r - env.CoreRadius (p[2]));
					hTop = std::max (hTop, p[2]);
				}
			GapScene scene;
			for (const detail::VolutePiece &v : pair)
				scene.Add (CapitalPieceKind::Volute, v.positions, v.triangles);
			for (const std::size_t i : near)
				scene.AddFrom (mesh, P[i]);
			const auto gapMesh = MakeTriangleMesh (scene.positions, scene.triangles);
			const double pairGap = acanthus_test::PieceGap (*gapMesh, env, scene.pieces[0], scene.pieces[1], 5.0);
			double outerD = 3.0, others = 3.0;
			std::size_t closest = 0;
			for (std::size_t k = 0; k < near.size (); ++k)
				for (std::size_t s = 0; s < 2; ++s)
				{
					const double g = acanthus_test::PieceGap (*gapMesh, env, scene.pieces[s], scene.pieces[2 + k], 3.0);
					if (near[k] == 25 + 2 * s)
						outerD = std::min (outerD, g);
					if (g < others)
					{
						others = g;
						closest = near[k];
					}
				}
			const Point eye {pair[0].eye[0], pair[0].eye[1], pair[0].eye[2]};
			const double eyeR = std::hypot (eye[0], eye[1]), eyeT = std::atan2 (eye[1], eye[0]);
			const double d27 = eyeR * std::cos (eyeT - 45.0 * kDeg) - 27.1, d17 = eyeR * std::sin (eyeT) - 17.4, dh = eye[2] - 31.5;
			const double a = 90.0 * kDeg - spec.planeAngle;
			const double ux = std::cos (a), uy = std::sin (a);
			double uLo = 1e300, uHi = -1e300, vLo = 1e300, vHi = -1e300, yLo = 1e300, yHi = -1e300;
			for (std::size_t j = static_cast<std::size_t> (pair[0].stemStations) * pair[0].sectionPoints; j < pair[0].ribbonVertices; ++j)
			{
				const Point p = PartsOf (pair[0].positions[j], env);
				const double u = (p[0] - eye[0]) * ux + (p[1] - eye[1]) * uy;
				uLo = std::min (uLo, u);
				uHi = std::max (uHi, u);
				vLo = std::min (vLo, p[2]);
				vHi = std::max (vHi, p[2]);
				yLo = std::min (yLo, p[1]);
				yHi = std::max (yHi, p[1]);
			}
			const double H = uHi - uLo, V = vHi - vLo, front = (yHi - yLo) / H;
			const bool feasible = vsA <= -kC && cone < 0.0 && core >= kC - kTau && hTop <= 36.0 - kC + kTau && pairGap >= kC && outerD >= kC &&
			                      others >= kC && std::max ({std::fabs (d27), std::fabs (d17), std::fabs (dh)}) <= 0.5 && front >= 0.90;
			char row[512];
			std::snprintf (row, sizeof row, "%s,1,,%+.4f,%+.4f,%+.4f,%.4f,%.3f,%.4f,%.4f,%.4f,%zu,%+.3f,%+.3f,%+.3f,%.4f,%.3f,%.3f,%d\n", head, vsA, vsB,
			               cone, core, hTop, pairGap, outerD, others, closest, d27, d17, dh, front, H, V, feasible ? 1 : 0);
			csv << row;
			std::printf ("BALAYAGE %s", row);
			if (pose.planeAngle == 22.5 && pose.eyeRadius == 27.6 && pose.eyeTheta == 39.0)
			{
				EXPECT_NEAR (vsA, 1.22, 0.01) << LevelName (level);
				EXPECT_NEAR (vsB, 3.65, 0.01) << LevelName (level);
				const detail::VolutePiece &v = pair[0];
				const char *where = "oeil";
				std::size_t station = 0;
				if (worstIndex < v.ribbonVertices)
				{
					station = worstIndex / v.sectionPoints;
					where = station < v.stemStations ? "tige" : "spirale";
				}
				std::printf ("%s : sommet saillant contre A, indice %zu : %s, station %zu (tige %u stations, spirale %u, %u points par section, "
				             "%.3f tours)\n", LevelName (level), worstIndex, where, station, v.stemStations, v.spiralStations, v.sectionPoints, v.turns);
			}
			if (!dir.empty () && level == OrnamentDetail::Light)
			{
				char name[128];
				std::snprintf (name, sizeof name, "/v_%.1f_%.1f_%.1f.csv", pose.planeAngle, pose.eyeRadius, pose.eyeTheta);
				std::ofstream out (dir + name);
				out.precision (9);
				for (const detail::VolutePiece &v : pair)
					for (const std::array<double, 3> &q : v.positions)
					{
						const Point p = PartsOf (q, env);
						out << "v," << p[0] << ',' << p[1] << ',' << p[2] << '\n';
					}
				const unsigned offset = static_cast<unsigned> (pair[0].positions.size ());
				for (std::size_t s = 0; s < 2; ++s)
					for (const std::array<unsigned, 3> &t : pair[s].triangles)
						out << "f," << t[0] + s * offset << ',' << t[1] + s * offset << ',' << t[2] + s * offset << '\n';
			}
		}
	}
	if (!dir.empty ())
	{
		std::ofstream out (dir + "/volute_pose_sweep.csv");
		out << csv.str ();
	}
}
