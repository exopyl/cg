#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_peristyle.h>

// ===========================================================================
//  Peristyle : implantation des axes, sans maillage
// ===========================================================================
//
// Entraxes d'essai synthetiques, distincts et non exacts en binaire (1,1 et
// 0,7) : une confusion des deux entraxes ou une erreur d'un facteur 2 ne
// retombe pas sur la bonne valeur. Le cas a entraxes egaux (kEqual) montre ce
// qu'un essai a entraxes egaux ne distingue pas.

using namespace cgarchi::classical;

namespace {

constexpr double kFrontSpacing = 1.1;
constexpr double kFlankSpacing = 0.7;
constexpr double kEqual = 1.1;
constexpr double kRelative = 1e-12;
constexpr int kMinSide = 2;
constexpr int kMaxSide = 20;

bool Near (double a, double b, double scale)
{
	return std::fabs (a - b) <= kRelative * scale;
}

double Scale (const PeristyleLayout &l)
{
	return l.halfWidth + l.halfDepth + l.frontSpacing + l.flankSpacing;
}

// Coordonnees, triees, des colonnes dont l'autre coordonnee vaut `at`.
std::vector<double> Row (const PeristyleLayout &l, bool alongX, double at)
{
	std::vector<double> row;
	for (const PeristyleColumn &c : l.columns)
		if (Near (alongX ? c.y : c.x, at, Scale (l)))
			row.push_back (alongX ? c.x : c.y);
	std::sort (row.begin (), row.end ());
	return row;
}

void ExpectRow (const PeristyleLayout &l, bool alongX, double at, int count, double spacing, const char *what)
{
	SCOPED_TRACE (what);
	const std::vector<double> row = Row (l, alongX, at);
	ASSERT_EQ (static_cast<int> (row.size ()), count);
	const double half = alongX ? l.halfWidth : l.halfDepth;
	EXPECT_TRUE (Near (row.front (), -half, Scale (l))) << row.front () << " " << -half;
	EXPECT_TRUE (Near (row.back (), half, Scale (l))) << row.back () << " " << half;
	for (std::size_t k = 1; k < row.size (); ++k)
		EXPECT_TRUE (Near (row[k] - row[k - 1], spacing, spacing)) << k << " : " << row[k] - row[k - 1];
}

void ExpectRefused (int front, int flank, double frontSpacing, double flankSpacing, const std::string &argument)
{
	try
	{
		LayoutPeristyle (front, flank, frontSpacing, flankSpacing);
		ADD_FAILURE () << "accepte : " << front << " " << flank << " " << frontSpacing << " " << flankSpacing;
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("'" + argument + "'"), std::string::npos) << e.what ();
	}
}

}  // namespace

// P1
TEST (TEST_cgarchi_classical_peristyle, the_column_count_is_twice_the_sides_minus_the_four_corners)
{
	for (int front = kMinSide; front <= kMaxSide; ++front)
		for (int flank = kMinSide; flank <= kMaxSide; ++flank)
		{
			const PeristyleLayout l = LayoutPeristyle (front, flank, kFrontSpacing, kFlankSpacing);
			EXPECT_EQ (static_cast<int> (l.columns.size ()), 2 * (front + flank) - 4) << front << " x " << flank;
		}
	EXPECT_EQ (LayoutPeristyle (6, 13, kFrontSpacing, kFlankSpacing).columns.size (), 34u);
	EXPECT_EQ (LayoutPeristyle (2, 2, kFrontSpacing, kFlankSpacing).columns.size (), 4u);
}

