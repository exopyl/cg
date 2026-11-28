#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;

// ===========================================================================
//  Tiges des caulicoles (C) : K15 a K21, K24, K24', K25, K28
// ===========================================================================
//
// Cotes de la pl. XXVI (ecrites ici en litteraux, non lues dans le produit) : axe de la tige a
// pi/8 + k pi/4, r_C 16,9 p, diametre 2,0 p, bague de 15,3 a 17,8 p (gorge 16,6), cordons de rayon 1,2 et 1,55 p,
// pied a 12,40 p (crete extrados de F 12,35 + c), tete a 17,8 p.

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 1e-4;   // p : sommets stockes en float
constexpr int kPerRank = 8;
constexpr std::size_t kFirstStem = 16;   // 8 F et 8 E avant les tiges (registre AC-46)

using Point = std::array<double, 3>;

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

ColumnProfile CorinthianProfile ()
{
	ColumnOptions options;
	options.order = Order::Corinthian;
	return BuildColumnProfile (ClassicalOrders ().Get (Order::Corinthian), options);
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
		const ColumnProfile profile = CorinthianProfile ();
		for (const OrnamentDetail detail : {OrnamentDetail::Light, OrnamentDetail::Heavy})
			for (const double scale : {1.0, 2.0})
			{
				AcanthusOptions options;
				options.detail = detail;
				options.scale = scale;
				out.push_back ({detail, scale, detail::ReadCapitalEnvelope (order, profile, scale),
				                BuildCorinthianOrnament (order, profile, options)});
			}
		return out;
	}();
	return cases;
}

Point Parts (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned v)
{
	float p[3] {};
	mesh.GetVertex (v, p);
	return {p[0] / env.scale, p[1] / env.scale, p[2] / env.scale - env.foot};
}

// Axe de la tige k, d'apres les planches.
double StemAxis (int k)
{
	return kPi / 8.0 + k * kPi / 4.0;
}

const CapitalPiece &Stem (const Case &c, int k)
{
	return c.ornament.pieces[kFirstStem + static_cast<std::size_t> (k)];
}

// Les 60 pieces et les 8 tiges a leur place (registre AC-46), avant toute lecture par Stem.
::testing::AssertionResult HasStems (const Case &c)
{
	if (c.ornament.pieces.size () != 7u * kPerRank + detail::kFleurons)
		return ::testing::AssertionFailure () << c.ornament.pieces.size () << " pieces, 60 attendues";
	for (int k = 0; k < kPerRank; ++k)
		if (Stem (c, k).kind != CapitalPieceKind::Cauliculus)
			return ::testing::AssertionFailure () << "piece " << kFirstStem + static_cast<std::size_t> (k) << " : pas une tige";
	return ::testing::AssertionSuccess ();
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

#define CASE_TRACE(c) SCOPED_TRACE (std::string (LevelName ((c).detail)) + ", echelle " + std::to_string ((c).scale))

} // namespace

// K15 : grille de la tige, n_phi = N s (24 / 48) et n_z = 2 + rampe + 2 cordons (15 / 32) ; triangles 2 n_phi n_z,
// sommets n_phi n_z + 2 ; une coque fermee par tige.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stem_grid_has_its_counts)
{
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (ClassicalOrders ().Get (Order::Corinthian), CorinthianProfile ());
	struct Expected
	{
		OrnamentDetail detail;
		unsigned nPhi, nZ, faces, vertices;
	};
	for (const Expected &e : {Expected {OrnamentDetail::Light, 24, 15, 720, 362}, Expected {OrnamentDetail::Heavy, 48, 32, 3072, 1538}})
	{
		SCOPED_TRACE (LevelName (e.detail));
		const detail::CauliculusSpec spec = detail::DefaultCauliculusSpec (e.detail);
		const detail::CauliculusSheet sheet = detail::BuildCauliculusSheet (env, spec, StemAxis (0));
		EXPECT_EQ (sheet.angularSamples, e.nPhi);
		EXPECT_EQ (sheet.rings, e.nZ);
		EXPECT_EQ (sheet.mesh->GetNFaces (), static_cast<int> (e.faces));
		EXPECT_EQ (sheet.mesh->GetNVertices (), static_cast<int> (e.vertices));
		const std::vector<double> rings = detail::CauliculusRings (spec);
		ASSERT_EQ (rings.size (), e.nZ);
		EXPECT_TRUE (std::is_sorted (rings.begin (), rings.end ()));
		EXPECT_EQ (std::adjacent_find (rings.begin (), rings.end ()), rings.end ());
		MeshValidationOptions one;
		one.expectedComponents = 1;
		const MeshValidationReport r = ValidateMesh (*sheet.mesh, one);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_GT (r.signedVolume, 0.0);
	}
}

