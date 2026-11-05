#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_orders_data.h>
#include <cgarchi/classical_ionic_capital.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/classical_volute.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Volutes ioniques (Vignole, pl. XIX et XX)
// ===========================================================================

using namespace cgarchi::classical;

namespace {

const IonicVoluteData &Volute ()
{
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	if (!ionic.volute)
		throw std::runtime_error ("ornements.volute absent");
	return *ionic.volute;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;

// Courbe en coordonnees polaires autour de l'oeil : angle horaire depuis le
// haut, deroule, et distance au centre. Le prefixe rectiligne est exclu.
struct Polar
{
	std::vector<double> angle, radius;

	explicit Polar (const VoluteCurve &curve, int segmentsPerQuarter)
	{
		VoluteCurve arcs = curve;
		arcs.prefixStart.reset ();
		double previous = 0.0;
		for (const VolutePoint &p : SampleCurve (arcs, segmentsPerQuarter))
		{
			const double a = std::atan2 (p.u, p.v);
			if (angle.empty ())
				angle.push_back (a);
			else
			{
				double d = a - previous;
				while (d < -kPi) d += 2.0 * kPi;
				while (d > kPi) d -= 2.0 * kPi;
				angle.push_back (angle.back () + d);
			}
			previous = a;
			radius.push_back (std::hypot (p.u, p.v));
		}
	}

	bool Covers (double psi) const { return psi > angle.front () && psi < angle.back (); }

	double At (double psi) const
	{
		const auto it = std::lower_bound (angle.begin (), angle.end (), psi);
		const std::size_t i = static_cast<std::size_t> (it - angle.begin ());
		const double t = (psi - angle[i - 1]) / (angle[i] - angle[i - 1]);
		return radius[i - 1] + t * (radius[i] - radius[i - 1]);
	}
};

std::string Defects (const MeshValidationReport &r)
{
	std::string out;
	for (std::size_t k = 0; k < kMeshDefectCount; ++k)
		if (r.counts[k])
			out += std::string (" ") + MeshDefectName (static_cast<MeshDefect> (k)) + "=" + std::to_string (r.counts[k]);
	if (r.selfIntersectionTruncated)
		out += " truncated";
	return out;
}

struct Widths
{
	double listel = 1e300;   // min O(psi) - I(psi)
	double canal = 1e300;    // min I(psi) - O(psi + 2 pi)
};

Widths MeasureWidths (const VoluteSpiral &spiral)
{
	const Polar o (spiral.outer, 64), in (spiral.inner, 64);
	Widths w;
	const int n = 6000;
	for (int k = 1; k < n; ++k)
	{
		const double psi = o.angle.front () + (o.angle.back () - o.angle.front ()) * k / n;
		if (o.Covers (psi) && in.Covers (psi))
			w.listel = std::min (w.listel, o.At (psi) - in.At (psi));
		if (in.Covers (psi) && o.Covers (psi + 2.0 * kPi))
			w.canal = std::min (w.canal, in.At (psi) - o.At (psi + 2.0 * kPi));
	}
	return w;
}

}  // namespace

// VE1
TEST (TEST_cgarchi_classical_volute, the_volute_data_is_read_exactly_and_is_consistent)
{
	const IonicVoluteData &v = Volute ();
	EXPECT_EQ (v.height, 16.0);
	EXPECT_EQ (v.eyeDiameter, 2.0);
	EXPECT_EQ (v.centreFromTop, 9.0);
	EXPECT_EQ (v.centreFromBottom, 7.0);
	EXPECT_EQ (v.top, 9.0);
	EXPECT_EQ (v.right, 8.0);
	EXPECT_EQ (v.bottom, 7.0);
	EXPECT_EQ (v.left, 6.0);
	EXPECT_EQ (v.catheteSpacing, 36.0);
	EXPECT_EQ (v.eyeFromAxis, 18.0);
	EXPECT_EQ (v.topBelowCapital, 3.0);
	EXPECT_EQ (v.faceSpacing, 35.0);
	EXPECT_EQ (v.cushionWidth, 14.0);
	EXPECT_EQ (v.listelFace, 2.0);

	// Relations de la planche : cathete 16 = 9 + 7 ; coussinet 14 = 8 + 6 ;
	// entraxe des cathetes 36 = 2 x 18 ; le centre de l'oeil est au bas du
	// chapiteau (3 + 9 = 12 = hauteur du chapiteau).
	EXPECT_EQ (v.height, v.centreFromTop + v.centreFromBottom);
	EXPECT_EQ (v.cushionWidth, v.right + v.left);
	EXPECT_EQ (v.catheteSpacing, 2.0 * v.eyeFromAxis);
	const Block *capital = ClassicalOrders ().Get (Order::Ionic).FindBlock ("chapiteau");
	ASSERT_NE (capital, nullptr);
	ASSERT_TRUE (capital->totalHeight.has_value ());
	EXPECT_EQ (v.topBelowCapital + v.centreFromTop, *capital->totalHeight);

	// Les autres ordres n'ont pas de volute ionique.
	for (Order o : { Order::Tuscan, Order::Doric, Order::Corinthian, Order::Composite })
		EXPECT_FALSE (ClassicalOrders ().Get (o).volute.has_value ()) << OrderKey (o);
}

TEST (TEST_cgarchi_classical_volute, a_missing_volute_key_is_named_in_the_error)
{
	std::string text (cgarchi::ClassicalOrdersJson ());
	const std::size_t at = text.find ("\"listel_face\"");
	ASSERT_NE (at, std::string::npos);
	text.replace (at, 13, "\"listel_fac\"");
	try
	{
		ParseClassicalOrders (text);
		FAIL () << "cle retiree acceptee";
	}
	catch (const std::runtime_error &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("ornements/volute/listel_face"), std::string::npos) << e.what ();
	}

