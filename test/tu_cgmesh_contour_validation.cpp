#include <gtest/gtest.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "../src/cgmesh/contour_validation.h"
#include "../src/cgmesh/polygon2.h"

// ===========================================================================
//  Validateur de contours 2D -- chaque controle prouve capable d'echouer
// ===========================================================================
//
// Chaque categorie a un cas negatif (compte exact quand il est defini) et un
// temoin positif (0). La phase large (grille) est confrontee a une reference
// quadratique en arithmetique entiere exacte sur des jeux aleatoires
// reproductibles.

namespace
{
    constexpr double kTol = 1e-2;

    using Pts = std::vector<Vector2f>;

    Pts rect(float x0, float y0, float x1, float y1)
    {
        return {Vector2f(x0, y0), Vector2f(x1, y0), Vector2f(x1, y1), Vector2f(x0, y1)};   // CCW
    }

    Pts reversed(Pts p)
    {
        std::reverse(p.begin(), p.end());
        return p;
    }

    ExtrudeContour outer(Pts p) { return ExtrudeContour{std::move(p), false}; }
    ExtrudeContour hole(Pts p) { return ExtrudeContour{std::move(p), true}; }

    ContourValidationReport validate(const std::vector<ExtrudeContour> &c, double tol = kTol)
    {
        ContourValidationOptions o;
        o.tolerance = tol;
        return ValidateContours(c, o);
    }

    std::size_t total(const ContourValidationReport &r)
    {
        std::size_t n = 0;
        for (std::size_t c : r.counts)
            n += c;
        return n;
    }

    const ContourDefectRecord *firstRecord(const ContourValidationReport &r, ContourDefect kind)
    {
        for (const auto &rec : r.records)
            if (rec.kind == kind)
                return &rec;
        return nullptr;
    }

    Polygon2 toPolygon(const std::vector<ExtrudeContour> &cs)
    {
        Polygon2 poly;
        poly.alloc_contours(static_cast<int>(cs.size()));
        for (std::size_t i = 0; i < cs.size(); ++i)
        {
            std::vector<float> xy;
            for (const Vector2f &p : cs[i].pts)
            {
                xy.push_back(p.x);
                xy.push_back(p.y);
            }
            poly.add_contour(static_cast<unsigned>(i), static_cast<unsigned>(cs[i].pts.size()), xy.data());
        }
        return poly;
    }
}

// --- NonFinitePoint ----------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, non_finite_point)
{
    Pts p = rect(0, 0, 1, 1);
    p[2].y = std::numeric_limits<float>::quiet_NaN();
    const auto r = validate({outer(p)});
    EXPECT_EQ(r.Count(ContourDefect::NonFinitePoint), 1u);
    EXPECT_EQ(total(r), 1u);   // contour exclu de toute autre mesure
    EXPECT_EQ(r.nestingLevel[0], -1);
    const auto *rec = firstRecord(r, ContourDefect::NonFinitePoint);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->contours, (std::vector<unsigned>{0}));
    EXPECT_EQ(rec->points, (std::vector<unsigned>{2}));

    EXPECT_TRUE(validate({outer(rect(0, 0, 1, 1))}).IsValid());
}

// --- TooFewPoints ------------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, too_few_points)
{
    const auto r = validate({outer({Vector2f(0, 0), Vector2f(1, 0)})});
    EXPECT_EQ(r.Count(ContourDefect::TooFewPoints), 1u);
    EXPECT_EQ(total(r), 1u);

    EXPECT_TRUE(validate({outer({Vector2f(0, 0), Vector2f(1, 0), Vector2f(0, 1)})}).IsValid());
}

// --- ShortEdge ---------------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, duplicated_point_is_one_short_edge_without_cascade)
{
    Pts p = rect(0, 0, 1, 1);
    p.insert(p.begin() + 2, p[1]);   // (1,0) repete
    const auto r = validate({outer(p)});
    EXPECT_EQ(r.Count(ContourDefect::ShortEdge), 1u);
    EXPECT_EQ(total(r), 1u);   // ni auto-intersection, ni autre defaut
    const auto *rec = firstRecord(r, ContourDefect::ShortEdge);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->edges, (std::vector<unsigned>{1}));
}

TEST(TEST_cgmesh_contour_validation, explicit_closing_point_is_a_zero_length_edge)
{
    Pts p = rect(0, 0, 1, 1);
    p.push_back(p.front());
    const auto r = validate({outer(p)});
    EXPECT_EQ(r.Count(ContourDefect::ShortEdge), 1u);
    EXPECT_EQ(total(r), 1u);
    const auto *rec = firstRecord(r, ContourDefect::ShortEdge);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->edges, (std::vector<unsigned>{4}));
}

TEST(TEST_cgmesh_contour_validation, edge_shorter_than_tolerance)
{
    const float d = static_cast<float>(0.5 * kTol);
    const Pts p = {Vector2f(0, 0), Vector2f(1, 0), Vector2f(1, d), Vector2f(1, 1), Vector2f(0, 1)};
    const auto r = validate({outer(p)});
    EXPECT_EQ(r.Count(ContourDefect::ShortEdge), 1u);
    EXPECT_EQ(total(r), 1u);

    const float D = static_cast<float>(2.0 * kTol);
    const Pts q = {Vector2f(0, 0), Vector2f(1, 0), Vector2f(1, D), Vector2f(1, 1), Vector2f(0, 1)};
    EXPECT_TRUE(validate({outer(q)}).IsValid());
}

