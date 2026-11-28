#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>

using namespace cgarchi::classical;

// ===========================================================================
//  Porteuse de la feuille : meridien d'axe plie et borne par le cone
// ===========================================================================

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

const detail::CapitalEnvelope &Envelope ()
{
	static const detail::CapitalEnvelope env = [] {
		ColumnOptions options;
		options.order = Order::Corinthian;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		return detail::ReadCapitalEnvelope (order, BuildColumnProfile (order, options));
	}();
	return env;
}

struct Pose
{
	const char *name;
	CapitalPieceKind kind;
	double theta;
	// Cotes de la pl. XXV corrigees (r_donnee = 0,9375 r_image + 2,06, registre AC-44) : crete et pointe de l'extrados.
	std::array<double, 2> crest, tip;
	double radiusLo, radiusHi;   // J13' : R_S
	double halfWidth;            // hw du rang (p), registre AC-41
};

const Pose kPoses[] = {
	{"F pi/8", CapitalPieceKind::LowerLeaf, kPi / 8.0, {19.87, 12.4}, {22.12, 10.4}, 1.8, 2.4, 4.85},
	{"E 0", CapitalPieceKind::MiddleLeaf, 0.0, {24.19, 24.3}, {27.19, 21.5}, 2.5, 3.1, 5.25},
	{"E pi/4", CapitalPieceKind::MiddleLeaf, kPi / 4.0, {24.19, 24.3}, {27.19, 21.5}, 2.5, 3.1, 5.25},
};

struct Extrados
{
	double contact = -std::numeric_limits<double>::infinity ();   // max (r - borne) sur l'axe
	std::array<double, 2> crest {0.0, -std::numeric_limits<double>::infinity ()}, tip {};
	double crestS = -std::numeric_limits<double>::infinity ();    // crete de S
	double tangent = 0.0;                                          // tangente finale, depuis la verticale montante (deg)
};

Extrados Measure (const detail::LeafBend &bend, const detail::LeafTemplate &tpl, const detail::LeafSpec &spec)
{
	const double back = 0.5 * spec.thickness + tpl.backHeight;
	Extrados m;
	constexpr int kSamples = 3000;
	for (int i = 0; i <= kSamples; ++i)
	{
		const double y = 2.0 * i / kSamples;
		const std::array<double, 2> s = bend.Meridian (0.0, y), n = bend.BackNormal (0.0, y);
		const std::array<double, 2> e {s[0] + back * n[0], s[1] + back * n[1]};
		m.contact = std::max (m.contact, e[0] - Envelope ().LeafBoundRadius (e[1]));
		m.crestS = std::max (m.crestS, s[1]);
		if (y >= 1.0 && e[1] > m.crest[1])
			m.crest = e;
		if (i == kSamples)
			m.tip = e;
	}
	const std::array<double, 2> a = bend.Meridian (0.0, 2.0 - 1e-4), b = bend.Meridian (0.0, 2.0);
	m.tangent = std::atan2 (b[0] - a[0], b[1] - a[1]) / kDeg;
	return m;
}

bool ThrowsWith (const std::function<void ()> &f, const std::string &fragment)
{
	try
	{
		f ();
	}
	catch (const std::invalid_argument &e)
	{
		if (std::string (e.what ()).find (fragment) != std::string::npos)
			return true;
		ADD_FAILURE () << "message sans « " << fragment << " » : " << e.what ();
		return false;
	}
	ADD_FAILURE () << "aucun refus, « " << fragment << " » attendu";
	return false;
}

// Fichier ouvert en ecriture, nullptr en cas d'echec.
std::FILE *OpenForWriting (const char *path)
{
#if defined (_MSC_VER)
	std::FILE *f = nullptr;
	return fopen_s (&f, path, "w") == 0 ? f : nullptr;
#else
	return std::fopen (path, "w");
#endif
}

} // namespace

