#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_partition.h>
#include <cgarchi/classical_peristyle.h>

// ===========================================================================
//  Partitions 1D : bornes cumulees, parties egales, intervalles entre parties
//  centrees
// ===========================================================================
//
// Valeurs d'essai non exactes en binaire (0,1 ; 0,2 ; 1/3), choisies pour que
// deux formules equivalentes en reels donnent des double differents : une
// egalite au bit pres les distingue.

using namespace cgarchi::classical;

namespace {

bool SameBits (double a, double b)
{
	return std::memcmp (&a, &b, sizeof (double)) == 0;
}

// ((n - k) lo + k hi) / n, chaque produit arrondi en double avant la somme :
// l'evaluation que promet EqualParts. Ecrite en une expression, la reference
// serait contractee en FMA par GCC ou Clang sur une cible a FMA, et ne serait
// plus celle du contrat.
double StrictPart (double lo, double hi, int n, int k)
{
	const volatile double left = static_cast<double> (n - k) * lo;
	const volatile double right = static_cast<double> (k) * hi;
	return (left + right) / static_cast<double> (n);
}

std::string RefusalOf (const std::function<void ()> &call)
{
	try
	{
		call ();
	}
	catch (const std::invalid_argument &e)
	{
		return e.what ();
	}
	return "";
}

void ExpectRefusedNaming (const std::function<void ()> &call, const std::string &name)
{
	const std::string refusal = RefusalOf (call);
	EXPECT_NE (refusal.find ("Partition : '" + name + "'"), std::string::npos) << "attendu : " << name << " ; obtenu : '"
	                                                                            << refusal << "'";
}

}  // namespace

// Q1
TEST (TEST_cgarchi_classical_partition, cumulative_bounds_are_the_running_sum)
{
	const double start = 1.9;
	const std::vector<double> lengths = {0.1, 0.2, 0.7, 1.0 / 3.0};
	const std::vector<double> b = CumulativeBounds (start, lengths, "bands");
	ASSERT_EQ (b.size (), 5u);
	// Somme courante, ecrite ici.
	const double r1 = start + 0.1;
	const double r2 = r1 + 0.2;
	const double r3 = r2 + 0.7;
	const double r4 = r3 + 1.0 / 3.0;
	EXPECT_TRUE (SameBits (b[0], start));
	EXPECT_TRUE (SameBits (b[1], r1));
	EXPECT_TRUE (SameBits (b[2], r2));
	EXPECT_TRUE (SameBits (b[3], r3));
	EXPECT_TRUE (SameBits (b[4], r4));
	for (std::size_t k = 0; k + 1 < b.size (); ++k)
		EXPECT_LT (b[k], b[k + 1]) << k;

	// Le jeu distingue la somme courante du debut plus une somme separee des
	// longueurs : sans quoi l'egalite ci-dessus ne prouverait rien.
	const double separate3 = start + ((0.1 + 0.2) + 0.7);
	ASSERT_FALSE (SameBits (separate3, r3)) << "jeu non discriminant";

	EXPECT_EQ (CumulativeBounds (-0.3, {}, "bands"), std::vector<double> {-0.3});

	const double nan = std::numeric_limits<double>::quiet_NaN (), inf = std::numeric_limits<double>::infinity ();
	ExpectRefusedNaming ([&] { CumulativeBounds (nan, lengths, "bands"); }, "bands");
	ExpectRefusedNaming ([&] { CumulativeBounds (inf, lengths, "bands"); }, "bands");
	ExpectRefusedNaming ([&] { CumulativeBounds (0.0, {0.1, nan}, "bands"); }, "bands[1]");
	ExpectRefusedNaming ([&] { CumulativeBounds (0.0, {0.1, 0.2, inf}, "bands"); }, "bands[2]");
	ExpectRefusedNaming ([&] { CumulativeBounds (0.0, {-inf}, "bands"); }, "bands[0]");
	ExpectRefusedNaming ([&] { CumulativeBounds (0.0, {0.1, 0.0}, "bands"); }, "bands[1]");
	ExpectRefusedNaming ([&] { CumulativeBounds (0.0, {0.1, 0.2, -1.0}, "bands"); }, "bands[2]");
	ExpectRefusedNaming ([&] { CumulativeBounds (std::numeric_limits<double>::max (), {std::numeric_limits<double>::max ()}, "bands"); },
	                     "bands[0]");
}