// --- SelfIntersection --------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, bow_tie_self_intersects_once)
{
    // Aretes 0 : (0,0)-(2,2) et 2 : (2,0)-(0,1) se croisent en (2/3, 2/3).
    const Pts p = {Vector2f(0, 0), Vector2f(2, 2), Vector2f(2, 0), Vector2f(0, 1)};
    const auto r = validate({outer(p)});
    EXPECT_EQ(r.Count(ContourDefect::SelfIntersection), 1u);
    const auto *rec = firstRecord(r, ContourDefect::SelfIntersection);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->contours, (std::vector<unsigned>{0, 0}));
    EXPECT_EQ(rec->edges, (std::vector<unsigned>{0, 2}));

    const Pts q = {Vector2f(0, 0), Vector2f(2, 0), Vector2f(2, 2), Vector2f(0, 1)};
    EXPECT_TRUE(validate({outer(q)}).IsValid());
}

TEST(TEST_cgmesh_contour_validation, vertex_touching_its_own_edge_self_intersects)
{
    // Le sommet (1, 0) d'une encoche touche l'arete du bas : pincement.
    const Pts p = {Vector2f(0, 0), Vector2f(2, 0), Vector2f(2, 2), Vector2f(1, 2),
                   Vector2f(1, 0), Vector2f(0.5f, 2), Vector2f(0, 2)};
    const auto r = validate({outer(p)});
    EXPECT_GE(r.Count(ContourDefect::SelfIntersection), 1u);
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 0u);
}

TEST(TEST_cgmesh_contour_validation, spike_thinner_than_tolerance_is_a_fold)
{
    // Pointe (1,.5) -> (3,.5) -> (1,.5+h). Pour h = t/2, quatre paires a moins
    // de t : le repli (aretes 2,3 voisines) et les paires non voisines
    // (1,3), (2,4), (1,4) que la pointe resserre.
    auto spike = [](float h) {
        return Pts{Vector2f(0, 0), Vector2f(1, 0), Vector2f(1, 0.5f), Vector2f(3, 0.5f),
                   Vector2f(1, 0.5f + h), Vector2f(1, 1), Vector2f(0, 1)};
    };
    const auto r = validate({outer(spike(static_cast<float>(0.5 * kTol)))});
    EXPECT_EQ(r.Count(ContourDefect::SelfIntersection), 4u);

    EXPECT_TRUE(validate({outer(spike(static_cast<float>(2.0 * kTol)))}).IsValid());
}

// --- ContourContact ----------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, squares_sharing_an_edge_are_in_contact)
{
    // Paires a distance nulle : droite(A) x {gauche, haut, bas}(B), haut(A) x
    // {gauche, haut}(B), bas(A) x {gauche, bas}(B) = 7.
    const auto r = validate({outer(rect(0, 0, 1, 1)), outer(rect(1, 0, 2, 1))});
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 7u);
    EXPECT_EQ(r.contactContourPairs, 1u);
    EXPECT_EQ(total(r), 7u);
    const auto *rec = firstRecord(r, ContourDefect::ContourContact);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->contours, (std::vector<unsigned>{0, 1}));
}

TEST(TEST_cgmesh_contour_validation, squares_touching_at_one_vertex_are_in_contact)
{
    // Les deux aretes de A et les deux de B incidentes a (1,1) : 4 paires.
    const auto r = validate({outer(rect(0, 0, 1, 1)), outer(rect(1, 1, 2, 2))});
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 4u);
    EXPECT_EQ(r.contactContourPairs, 1u);
}

TEST(TEST_cgmesh_contour_validation, vertex_on_another_contour_edge_is_a_contact)
{
    // Pointe du triangle B posee en (1, .5) sur l'arete droite de A : ses deux
    // aretes incidentes touchent cette arete.
    const Pts tri = {Vector2f(1, 0.5f), Vector2f(2, 0), Vector2f(2, 1)};
    const auto r = validate({outer(rect(0, 0, 1, 1)), outer(tri)});
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 2u);
    EXPECT_EQ(total(r), 2u);
}

TEST(TEST_cgmesh_contour_validation, contact_is_measured_against_the_tolerance)
{
    const float nearGap = static_cast<float>(0.5 * kTol);
    const auto r = validate({outer(rect(0, 0, 1, 1)), outer(rect(1 + nearGap, 0, 2, 1))});
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 7u);   // memes paires que l'arete partagee

    const float farGap = static_cast<float>(2.0 * kTol);
    EXPECT_TRUE(validate({outer(rect(0, 0, 1, 1)), outer(rect(1 + farGap, 0, 2, 1))}).IsValid());
}

TEST(TEST_cgmesh_contour_validation, crossing_contours_are_in_contact)
{
    const auto r = validate({outer(rect(0, 0, 2, 2)), outer(rect(1, 1, 3, 3))});
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 2u);   // deux croisements
}