// K16, K25 : sur chaque tige de l'ornement, azimut de l'axe (centres des disques et barycentre), rayon de l'axe,
// diametre du fut, bague (hauteurs et rayons des cordons), pied et tete, contre les cotes de la planche.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stems_meet_the_plate_dimensions)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_EQ (c.ornament.pieces.size (), 7u * kPerRank + detail::kFleurons);
		const Mesh &mesh = *c.ornament.mesh;
		double worstAxis = 0.0, worstBary = 0.0, rcLo = 1e9, rcHi = -1e9;
		for (int k = 0; k < kPerRank; ++k)
		{
			const CapitalPiece &piece = Stem (c, k);
			ASSERT_EQ (piece.kind, CapitalPieceKind::Cauliculus) << k;
			// Les deux derniers sommets sont les centres des disques (pied, tete).
			const Point foot = Parts (mesh, c.env, piece.vertexEnd - 2), head = Parts (mesh, c.env, piece.vertexEnd - 1);
			const double axis = StemAxis (k);
			worstAxis = std::max ({worstAxis, std::fabs (std::remainder (std::atan2 (foot[1], foot[0]) - axis, 2.0 * kPi)),
			                       std::fabs (std::remainder (std::atan2 (head[1], head[0]) - axis, 2.0 * kPi))});
			const double rc = std::hypot (foot[0], foot[1]);
			rcLo = std::min (rcLo, rc);
			rcHi = std::max (rcHi, rc);
			double sx = 0.0, sy = 0.0, hLo = 1e9, hHi = -1e9;
			// Rayon local autour de l'axe de la tige, par tranche de h : fut (12,4 a 15,0), cordon bas, gorge, cordon haut.
			double shaft = 0.0, lower = 0.0, upper = 0.0, gorgeR = 1e9, hLowerFirst = 1e9;
			for (unsigned v = piece.vertexBegin; v + 2 < piece.vertexEnd; ++v)
			{
				const Point p = Parts (mesh, c.env, v);
				sx += p[0];
				sy += p[1];
				hLo = std::min (hLo, p[2]);
				hHi = std::max (hHi, p[2]);
				const double local = std::hypot (p[0] - foot[0], p[1] - foot[1]);
				if (p[2] < 15.0 - 1e-4)
					shaft = std::max (shaft, local);
				else if (p[2] > 15.3 + 1e-4 && p[2] < 16.6 - 1e-4)
					lower = std::max (lower, local);
				else if (p[2] > 16.6 + 1e-4 && p[2] < 17.8 - 1e-4)
					upper = std::max (upper, local);
				if (std::fabs (p[2] - 16.6) < 1e-4)
					gorgeR = std::min (gorgeR, local);
				if (local > 1.0 + 0.05)
					hLowerFirst = std::min (hLowerFirst, p[2]);
			}
			worstBary = std::max (worstBary, std::fabs (std::remainder (std::atan2 (sy, sx) - axis, 2.0 * kPi)));
			EXPECT_NEAR (hLo, 12.40, kTau) << k;            // pied (AC-52)
			EXPECT_NEAR (hHi, 17.8, 0.3) << k;              // tete, h_C1 (C-7)
			EXPECT_NEAR (2.0 * shaft, 2.0, 0.3) << k;       // diametre du fut (C-5)
			EXPECT_NEAR (lower, 1.2, 0.15) << k;            // cordon bas (C-4, largeur 2,4 p ; C-5 : +-0,3 p sur le diametre)
			EXPECT_NEAR (upper, 1.55, 0.15) << k;           // cordon haut (C-4, 2,75 a 3,4 p)
			EXPECT_NEAR (gorgeR, 1.0, 0.05) << k;           // gorge a h 16,6
			EXPECT_NEAR (hLowerFirst, 15.3, 0.5) << k;      // bas de la bague
			if (k == 0)
				std::printf ("%s, echelle %g : K25 tige 0 : pied %.4f, tete %.4f, diametre du fut %.4f, cordons %.4f / %.4f, gorge %.4f, "
				             "bas de bague %.4f p\n",
				             LevelName (c.detail), c.scale, hLo, hHi, 2.0 * shaft, lower, upper, gorgeR, hLowerFirst);
		}
		EXPECT_LE (worstAxis, 1e-6);
		EXPECT_LE (worstBary, 1e-4);
		EXPECT_NEAR (rcLo, 16.9, 0.6);
		EXPECT_NEAR (rcHi, 16.9, 0.6);
		std::printf ("%s, echelle %g : K16 axe des tiges, ecart maximal %.2e rad (barycentre %.2e) ; r_C de %.6f a %.6f p\n",
		             LevelName (c.detail), c.scale, worstAxis, worstBary, rcLo, rcHi);
	}
}