TEST (TEST_cgarchi_classical_acanthus_bend, the_axis_meridian_meets_plate_XXV)
{
	const detail::LeafStyle style = detail::LeafStyle::Olive;
	for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Pose &pose : kPoses)
		{
			SCOPED_TRACE (std::string (pose.name) + " Olive " +
			              (level == OrnamentDetail::Light ? "Light" : "Heavy"));
			const detail::LeafTemplate &tpl = detail::LeafTemplateOf (style, level);
			const detail::LeafSpec spec = detail::DefaultLeafSpec (level, style);
			const detail::LeafPlacement placement = detail::DefaultLeafPlacement (pose.kind, pose.theta);
			const detail::LeafBend bend (Envelope (), spec, placement, tpl);
			const Extrados m = Measure (bend, tpl, spec);
			// J4 : crete de S a hTop.
			EXPECT_NEAR (m.crestS, placement.hTop, 0.05);
			// J13' : flexion totale, tangente finale, rayon du bec.
			EXPECT_GE (bend.TipTurn () / kDeg, 150.0);
			EXPECT_LE (bend.TipTurn () / kDeg, 200.0);
			EXPECT_GE (m.tangent, 160.0);
			EXPECT_LE (m.tangent, 195.0);
			EXPECT_GE (bend.BendRadius (), pose.radiusLo);
			EXPECT_LE (bend.BendRadius (), pose.radiusHi);
			// J16' : contact de l'extrados avec la borne.
			EXPECT_LE (m.contact, 0.0);
			EXPECT_GE (m.contact, -0.35);
			// J18 : cotes corrigees de la pl. XXV.
			EXPECT_NEAR (m.crest[0], pose.crest[0], 0.6);
			EXPECT_NEAR (m.crest[1], pose.crest[1], 0.5);
			EXPECT_NEAR (m.tip[0], pose.tip[0], 0.6);
			EXPECT_NEAR (m.tip[1], pose.tip[1], 0.5);
			std::printf ("%-7s %s %s : R_S %.3f phi_t %.1f tangente %.1f | crete S %.3f | contact %+.3f | crete extrados "
			             "(%.2f, %.2f) [%+.2f, %+.2f] | pointe (%.2f, %.2f) [%+.2f, %+.2f] | corps %.2f p (%.3f x nominal), "
			             "bec %.2f p (%.3f x nominal)\n",
			             pose.name, "Olive", level == OrnamentDetail::Light ? "Light" : "Heavy",
			             bend.BendRadius (), bend.TipTurn () / kDeg, m.tangent, m.crestS, m.contact, m.crest[0], m.crest[1],
			             m.crest[0] - pose.crest[0], m.crest[1] - pose.crest[1], m.tip[0], m.tip[1], m.tip[0] - pose.tip[0],
			             m.tip[1] - pose.tip[1], bend.BodyLength (), bend.BodyLength () / tpl.nominalBody, bend.BendLength (),
			             bend.BendLength () / tpl.nominalCurl);
		}
}

TEST (TEST_cgarchi_classical_acanthus_bend, the_dome_lowers_the_cap_edges_and_stays_inside)
{
	// J12' (a) : a la crete, le meridien du bord de la calotte (|u| = u_cap) est plus bas que l'axe d'au
	// moins 0,5 d_max, d_max = kappa u_cap hw avec kappa = 0,2 du registre AC-43 et hw du rang (AC-41).
	constexpr double kDomeRegister = 0.2;
	for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Pose &pose : kPoses)
		{
			SCOPED_TRACE (std::string (pose.name) + (level == OrnamentDetail::Light ? " Light" : " Heavy"));
			const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, level);
			const detail::LeafSpec spec = detail::DefaultLeafSpec (level, detail::LeafStyle::Olive);
			const detail::LeafBend bend (Envelope (), spec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
			const double yc = bend.CrestY ();
			const std::array<double, 2> axis = bend.Meridian (0.0, yc), edge = bend.Meridian (tpl.capHalfWidth, yc);
			const double dMax = kDomeRegister * tpl.capHalfWidth * pose.halfWidth;
			EXPECT_LE (edge[1], axis[1] - 0.5 * dMax);
			for (int i = 0; i <= 200; ++i)
			{
				const double y = 1.0 + i / 200.0;
				const std::array<double, 3> p = bend.At (tpl.capHalfWidth, y);
				EXPECT_LE (std::hypot (p[0], p[1]), Envelope ().LeafBoundRadius (p[2]) - spec.margin + 1e-9) << y;
			}
			// Plafond : la place le long de la normale revers a la pointe est finie et positive.
			const std::array<double, 2> n = bend.BackNormal (0.0, 2.0);
			const std::array<double, 3> tip = bend.At (0.0, 2.0);
			const double free = bend.Free (tip, {n[0], 0.0, n[1]});
			EXPECT_GT (free, 0.0);
			EXPECT_LT (free, 1.0);
		}
}