// --- WrongOrientation --------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, outer_and_hole_orientation)
{
    const auto good = validate({outer(rect(0, 0, 10, 10)), hole(reversed(rect(2, 2, 8, 8)))});
    EXPECT_TRUE(good.IsValid());
    EXPECT_EQ(good.nestingLevel, (std::vector<int>{0, 1}));

    const auto cwOuter = validate({outer(reversed(rect(0, 0, 10, 10)))});
    EXPECT_EQ(cwOuter.Count(ContourDefect::WrongOrientation), 1u);
    EXPECT_EQ(total(cwOuter), 1u);

    const auto ccwHole = validate({outer(rect(0, 0, 10, 10)), hole(rect(2, 2, 8, 8))});
    EXPECT_EQ(ccwHole.Count(ContourDefect::WrongOrientation), 1u);
    EXPECT_EQ(total(ccwHole), 1u);
    const auto *rec = firstRecord(ccwHole, ContourDefect::WrongOrientation);
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(rec->contours, (std::vector<unsigned>{1}));
}

TEST(TEST_cgmesh_contour_validation, orientation_convention_is_selectable)
{
    const std::vector<ExtrudeContour> cs = {outer(reversed(rect(0, 0, 10, 10))), hole(rect(2, 2, 8, 8))};
    ContourValidationOptions o;
    o.tolerance = kTol;
    EXPECT_EQ(ValidateContours(cs, o).Count(ContourDefect::WrongOrientation), 2u);
    o.orientation = ContourOrientation::OuterCwHoleCcw;
    EXPECT_TRUE(ValidateContours(cs, o).IsValid());
    o.orientation = ContourOrientation::Unchecked;
    EXPECT_TRUE(ValidateContours({outer(reversed(rect(0, 0, 1, 1)))}, o).IsValid());
}

// Sans imbrication : niveaux -1, ni WrongOrientation ni BadNesting ; les
// defauts d'aretes restent comptes.
TEST(TEST_cgmesh_contour_validation, nesting_can_be_skipped)
{
    const std::vector<ExtrudeContour> cs = {outer(reversed(rect(0, 0, 10, 10))),   // CW au niveau 0
                                            hole(rect(20, 20, 21, 21)),            // trou hors de toute enveloppe
                                            outer(rect(10, 0, 15, 5))};            // touche le premier en x = 10
    ContourValidationOptions o;
    o.tolerance = kTol;
    const auto full = ValidateContours(cs, o);
    EXPECT_GT(full.Count(ContourDefect::WrongOrientation), 0u);
    EXPECT_GT(full.Count(ContourDefect::BadNesting), 0u);
    EXPECT_GT(full.Count(ContourDefect::ContourContact), 0u);

    o.checkNesting = false;
    const auto edgesOnly = ValidateContours(cs, o);
    EXPECT_EQ(edgesOnly.Count(ContourDefect::WrongOrientation), 0u);
    EXPECT_EQ(edgesOnly.Count(ContourDefect::BadNesting), 0u);
    EXPECT_EQ(edgesOnly.Count(ContourDefect::ContourContact), full.Count(ContourDefect::ContourContact));
    EXPECT_EQ(edgesOnly.nestingLevel, (std::vector<int>{-1, -1, -1}));
}

// --- BadNesting --------------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, hole_outside_every_outer)
{
    const std::vector<ExtrudeContour> cs = {outer(rect(0, 0, 1, 1)), hole(reversed(rect(3, 3, 4, 4)))};
    const auto r = validate(cs);
    EXPECT_EQ(r.Count(ContourDefect::BadNesting), 1u);         // isHole au niveau 0
    EXPECT_EQ(r.Count(ContourDefect::WrongOrientation), 1u);   // CW au niveau 0
    EXPECT_EQ(total(r), 2u);

    // Sans drapeau (Polygon2), seule l'orientation trahit le role.
    ContourValidationOptions o;
    o.tolerance = kTol;
    const auto p = ValidateContours(toPolygon(cs), o);
    EXPECT_EQ(p.Count(ContourDefect::BadNesting), 0u);
    EXPECT_EQ(p.Count(ContourDefect::WrongOrientation), 1u);
}

TEST(TEST_cgmesh_contour_validation, outer_inside_outer)
{
    const auto r = validate({outer(rect(0, 0, 10, 10)), outer(rect(2, 2, 8, 8))});
    EXPECT_EQ(r.Count(ContourDefect::BadNesting), 1u);
    EXPECT_EQ(r.Count(ContourDefect::WrongOrientation), 1u);
}

TEST(TEST_cgmesh_contour_validation, island_in_a_hole_is_valid_unless_depth_is_bounded)
{
    const std::vector<ExtrudeContour> cs = {
        outer(rect(0, 0, 10, 10)),
        hole(reversed(rect(2, 2, 8, 8))),
        outer(rect(4, 4, 6, 6)),                        // ilot de matiere, niveau 2
        hole(reversed(rect(4.5f, 4.5f, 5.5f, 5.5f))),   // vide dans l'ilot, niveau 3
    };
    const auto r = validate(cs);
    EXPECT_TRUE(r.IsValid());
    EXPECT_EQ(r.nestingLevel, (std::vector<int>{0, 1, 2, 3}));

    ContourValidationOptions o;
    o.tolerance = kTol;
    o.maxNestingLevel = 1u;   // enveloppes + trous seulement (contourRegions)
    const auto bounded = ValidateContours(cs, o);
    EXPECT_EQ(bounded.Count(ContourDefect::BadNesting), 2u);
    EXPECT_EQ(total(bounded), 2u);
}

