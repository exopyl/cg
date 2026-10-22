#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : PRC (etape 2, Algorithme 1 et selection)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §6.2, §8 J2-c,
// §9). Tolerance : 1e-12 * D sur les abscisses analytiques.

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";
    const char *const kC4Pinwheel = "./test/data/gothic_micro_c4_pinwheel.json";

    double dist(const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }

    SymmetrySpec symmetry(int n, std::optional<double> axisDeg)
    {
        SymmetrySpec s;
        s.center = Vector2d(0, 0);
        s.rotation_order = n;
        s.reflection_axis_deg = axisDeg;
        return s;
    }

    PointSpec point(const std::string &id, double x, double y, std::optional<double> height)
    {
        PointSpec p;
        p.id = id;
        p.xy = Vector2d(x, y);
        p.ground = !height;
        p.height = height;
        return p;
    }

    BasicCurveSpec segmentCurve(const std::string &id, const MicroarchSpec &spec, int a, int b)
    {
        BasicCurveSpec c;
        c.id = id;
        c.start_point = a;
        c.end_point = b;
        c.plan = PlanSegment{spec.points[a].xy, spec.points[b].xy};
        return c;
    }

    BasicCurveSpec arcCurve(const std::string &id, const MicroarchSpec &spec, int a, int b, const Vector2d &center, bool ccw)
    {
        BasicCurveSpec c = segmentCurve(id, spec, a, b);
        c.plan = MakeArc(spec.points[a].xy, spec.points[b].xy, center, ccw);
        return c;
    }

    // Segment (x0,y0) -> (x1,y1), depart au sol, arrivee a `height`.
    MicroarchSpec oneSegment(const SymmetrySpec &sym, double x0, double y0, double x1, double y1, double height)
    {
        MicroarchSpec spec;
        spec.symmetry = sym;
        spec.points = {point("A", x0, y0, std::nullopt), point("B", x1, y1, height)};
        spec.curves = {segmentCurve("l", spec, 0, 1)};
        return spec;
    }

    // Arc de reference (§9.3) : centre (5,0), r 5, de (1,3) a (8,4), horaire.
    MicroarchSpec referenceArc(const SymmetrySpec &sym)
    {
        MicroarchSpec spec;
        spec.symmetry = sym;
        spec.points = {point("C", 1, 3, std::nullopt), point("E", 8, 4, 5 * kPi)};
        spec.curves = {arcCurve("l", spec, 0, 1, Vector2d(5, 0), false)};
        return spec;
    }

    struct PrcRun
    {
        DevelopedPlan                         plan;
        MicroarchReport                       report;
        std::optional<std::vector<PrcResult>> prc;
    };

    PrcRun run(const MicroarchSpec &spec)
    {
        PrcRun r;
        const std::optional<DevelopedPlan> plan = DevelopPlan(spec, r.report);
        EXPECT_TRUE(plan.has_value()) << ReportToJson(r.report);
        if (!plan)
            return r;
        r.plan = *plan;
        r.prc = ComputePrcs(spec, r.plan, r.report);
        return r;
    }

    MicroarchSpec loadFile(const char *path)
    {
        MicroarchSpec spec;
        MicroarchReport report;
        EXPECT_TRUE(LoadSpecFromFile(path, spec, report)) << ReportToJson(report);
        return spec;
    }

    std::size_t warnings(const MicroarchReport &r, MicroarchWarning w) { return r.Count(w); }

    // Abscisses du palier 1 rendues par ComputePrcs (points fixes du groupe).
    std::vector<double> tierOne(const PrcResult &prc)
    {
        std::vector<double> s;
        for (const PrcCandidate &c : prc.candidates)
            if (c.tier == PrcRule::ReplicaFixedPoint)
                s.push_back(c.s);
        return s;
    }

    // Oracle generique du palier 1 (§6.2) : intersections de l avec chacune de
    // ses images g(l), g != id, retenues si |l(s) - g(l(s))| <= tol.point dans le
    // plan, interieures (A-3), fusionnees a tol.parameter pres. Independant des
    // axes miroirs et du centre.
    std::vector<double> oracleTierOne(const MicroarchSpec &spec, const ResolvedTolerances &tol, std::size_t i)
    {
        const PlanCurve &l = spec.curves[i].plan;
        const double L = Length(l);
        std::vector<double> s;
        for (SymmetryElement g : Elements(spec.symmetry))
        {
            if (g.k == 0 && !g.reflected)
                continue;
            const Intersection x = Intersect(l, Apply(spec.symmetry, g, l), tol.point);
            std::vector<double> found;
            for (const CurveHit &h : x.hits)
                found.push_back(h.s1);
            // Portion commune parcourue en sens oppose (l et g(l) sur le meme support) :
            // s1 - s2 y est affine, nul en un point. En meme sens, l'ecart est
            // constant : soit aucun point, soit toute la portion (stabilisateur, sautee).
            for (const CurveOverlap &o : x.overlaps)
            {
                const double d0 = o.s1_begin - o.s2_begin;
                const double slope = (o.s1_end - o.s1_begin) - (o.s2_end - o.s2_begin);
                if (std::fabs(slope) <= tol.parameter)
                    continue;
                const double t = -d0 / slope;
                if (t >= 0.0 && t <= 1.0)
                    found.push_back(o.s1_begin + t * (o.s1_end - o.s1_begin));
            }
            for (double v : found)
            {
                // Point fixe : l(s) a tol.point de son image par g, dans le plan.
                const Vector2d p = PointAt(l, v);
                if (dist(p, Apply(spec.symmetry, g, p)) > tol.point || !(v > tol.point && v < L - tol.point))
                    continue;
                if (std::none_of(s.begin(), s.end(), [&](double w) { return std::fabs(w - v) <= tol.parameter; }))
                    s.push_back(v);
            }
        }
        std::sort(s.begin(), s.end());
        return s;
    }

    // Les deux instruments du palier 1 coincident, cardinal compris.
    void expectOracle(const MicroarchSpec &spec, const PrcRun &r)
    {
        ASSERT_TRUE(r.prc.has_value());
        for (std::size_t i = 0; i < spec.curves.size(); ++i)
        {
            const std::vector<double> fixed = tierOne((*r.prc)[i]);
            const std::vector<double> oracle = oracleTierOne(spec, r.plan.tol, i);
            ASSERT_EQ(fixed.size(), oracle.size()) << "courbe " << i;
            for (std::size_t k = 0; k < fixed.size(); ++k)
                EXPECT_NEAR(fixed[k], oracle[k], 1e-9 * r.plan.tol.scale) << "courbe " << i << ", candidat " << k;
        }
    }
}

