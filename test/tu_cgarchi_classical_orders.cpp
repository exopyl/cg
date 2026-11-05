#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_orders_data.h>
#include <cgarchi/classical_diminution.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Cinq ordres : lecture des donnees et controles (T1)
// ===========================================================================

using namespace cgarchi::classical;

namespace {

// Remplace la premiere occurrence de `from` par `to` ; echoue si absente.
std::string ReplaceOnce (std::string text, const std::string &from, const std::string &to)
{
	const std::size_t at = text.find (from);
	EXPECT_NE (at, std::string::npos) << from;
	if (at != std::string::npos)
		text.replace (at, from.size (), to);
	return text;
}

}  // namespace

TEST (TEST_cgarchi_classical_orders, the_embedded_data_parses_and_carries_the_five_orders)
{
	const OrdersData &data = ClassicalOrders ();
	EXPECT_EQ (data.version, "2026-11-04");
	EXPECT_EQ (data.controls.size (), 61u);
	const int ppm[kOrderCount] = { 12, 12, 18, 18, 18 };
	for (int i = 0; i < kOrderCount; ++i)
	{
		const OrderData &o = data.orders[i];
		EXPECT_EQ (o.partsPerModule, ppm[i]) << o.key;
		EXPECT_NE (o.FindBlock ("base"), nullptr) << o.key;
		EXPECT_NE (o.FindBlock ("fut_haut"), nullptr) << o.key;
		EXPECT_FALSE (o.FindBlock ("fut_haut")->totalHeight.has_value ()) << o.key;
	}
	EXPECT_NE (data.Get (Order::Doric).FindBlock ("corniche_denticules"), nullptr);
	EXPECT_NE (data.Get (Order::Doric).FindBlock ("corniche_mutules"), nullptr);
	EXPECT_EQ (data.Get (Order::Doric).FindBlock ("corniche"), nullptr);
	EXPECT_EQ (data.Get (Order::Tuscan).blocks.front ().key, "corniche");
	EXPECT_EQ (data.Get (Order::Tuscan).blocks.back ().key, "piedestal");
	EXPECT_NEAR (data.conchoidCDPlate_M, 29.0 / 27.0, 1e-15);
}

TEST (TEST_cgarchi_classical_orders, every_projection_variant_of_the_data_is_read)
{
	// Inventaire des structures de saillie et des plans de la version
	// 2026-11-04 (classical_orders_implementation.md, section 3).
	std::size_t members = 0;
	std::set<ProjectionKind> kinds;
	std::set<PlanForm> plans;
	std::size_t twoCutsSingle = 0;
	for (const OrderData &o : ClassicalOrders ().orders)
		for (const Block &b : o.blocks)
			for (const Member &m : b.members)
			{
				++members;
				kinds.insert (m.projection.kind);
				plans.insert (m.plan);
				// carre_concave a coupe unique : canal de la volute composite.
				if (m.plan == PlanForm::ConcaveSquare && m.projection.kind != ProjectionKind::TwoCuts)
					++twoCutsSingle;
			}
	EXPECT_EQ (members, 239u);
	EXPECT_EQ (kinds.size (), 6u);
	EXPECT_EQ (plans.size (), 5u);
	EXPECT_EQ (twoCutsSingle, 1u);
}

TEST (TEST_cgarchi_classical_orders, an_unknown_projection_key_set_is_refused)
{
	const std::string text (cgarchi::ClassicalOrdersJson ());
	// Positif : le texte d'origine passe.
	EXPECT_NO_THROW (ParseClassicalOrders (text));
	// Une cle de saillie renommee, une forme inconnue, un plan inconnu.
	EXPECT_THROW (ParseClassicalOrders (ReplaceOnce (text, "\"attache\"", "\"attach\"")), std::runtime_error);
	EXPECT_THROW (ParseClassicalOrders (ReplaceOnce (text, "\"forme\": \"tore\"", "\"forme\": \"tor\"")), std::runtime_error);
	EXPECT_THROW (ParseClassicalOrders (ReplaceOnce (text, "\"forme\": \"carre\"", "\"forme\": \"carree\"")), std::runtime_error);
}

TEST (TEST_cgarchi_classical_orders, exact_numbers_read_the_fraction_field)
{
	EXPECT_DOUBLE_EQ (ParseExactNumber ("45/2"), 22.5);
	EXPECT_DOUBLE_EQ (ParseExactNumber ("14/3"), 14.0 / 3.0);
	EXPECT_DOUBLE_EQ (ParseExactNumber ("35.83"), 35.83);
	EXPECT_THROW (ParseExactNumber ("1/0"), std::invalid_argument);
	EXPECT_THROW (ParseExactNumber ("x"), std::invalid_argument);
	EXPECT_THROW (ParseExactNumber (""), std::invalid_argument);
}

