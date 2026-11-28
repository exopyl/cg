#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Feuilles D (foglie minori) seules : extensions de la porteuse (registre AC-53, AC-54, AC-57, AC-58)
// ===========================================================================
//
// Cotes de la pl. XXVI et du plan de la pl. XXV, ecrites ici en litteraux : pied au haut de la bague (h 17,8) ; D interieur, pointe
// r 19,0 h 25,5 a theta_C - (17 a 19,5) deg, crete h 26,5 ; D exterieur, crete h 28,0, pointe r 26,2 h 25,8 (profil
// corrige AC-44) a theta_C + (18,5 a 20) deg. Tolerance des cotes du bec : 1 p (bec en arc, crosse gravee en spirale).

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
using Side = detail::MinorLeafSide;

const detail::CapitalEnvelope &Env ()
{
	static const detail::CapitalEnvelope env = [] {
		ColumnOptions options;
		options.order = Order::Corinthian;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		return detail::ReadCapitalEnvelope (order, BuildColumnProfile (order, options));
	}();
	return env;
}

const char *Name (Side s)
{
	return s == Side::Inner ? "D interieur" : "D exterieur";
}

detail::LeafSheet Sheet (Side side, OrnamentDetail level, const detail::LeafPlacement &placement)
{
	return detail::BuildLeafSheet (Env (), detail::DefaultMinorLeafSpec (level, side), placement,
	                               detail::LeafTemplateOf (detail::LeafStyle::OliveMinor, level));
}

// Sommet de la face exterieure de y maximal : l'apex du gabarit (u = 0, y = 2), pointe du bec.
std::array<double, 3> Apex (const detail::LeafSheet &s)
{
	unsigned apex = 0;
	for (unsigned v = 0; v < s.faceVertices; ++v)
		if (s.planar[v][1] > s.planar[apex][1])
			apex = v;
	float p[3] {};
	s.mesh->GetVertex (apex, p);
	return {std::hypot (p[0], p[1]) / Env ().scale, p[2] / Env ().scale - Env ().foot, std::atan2 (p[1], p[0])};
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

} // namespace

// AC-54, AC-57 : la pose suit la tige et le cote. Tige a pi/8 (l'axe de face 0 est en dessous) : D interieur pose et penche vers
// les theta decroissants, D exterieur vers les croissants ; tige a 3 pi/8 (axe de face pi/2 au-dessus) : l'inverse.
// Les pieds sont de part et d'autre de la tige, sur le dessus de la bague (h 17,87, footOffset 0,5).
TEST (TEST_cgarchi_classical_acanthus_minor, the_pose_follows_the_stem_and_the_side)
{
	for (const double stem : {kPi / 8.0, 3.0 * kPi / 8.0, -kPi / 8.0, 13.0 * kPi / 8.0})
	{
		const detail::LeafPlacement in = detail::DefaultMinorLeafPlacement (Env (), Side::Inner, stem);
		const detail::LeafPlacement out = detail::DefaultMinorLeafPlacement (Env (), Side::Outer, stem);
		const double towardFace = std::remainder (stem, 0.5 * kPi) > 0.0 ? -1.0 : 1.0;
		EXPECT_GT (in.axisLean * towardFace, 0.0) << stem;
		EXPECT_LT (out.axisLean * towardFace, 0.0) << stem;
		EXPECT_GT (std::remainder (in.theta - stem, 2.0 * kPi) * towardFace, 0.0) << stem;
		EXPECT_LT (std::remainder (out.theta - stem, 2.0 * kPi) * towardFace, 0.0) << stem;
		for (const detail::LeafPlacement &p : {in, out})
		{
			EXPECT_DOUBLE_EQ (p.hFoot, 17.87);
			EXPECT_DOUBLE_EQ (p.hStem, p.hFoot);
			EXPECT_DOUBLE_EQ (p.footOffset, 0.5);
			EXPECT_TRUE (p.explicitBend);
		}
		EXPECT_DOUBLE_EQ (in.bodyHold, 0.0);
		EXPECT_DOUBLE_EQ (out.bodyHold, 23.1);
	}
	ExpectInvalid ([] { detail::DefaultMinorLeafPlacement (Env (), Side::Inner, std::nan ("")); }, "stemTheta non fini");
}