// P2
TEST (TEST_cgarchi_classical_peristyle, no_column_is_placed_twice)
{
	const double minimum = 0.5 * std::min (kFrontSpacing, kFlankSpacing);
	for (int front = kMinSide; front <= kMaxSide; ++front)
		for (int flank = kMinSide; flank <= kMaxSide; ++flank)
		{
			const PeristyleLayout l = LayoutPeristyle (front, flank, kFrontSpacing, kFlankSpacing);
			for (std::size_t a = 0; a < l.columns.size (); ++a)
				for (std::size_t b = a + 1; b < l.columns.size (); ++b)
				{
					const double d = std::hypot (l.columns[a].x - l.columns[b].x, l.columns[a].y - l.columns[b].y);
					EXPECT_GE (d, minimum) << front << " x " << flank << " : " << a << ", " << b;
				}
		}
}

// P3
TEST (TEST_cgarchi_classical_peristyle, exactly_four_columns_are_corners_at_the_ends_of_both_axes)
{
	for (int front = kMinSide; front <= kMaxSide; ++front)
		for (int flank = kMinSide; flank <= kMaxSide; ++flank)
		{
			SCOPED_TRACE (std::to_string (front) + " x " + std::to_string (flank));
			const PeristyleLayout l = LayoutPeristyle (front, flank, kFrontSpacing, kFlankSpacing);
			const double halfWidth = (front - 1) * kFrontSpacing / 2.0;
			const double halfDepth = (flank - 1) * kFlankSpacing / 2.0;
			const double scale = halfWidth + halfDepth + kFrontSpacing + kFlankSpacing;
			EXPECT_TRUE (Near (l.halfWidth, halfWidth, scale)) << l.halfWidth << " " << halfWidth;
			EXPECT_TRUE (Near (l.halfDepth, halfDepth, scale)) << l.halfDepth << " " << halfDepth;

			// Angle attendu de chaque cote : facade (-,-), droit (+,-), arriere (+,+), gauche (-,+).
			const double sx[4] = { -1.0, 1.0, 1.0, -1.0 };
			const double sy[4] = { -1.0, -1.0, 1.0, 1.0 };
			int corners = 0;
			int seen[4] = { 0, 0, 0, 0 };
			for (std::size_t k = 0; k < l.columns.size (); ++k)
			{
				const PeristyleColumn &c = l.columns[k];
				if (!c.corner)
					continue;
				++corners;
				const int s = static_cast<int> (c.side);
				++seen[s];
				EXPECT_TRUE (Near (c.x, sx[s] * halfWidth, scale)) << k << " : " << c.x;
				EXPECT_TRUE (Near (c.y, sy[s] * halfDepth, scale)) << k << " : " << c.y;
				EXPECT_TRUE (k == 0 || l.columns[k - 1].side != c.side) << k << " : l'angle n'ouvre pas son cote";
			}
			EXPECT_EQ (corners, 4);
			for (int s = 0; s < 4; ++s)
				EXPECT_EQ (seen[s], 1) << s;
		}
}

// P4
TEST (TEST_cgarchi_classical_peristyle, each_side_is_a_row_at_its_spacing)
{
	const std::pair<double, double> cases[] = { { kFrontSpacing, kFlankSpacing },
	                                            { kFlankSpacing, kFrontSpacing },
	                                            { kEqual, kEqual } };
	for (const auto &spacings : cases)
		for (int front = kMinSide; front <= kMaxSide; ++front)
			for (int flank = kMinSide; flank <= kMaxSide; ++flank)
			{
				SCOPED_TRACE ("entraxes " + std::to_string (spacings.first) + " / " + std::to_string (spacings.second)
				              + ", " + std::to_string (front) + " x " + std::to_string (flank));
				const PeristyleLayout l = LayoutPeristyle (front, flank, spacings.first, spacings.second);
				ExpectRow (l, true, -l.halfDepth, front, spacings.first, "facade");
				ExpectRow (l, true, l.halfDepth, front, spacings.first, "arriere");
				ExpectRow (l, false, l.halfWidth, flank, spacings.second, "flanc droit");
				ExpectRow (l, false, -l.halfWidth, flank, spacings.second, "flanc gauche");
			}
}