// K17 : copie = construction. La tige k est, indice par indice, celle que BuildCauliculusSheet pose a StemAxis (k)
// (4 ulp float a r ~ 18 p), triangles identiques au decalage de la piece pres.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, each_stem_copy_is_the_stem_built_at_its_place)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasStems (c));
		const Mesh &mesh = *c.ornament.mesh;
		double worst = 0.0;
		for (int k = 0; k < kPerRank; ++k)
		{
			const CapitalPiece &piece = Stem (c, k);
			const detail::CauliculusSheet sheet = detail::BuildCauliculusSheet (c.env, detail::DefaultCauliculusSpec (c.detail), StemAxis (k));
			const Mesh &stem = *sheet.mesh;
			ASSERT_EQ (piece.vertexEnd - piece.vertexBegin, static_cast<unsigned> (stem.GetNVertices ())) << k;
			ASSERT_EQ (piece.faceEnd - piece.faceBegin, static_cast<unsigned> (stem.GetNFaces ())) << k;
			for (unsigned v = 0; v < static_cast<unsigned> (stem.GetNVertices ()); ++v)
			{
				const Point a = Parts (mesh, c.env, piece.vertexBegin + v), b = Parts (stem, c.env, v);
				worst = std::max ({worst, std::fabs (a[0] - b[0]), std::fabs (a[1] - b[1]), std::fabs (a[2] - b[2])});
			}
			bool same = true;
			for (unsigned f = 0; f < static_cast<unsigned> (stem.GetNFaces ()); ++f)
				for (unsigned j = 0; j < 3; ++j)
					same = same && static_cast<unsigned> (mesh.GetFaceVertex (piece.faceBegin + f, j)) ==
					                   piece.vertexBegin + static_cast<unsigned> (stem.GetFaceVertex (f, j));
			EXPECT_TRUE (same) << k;
		}
		EXPECT_LE (worst, 8e-6);
		std::printf ("%s, echelle %g : K17 ecart maximal copie / construction des tiges %.2e p\n", LevelName (c.detail), c.scale, worst);
	}
}

