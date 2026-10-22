#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : intersections exactes du plan (§6.6)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §8 J2-b).
// Tolerances : tol_point = 1e-9 * D, valeurs transversales a 1e-12 * D.

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double dist(const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }

    PlanCurve segment(double ax, double ay, double bx, double by) { return PlanSegment{Vector2d(ax, ay), Vector2d(bx, by)}; }

    // Arc de centre `c`, de rayon r, de a0 a a0 + sweep (degres).
    PlanCurve arcDeg(const Vector2d &c, double r, double a0, double sweep)
    {
        PlanArc a;
        a.center = c;
        a.radius = r;
        a.start_angle = a0 * kPi / 180.0;
        a.sweep = sweep * kPi / 180.0;
        return a;
    }

    // Arc de reference : centre (5,0), r = 5, de (1,3) a (8,4), sens horaire.
    PlanCurve referenceArc() { return MakeArc(Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), false); }

    PlanCurve scaled(const PlanCurve &c, double k)
    {
        if (const auto *s = std::get_if<PlanSegment>(&c))
            return PlanSegment{Vector2d(s->start.x * k, s->start.y * k), Vector2d(s->end.x * k, s->end.y * k)};
        PlanArc a = std::get<PlanArc>(c);
        a.center = Vector2d(a.center.x * k, a.center.y * k);
        a.radius *= k;
        return a;
    }

    // Intersect(b, a) est exactement Intersect(a, b) avec s1 et s2 echanges.
    void expectSymmetric(const PlanCurve &a, const PlanCurve &b, double tol)
    {
        const Intersection ab = Intersect(a, b, tol);
        const Intersection ba = Intersect(b, a, tol);
        ASSERT_EQ(ab.hits.size(), ba.hits.size());
        ASSERT_EQ(ab.overlaps.size(), ba.overlaps.size());
        // Les points de ba sont tries par s1 de ba, soit s2 de ab : on les apparie par position.
        for (const CurveHit &h : ab.hits)
        {
            const CurveHit *match = nullptr;
            for (const CurveHit &g : ba.hits)
                if (g.xy.x == h.xy.x && g.xy.y == h.xy.y)
                    match = &g;
            ASSERT_NE(match, nullptr) << "(" << h.xy.x << ", " << h.xy.y << ")";
            EXPECT_EQ(match->s1, h.s2);
            EXPECT_EQ(match->s2, h.s1);
            EXPECT_EQ(match->tangential, h.tangential);
            EXPECT_EQ(match->at_end1, h.at_end2);
            EXPECT_EQ(match->at_end2, h.at_end1);
        }
        for (const CurveOverlap &o : ab.overlaps)
        {
            const double lo2 = std::fmin(o.s2_begin, o.s2_end), hi2 = std::fmax(o.s2_begin, o.s2_end);
            const double lo1 = o.s2_begin <= o.s2_end ? o.s1_begin : o.s1_end;
            const double hi1 = o.s2_begin <= o.s2_end ? o.s1_end : o.s1_begin;
            bool found = false;
            for (const CurveOverlap &p : ba.overlaps)
                found = found || (p.s1_begin == lo2 && p.s1_end == hi2 && p.s2_begin == lo1 && p.s2_end == hi1);
            EXPECT_TRUE(found) << "[" << o.s1_begin << ", " << o.s1_end << "]";
        }
    }
}

// ---------------------------------------------------------------------------
//  Cas transversaux
// ---------------------------------------------------------------------------

// Moulinet : l = (3,-1)->(-1,2) et sa rotation de 90 deg, (1,3)->(-2,-1).
TEST(TEST_cgarchi_gothic_micro_intersect, pinwheel_segments_cross_once)
{
    const double D = 5.0;
    const PlanCurve l = segment(3, -1, -1, 2), m = segment(1, 3, -2, -1);
    const Intersection x = Intersect(l, m, 1e-9 * D);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.overlaps.empty());
    EXPECT_LE(dist(x.hits[0].xy, Vector2d(-0.2, 1.4)), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s1, 4.0, 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s2, 2.0, 1e-12 * D);
    EXPECT_FALSE(x.hits[0].tangential);
    EXPECT_FALSE(x.hits[0].at_end1);
    EXPECT_FALSE(x.hits[0].at_end2);
    expectSymmetric(l, m, 1e-9 * D);
}

// Arc (5,0) r 5 de (1,3) a (8,4) horaire et arc (0,5) r 5 de (3,1) a (4,8)
// direct : les cercles se coupent en (5,5) et (0,0) ; (0,0) n'est sur aucun arc.
TEST(TEST_cgarchi_gothic_micro_intersect, arcs_cross_once)
{
    const double D = 10.0;
    const PlanCurve a = referenceArc();
    const PlanCurve b = MakeArc(Vector2d(3, 1), Vector2d(4, 8), Vector2d(0, 5), true);
    const Intersection x = Intersect(a, b, 1e-9 * D);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.overlaps.empty());
    EXPECT_LE(dist(x.hits[0].xy, Vector2d(5, 5)), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s1, 5 * std::acos(0.6), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s2, 5 * std::acos(0.6), 1e-12 * D);
    EXPECT_FALSE(x.hits[0].tangential);
    expectSymmetric(a, b, 1e-9 * D);

    // Les deux points des cercles quand les arcs les couvrent : arcs complets a epsilon pres.
    const PlanCurve ca = arcDeg(Vector2d(5, 0), 5, 0, 359), cb = arcDeg(Vector2d(0, 5), 5, -1, 358);
    const Intersection y = Intersect(ca, cb, 1e-9 * D);
    ASSERT_EQ(y.hits.size(), 2u);
    const bool firstIsOrigin = dist(y.hits[0].xy, Vector2d(0, 0)) < dist(y.hits[1].xy, Vector2d(0, 0));
    EXPECT_LE(dist(y.hits[firstIsOrigin ? 0 : 1].xy, Vector2d(0, 0)), 1e-12 * D);
    EXPECT_LE(dist(y.hits[firstIsOrigin ? 1 : 0].xy, Vector2d(5, 5)), 1e-12 * D);
    EXPECT_LT(y.hits[0].s1, y.hits[1].s1);   // tries par s1
    expectSymmetric(ca, cb, 1e-9 * D);

    // Rayons distincts : cercle (0,0) r 5 et cercle (4,0) r 3, points (4, +/-3).
    const PlanCurve big = arcDeg(Vector2d(0, 0), 5, 0, 90), small = arcDeg(Vector2d(4, 0), 3, 0, 180);
    const Intersection z = Intersect(big, small, 1e-9 * D);
    ASSERT_EQ(z.hits.size(), 1u);
    EXPECT_LE(dist(z.hits[0].xy, Vector2d(4, 3)), 1e-12 * D);
    EXPECT_NEAR(z.hits[0].s1, 5 * std::atan2(3.0, 4.0), 1e-12 * D);
    EXPECT_NEAR(z.hits[0].s2, 3 * kPi / 2, 1e-12 * D);
    expectSymmetric(big, small, 1e-9 * D);
}