// P5 : controle de coherence entre deux copies de la meme expression
// (LayoutColonnade et LayoutPeristyle), pas une preuve de justesse.
TEST (TEST_cgarchi_classical_peristyle, the_front_row_is_the_colonnade_of_the_same_spacing)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		const OrderData &order = ClassicalOrders ().Get (static_cast<Order> (o));
		for (int front = kMinSide; front <= 12; ++front)
		{
			const ColonnadeLayout colonnade = LayoutColonnade (order, front);
			const PeristyleLayout l = LayoutPeristyle (front, 13, AxisSpacing (order), kFlankSpacing);
			// Garde : LayoutColonnade rend une rangee de `front` axes.
			ASSERT_EQ (static_cast<int> (colonnade.axes.size ()), front);
			std::vector<double> facade;
			for (const PeristyleColumn &c : l.columns)
				if (c.y == -l.halfDepth)
					facade.push_back (c.x);
			std::sort (facade.begin (), facade.end ());
			ASSERT_EQ (static_cast<int> (facade.size ()), front) << OrderKey (static_cast<Order> (o));
			EXPECT_EQ (std::memcmp (facade.data (), colonnade.axes.data (), front * sizeof (double)), 0)
				<< OrderKey (static_cast<Order> (o)) << ", " << front << " colonnes";
		}
	}
}

// P6
TEST (TEST_cgarchi_classical_peristyle, the_layout_is_symmetric_about_both_axes)
{
	for (int front = kMinSide; front <= kMaxSide; ++front)
		for (int flank = kMinSide; flank <= kMaxSide; ++flank)
		{
			const PeristyleLayout l = LayoutPeristyle (front, flank, kFrontSpacing, kFlankSpacing);
			std::set<std::pair<double, double>> axes;
			for (const PeristyleColumn &c : l.columns)
				axes.insert ({ c.x, c.y });
			for (const PeristyleColumn &c : l.columns)
			{
				EXPECT_EQ (axes.count ({ -c.x, c.y }), 1u) << front << " x " << flank << " : (" << c.x << ", " << c.y << ")";
				EXPECT_EQ (axes.count ({ c.x, -c.y }), 1u) << front << " x " << flank << " : (" << c.x << ", " << c.y << ")";
			}
		}
}

// P7
TEST (TEST_cgarchi_classical_peristyle, the_order_runs_counterclockwise_from_the_front_left_corner)
{
	for (int front = kMinSide; front <= kMaxSide; ++front)
		for (int flank = kMinSide; flank <= kMaxSide; ++flank)
		{
			SCOPED_TRACE (std::to_string (front) + " x " + std::to_string (flank));
			const PeristyleLayout l = LayoutPeristyle (front, flank, kFrontSpacing, kFlankSpacing);
			// Garde d'acces a front (), sans valeur de couverture : P1 fixe le compte.
			ASSERT_FALSE (l.columns.empty ());
			EXPECT_TRUE (Near (l.columns.front ().x, -l.halfWidth, Scale (l))) << l.columns.front ().x;
			EXPECT_TRUE (Near (l.columns.front ().y, -l.halfDepth, Scale (l))) << l.columns.front ().y;

			double twiceArea = 0.0;
			for (std::size_t k = 0; k < l.columns.size (); ++k)
			{
				const PeristyleColumn &a = l.columns[k];
				const PeristyleColumn &b = l.columns[(k + 1) % l.columns.size ()];
				twiceArea += a.x * b.y - b.x * a.y;
			}
			const double expected = 4.0 * l.halfWidth * l.halfDepth;
			EXPECT_TRUE (Near (0.5 * twiceArea, expected, expected)) << 0.5 * twiceArea << " " << expected;

			for (std::size_t k = 1; k < l.columns.size (); ++k)
				EXPECT_LE (static_cast<int> (l.columns[k - 1].side), static_cast<int> (l.columns[k].side)) << k;
		}
}