// AC-53 : en pose explicite, l'arc du bec est centre en bendCenter : h_b = h du centre, off_b = r - R_S - Base (h).
TEST (TEST_cgarchi_classical_acanthus_minor, the_explicit_bend_sits_on_its_center)
{
	for (const Side side : {Side::Inner, Side::Outer})
	{
		SCOPED_TRACE (Name (side));
		const detail::LeafPlacement p = detail::DefaultMinorLeafPlacement (Env (), side, kPi / 8.0);
		const detail::LeafSpec spec = detail::DefaultMinorLeafSpec (OrnamentDetail::Light, side);
		const detail::LeafBend bend (Env (), spec, p, detail::LeafTemplateOf (detail::LeafStyle::OliveMinor, OrnamentDetail::Light));
		const double base = Env ().CoreRadius (p.bendCenter[1]) + spec.clearance + 0.5 * spec.thickness;
		EXPECT_DOUBLE_EQ (bend.BendStartHeight (), p.bendCenter[1]);
		EXPECT_NEAR (bend.BendStartOffset (), p.bendCenter[0] - bend.BendRadius () - base, 1e-12);
		// Pied au meridien d'axe : footOffset, creuse par la gouttiere du corps (registre AC-9).
		EXPECT_NEAR (bend.Meridian (0.0, 0.0)[0], Env ().CoreRadius (17.87) + spec.clearance + 0.5 * spec.thickness + 0.5 * (1.0 - bend.GutterDepth ()), 1e-9);
		const detail::LeafCurvatureMargin m = bend.CurvatureMargin ();
		EXPECT_GE (m.margin, 0.1);
		std::printf ("%s : R_S %.4f, off_b %.4f, garde de courbure %+.4f p\n", Name (side), bend.BendRadius (), bend.BendStartOffset (), m.margin);
	}
}

// AC-57, K16, K25 : la pointe est posee sur son azimut releve par l'inclinaison de l'axe, et les cotes du bec tiennent.
TEST (TEST_cgarchi_classical_acanthus_minor, the_minor_leaves_meet_the_plate_dimensions)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		for (const Side side : {Side::Inner, Side::Outer})
		{
			SCOPED_TRACE (std::string (Name (side)) + (level == OrnamentDetail::Light ? ", Light" : ", Heavy"));
			const detail::LeafSheet s = Sheet (side, level, detail::DefaultMinorLeafPlacement (Env (), side, kPi / 8.0));
			const std::array<double, 3> apex = Apex (s);
			double crest = -1e9, widest = -1e9;
			for (unsigned v = 0; v < static_cast<unsigned> (s.mesh->GetNVertices ()); ++v)
			{
				float p[3] {};
				s.mesh->GetVertex (v, p);
				crest = std::max (crest, static_cast<double> (p[2]) / Env ().scale - Env ().foot);
				widest = std::max (widest, std::hypot (p[0], p[1]) / Env ().scale);
			}
			const double offset = (apex[2] - kPi / 8.0) / kDeg;
			if (side == Side::Inner)
			{
				EXPECT_GE (-offset, 16.9);
				EXPECT_LE (-offset, 19.5);
				EXPECT_NEAR (apex[0], 19.0, 1.0);
				EXPECT_NEAR (apex[1], 25.5, 1.0);
				EXPECT_NEAR (crest, 26.5, 1.0);
				EXPECT_NEAR (widest, 19.5, 0.6);   // tete lobee r 19,4 a 19,6 (plan)
			}
			else
			{
				EXPECT_GE (offset, 18.5);
				EXPECT_LE (offset, 20.0);
				EXPECT_NEAR (apex[0], 26.2, 1.0);
				EXPECT_NEAR (apex[1], 25.8, 1.0);
				EXPECT_NEAR (crest, 28.0, 1.0);
				EXPECT_NEAR (widest, 27.4, 1.0);
			}
			MeshValidationOptions one;
			one.expectedComponents = 1;
			const MeshValidationReport r = ValidateMesh (*s.mesh, one);
			std::string defects;
			for (std::size_t k = 0; k < kMeshDefectCount; ++k)
				if (r.counts[k])
					defects += std::string (" ") + MeshDefectName (static_cast<MeshDefect> (k)) + "=" + std::to_string (r.counts[k]);
			EXPECT_TRUE (r.IsValid ()) << defects;
			std::printf ("%s %s : pointe r %.3f h %.3f a %+.2f deg de la tige ; crete h %.3f ; rayon maximal %.3f p ; alpha %+.2f deg (D-7 : 35 / 30)\n", Name (side),
			             level == OrnamentDetail::Light ? "Light" : "Heavy", apex[0], apex[1], offset, crest, widest,
			             detail::DefaultMinorLeafPlacement (Env (), side, kPi / 8.0).axisLean * 180.0 / kPi);
		}
}