TEST(TEST_cgarchi_gothic_micro_intersect, segment_crosses_an_arc)
{
    const double D = 10.0;
    // Droite x = 5 : coupe le cercle (5,0) r 5 en (5,5) et (5,-5) ; seul (5,5) est sur l'arc.
    const PlanCurve s = segment(5, -6, 5, 6);
    const Intersection x = Intersect(s, referenceArc(), 1e-9 * D);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_LE(dist(x.hits[0].xy, Vector2d(5, 5)), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s1, 11.0, 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s2, 5 * std::acos(0.6), 1e-12 * D);
    EXPECT_FALSE(x.hits[0].tangential);
    expectSymmetric(s, referenceArc(), 1e-9 * D);

    // Contact a une extremite du segment, a l'interieur de l'arc.
    const Intersection y = Intersect(segment(5, 5, 5, 8), referenceArc(), 1e-9 * D);
    ASSERT_EQ(y.hits.size(), 1u);
    EXPECT_EQ(y.hits[0].s1, 0.0);
    EXPECT_TRUE(y.hits[0].at_end1);
    EXPECT_FALSE(y.hits[0].at_end2);
    // Les drapeaux suivent l'ordre des arguments.
    const Intersection yr = Intersect(referenceArc(), segment(5, 5, 5, 8), 1e-9 * D);
    ASSERT_EQ(yr.hits.size(), 1u);
    EXPECT_FALSE(yr.hits[0].at_end1);
    EXPECT_TRUE(yr.hits[0].at_end2);
}

// ---------------------------------------------------------------------------
//  Tangences
// ---------------------------------------------------------------------------

// Segment (0,5)->(10,5) et arc de centre (5,0), r 5 : tangent en (5,5).
TEST(TEST_cgarchi_gothic_micro_intersect, tangency_is_one_tangential_hit)
{
    const double D = 10.0, tol = 1e-9 * D;
    const Intersection x = Intersect(segment(0, 5, 10, 5), referenceArc(), tol);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.hits[0].tangential);
    EXPECT_LE(dist(x.hits[0].xy, Vector2d(5, 5)), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s1, 5.0, 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s2, 5 * std::acos(0.6), 1e-12 * D);
    expectSymmetric(segment(0, 5, 10, 5), referenceArc(), tol);

    // A 0,5 tol de la tangence, de part et d'autre : un seul point tangent.
    for (double gap : {-0.5 * tol, 0.5 * tol})
    {
        const Intersection z = Intersect(segment(0, 5 + gap, 10, 5 + gap), referenceArc(), tol);
        ASSERT_EQ(z.hits.size(), 1u) << "ecart " << gap;
        EXPECT_TRUE(z.hits[0].tangential);
    }

    // +1e-6 en y : la droite passe au-dessus du cercle.
    EXPECT_TRUE(Intersect(segment(0, 5 + 1e-6, 10, 5 + 1e-6), referenceArc(), tol).hits.empty());

    // -1e-6 en y : deux points a +/- sqrt(25 - (5 - 1e-6)^2) ~ 3,16e-3 de x = 5.
    const Intersection y = Intersect(segment(0, 5 - 1e-6, 10, 5 - 1e-6), referenceArc(), tol);
    ASSERT_EQ(y.hits.size(), 2u);
    const double half = std::sqrt(1e-6 * (10 - 1e-6));
    EXPECT_NEAR(y.hits[0].xy.x, 5 - half, 1e-9);
    EXPECT_NEAR(y.hits[1].xy.x, 5 + half, 1e-9);
    EXPECT_FALSE(y.hits[0].tangential);
    EXPECT_FALSE(y.hits[1].tangential);
}