// P8
TEST (TEST_cgarchi_classical_peristyle, invalid_arguments_are_refused)
{
	const double inf = std::numeric_limits<double>::infinity ();
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	for (const int count : { 1, 0, -1, kPeristyleMaxColumnsPerSide + 1 })
	{
		ExpectRefused (count, 6, kFrontSpacing, kFlankSpacing, "front");
		ExpectRefused (6, count, kFrontSpacing, kFlankSpacing, "flank");
	}
	for (const double spacing : { 0.0, -1.0, nan, inf, -inf })
	{
		ExpectRefused (6, 13, spacing, kFlankSpacing, "frontSpacing");
		ExpectRefused (6, 13, kFrontSpacing, spacing, "flankSpacing");
	}

	// Entraxes finis dont la rangee n'est pas representable : demi-largeur
	// infinie, ou axes voisins confondus par un entraxe sous-normal.
	const double denormal = std::numeric_limits<double>::denorm_min ();
	ExpectRefused (kPeristyleMaxColumnsPerSide, 2, 1e306, kFlankSpacing, "frontSpacing");
	ExpectRefused (2, kPeristyleMaxColumnsPerSide, kFrontSpacing, 1e306, "flankSpacing");
	ExpectRefused (2, 2, denormal, kFlankSpacing, "frontSpacing");
	ExpectRefused (2, 2, kFrontSpacing, denormal, "flankSpacing");

	const PeristyleLayout l =
		LayoutPeristyle (kPeristyleMaxColumnsPerSide, kPeristyleMaxColumnsPerSide, kFrontSpacing, kFlankSpacing);
	EXPECT_EQ (l.columns.size (), 4092u);
}

// ---------------------------------------------------------------------------
//  Rangee contractee (registre TE6)
// ---------------------------------------------------------------------------
//
// Entraxes d'essai synthetiques : courants 1,9 et 1,7, d'angle 1,73 et 1,61,
// tous distincts, pour qu'une travee d'angle prise pour l'autre se voie.

namespace {

constexpr double kContractedFront = 1.9;
constexpr double kContractedFlank = 1.7;
constexpr double kFrontCorner = 1.73;
constexpr double kFlankCorner = 1.61;

PeristyleSettings Settings (int front, int flank, double frontSpacing, double flankSpacing, double frontCorner,
                            double flankCorner)
{
	PeristyleSettings s;
	s.front = front;
	s.flank = flank;
	s.frontSpacing = frontSpacing;
	s.flankSpacing = flankSpacing;
	s.frontCornerSpacing = frontCorner;
	s.flankCornerSpacing = flankCorner;
	return s;
}

void ExpectSettingsRefused (const PeristyleSettings &s, const std::string &field)
{
	try
	{
		LayoutPeristyle (s);
		ADD_FAILURE () << "accepte : " << s.front << " x " << s.flank << ", '" << field << "' attendu";
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find ("'" + field + "'"), std::string::npos) << e.what ();
	}
}

// Axes, tries, de la facade (alongX) ou du flanc droit.
std::vector<double> SortedRow (const PeristyleLayout &l, bool alongX)
{
	std::vector<double> row;
	for (const PeristyleColumn &c : l.columns)
		if (alongX ? c.y == -l.halfDepth : c.x == l.halfWidth)
			row.push_back (alongX ? c.x : c.y);
	std::sort (row.begin (), row.end ());
	return row;
}

}  // namespace

