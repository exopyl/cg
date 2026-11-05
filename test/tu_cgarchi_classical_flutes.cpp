#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_orders_data.h>
#include <cgarchi/classical_diminution.h>
#include <cgarchi/classical_flutes.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Conges du fut et cannelures
// ===========================================================================

using namespace cgarchi::classical;

namespace {

constexpr double kPi = 3.14159265358979323846;

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

void ExpectParseErrorNaming (std::string text, const std::string &from, const std::string &to, const std::string &path)
{
	const std::size_t at = text.find (from);
	ASSERT_NE (at, std::string::npos) << from;
	text.replace (at, from.size (), to);
	try
	{
		ParseClassicalOrders (text);
		FAIL () << "accepte : " << from << " -> " << to;
	}
	catch (const std::runtime_error &e)
	{
		EXPECT_NE (std::string (e.what ()).find (path), std::string::npos) << e.what ();
	}
}

}  // namespace

// C0
TEST (TEST_cgarchi_classical_flutes, the_shaft_fillet_rule_and_the_flutes_are_read)
{
	const OrdersData &data = ClassicalOrders ();
	EXPECT_NE (data.shaftFilletRule.find ("quart de cercle"), std::string::npos);
	EXPECT_FALSE (data.Get (Order::Tuscan).flutes.has_value ());

	const FluteData &doric = *data.Get (Order::Doric).flutes;
	EXPECT_EQ (doric.count, 20);
	EXPECT_FALSE (doric.listels);
	ASSERT_EQ (doric.sections.size (), 2u);
	EXPECT_EQ (doric.sections[0], FluteSection::Segment60);
	EXPECT_EQ (doric.sections[1], FluteSection::Semicircle);

	for (Order o : { Order::Ionic, Order::Corinthian, Order::Composite })
	{
		const OrderData &order = data.Get (o);
		ASSERT_TRUE (order.flutes.has_value ()) << order.key;
		const FluteData &f = *order.flutes;
		EXPECT_EQ (f.count, 24) << order.key;
		EXPECT_TRUE (f.listels) << order.key;
		ASSERT_EQ (f.sections.size (), 1u);
		EXPECT_EQ (f.sections[0], FluteSection::Semicircle);
		EXPECT_EQ (f.footListel, 0.8);
		EXPECT_EQ (f.footFlute, 3.9);
		EXPECT_EQ (f.footDepth, 1.95);
		// Coherence de au_pied avec la regle : pas pi 36/24 ; listel 0,17 du
		// pas ; cannelure = pas - listel a la mesure pres ; profondeur = demi
		// cannelure.
		const double pitch = kPi * order.general.lowerDiameter_p / f.count;
		EXPECT_NEAR (pitch, 4.71, 0.005);
		EXPECT_NEAR (f.footListel / pitch, 0.17, 0.005);
		EXPECT_NEAR (f.footFlute, pitch - f.footListel, 0.02);
		EXPECT_EQ (f.footDepth, f.footFlute / 2.0);
	}
}

TEST (TEST_cgarchi_classical_flutes, a_missing_flute_or_fillet_key_is_named)
{
	const std::string text (cgarchi::ClassicalOrdersJson ());
	ExpectParseErrorNaming (text, "\"conges_du_fut\"", "\"conges_du_fu\"", "conventions/conges_du_fut");
	ExpectParseErrorNaming (text, "\"profondeur\": \"39/20\"", "\"profondeu\": \"39/20\"",
	                        "ornements/cannelures/au_pied/profondeur");
	ExpectParseErrorNaming (text, "\"separees_par\": \"listels\"", "\"separees_par\": \"rien\"",
	                        "ornements/cannelures/separees_par");
	ExpectParseErrorNaming (text, "\"nombre\": 20", "\"nombre\": 0", "dorique/ornements/cannelures/nombre");
	// Le tour suppose une section a symetrie d'ordre 4.
	ExpectParseErrorNaming (text, "\"nombre\": 20", "\"nombre\": 18", "dorique/ornements/cannelures/nombre");
	ExpectParseErrorNaming (text, "\"nombre\": 24", "\"nombre\": 22", "ionique/ornements/cannelures/nombre");
}

// ===========================================================================
//  C1 : conges du fut
// ===========================================================================