TEST(TEST_cgmesh_contour_validation, polygon2_input_matches_extrude_input)
{
    const std::vector<ExtrudeContour> cs = {
        outer(rect(0, 0, 10, 10)), hole(reversed(rect(2, 2, 8, 8))), outer(rect(4, 4, 6, 6)),
        outer(rect(10, 0, 11, 1)),   // contact avec l'enveloppe
    };
    const auto a = validate(cs);
    ContourValidationOptions o;
    o.tolerance = kTol;
    const auto b = ValidateContours(toPolygon(cs), o);
    EXPECT_EQ(a.counts, b.counts);
    EXPECT_EQ(a.nestingLevel, b.nestingLevel);
    EXPECT_GT(a.Count(ContourDefect::ContourContact), 0u);
}

// --- NegligibleArea ----------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, zero_area_contour)
{
    // Trois points alignes : aire nulle, et deux replis (aretes voisines 0-2 et
    // 1-2, dont l'extremite libre est sur l'autre).
    const auto r = validate({outer({Vector2f(0, 0), Vector2f(1, 0), Vector2f(2, 0)})});
    EXPECT_EQ(r.Count(ContourDefect::NegligibleArea), 1u);
    EXPECT_EQ(r.Count(ContourDefect::WrongOrientation), 0u);   // sens non defini, non controle
    EXPECT_EQ(r.Count(ContourDefect::SelfIntersection), 2u);

    EXPECT_TRUE(validate({outer(rect(0, 0, 1, 1))}).IsValid());
}

// --- API ---------------------------------------------------------------------

TEST(TEST_cgmesh_contour_validation, defect_names_are_stable_and_unique)
{
    const std::vector<std::string> expected = {
        "non_finite_point", "too_few_points", "short_edge", "self_intersection",
        "contour_contact", "wrong_orientation", "bad_nesting", "negligible_area"};
    ASSERT_EQ(expected.size(), kContourDefectCount);
    for (std::size_t k = 0; k < kContourDefectCount; ++k)
        EXPECT_EQ(ContourDefectName(static_cast<ContourDefect>(k)), expected[k]);
}

TEST(TEST_cgmesh_contour_validation, rejects_invalid_tolerances)
{
    const std::vector<ExtrudeContour> cs = {outer(rect(0, 0, 1, 1))};
    for (double t : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        ContourValidationOptions o;
        o.tolerance = t;
        EXPECT_THROW(ValidateContours(cs, o), std::invalid_argument) << t;
    }
}

TEST(TEST_cgmesh_contour_validation, default_tolerance_follows_the_extent)
{
    const std::vector<ExtrudeContour> cs = {outer(rect(0, 0, 100, 100))};
    const double expected = std::max(1e-6 * std::sqrt(2.0) * 100.0, 4.0 * FLT_EPSILON * 100.0);
    EXPECT_DOUBLE_EQ(DefaultContourTolerance(cs), expected);
    EXPECT_DOUBLE_EQ(ValidateContours(cs).tolerance, expected);
    EXPECT_DOUBLE_EQ(DefaultContourTolerance(toPolygon(cs)), expected);
}

TEST(TEST_cgmesh_contour_validation, records_are_capped_but_counts_are_not)
{
    Pts p;
    for (int i = 0; i < 10; ++i)
    {
        p.push_back(Vector2f(static_cast<float>(i), 0));
        p.push_back(Vector2f(static_cast<float>(i), 0));   // 10 doublons
    }
    p.push_back(Vector2f(5, 5));
    ContourValidationOptions o;
    o.tolerance = kTol;
    o.maxRecordsPerKind = 3;
    const auto r = ValidateContours({outer(p)}, o);
    EXPECT_EQ(r.Count(ContourDefect::ShortEdge), 10u);
    EXPECT_EQ(std::count_if(r.records.begin(), r.records.end(),
                            [](const ContourDefectRecord &x) { return x.kind == ContourDefect::ShortEdge; }),
              3);
}

// --- Phase large : grille contre reference quadratique exacte ----------------

namespace
{
    // SplitMix64 : graine fixe, suite reproductible sur toute plateforme (les
    // distributions de <random> ne le sont pas d'une bibliotheque a l'autre).
    struct SplitMix64
    {
        std::uint64_t state;
        std::uint64_t next()
        {
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
        long long range(long long lo, long long hi) { return lo + static_cast<long long>(next() % static_cast<std::uint64_t>(hi - lo + 1)); }
    };

    struct IP
    {
        long long x, y;
    };

    long long crossI(IP o, IP a, IP b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }
    long long sqI(long long v) { return v * v; }

    // Distance au carre en rationnel exact num / den (den > 0).
    struct Q
    {
        long long num, den;
    };
    bool qLess(Q a, Q b) { return a.num * b.den < b.num * a.den; }

    Q pointSegment(IP p, IP a, IP b)
    {
        const long long ux = b.x - a.x, uy = b.y - a.y;
        const long long len2 = ux * ux + uy * uy;
        const long long t = (p.x - a.x) * ux + (p.y - a.y) * uy;
        if (len2 == 0 || t <= 0)
            return {sqI(p.x - a.x) + sqI(p.y - a.y), 1};
        if (t >= len2)
            return {sqI(p.x - b.x) + sqI(p.y - b.y), 1};
        return {sqI(ux * (p.y - a.y) - uy * (p.x - a.x)), len2};
    }

    bool opposite(long long u, long long v) { return (u > 0 && v < 0) || (u < 0 && v > 0); }