TEST (TEST_cgarchi_classical_acanthus_bend, the_guards_are_seen)
{
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
	const detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
	const auto with = [&] (const std::function<void (detail::LeafPlacement &)> &change) {
		return [&, change] {
			detail::LeafPlacement p = detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0);
			change (p);
			detail::LeafBend (Envelope (), spec, p, tpl);
		};
	};
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.bendRadius = 1.2; }), "rayon de courbure"));
	// J9 : R_S = 1,9 - t/2 - Hb_ref = 1,55 p tient 2 t (0,4 p) mais pas 2 t + H au depart du bec (1,8 p) :
	// seul le relief fait refuser ce bec.
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.bendRadius = 1.9; }), "rayon de courbure du bec"));
	// Coin concave a la jonction de la bande de tige et du corps : E penche au pied est refuse : coin a la
	// jonction (y = 0), rayon nul.
	EXPECT_TRUE (ThrowsWith (
		[&] {
			detail::LeafPlacement p = detail::DefaultLeafPlacement (CapitalPieceKind::MiddleLeaf, 0.0);
			p.footLean = 25.0 * kDeg;
			detail::LeafBend (Envelope (), spec, p, tpl);
		},
		"y = 0.000000"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.hTop = p.hFoot + 1.0; }), "bec plus bas"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.halfWidth = std::nan (""); }), "pose hors domaine : halfWidth"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.hStem = p.hFoot + 1.0; }), "pose hors domaine : hStem"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.tipTurn = 0.0; }), "pose hors domaine : tipTurn"));
	EXPECT_TRUE (ThrowsWith ([] { detail::DefaultLeafPlacement (CapitalPieceKind::Volute, 0.0); }, "F ou E"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.theta = std::nan (""); }), "pose hors domaine : theta fini"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.hTop = std::nan (""); }), "pose hors domaine : hTop fini"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.bendRadius = 0.0; }), "pose hors domaine : bendRadius"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafPlacement &p) { p.footLean = -0.1; }), "pose hors domaine : footLean"));
	const auto shape = [&] (const std::function<void (detail::LeafSpec &)> &change) {
		return [&, change] {
			detail::LeafSpec s = spec;
			change (s);
			detail::LeafBend (Envelope (), s, detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0), tpl);
		};
	};
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.thickness = 0.0; }), "thickness fini et > 0"));
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.clearance = std::nan (""); }), "clearance fini et >= 0"));
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.clearance = -0.01; }), "clearance fini et >= 0"));
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.margin = -0.1; }), "margin fini et >= 0"));
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.gutter = 1.0; }), "gutter dans [0, 1["));
	EXPECT_TRUE (ThrowsWith (shape ([] (detail::LeafSpec &s) { s.margin = 30.0; }), "aucune place pour le bec"));
	const auto form = [&] (const std::function<void (detail::LeafTemplate &)> &change) {
		return [&, change] {
			detail::LeafTemplate t = tpl;
			change (t);
			detail::LeafBend (Envelope (), spec, detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0), t);
		};
	};
	EXPECT_TRUE (ThrowsWith (form ([] (detail::LeafTemplate &t) { t.leafVertices = 0; }), "gabarit hors domaine : prefixe"));
	EXPECT_TRUE (ThrowsWith (form ([] (detail::LeafTemplate &t) { t.capHalfWidth = 0.0; }), "aucun sommet sur le bec"));
	EXPECT_TRUE (ThrowsWith (form ([] (detail::LeafTemplate &t) { t.fade = {1.0, 1.9}; }), "effacement du relief au-dela de la crete"));
	EXPECT_TRUE (ThrowsWith (form ([] (detail::LeafTemplate &t) {
		                         for (detail::LeafTemplateVertex &v : t.vertices)
			                         if (v.y >= 0.0 && v.y <= 1.0)
				                         v.u = 0.0;
	                         }),
	                         "corps sans largeur"));
	detail::CapitalEnvelope broken = Envelope ();
	broken.scale = std::nan ("");
	EXPECT_TRUE (ThrowsWith ([&] { detail::LeafBend (broken, spec, detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0), tpl); },
	                         "enveloppe hors domaine"));
}