	std::string nested (cgarchi::ClassicalOrdersJson ());
	const std::size_t right = nested.find ("\"droite\"");
	ASSERT_NE (right, std::string::npos);
	nested.replace (right, 8, "\"droit\"");
	try
	{
		ParseClassicalOrders (nested);
		FAIL () << "cle retiree acceptee";
	}
	catch (const std::runtime_error &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("ornements/volute/distances_au_centre/droite"), std::string::npos)
			<< e.what ();
	}
}

// VE2
TEST (TEST_cgarchi_classical_volute, the_first_turn_meets_the_engraved_extents)
{
	const VoluteSpiral spiral = BuildTwelveCentreVolute (Volute ());
	ASSERT_EQ (spiral.outer.arcs.size (), 12u);
	ASSERT_EQ (spiral.inner.arcs.size (), 12u);
	// « p 8 » a droite, 7 en dessous, 6 a gauche, sommet a 9.
	const VoluteBox box = CurveExtents (spiral.outer, 4);
	EXPECT_NEAR (box.vMax, 9.0, 1e-12);
	EXPECT_NEAR (box.uMax, 8.0, 1e-12);
	EXPECT_NEAR (box.vMin, -7.0, 1e-12);
	EXPECT_NEAR (box.uMin, -6.0, 1e-12);
	// Le premier tour revient a la cathete du premier centre en (-1/2, 5) :
	// le 4e arc, de centre (-1/2, -1/2) et de rayon 11/2, y passe a la
	// verticale de son centre.
	const VoluteArc &fourth = spiral.outer.arcs[3];
	EXPECT_NEAR (fourth.centre.u, -0.5, 1e-15);
	EXPECT_NEAR (fourth.centre.v + fourth.radius, 5.0, 1e-12);
	EXPECT_GT (0.5 * kPi, fourth.a1);
	EXPECT_LT (0.5 * kPi, fourth.a0);
	EXPECT_NEAR (spiral.listelWidth, 1.0, 1e-12);
	// Toute la volute tient dans la cathete et le coussinet : 9 + 7, 8 + 6.
	const VoluteBox all = CurveExtents (spiral.outer, 12);
	EXPECT_NEAR (all.vMax - all.vMin, Volute ().height, 1e-12);
	EXPECT_NEAR (all.uMax - all.uMin, Volute ().cushionWidth, 1e-12);
}

TEST (TEST_cgarchi_classical_volute, the_centre_line_junction_is_tangent_and_the_axis_junction_is_not)
{
	const VoluteSpiral g1 = BuildTwelveCentreVolute (Volute ());
	EXPECT_LT (MaxTangentBreak (g1.outer), 1e-9);
	EXPECT_LT (MaxTangentBreak (g1.inner), 1e-9);   // prefixe compris
	// Negatif : le raccord sur l'axe du centre (section 5.7) casse la
	// tangente aux changements de tour, 4,39 degres au pire (8e -> 9e arc).
	const VoluteSpiral axis = BuildTwelveCentreVolute (Volute (), VoluteJunction::Axis);
	EXPECT_NEAR (MaxTangentBreak (axis.outer) / kDegree, 4.393, 0.01);
}