// K18 : chaque tige est symetrique par son plan radial (theta = axe) : la reflexion de ses sommets par ce plan
// retombe sur ses sommets (plus proche voisin), et l'ensemble des 8 tiges est invariant par la rotation de pi/4.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stems_have_the_symmetry_of_the_octagon)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasStems (c));
		const Mesh &mesh = *c.ornament.mesh;
		std::vector<Point> all;
		for (int k = 0; k < kPerRank; ++k)
			for (unsigned v = Stem (c, k).vertexBegin; v < Stem (c, k).vertexEnd; ++v)
				all.push_back (Parts (mesh, c.env, v));
		const auto nearest = [&all] (const Point &q) {
			double best = std::numeric_limits<double>::infinity ();
			for (const Point &p : all)
				best = std::min (best, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]));
			return best;
		};
		double reflection = 0.0, rotation = 0.0;
		const double cr = std::cos (kPi / 4.0), sr = std::sin (kPi / 4.0);
		for (const Point &p : all)
		{
			// Reflexion par le plan d'axe pi/8 (axe de la tige 0) : angle a -> pi/4 - a.
			const double a = std::atan2 (p[1], p[0]), r = std::hypot (p[0], p[1]);
			reflection = std::max (reflection, nearest ({r * std::cos (kPi / 4.0 - a), r * std::sin (kPi / 4.0 - a), p[2]}));
			rotation = std::max (rotation, nearest ({cr * p[0] - sr * p[1], sr * p[0] + cr * p[1], p[2]}));
		}
		EXPECT_LE (reflection, 1e-5);
		EXPECT_LE (rotation, 1e-5);
		std::printf ("%s, echelle %g : K18 tiges : reflexion %.2e p, rotation de pi/4 %.2e p\n", LevelName (c.detail), c.scale, reflection,
		             rotation);
	}
}

// K19, K20 : jeu au noyau (cordon haut : 16,9 - 1,55 - 15 = 0,35 p) et borne P4 (abaque, registre AC-48). L'abaque
// descend vers l'axe de face plus vite que la section du cordon haut ne recule : le plus grand debord est sur le
// flanc tourne vers l'axe de face, pas a 22,5 degres. Attendu : max sur la section (r_C 16,9, rayon 1,55, a h 17,2)
// de r - EnvelopeRadius, calcule ici sur les cotes de la planche.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stems_keep_off_the_bell_and_under_the_abacus)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasStems (c));
		double clearance = std::numeric_limits<double>::infinity (), excess = -clearance;
		for (int k = 0; k < kPerRank; ++k)
		{
			clearance = std::min (clearance, detail::CoreClearance (*c.ornament.mesh, c.env, Stem (c, k)));
			excess = std::max (excess, detail::EnvelopeExcess (*c.ornament.mesh, c.env, Stem (c, k)));
		}
		double expected = -std::numeric_limits<double>::infinity ();
		for (int i = 0; i < 3600; ++i)
		{
			const double alpha = 2.0 * kPi * i / 3600.0, x = 16.9 + 1.55 * std::cos (alpha), y = 1.55 * std::sin (alpha);
			const double theta = kPi / 8.0 + std::atan2 (y, x);
			expected = std::max (expected, std::hypot (x, y) - c.env.EnvelopeRadius (17.2, theta));
		}
		EXPECT_NEAR (clearance, 0.35, 0.01);
		EXPECT_GE (clearance, 0.05 - kTau);
		EXPECT_LE (excess, kTau);
		EXPECT_NEAR (excess, expected, 0.02);
		std::printf ("%s, echelle %g : K19 CoreClearance des tiges %.5f p ; K20 EnvelopeExcess %.5f p\n", LevelName (c.detail), c.scale,
		             clearance, excess);
	}
}