namespace {

// Raccord du fut a ses listels, du coin exterieur du listel du pied (premier
// sommet a y = foot) au coin exterieur du listel du haut (dernier sommet a
// y = top) : plus grand coude aux sommets intermediaires, et ecart a
// l'horizontale du premier et du dernier segment (tangence aux listels).
struct Junction
{
	double maxTurn = 0.0;
	double footSlope = 0.0;
	double topSlope = 0.0;
};

Junction MeasureJunction (const ColumnProfile &p)
{
	std::size_t i0 = 0, i1 = 0;
	for (std::size_t i = 0; i < p.vertices.size (); ++i)
	{
		if (i0 == 0 && std::fabs (p.vertices[i].y - p.shaft.foot) < 1e-9)
			i0 = i;
		if (std::fabs (p.vertices[i].y - p.shaft.top) < 1e-9)
			i1 = i;
	}
	auto angle = [&] (std::size_t a, std::size_t b) {
		return std::atan2 (p.vertices[b].y - p.vertices[a].y, p.vertices[b].ring.radius - p.vertices[a].ring.radius);
	};
	Junction j;
	for (std::size_t i = i0 + 1; i < i1; ++i)
	{
		double d = std::fabs (angle (i, i + 1) - angle (i - 1, i));
		if (d > kPi) d = 2.0 * kPi - d;
		j.maxTurn = std::max (j.maxTurn, d);
	}
	// Segments horizontaux : angle 0 ou pi.
	j.footSlope = std::fabs (std::sin (angle (i0, i0 + 1)));
	j.topSlope = std::fabs (std::sin (angle (i1 - 1, i1)));
	return j;
}

}  // namespace

TEST (TEST_cgarchi_classical_flutes, the_shaft_meets_its_listels_through_tangent_quarter_circles)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		const OrderData &order = ClassicalOrders ().Get (static_cast<Order> (o));
		ColumnOptions options;
		options.order = order.order;
		options.curveSegments = 32;
		options.shaftSegments = 64;
		const ColumnProfile p = BuildColumnProfile (order, options);
		const ShaftZone &z = p.shaft;
		const ShaftProfile shaft (order);
		const double P = order.partsPerModule;

		// Rayon = saillie du listel sur le vif local, pris dans fut_M.
		EXPECT_GT (z.footFillet, 0.0) << order.key;
		EXPECT_GT (z.topFillet, 0.0) << order.key;
		EXPECT_NEAR (z.vifBottom - z.foot, z.footFillet, 1e-12) << order.key;
		EXPECT_NEAR (z.top - z.vifTop, z.topFillet, 1e-12) << order.key;
		EXPECT_NEAR (p.height, order.general.column_M * P, 1e-9) << order.key;

		// Tangence : a 32 segments par quart de cercle, aucun coude au-dela
		// d'un pas angulaire (2,8 degres) entre les coins des deux listels, et
		// des segments extremes presque horizontaux.
		const Junction j = MeasureJunction (p);
		EXPECT_LT (j.maxTurn, 1.6 * (kPi / 2.0) / 32.0) << order.key;
		EXPECT_LT (j.footSlope, 0.05) << order.key;
		EXPECT_LT (j.topSlope, 0.05) << order.key;
		std::cout << "[ mesure ] " << order.key << " : conge du pied " << z.footFillet << " p, du haut " << z.topFillet << " p\n";
	}
	// Ionique : conge de 2,4 p mesure sur la pl. XVIII ; la regle (saillie 7/3
	// sur le vif local, galbe compris) donne 2,29 p.
	ColumnOptions ionicOptions;
	ionicOptions.order = Order::Ionic;
	const ColumnProfile ionic = BuildColumnProfile (ClassicalOrders ().Get (Order::Ionic), ionicOptions);
	EXPECT_NEAR (ionic.shaft.footFillet, 2.4, 0.15);
}

TEST (TEST_cgarchi_classical_flutes, without_fillets_the_shaft_turns_square_at_its_listels)
{
	// Negatif : sans conge, l'anneau plat fait un coude de 90 degres.
	ColumnOptions options;
	options.curveSegments = 32;
	options.shaftFillets = false;
	const ColumnProfile p = BuildColumnProfile (ClassicalOrders ().Get (Order::Tuscan), options);
	EXPECT_NEAR (MeasureJunction (p).maxTurn, kPi / 2.0, 0.05);
	EXPECT_EQ (p.shaft.footFillet, 0.0);
}

// ===========================================================================
//  F1 : section en plan
// ===========================================================================