TEST(TEST_cgarchi_gothic_micro_intersect, tangent_circles_give_one_tangential_hit)
{
    const double D = 4.0, tol = 1e-9 * D;
    // Exterieurement tangents en (1,0).
    const PlanCurve a = arcDeg(Vector2d(0, 0), 1, -90, 180), b = arcDeg(Vector2d(2, 0), 1, 90, 180);
    const Intersection x = Intersect(a, b, tol);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.hits[0].tangential);
    EXPECT_LE(dist(x.hits[0].xy, Vector2d(1, 0)), 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s1, kPi / 2, 1e-12 * D);
    EXPECT_NEAR(x.hits[0].s2, kPi / 2, 1e-12 * D);
    expectSymmetric(a, b, tol);

    // Interieurement tangents en (2,0) : cercle (0,0) r 2 et cercle (1,0) r 1.
    const PlanCurve big = arcDeg(Vector2d(0, 0), 2, -45, 90), small = arcDeg(Vector2d(1, 0), 1, -90, 180);
    const Intersection y = Intersect(big, small, tol);
    ASSERT_EQ(y.hits.size(), 1u);
    EXPECT_TRUE(y.hits[0].tangential);
    EXPECT_LE(dist(y.hits[0].xy, Vector2d(2, 0)), 1e-12 * D);
    EXPECT_NEAR(y.hits[0].s1, 2 * kPi / 4, 1e-12 * D);
    EXPECT_NEAR(y.hits[0].s2, kPi / 2, 1e-12 * D);
    expectSymmetric(big, small, tol);

    // Secants a 0,5 tol pres de la tangence : les deux racines, distantes de
    // ~sqrt(tol), sont un seul point tangent.
    for (double gap : {-0.5 * tol, 0.5 * tol})
    {
        const PlanCurve c = arcDeg(Vector2d(2 + gap, 0), 1, 90, 180);
        const Intersection z = Intersect(a, c, tol);
        ASSERT_EQ(z.hits.size(), 1u) << "ecart " << gap;
        EXPECT_TRUE(z.hits[0].tangential);
        EXPECT_LE(dist(z.hits[0].xy, Vector2d(1, 0)), tol);
        expectSymmetric(a, c, tol);
    }
}

// ---------------------------------------------------------------------------
//  Cas sans point commun
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_intersect, disjoint_curves_give_nothing)
{
    const double tol = 1e-8;
    const auto empty = [&](const PlanCurve &a, const PlanCurve &b) {
        const Intersection x = Intersect(a, b, tol);
        return x.hits.empty() && x.overlaps.empty();
    };
    EXPECT_TRUE(empty(segment(0, 0, 1, 0), segment(0, 1, 1, 2)));              // non paralleles, hors des segments
    EXPECT_TRUE(empty(segment(0, 0, 1, 0), segment(0, 1, 1, 1)));              // paralleles distincts
    EXPECT_TRUE(empty(segment(0, 0, 1, 0), segment(2, 0, 3, 0)));              // colineaires separes
    EXPECT_TRUE(empty(segment(0, 0, 1, 0), referenceArc()));                   // droite coupant le cercle hors de l'arc
    EXPECT_TRUE(empty(segment(20, 0, 21, 0), referenceArc()));                 // loin du cercle
    EXPECT_TRUE(empty(referenceArc(), arcDeg(Vector2d(30, 0), 5, 0, 90)));     // cercles disjoints
    EXPECT_TRUE(empty(arcDeg(Vector2d(0, 0), 5, 0, 90), arcDeg(Vector2d(1, 0), 1, 0, 90)));   // cercle interieur
    // Concentriques de rayons distincts.
    EXPECT_TRUE(empty(arcDeg(Vector2d(0, 0), 3, 0, 300), arcDeg(Vector2d(0, 0), 5, 0, 300)));
}

// ---------------------------------------------------------------------------
//  Courbes de meme support
// ---------------------------------------------------------------------------

// Piege du script de controle : colineaires qui se touchent par un bout.
TEST(TEST_cgarchi_gothic_micro_intersect, collinear_segments_touching_give_one_hit)
{
    const double D = 8.0, tol = 1e-9 * D;
    const Intersection x = Intersect(segment(4, 0, 0, 0), segment(-4, 0, 0, 0), tol);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.overlaps.empty());
    EXPECT_EQ(x.hits[0].xy.x, 0.0);
    EXPECT_EQ(x.hits[0].xy.y, 0.0);
    EXPECT_EQ(x.hits[0].s1, 4.0);
    EXPECT_EQ(x.hits[0].s2, 4.0);
    EXPECT_TRUE(x.hits[0].tangential);   // supports confondus
    EXPECT_TRUE(x.hits[0].at_end1);
    EXPECT_TRUE(x.hits[0].at_end2);
    expectSymmetric(segment(4, 0, 0, 0), segment(-4, 0, 0, 0), tol);

    // Sous un angle, extremite a 0,5 tol de l'autre segment : s recale sur 0 ou L.
    const Intersection t = Intersect(segment(0, 0, 4, 0), segment(4 - 0.5 * tol, 0, 4, 3), tol);
    ASSERT_EQ(t.hits.size(), 1u);
    EXPECT_EQ(t.hits[0].s1, 4.0);
    EXPECT_EQ(t.hits[0].s2, 0.0);

    // Meme sens de parcours, longueurs differentes.
    const Intersection y = Intersect(segment(0, 0, 4, 0), segment(4, 0, 5, 0), tol);
    ASSERT_EQ(y.hits.size(), 1u);
    EXPECT_EQ(y.hits[0].s1, 4.0);
    EXPECT_EQ(y.hits[0].s2, 0.0);
    expectSymmetric(segment(0, 0, 4, 0), segment(4, 0, 5, 0), tol);

    // Colineaires obliques, a un espace de 0,5 * tol : un contact.
    const double g = 0.5 * tol / std::sqrt(2.0);
    const Intersection z = Intersect(segment(0, 0, 1, 1), segment(1 + g, 1 + g, 3, 3), tol);
    ASSERT_EQ(z.hits.size(), 1u);
    EXPECT_TRUE(z.overlaps.empty());
}