// AC-53, AC-54, AC-57, AC-58 : gardes de pose, chacune vue par un fragment de son message.
TEST (TEST_cgarchi_classical_acanthus_minor, the_pose_guards_are_seen)
{
	const auto build = [] (const std::function<void (detail::LeafPlacement &)> &change) {
		detail::LeafPlacement p = detail::DefaultMinorLeafPlacement (Env (), Side::Outer, kPi / 8.0);
		change (p);
		Sheet (Side::Outer, OrnamentDetail::Light, p);
	};
	EXPECT_NO_THROW (build ([] (detail::LeafPlacement &) {}));
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.footOffset = -0.1; }); }, "footOffset fini et >= 0");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.hStem = 17.0; }); }, "footOffset nul avec une bande de tige");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.axisLean = 61.0 * kDeg; }); }, "axisLean dans ]-60, 60[");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bendCenter = {40.0, 26.0}; }); }, "pose explicite au-dessus de la borne");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bendCenter = {15.5, 26.0}; }); }, "off_b < 0");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bendCenter = {std::nan (""), 26.0}; }); }, "bendCenter fini");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bodyHold = -1.0; }); }, "bodyHold fini et >= 0");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bodyHold = 26.5; }); }, "bodyHold dans ]hFoot, h_b[");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.bodyHold = 18.0; }); }, "deux rayons de coude");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) { p.footLean = 0.1; }); }, "footLean nul requis");
	ExpectInvalid ([&] { build ([] (detail::LeafPlacement &p) {
		               p.bodyHold = 0.0;
		               p.footOffset = 14.0;
	               }); },
	               "corps au-dessus de la borne");
	// Point fixe du pied : converge en quelques pas (garde a 50).
	for (const Side side : {Side::Inner, Side::Outer})
	{
		int steps = -1;
		detail::DefaultMinorLeafPlacement (Env (), side, kPi / 8.0, &steps);
		EXPECT_GT (steps, 0);
		EXPECT_LT (steps, 50);
		std::printf ("%s : point fixe du pied en %d pas\n", Name (side), steps);
	}
}

// K26 : les extensions de la porteuse (AC-53, AC-54, AC-57, AC-58) restent a leurs defauts pour F et E, quel que soit l'azimut : F et
// E ne dependent pas des feuilles D.
TEST (TEST_cgarchi_classical_acanthus_minor, the_extensions_stay_off_for_F_and_E)
{
	for (const CapitalPieceKind kind : {CapitalPieceKind::LowerLeaf, CapitalPieceKind::MiddleLeaf})
		for (const double theta : {0.0, kPi / 8.0, 3.0 * kPi / 8.0})
		{
			const detail::LeafPlacement p = detail::DefaultLeafPlacement (kind, theta);
			EXPECT_EQ (p.footOffset, 0.0) << static_cast<int> (kind);
			EXPECT_EQ (p.axisLean, 0.0) << static_cast<int> (kind);
			EXPECT_FALSE (p.explicitBend) << static_cast<int> (kind);
			EXPECT_EQ (p.bodyHold, 0.0) << static_cast<int> (kind);
		}
}

// AC-58 : corps tenu du D exterieur : au meridien d'axe, au plus 1 p de la corbeille (off) jusqu'a h 24,3, puis depart
// du bec vertical a (off_b, h_b) ; derivees continues aux raccords (coudes tangents).
TEST (TEST_cgarchi_classical_acanthus_minor, the_held_body_hugs_the_bell_up_to_the_crest_of_E)
{
	const detail::LeafPlacement p = detail::DefaultMinorLeafPlacement (Env (), Side::Outer, kPi / 8.0);
	const detail::LeafSpec spec = detail::DefaultMinorLeafSpec (OrnamentDetail::Light, Side::Outer);
	const detail::LeafBend bend (Env (), spec, p, detail::LeafTemplateOf (detail::LeafStyle::OliveMinor, OrnamentDetail::Light));
	double worst = -1e9, lastH = -1e9;
	for (int i = 0; i <= 2000; ++i)
	{
		const std::array<double, 2> q = bend.Meridian (0.0, i / 2000.0);
		const double off = q[0] - (Env ().CoreRadius (q[1]) + spec.clearance + 0.5 * spec.thickness);
		if (q[1] <= 24.3)
			worst = std::max (worst, off);
		EXPECT_GE (q[1], lastH - 1e-12) << i;
		lastH = q[1];
	}
	EXPECT_LE (worst, 1.0);
	const std::array<double, 2> start = bend.Meridian (0.0, 1.0);
	EXPECT_NEAR (start[1], p.bendCenter[1], 1e-9);
	// Depart du bec vertical dans le plan (off, h) : r porte en plus la pente du noyau.
	const auto offOf = [&] (const std::array<double, 2> &q) { return q[0] - (Env ().CoreRadius (q[1]) + spec.clearance + 0.5 * spec.thickness); };
	const std::array<double, 2> a = bend.Meridian (0.0, 1.0 - 1e-6), b = bend.Meridian (0.0, 1.0 + 1e-6);
	EXPECT_LT (std::fabs (offOf (b) - offOf (a)) / std::hypot (offOf (b) - offOf (a), b[1] - a[1]), 1e-3);
	std::printf ("D exterieur : off maximal sous h 24,3 : %.4f p\n", worst);
}