// La porteuse garde l'enveloppe par reference : un temporaire est refuse a la compilation. Le gabarit
// n'est lu qu'a la construction, mais un temporaire l'est aussi, par symetrie.
static_assert (std::is_constructible_v<detail::LeafBend, const detail::CapitalEnvelope &, const detail::LeafSpec &,
                                       const detail::LeafPlacement &, const detail::LeafTemplate &>);
static_assert (!std::is_constructible_v<detail::LeafBend, detail::CapitalEnvelope &&, const detail::LeafSpec &,
                                        const detail::LeafPlacement &, const detail::LeafTemplate &>);
static_assert (!std::is_constructible_v<detail::LeafBend, const detail::CapitalEnvelope &, const detail::LeafSpec &,
                                        const detail::LeafPlacement &, detail::LeafTemplate &&>);

// La recherche de place est bornee a 40 p quelle que soit la portee demandee : vers le haut, la borne (cone)
// s'elargit plus vite que la feuille ne s'en eloigne, la place ne finit pas. Le plaquage avec psi minuscule
// (portee H / psi demesuree) se termine et plafonne comme avec une portee de 40.
TEST (TEST_cgarchi_classical_acanthus_bend, the_free_room_is_bounded)
{
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
	detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
	const detail::LeafPlacement p = detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0);
	const detail::LeafBend bend (Envelope (), spec, p, tpl);
	const std::array<double, 3> s = bend.At (0.0, 0.5), up {0.0, 0.0, 1.0};
	ASSERT_EQ (bend.Free (s, up, 1000.0), 40.0);
	EXPECT_EQ (bend.Free (s, up, 1e300), 40.0);
	EXPECT_EQ (bend.Free (s, up, std::numeric_limits<double>::infinity ()), 40.0);
	EXPECT_EQ (bend.Free (s, up), 40.0);
	EXPECT_EQ (bend.CoreRoom (s, {s[0], s[1], 0.0}, 1000.0), 40.0);
	for (const double psi : {1e-3, 1e-300})
	{
		spec.reliefShare = psi;
		const detail::LeafSheet sheet = detail::BuildLeafSheet (Envelope (), spec, p, tpl);
		double worst = 0.0;
		for (const std::array<double, 4> &q : sheet.planar)
			worst = std::max ({worst, q[2] - 40.0 * psi, q[3] - 40.0 * psi});
		EXPECT_LE (worst, 0.0) << psi;
	}
}

// La garde de courbure lit le relief plaque, reliefScale H, et non le H du gabarit.
TEST (TEST_cgarchi_classical_acanthus_bend, the_curvature_guard_reads_the_scaled_relief)
{
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
	detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
	const detail::LeafPlacement p = detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0);
	EXPECT_NO_THROW (detail::LeafBend (Envelope (), spec, p, tpl));
	spec.reliefScale = 3.0;
	EXPECT_TRUE (ThrowsWith ([&] { detail::LeafBend (Envelope (), spec, p, tpl); }, "rayon de courbure"));
	spec.reliefScale = std::nan ("");
	EXPECT_TRUE (ThrowsWith ([&] { detail::LeafBend (Envelope (), spec, p, tpl); }, "reliefScale"));
}

TEST (TEST_cgarchi_classical_acanthus_bend, the_stem_band_is_vertical_on_the_bell_and_absent_from_F)
{
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Light);
	const detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Light, detail::LeafStyle::Olive);
	const detail::LeafPlacement pe = detail::DefaultLeafPlacement (CapitalPieceKind::MiddleLeaf, 0.0);
	const detail::LeafBend e (Envelope (), spec, pe, tpl);
	EXPECT_NEAR (e.StemLength (), pe.hFoot - pe.hStem, 1e-12);
	for (const double y : {-1.0, -0.5, -1e-3})
	{
		const std::array<double, 2> s = e.Meridian (0.0, y), n = e.BackNormal (0.0, y);
		EXPECT_NEAR (s[0], e.Base (s[1]), 1e-12);
		EXPECT_NEAR (s[1], pe.hFoot + y * e.StemLength (), 1e-12);
		EXPECT_NEAR (n[0], -1.0, 1e-9);
	}
	const detail::LeafBend f (Envelope (), spec, detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, kPi / 8.0), tpl);
	EXPECT_EQ (f.StemLength (), 0.0);
	const std::array<double, 2> n = f.BackNormal (0.0, -0.5);
	EXPECT_TRUE (std::isfinite (n[0]) && std::isfinite (n[1]));
}