    Q segmentSegment(IP a0, IP a1, IP b0, IP b1)
    {
        if (opposite(crossI(b0, b1, a0), crossI(b0, b1, a1)) && opposite(crossI(a0, a1, b0), crossI(a0, a1, b1)))
            return {0, 1};
        Q m = pointSegment(a0, b0, b1);
        for (Q q : {pointSegment(a1, b0, b1), pointSegment(b0, a0, a1), pointSegment(b1, a0, a1)})
            if (qLess(q, m))
                m = q;
        return m;
    }

    // Tolerance 3/2 : d^2 < t^2  <=>  4 num < 9 den. Proche manque : t <= d < 3t.
    bool underTol(Q q) { return 4 * q.num < 9 * q.den; }
    bool nearMiss(Q q) { return !underTol(q) && 4 * q.num < 81 * q.den; }

    // Polygone etoile autour d'un centre entier : decalages tries par angle
    // exact (demi-plan puis produit vectoriel), sans trigonometrie. Les points
    // trop proches du precedent sont ecartes : aucune arete n'est courte (< t),
    // la reference n'a donc pas a connaitre les grappes. `scramble` laisse
    // l'ordre aleatoire : contour auto-intersecte.
    std::vector<IP> randomContour(SplitMix64 &rng, long long extent, long long maxRadius, bool scramble)
    {
        for (;;)
        {
            const IP c {rng.range(0, extent), rng.range(0, extent)};
            const long long r = rng.range(3, maxRadius);
            const int m = static_cast<int>(rng.range(3, 24));
            std::vector<IP> d;
            for (int i = 0; i < m; ++i)
            {
                IP v {rng.range(-r, r), rng.range(-r, r)};
                if (v.x != 0 || v.y != 0)
                    d.push_back(v);
            }
            if (!scramble)
            {
                auto half = [](IP v) { return (v.y < 0 || (v.y == 0 && v.x < 0)) ? 1 : 0; };
                std::sort(d.begin(), d.end(), [&](IP a, IP b) {
                    if (half(a) != half(b))
                        return half(a) < half(b);
                    return a.x * b.y - a.y * b.x > 0;
                });
            }
            std::vector<IP> pts;
            for (IP v : d)
            {
                const IP p {c.x + v.x, c.y + v.y};
                if (pts.empty() || sqI(p.x - pts.back().x) + sqI(p.y - pts.back().y) >= 4)
                    pts.push_back(p);
            }
            while (pts.size() >= 2 && sqI(pts.back().x - pts.front().x) + sqI(pts.back().y - pts.front().y) < 4)
                pts.pop_back();
            if (pts.size() >= 3)
                return pts;
        }
    }

    using PairKey = std::tuple<unsigned, unsigned, unsigned, unsigned>;   // contour a, arete a, contour b, arete b

    struct FuzzTally
    {
        std::size_t contactZero = 0, contactNear = 0, contactNearMiss = 0;
        std::size_t selfCrossing = 0, selfFold = 0, selfNearMiss = 0;
        std::size_t missed = 0, spurious = 0;
        std::size_t edges = 0, pairsTested = 0;
    };