TEST (TEST_cgarchi_classical_volute, the_spiral_ends_in_the_eye_with_a_positive_listel_and_canal)
{
	const VoluteSpiral spiral = BuildTwelveCentreVolute (Volute ());
	const VolutePoint end = spiral.outer.arcs.back ().End ();
	const double r = std::hypot (end.u, end.v);
	EXPECT_GE (r, 0.95);
	EXPECT_LT (r, spiral.eyeRadius);
	const Widths w = MeasureWidths (spiral);
	EXPECT_GT (w.listel, 0.0);
	EXPECT_GE (w.canal, 1.0);
	std::cout << "[ mesure ] fin exterieure " << r << ", listel min " << w.listel << ", canal min " << w.canal << '\n';

	// Les centres interieurs decales vers l'exterieur (lecture inverse du
	// texte) donnent des rayons negatifs des le 10e arc : refus.
	EXPECT_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, -0.5 / 12.0), std::invalid_argument);
	EXPECT_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, -0.5 / 48.0), std::invalid_argument);

	// Negatifs de l'instrument : un bord interieur decale radialement (les
	// raccords restent sur les lignes des centres) fait echouer chaque critere.
	VoluteSpiral deep = spiral;
	for (VoluteArc &arc : deep.inner.arcs)
		arc.radius -= 1.2;
	EXPECT_LT (MeasureWidths (deep).canal, 0.0);
	VoluteSpiral crossed = spiral;
	for (VoluteArc &arc : crossed.inner.arcs)
		arc.radius += 0.5;
	EXPECT_LT (MeasureWidths (crossed).listel, 0.0);
}

// VE3
TEST (TEST_cgarchi_classical_volute, the_section_partitions_the_body_into_canal_and_relief)
{
	const VoluteSpiral spiral = BuildTwelveCentreVolute (Volute ());
	for (int segments : { 2, 6, 32 })
	{
		const VoluteSection section = BuildVoluteSection (spiral, -0.5, segments);
		const double s = PolygonArea (section.points, section.body);
		const double r = PolygonArea (section.points, section.raised);
		const double c = PolygonArea (section.points, section.canal);
		EXPECT_GT (s, 0.0);
		EXPECT_GT (r, 0.0);
		EXPECT_GT (c, 0.0);
		EXPECT_LE (std::fabs (c + r - s), 1e-9 * s) << segments;
		EXPECT_FALSE (PolygonSelfIntersects (section.points, section.body)) << segments;
		EXPECT_FALSE (PolygonSelfIntersects (section.points, section.raised)) << segments;
		EXPECT_FALSE (PolygonSelfIntersects (section.points, section.canal)) << segments;
		if (segments == 32)
			std::cout << "[ mesure ] aires S " << s << ", R " << r << ", C " << c << '\n';
	}
}

TEST (TEST_cgarchi_classical_volute, each_spiral_edge_enters_the_eye_once)
{
	const VoluteSpiral spiral = BuildTwelveCentreVolute (Volute ());
	for (const VoluteCurve *curve : { &spiral.outer, &spiral.inner })
	{
		const std::vector<VolutePoint> p = SampleCurve (*curve, 32);
		int crossings = 0;
		for (std::size_t i = 1; i < p.size (); ++i)
		{
			const bool a = std::hypot (p[i - 1].u, p[i - 1].v) < spiral.eyeRadius;
			const bool b = std::hypot (p[i].u, p[i].v) < spiral.eyeRadius;
			crossings += a != b ? 1 : 0;
		}
		EXPECT_EQ (crossings, 1);
	}
	const Polar o (spiral.outer, 256), in (spiral.inner, 256);
	auto entry = [&] (const Polar &polar) {
		for (std::size_t i = 1; i < polar.radius.size (); ++i)
			if (polar.radius[i - 1] >= spiral.eyeRadius && polar.radius[i] < spiral.eyeRadius)
				return polar.angle[i] / kDegree;
		return 0.0;
	};
	std::cout << "[ mesure ] entree dans l'oeil : contour " << entry (o) << " deg, bord interieur " << entry (in) << " deg\n";
	EXPECT_NEAR (entry (o), 1060.8, 0.5);
	EXPECT_NEAR (entry (in), 1055.4, 0.5);
}