// Cache des poses des feuilles D (DefaultMinorLeafPlacements) : chaque pose est celle du point fixe calcule sur
// l'enveloppe passee, a l'octet ; la cle est le contenu de l'enveloppe en parties. Chaque groupe de cotes que le
// point fixe lit (distance a l'axe, hauteurs des bandes, noyau, cone) est change seul, sur une enveloppe qui deplace
// au moins une pose : le cache doit rendre la pose de l'enveloppe changee, pas celle de la reference deja gardee. La
// meme enveloppe a l'echelle 2 donne la meme pose. Des fils concurrents parcourent plus d'enveloppes que le cache
// n'a de places (insertions et evictions concurrentes) et lisent chacun la pose de l'enveloppe passee.
TEST (TEST_cgarchi_classical_acanthus_minor, the_cached_poses_follow_the_envelope_content)
{
	const auto same = [] (const detail::LeafPlacement &a, const detail::LeafPlacement &b) {
		return a.theta == b.theta && a.hFoot == b.hFoot && a.hTop == b.hTop && a.hStem == b.hStem &&
		       a.halfWidth == b.halfWidth && a.bendRadius == b.bendRadius && a.tipTurn == b.tipTurn &&
		       a.footLean == b.footLean && a.footOffset == b.footOffset && a.axisLean == b.axisLean &&
		       a.explicitBend == b.explicitBend && a.bendCenter == b.bendCenter && a.bodyHold == b.bodyHold;
	};
	const auto direct = [] (const detail::CapitalEnvelope &env) {
		std::array<detail::LeafPlacement, 4> poses;
		for (int k = 0; k < 2; ++k)
			for (int side = 0; side < 2; ++side)
				poses[static_cast<std::size_t> (2 * k + side)] = detail::DefaultMinorLeafPlacement (
				    env, side == 0 ? Side::Inner : Side::Outer, detail::RankTheta (CapitalPieceKind::Cauliculus, k));
		return poses;
	};
	const auto expectSame = [&] (const std::array<detail::LeafPlacement, 4> &a, const std::array<detail::LeafPlacement, 4> &b,
	                             const char *what) {
		for (std::size_t i = 0; i < 4; ++i)
			EXPECT_TRUE (same (a[i], b[i])) << what << ", pose " << i;
	};
	const detail::CapitalEnvelope &env = Env ();
	const std::array<detail::LeafPlacement, 4> reference = direct (env);
	expectSame (detail::DefaultMinorLeafPlacements (env), reference, "enveloppe de reference");
	expectSame (detail::DefaultMinorLeafPlacements (env), reference, "second appel");

	detail::CapitalEnvelope scaled = env;
	scaled.scale = 2.0 * env.scale;
	scaled.foot = env.foot + 3.0;
	expectSame (direct (scaled), reference, "point fixe direct, echelle 2, pied decale");
	expectSame (detail::DefaultMinorLeafPlacements (scaled), reference, "echelle 2, pied decale");

	detail::CapitalEnvelope invalid = env;
	invalid.scale = 0.0;
	EXPECT_THROW (detail::DefaultMinorLeafPlacements (invalid), std::invalid_argument) << "enveloppe invalide, cle deja en cache";

	const auto differ = [&] (const std::array<detail::LeafPlacement, 4> &a, const std::array<detail::LeafPlacement, 4> &b) {
		bool any = false;
		for (std::size_t i = 0; i < 4; ++i)
			any = any || !same (a[i], b[i]);
		return any;
	};
	const auto moves = [&] (const std::array<detail::LeafPlacement, 4> &poses) { return differ (poses, reference); };
	struct Variant
	{
		const char *what;
		std::function<void (detail::CapitalEnvelope &)> change;
	};
	const Variant variants[] = {
	    {"distance a l'axe + 0,3", [] (detail::CapitalEnvelope &e) { e.axisDistance += 0.3; }},
	    {"limites interieures des bandes + 0,1",
	     [] (detail::CapitalEnvelope &e) {
		     for (std::size_t i = 0; i + 1 < e.bands.size (); ++i)
		     {
			     e.bands[i].h1 += 0.1;
			     e.bands[i + 1].h0 += 0.1;
		     }
	     }},
	    {"noyau releve de 0,3",
	     [] (detail::CapitalEnvelope &e) {
		     for (detail::CapitalEnvelopeBand &band : e.bands)
		     {
			     band.core.bottom += 0.3;
			     band.core.top += 0.3;
		     }
	     }},
	};
	ASSERT_GE (env.bands.size (), 2u);
	for (const Variant &v : variants)
	{
		SCOPED_TRACE (v.what);
		detail::CapitalEnvelope changed = env;
		v.change (changed);
		const std::array<detail::LeafPlacement, 4> moved = direct (changed);
		ASSERT_TRUE (moves (moved)) << "la variante doit deplacer une pose";
		expectSame (detail::DefaultMinorLeafPlacements (env), reference, "reference gardee");
		expectSame (detail::DefaultMinorLeafPlacements (changed), moved, v.what);
	}
	// Le cone ne deplace pas les poses explicites mais borne le bec (garde de la pose) : un cone abaisse jusqu'a ce que
	// le calcul direct leve doit lever aussi par le cache, au lieu d'y trouver la pose de la reference.
	detail::CapitalEnvelope lowered = env;
	double drop = 0.0;
	for (const double d : {0.5, 1.0, 2.0, 4.0, 8.0})
	{
		lowered = env;
		for (detail::CapitalEnvelopeBand &band : lowered.bands)
		{
			band.envelope.bottom -= d;
			band.envelope.top -= d;
		}
		try
		{
			direct (lowered);
		}
		catch (const std::invalid_argument &)
		{
			drop = d;
			break;
		}
	}
	ASSERT_GT (drop, 0.0) << "aucun cone abaisse ne leve";
	expectSame (detail::DefaultMinorLeafPlacements (env), reference, "reference gardee");
	EXPECT_THROW (detail::DefaultMinorLeafPlacements (lowered), std::invalid_argument) << "cone abaisse de " << drop << " p";
	std::printf ("Cache des poses : cone abaisse de %g p refuse par la pose\n", drop);

	constexpr std::size_t kEnvelopes = 12, kThreads = 8;   // plus d'enveloppes que de places (8)
	std::vector<detail::CapitalEnvelope> envelopes (kEnvelopes, env);
	std::vector<std::array<detail::LeafPlacement, 4>> expected;
	for (std::size_t i = 0; i < kEnvelopes; ++i)
	{
		for (detail::CapitalEnvelopeBand &band : envelopes[i].bands)
		{
			band.core.bottom += 0.02 * static_cast<double> (i + 1);
			band.core.top += 0.02 * static_cast<double> (i + 1);
		}
		expected.push_back (direct (envelopes[i]));
	}
	for (std::size_t i = 1; i < kEnvelopes; ++i)
		ASSERT_TRUE (differ (expected[i], expected[i - 1])) << "enveloppes " << i - 1 << " et " << i << " : memes poses";
	std::vector<std::thread> threads;
	std::array<int, kThreads> wrong {};
	for (std::size_t t = 0; t < kThreads; ++t)
		threads.emplace_back ([&, t] {
			for (std::size_t round = 0; round < 2; ++round)
				for (std::size_t j = 0; j < kEnvelopes; ++j)
				{
					const std::size_t i = (j + 5 * t + round) % kEnvelopes;
					const std::array<detail::LeafPlacement, 4> got = detail::DefaultMinorLeafPlacements (envelopes[i]);
					for (std::size_t q = 0; q < 4; ++q)
						wrong[t] += same (got[q], expected[i][q]) ? 0 : 1;
				}
		});
	for (std::thread &th : threads)
		th.join ();
	for (std::size_t t = 0; t < kThreads; ++t)
		EXPECT_EQ (wrong[t], 0) << "fil " << t;
}