// T1
TEST (TEST_cgarchi_classical_orders, the_profiles_reproduce_every_computable_control)
{
	const OrdersData &data = ClassicalOrders ();
	const std::vector<ControlCheck> checks = RecomputeControls (data);
	ASSERT_EQ (checks.size (), 61u);
	std::size_t computable = 0;
	for (const ControlCheck &c : checks)
	{
		if (!c.computable)
		{
			// Seules les mesures sur la gravure echappent au recalcul.
			EXPECT_EQ (c.control->label.rfind ("mesure sur la gravure", 0), 0u) << c.control->label;
			continue;
		}
		++computable;
		EXPECT_TRUE (c.matches) << c.control->order << "/" << c.control->block << " " << c.control->label
		                        << " : attendu " << c.expected << ", recalcule " << c.recomputed;
	}
	EXPECT_EQ (computable, 52u);
}

TEST (TEST_cgarchi_classical_orders, an_altered_height_makes_its_control_diverge)
{
	// La base toscane : tore C, h 5 -> 6. Le controle « somme des hauteurs »
	// de la base (12) doit diverger, et lui seul.
	std::string altered (cgarchi::ClassicalOrdersJson ());
	const std::size_t member = altered.find ("toro (tore)\"");
	ASSERT_NE (member, std::string::npos);
	const std::size_t height = altered.find ("\"f\": \"5\"", member);
	ASSERT_NE (height, std::string::npos);
	altered.replace (height, 8, "\"f\": \"6\"");
	const OrdersData data = ParseClassicalOrders (altered);
	std::size_t diverging = 0;
	for (const ControlCheck &c : RecomputeControls (data))
		if (c.computable && !c.matches)
		{
			++diverging;
			EXPECT_EQ (c.control->order, "toscan");
			EXPECT_EQ (c.control->block, "base");
			EXPECT_EQ (c.control->label, "somme des hauteurs");
			EXPECT_DOUBLE_EQ (c.recomputed, 13.0);
		}
	EXPECT_EQ (diverging, 1u);
}

// ===========================================================================
//  Diminution du fut (T3, T4)
// ===========================================================================

// T3
TEST (TEST_cgarchi_classical_orders, the_shaft_radius_meets_both_diameters_of_every_order)
{
	for (const OrderData &o : ClassicalOrders ().orders)
	{
		const ShaftProfile shaft (o);
		EXPECT_DOUBLE_EQ (shaft.Height (), o.general.shaft_M * o.partsPerModule) << o.key;
		EXPECT_NEAR (shaft.Radius (0.0), o.general.lowerDiameter_p / 2.0, 1e-9) << o.key;
		EXPECT_NEAR (shaft.Radius (shaft.Height ()), o.general.upperDiameter_p / 2.0, 1e-9) << o.key;
	}
}

TEST (TEST_cgarchi_classical_orders, the_solved_conchoid_cd_matches_the_value_computed_upstream)
{
	const OrdersData &data = ClassicalOrders ();
	ASSERT_EQ (data.conchoidCDComputed_M.size (), 2u);
	for (const auto &[key, cd_M] : data.conchoidCDComputed_M)
	{
		const OrderData *order = nullptr;
		for (const OrderData &o : data.orders)
			if (o.key == key)
				order = &o;
		ASSERT_NE (order, nullptr) << key;
		const ShaftProfile shaft (*order);
		ASSERT_EQ (shaft.Method (), DiminutionMethod::Conchoid) << key;
		EXPECT_NEAR (shaft.ConchoidCd () / order->partsPerModule, cd_M, 5e-5) << key;
	}
}

TEST (TEST_cgarchi_classical_orders, the_engraved_cd_does_not_give_one_module_at_the_foot)
{
	// Negatif de T3 : CD = 29/27 M (planche) ne donne pas r(0) = 1 M ; c'est
	// pourquoi le generateur resout CD.
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	const double P = ionic.partsPerModule;
	const Conchoid plate (ionic.general.shaft_M * P, ionic.general.upperDiameter_p / 2.0,
	                      ClassicalOrders ().conchoidCDPlate_M * P);
	EXPECT_GT (std::fabs (plate.Radius (0.0) - P), 1e-3 * P);
}