TEST (TEST_cgarchi_classical_volute, a_relief_without_the_eye_is_refused)
{
	const VoluteSection section = BuildVoluteSection (BuildTwelveCentreVolute (Volute ()), -0.5, 6);
	EXPECT_NO_THROW (ValidateVoluteSection (section));
	// Negatif : R sans l'oeil (les points propres a l'arc de l'oeil retires de
	// R) ne partitionne plus S avec C.
	VoluteSection noEye = section;
	std::vector<unsigned> raised;
	for (unsigned index : noEye.raised)
		if (std::find (section.body.begin (), section.body.end (), index) != section.body.end () ||
		    std::find (section.canal.begin (), section.canal.end (), index) == section.canal.end () ||
		    std::fabs (std::hypot (section.points[index].u, section.points[index].v) - 1.0) > 1e-9)
			raised.push_back (index);
	ASSERT_LT (raised.size (), noEye.raised.size ());
	noEye.raised = raised;
	try
	{
		ValidateVoluteSection (noEye);
		FAIL () << "relief sans oeil accepte";
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("ne partitionnent pas"), std::string::npos) << e.what ();
	}
	// Une fermeture qui ne passe pas par le sommet de la volute est refusee.
	EXPECT_THROW (BuildVoluteSection (BuildTwelveCentreVolute (Volute ()), 0.0, 6), std::invalid_argument);
}

// VE4
TEST (TEST_cgarchi_classical_volute, the_bolster_is_a_closed_valid_solid_of_the_expected_volume)
{
	const IonicVoluteData &v = Volute ();
	const double F = v.faceSpacing / 2.0, d = v.listelFace;
	const VoluteSpiral spiral = BuildTwelveCentreVolute (v);
	for (int segments : { 2, 6, 32 })
	{
		const VoluteSection section = BuildVoluteSection (spiral, -0.5, segments);
		const std::unique_ptr<Mesh> bolster = BuildBolster (section, F, d);
		ASSERT_NE (bolster, nullptr);
		MeshValidationOptions options;
		options.expectedComponents = 1u;
		const MeshValidationReport r = ValidateMesh (*bolster, options);
		EXPECT_TRUE (r.IsValid ()) << segments << " :" << Defects (r);
		EXPECT_EQ (r.components, 1u);
		const double expected = 2.0 * F * PolygonArea (section.points, section.body)
		                      + 2.0 * d * PolygonArea (section.points, section.raised);
		EXPECT_NEAR (r.signedVolume, expected, 1e-4 * expected) << segments;
		std::cout << "[ mesure ] coussinet a " << segments << " segments par quart : " << bolster->GetNFaces ()
		          << " triangles, volume " << r.signedVolume << '\n';
	}
}

TEST (TEST_cgarchi_classical_volute, walls_not_split_at_the_faces_leave_the_bolster_open)
{
	// Negatif : une paroi commune a S et R d'un seul tenant de -(F + d) a
	// F + d ne partage pas ses aretes avec les parois coupees en +-F.
	const VoluteSection section = BuildVoluteSection (BuildTwelveCentreVolute (Volute ()), -0.5, 6);
	const MeshValidationReport r = ValidateMesh (*BuildBolster (section, 17.5, 2.0, false));
	EXPECT_GT (r.Count (MeshDefect::BoundaryEdge), 0u) << Defects (r);
}

TEST (TEST_cgarchi_classical_volute, a_polygon_the_tessellator_must_split_is_refused)
{
	// Le noeud papillon oblige le tessellateur a creer le point de croisement.
	const std::vector<VolutePoint> points { {0.0, 0.0}, {2.0, 2.0}, {2.0, 0.0}, {0.0, 2.0} };
	EXPECT_THROW (TessellatePolygon (points, {0, 1, 2, 3}), std::invalid_argument);
	// Positif : le carre se triangule sans sommet ajoute.
	EXPECT_EQ (TessellatePolygon (points, {0, 2, 1, 3}).size (), 2u);
	// Le meme carre en sens horaire : les triangles rendus (sens
	// trigonometrique) ne couvrent pas l'aire signee du polygone.
	EXPECT_THROW (TessellatePolygon (points, {3, 1, 2, 0}), std::invalid_argument);
}