// K21 : ordre radial (registre AC-51). F devant la tige : aux noeuds (theta, h) communs a la face interieure de F et a
// la tige, rayon de F - c - rayon de la tige >= 0. Tige dans le jour entre deux E : aucun noeud commun entre la tige
// et ses deux E voisines ; sur toute la hauteur de la tige, une E de chaque cote, et jour lateral (au rayon r_C, minorant
// a deux pas de grille pres) entre le bord de la tige et celui de chaque E >= c.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, each_stem_stands_behind_its_lower_leaf_between_two_middle_leaves)
{
	using acanthus_test::RadiusGrid;
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasStems (c));
		const Mesh &mesh = *c.ornament.mesh;
		double frontF = std::numeric_limits<double>::infinity (), lateral = frontF;
		unsigned commonF = 0, commonE = 0, stemRows = 0, unflanked = 0;
		for (int k = 0; k < kPerRank; ++k)
		{
			const double axis = StemAxis (k);
			const CapitalPiece &t = Stem (c, k);
			RadiusGrid stemOut, fIn, eOut;
			acanthus_test::Rasterize (stemOut, mesh, c.env, t.faceBegin, t.faceEnd, true, axis);
			const CapitalPiece &f = c.ornament.pieces[static_cast<std::size_t> (k)];
			ASSERT_EQ (f.kind, CapitalPieceKind::LowerLeaf);
			acanthus_test::Rasterize (fIn, mesh, c.env, f.faceBegin, f.faceEnd, false, axis);
			for (const int ke : {k, (k + 1) % kPerRank})
			{
				const CapitalPiece &e = c.ornament.pieces[static_cast<std::size_t> (kPerRank + ke)];
				ASSERT_EQ (e.kind, CapitalPieceKind::MiddleLeaf);
				acanthus_test::Rasterize (eOut, mesh, c.env, e.faceBegin, e.faceEnd, true, axis);
			}
			for (int j = 0; j < RadiusGrid::kNH; ++j)
			{
				int lo = RadiusGrid::kNTheta, hi = -1;
				for (int i = 0; i < RadiusGrid::kNTheta; ++i)
				{
					const double rt = stemOut.At (i, j);
					if (std::isnan (rt))
						continue;
					lo = std::min (lo, i);
					hi = std::max (hi, i);
					if (!std::isnan (fIn.At (i, j)))
					{
						++commonF;
						frontF = std::min (frontF, fIn.At (i, j) - 0.05 - rt);
					}
					if (!std::isnan (eOut.At (i, j)))
						++commonE;
				}
				if (hi < 0)
					continue;
				++stemRows;
				int left = -1, right = RadiusGrid::kNTheta;
				for (int i = 0; i < RadiusGrid::kNTheta; ++i)
					if (!std::isnan (eOut.At (i, j)))
					{
						if (i < lo)
							left = std::max (left, i);
						if (i > hi)
							right = std::min (right, i);
					}
				// Chaque bord est entre deux noeuds : le jour vrai depasse (ecart en noeuds - 2) pas.
				const double step = RadiusGrid::kDTheta * kPi / 180.0 * 16.9;
				if (left >= 0)
					lateral = std::min (lateral, (lo - left - 2) * step);
				if (right < RadiusGrid::kNTheta)
					lateral = std::min (lateral, (right - hi - 2) * step);
				if (left < 0 || right >= RadiusGrid::kNTheta)
					++unflanked;
			}
		}
		EXPECT_GT (stemRows, 0u);
		EXPECT_EQ (unflanked, 0u);   // une E de chaque cote de la tige sur toute sa hauteur
		EXPECT_EQ (commonE, 0u);
		EXPECT_GE (lateral, 0.05);
		if (commonF > 0)
			EXPECT_GE (frontF, 0.0);
		std::printf ("%s, echelle %g : K21 F devant C : %u noeuds communs, marge %+.4f p ; C/E : %u noeuds communs, jour lateral "
		             "minimal (minorant) %.4f p, %u rangees de tige dont %u sans E d'un cote\n",
		             LevelName (c.detail), c.scale, commonF, frontF, commonE, lateral, stemRows, unflanked);
	}
}