// ---------------------------------------------------------------------------
//  Palier 1 : points fixes
// ---------------------------------------------------------------------------

// Reference D4 segment (§9.1) : l'axe y = x coupe l en t = 3/7.
TEST(TEST_cgarchi_gothic_micro_prc, d4_segment_reference)
{
    const MicroarchSpec spec = loadFile(kD4Segment);
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const double D = r.plan.tol.scale;
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
    EXPECT_NEAR(p.s, 15.0 / 7, 1e-12 * D);
    EXPECT_NEAR(p.s / Length(spec.curves[0].plan), 3.0 / 7, 1e-12);
    const Vector2d xy = PointAt(spec.curves[0].plan, p.s);
    EXPECT_LE(dist(xy, Vector2d(19.0 / 7, 19.0 / 7)), 1e-12 * D);
    // Point fixe de la reflexion d'axe y = x : l'element (k = 3, reflechi), psi = 90 deg.
    EXPECT_LE(dist(Apply(spec.symmetry, SymmetryElement{3, true}, xy), xy), 1e-12 * D);
    EXPECT_EQ(r.report.WarningCount(), 0u) << ReportToJson(r.report);
    ASSERT_EQ(tierOne(p).size(), 1u);
    expectOracle(spec, r);
}

// D4 arc (§9.3) : l'axe y = x coupe l'arc en (5,5).
TEST(TEST_cgarchi_gothic_micro_prc, d4_arc)
{
    const MicroarchSpec spec = referenceArc(symmetry(4, 90.0));
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const double D = r.plan.tol.scale;
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
    EXPECT_NEAR(p.s, 5 * std::acos(0.6), 1e-12 * D);
    EXPECT_LE(dist(PointAt(spec.curves[0].plan, p.s), Vector2d(5, 5)), 1e-12 * D);
    EXPECT_EQ(r.report.WarningCount(), 0u) << ReportToJson(r.report);
    expectOracle(spec, r);
}