// P9
TEST (TEST_cgarchi_classical_peristyle, a_contracted_row_moves_only_its_corner_axes)
{
	for (int front = 4; front <= kMaxSide; ++front)
		for (int flank = 4; flank <= kMaxSide; ++flank)
		{
			SCOPED_TRACE (std::to_string (front) + " x " + std::to_string (flank));
			const PeristyleLayout l =
				LayoutPeristyle (Settings (front, flank, kContractedFront, kContractedFlank, kFrontCorner, kFlankCorner));
			const PeristyleLayout t = LayoutPeristyle (front, flank, kContractedFront, kContractedFlank);
			const double cornerX = 0.5 * (front - 3) * kContractedFront + kFrontCorner;
			const double cornerY = 0.5 * (flank - 3) * kContractedFlank + kFlankCorner;
			const double scale = cornerX + cornerY;
			EXPECT_TRUE (Near (l.halfWidth, cornerX, scale)) << l.halfWidth << " " << cornerX;
			EXPECT_TRUE (Near (l.halfDepth, cornerY, scale)) << l.halfDepth << " " << cornerY;
			EXPECT_EQ (l.frontCornerSpacing, kFrontCorner);
			EXPECT_EQ (l.flankCornerSpacing, kFlankCorner);

			// Compte et ordre de T0 : meme cote, meme drapeau d'angle, colonne par colonne.
			ASSERT_EQ (l.columns.size (), t.columns.size ());
			for (std::size_t k = 0; k < l.columns.size (); ++k)
			{
				const PeristyleColumn &c = l.columns[k], &d = t.columns[k];
				EXPECT_EQ (c.side, d.side) << k;
				EXPECT_EQ (c.corner, d.corner) << k;
				// Axe interieur : celui de T0 au bit pres ; axe d'angle : +-halfWidth, +-halfDepth.
				if (std::fabs (d.x) == t.halfWidth)
					EXPECT_EQ (c.x, std::copysign (l.halfWidth, d.x)) << k;
				else
					EXPECT_EQ (std::memcmp (&c.x, &d.x, sizeof (double)), 0) << k << " : " << c.x << " " << d.x;
				if (std::fabs (d.y) == t.halfDepth)
					EXPECT_EQ (c.y, std::copysign (l.halfDepth, d.y)) << k;
				else
					EXPECT_EQ (std::memcmp (&c.y, &d.y, sizeof (double)), 0) << k << " : " << c.y << " " << d.y;
			}

			// Travees : d'angle aux deux bouts, courantes entre.
			const std::pair<std::vector<double>, std::pair<double, double>> rows[] = {
				{SortedRow (l, true), {kContractedFront, kFrontCorner}},
				{SortedRow (l, false), {kContractedFlank, kFlankCorner}}};
			for (const auto &[row, spacings] : rows)
			{
				ASSERT_GE (row.size (), 4u);
				const std::size_t n = row.size ();
				EXPECT_TRUE (Near (row[1] - row[0], spacings.second, spacings.second)) << row[1] - row[0];
				EXPECT_TRUE (Near (row[n - 1] - row[n - 2], spacings.second, spacings.second)) << row[n - 1] - row[n - 2];
				for (std::size_t k = 2; k + 1 < n; ++k)
					EXPECT_TRUE (Near (row[k] - row[k - 1], spacings.first, spacings.first)) << k;
			}

			// Symetrie exacte.
			std::set<std::pair<double, double>> axes;
			for (const PeristyleColumn &c : l.columns)
				axes.insert ({c.x, c.y});
			for (const PeristyleColumn &c : l.columns)
			{
				EXPECT_EQ (axes.count ({-c.x, c.y}), 1u) << "(" << c.x << ", " << c.y << ")";
				EXPECT_EQ (axes.count ({c.x, -c.y}), 1u) << "(" << c.x << ", " << c.y << ")";
			}
		}
}