namespace {

double Pitch (int count) { return 2.0 * kPi / count; }

// Aire retiree au disque de rayon R par les cannelures, par integration
// polaire fine : somme de (R^2 - r^2) / 2 dtheta.
double GrooveArea (double R, int count, double beta, FluteSection section, int samples)
{
	double area = 0.0;
	const double step = 2.0 * kPi / samples;
	for (int i = 0; i < samples; ++i)
	{
		const double r = FlutedRadius (R, count, beta, section, (i + 0.5) * step);
		area += 0.5 * (R * R - r * r) * step;
	}
	return area;
}

// Meme aire sur les seuls sommets d'une grille (polygone etoile).
double GrooveAreaOnGrid (double R, int count, double beta, FluteSection section, int m)
{
	std::vector<double> angles;
	for (int j = 0; j < count; ++j)
		for (double o : FluteGridOffsets (beta, section, m, 0))
			for (double side : { -1.0, 1.0 })
				angles.push_back (std::fmod (Pitch (count) * j + side * o + 2.0 * kPi, 2.0 * kPi));
	std::sort (angles.begin (), angles.end ());
	angles.erase (std::unique (angles.begin (), angles.end (), [] (double a, double b) { return b - a < 1e-12; }),
	              angles.end ());
	double polygon = 0.0;
	for (std::size_t i = 0; i < angles.size (); ++i)
	{
		const double a = angles[i], b = i + 1 < angles.size () ? angles[i + 1] : angles[0] + 2.0 * kPi;
		polygon += 0.5 * FlutedRadius (R, count, beta, section, a) * FlutedRadius (R, count, beta, section, b) * std::sin (b - a);
	}
	return kPi * R * R - polygon;
}

// Nombre de creux : minima locaux stricts du rayon sur le tour.
int CountHollows (double R, int count, double beta, FluteSection section)
{
	const int n = 20000;
	auto r = [&] (int i) { return FlutedRadius (R, count, beta, section, 2.0 * kPi * ((i % n + n) % n) / n); };
	int hollows = 0;
	for (int i = 0; i < n; ++i)
		if (r (i) < r (i - 1) && r (i) <= r (i + 1))
			++hollows;
	return hollows;
}

}  // namespace

TEST (TEST_cgarchi_classical_flutes, each_section_has_its_count_of_hollows_and_its_proportions)
{
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	const OrderData &doric = ClassicalOrders ().Get (Order::Doric);
	const double bi = FluteHalfAngle (ionic), bd = FluteHalfAngle (doric);
	EXPECT_EQ (CountHollows (18.0, 24, bi, FluteSection::Semicircle), 24);
	EXPECT_EQ (CountHollows (12.0, 20, bd, FluteSection::Segment60), 20);
	EXPECT_EQ (CountHollows (12.0, 20, bd, FluteSection::Semicircle), 20);

	// Toutes les aretes sont sur le cercle du fut, des deux cotes de chaque
	// cannelure.
	for (int j = 0; j < 24; ++j)
		for (double side : { -1.0, 1.0 })
			EXPECT_EQ (FlutedRadius (18.0, 24, bi, FluteSection::Semicircle, Pitch (24) * j + side * bi), 18.0) << j;
	for (int j = 0; j < 20; ++j)
		for (FluteSection section : { FluteSection::Segment60, FluteSection::Semicircle })
			EXPECT_EQ (FlutedRadius (12.0, 20, bd, section, Pitch (20) * j + bd), 12.0) << j;

	// Listel : 0,17 du pas, soit 4/5 au pied (rayon 18).
	EXPECT_NEAR ((Pitch (24) - 2.0 * bi) / Pitch (24), 0.8 / (kPi * 36.0 / 24.0), 1e-12);
	EXPECT_NEAR (18.0 * (Pitch (24) - 2.0 * bi), 0.8, 1e-12);
	EXPECT_NEAR (bd, kPi / 20.0, 1e-15);   // jointives : corde = 2 r sin 9 degres

	// Au pied : cannelure 39/10 (arc ; corde 3,904), profondeur sous la corde =
	// demi-corde (39/20 a la mesure pres).
	const double chord = 2.0 * 18.0 * std::sin (bi);
	EXPECT_NEAR (18.0 * 2.0 * bi, 3.9, 0.02);
	EXPECT_NEAR (chord, 3.9, 0.01);
	const double depth = 18.0 * std::cos (bi) - FlutedRadius (18.0, 24, bi, FluteSection::Semicircle, 0.0);
	EXPECT_NEAR (depth, chord / 2.0, 1e-12);
	EXPECT_NEAR (depth, 1.95, 0.01);
	std::cout << "[ mesure ] ionique au pied : corde " << chord << ", arc " << 36.0 * bi << ", profondeur " << depth << '\n';

	// Profondeur proportionnelle au rayon local (beta fixe), sur trois rayons.
	for (double R : { 15.0, 16.5, 18.0 })
	{
		const double d = R * std::cos (bi) - FlutedRadius (R, 24, bi, FluteSection::Semicircle, 0.0);
		EXPECT_NEAR (d / R, depth / 18.0, 1e-12) << R;
	}
	// Negatif : une cannelure de largeur absolue figee (beta = w / R) n'est
	// pas proportionnelle.
	const double w = 3.9;
	const double d15 = 15.0 * std::cos (w / 30.0) - FlutedRadius (15.0, 24, w / 30.0, FluteSection::Semicircle, 0.0);
	EXPECT_GT (std::fabs (d15 / 15.0 - depth / 18.0), 1e-3);
}