// Segment (-2,-2) -> (2,2) sous C3 : le centre de rotation, point fixe, est au
// milieu ; il reste un point fixe (palier 1), pas le milieu (palier 3).
TEST(TEST_cgarchi_gothic_micro_prc, rotation_centre_is_a_fixed_point)
{
    const MicroarchSpec spec = oneSegment(symmetry(3, std::nullopt), -2, -2, 2, 2, 10);
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
    EXPECT_NEAR(p.s, 2 * std::sqrt(2.0), 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(r.report.WarningCount(), 0u) << ReportToJson(r.report);
    expectOracle(spec, r);
}

// Le palier 1 l'emporte sur un candidat du palier 2 plus proche du milieu :
// segment de reference D4 (point fixe s = 15/7) et une seconde courbe de base
// qui le coupe en son milieu, (2,5 ; 3), s = 2,5.
TEST(TEST_cgarchi_gothic_micro_prc, fixed_point_tier_wins_over_other_curves)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(4, 90.0);
    spec.points = {point("A", 4, 1, std::nullopt), point("B", 1, 5, 10.0),
                   point("C", 1.7, 2.4, std::nullopt), point("E", 3.3, 3.6, 10.0)};
    spec.curves = {segmentCurve("l1", spec, 0, 1), segmentCurve("l2", spec, 2, 3)};
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
    EXPECT_NEAR(p.s, 15.0 / 7, 1e-12 * r.plan.tol.scale);
    const bool seesOther = std::any_of(p.candidates.begin(), p.candidates.end(), [&](const PrcCandidate &c) {
        return c.tier == PrcRule::OtherBasicCurve && std::fabs(c.s - 2.5) <= 1e-12 * r.plan.tol.scale;
    });
    EXPECT_TRUE(seesOther);
    EXPECT_EQ((*r.prc)[1].rule, PrcRule::OtherBasicCurve);
    expectOracle(spec, r);
}

// ---------------------------------------------------------------------------
//  Palier 3 : milieu
// ---------------------------------------------------------------------------

// C4 arc : le meme arc sans reflexion. Une rotation pure n'a que le centre pour
// point fixe, et le centre n'est pas sur l'arc.
TEST(TEST_cgarchi_gothic_micro_prc, c4_arc_falls_back_to_the_midpoint)
{
    const MicroarchSpec spec = referenceArc(symmetry(4, std::nullopt));
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::Midpoint);
    EXPECT_NEAR(p.s, 5 * kPi / 4, 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcMidpointFallback), 1u);
    EXPECT_EQ(r.report.WarningCount(), 1u) << ReportToJson(r.report);
    expectOracle(spec, r);
}

// Moulinet C4 : l coupe R90(l) en s = 4 (s' = 2) et R270(l) en s = 2 (s' = 4).
// Regle en vigueur, A-1 et M7 : le palier 1 ne retient que s = s', donc aucun
// de ces croisements ; le PRC est le milieu, s = 2,5, et non s = 2.
TEST(TEST_cgarchi_gothic_micro_prc, pinwheel_takes_the_midpoint_not_a_false_crossing)
{
    const MicroarchSpec spec = loadFile(kC4Pinwheel);
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::Midpoint);
    EXPECT_NEAR(p.s, 2.5, 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcMidpointFallback), 1u);
    // Les croisements faux sont vus, pour la selection explicite.
    std::vector<double> crossings;
    for (const PrcCandidate &c : p.candidates)
        if (c.tier == PrcRule::ReplicaCrossing)
            crossings.push_back(c.s);
    ASSERT_EQ(crossings.size(), 2u);
    EXPECT_NEAR(crossings[0], 2.0, 1e-12 * r.plan.tol.scale);
    EXPECT_NEAR(crossings[1], 4.0, 1e-12 * r.plan.tol.scale);
    expectOracle(spec, r);
}