// Paralleles a tol pres sans etre colineaires : le plus court, (0,0)->(10,0),
// s'ecarte de 1,8 tol de la droite du plus long. Les deux extremites en contact,
// a 0,9 tol l'une de l'autre, sont un seul point commun.
TEST(TEST_cgarchi_gothic_micro_intersect, parallel_segments_touching_within_tolerance_give_one_hit)
{
    const double tol = 1e-8;
    const PlanCurve a = segment(0, 0, 10, 0), b = segment(10, 0.9 * tol, 30, -0.9 * tol);
    const Intersection x = Intersect(a, b, tol);
    EXPECT_TRUE(x.overlaps.empty());
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_NEAR(x.hits[0].s1, 10.0, 1e-12 * 30);
    EXPECT_NEAR(x.hits[0].s2, 0.0, 1e-12 * 30);
    expectSymmetric(a, b, tol);
}

// Secants sous un angle rasant (pente 1e-8) : l'extremite (5 ; 0,5 tol) du
// second est a tol du premier, mais les droites se coupent en x = 4,5, hors de
// la marge en t. Le contact d'extremite est le point commun.
TEST(TEST_cgarchi_gothic_micro_intersect, grazing_segments_touch_by_an_endpoint)
{
    const double tol = 1e-8;
    const PlanCurve a = segment(0, 0, 10, 0), b = segment(5, 0.5 * tol, 15, 0.5 * tol + 1e-7);
    const Intersection x = Intersect(a, b, tol);
    ASSERT_EQ(x.hits.size(), 1u);
    EXPECT_TRUE(x.overlaps.empty());
    EXPECT_NEAR(x.hits[0].s1, 5.0, 1e-12 * 15);
    EXPECT_EQ(x.hits[0].s2, 0.0);
    EXPECT_FALSE(x.hits[0].tangential);
    EXPECT_FALSE(x.hits[0].at_end1);
    EXPECT_TRUE(x.hits[0].at_end2);
    expectSymmetric(a, b, tol);
}

// Raccords qui partagent EXACTEMENT une extremite P = (0,1), supports a moins de
// tol de la tangence ou un peu au-dela : un seul point commun, en P. Le pied de
// la tangence est a ~sqrt(2 r delta) de P et peut tomber hors d'une des courbes.
TEST(TEST_cgarchi_gothic_micro_intersect, near_tangent_joints_at_a_shared_endpoint)
{
    const double D = 4.0, tol = 1e-9 * D;
    const Vector2d P(0, 1);
    const auto expectJointAtP = [&](const PlanCurve &a, const PlanCurve &b) {
        const Intersection x = Intersect(a, b, tol);
        EXPECT_TRUE(x.overlaps.empty());
        ASSERT_EQ(x.hits.size(), 1u);
        EXPECT_LE(dist(x.hits[0].xy, P), tol);
        EXPECT_TRUE(x.hits[0].at_end1);
        EXPECT_TRUE(x.hits[0].at_end2);
        expectSymmetric(a, b, tol);
    };

    // Segment -> arc (centre (0,0), r 1, horaire depuis P).
    const PlanCurve arc = MakeArc(Vector2d(0, 1), Vector2d(1, 0), Vector2d(0, 0), false);
    for (double eta : {0.0, 1e-6, -1e-6, 1e-5, 5e-5, 1e-4, -1e-4, 1.5e-4, 2e-4, 1e-3})
    {
        SCOPED_TRACE(::testing::Message() << "segment -> arc, eta = " << eta);
        expectJointAtP(segment(-2, 1 + eta, 0, 1), arc);
    }

    // Arc -> arc en S : a finit en P, b (centre (xi, 2)) en part.
    const PlanCurve a = MakeArc(Vector2d(-1, 0), Vector2d(0, 1), Vector2d(0, 0), false);
    for (double xi : {0.0, 1e-5, 5e-5, 1e-4, -1e-4, 2e-4, 1e-3})
    {
        SCOPED_TRACE(::testing::Message() << "arc -> arc en S, xi = " << xi);
        expectJointAtP(a, MakeArc(Vector2d(0, 1), Vector2d(xi + 1, 2), Vector2d(xi, 2), true));
    }

    // Tangence interieure : b (centre (xi ; 0,5), r ~ 0,5) part de P du meme cote.
    for (double xi : {0.0, 1e-5})
    {
        SCOPED_TRACE(::testing::Message() << "tangence interieure, xi = " << xi);
        expectJointAtP(a, MakeArc(Vector2d(0, 1), Vector2d(xi + 0.5, 0.5), Vector2d(xi, 0.5), false));
    }

    // Rebroussements : les deux courbes quittent P du meme cote. Le pied de la
    // tangence, a ~5e-6 de P, est alors sur les deux courbes ; P reste le seul point.
    const PlanCurve back = MakeArc(Vector2d(0, 1), Vector2d(-1, 0), Vector2d(0, 0), true);
    for (double eta : {-1e-5, -1e-4})
    {
        SCOPED_TRACE(::testing::Message() << "rebroussement segment / arc, eta = " << eta);
        expectJointAtP(segment(-2, 1 + eta, 0, 1), back);
    }
    for (double xi : {-1e-5, -1e-4})
    {
        SCOPED_TRACE(::testing::Message() << "rebroussement arc / arc, xi = " << xi);
        expectJointAtP(a, MakeArc(Vector2d(0, 1), Vector2d(xi - 1, 2), Vector2d(xi, 2), false));
    }
}