// Gouttiere AC-9 : sur le corps, off a l'axe = (1 - g) off au bord du corps, g = 0,2 du registre.
TEST (TEST_cgarchi_classical_acanthus_bend, the_gutter_hollows_the_body_axis)
{
	constexpr double kGutter = 0.2;
	for (const Pose &pose : kPoses)
	{
		SCOPED_TRACE (pose.name);
		const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
		const detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
		const detail::LeafBend bend (Envelope (), spec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
		for (const double y : {0.3, 0.6, 0.9})
		{
			const std::array<double, 2> axis = bend.Meridian (0.0, y), edge = bend.Meridian (bend.BodyHalfWidth (), y);
			const double offAxis = axis[0] - bend.Base (axis[1]), offEdge = edge[0] - bend.Base (edge[1]);
			ASSERT_GT (offEdge, 0.0);
			EXPECT_NEAR (offAxis / offEdge, 1.0 - kGutter, 1e-9) << y;
		}
		// Contre la meme porteuse sans gouttiere : facteur 1 - g sur le corps (controle positif d'un ecart
		// > 0,05 p), entre 1 - g et 1 au milieu de la rampe, 1 au-dela de la crete sur tout le bec.
		detail::LeafSpec flatSpec = spec;
		flatSpec.gutter = 0.0;
		const detail::LeafBend flat (Envelope (), flatSpec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
		const auto factor = [&] (double y) {
			const std::array<double, 2> m = bend.Meridian (0.0, y), f = flat.Meridian (0.0, y);
			return 1.0 - (f[0] - m[0]) / (f[0] - flat.Base (f[1]));
		};
		const std::array<double, 2> body = bend.Meridian (0.0, 0.5), bodyFlat = flat.Meridian (0.0, 0.5);
		EXPECT_GT (bodyFlat[0] - body[0], 0.05);
		EXPECT_NEAR (factor (0.5), 1.0 - kGutter, 1e-9);
		const double rampStart = std::max (1.0, tpl.fade[1]);
		const double middle = factor (0.5 * (rampStart + bend.CrestY ()));
		EXPECT_GT (middle, 1.0 - kGutter + 0.01);
		EXPECT_LT (middle, 1.0 - 0.01);
		for (int i = 0; i <= 20; ++i)
		{
			const double y = bend.CrestY () + (2.0 - bend.CrestY ()) * i / 20.0;
			for (const double u : {0.0, 0.5 * tpl.capHalfWidth, tpl.capHalfWidth})
			{
				const std::array<double, 2> m = bend.Meridian (u, y), f = flat.Meridian (u, y);
				EXPECT_NEAR (m[0], f[0], 1e-9) << u << ", " << y;
				EXPECT_NEAR (m[1], f[1], 1e-9) << u << ", " << y;
			}
		}
	}
}

// Gouttiere admissible par cas : marge de la garde a g = spec.gutter, et plus grand g accepte (bissection
// sur [0 ; 0,6]).
TEST (TEST_cgarchi_classical_acanthus_bend, DISABLED_gutter_margin)
{
	const detail::LeafStyle style = detail::LeafStyle::Olive;
	for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Pose &pose : kPoses)
		{
			const detail::LeafTemplate &tpl = detail::LeafTemplateOf (style, level);
			detail::LeafSpec spec = detail::DefaultLeafSpec (level, style);
			const detail::LeafPlacement placement = detail::DefaultLeafPlacement (pose.kind, pose.theta);
			const detail::LeafCurvatureMargin m = detail::LeafBend (Envelope (), spec, placement, tpl).CurvatureMargin ();
			double lo = 0.0, hi = 0.6;
			for (int i = 0; i < 30; ++i)
			{
				spec.gutter = 0.5 * (lo + hi);
				try
				{
					detail::LeafBend (Envelope (), spec, placement, tpl);
					lo = spec.gutter;
				}
				catch (const std::invalid_argument &)
				{
					hi = spec.gutter;
				}
			}
			std::printf ("%-7s %s %s : g max %.4f | a g = %.2f : marge %+.4f (u %.3f, y %.4f, R %.3f)\n", pose.name, "Olive",
			             level == OrnamentDetail::Light ? "Light" : "Heavy", lo, detail::DefaultLeafSpec (level, style).gutter, m.margin,
			             m.u, m.y, m.radius);
		}
}

// Garde de courbure (registre AC-8) : marge >= 0,1 p sur les quatre feuilles, et le point le plus serre ne
// depend pas de l'echantillonnage (4 et 64 echantillons par intervalle lisse).
TEST (TEST_cgarchi_classical_acanthus_bend, the_curvature_guard_keeps_its_margin_at_any_sampling)
{
	for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Pose &pose : {kPoses[0], kPoses[1]})
		{
			SCOPED_TRACE (std::string (pose.name) + (level == OrnamentDetail::Light ? " Light" : " Heavy"));
			const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, level);
			const detail::LeafSpec spec = detail::DefaultLeafSpec (level, detail::LeafStyle::Olive);
			const detail::LeafBend bend (Envelope (), spec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
			const detail::LeafCurvatureMargin coarse = bend.CurvatureMargin (4), fine = bend.CurvatureMargin (64),
			                                  standard = bend.CurvatureMargin ();
			EXPECT_GE (standard.margin, 0.1);
			EXPECT_NEAR (coarse.margin, fine.margin, 1e-6);
			EXPECT_NEAR (standard.margin, fine.margin, 1e-6);
			EXPECT_NEAR (coarse.y, fine.y, 1e-4);
			EXPECT_EQ (coarse.u, fine.u);
			std::printf ("%-7s %s : marge %+.6f (4) %+.6f (16) %+.6f (64) a u %.3f, y %.5f, R %.4f, 2 t + H %.3f\n", pose.name,
			             level == OrnamentDetail::Light ? "Light" : "Heavy", coarse.margin, standard.margin, fine.margin, fine.u, fine.y,
			             fine.radius, fine.need);
		}
}