// T4
TEST (TEST_cgarchi_classical_orders, cosine_diminution_is_constant_then_non_increasing)
{
	for (Order order : { Order::Tuscan, Order::Doric })
	{
		const ShaftProfile shaft (ClassicalOrders ().Get (order));
		ASSERT_EQ (shaft.Method (), DiminutionMethod::Cosine);
		const double H = shaft.Height ();
		double previous = shaft.Radius (0.0);
		bool decreased = false;
		for (int i = 1; i <= 300; ++i)
		{
			const double y = H * i / 300.0;
			const double r = shaft.Radius (y);
			if (y <= H / 3.0)
				EXPECT_DOUBLE_EQ (r, shaft.LowerRadius ()) << y;
			EXPECT_LE (r, previous + 1e-12) << y;
			decreased = decreased || r < previous - 1e-9;
			previous = r;
		}
		EXPECT_TRUE (decreased);
	}
}

TEST (TEST_cgarchi_classical_orders, conchoid_diminution_swells_to_the_third_then_tapers)
{
	for (Order order : { Order::Ionic, Order::Corinthian, Order::Composite })
	{
		const ShaftProfile shaft (ClassicalOrders ().Get (order));
		ASSERT_EQ (shaft.Method (), DiminutionMethod::Conchoid);
		const double H = shaft.Height ();
		// Maximum CD au tiers, superieur au rayon du pied : le galbe existe.
		EXPECT_NEAR (shaft.Radius (H / 3.0), shaft.ConchoidCd (), 1e-9);
		EXPECT_GT (shaft.ConchoidCd (), shaft.LowerRadius () + 0.5);
		double previous = shaft.Radius (0.0);
		for (int i = 1; i <= 300; ++i)
		{
			const double y = H * i / 300.0;
			const double r = shaft.Radius (y);
			if (y <= H / 3.0)
				EXPECT_GE (r, previous - 1e-12) << y;
			else
				EXPECT_LE (r, previous + 1e-12) << y;
			previous = r;
		}
	}
	// Negatif : la methode 1 ne galbe pas -- le meme critere echoue sur elle.
	const ShaftProfile tuscan (ClassicalOrders ().Get (Order::Tuscan));
	EXPECT_FALSE (tuscan.Radius (tuscan.Height () / 3.0) > tuscan.LowerRadius () + 0.5);
}

// ===========================================================================
//  Configurations : 5 ordres x 2 corniches doriques x piedestal x entablement
//  x noyau/enveloppe
// ===========================================================================

namespace {

std::vector<ColumnOptions> AllConfigurations (int curveSegments, int shaftSegments)
{
	std::vector<ColumnOptions> all;
	for (int o = 0; o < kOrderCount; ++o)
		for (int cornice = 0; cornice < (o == static_cast<int> (Order::Doric) ? 2 : 1); ++cornice)
			for (int pedestal = 0; pedestal < 2; ++pedestal)
				for (int entablature = 0; entablature < 2; ++entablature)
					for (int envelope = 0; envelope < 2; ++envelope)
					{
						ColumnOptions c;
						c.order = static_cast<Order> (o);
						c.doricCornice = cornice ? DoricCornice::Mutules : DoricCornice::Dentils;
						c.pedestal = pedestal != 0;
						c.entablature = entablature != 0;
						c.ornamentEnvelope = envelope != 0;
						c.curveSegments = curveSegments;
						c.shaftSegments = shaftSegments;
						all.push_back (c);
					}
	return all;
}

std::string Describe (const ColumnOptions &c)
{
	return std::string (OrderKey (c.order)) + (c.doricCornice == DoricCornice::Mutules ? " mutules" : "")
	     + (c.pedestal ? " +piedestal" : "") + (c.entablature ? " +entablement" : "")
	     + (c.ornamentEnvelope ? " enveloppe" : " noyau");
}

}  // namespace

// T2
TEST (TEST_cgarchi_classical_orders, the_stacked_profile_has_the_heights_of_the_general_table)
{
	const std::vector<ColumnOptions> all = AllConfigurations (4, 8);
	EXPECT_EQ (all.size (), 48u);
	for (const ColumnOptions &c : all)
	{
		const OrderData &o = ClassicalOrders ().Get (c.order);
		const double P = o.partsPerModule;
		double expected_M = o.general.column_M;
		if (c.entablature)
			expected_M = c.pedestal ? o.general.totalWithPedestal_M : o.general.totalWithoutPedestal_M;
		else if (c.pedestal)
			expected_M += o.general.pedestal_M;
		const ColumnProfile p = BuildColumnProfile (o, c);
		EXPECT_NEAR (p.height, expected_M * P, 1e-9) << Describe (c);
		ASSERT_FALSE (p.vertices.empty ());
		EXPECT_DOUBLE_EQ (p.vertices.front ().y, 0.0) << Describe (c);
		EXPECT_NEAR (p.vertices.back ().y, p.height, 1e-9) << Describe (c);
	}
}