// Etoile : segment (4,0) -> (0,0) sous D4. Le centre est une extremite (exclue,
// A-3) ; l'axe x porte le segment (saute) ; les autres axes passent par (0,0).
TEST(TEST_cgarchi_gothic_micro_prc, star_takes_the_midpoint)
{
    const MicroarchSpec spec = oneSegment(symmetry(4, 90.0), 4, 0, 0, 0, 8);
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::Midpoint);
    EXPECT_NEAR(p.s, 2.0, 1e-12 * r.plan.tol.scale);
    EXPECT_TRUE(tierOne(p).empty());
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcMidpointFallback), 1u);
    expectOracle(spec, r);
}

// Arc qui part de P = (0,0) presque tangent a l'axe miroir x (centre (xi, 1),
// r = sqrt(1 + xi^2)), sous D1 d'axe 0 deg ; D = sqrt(2 + ...), tol.point ~ 1,4e-9.
// L'arc descend sous l'axe de sa fleche r - 1 ~ xi^2 / 2 et le recoupe en (2 xi, 0).
// P est une extremite, exclue (A-3). Le point (2 xi, 0) n'est un point fixe que si
// la fleche depasse tol.point / 2 (un point est a tol de son reflet si et seulement
// si il est a tol / 2 de l'axe) ; sinon le pied de la tangence ne doit pas en creer un.
TEST(TEST_cgarchi_gothic_micro_prc, arc_nearly_tangent_to_a_mirror_axis_at_its_start)
{
    for (double xi : {0.0, 1e-5, 5e-5, 1e-4})
    {
        SCOPED_TRACE(::testing::Message() << "xi = " << xi);
        const double r = std::sqrt(1 + xi * xi);
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, 0.0);
        spec.points = {point("P", 0, 0, std::nullopt), point("E", xi + r, 1, 5.0)};
        spec.curves = {arcCurve("l", spec, 0, 1, Vector2d(xi, 1), true)};
        const PrcRun run1 = run(spec);
        ASSERT_TRUE(run1.prc.has_value()) << ReportToJson(run1.report);
        const std::vector<double> t1 = tierOne((*run1.prc)[0]);
        if (r - 1 <= 0.5 * run1.plan.tol.point)
        {
            EXPECT_TRUE(t1.empty());
            EXPECT_EQ((*run1.prc)[0].rule, PrcRule::Midpoint);
            EXPECT_EQ(warnings(run1.report, MicroarchWarning::PrcNearEndpoint), 0u);
        }
        else
        {
            // Second point commun avec l'axe : (2 xi, 0), a l'angle 2 atan(xi) du depart.
            // Le PRC y est garde, a u ~ 1e-4 : arc de profil « aiguille », signale (M13).
            ASSERT_EQ(t1.size(), 1u);
            EXPECT_NEAR(t1[0], r * 2 * std::atan(xi), 1e-9);
            EXPECT_EQ((*run1.prc)[0].rule, PrcRule::ReplicaFixedPoint);
            EXPECT_EQ(warnings(run1.report, MicroarchWarning::PrcNearEndpoint), 1u) << ReportToJson(run1.report);
        }
        expectOracle(spec, run1);
    }
}