// La courbure de la garde, en forme close, est celle du cercle par trois points voisins (pas de 1e-4 en y), sur
// les 8 meridiens de la garde (corps : u = 0 a u_corps par quarts, y dans [0, 1] ; arc : u = 0, u_cap / 2, u_cap,
// y dans [1, 2]), y au pas de 1e-3, a plus de 2 pas des coupures (raccords, debut de rampe, crete). Exigence
// |1/kappa - R_3| <= 1e-3 R_3, ecrite sur les courbures avec un plancher de 1e-6 / p, la ou le meridien est
// presque droit (inflexion du corps).
TEST (TEST_cgarchi_classical_acanthus_bend, the_curvature_guard_matches_three_close_points)
{
	constexpr double kStep = 1e-4;
	for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Pose &pose : {kPoses[0], kPoses[1]})
		{
			SCOPED_TRACE (std::string (pose.name) + (level == OrnamentDetail::Light ? " Light" : " Heavy"));
			const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, level);
			const detail::LeafSpec spec = detail::DefaultLeafSpec (level, detail::LeafStyle::Olive);
			const detail::LeafBend bend (Envelope (), spec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
			const double b = bend.BodyHalfWidth (), c = tpl.capHalfWidth;
			const std::array<double, 5> cuts {0.0, 1.0, 2.0, std::max (1.0, tpl.fade[1]), bend.CrestY ()};
			struct Run
			{
				double y0, y1, u;
			};
			const std::array<Run, 8> runs {{{0.0, 1.0, 0.0}, {0.0, 1.0, 0.25 * b}, {0.0, 1.0, 0.5 * b}, {0.0, 1.0, 0.75 * b}, {0.0, 1.0, b},
			                                {1.0, 2.0, 0.0}, {1.0, 2.0, 0.5 * c}, {1.0, 2.0, c}}};
			double worst = 0.0;
			unsigned points = 0;
			for (const Run &run : runs)
				for (int i = 0; i <= 1000; ++i)
				{
					const double y = run.y0 + (run.y1 - run.y0) * i / 1000.0;
					if (std::any_of (cuts.begin (), cuts.end (), [&] (double cut) { return std::fabs (y - cut) <= 2.0 * kStep; }))
						continue;
					const std::array<double, 2> p = bend.Meridian (run.u, y - kStep), q = bend.Meridian (run.u, y),
					                            r = bend.Meridian (run.u, y + kStep);
					const double area2 = std::fabs ((q[0] - p[0]) * (r[1] - p[1]) - (r[0] - p[0]) * (q[1] - p[1]));
					const double chords = std::hypot (q[0] - p[0], q[1] - p[1]) * std::hypot (r[0] - q[0], r[1] - q[1]) *
					                      std::hypot (p[0] - r[0], p[1] - r[1]);
					const double circle = 2.0 * area2 / chords;   // 1 / R_3
					const double kappa = bend.MeridianCurvature (run.u, y);
					EXPECT_LE (std::fabs (kappa - circle), 1e-3 * circle + 1e-6) << "u = " << run.u << ", y = " << y;
					worst = std::max (worst, std::fabs (kappa - circle) / std::max (circle, 1e-3));
					++points;
				}
			EXPECT_GT (points, 7000u);
			std::printf ("%-7s %s : forme close contre trois points, %u points, ecart relatif maximal %.2e\n", pose.name,
			             level == OrnamentDetail::Light ? "Light" : "Heavy", points, worst);
		}
}