TEST (TEST_cgarchi_classical_flutes, doric_a_is_a_hollow_60_degree_arc)
{
	const double R = 12.0, b = kPi / 20.0, chord = 2.0 * R * std::sin (b);
	const double sagitta = R * std::cos (b) - FlutedRadius (R, 20, b, FluteSection::Segment60, 0.0);
	EXPECT_NEAR (sagitta, chord * kFluteSegment60Sagitta, 1e-12);
	EXPECT_GT (sagitta, 0.0);   // creuse : sous la corde
	// Arete sur le cercle du fut, rayon de l'arc = corde.
	EXPECT_NEAR (FlutedRadius (R, 20, b, FluteSection::Segment60, b), R, 1e-12);
	// Negatif : le centre « vers l'axe » pris a la lettre bombe l'arc au-dessus
	// de la corde.
	EXPECT_GT (Segment60TowardAxisRadius (R, b, 0.0), R * std::cos (b));
	EXPECT_NEAR (Segment60TowardAxisRadius (R, b, 0.0) - R * std::cos (b), chord * kFluteSegment60Sagitta, 1e-12);
	EXPECT_NEAR (Segment60TowardAxisRadius (R, b, b), R, 1e-12);
}

TEST (TEST_cgarchi_classical_flutes, the_clipped_semicircle_differs_from_the_exact_one_by_its_undercut_only)
{
	// Ecart a la forme demandee : le demi-cercle exact a, de chaque cote, une
	// bande en surplomb sous l'arete ; l'ecretage la laisse pleine.
	for (int order : { 0, 1 })
	{
		const double R = order ? 12.0 : 18.0;
		const int N = order ? 20 : 24;
		const double b = order ? kPi / 20.0 : FluteHalfAngle (ClassicalOrders ().Get (Order::Ionic));
		const double chord = 2.0 * R * std::sin (b);
		// Aire exacte du demi-cercle sous la corde + segment du disque au-dela.
		const double exact = N * (0.125 * kPi * chord * chord + 0.5 * R * R * (2.0 * b - std::sin (2.0 * b)));
		const double clipped = GrooveArea (R, N, b, FluteSection::Semicircle, 4000000);
		const double undercut = std::asin (std::tan (b)) - b;
		std::cout << "[ mesure ] " << (order ? "dorique B" : "ionique") << " : surplomb " << undercut
		          << " rad, aire exacte " << exact << ", ecretee " << clipped << ", ecart relatif "
		          << (exact - clipped) / exact << '\n';
		EXPECT_GT (exact, clipped);
		EXPECT_LT ((exact - clipped) / exact, 0.01);
	}
}

TEST (TEST_cgarchi_classical_flutes, the_sampled_groove_area_converges)
{
	const double b = FluteHalfAngle (ClassicalOrders ().Get (Order::Ionic));
	const double reference = GrooveArea (18.0, 24, b, FluteSection::Semicircle, 4000000);
	double previous = 1e300;
	for (int m : { 2, 4, 8, 16 })
	{
		const double error = std::fabs (GrooveAreaOnGrid (18.0, 24, b, FluteSection::Semicircle, m) - reference) / reference;
		EXPECT_LT (error, previous) << m;
		previous = error;
	}
	EXPECT_LT (previous, 0.01);
}

// ===========================================================================
//  F2 : angles du tour
// ===========================================================================