// Arc qui part de P = (0,0), presque tangent a l'axe x (centre (xi, 1),
// r = sqrt(1 + xi^2)) : le seul point commun avec l'axe est P, comme avec son
// reflet par l'axe. Dans la bande tangente, le pied (xi, 0) est sur l'arc mais
// n'est pas un point commun.
TEST(TEST_cgarchi_gothic_micro_intersect, axis_nearly_tangent_at_an_arc_end)
{
    const double tol = 4e-9;
    const Axis xAxis{Vector2d(0, 0), Vector2d(1, 0)};
    for (double xi : {0.0, 1e-5, 5e-5})
    {
        SCOPED_TRACE(::testing::Message() << "xi = " << xi);
        const double r = std::sqrt(1 + xi * xi);
        const PlanCurve l = MakeArc(Vector2d(0, 0), Vector2d(xi + r, 1), Vector2d(xi, 1), true);
        const std::vector<CurveHit> h = IntersectAxis(l, xAxis, tol);
        ASSERT_EQ(h.size(), 1u);
        EXPECT_EQ(h[0].s1, 0.0);
        EXPECT_TRUE(h[0].at_end1);
        EXPECT_TRUE(h[0].tangential);
        // Oracle : intersection avec le reflet, un seul point, en s = 0 sur les deux.
        const PlanCurve g = MakeArc(Vector2d(0, 0), Vector2d(xi + r, -1), Vector2d(xi, -1), false);
        const Intersection x = Intersect(l, g, tol);
        ASSERT_EQ(x.hits.size(), 1u);
        EXPECT_LE(x.hits[0].s1, tol);
        EXPECT_LE(x.hits[0].s2, tol);
    }
}

// Arcs de 0 a 120 deg et de 90 a 210 deg, centres ecartes de 1,5 tol, rayons
// de 0,7 tol : sur la portion commune (90 a 120 deg), l'ecart entre les cercles
// est au plus 0,7 tol. C'est un recouvrement, pas deux contacts.
TEST(TEST_cgarchi_gothic_micro_intersect, nearly_co_circular_arcs_overlap)
{
    const double tol = 4e-9;
    const PlanCurve a = arcDeg(Vector2d(0, 0), 1, 0, 120);
    const PlanCurve b = arcDeg(Vector2d(1.5 * tol, 0), 1 + 0.7 * tol, 90, 120);
    const Intersection x = Intersect(a, b, tol);
    EXPECT_TRUE(x.hits.empty());
    ASSERT_EQ(x.overlaps.size(), 1u);
    EXPECT_NEAR(x.overlaps[0].s1_begin, kPi / 2, 1e-8);
    EXPECT_NEAR(x.overlaps[0].s1_end, 2 * kPi / 3, 1e-8);
    expectSymmetric(a, b, tol);

    // Dans la pre-condition (d <= 2 tol, |dr| <= 2 tol), toute portion commune plus
    // longue que tol est un recouvrement, quel que soit l'ecart (au plus 4 tol, M18) :
    // ecart de 1,9 tol des 90 deg, puis ecart de 1,9 tol sur la ligne des centres.
    const PlanCurve far = arcDeg(Vector2d(1.9 * tol, 0), 1 - 1.9 * tol, 90, 120);
    EXPECT_EQ(Intersect(a, far, tol).overlaps.size(), 1u);
    const PlanCurve m1 = arcDeg(Vector2d(0, 0), 1, -60, 120), m2 = arcDeg(Vector2d(1.9 * tol, 0), 1, -60, 120);
    EXPECT_EQ(Intersect(m1, m2, tol).overlaps.size(), 1u);
    // Centres a 0,9 tol, rayons a 1,5 tol (au-dela de l'ancienne regle |dr| <= tol) :
    // recouvrement aussi.
    EXPECT_EQ(Intersect(m1, arcDeg(Vector2d(0.9 * tol, 0), 1 + 1.5 * tol, -60, 120), tol).overlaps.size(), 1u);
    // Hors de la pre-condition (d = 2,5 tol) : pas de recouvrement.
    EXPECT_TRUE(Intersect(m1, arcDeg(Vector2d(2.5 * tol, 0), 1, -60, 120), tol).overlaps.empty());

    // Centres ecartes de 1e-3 : deux cercles distincts qui se coupent pres de
    // (0,1) ; pas de recouvrement.
    const PlanCurve c = arcDeg(Vector2d(1e-3, 0), 1, 90, 120);
    const Intersection y = Intersect(a, c, tol);
    EXPECT_TRUE(y.overlaps.empty());
    ASSERT_EQ(y.hits.size(), 1u);
    EXPECT_LE(dist(y.hits[0].xy, Vector2d(5e-4, std::sqrt(1 - 25e-8))), 1e-12);

    // Vraie tangence interieure (d = 0,5, rayons 1 et 0,5) : un point, pas un
    // recouvrement, meme si les intervalles angulaires se recouvrent.
    const PlanCurve big = arcDeg(Vector2d(0, 0), 1, -30, 60), small = arcDeg(Vector2d(0.5, 0), 0.5, -30, 60);
    const Intersection z = Intersect(big, small, tol);
    EXPECT_TRUE(z.overlaps.empty());
    ASSERT_EQ(z.hits.size(), 1u);
    EXPECT_TRUE(z.hits[0].tangential);
}