// La garde de courbure exige 2 t + H : sans relief (reliefScale 0), la marge croit de plus de 0,05 p.
TEST (TEST_cgarchi_classical_acanthus_bend, the_curvature_guard_requires_the_relief)
{
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
	for (const Pose &pose : {kPoses[0], kPoses[1]})
	{
		SCOPED_TRACE (pose.name);
		detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
		const detail::LeafPlacement p = detail::DefaultLeafPlacement (pose.kind, pose.theta);
		const detail::LeafCurvatureMargin withRelief = detail::LeafBend (Envelope (), spec, p, tpl).CurvatureMargin ();
		spec.reliefScale = 0.0;
		const detail::LeafCurvatureMargin bare = detail::LeafBend (Envelope (), spec, p, tpl).CurvatureMargin ();
		EXPECT_GT (bare.margin, withRelief.margin + 0.05);
		EXPECT_NEAR (bare.need, 2.0 * spec.thickness, 1e-12);
		EXPECT_TRUE (ThrowsWith ([&] { static_cast<void> (detail::LeafBend (Envelope (), spec, p, tpl).CurvatureMargin (1)); },
		                         "au moins 2 echantillons"));
	}
}

// Vidage des meridiens (axe et bord de la calotte) pour la planche sur la pl. XXV :
// meridians_a1e.csv dans le repertoire courant.
TEST (TEST_cgarchi_classical_acanthus_bend, DISABLED_dump_meridians)
{
	std::FILE *f = OpenForWriting ("meridians_a1e.csv");
	ASSERT_NE (f, nullptr);
	std::fprintf (f, "rank,u,y,r,h,re,he,bound\n");
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy);
	const detail::LeafSpec spec = detail::DefaultLeafSpec (OrnamentDetail::Heavy, detail::LeafStyle::Olive);
	const double back = 0.5 * spec.thickness + tpl.backHeight;
	for (const Pose &pose : {kPoses[0], kPoses[1]})
	{
		const detail::LeafBend bend (Envelope (), spec, detail::DefaultLeafPlacement (pose.kind, pose.theta), tpl);
		for (const double u : {0.0, tpl.capHalfWidth})
			for (int i = 0; i <= 1200; ++i)
			{
				const double y = -1.0 + 3.0 * i / 1200;
				if (y < 0.0 && bend.StemLength () <= 0.0)
					continue;
				const std::array<double, 2> s = bend.Meridian (u, y), n = bend.BackNormal (u, y);
				std::fprintf (f, "%c,%.4f,%.5f,%.6f,%.6f,%.6f,%.6f,%.6f\n", pose.name[0], u, y, s[0], s[1], s[0] + back * n[0],
				              s[1] + back * n[1], Envelope ().LeafBoundRadius (s[1]));
			}
	}
	std::fclose (f);
}