TEST (TEST_cgarchi_classical_flutes, the_lathe_grid_holds_edges_bottoms_and_square_corners)
{
	for (int o : { 1, 2 })   // dorique, ionique
	{
		const Order order = static_cast<Order> (o);
		const OrderData &data = ClassicalOrders ().Get (order);
		ColumnOptions options;
		options.order = order;
		options.flutes = true;
		const ColumnProfile p = BuildColumnProfile (data, options);
		ASSERT_TRUE (p.flutes.has_value ());
		const std::vector<double> angles = LatheAngles (p, LatheOptions {});
		auto has = [&] (double t) {
			t = std::fmod (t + 2.0 * kPi, 2.0 * kPi);
			for (double a : angles)
				if (std::fabs (a - t) < 1e-9 || std::fabs (a - t - 2.0 * kPi) < 1e-9 || std::fabs (a + 2.0 * kPi - t) < 1e-9)
					return true;
			return false;
		};
		const double b = p.flutes->halfAngle;
		for (int j = 0; j < p.flutes->count; ++j)
		{
			const double c = Pitch (p.flutes->count) * j;
			EXPECT_TRUE (has (c)) << j;
			EXPECT_TRUE (has (c - b)) << j;
			EXPECT_TRUE (has (c + b)) << j;
		}
		for (int k = 0; k < 8; ++k)
			EXPECT_TRUE (has (kPi * k / 4.0)) << k;
		std::cout << "[ mesure ] " << data.key << " : " << angles.size () << " angles (m = " << p.flutes->samples << ")\n";
		// Negatif : la grille reguliere du tour sans cannelures (48 angles)
		// manque les aretes.
		int missing = 0;
		const int n = 48;
		for (int j = 0; j < p.flutes->count; ++j)
		{
			const double e = Pitch (p.flutes->count) * j + b;
			bool found = false;
			for (int i = 0; i < n; ++i)
				found = found || std::fabs (2.0 * kPi * i / n - e) < 1e-9;
			missing += found ? 0 : 1;
		}
		EXPECT_GT (missing, 0);
	}
}

// ===========================================================================
//  F3 : profil, tetes et pied
// ===========================================================================

TEST (TEST_cgarchi_classical_flutes, flutes_off_or_absent_leave_the_profile_unchanged)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		ColumnOptions plain;
		plain.order = static_cast<Order> (o);
		ColumnOptions fluted = plain;
		fluted.flutes = true;
		const OrderData &data = ClassicalOrders ().Get (plain.order);
		const ColumnProfile a = BuildColumnProfile (data, plain);
		const ColumnProfile b = BuildColumnProfile (data, fluted);
		EXPECT_FALSE (a.flutes.has_value ());
		if (plain.order == Order::Tuscan)
		{
			ASSERT_EQ (a.vertices.size (), b.vertices.size ());
			for (std::size_t i = 0; i < a.vertices.size (); ++i)
			{
				EXPECT_EQ (a.vertices[i].y, b.vertices[i].y);
				EXPECT_EQ (a.vertices[i].ring, b.vertices[i].ring);
			}
			EXPECT_FALSE (b.flutes.has_value ());
		}
		else
			EXPECT_TRUE (b.flutes.has_value ()) << data.key;
	}
}

TEST (TEST_cgarchi_classical_flutes, the_heads_are_semicircles_tangent_to_the_upper_fillet)
{
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	ColumnOptions options;
	options.order = Order::Ionic;
	options.flutes = true;
	options.fluteHeadRings = 3;
	const ColumnProfile p = BuildColumnProfile (ionic, options);
	const FluteGrid &g = *p.flutes;
	ASSERT_EQ (g.headHalfAngles.size (), 3u);
	const double top = p.shaft.vifTop, head = top - g.headBottom;
	double topRadius = 0.0;
	for (const ProfileVertex &v : p.vertices)
		if (std::fabs (v.y - top) < 1e-9 && v.ring.shape == RingShape::Circle)
			topRadius = v.ring.radius;
	ASSERT_GT (topRadius, 0.0);
	std::vector<const ProfileVertex *> heads;
	for (const ProfileVertex &v : p.vertices)
		if (v.y > g.headBottom + 1e-9 && v.y < top - 1e-9)
			heads.push_back (&v);
	ASSERT_EQ (heads.size (), 3u);
	for (std::size_t i = 0; i < heads.size (); ++i)
	{
		const ProfileVertex &v = *heads[i];
		ASSERT_EQ (v.ring.shape, RingShape::Fluted);
		EXPECT_EQ (v.ring.halfAngle, g.headHalfAngles[i]);
		// Arete et pied de paroi de l'anneau : angles de la grille.
		EXPECT_TRUE (std::binary_search (g.offsets.begin (), g.offsets.end (), v.ring.halfAngle)) << i;
		bool partner = false;
		for (double o : g.offsets)
			partner = partner || std::fabs (o - v.ring.halfAngle * (1.0 - kFluteWallGap)) < 1e-15;
		EXPECT_TRUE (partner) << i;
		EXPECT_EQ (v.ring.RadiusAt (v.ring.halfAngle), v.ring.radius);
		// Le point d'arete (demi-largeur developpee, hauteur) est sur le
		// demi-cercle de la tete, de rayon R(vifTop) beta.
		const double x = topRadius * v.ring.halfAngle, y = v.y - g.headBottom;
		EXPECT_NEAR (std::hypot (x, y), head, 1e-9 * head) << i;
	}
	// Sommet de la tete au pied du conge superieur : anneau rond.
	bool topRound = false;
	for (const ProfileVertex &v : p.vertices)
		topRound = topRound || (std::fabs (v.y - top) < 1e-9 && v.ring.shape == RingShape::Circle);
	EXPECT_TRUE (topRound);
	// Pied : arret droit, anneau rond puis anneau cannele a la meme hauteur.
	int atFoot = 0;
	for (const ProfileVertex &v : p.vertices)
		atFoot += std::fabs (v.y - p.shaft.vifBottom) < 1e-9 ? 1 : 0;
	EXPECT_EQ (atFoot, 2);

	// Negatif : des anneaux de tete a pas uniforme ne portent pas les aretes sur
	// le demi-cercle.
	double worst = 0.0;
	for (std::size_t i = 0; i < 3; ++i)
	{
		const double y = head * (i + 1) / 4.0;
		worst = std::max (worst, std::fabs (std::hypot (heads[0]->ring.radius * g.headHalfAngles[i], y) - head));
	}
	EXPECT_GT (worst, 1e-2 * head);
}