// Q2
TEST (TEST_cgarchi_classical_partition, equal_parts_are_exact_at_the_ends_and_symmetric)
{
	const std::vector<std::pair<double, double>> intervals = {
		{0.1, 0.7}, {-1.3, 2.9}, {0.3, 1.0 / 3.0 + 1.0}, {1.73, 3.63}, {-3.31, -1.7},
		{-0.2, 0.1}, {2.68, 3.31}, {1e-3, 1.0 / 7.0}, {-5.715, 6.551}, {0.0, 0.3}};
	for (const auto &[lo, hi] : intervals)
		for (int n = 1; n <= 7; ++n)
		{
			SCOPED_TRACE (std::to_string (lo) + " " + std::to_string (hi) + " n=" + std::to_string (n));
			const std::vector<double> b = EqualParts (lo, hi, n, "peristyle");
			ASSERT_EQ (b.size (), static_cast<std::size_t> (n) + 1);
			EXPECT_TRUE (SameBits (b.front (), lo));
			EXPECT_TRUE (SameBits (b.back (), hi));
			for (int k = 1; k < n; ++k)
				EXPECT_TRUE (SameBits (b[static_cast<std::size_t> (k)], StrictPart (lo, hi, n, k))) << k;
			if (n == 2)
				EXPECT_TRUE (SameBits (b[1], (lo + hi) / 2.0));
			const std::vector<double> m = EqualParts (-hi, -lo, n, "peristyle");
			ASSERT_EQ (m.size (), b.size ());
			// Au bit pres, sauf le signe d'une borne interieure nulle (+0 des
			// deux cotes) ; les bornes extremes sont des negations exactes.
			for (std::size_t k = 0; k < b.size (); ++k)
			{
				const double mirrored = -b[b.size () - 1 - k];
				const bool interior = k > 0 && k + 1 < b.size ();
				if (interior && mirrored == 0.0)
				{
					EXPECT_EQ (m[k], 0.0) << k;
					EXPECT_FALSE (std::signbit (m[k])) << k;
				}
				else
					EXPECT_TRUE (SameBits (m[k], mirrored)) << k;
			}
		}
	// Le cas du zero signe est atteint par le jeu : sans quoi la clause
	// ci-dessus ne serait pas exercee.
	EXPECT_TRUE (std::signbit (-EqualParts (-0.2, 0.1, 3, "peristyle")[2]));
	EXPECT_FALSE (std::signbit (EqualParts (-0.1, 0.2, 3, "peristyle")[1]));
	// Le jeu distingue l'evaluation stricte de chacune des deux contractions en
	// FMA (premier produit fusionne, second produit fusionne) : sans quoi
	// l'egalite a StrictPart et la symetrie ne prouveraient rien sur une cible a
	// FMA.
	ASSERT_FALSE (SameBits (StrictPart (-1.3, 2.9, 4, 1), std::fma (3.0, -1.3, 1.0 * 2.9) / 4.0)) << "jeu non discriminant";
	ASSERT_FALSE (SameBits (StrictPart (0.1, 0.7, 6, 3), std::fma (3.0, 0.7, 3.0 * 0.1) / 6.0)) << "jeu non discriminant";

	const double nan = std::numeric_limits<double>::quiet_NaN ();
	ExpectRefusedNaming ([] { EqualParts (0.0, 1.0, 0, "travee"); }, "travee");
	ExpectRefusedNaming ([] { EqualParts (0.0, 1.0, -2, "travee"); }, "travee");
	ExpectRefusedNaming ([] { EqualParts (0.3, 0.3, 2, "travee"); }, "travee");
	ExpectRefusedNaming ([] { EqualParts (0.4, 0.3, 2, "travee"); }, "travee");
	ExpectRefusedNaming ([&] { EqualParts (nan, 0.3, 2, "travee"); }, "travee");
	ExpectRefusedNaming ([&] { EqualParts (0.0, std::numeric_limits<double>::infinity (), 2, "travee"); }, "travee");
}