// Segment (-2,e) -> (3,e) sous C4, D ~ 5, tol.point ~ 5e-9. Le centre est un point
// fixe a tol.point pres si |l(s) - R(l(s))| <= tol.point, soit e <= tol / (2 sin 45 deg).
// Sinon, les croisements avec R90(l) et R270(l), en s ~ 2, sont des croisements faux,
// selectionnables : aucun croisement ne tombe entre les deux categories.
TEST(TEST_cgarchi_gothic_micro_prc, rotation_centre_band_matches_the_oracle)
{
    // 4e-9 est entre tol / (2 sin 45 deg) ~ 3,5e-9 et tol ~ 5e-9.
    for (double e : {0.0, 1e-9, 4e-9, 2e-8})
    {
        SCOPED_TRACE(::testing::Message() << "e = " << e);
        MicroarchSpec spec = oneSegment(symmetry(4, std::nullopt), -2, e, 3, e, 10);
        const PrcRun r = run(spec);
        ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
        const PrcResult &p = (*r.prc)[0];
        const double band = r.plan.tol.point / (2 * std::sin(kPi / 4));
        const bool crossing = std::any_of(p.candidates.begin(), p.candidates.end(), [&](const PrcCandidate &c) {
            return c.tier == PrcRule::ReplicaCrossing && std::fabs(c.s - 2) <= 1e-6;
        });
        if (e <= band)
        {
            EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
            EXPECT_NEAR(p.s, 2.0, 1e-6);
            EXPECT_FALSE(crossing);
        }
        else
        {
            EXPECT_EQ(p.rule, PrcRule::Midpoint);
            EXPECT_TRUE(crossing);
            // Selection explicite du croisement : accroche, croisement faux.
            spec.curves[0].prc.u = 0.4;
            const PrcRun x = run(spec);
            ASSERT_TRUE(x.prc.has_value()) << ReportToJson(x.report);
            EXPECT_NEAR((*x.prc)[0].s, 2.0, 1e-6);
            EXPECT_EQ(warnings(x.report, MicroarchWarning::PrcOnFalseIntersection), 1u);
        }
        expectOracle(spec, r);
    }
}

// ---------------------------------------------------------------------------
//  Choix dans un palier
// ---------------------------------------------------------------------------

// Segment (-2,4) -> (6,4) sous D4, L = 8 : candidats s = 2 (axe x = 0) et s = 6
// (axe y = x), a egale distance du milieu. On garde s = 2, avec prc_tie.
TEST(TEST_cgarchi_gothic_micro_prc, tie_keeps_the_smallest_s)
{
    const MicroarchSpec spec = oneSegment(symmetry(4, 90.0), -2, 4, 6, 4, 10);
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const PrcResult &p = (*r.prc)[0];
    EXPECT_EQ(p.rule, PrcRule::ReplicaFixedPoint);
    EXPECT_NEAR(p.s, 2.0, 1e-12 * r.plan.tol.scale);
    const std::vector<double> t1 = tierOne(p);
    ASSERT_EQ(t1.size(), 2u);
    EXPECT_NEAR(t1[0], 2.0, 1e-12 * r.plan.tol.scale);
    EXPECT_NEAR(t1[1], 6.0, 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcTie), 1u);
    EXPECT_EQ(r.report.WarningCount(), 1u) << ReportToJson(r.report);
    expectOracle(spec, r);

    // Sans egalite : (-2,4) -> (7,4), L = 9, milieu 4,5 ; s = 6 (axe y = x) est le plus proche.
    const MicroarchSpec spec2 = oneSegment(symmetry(4, 90.0), -2, 4, 7, 4, 10);
    const PrcRun r2 = run(spec2);
    ASSERT_TRUE(r2.prc.has_value()) << ReportToJson(r2.report);
    EXPECT_NEAR((*r2.prc)[0].s, 6.0, 1e-12 * r2.plan.tol.scale);
    EXPECT_EQ(warnings(r2.report, MicroarchWarning::PrcTie), 0u);
    expectOracle(spec2, r2);
}