// ===========================================================================
//  Precision de la section echantillonnee, a la resolution par defaut
// ===========================================================================

namespace {

struct SectionError
{
	double radial = 0.0;   // ecart radial max / profondeur de la cannelure pleine
	double area = 0.0;     // ecart relatif de l'aire creusee
};

// Section d'un anneau cannele lue sur les angles `angles` (interpolation
// lineaire du rayon), comparee a FlutedRadius : ecart radial hors des bandes de
// paroi (|offset| dans ]halfAngle (1 - kFluteWallGap), halfAngle[, ou la paroi
// radiale est portee par ses deux meridiens), et aire creusee.
SectionError MeasureSection (const Ring &ring, const std::vector<double> &angles, double depth)
{
	auto exact = [&] (double t) { return ring.RadiusAt (t); };
	const double pitch = 2.0 * kPi / ring.flutes;
	SectionError e;
	const int n = 200000;
	double exactArea = 0.0, gridArea = 0.0;
	std::size_t k = 0;
	for (int i = 0; i < n; ++i)
	{
		const double t = 2.0 * kPi * (i + 0.5) / n;
		while (k + 1 < angles.size () && angles[k + 1] <= t)
			++k;
		const double a0 = angles[k], a1 = k + 1 < angles.size () ? angles[k + 1] : angles[0] + 2.0 * kPi;
		const double f = (t - a0) / (a1 - a0);
		const double r = exact (a0) + f * (exact (a1) - exact (a0));
		const double offset = std::fabs (t - std::round (t / pitch) * pitch);
		const bool wall = offset > ring.halfAngle * (1.0 - kFluteWallGap) && offset < ring.halfAngle;
		if (!wall)
			e.radial = std::max (e.radial, std::fabs (r - exact (t)) / depth);
		exactArea += 0.5 * (ring.radius * ring.radius - exact (t) * exact (t)) * (2.0 * kPi / n);
	}
	for (std::size_t i = 0; i < angles.size (); ++i)
	{
		const double a = angles[i], b = i + 1 < angles.size () ? angles[i + 1] : angles[0] + 2.0 * kPi;
		gridArea += 0.5 * exact (a) * exact (b) * std::sin (b - a);
	}
	e.area = std::fabs ((kPi * ring.radius * ring.radius - gridArea) - exactArea) / exactArea;
	return e;
}

}  // namespace