// Q3
TEST (TEST_cgarchi_classical_partition, gaps_between_centred_parts_refuse_an_empty_gap)
{
	const std::vector<double> centres = {-1.9, -0.95, 0.1, 1.0 / 3.0 + 0.9};
	const double w = 0.37;
	const std::vector<Interval> g = GapsBetweenCentred (centres, w, "triglyph.width");
	ASSERT_EQ (g.size (), 3u);
	for (std::size_t k = 0; k < g.size (); ++k)
	{
		EXPECT_TRUE (SameBits (g[k].lo, centres[k] + 0.5 * w)) << k;
		EXPECT_TRUE (SameBits (g[k].hi, centres[k + 1] - 0.5 * w)) << k;
	}
	EXPECT_TRUE (GapsBetweenCentred ({0.4}, w, "triglyph.width").empty ());

	// Largeur egale a un ecart de centres : intervalle de longueur nulle, refuse ;
	// de meme a la valeur double immediatement superieure. Juste en dessous,
	// l'arrondi des deux bornes peut encore rendre l'intervalle vide (0,25 +
	// (0,25 - 2^-55) vaut 0,5) : le refus porte sur l'intervalle calcule.
	const std::vector<double> pair = {0.25, 0.75};
	ExpectRefusedNaming ([&] { GapsBetweenCentred (pair, 0.5, "triglyph.width"); }, "triglyph.width[0]");
	ExpectRefusedNaming ([&] { GapsBetweenCentred (pair, std::nextafter (0.5, 1.0), "triglyph.width"); },
	                     "triglyph.width[0]");
	ExpectRefusedNaming ([&] { GapsBetweenCentred (pair, std::nextafter (0.5, 0.0), "triglyph.width"); },
	                     "triglyph.width[0]");
	const std::vector<Interval> narrow = GapsBetweenCentred (pair, 0.5 - 1e-12, "triglyph.width");
	ASSERT_EQ (narrow.size (), 1u);
	EXPECT_LT (narrow[0].lo, narrow[0].hi);

	const double nan = std::numeric_limits<double>::quiet_NaN ();
	ExpectRefusedNaming ([&] { GapsBetweenCentred ({0.0, 2.0, 2.3}, 0.5, "triglyph.width"); }, "triglyph.width[1]");
	ExpectRefusedNaming ([&] { GapsBetweenCentred (pair, 0.0, "triglyph.width"); }, "triglyph.width");
	ExpectRefusedNaming ([&] { GapsBetweenCentred (pair, nan, "triglyph.width"); }, "triglyph.width");
	ExpectRefusedNaming ([&] { GapsBetweenCentred ({0.0, nan}, 0.1, "triglyph.width"); }, "triglyph.width[1]");
	ExpectRefusedNaming ([&] { GapsBetweenCentred ({1.0, 0.0}, 0.1, "triglyph.width"); }, "triglyph.width[1]");
}

// Q4 : controle de coherence, pas une preuve de justesse. Sans contraction, le
// milieu de chaque travee de LayoutPeristyle est a (i - (n - 1) / 2 + 1 / 2) s.
TEST (TEST_cgarchi_classical_partition, bay_midpoints_agree_with_the_peristyle_grid)
{
	for (const auto &[front, flank] : std::vector<std::pair<int, int>> {{4, 5}, {6, 13}, {2, 2}, {7, 12}})
	{
		const double sx = 1.9, sy = 1.7;
		const PeristyleLayout p = LayoutPeristyle (front, flank, sx, sy);
		std::vector<double> xs, ys;
		for (const PeristyleColumn &c : p.columns)
		{
			if (c.side == PeristyleSide::Front)
				xs.push_back (c.x);
			if (c.side == PeristyleSide::Right)
				ys.push_back (c.y);
		}
		xs.push_back (p.halfWidth);
		ys.push_back (p.halfDepth);
		ASSERT_EQ (xs.size (), static_cast<std::size_t> (front));
		ASSERT_EQ (ys.size (), static_cast<std::size_t> (flank));
		for (int i = 0; i + 1 < front; ++i)
		{
			const double mid = EqualParts (xs[static_cast<std::size_t> (i)], xs[static_cast<std::size_t> (i) + 1], 2, "x")[1];
			EXPECT_NEAR (mid, (i - (front - 1) / 2.0 + 0.5) * sx, 1e-12) << front << " " << i;
		}
		for (int i = 0; i + 1 < flank; ++i)
		{
			const double mid = EqualParts (ys[static_cast<std::size_t> (i)], ys[static_cast<std::size_t> (i) + 1], 2, "y")[1];
			EXPECT_NEAR (mid, (i - (flank - 1) / 2.0 + 0.5) * sy, 1e-12) << flank << " " << i;
		}
	}
}