// P10 : controle de coherence entre deux chemins, pas une preuve de justesse
// (la justesse de la rangee contractee est portee par P9).
TEST (TEST_cgarchi_classical_peristyle, without_contraction_the_settings_give_the_four_argument_layout)
{
	const std::pair<double, double> cases[] = {
		{kContractedFront, kContractedFlank}, {kFrontSpacing, kFlankSpacing}, {0.1, 0.3}, {1.0 / 3.0, 0.7}};
	for (const auto &[frontSpacing, flankSpacing] : cases)
		for (int front = kMinSide; front <= kMaxSide; ++front)
			for (int flank = kMinSide; flank <= kMaxSide; ++flank)
			{
				SCOPED_TRACE (std::to_string (frontSpacing) + " / " + std::to_string (flankSpacing) + ", " +
				              std::to_string (front) + " x " + std::to_string (flank));
				const PeristyleLayout l =
					LayoutPeristyle (Settings (front, flank, frontSpacing, flankSpacing, frontSpacing, flankSpacing));
				const PeristyleLayout t = LayoutPeristyle (front, flank, frontSpacing, flankSpacing);
				EXPECT_EQ (std::memcmp (&l.halfWidth, &t.halfWidth, sizeof (double)), 0);
				EXPECT_EQ (std::memcmp (&l.halfDepth, &t.halfDepth, sizeof (double)), 0);
				EXPECT_EQ (l.frontCornerSpacing, t.frontCornerSpacing);
				EXPECT_EQ (l.flankCornerSpacing, t.flankCornerSpacing);
				ASSERT_EQ (l.columns.size (), t.columns.size ());
				for (std::size_t k = 0; k < l.columns.size (); ++k)
				{
					EXPECT_EQ (std::memcmp (&l.columns[k].x, &t.columns[k].x, sizeof (double)), 0) << k;
					EXPECT_EQ (std::memcmp (&l.columns[k].y, &t.columns[k].y, sizeof (double)), 0) << k;
					EXPECT_EQ (l.columns[k].side, t.columns[k].side) << k;
					EXPECT_EQ (l.columns[k].corner, t.columns[k].corner) << k;
				}
			}
}

// P11
TEST (TEST_cgarchi_classical_peristyle, a_contraction_needs_four_columns_per_row_and_a_valid_spacing)
{
	for (const int count : {2, 3})
	{
		ExpectSettingsRefused (Settings (count, 5, kContractedFront, kContractedFlank, kFrontCorner, kFlankCorner),
		                       "frontCornerSpacing");
		ExpectSettingsRefused (Settings (4, count, kContractedFront, kContractedFlank, kFrontCorner, kFlankCorner),
		                       "flankCornerSpacing");
	}
	const double inf = std::numeric_limits<double>::infinity ();
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	for (const double corner : {0.0, -1.0, nan, inf, -inf})
	{
		ExpectSettingsRefused (Settings (4, 5, kContractedFront, kContractedFlank, corner, kFlankCorner),
		                       "frontCornerSpacing");
		ExpectSettingsRefused (Settings (4, 5, kContractedFront, kContractedFlank, kFrontCorner, corner),
		                       "flankCornerSpacing");
	}
	// Axe d'angle confondu avec son voisin, ou infini.
	const double denormal = std::numeric_limits<double>::denorm_min ();
	ExpectSettingsRefused (Settings (4, 5, kContractedFront, kContractedFlank, denormal, kFlankCorner), "frontCornerSpacing");
	ExpectSettingsRefused (Settings (4, 5, kContractedFront, kContractedFlank, kFrontCorner, denormal), "flankCornerSpacing");
	ExpectSettingsRefused (Settings (4, 5, 1e308, kContractedFlank, 1.5e308, kFlankCorner), "frontCornerSpacing");
	// Refus de la forme a quatre arguments, repris.
	ExpectSettingsRefused (Settings (1, 5, kContractedFront, kContractedFlank, kFrontCorner, kFlankCorner), "front");
	ExpectSettingsRefused (Settings (4, 5, nan, kContractedFlank, kFrontCorner, kFlankCorner), "frontSpacing");

	// Dilatation : une travee d'angle plus longue que la travee courante est acceptee.
	const PeristyleLayout l = LayoutPeristyle (Settings (4, 5, kContractedFront, kContractedFlank, 2.3, 2.1));
	const std::vector<double> front = SortedRow (l, true), flank = SortedRow (l, false);
	ASSERT_EQ (front.size (), 4u);
	ASSERT_EQ (flank.size (), 5u);
	EXPECT_TRUE (Near (front[1] - front[0], 2.3, 2.3)) << front[1] - front[0];
	EXPECT_TRUE (Near (flank[4] - flank[3], 2.1, 2.1)) << flank[4] - flank[3];
}