TEST (TEST_cgarchi_classical_flutes, the_sampled_sections_follow_the_exact_ones_at_the_default_resolution)
{
	// Tolerances : 5 % de la profondeur en rayon, 1 % en aire creusee.
	for (int o : { 1, 2, 3, 4 })
		for (int section = 0; section < (o == 1 ? 2 : 1); ++section)
		{
			cgarchi::ParameterizedClassicalColumn column;
			ColumnOptions options;
			options.order = static_cast<Order> (o);
			options.flutes = true;
			options.doricFlutes = section ? FluteSection::Semicircle : FluteSection::Segment60;
			const OrderData &data = ClassicalOrders ().Get (options.order);
			const ColumnProfile p = BuildColumnProfile (data, options);
			const std::vector<double> angles = LatheAngles (p, LatheOptions {});
			double worstRadial = 0.0, worstArea = 0.0, worstHead = 0.0;
			for (const ProfileVertex &v : p.vertices)
			{
				if (v.ring.shape != RingShape::Fluted)
					continue;
				const Ring full = Ring::MakeFluted (v.ring.radius, p.flutes->count, p.flutes->halfAngle, p.flutes->section);
				const double depth = v.ring.radius - full.RadiusAt (0.0);
				const SectionError e = MeasureSection (v.ring, angles, depth);
				if (v.ring.halfAngle == p.flutes->halfAngle)
				{
					worstRadial = std::max (worstRadial, e.radial);
					worstArea = std::max (worstArea, e.area);
				}
				else
					worstHead = std::max (worstHead, e.radial);
			}
			EXPECT_LT (worstRadial, 0.05) << data.key << section;
			EXPECT_LT (worstArea, 0.01) << data.key << section;
			// Tetes : section reduite echantillonnee sur la grille de la cannelure
			// pleine, plus grossiere a son echelle.
			EXPECT_LT (worstHead, 0.15) << data.key << section;
			std::cout << "[ mesure ] " << data.key << (o == 1 ? (section ? " B" : " A") : "") << " : " << angles.size ()
			          << " angles ; vif : ecart radial max " << 100.0 * worstRadial << " % de la profondeur, aire "
			          << 100.0 * worstArea << " % ; tetes : " << 100.0 * worstHead << " %\n";
		}

	// Negatif : la grille uniforme beta k / 3 d'avant etale la paroi en
	// chanfrein (ecart radial de l'ordre de 30 % de la profondeur).
	const double b = FluteHalfAngle (ClassicalOrders ().Get (Order::Ionic));
	const Ring ring = Ring::MakeFluted (18.0, 24, b, FluteSection::Semicircle);
	std::vector<double> uniform;
	for (int j = 0; j < 24; ++j)
		for (int k = -3; k <= 3; ++k)
			uniform.push_back (std::fmod (Pitch (24) * j + b * k / 3.0 + 2.0 * kPi, 2.0 * kPi));
	std::sort (uniform.begin (), uniform.end ());
	const SectionError bad = MeasureSection (ring, uniform, 18.0 - ring.RadiusAt (0.0));
	EXPECT_GT (bad.radial, 0.2);
	EXPECT_GT (bad.area, 0.05);
}

TEST (TEST_cgarchi_classical_flutes, every_fluted_ring_is_on_the_shaft_circle_at_and_beyond_its_edges)
{
	// Sur chaque angle du tour et chaque anneau cannele du profil (tetes
	// comprises) : r = R des que |offset| >= demi-angle de l'anneau.
	for (int o : { 1, 2 })
		for (int section = 0; section < (o == 1 ? 2 : 1); ++section)
		{
			ColumnOptions options;
			options.order = static_cast<Order> (o);
			options.flutes = true;
			options.doricFlutes = section ? FluteSection::Semicircle : FluteSection::Segment60;
			const OrderData &data = ClassicalOrders ().Get (options.order);
			const ColumnProfile p = BuildColumnProfile (data, options);
			const std::vector<double> angles = LatheAngles (p, LatheOptions {});
			std::size_t checked = 0, heads = 0;
			for (const ProfileVertex &v : p.vertices)
			{
				if (v.ring.shape != RingShape::Fluted)
					continue;
				heads += v.ring.halfAngle < p.flutes->halfAngle ? 1 : 0;
				const double pitch = 2.0 * kPi / v.ring.flutes;
				for (double t : angles)
				{
					const double offset = std::fabs (t - std::round (t / pitch) * pitch);
					// Arete exacte de la grille, a l'arrondi du recalcul pres.
					if (offset >= v.ring.halfAngle - 1e-12)
					{
						++checked;
						EXPECT_EQ (v.ring.RadiusAt (t), v.ring.radius) << data.key << " " << t;
					}
				}
			}
			EXPECT_GT (heads, 0u);
			EXPECT_GT (checked, 0u);
		}
}

TEST (TEST_cgarchi_classical_flutes, an_edge_angle_is_never_replaced_by_a_nearby_crossing)
{
	const double edge = 0.1086770000;
	const std::vector<double> merged = MergeAngles ({0.0, edge, 1.0}, {edge - 5e-10, edge + 2e-10, 0.5});
	ASSERT_EQ (merged.size (), 4u);
	EXPECT_EQ (merged[1], edge);
	EXPECT_EQ (merged[2], 0.5);
	// Un croisement a plus de la tolerance est garde.
	EXPECT_EQ (MergeAngles ({0.0, edge}, {edge - 1e-6}).size (), 3u);
}

// ===========================================================================
//  F4 : maillages
// ===========================================================================