TEST(TEST_cgarchi_gothic_micro_intersect, collinear_overlap)
{
    const double D = 6.0, tol = 1e-9 * D;
    const Intersection x = Intersect(segment(0, 0, 4, 0), segment(2, 0, 6, 0), tol);
    EXPECT_TRUE(x.hits.empty());
    ASSERT_EQ(x.overlaps.size(), 1u);
    EXPECT_NEAR(x.overlaps[0].s1_begin, 2.0, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s1_end, 4.0, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s2_begin, 0.0, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s2_end, 2.0, 1e-12 * D);
    expectSymmetric(segment(0, 0, 4, 0), segment(2, 0, 6, 0), tol);

    // Sens opposes : s2 decroit quand s1 croit.
    const Intersection y = Intersect(segment(0, 0, 4, 0), segment(6, 0, 2, 0), tol);
    ASSERT_EQ(y.overlaps.size(), 1u);
    EXPECT_NEAR(y.overlaps[0].s1_begin, 2.0, 1e-12 * D);
    EXPECT_NEAR(y.overlaps[0].s1_end, 4.0, 1e-12 * D);
    EXPECT_NEAR(y.overlaps[0].s2_begin, 4.0, 1e-12 * D);
    EXPECT_NEAR(y.overlaps[0].s2_end, 2.0, 1e-12 * D);
    expectSymmetric(segment(0, 0, 4, 0), segment(6, 0, 2, 0), tol);

    // Le plus court contenu dans le plus long, le plus court en premier.
    const Intersection z = Intersect(segment(1, 1, 2, 2), segment(0, 0, 3, 3), tol);
    ASSERT_EQ(z.overlaps.size(), 1u);
    EXPECT_NEAR(z.overlaps[0].s1_begin, 0.0, 1e-12 * D);
    EXPECT_NEAR(z.overlaps[0].s1_end, std::sqrt(2.0), 1e-12 * D);
    EXPECT_NEAR(z.overlaps[0].s2_begin, std::sqrt(2.0), 1e-12 * D);
    EXPECT_NEAR(z.overlaps[0].s2_end, 2 * std::sqrt(2.0), 1e-12 * D);
}

TEST(TEST_cgarchi_gothic_micro_intersect, co_circular_arcs)
{
    const double r = 3.0, D = 6.0, tol = 1e-9 * D;
    // 0 a 120 deg et 90 a 210 deg : portion commune de 90 a 120 deg.
    const PlanCurve a = arcDeg(Vector2d(0, 0), r, 0, 120), b = arcDeg(Vector2d(0, 0), r, 90, 120);
    const Intersection x = Intersect(a, b, tol);
    EXPECT_TRUE(x.hits.empty());
    ASSERT_EQ(x.overlaps.size(), 1u);
    EXPECT_NEAR(x.overlaps[0].s1_begin, r * kPi / 2, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s1_end, r * 2 * kPi / 3, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s2_begin, 0.0, 1e-12 * D);
    EXPECT_NEAR(x.overlaps[0].s2_end, r * kPi / 6, 1e-12 * D);
    expectSymmetric(a, b, tol);

    // Sens horaire pour le second : 210 -> 90 deg ; s2 decroit.
    const PlanCurve bc = arcDeg(Vector2d(0, 0), r, 210, -120);
    const Intersection y = Intersect(a, bc, tol);
    ASSERT_EQ(y.overlaps.size(), 1u);
    EXPECT_NEAR(y.overlaps[0].s2_begin, r * 2 * kPi / 3, 1e-12 * D);
    EXPECT_NEAR(y.overlaps[0].s2_end, r * kPi / 2, 1e-12 * D);
    expectSymmetric(a, bc, tol);

    // Deux portions : 0 -> 270 et 180 -> 450 deg se recouvrent sur [0, 90] et [180, 270].
    const PlanCurve c = arcDeg(Vector2d(0, 0), r, 0, 270), d = arcDeg(Vector2d(0, 0), r, 180, 270);
    const Intersection z = Intersect(c, d, tol);
    EXPECT_TRUE(z.hits.empty());
    ASSERT_EQ(z.overlaps.size(), 2u);
    EXPECT_NEAR(z.overlaps[0].s1_begin, 0.0, 1e-12 * D);
    EXPECT_NEAR(z.overlaps[0].s1_end, r * kPi / 2, 1e-12 * D);
    EXPECT_NEAR(z.overlaps[1].s1_begin, r * kPi, 1e-12 * D);
    EXPECT_NEAR(z.overlaps[1].s1_end, r * 3 * kPi / 2, 1e-12 * D);
    expectSymmetric(c, d, tol);

    // Bout a bout : 0 -> 90 et 90 -> 180 deg, un contact en (0, 3), non tangent.
    const PlanCurve e = arcDeg(Vector2d(0, 0), r, 0, 90), f = arcDeg(Vector2d(0, 0), r, 90, 90);
    const Intersection w = Intersect(e, f, tol);
    EXPECT_TRUE(w.overlaps.empty());
    ASSERT_EQ(w.hits.size(), 1u);
    EXPECT_LE(dist(w.hits[0].xy, Vector2d(0, 3)), 1e-12 * D);
    EXPECT_NEAR(w.hits[0].s1, r * kPi / 2, 1e-12 * D);
    EXPECT_NEAR(w.hits[0].s2, 0.0, 1e-12 * D);
    EXPECT_TRUE(w.hits[0].tangential);   // supports confondus
    EXPECT_TRUE(w.hits[0].at_end1);
    EXPECT_TRUE(w.hits[0].at_end2);
    expectSymmetric(e, f, tol);

    // Bout a bout a travers l'angle 0 : 270 -> 360 et 0 -> 90 deg.
    const Intersection v = Intersect(arcDeg(Vector2d(0, 0), r, 270, 90), arcDeg(Vector2d(0, 0), r, 0, 90), tol);
    EXPECT_TRUE(v.overlaps.empty());
    ASSERT_EQ(v.hits.size(), 1u);
    EXPECT_LE(dist(v.hits[0].xy, Vector2d(3, 0)), 1e-12 * D);
}