TEST (TEST_cgarchi_classical_volute, the_self_intersection_detector_finds_crossings_and_contacts)
{
	// Noeud papillon : deux aretes non voisines se croisent.
	const std::vector<VolutePoint> bow { {0.0, 0.0}, {2.0, 2.0}, {2.0, 0.0}, {0.0, 2.0} };
	EXPECT_TRUE (PolygonSelfIntersects (bow, {0, 1, 2, 3}));
	EXPECT_FALSE (PolygonSelfIntersects (bow, {0, 2, 1, 3}));
	// Sommet (2, 0) pose sur l'arete non voisine (0, 0) - (4, 0).
	const std::vector<VolutePoint> touch { {0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 0.0}, {0.0, 4.0} };
	EXPECT_TRUE (PolygonSelfIntersects (touch, {0, 1, 2, 3, 4}));
	// Le meme contour, le sommet releve a (2, 1) : simple.
	const std::vector<VolutePoint> lifted { {0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 1.0}, {0.0, 4.0} };
	EXPECT_FALSE (PolygonSelfIntersects (lifted, {0, 1, 2, 3, 4}));
	// Croisement loin dans l'ordre du contour, entre aretes d'abscisses
	// eloignees dans le tri du balayage.
	std::vector<VolutePoint> zig;
	std::vector<unsigned> loop;
	for (int k = 0; k <= 20; ++k)
		zig.push_back ({static_cast<double> (k), k % 2 ? 1.0 : 0.0});
	zig.push_back ({20.0, 5.0});
	zig.push_back ({10.0, -1.0});   // retombe sous le zigzag : coupe ses aretes
	zig.push_back ({0.0, 5.0});
	for (unsigned k = 0; k < zig.size (); ++k)
		loop.push_back (k);
	EXPECT_TRUE (PolygonSelfIntersects (zig, loop));
	zig[22] = {10.0, 3.0};
	EXPECT_FALSE (PolygonSelfIntersects (zig, loop));
}

TEST (TEST_cgarchi_classical_volute, an_inner_shift_out_of_its_domain_is_refused)
{
	const double e = Volute ().eyeDiameter / 2.0;
	EXPECT_NO_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, e / 12.0));
	// e/6 : les centres du dernier tour se confondent au centre de l'oeil.
	EXPECT_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, e / 6.0), std::invalid_argument);
	// Au-dela, ils s'inversent (arcs de trois quarts de tour sans garde).
	EXPECT_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, 0.2), std::invalid_argument);
	EXPECT_THROW (BuildTwelveCentreVolute (Volute (), VoluteJunction::CentreLine, 0.3), std::invalid_argument);
}

TEST (TEST_cgarchi_classical_volute, the_axis_junction_does_not_close_into_the_eye)
{
	// Le raccord sur l'axe acheve le 12e arc a 1,023 du centre, hors de l'oeil.
	const VoluteSpiral axis = BuildTwelveCentreVolute (Volute (), VoluteJunction::Axis);
	const VolutePoint end = axis.outer.arcs.back ().End ();
	EXPECT_NEAR (std::hypot (end.u, end.v), 1.023, 1e-3);
	try
	{
		BuildVoluteSection (axis, -0.5, 6);
		FAIL () << "section construite sur le raccord Axis";
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("n'entre pas dans l'oeil"), std::string::npos) << e.what ();
	}
}

// ===========================================================================
//  VE5 : placement sur le chapiteau
// ===========================================================================