TEST (TEST_cgarchi_classical_orders, counting_the_outside_module_member_in_the_base_breaks_the_column_height)
{
	// Negatif de T2 : le listel hors module compte dans la base donne 19,5 p
	// au lieu de 18 -- la colonne ne tomberait plus sur colonne_M.
	auto allBaseMembers = [] (const OrderData &o) {
		double h = 0.0;
		for (const Member &m : o.FindBlock ("base")->members)
			h += m.h;
		return h;
	};
	for (Order order : { Order::Ionic, Order::Corinthian, Order::Composite })
	{
		const OrderData &o = ClassicalOrders ().Get (order);
		const double P = o.partsPerModule;
		EXPECT_NEAR (allBaseMembers (o), o.general.base_M * P + 1.5, 1e-9) << o.key;
		EXPECT_GT (std::fabs (allBaseMembers (o) - o.general.base_M * P), 1.0) << o.key;
	}
	// Les ordres sans membre hors module ne bougent pas.
	const OrderData &tuscan = ClassicalOrders ().Get (Order::Tuscan);
	EXPECT_NEAR (allBaseMembers (tuscan), tuscan.general.base_M * tuscan.partsPerModule, 1e-9);
}

// ===========================================================================
//  Formes de moulure (T5)
// ===========================================================================

namespace {

const MouldingForm kAllForms[] = {
	MouldingForm::Fillet, MouldingForm::Face, MouldingForm::Ornament, MouldingForm::Ovolo,
	MouldingForm::Cavetto, MouldingForm::CymaRecta, MouldingForm::CymaReversa, MouldingForm::Conge };

double Slope (const ProfilePoint &a, const ProfilePoint &b)
{
	return std::atan2 (b.y - a.y, b.s - a.s);
}

}  // namespace

TEST (TEST_cgarchi_classical_orders, every_form_spans_its_box_from_s_bas_to_s_haut)
{
	for (MouldingForm form : kAllForms)
		for (Span span : { Span {1.0, 3.0}, Span {3.0, 1.0} })
		{
			const std::vector<ProfilePoint> p = TraceMember (form, 2.0, span, 0.0, 8);
			ASSERT_GE (p.size (), 2u);
			EXPECT_DOUBLE_EQ (p.front ().s, span.bottom);
			EXPECT_DOUBLE_EQ (p.front ().y, 0.0);
			EXPECT_DOUBLE_EQ (p.back ().s, span.top);
			EXPECT_DOUBLE_EQ (p.back ().y, 2.0);
		}
}

TEST (TEST_cgarchi_classical_orders, ovolo_and_cavetto_have_their_tangents)
{
	const double kSmall = 0.05;   // rad, a 64 segments
	const double kRight = 1.5707963267948966;
	const std::vector<ProfilePoint> ove = TraceMember (MouldingForm::Ovolo, 2.0, {1.0, 3.0}, 0.0, 64);
	ASSERT_EQ (ove.size (), 65u);
	EXPECT_LT (std::fabs (Slope (ove[0], ove[1])), kSmall);               // horizontale cote rentrant
	EXPECT_LT (std::fabs (Slope (ove[63], ove[64]) - kRight), kSmall);    // verticale cote saillant
	const std::vector<ProfilePoint> cavet = TraceMember (MouldingForm::Cavetto, 2.0, {1.0, 3.0}, 0.0, 64);
	ASSERT_EQ (cavet.size (), 65u);
	EXPECT_LT (std::fabs (Slope (cavet[0], cavet[1]) - kRight), kSmall);
	EXPECT_LT (std::fabs (Slope (cavet[63], cavet[64])), kSmall);
	// Convexe / concave : au milieu du trace, l'ove est a droite de la corde,
	// le cavet a gauche (la corde va de (1,0) a (3,2) : s = 1 + y).
	EXPECT_GT (ove[32].s, 1.0 + ove[32].y);
	EXPECT_LT (cavet[32].s, 1.0 + cavet[32].y);
}

TEST (TEST_cgarchi_classical_orders, a_reversed_box_mirrors_the_trace_vertically)
{
	for (MouldingForm form : kAllForms)
	{
		const std::vector<ProfilePoint> up = TraceMember (form, 2.0, {1.0, 3.0}, 0.0, 8);
		const std::vector<ProfilePoint> down = TraceMember (form, 2.0, {3.0, 1.0}, 0.0, 8);
		ASSERT_EQ (up.size (), down.size ());
		for (std::size_t i = 0; i < up.size (); ++i)
		{
			const ProfilePoint &m = up[up.size () - 1 - i];
			EXPECT_NEAR (down[i].s, m.s, 1e-12);
			EXPECT_NEAR (down[i].y, 2.0 - m.y, 1e-12);
		}
	}
}