namespace {

// `allBlocks` : toutes les combinaisons piedestal / entablement, sinon la
// colonne seule (ils n'interagissent pas avec les cannelures).
void ExpectFlutedConfigurationsValid (int curveSegments, int shaftSegments, bool allBlocks)
{
	for (int o : { 1, 2, 3, 4 })
		for (int section = 0; section < (o == 1 ? 2 : 1); ++section)
			for (int pedestal = 0; pedestal < (allBlocks ? 2 : 1); ++pedestal)
				for (int entablature = 0; entablature < (allBlocks ? 2 : 1); ++entablature)
				{
					ColumnOptions options;
					options.order = static_cast<Order> (o);
					options.flutes = true;
					options.doricFlutes = section ? FluteSection::Semicircle : FluteSection::Segment60;
					options.pedestal = pedestal != 0;
					options.entablature = entablature != 0;
					options.ionicVolutes = true;
					options.curveSegments = curveSegments;
					options.shaftSegments = shaftSegments;
					const OrderData &data = ClassicalOrders ().Get (options.order);
					const ColumnProfile p = BuildColumnProfile (data, options);
					LatheOptions lathe;
					lathe.scale = 1.0 / data.partsPerModule;
					const std::unique_ptr<Mesh> mesh = LatheColumnProfile (p, lathe);
					MeshValidationOptions one;
					one.expectedComponents = 1u;
					const MeshValidationReport r = ValidateMesh (*mesh, one);
					EXPECT_TRUE (r.IsValid ()) << data.key << " " << section << pedestal << entablature << " :" << Defects (r);
					EXPECT_GT (r.signedVolume, 0.0);
				}
}

}  // namespace

TEST (TEST_cgarchi_classical_flutes, every_fluted_configuration_lathes_into_a_valid_solid)
{
	ExpectFlutedConfigurationsValid (2, 8, false);
}

// Resolution par defaut. Label `slow` (test/slow_tests.cmake).
TEST (TEST_cgarchi_classical_flutes, every_fluted_configuration_lathes_into_a_valid_solid_at_the_default_resolution)
{
	ExpectFlutedConfigurationsValid (6, 24, true);
}

// ===========================================================================
//  F5 : forme parametrique
// ===========================================================================

TEST (TEST_cgarchi_classical_flutes, the_parameterized_column_exposes_flutes_unchecked_by_default)
{
	cgarchi::ParameterizedClassicalColumn column;
	std::vector<Parameter> params = column.GetParameters ();
	auto find = [&] (const char *name) -> Parameter & {
		for (Parameter &p : params)
			if (p.GetName () == name)
				return p;
		throw std::runtime_error (name);
	};
	EXPECT_FALSE (find ("Flutes").GetBool ());
	EXPECT_EQ (find ("Doric flutes").GetInt (), 0);
	ASSERT_EQ (find ("Doric flutes").GetChoices ().size (), 2u);

	// Toscan : sans cannelures, la case n'y change rien.
	const unsigned tuscan = column.GetMesh ()->GetNFaces ();
	find ("Flutes").SetBool (true);
	column.Regenerate ();
	EXPECT_EQ (column.GetMesh ()->GetNFaces (), tuscan);

	auto measure = [&] (int order, bool flutes, int doric, const char *label) {
		find ("Order").SetInt (order);
		find ("Flutes").SetBool (flutes);
		find ("Doric flutes").SetInt (doric);
		const auto t0 = std::chrono::steady_clock::now ();
		column.Regenerate ();
		const auto t1 = std::chrono::steady_clock::now ();
		EXPECT_TRUE (column.LastError ().empty ()) << label << " : " << column.LastError ();
		std::cout << "[ mesure ] " << label << " : " << column.GetMesh ()->GetNFaces () << " triangles, Regenerate "
		          << std::chrono::duration<double, std::milli> (t1 - t0).count () << " ms (Debug)\n";
		return column.GetMesh ()->GetNFaces ();
	};
	const unsigned ionicPlain = measure (2, false, 0, "ionique a volutes, lisse");
	const unsigned ionicFluted = measure (2, true, 0, "ionique a volutes, cannele");
	EXPECT_GT (ionicFluted, ionicPlain);
	const unsigned doricA = measure (1, true, 0, "dorique A");
	const unsigned doricB = measure (1, true, 1, "dorique B");
	measure (1, false, 0, "dorique lisse");
	measure (3, true, 0, "corinthien cannele");
	// B a des meridiens de paroi que A (arc sans surplomb) n'a pas.
	EXPECT_GT (doricB, doricA);
}