    void fuzzTrial(SplitMix64 &rng, FuzzTally &t, bool longEdges)
    {
        const long long extent = 400;
        const int nContours = longEdges ? static_cast<int>(rng.range(2, 6)) : static_cast<int>(rng.range(5, 30));
        const long long maxRadius = longEdges ? 200 : 40;
        std::vector<std::vector<IP>> cs;
        for (int i = 0; i < nContours; ++i)
            cs.push_back(randomContour(rng, extent, maxRadius, rng.range(0, 3) == 0));

        // Reference quadratique exacte.
        std::set<PairKey> expected;
        for (unsigned ca = 0; ca < cs.size(); ++ca)
            for (unsigned cb = ca; cb < cs.size(); ++cb)
            {
                const auto &A = cs[ca];
                const auto &B = cs[cb];
                const unsigned na = static_cast<unsigned>(A.size()), nb = static_cast<unsigned>(B.size());
                for (unsigned i = 0; i < na; ++i)
                    for (unsigned j = (ca == cb ? i + 1 : 0); j < nb; ++j)
                    {
                        const IP a0 = A[i], a1 = A[(i + 1) % na], b0 = B[j], b1 = B[(j + 1) % nb];
                        bool flagged = false;
                        if (ca != cb)
                        {
                            const Q q = segmentSegment(a0, a1, b0, b1);
                            flagged = underTol(q);
                            if (flagged && q.num == 0) ++t.contactZero;
                            else if (flagged) ++t.contactNear;
                            else if (nearMiss(q)) ++t.contactNearMiss;
                        }
                        else if (j == i + 1 || (i == 0 && j == na - 1))
                        {
                            // Voisines : sommet commun s, extremites libres p (de i) et q (de j).
                            const bool iThenJ = (j == i + 1);
                            const IP p = iThenJ ? a0 : a1;
                            const IP q = iThenJ ? b1 : b0;
                            const Q d1 = pointSegment(p, b0, b1), d2 = pointSegment(q, a0, a1);
                            flagged = underTol(d1) || underTol(d2);
                            if (flagged) ++t.selfFold;
                            else if (nearMiss(d1) || nearMiss(d2)) ++t.selfNearMiss;
                        }
                        else
                        {
                            const Q q = segmentSegment(a0, a1, b0, b1);
                            flagged = underTol(q);
                            if (flagged) ++t.selfCrossing;
                            else if (nearMiss(q)) ++t.selfNearMiss;
                        }
                        if (flagged)
                            expected.insert({ca, i, cb, j});
                    }
            }

        std::vector<ExtrudeContour> in;
        for (const auto &c : cs)
        {
            ExtrudeContour e;
            for (IP p : c)
                e.pts.push_back(Vector2f(static_cast<float>(p.x), static_cast<float>(p.y)));
            t.edges += c.size();
            in.push_back(std::move(e));
        }
        ContourValidationOptions o;
        o.tolerance = 1.5;
        o.orientation = ContourOrientation::Unchecked;
        o.maxRecordsPerKind = std::numeric_limits<std::size_t>::max();
        const auto r = ValidateContours(in, o);
        t.pairsTested += r.edgePairsTested;

        std::set<PairKey> reported;
        for (const auto &rec : r.records)
            if (rec.kind == ContourDefect::SelfIntersection || rec.kind == ContourDefect::ContourContact)
                reported.insert({rec.contours[0], rec.edges[0], rec.contours[1], rec.edges[1]});
        EXPECT_EQ(reported.size(),
                  r.Count(ContourDefect::SelfIntersection) + r.Count(ContourDefect::ContourContact));
        for (const auto &k : expected)
            t.missed += reported.count(k) ? 0 : 1;
        for (const auto &k : reported)
            t.spurious += expected.count(k) ? 0 : 1;
    }
}

TEST(TEST_cgmesh_contour_validation, broad_phase_matches_exact_quadratic_reference)
{
    SplitMix64 rng {0xC0A7u};
    FuzzTally t;
    for (int trial = 0; trial < 120; ++trial)
        fuzzTrial(rng, t, trial % 4 == 3);

    EXPECT_EQ(t.missed, 0u);
    EXPECT_EQ(t.spurious, 0u);
    // Non vacuite : chaque famille de cas, positive et negative, est presente.
    // Seuils a environ la moitie des comptes mesures (MSVC Release : 1976,
    // 1259, 3137, 13 132, 1288, 21 951) ; la suite est entiere et portable,
    // l'ecart ne peut venir que d'une modification du generateur.
    EXPECT_GE(t.contactZero, 1000u);
    EXPECT_GE(t.contactNear, 600u);
    EXPECT_GE(t.contactNearMiss, 1500u);
    EXPECT_GE(t.selfCrossing, 6000u);
    EXPECT_GE(t.selfFold, 600u);
    EXPECT_GE(t.selfNearMiss, 10000u);
    RecordProperty("contact_zero", static_cast<int>(t.contactZero));
    RecordProperty("contact_near", static_cast<int>(t.contactNear));
    RecordProperty("contact_near_miss", static_cast<int>(t.contactNearMiss));
    RecordProperty("self_crossing", static_cast<int>(t.selfCrossing));
    RecordProperty("self_fold", static_cast<int>(t.selfFold));
    RecordProperty("self_near_miss", static_cast<int>(t.selfNearMiss));
    RecordProperty("edges", static_cast<int>(t.edges));
    RecordProperty("pairs_tested", static_cast<int>(t.pairsTested));
}

TEST(TEST_cgmesh_contour_validation, broad_phase_is_linear_on_a_large_valid_set)
{
    // 120 x 120 octogones disjoints : 115 200 points. Une phase large
    // quadratique examinerait ~6,6e9 paires ; la grille doit rester a quelques
    // paires par arete.
    const Pts octagon = {Vector2f(3, 0), Vector2f(2, 2), Vector2f(0, 3), Vector2f(-2, 2),
                         Vector2f(-3, 0), Vector2f(-2, -2), Vector2f(0, -3), Vector2f(2, -2)};
    std::vector<ExtrudeContour> cs;
    for (int i = 0; i < 120; ++i)
        for (int j = 0; j < 120; ++j)
        {
            ExtrudeContour c;
            for (const Vector2f &p : octagon)
                c.pts.push_back(Vector2f(p.x + 10.f * i, p.y + 10.f * j));
            cs.push_back(std::move(c));
        }
    ContourValidationOptions o;
    o.tolerance = 0.5;
    const auto r = ValidateContours(cs, o);
    EXPECT_EQ(r.pointCount, 115200u);
    EXPECT_TRUE(r.IsValid());
    EXPECT_LE(r.edgePairsTested, 8u * r.pointCount);
    RecordProperty("pairs_tested", static_cast<int>(r.edgePairsTested));
}

TEST(TEST_cgmesh_contour_validation, broad_phase_stays_linear_when_detail_is_clustered)
{
    // Cadre de 1000 et 10 000 trous carres concentres dans un coin de 1 x 1 :
    // une grille reglee sur l'emprise y mettrait tous les carres dans une
    // poignee de cellules (8e8 paires mesurees). La grille creuse suit la
    // taille des aretes.
    std::vector<ExtrudeContour> cs = {outer(rect(0, 0, 1000, 1000))};
    const int side = 100;
    const double pitch = 1.0 / side;
    for (int i = 0; i < side * side; ++i)
    {
        const double cx = 1 + (i % side + 0.5) * pitch, cy = 1 + (i / side + 0.5) * pitch;
        const float h = static_cast<float>(0.3 * pitch);
        cs.push_back(hole(reversed(rect(static_cast<float>(cx) - h, static_cast<float>(cy) - h,
                                        static_cast<float>(cx) + h, static_cast<float>(cy) + h))));
    }
    ContourValidationOptions o;
    o.tolerance = 1e-5;
    const auto r = ValidateContours(cs, o);
    EXPECT_FALSE(r.truncated);
    EXPECT_TRUE(r.IsValid());
    EXPECT_LE(r.edgePairsTested, 8u * r.pointCount);
    EXPECT_LE(r.candidatesVisited, 200u * r.pointCount);
    RecordProperty("candidates", static_cast<int>(r.candidatesVisited));
}

TEST(TEST_cgmesh_contour_validation, exhausted_budget_truncates_and_invalidates)
{
    // Barres longues et serrees : le pire cas documente de la phase large.
    std::vector<ExtrudeContour> cs;
    for (int i = 0; i < 200; ++i)
        cs.push_back(outer(rect(0, static_cast<float>(i), 1000, static_cast<float>(i) + 0.5f)));
    ContourValidationOptions o;
    o.tolerance = kTol;
    const auto full = ValidateContours(cs, o);
    EXPECT_FALSE(full.truncated);
    EXPECT_TRUE(full.IsValid());
    EXPECT_EQ(full.candidateBudget, std::max(kMinContourCandidates, kContourCandidatesPerEdge * full.pointCount));

    o.maxCandidates = full.candidatesVisited / 2;
    const auto cut = ValidateContours(cs, o);
    EXPECT_TRUE(cut.truncated);
    EXPECT_FALSE(cut.IsValid());
    EXPECT_EQ(cut.candidateBudget, full.candidatesVisited / 2);
    EXPECT_LE(cut.candidatesVisited, cut.candidateBudget);
    EXPECT_EQ(total(cut), 0u);   // aucun defaut invente par la troncature
}

TEST(TEST_cgmesh_contour_validation, truncation_during_pairs_skips_nesting)
{
    std::vector<ExtrudeContour> cs = {outer(rect(0, 0, 10, 10)), hole(reversed(rect(2, 2, 8, 8)))};
    ContourValidationOptions o;
    o.tolerance = kTol;
    o.maxCandidates = 1;
    const auto r = ValidateContours(cs, o);
    EXPECT_TRUE(r.truncated);
    EXPECT_EQ(r.nestingLevel, (std::vector<int>{-1, -1}));
    EXPECT_EQ(r.Count(ContourDefect::WrongOrientation), 0u);
    EXPECT_EQ(r.Count(ContourDefect::BadNesting), 0u);
}

// --- Niveau d'imbrication : parite du rayon ----------------------------------

TEST(TEST_cgmesh_contour_validation, ray_through_a_vertex_counts_once)
{
    // Le point du trou, milieu de son arete (-1,-3)-(-1,3), est a y = 0 : le
    // rayon passe par le sommet (10, 0) du losange. Regle semi-ouverte : une
    // seule traversee.
    const std::vector<ExtrudeContour> cs = {
        outer({Vector2f(0, -10), Vector2f(10, 0), Vector2f(0, 10), Vector2f(-10, 0)}),
        hole({Vector2f(-1, -3), Vector2f(-1, 3), Vector2f(1, 3), Vector2f(1, -3)})};
    const auto r = ValidateContours(cs);
    EXPECT_EQ(r.nestingLevel, (std::vector<int>{0, 1}));
    EXPECT_TRUE(r.IsValid());
}

TEST(TEST_cgmesh_contour_validation, contour_crossed_twice_by_the_ray_does_not_count)
{
    // Le rayon du premier trou traverse deux fois le second : parite paire.
    const std::vector<ExtrudeContour> cs = {
        outer(rect(-20, -10, 20, 10)),
        hole({Vector2f(-10, -3), Vector2f(-10, 3), Vector2f(-8, 3), Vector2f(-8, -3)}),
        hole({Vector2f(5, -3), Vector2f(5, 3), Vector2f(7, 3), Vector2f(7, -3)})};
    const auto r = ValidateContours(cs);
    EXPECT_EQ(r.nestingLevel, (std::vector<int>{0, 1, 1}));
    EXPECT_TRUE(r.IsValid());
}

TEST(TEST_cgmesh_contour_validation, level_probe_avoids_edges_in_contact)
{
    // Le trou H2 touche le trou H le long de x = 30. La plus longue arete de H
    // (30,90)-(30,10) est en contact : depuis son milieu (30, 50), le rayon
    // traverserait le bord droit de H2 et donnerait a H le niveau 2. Le point
    // pris sur l'arete sans contact (10,10)-(10,90) donne le niveau exact.
    const std::vector<ExtrudeContour> cs = {
        outer(rect(0, 0, 100, 100)),
        hole(reversed(rect(10, 10, 30, 90))),
        hole(reversed(rect(30, 40, 40, 60)))};
    const auto r = validate(cs);
    EXPECT_GT(r.Count(ContourDefect::ContourContact), 0u);
    EXPECT_EQ(r.nestingLevel, (std::vector<int>{0, 1, 1}));
    EXPECT_EQ(r.Count(ContourDefect::WrongOrientation), 0u);
    EXPECT_EQ(r.Count(ContourDefect::BadNesting), 0u);
}

namespace
{
    // Tours imbriquees : formes concentriques a coordonnees entieres
    // (carre, losange, octogone, rectangle haut), niveaux connus par
    // construction. Le rectangle haut a sa plus longue arete verticale, de
    // milieu a y = cy : son rayon passe par le sommet droit d'un losange
    // parent, seul sur le rayon. Rangees alignees : les rayons longent des
    // aretes horizontales et passent par des sommets d'autres tours.
    std::vector<IP> towerShape(int kind, long long cx, long long cy, long long s)
    {
        if (kind == 0)
            return {{cx - s, cy - s}, {cx + s, cy - s}, {cx + s, cy + s}, {cx - s, cy + s}};
        if (kind == 1)
            return {{cx, cy - s}, {cx + s, cy}, {cx, cy + s}, {cx - s, cy}};
        if (kind == 3)
        {
            const long long w = s / 5, h = s / 2;
            return {{cx - w, cy - h}, {cx + w, cy - h}, {cx + w, cy + h}, {cx - w, cy + h}};
        }
        const long long b = s / 2;
        return {{cx - b, cy - s}, {cx + b, cy - s}, {cx + s, cy - b}, {cx + s, cy + b},
                {cx + b, cy + s}, {cx - b, cy + s}, {cx - s, cy + b}, {cx - s, cy - b}};
    }