namespace {

// Parite des traversees d'un rayon : vrai si p est a l'interieur d'une coque
// fermee. Direction oblique, pour ne passer ni par une arete ni par un sommet.
bool Inside (const Mesh &mesh, const std::array<double, 3> &p)
{
	const double d[3] = { 0.5773, 0.3141, 0.7536 };
	int hits = 0;
	for (unsigned f = 0; f < mesh.GetNFaces (); ++f)
	{
		float a[3], b[3], c[3];
		mesh.GetVertex (static_cast<unsigned> (mesh.GetFaceVertex (f, 0)), a);
		mesh.GetVertex (static_cast<unsigned> (mesh.GetFaceVertex (f, 1)), b);
		mesh.GetVertex (static_cast<unsigned> (mesh.GetFaceVertex (f, 2)), c);
		const double e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
		const double e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
		const double h[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
		const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
		if (std::fabs (det) < 1e-15)
			continue;
		const double s[3] = { p[0] - a[0], p[1] - a[1], p[2] - a[2] };
		const double u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;
		if (u < 0.0 || u > 1.0)
			continue;
		const double q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
		const double v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
		if (v < 0.0 || u + v > 1.0)
			continue;
		const double t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
		if (t > 0.0)
			++hits;
	}
	return hits % 2 == 1;
}

struct Ionic
{
	OrderData order;
	ColumnProfile profile;
	double zEye = 0.0;

	explicit Ionic (ColumnOptions options = {})
		: order (ClassicalOrders ().Get (Order::Ionic))
	{
		options.order = Order::Ionic;
		options.ionicVolutes = true;
		profile = BuildColumnProfile (order, options);
		double top = 0.0;
		for (const ProfileMember &m : profile.members)
			if (m.block == "chapiteau")
				top = std::max (top, m.y1);
		zEye = top - order.volute->topBelowCapital - order.volute->centreFromTop;
	}
};

void Box (const Mesh &mesh, float lo[3], float hi[3])
{
	for (int k = 0; k < 3; ++k) { lo[k] = 1e30f; hi[k] = -1e30f; }
	for (unsigned i = 0; i < mesh.GetNVertices (); ++i)
	{
		float p[3];
		mesh.GetVertex (i, p);
		for (int k = 0; k < 3; ++k) { lo[k] = std::min (lo[k], p[k]); hi[k] = std::max (hi[k], p[k]); }
	}
}

}  // namespace

TEST (TEST_cgarchi_classical_volute, the_bolsters_sit_on_both_sides_at_the_engraved_place)
{
	const Ionic ionic;
	// L'oeil est au bas du chapiteau.
	const Block *capital = ionic.order.FindBlock ("chapiteau");
	double capitalBottom = 1e300;
	for (const ProfileMember &m : ionic.profile.members)
		if (m.block == "chapiteau")
			capitalBottom = std::min (capitalBottom, m.y0);
	EXPECT_NEAR (ionic.zEye, capitalBottom, 1e-9);
	ASSERT_NE (capital, nullptr);

	for (double scale : { 1.0, 1.0 / 18.0 })
	{
		VoluteOptions options;
		options.scale = scale;
		const auto volutes = BuildIonicVolutes (ionic.order, ionic.profile, options);
		ASSERT_EQ (volutes.size (), 2u);
		for (std::size_t k = 0; k < 2; ++k)
		{
			const double side = k == 0 ? 1.0 : -1.0;
			float lo[3], hi[3];
			Box (*volutes[k], lo, hi);
			const double tol = 1e-4 * std::max (1.0, 60.0 * scale);
			EXPECT_NEAR (side > 0 ? lo[0] : -hi[0], 12.0 * scale, tol);
			EXPECT_NEAR (side > 0 ? hi[0] : -lo[0], 26.0 * scale, tol);
			EXPECT_NEAR (lo[1], -19.5 * scale, tol);
			EXPECT_NEAR (hi[1], 19.5 * scale, tol);
			EXPECT_NEAR (lo[2], (ionic.zEye - 7.0) * scale, tol);
			EXPECT_NEAR (hi[2], (ionic.zEye + 9.0) * scale, tol);
			const MeshValidationReport r = ValidateMesh (*volutes[k]);
			EXPECT_TRUE (r.IsValid ()) << Defects (r);
			EXPECT_GT (r.signedVolume, 0.0);
		}
	}
}

TEST (TEST_cgarchi_classical_volute, a_witness_point_is_inside_the_bolster_and_the_column)
{
	const Ionic ionic;
	const auto volutes = BuildIonicVolutes (ionic.order, ionic.profile, {});
	LatheOptions lathe;
	const std::unique_ptr<Mesh> column = LatheColumnProfile (ionic.profile, lathe);
	for (double side : { 1.0, -1.0 })
	{
		const std::array<double, 3> p { side * 14.0, 0.0, ionic.zEye + 0.5 };
		EXPECT_TRUE (Inside (*volutes[side > 0 ? 0 : 1], p)) << side;
		EXPECT_TRUE (Inside (*column, p)) << side;
		EXPECT_FALSE (Inside (*volutes[side > 0 ? 1 : 0], p)) << side;
	}
	// Negatif : oeil a 30 de l'axe, le point temoin sort du coussinet.
	Ionic far;
	far.order.volute->eyeFromAxis = 30.0;
	const auto shifted = BuildIonicVolutes (far.order, far.profile, {});
	EXPECT_FALSE (Inside (*shifted[0], { 14.0, 0.0, far.zEye + 0.5 }));
}

TEST (TEST_cgarchi_classical_volute, the_ovolo_beyond_the_band_is_buried_in_the_bolster_on_x_and_free_on_y)
{
	// Le conflit ionique (ove de rayon 22 sous la bande carree de demi-cote
	// 17,5) reste une jonction croisee du tour ; sur +-X, l'exces de l'ove est
	// dans le coussinet, sur +-Y c'est l'echine qui deborde entre les volutes.
	const Ionic ionic;
	const std::vector<PlanConflict> conflicts = FindPlanConflicts (ionic.profile);
	ASSERT_EQ (conflicts.size (), 1u);
	ASSERT_EQ (conflicts[0].angles.size (), 1u);
	EXPECT_NEAR (conflicts[0].angles[0], std::acos (17.5 / 22.0), 1e-9);

	const auto volutes = BuildIonicVolutes (ionic.order, ionic.profile, {});
	const double r = 21.5, z = ionic.zEye + 4.5;   // dans l'ove (rayon 21,98 a cette hauteur)
	for (double deg : { -30.0, -15.0, 0.0, 15.0, 30.0 })
		for (int side : { 0, 1 })
		{
			const double t = deg * kDegree + (side ? kPi : 0.0);
			const std::array<double, 3> p { r * std::cos (t), r * std::sin (t), z };
			ASSERT_GT (std::fabs (p[0]), 17.5);
			EXPECT_TRUE (Inside (*volutes[side], p)) << deg << " " << side;
		}
	for (double deg : { 60.0, 90.0, 120.0, 240.0, 270.0, 300.0 })
	{
		const double t = deg * kDegree;
		const std::array<double, 3> p { r * std::cos (t), r * std::sin (t), z };
		ASSERT_GT (std::fabs (p[1]), 17.5);
		EXPECT_FALSE (Inside (*volutes[0], p)) << deg;
		EXPECT_FALSE (Inside (*volutes[1], p)) << deg;
	}
}

TEST (TEST_cgarchi_classical_volute, every_ionic_configuration_with_volutes_is_valid_part_by_part)
{
	for (int pedestal = 0; pedestal < 2; ++pedestal)
		for (int entablature = 0; entablature < 2; ++entablature)
			for (int envelope = 0; envelope < 2; ++envelope)
			{
				ColumnOptions options;
				options.pedestal = pedestal != 0;
				options.entablature = entablature != 0;
				options.ornamentEnvelope = envelope != 0;
				options.curveSegments = 2;
				options.shaftSegments = 8;
				const Ionic ionic (options);
				LatheOptions lathe;
				lathe.radialSegments = 16;
				MeshValidationOptions one;
				one.expectedComponents = 1u;
				const MeshValidationReport tour = ValidateMesh (*LatheColumnProfile (ionic.profile, lathe), one);
				EXPECT_TRUE (tour.IsValid ()) << pedestal << entablature << envelope << Defects (tour);
				VoluteOptions vo;
				vo.segmentsPerQuarter = 2;
				for (const auto &bolster : BuildIonicVolutes (ionic.order, ionic.profile, vo))
				{
					const MeshValidationReport r = ValidateMesh (*bolster, one);
					EXPECT_TRUE (r.IsValid ()) << pedestal << entablature << envelope << Defects (r);
					EXPECT_GT (r.signedVolume, 0.0);
				}
			}
}

TEST (TEST_cgarchi_classical_volute, without_volutes_the_profile_is_unchanged)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		ColumnOptions plain;
		plain.order = static_cast<Order> (o);
		plain.pedestal = plain.entablature = true;
		ColumnOptions with = plain;
		with.ionicVolutes = true;
		const OrderData &order = ClassicalOrders ().Get (plain.order);
		const ColumnProfile a = BuildColumnProfile (order, plain);
		const ColumnProfile b = BuildColumnProfile (order, with);
		std::size_t differing = 0;
		ASSERT_EQ (a.vertices.size () == b.vertices.size () || plain.order == Order::Ionic, true);
		for (std::size_t i = 0; i < std::min (a.vertices.size (), b.vertices.size ()); ++i)
			if (a.vertices[i].y != b.vertices[i].y || a.vertices[i].ring != b.vertices[i].ring)
				++differing;
		if (plain.order == Order::Ionic)
			EXPECT_GT (differing + (a.vertices.size () != b.vertices.size () ? 1u : 0u), 0u);
		else
			EXPECT_EQ (differing, 0u) << OrderKey (plain.order);
	}
	// La bande des volutes : listel et canal au demi-cote F = 17,5.
	const Ionic ionic;
	std::size_t band = 0;
	for (const ProfileVertex &v : ionic.profile.vertices)
		if (v.member >= 0 && ionic.profile.members[v.member].plan == PlanForm::IonicOriented)
		{
			++band;
			EXPECT_EQ (v.ring, Ring::MakeSquare (17.5));
		}
	EXPECT_GT (band, 0u);
}