TEST (TEST_cgarchi_classical_orders, torus_and_scotia_reach_their_extremum_where_the_convention_puts_it)
{
	const std::vector<ProfilePoint> tore = TraceMember (MouldingForm::Torus, 5.0, {2.0, 2.0}, 4.5, 8);
	EXPECT_DOUBLE_EQ (tore.front ().s, 2.0);
	EXPECT_DOUBLE_EQ (tore.back ().s, 2.0);
	const auto maxTore = std::max_element (tore.begin (), tore.end (),
	                                       [] (const ProfilePoint &a, const ProfilePoint &b) { return a.s < b.s; });
	EXPECT_NEAR (maxTore->s, 4.5, 1e-12);
	EXPECT_NEAR (maxTore->y, 2.5, 1e-12);

	const std::vector<ProfilePoint> scotie = TraceMember (MouldingForm::Scotia, 2.0, {4.5, 3.5}, 2.5, 8);
	EXPECT_DOUBLE_EQ (scotie.front ().s, 4.5);
	EXPECT_DOUBLE_EQ (scotie.back ().s, 3.5);
	const auto minScotie = std::min_element (scotie.begin (), scotie.end (),
	                                         [] (const ProfilePoint &a, const ProfilePoint &b) { return a.s < b.s; });
	EXPECT_NEAR (minScotie->s, 2.5, 1e-12);
	EXPECT_NEAR (minScotie->y, 1.2, 1e-12);
}

TEST (TEST_cgarchi_classical_orders, cyma_recta_and_cyma_reversa_differ_and_meet_at_the_box_centre)
{
	const std::vector<ProfilePoint> doucine = TraceMember (MouldingForm::CymaRecta, 2.0, {1.0, 3.0}, 0.0, 8);
	const std::vector<ProfilePoint> talon = TraceMember (MouldingForm::CymaReversa, 2.0, {1.0, 3.0}, 0.0, 8);
	ASSERT_EQ (doucine.size (), 9u);
	ASSERT_EQ (talon.size (), 9u);
	EXPECT_NEAR (doucine[4].s, 2.0, 1e-12);
	EXPECT_NEAR (doucine[4].y, 1.0, 1e-12);
	EXPECT_NEAR (talon[4].s, 2.0, 1e-12);
	// Doucine : convexe en bas (a droite de la corde s = 1 + y), concave en
	// haut ; talon : l'inverse. Negatif : les deux traces different.
	EXPECT_GT (doucine[2].s, 1.0 + doucine[2].y);
	EXPECT_LT (doucine[6].s, 1.0 + doucine[6].y);
	EXPECT_LT (talon[2].s, 1.0 + talon[2].y);
	EXPECT_GT (talon[6].s, 1.0 + talon[6].y);
	EXPECT_GT (std::fabs (doucine[2].s - talon[2].s), 0.1);
}

// ===========================================================================
//  Conflits de plan (T8)
// ===========================================================================

TEST (TEST_cgarchi_classical_orders, the_plan_conflicts_of_the_data_are_exactly_the_known_ones)
{
	// Jonctions plates ou un anneau rond et un anneau carre (ou carre concave)
	// se croisent. Le maillage les traite (RingCrossings) ; elles restent
	// SIGNALEES. Toute autre jonction croisee fait echouer ce cas.
	//   ionique   : ove (rond, 22) sous le canal de la volute (oriente_ionique
	//               rendu carre, demi-cote 17) ;
	//   corinthien, noyau : caulicoles (rond, 20,5) sous l'abaque concave
	//               (cornes 32, faces 17,6) ;
	//   composite : canal de la volute (carre_concave a coupe unique, rond
	//               21,5) sous l'abaque concave (cornes 32,5, faces 17,73).
	std::size_t total = 0;
	for (const ColumnOptions &c : AllConfigurations (4, 8))
	{
		const ColumnProfile p = BuildColumnProfile (ClassicalOrders ().Get (c.order), c);
		const std::vector<PlanConflict> conflicts = FindPlanConflicts (p);
		total += conflicts.size ();
		std::size_t expected = 0;
		const char *lower = "";
		const char *upper = "";
		if (c.order == Order::Ionic)
		{
			expected = 1; lower = "chapiteau/ove"; upper = "chapiteau/canal de la volute";
		}
		else if (c.order == Order::Corinthian && !c.ornamentEnvelope)
		{
			expected = 1; lower = "caulicoli"; upper = "abaco";
		}
		else if (c.order == Order::Composite)
		{
			expected = 1; lower = "chapiteau/canal de la volute"; upper = "chapiteau/abaque";
		}
		ASSERT_EQ (conflicts.size (), expected) << Describe (c);
		if (expected)
		{
			EXPECT_NE (conflicts[0].lower.find (lower), std::string::npos) << Describe (c) << " : " << conflicts[0].lower;
			EXPECT_NE (conflicts[0].upper.find (upper), std::string::npos) << Describe (c) << " : " << conflicts[0].upper;
			EXPECT_EQ (conflicts[0].angles.size (), 1u) << Describe (c);
		}
	}
	EXPECT_EQ (total, 8u + 4u + 8u);
}