// Palier 2 : deux courbes de base sans symetrie (n = 1), (0,0) -> (4,0) et
// (2,-1) -> (2,3). Aucun point fixe ; elles se coupent en (2,0).
TEST(TEST_cgarchi_gothic_micro_prc, other_basic_curve_tier)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(1, std::nullopt);
    spec.points = {point("A", 0, 0, std::nullopt), point("B", 4, 0, 6.0),
                   point("C", 2, -1, std::nullopt), point("E", 2, 3, 6.0)};
    spec.curves = {segmentCurve("l1", spec, 0, 1), segmentCurve("l2", spec, 2, 3)};
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    const double D = r.plan.tol.scale;
    EXPECT_EQ((*r.prc)[0].rule, PrcRule::OtherBasicCurve);
    EXPECT_NEAR((*r.prc)[0].s, 2.0, 1e-12 * D);
    EXPECT_EQ((*r.prc)[1].rule, PrcRule::OtherBasicCurve);
    EXPECT_NEAR((*r.prc)[1].s, 1.0, 1e-12 * D);
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcFromOtherBasicCurve), 2u);
    expectOracle(spec, r);
}

// ---------------------------------------------------------------------------
//  Selection explicite (M5)
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_prc, explicit_selection_on_the_pinwheel)
{
    MicroarchSpec spec = loadFile(kC4Pinwheel);

    // u = 0,4 : le croisement faux s = 2, avec prc_on_false_intersection.
    spec.curves[0].prc.u = 0.4;
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    EXPECT_EQ((*r.prc)[0].rule, PrcRule::Explicit);
    EXPECT_NEAR((*r.prc)[0].s, 2.0, 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(warnings(r.report, MicroarchWarning::PrcOnFalseIntersection), 1u);
    EXPECT_EQ(r.report.WarningCount(), 1u) << ReportToJson(r.report);

    // u = 0,4 + 0,9 prc_snap : accroche ; u = 0,4 + 1,1 prc_snap : 0,5 est plus loin encore.
    spec.curves[0].prc.u = 0.4 + 0.9e-3;
    const PrcRun snapped = run(spec);
    ASSERT_TRUE(snapped.prc.has_value()) << ReportToJson(snapped.report);
    EXPECT_NEAR((*snapped.prc)[0].s, 2.0, 1e-12 * snapped.plan.tol.scale);

    // u = 0,7 : rien a moins de prc_snap ; le message liste 0,4 ; 0,5 ; 0,8.
    spec.curves[0].prc.u = 0.7;
    MicroarchReport report;
    const std::optional<DevelopedPlan> plan = DevelopPlan(spec, report);
    ASSERT_TRUE(plan.has_value());
    EXPECT_FALSE(ComputePrcs(spec, *plan, report).has_value());
    ASSERT_EQ(report.ErrorCount(), 1u) << ReportToJson(report);
    ASSERT_EQ(report.entries.size(), 1u);
    const MicroarchDiagnostic &d = report.entries[0];
    EXPECT_TRUE(d.error);
    EXPECT_EQ(d.code, static_cast<int>(MicroarchError::PrcSelectionNoCandidate));
    EXPECT_EQ(d.path, "curves[0].prc.u");
    EXPECT_NE(d.message.find("0.4, 0.5, 0.8"), std::string::npos) << d.message;
}

TEST(TEST_cgarchi_gothic_micro_prc, explicit_selection_of_a_fixed_point_or_the_midpoint)
{
    // D4 segment, u = 3/7 : le point fixe, sans avertissement.
    MicroarchSpec spec = loadFile(kD4Segment);
    spec.curves[0].prc.u = 3.0 / 7;
    const PrcRun r = run(spec);
    ASSERT_TRUE(r.prc.has_value()) << ReportToJson(r.report);
    EXPECT_EQ((*r.prc)[0].rule, PrcRule::Explicit);
    EXPECT_NEAR((*r.prc)[0].s, 15.0 / 7, 1e-12 * r.plan.tol.scale);
    EXPECT_EQ(r.report.WarningCount(), 0u) << ReportToJson(r.report);

    // u = 0,5 : le milieu, qui n'est pas une intersection.
    spec.curves[0].prc.u = 0.5;
    const PrcRun m = run(spec);
    ASSERT_TRUE(m.prc.has_value()) << ReportToJson(m.report);
    EXPECT_NEAR((*m.prc)[0].s, 2.5, 1e-12 * m.plan.tol.scale);
    EXPECT_EQ(warnings(m.report, MicroarchWarning::PrcMidpointFallback), 1u);
}