    // Pair-impair exact, point hors de tout bord.
    bool insideExact(IP p, const std::vector<IP> &poly)
    {
        bool in = false;
        for (std::size_t i = 0, n = poly.size(); i < n; ++i)
        {
            const IP a = poly[i], b = poly[(i + 1) % n];
            if ((a.y > p.y) == (b.y > p.y))
                continue;
            // x d'intersection > p.x, sans division : signe selon le sens de l'arete.
            const long long lhs = (p.y - a.y) * (b.x - a.x) - (p.x - a.x) * (b.y - a.y);
            if ((b.y > a.y) ? lhs > 0 : lhs < 0)
                in = !in;
        }
        return in;
    }
}

TEST(TEST_cgmesh_contour_validation, nesting_levels_match_construction_on_random_towers)
{
    SplitMix64 rng {0x70E25u};
    std::size_t trials = 0, contours = 0, alignedTrials = 0, deep = 0, vertexRays = 0, mismatches = 0, refMismatches = 0;
    for (int trial = 0; trial < 1500; ++trial)
    {
        std::vector<std::vector<IP>> shapes;
        std::vector<int> expected;
        const bool frame = rng.range(0, 1) == 1;
        const int base = frame ? 1 : 0;
        if (frame)
        {
            shapes.push_back(towerShape(0, 0, 0, 3000));
            expected.push_back(0);
        }
        const int towers = static_cast<int>(rng.range(1, 12));
        const int rowMode = static_cast<int>(rng.range(0, 2));
        alignedTrials += rowMode == 0;
        for (int k = 0; k < towers; ++k)
        {
            const long long cx = -2400 + 400LL * k;
            const long long cy = rowMode == 0 ? 0 : (rowMode == 1 ? 100 * rng.range(0, 2) : rng.range(-2000, 1999));
            const int depth = static_cast<int>(rng.range(1, 4));
            int parent = 0;
            for (int r = 0; r < depth; ++r)
            {
                int kind = static_cast<int>(rng.range(0, 3));
                if (r > 0 && parent == 3) kind = 3;                // rectangle haut : enfant rectangle haut
                if (r > 0 && parent == 1 && kind != 3) kind = 1;   // losange : enfant losange ou rectangle haut
                if (r > 0 && parent == 2 && kind == 0) kind = 1;   // octogone : pas d'enfant carre
                vertexRays += (r > 0 && parent == 1 && kind == 3) ? 1 : 0;
                parent = kind;
                shapes.push_back(towerShape(kind, cx, cy, 10LL * (2 * (depth - r) + 1)));
                expected.push_back(base + r);
                deep += (base + r) >= 3;
            }
        }

        std::vector<ExtrudeContour> cs;
        for (std::size_t i = 0; i < shapes.size(); ++i)
        {
            // Reference independante : nombre de formes contenant le premier sommet.
            int level = 0;
            for (std::size_t j = 0; j < shapes.size(); ++j)
                level += (j != i && insideExact(shapes[i][0], shapes[j])) ? 1 : 0;
            refMismatches += level != expected[i];

            const bool odd = expected[i] % 2 == 1;
            ExtrudeContour e;
            e.isHole = odd;
            for (const IP &p : shapes[i])
                e.pts.push_back(Vector2f(static_cast<float>(p.x), static_cast<float>(p.y)));
            if (odd)
                std::reverse(e.pts.begin(), e.pts.end());
            cs.push_back(std::move(e));
        }
        const auto r = ValidateContours(cs);
        ++trials;
        contours += cs.size();
        if (!r.IsValid() || r.nestingLevel != expected)
            ++mismatches;
    }
    EXPECT_EQ(refMismatches, 0u);
    EXPECT_EQ(mismatches, 0u);
    // Non vacuite : rangees alignees et tours profondes presentes.
    EXPECT_EQ(trials, 1500u);
    EXPECT_GE(alignedTrials, 300u);
    EXPECT_GE(deep, 500u);
    EXPECT_GE(vertexRays, 300u);
    RecordProperty("contours", static_cast<int>(contours));
    RecordProperty("aligned_trials", static_cast<int>(alignedTrials));
    RecordProperty("level_3_or_more", static_cast<int>(deep));
    RecordProperty("rays_through_parent_vertex", static_cast<int>(vertexRays));
}