// ---------------------------------------------------------------------------
//  Invariance d'echelle et entrees invalides
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_intersect, results_are_scale_invariant)
{
    const double D = 10.0;
    const std::vector<std::pair<PlanCurve, PlanCurve>> pairs = {
        {segment(3, -1, -1, 2), segment(1, 3, -2, -1)},
        {referenceArc(), MakeArc(Vector2d(3, 1), Vector2d(4, 8), Vector2d(0, 5), true)},
        {segment(0, 5, 10, 5), referenceArc()},
        {segment(0, 5 - 1e-6, 10, 5 - 1e-6), referenceArc()},
        {segment(4, 0, 0, 0), segment(-4, 0, 0, 0)},
        {segment(0, 0, 4, 0), segment(2, 0, 6, 0)},
        {arcDeg(Vector2d(0, 0), 3, 0, 120), arcDeg(Vector2d(0, 0), 3, 90, 120)},
        {arcDeg(Vector2d(0, 0), 1, -90, 180), arcDeg(Vector2d(2, 0), 1, 90, 180)},
    };
    for (std::size_t i = 0; i < pairs.size(); ++i)
    {
        const PlanCurve &a = pairs[i].first, &b = pairs[i].second;
        const Intersection ref = Intersect(a, b, 1e-9 * D);
        ASSERT_FALSE(ref.hits.empty() && ref.overlaps.empty()) << "paire " << i;
        for (double k : {1e3, 1e-3})
        {
            SCOPED_TRACE(::testing::Message() << "paire " << i << ", echelle " << k);
            const PlanCurve ak = scaled(a, k), bk = scaled(b, k);
            const Intersection x = Intersect(ak, bk, 1e-9 * D * k);
            ASSERT_EQ(x.hits.size(), ref.hits.size());
            ASSERT_EQ(x.overlaps.size(), ref.overlaps.size());
            const double la = Length(a), lb = Length(b), lak = Length(ak), lbk = Length(bk);
            for (std::size_t h = 0; h < x.hits.size(); ++h)
            {
                EXPECT_NEAR(x.hits[h].s1 / lak, ref.hits[h].s1 / la, 1e-9);
                EXPECT_NEAR(x.hits[h].s2 / lbk, ref.hits[h].s2 / lb, 1e-9);
                EXPECT_EQ(x.hits[h].tangential, ref.hits[h].tangential);
            }
            for (std::size_t o = 0; o < x.overlaps.size(); ++o)
            {
                EXPECT_NEAR(x.overlaps[o].s1_begin / lak, ref.overlaps[o].s1_begin / la, 1e-9);
                EXPECT_NEAR(x.overlaps[o].s1_end / lak, ref.overlaps[o].s1_end / la, 1e-9);
                EXPECT_NEAR(x.overlaps[o].s2_begin / lbk, ref.overlaps[o].s2_begin / lb, 1e-9);
                EXPECT_NEAR(x.overlaps[o].s2_end / lbk, ref.overlaps[o].s2_end / lb, 1e-9);
            }
        }
    }
}

TEST(TEST_cgarchi_gothic_micro_intersect, invalid_inputs_give_nothing)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const PlanCurve s = segment(3, -1, -1, 2), m = segment(1, 3, -2, -1);
    ASSERT_EQ(Intersect(s, m, 1e-8).hits.size(), 1u);
    EXPECT_TRUE(Intersect(s, m, nan).hits.empty());
    EXPECT_TRUE(Intersect(s, m, -1.0).hits.empty());
    EXPECT_TRUE(Intersect(segment(0, 0, 0, 0), m, 1e-8).hits.empty());
    EXPECT_TRUE(Intersect(s, segment(nan, 0, 1, 1), 1e-8).hits.empty());
    PlanArc zero = std::get<PlanArc>(referenceArc());
    zero.sweep = 0.0;
    EXPECT_TRUE(Intersect(segment(5, -6, 5, 6), zero, 1e-8).hits.empty());
    // Balayage hors de (0, 2*pi) : contrat de PlanArc.
    PlanArc full;
    full.center = Vector2d(0, 0);
    full.radius = 1;
    full.sweep = 7.0;
    ASSERT_EQ(Intersect(segment(0.5, -2, 0.5, 2), arcDeg(Vector2d(0, 0), 1, 0, 359), 1e-8).hits.size(), 2u);
    EXPECT_TRUE(Intersect(segment(0.5, -2, 0.5, 2), full, 1e-8).hits.empty());
    full.sweep = -2 * kPi;
    EXPECT_TRUE(Intersect(segment(0.5, -2, 0.5, 2), full, 1e-8).hits.empty());
    EXPECT_TRUE(IntersectAxis(full, Axis{Vector2d(0.5, 0), Vector2d(0, 1)}, 1e-8).empty());
    EXPECT_TRUE(IntersectAxis(s, Axis{Vector2d(0, 0), Vector2d(0, 0)}, 1e-8).empty());
    EXPECT_TRUE(IntersectAxis(s, Axis{Vector2d(0, 0), Vector2d(1, 1)}, nan).empty());
}