// VE6
TEST (TEST_cgarchi_classical_volute, the_parameterized_ionic_column_carries_its_two_bolsters)
{
	cgarchi::ParameterizedClassicalColumn column;
	std::vector<Parameter> params = column.GetParameters ();
	auto find = [&] (const char *name) -> Parameter & {
		for (Parameter &p : params)
			if (p.GetName () == name)
				return p;
		throw std::runtime_error (name);
	};
	EXPECT_TRUE (find ("Volutes").GetBool ());
	// Le toscan par defaut n'a pas de volute : son maillage est le tour seul.
	{
		ColumnOptions tuscan;
		tuscan.curveSegments = 6;
		tuscan.shaftSegments = 24;
		LatheOptions lathe;
		lathe.scale = 1.0 / 12.0;
		const ColumnProfile p = BuildColumnProfile (ClassicalOrders ().Get (Order::Tuscan), tuscan);
		EXPECT_EQ (column.GetMesh ()->GetNFaces (), LatheColumnProfile (p, lathe)->GetNFaces ());
	}

	find ("Order").SetInt (2);
	find ("Volutes").SetBool (false);
	auto t0 = std::chrono::steady_clock::now ();
	column.Regenerate ();
	auto t1 = std::chrono::steady_clock::now ();
	ASSERT_TRUE (column.LastError ().empty ()) << column.LastError ();
	const unsigned plain = column.GetMesh ()->GetNFaces ();
	const double plainMs = std::chrono::duration<double, std::milli> (t1 - t0).count ();

	find ("Volutes").SetBool (true);
	t0 = std::chrono::steady_clock::now ();
	column.Regenerate ();
	t1 = std::chrono::steady_clock::now ();
	ASSERT_TRUE (column.LastError ().empty ()) << column.LastError ();
	const unsigned with = column.GetMesh ()->GetNFaces ();
	const double withMs = std::chrono::duration<double, std::milli> (t1 - t0).count ();

	// Faces : tour (bande des volutes) + les deux coussinets, construits a part.
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	ColumnOptions options;
	options.order = Order::Ionic;
	options.ionicVolutes = true;
	options.curveSegments = 6;
	options.shaftSegments = 24;
	const ColumnProfile profile = BuildColumnProfile (ionic, options);
	LatheOptions lathe;
	lathe.scale = 1.0 / 18.0;
	unsigned expected = LatheColumnProfile (profile, lathe)->GetNFaces ();
	VoluteOptions vo;
	vo.scale = lathe.scale;
	for (const auto &bolster : BuildIonicVolutes (ionic, profile, vo))
		expected += bolster->GetNFaces ();
	EXPECT_EQ (with, expected);

	// Largeur en X : de -26 a 26 parties, soit 52/18 M (module 1) ; sans
	// volutes, la plinthe carree (50 parties) est le plus large.
	float lo[3], hi[3];
	Box (*column.GetMesh (), lo, hi);
	EXPECT_NEAR (hi[0] - lo[0], 52.0 / 18.0, 1e-4);
	EXPECT_NEAR (hi[1] - lo[1], 50.0 / 18.0, 1e-4);
	std::cout << "[ mesure ] ionique : " << plain << " triangles sans volutes (" << plainMs << " ms), " << with
	          << " avec (" << withMs << " ms)\n";
}