TEST (TEST_cgarchi_classical_orders, a_flat_box_that_pushes_the_arcs_out_is_refused)
{
	// Diagonale a moins de 30 degres : les arcs de 60 degres d'une doucine
	// sortent de [0, h] (en dessous de 0 au pied). Refus plutot qu'un trace qui
	// chevaucherait les membres voisins.
	EXPECT_THROW (TraceMember (MouldingForm::CymaRecta, 1.0, {0.0, 4.0}, 0.0, 8), std::invalid_argument);
	EXPECT_THROW (TraceMember (MouldingForm::CymaRecta, 1.0, {4.0, 0.0}, 0.0, 8), std::invalid_argument);
	// Positif : a 30 degres et au-dela, la doucine tient dans sa boite.
	EXPECT_NO_THROW (TraceMember (MouldingForm::CymaRecta, 1.0, {0.0, 1.7}, 0.0, 8));

	// Les talons des donnees, jusqu'au plus plat (23,2 degres, corniche
	// corinthienne), restent dans leur boite, coupes diagonale et face.
	std::size_t cymas = 0;
	for (const OrderData &o : ClassicalOrders ().orders)
		for (const Block &b : o.blocks)
			for (const Member &m : b.members)
			{
				if (m.form != MouldingForm::CymaRecta && m.form != MouldingForm::CymaReversa)
					continue;
				++cymas;
				std::vector<Span> cuts { m.projection.main };
				if (m.projection.kind == ProjectionKind::TwoCuts)
					cuts.push_back (m.projection.face);
				for (const Span &cut : cuts)
				{
					std::vector<ProfilePoint> p;
					ASSERT_NO_THROW (p = TraceMember (m.form, m.h, cut, 0.0, 32)) << o.key << "/" << b.key << "/" << m.name;
					for (const ProfilePoint &pt : p)
					{
						EXPECT_GE (pt.y, 0.0) << m.name;
						EXPECT_LE (pt.y, m.h) << m.name;
					}
				}
			}
	EXPECT_EQ (cymas, 34u);
}

TEST (TEST_cgarchi_classical_orders, a_concave_square_too_deep_to_be_seen_from_the_axis_is_refused)
{
	// Face creusee : valide si et seulement si face >= (2 - sqrt2) a,
	// a = corne / sqrt2. Au-dela, le rayon a 45 degres rencontre l'arc avant
	// la corne.
	const double corner = std::sqrt (2.0);   // a = 1
	EXPECT_THROW (Ring::MakeConcaveSquare (corner, 0.3), std::invalid_argument);
	EXPECT_THROW (Ring::MakeConcaveSquare (corner, (2.0 - std::sqrt (2.0)) * 0.999), std::invalid_argument);
	const Ring limit = Ring::MakeConcaveSquare (corner, (2.0 - std::sqrt (2.0)) * 1.001);
	EXPECT_NEAR (limit.RadiusAt (std::atan (1.0)), corner, 1e-9);
	// L'abaque corinthien des donnees : cornes 32, faces 17,6 ; la corne est
	// bien la pointe a 45 degres, le milieu de face a 0.
	const Ring abacus = Ring::MakeConcaveSquare (32.0, 17.6);
	EXPECT_NEAR (abacus.RadiusAt (std::atan (1.0)), 32.0, 1e-9);
	EXPECT_NEAR (abacus.RadiusAt (0.0), 17.6, 1e-9);
	// Face bombee : l'axe doit rester dans le cercle de la face.
	EXPECT_NO_THROW (Ring::MakeConcaveSquare (corner, 1.2));
}