// K24 (journal) et K24' : couverture angulaire F + E + C de la bande [1, 23], et part visible de la corbeille en
// elevation de face (|y| <= 10 p, h 1 a 23) contre V-1 trace sur la pl. XXVI (0,0557, deux traces, dispersion
// 0,011). Le critere de K24' (ecart <= max (5 points, dispersion)) porte sur l'ornement complet : ici, mesure.
// Apport minimal des tiges, d'apres les cotes du fut seul (diametre 2,0 p de h 12,4 a 17,8, a r_C 16,9) : en projection
// angulaire, 8 x 2 / 16,9 rad x 5,4 p sur 2 pi x 22 p, soit 0,037 ; en elevation, les deux tiges a +-22,5 degres
// dans la fenetre, 2 x 2,0 x 5,4 p sur 20 x 22 p, soit 0,049. Seuils a 80 % de ces aires.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stems_add_to_the_coverage_of_the_bell)
{
	for (const Case &c : Cases ())
	{
		CASE_TRACE (c);
		ASSERT_TRUE (HasStems (c));
		const std::vector<CapitalPiece> all (c.ornament.pieces.begin (), c.ornament.pieces.begin () + 3 * kPerRank);
		const std::vector<CapitalPiece> leaves (all.begin (), all.begin () + 2 * kPerRank);
		const double fe = detail::BellCoverage (*c.ornament.mesh, leaves, c.env, 1.0, 23.0);
		const double fec = detail::BellCoverage (*c.ornament.mesh, all, c.env, 1.0, 23.0);
		const double visibleFE = detail::VisibleBellFraction (*c.ornament.mesh, leaves, c.env, 10.0, 1.0, 23.0);
		const double visible = detail::VisibleBellFraction (*c.ornament.mesh, all, c.env, 10.0, 1.0, 23.0);
		EXPECT_GE (fec - fe, 0.8 * 8.0 * (2.0 / 16.9) * 5.4 / (2.0 * kPi * 22.0));
		EXPECT_GE (visibleFE - visible, 0.8 * 2.0 * 2.0 * 5.4 / (20.0 * 22.0));
		std::printf ("%s, echelle %g : K24 couverture angulaire F+E %.4f, F+E+C %.4f ; K24' corbeille visible en elevation F+E "
		             "%.4f, F+E+C %.4f (V-1 0,0557)\n",
		             LevelName (c.detail), c.scale, fe, fec, visibleFE, visible);
	}
}

// K28 : gardes de la tige, chacune vue par un fragment de son message.
TEST (TEST_cgarchi_classical_acanthus_cauliculus, the_stem_guards_are_seen)
{
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (ClassicalOrders ().Get (Order::Corinthian), CorinthianProfile ());
	const detail::CauliculusSpec base = detail::DefaultCauliculusSpec (OrnamentDetail::Light);
	EXPECT_NO_THROW (detail::BuildCauliculusSheet (env, base, StemAxis (0)));
	auto with = [&] (const std::function<void (detail::CauliculusSpec &)> &change) {
		detail::CauliculusSpec s = base;
		change (s);
		return s;
	};
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.hFoot = 12.0; }), StemAxis (0)); },
	               "pied de tige sous la crete de F");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.hFoot = 12.39; }), StemAxis (0)); },
	               "pied de tige sous la crete de F");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.axisRadius = 16.5; }), StemAxis (0)); },
	               "tige dans la corbeille");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.flutes = 0; }), StemAxis (0)); }, "flutes");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.fluteDepth = 1.0; }), StemAxis (0)); }, "cannelure");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.fluteWidth = 0.9; }), StemAxis (0)); }, "cannelure");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.gorge = 18.0; }), StemAxis (0)); }, "hauteurs");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.upperCordRadius = 0.9; }), StemAxis (0)); },
	               "cordons");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, with ([] (auto &s) { s.detail = OrnamentDetail::None; }), StemAxis (0)); },
	               "detail Light ou Heavy");
	ExpectInvalid ([&] { detail::BuildCauliculusSheet (env, base, std::nan ("")); }, "theta fini");
	ExpectInvalid ([&] { detail::DefaultCauliculusSpec (OrnamentDetail::None); }, "Light ou Heavy attendu");
	// Le profil de rayon : fut a R, cordons a leur rayon au milieu, R aux bornes.
	EXPECT_DOUBLE_EQ (detail::CauliculusRadius (base, 14.0), 1.0);
	EXPECT_DOUBLE_EQ (detail::CauliculusRadius (base, 15.95), 1.2);
	EXPECT_DOUBLE_EQ (detail::CauliculusRadius (base, 17.2), 1.55);
	EXPECT_DOUBLE_EQ (detail::CauliculusRadius (base, 16.6), 1.0);
	EXPECT_DOUBLE_EQ (detail::CauliculusRadius (base, 17.8), 1.0);
}