// ---------------------------------------------------------------------------
//  IntersectAxis
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_intersect, axis_intersections)
{
    const double D = std::sqrt(41.0), tol = 1e-9 * D;
    const Axis diagonal{Vector2d(0, 0), Vector2d(std::sqrt(0.5), std::sqrt(0.5))};

    // Segment de reference (4,1)->(1,5) et axe y = x : (19/7, 19/7), s = 15/7.
    const std::vector<CurveHit> h = IntersectAxis(segment(4, 1, 1, 5), diagonal, tol);
    ASSERT_EQ(h.size(), 1u);
    EXPECT_LE(dist(h[0].xy, Vector2d(19.0 / 7, 19.0 / 7)), 1e-12 * D);
    EXPECT_NEAR(h[0].s1, 15.0 / 7, 1e-12 * D);
    EXPECT_NEAR(h[0].s2, 19.0 * std::sqrt(2.0) / 7, 1e-12 * D);
    EXPECT_FALSE(h[0].tangential);

    // Segment porte par l'axe : aucun point.
    EXPECT_TRUE(IntersectAxis(segment(1, 1, 3, 3), diagonal, tol).empty());
    // Segment d'un seul cote : aucun point.
    EXPECT_TRUE(IntersectAxis(segment(4, 1, 5, 2), diagonal, tol).empty());
    // Extremite sur l'axe : un point en s = 0.
    const std::vector<CurveHit> e = IntersectAxis(segment(2, 2, 4, 1), diagonal, tol);
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].s1, 0.0);
    EXPECT_TRUE(e[0].at_end1);
    EXPECT_FALSE(e[0].at_end2);
    // Arrivee sur l'axe : un point en s = L.
    const std::vector<CurveHit> f = IntersectAxis(segment(4, 1, 2, 2), diagonal, tol);
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].s1, std::hypot(2.0, 1.0));
    EXPECT_LE(dist(f[0].xy, Vector2d(2, 2)), 1e-12 * D);
    EXPECT_TRUE(f[0].at_end1);
    // Segment rasant : depart a 0,9 tol de l'axe, arrivee a -1,1 tol. Les signes
    // sont opposes : la traversee est en t = 0,9 / 2 = 0,45, pas au depart.
    const std::vector<CurveHit> g = IntersectAxis(segment(0, 0.9 * tol, 1, -1.1 * tol), Axis{Vector2d(0, 0), Vector2d(1, 0)}, tol);
    ASSERT_EQ(g.size(), 1u);
    EXPECT_NEAR(g[0].s1, 0.45, 1e-12);
    EXPECT_FALSE(g[0].at_end1);
    // Traversee a ~0,5 tol du depart : recalee exactement sur s = 0 (M14).
    const std::vector<CurveHit> k = IntersectAxis(segment(0, 0.5 * tol, 1, -1), Axis{Vector2d(0, 0), Vector2d(1, 0)}, tol);
    ASSERT_EQ(k.size(), 1u);
    EXPECT_TRUE(k[0].at_end1);
    EXPECT_EQ(k[0].s1, 0.0);
    // Direction non unitaire : meme resultat, s2 en unites de longueur.
    const std::vector<CurveHit> n = IntersectAxis(segment(4, 1, 1, 5), Axis{Vector2d(0, 0), Vector2d(3, 3)}, tol);
    ASSERT_EQ(n.size(), 1u);
    EXPECT_NEAR(n[0].s2, 19.0 * std::sqrt(2.0) / 7, 1e-12 * D);

    // Arc de reference et axe y = x : (0,0) et (5,5) sur le cercle, seul (5,5) sur l'arc.
    const double Da = 10.0;
    const std::vector<CurveHit> a = IntersectAxis(referenceArc(), diagonal, 1e-9 * Da);
    ASSERT_EQ(a.size(), 1u);
    EXPECT_LE(dist(a[0].xy, Vector2d(5, 5)), 1e-12 * Da);
    EXPECT_NEAR(a[0].s1, 5 * std::acos(0.6), 1e-12 * Da);
    EXPECT_NEAR(a[0].s2, 5 * std::sqrt(2.0), 1e-12 * Da);
    EXPECT_FALSE(a[0].tangential);

    // Axe y = 5, tangent au cercle en (5,5).
    const std::vector<CurveHit> t = IntersectAxis(referenceArc(), Axis{Vector2d(0, 5), Vector2d(1, 0)}, 1e-9 * Da);
    ASSERT_EQ(t.size(), 1u);
    EXPECT_TRUE(t[0].tangential);
    EXPECT_NEAR(t[0].s2, 5.0, 1e-12 * Da);

    // Axe x = 5 : deux points du cercle, (5,5) et (5,-5) ; un arc complet a epsilon pres les porte.
    const std::vector<CurveHit> two = IntersectAxis(arcDeg(Vector2d(5, 0), 5, 1, 358), Axis{Vector2d(5, 0), Vector2d(0, 1)}, 1e-9 * Da);
    ASSERT_EQ(two.size(), 2u);
    EXPECT_LT(two[0].s1, two[1].s1);
    EXPECT_NEAR(two[0].s2, 5.0, 1e-12 * Da);
    EXPECT_NEAR(two[1].s2, -5.0, 1e-12 * Da);
}

// ---------------------------------------------------------------------------
//  Arc a centre lointain (limite L2, §10.3) : domaine verifie
// ---------------------------------------------------------------------------

// Corde de (-1,0) a (1,0), fleche vers le haut, rayon rho * D. Sur ce domaine,
// les extremites et le sommet de l'arc sont retrouves a tol_point = 1e-9 * D.
TEST(TEST_cgarchi_gothic_micro_intersect, far_centre_arcs_in_the_checked_range)
{
    const double D = 2.0, tol = 1e-9 * D;
    for (double rho : {1.0, 1e2, 1e4})
    {
        SCOPED_TRACE(::testing::Message() << "r / D = " << rho);
        const double R = rho * D;
        const double h = std::sqrt((R - 1) * (R + 1));
        const PlanCurve arc = MakeArc(Vector2d(1, 0), Vector2d(-1, 0), Vector2d(0, -h), true);
        const double L = Length(arc);
        // Les extremites sont sur l'arc.
        ASSERT_TRUE(ParameterOf(arc, Vector2d(1, 0), tol).has_value());
        ASSERT_TRUE(ParameterOf(arc, Vector2d(-1, 0), tol).has_value());
        // Segment vertical par le sommet : s = L/2.
        const Intersection x = Intersect(segment(0, -1, 0, 1), arc, tol);
        ASSERT_EQ(x.hits.size(), 1u);
        EXPECT_NEAR(x.hits[0].s2, 0.5 * L, 1e-9 * D);
        // Segment qui touche l'arc par son extremite (1,0) : un contact en s = 0.
        const Intersection y = Intersect(segment(1, 0, 1, 1), arc, tol);
        ASSERT_EQ(y.hits.size(), 1u);
        EXPECT_NEAR(y.hits[0].s2, 0.0, 1e-9 * D);
    }
}