TEST (TEST_cgarchi_classical_orders, a_square_ring_on_a_crossing_circle_is_a_plan_conflict)
{
	// Carre de demi-cote b, cercle de rayon a : conflit si b < a < b sqrt 2.
	auto profileOf = [] (const Ring &lower, const Ring &upper) {
		ColumnProfile p;
		p.vertices = { {0.0, lower, -1}, {1.0, lower, -1}, {1.0, upper, -1}, {2.0, upper, -1} };
		return p;
	};
	const Ring square = Ring::MakeSquare (10.0);
	EXPECT_EQ (FindPlanConflicts (profileOf (square, Ring::MakeCircle (12.0))).size (), 1u);
	EXPECT_EQ (FindPlanConflicts (profileOf (Ring::MakeCircle (12.0), square)).size (), 1u);
	const std::vector<double> at = RingCrossings (square, Ring::MakeCircle (12.0));
	ASSERT_EQ (at.size (), 1u);
	EXPECT_NEAR (at[0], std::acos (10.0 / 12.0), 1e-12);
	// Hors de ]b, b sqrt 2[ : pas de conflit, y compris au contact (a = b).
	EXPECT_TRUE (FindPlanConflicts (profileOf (square, Ring::MakeCircle (9.0))).empty ());
	EXPECT_TRUE (FindPlanConflicts (profileOf (square, Ring::MakeCircle (10.0))).empty ());
	EXPECT_TRUE (FindPlanConflicts (profileOf (square, Ring::MakeCircle (14.2))).empty ());
}

// ===========================================================================
//  Maillage (T6) et forme parametrique
// ===========================================================================

namespace {

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

MeshValidationReport Validate (const Mesh &mesh)
{
	MeshValidationOptions options;
	options.expectedComponents = 1u;
	return ValidateMesh (mesh, options);
}

void ExpectEveryConfigurationValid (int curveSegments, int shaftSegments, int radialSegments)
{
	for (const ColumnOptions &c : AllConfigurations (curveSegments, shaftSegments))
	{
		const OrderData &o = ClassicalOrders ().Get (c.order);
		const ColumnProfile p = BuildColumnProfile (o, c);
		LatheOptions lathe;
		lathe.radialSegments = radialSegments;
		lathe.scale = 1.0 / o.partsPerModule;   // module = 1
		const std::unique_ptr<Mesh> mesh = LatheColumnProfile (p, lathe);
		ASSERT_NE (mesh, nullptr);
		const MeshValidationReport r = Validate (*mesh);
		EXPECT_TRUE (r.IsValid ()) << Describe (c) << " :" << Defects (r);
		EXPECT_EQ (r.components, 1u) << Describe (c);
		EXPECT_GT (r.signedVolume, 0.0) << Describe (c);
		// Hauteur = T2 x echelle (float).
		EXPECT_NEAR (r.bbMin[2], 0.0, 1e-6) << Describe (c);
		EXPECT_NEAR (r.bbMax[2], p.height / o.partsPerModule, 1e-4) << Describe (c);
	}
}

}  // namespace

// T6
TEST (TEST_cgarchi_classical_orders, every_configuration_lathes_into_a_closed_valid_solid)
{
	ExpectEveryConfigurationValid (2, 8, 16);
}

// T6 a la resolution par defaut de la forme (6 segments de courbe, 24 pour le
// fut, 48 angles) : seule a echantillonner l'interieur des talons non
// monotones. Label `slow` (test/slow_tests.cmake).
TEST (TEST_cgarchi_classical_orders, every_configuration_lathes_into_a_closed_valid_solid_at_the_default_resolution)
{
	ExpectEveryConfigurationValid (6, 24, 48);
}

TEST (TEST_cgarchi_classical_orders, a_crossing_junction_lathed_without_splitting_overlaps_itself)
{
	// Negatif de T6 : l'instrument doit pouvoir echouer. Carre b = 10 sous un
	// cercle a = 12 (b < a < b sqrt 2) : sans les angles de croisement, les deux
	// triangles plats de chaque secteur croise se recouvrent.
	ColumnProfile p;
	const Ring square = Ring::MakeSquare (10.0);
	const Ring circle = Ring::MakeCircle (12.0);
	p.vertices = { {0.0, square, -1}, {1.0, square, -1}, {1.0, circle, -1}, {2.0, circle, -1} };
	p.height = 2.0;
	LatheOptions lathe;
	lathe.radialSegments = 16;

	lathe.splitAtPlanCrossings = false;
	const MeshValidationReport raw = Validate (*LatheColumnProfile (p, lathe));
	EXPECT_FALSE (raw.IsValid ());
	EXPECT_GT (raw.Count (MeshDefect::SelfIntersection), 0u) << Defects (raw);

	lathe.splitAtPlanCrossings = true;
	const MeshValidationReport split = Validate (*LatheColumnProfile (p, lathe));
	EXPECT_TRUE (split.IsValid ()) << Defects (split);
	EXPECT_EQ (LatheAngles (p, lathe).size (), 16u + 8u);
}

TEST (TEST_cgarchi_classical_orders, the_ionic_capital_needs_the_crossing_split_to_be_valid)
{
	// Le meme negatif sur les donnees : l'ove sous le canal de la volute.
	ColumnOptions c;
	c.order = Order::Ionic;
	c.curveSegments = 2;
	c.shaftSegments = 8;
	const ColumnProfile p = BuildColumnProfile (ClassicalOrders ().Get (Order::Ionic), c);
	LatheOptions lathe;
	lathe.radialSegments = 16;
	lathe.splitAtPlanCrossings = false;
	const MeshValidationReport raw = Validate (*LatheColumnProfile (p, lathe));
	EXPECT_GT (raw.Count (MeshDefect::SelfIntersection), 0u) << Defects (raw);
	lathe.splitAtPlanCrossings = true;
	EXPECT_TRUE (Validate (*LatheColumnProfile (p, lathe)).IsValid ());
}

TEST (TEST_cgarchi_classical_orders, the_parameterized_column_follows_its_parameters)
{
	cgarchi::ParameterizedClassicalColumn column;
	EXPECT_EQ (column.GetName (), "Classical Column");
	EXPECT_TRUE (column.LastError ().empty ()) << column.LastError ();
	ASSERT_NE (column.GetMesh (), nullptr);

	// Par defaut : toscan, module 1, colonne seule (14 M).
	const MeshValidationReport r = Validate (*column.GetMesh ());
	EXPECT_TRUE (r.IsValid ()) << Defects (r);
	EXPECT_NEAR (r.bbMax[2] - r.bbMin[2], 14.0, 1e-4);
	std::cout << "[ mesure ] Classical Column par defaut : " << column.GetMesh ()->GetNFaces ()
	          << " triangles, " << column.GetMesh ()->GetNVertices () << " sommets\n";

	std::vector<Parameter> params = column.GetParameters ();
	ASSERT_EQ (params.size (), 11u);
	auto find = [&] (const char *name) -> Parameter & {
		for (Parameter &p : params)
			if (p.GetName () == name)
				return p;
		throw std::runtime_error (name);
	};
	EXPECT_EQ (find ("Order").GetChoices ().size (), 5u);
	EXPECT_EQ (find ("Doric cornice").GetChoices ().size (), 2u);

	// Corinthien, entablement, piedestal, module 2 : 32 M x 2.
	find ("Order").SetInt (3);
	find ("Entablature").SetBool (true);
	find ("Pedestal").SetBool (true);
	find ("Module").SetFloat (2.0f);
	const auto t0 = std::chrono::steady_clock::now ();
	column.Regenerate ();
	const auto t1 = std::chrono::steady_clock::now ();
	EXPECT_TRUE (column.LastError ().empty ()) << column.LastError ();
	const MeshValidationReport big = Validate (*column.GetMesh ());
	EXPECT_TRUE (big.IsValid ()) << Defects (big);
	EXPECT_NEAR (big.bbMax[2] - big.bbMin[2], 64.0, 1e-3);
	EXPECT_EQ (column.PlanConflicts ().size (), 1u);   // caulicoles (noyau) sous l'abaque
	std::cout << "[ mesure ] corinthien complet : " << column.GetMesh ()->GetNFaces () << " triangles, Regenerate "
	          << std::chrono::duration<double, std::milli> (t1 - t0).count () << " ms\n";

	// Enveloppe : le conflit disparait (caulicoles a 33,4 > cornes 32).
	find ("Ornament envelope").SetBool (true);
	column.Regenerate ();
	EXPECT_TRUE (column.PlanConflicts ().empty ());

	// Corniche dorique : l'une ou l'autre, jamais les deux.
	find ("Order").SetInt (1);
	find ("Doric cornice").SetInt (0);
	column.Regenerate ();
	ASSERT_FALSE (column.Profile ().members.empty ());
	EXPECT_EQ (column.Profile ().members.back ().block, "corniche_denticules");
	find ("Doric cornice").SetInt (1);
	column.Regenerate ();
	EXPECT_EQ (column.Profile ().members.back ().block, "corniche_mutules");
	for (const ProfileMember &m : column.Profile ().members)
		EXPECT_NE (m.block, "corniche_denticules");

	// Bornes : hors domaine, ramene.
	find ("Radial segments").SetInt (100000);
	find ("Order").SetInt (99);
	column.Regenerate ();
	EXPECT_EQ (find ("Radial segments").GetInt (), 256);
	EXPECT_EQ (find ("Order").GetInt (), 4);
}
