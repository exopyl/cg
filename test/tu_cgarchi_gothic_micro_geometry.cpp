#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : geometrie du plan, groupe de symetrie,
//  developpement du plan (etape 1)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §8 J1-a, J1-c).
// Tolerance : 1e-12 * D pour une valeur analytique, D etant l'echelle du cas.

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double dist(const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }

    // Arc de reference : centre (5,0), r = 5, de (1,3) a (8,4), sens horaire.
    PlanCurve referenceArc() { return MakeArc(Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), false); }
    // Echelle du cas : diagonale de la boite de (1,3), (8,4), (5,0).
    const double kArcScale = std::sqrt(65.0);

    SymmetrySpec d4(double axisDeg = 90.0)
    {
        SymmetrySpec s;
        s.center = Vector2d(0, 0);
        s.rotation_order = 4;
        s.reflection_axis_deg = axisDeg;
        return s;
    }
}

// ---------------------------------------------------------------------------
//  J1-a : courbes du plan
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_geometry, arc_reference_values)
{
    const PlanCurve c = referenceArc();
    const double tol = 1e-12 * kArcScale;
    const PlanArc &arc = std::get<PlanArc>(c);
    EXPECT_NEAR(arc.radius, 5.0, tol);
    EXPECT_NEAR(arc.sweep, -kPi / 2, 1e-12);
    EXPECT_NEAR(Length(c), 5 * kPi / 2, tol);
    EXPECT_LE(dist(StartPoint(c), Vector2d(1, 3)), tol);
    EXPECT_LE(dist(EndPoint(c), Vector2d(8, 4)), tol);

    // Le point (5,5) est a 53,13 deg (acos 0,6) du depart, en sens horaire.
    const double s55 = 5 * std::acos(0.6);   // 4,636476090008...
    EXPECT_LE(dist(PointAt(c, s55), Vector2d(5, 5)), tol);
    const auto s = ParameterOf(c, Vector2d(5, 5), 1e-9 * kArcScale);
    ASSERT_TRUE(s.has_value());
    EXPECT_NEAR(*s, 4.636476090008061, tol);

    // Tangente au depart : perpendiculaire au rayon (1,3) - (5,0) = (-4,3), et
    // orientee vers la droite et le haut en sens horaire : (0,6 ; 0,8).
    const Vector2d t0 = TangentAt(c, 0.0);
    EXPECT_NEAR(t0.x * -4 + t0.y * 3, 0.0, 1e-12);
    EXPECT_NEAR(t0.x, 0.6, 1e-12);
    EXPECT_NEAR(t0.y, 0.8, 1e-12);
}

TEST(TEST_cgarchi_gothic_micro_geometry, arc_ccw_takes_the_long_way)
{
    const PlanCurve c = MakeArc(Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), true);
    EXPECT_NEAR(std::get<PlanArc>(c).sweep, 3 * kPi / 2, 1e-12);
    // Le point (0,0) du cercle est sur l'arc direct, pas sur l'arc horaire.
    EXPECT_TRUE(ParameterOf(c, Vector2d(0, 0), 1e-9 * kArcScale).has_value());
    EXPECT_FALSE(ParameterOf(referenceArc(), Vector2d(0, 0), 1e-9 * kArcScale).has_value());
}

TEST(TEST_cgarchi_gothic_micro_geometry, segment_reference_values)
{
    const PlanCurve c = PlanSegment{Vector2d(4, 1), Vector2d(1, 5)};
    const double D = std::sqrt(41.0);
    EXPECT_NEAR(Length(c), 5.0, 1e-12 * D);
    EXPECT_LE(dist(PointAt(c, 1.0), Vector2d(3.4, 1.8)), 1e-12 * D);
    const Vector2d t = TangentAt(c, 2.0);
    EXPECT_NEAR(t.x, -0.6, 1e-12);
    EXPECT_NEAR(t.y, 0.8, 1e-12);
    const auto s = ParameterOf(c, Vector2d(3.4, 1.8), 1e-9 * D);
    ASSERT_TRUE(s.has_value());
    EXPECT_NEAR(*s, 1.0, 1e-12 * D);
}

TEST(TEST_cgarchi_gothic_micro_geometry, parameter_of_rejects_points_off_the_curve)
{
    const double tol = 1e-9 * kArcScale;
    // Sur le cercle porteur, hors de l'intervalle angulaire : (0,0) a 180 deg.
    EXPECT_FALSE(ParameterOf(referenceArc(), Vector2d(0, 0), tol).has_value());
    // Hors du cercle : (5,6) est a distance 6 du centre.
    EXPECT_FALSE(ParameterOf(referenceArc(), Vector2d(5, 6), tol).has_value());
    // Segment : sur la droite porteuse au-dela de l'extremite, puis hors de la droite.
    const PlanCurve seg = PlanSegment{Vector2d(4, 1), Vector2d(1, 5)};
    EXPECT_FALSE(ParameterOf(seg, Vector2d(0.4, 6.8), tol).has_value());
    EXPECT_FALSE(ParameterOf(seg, Vector2d(3.4, 1.9), tol).has_value());
}

// Un point juste avant le depart (dans le sens de parcours) donne un ecart
// angulaire voisin de 2*pi : il est rabattu sur s = 0 exactement.
TEST(TEST_cgarchi_gothic_micro_geometry, parameter_of_wraps_at_the_start)
{
    const double tol = 1e-9 * kArcScale;
    for (bool ccw : {false, true})
    {
        const PlanCurve c = MakeArc(Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), ccw);
        const PlanArc &arc = std::get<PlanArc>(c);
        const double backward = arc.start_angle - (ccw ? 1 : -1) * 1e-14;
        const double forward  = arc.start_angle + (ccw ? 1 : -1) * 1e-14;
        const Vector2d before(5 + 5 * std::cos(backward), 5 * std::sin(backward));
        const Vector2d after (5 + 5 * std::cos(forward),  5 * std::sin(forward));
        const auto sb = ParameterOf(c, before, tol);
        const auto sa = ParameterOf(c, after, tol);
        ASSERT_TRUE(sb.has_value()) << "ccw=" << ccw;
        ASSERT_TRUE(sa.has_value()) << "ccw=" << ccw;
        EXPECT_EQ(*sb, 0.0) << "ccw=" << ccw;
        EXPECT_LE(*sa, 1e-12 * kArcScale) << "ccw=" << ccw;
        // Le depart exact.
        const auto s0 = ParameterOf(c, StartPoint(c), tol);
        ASSERT_TRUE(s0.has_value());
        EXPECT_LE(*s0, 1e-12 * kArcScale);
    }
}

// ---------------------------------------------------------------------------
//  J1-a : groupe de symetrie
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_geometry, group_elements_and_mirror_axes)
{
    const SymmetrySpec sym = d4();
    const std::vector<SymmetryElement> elems = Elements(sym);
    ASSERT_EQ(elems.size(), 8u);
    EXPECT_EQ(elems[0].k, 0);
    EXPECT_FALSE(elems[0].reflected);
    std::set<std::pair<int, bool>> distinct;
    for (SymmetryElement g : elems)
        distinct.insert({g.k, g.reflected});
    EXPECT_EQ(distinct.size(), 8u);

    // Axes : {90, 135, 180, 225} deg modulo 180, soit {90, 135, 0, 45}.
    const std::vector<Axis> axes = MirrorAxes(sym);
    ASSERT_EQ(axes.size(), 4u);
    const double expected[] = {90.0, 135.0, 0.0, 45.0};
    for (std::size_t k = 0; k < axes.size(); ++k)
    {
        EXPECT_NEAR(std::hypot(axes[k].direction.x, axes[k].direction.y), 1.0, 1e-15);
        double deg = std::atan2(axes[k].direction.y, axes[k].direction.x) * 180.0 / kPi;
        deg = std::fmod(deg + 360.0, 180.0);
        if (deg > 180.0 - 1e-9)
            deg -= 180.0;
        EXPECT_NEAR(deg, expected[k], 1e-12) << "axe " << k;
    }

    SymmetrySpec c1;
    EXPECT_EQ(Elements(c1).size(), 1u);
    EXPECT_TRUE(MirrorAxes(c1).empty());

    SymmetrySpec c4 = sym;
    c4.reflection_axis_deg.reset();
    EXPECT_EQ(Elements(c4).size(), 4u);
}

TEST(TEST_cgarchi_gothic_micro_geometry, group_actions_on_points)
{
    const SymmetrySpec sym = d4();
    const Vector2d p(4, 1);
    // R^1 : (4,1) -> (-1,4). Reflexion R^3 Q, axe y = x : (4,1) -> (1,4).
    EXPECT_LE(dist(Apply(sym, SymmetryElement{1, false}, p), Vector2d(-1, 4)), 1e-12 * 4.2);
    EXPECT_LE(dist(Apply(sym, SymmetryElement{3, true}, p), Vector2d(1, 4)), 1e-12 * 4.2);
    // R^0 Q, axe x = 0 : (4,1) -> (-4,1).
    EXPECT_LE(dist(Apply(sym, SymmetryElement{0, true}, p), Vector2d(-4, 1)), 1e-12 * 4.2);

    // R^n est l'identite ; R^1 applique n fois aussi ; toute reflexion est une involution.
    EXPECT_LE(dist(Apply(sym, SymmetryElement{4, false}, p), p), 1e-12 * 4.2);
    Vector2d q = p;
    for (int i = 0; i < 4; ++i)
        q = Apply(sym, SymmetryElement{1, false}, q);
    EXPECT_LE(dist(q, p), 1e-12 * 4.2);
    for (int k = 0; k < 4; ++k)
    {
        const SymmetryElement g{k, true};
        EXPECT_LE(dist(Apply(sym, g, Apply(sym, g, p)), p), 1e-12 * 4.2) << "k=" << k;
    }

    // Centre hors de l'origine : le centre est fixe.
    SymmetrySpec off = sym;
    off.center = Vector2d(2, -3);
    for (SymmetryElement g : Elements(off))
        EXPECT_LE(dist(Apply(off, g, off.center), off.center), 1e-12 * 3.6);
}

// Le sens de parcours est conserve : Apply(g, c)(s) = g(c(s)). Echoue si une
// reflexion n'inverse pas le balayage d'un arc.
TEST(TEST_cgarchi_gothic_micro_geometry, apply_preserves_the_direction)
{
    const double D = kArcScale;
    for (const SymmetrySpec &sym : {d4(), d4(30.0)})
        for (const PlanCurve &c : {referenceArc(), PlanCurve(PlanSegment{Vector2d(4, 1), Vector2d(1, 5)})})
            for (SymmetryElement g : Elements(sym))
            {
                const PlanCurve img = Apply(sym, g, c);
                const double L = Length(c);
                EXPECT_NEAR(Length(img), L, 1e-12 * D);
                for (double s : {0.0, L / 3, L})
                    EXPECT_LE(dist(PointAt(img, s), Apply(sym, g, PointAt(c, s))), 1e-12 * D)
                        << "k=" << g.k << " r=" << g.reflected << " s=" << s;
            }
}

// ---------------------------------------------------------------------------
//  J1-a : rapport
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_geometry, diagnostic_names_are_distinct)
{
    std::set<std::string> names;
    for (std::size_t k = 0; k < kMicroarchErrorCount; ++k)
        names.insert(MicroarchErrorName(static_cast<MicroarchError>(k)));
    for (std::size_t k = 0; k < kMicroarchWarningCount; ++k)
        names.insert(MicroarchWarningName(static_cast<MicroarchWarning>(k)));
    EXPECT_EQ(names.size(), kMicroarchErrorCount + kMicroarchWarningCount);
    EXPECT_EQ(names.count("unknown"), 0u);
    EXPECT_STREQ(MicroarchErrorName(MicroarchError::SelfReversedReplica), "self_reversed_replica");
    EXPECT_STREQ(MicroarchWarningName(MicroarchWarning::EndpointHeightConflict), "endpoint_height_conflict");
}

TEST(TEST_cgarchi_gothic_micro_geometry, report_counts_everything_and_caps_entries)
{
    MicroarchReport a;
    for (std::size_t i = 0; i < kMicroarchReportMaxEntries + 6; ++i)
        a.Record(MicroarchError::JsonValue, "p", "m");
    a.Record(MicroarchWarning::PrcTie, "q", "m");
    EXPECT_EQ(a.Count(MicroarchError::JsonValue), kMicroarchReportMaxEntries + 6);
    EXPECT_EQ(a.entries.size(), kMicroarchReportMaxEntries);
    EXPECT_TRUE(a.HasErrors());
    EXPECT_TRUE(a.HasWarnings());

    MicroarchReport b;
    b.Record(MicroarchWarning::FieldIgnored, "r", "m");
    MicroarchReport c;
    c.Merge(b);
    c.Merge(a);
    EXPECT_EQ(c.ErrorCount(), kMicroarchReportMaxEntries + 6);
    EXPECT_EQ(c.WarningCount(), 2u);
    EXPECT_EQ(c.entries.size(), kMicroarchReportMaxEntries);
    EXPECT_EQ(c.entries.front().path, "r");
}

// ---------------------------------------------------------------------------
//  J1-c : developpement du plan
// ---------------------------------------------------------------------------

namespace
{
    PointSpec groundPoint(const std::string &id, double x, double y)
    {
        PointSpec p;
        p.id = id;
        p.xy = Vector2d(x, y);
        p.ground = true;
        return p;
    }

    PointSpec raisedPoint(const std::string &id, double x, double y, double h)
    {
        PointSpec p;
        p.id = id;
        p.xy = Vector2d(x, y);
        p.height = h;
        return p;
    }

    BasicCurveSpec segment(const MicroarchSpec &spec, const std::string &id, int a, int b)
    {
        BasicCurveSpec c;
        c.id = id;
        c.start_point = a;
        c.end_point = b;
        c.plan = PlanSegment{spec.points[static_cast<std::size_t>(a)].xy, spec.points[static_cast<std::size_t>(b)].xy};
        return c;
    }

    BasicCurveSpec arc(const MicroarchSpec &spec, const std::string &id, int a, int b, Vector2d center, bool ccw)
    {
        BasicCurveSpec c;
        c.id = id;
        c.start_point = a;
        c.end_point = b;
        c.plan = MakeArc(spec.points[static_cast<std::size_t>(a)].xy, spec.points[static_cast<std::size_t>(b)].xy,
                         center, ccw);
        return c;
    }

    SymmetrySpec c4()
    {
        SymmetrySpec s;
        s.center = Vector2d(0, 0);
        s.rotation_order = 4;
        return s;
    }

    // Reference D4 segment (plan d'execution §9.1).
    MicroarchSpec d4Segment()
    {
        MicroarchSpec s;
        s.symmetry = d4();
        s.points = {groundPoint("A", 4, 1), raisedPoint("B", 1, 5, 10)};
        s.curves = {segment(s, "l1", 0, 1)};
        return s;
    }

    // Moulinet C4 (§9.2).
    MicroarchSpec c4Pinwheel()
    {
        MicroarchSpec s;
        s.symmetry = c4();
        s.points = {groundPoint("A", 3, -1), raisedPoint("B", -1, 2, 10)};
        s.curves = {segment(s, "l1", 0, 1)};
        return s;
    }

    // Etoile : segment (4,0) -> (0,0) sous D4 (§9.3).
    MicroarchSpec d4Star()
    {
        MicroarchSpec s;
        s.symmetry = d4();
        s.points = {groundPoint("A", 4, 0), raisedPoint("O", 0, 0, 8)};
        s.curves = {segment(s, "l1", 0, 1)};
        return s;
    }

    std::size_t countEntries(const MicroarchReport &r, bool error, int code, const std::string &path)
    {
        std::size_t n = 0;
        for (const MicroarchDiagnostic &d : r.entries)
            if (d.error == error && d.code == code && d.path == path)
                ++n;
        return n;
    }

    // Toute image d'une courbe de base est exactement une des repliques, et les
    // repliques sont deux a deux distinctes. Comparaison par extremites et milieu,
    // independante du critere du developpement.
    void expectReplicasCoverImages(const MicroarchSpec &spec, const DevelopedPlan &plan)
    {
        const double tol = plan.tol.point;
        auto same = [&](const PlanCurve &a, const PlanCurve &b) {
            return dist(StartPoint(a), StartPoint(b)) <= tol && dist(EndPoint(a), EndPoint(b)) <= tol
                && dist(PointAt(a, Length(a) / 2), PointAt(b, Length(b) / 2)) <= tol;
        };
        for (std::size_t i = 0; i < spec.curves.size(); ++i)
            for (SymmetryElement g : Elements(spec.symmetry))
            {
                const PlanCurve img = Apply(spec.symmetry, g, spec.curves[i].plan);
                const auto n = std::count_if(plan.curves.begin(), plan.curves.end(), [&](const PlanReplica &r) {
                    return r.basic == static_cast<int>(i) && same(r.plan, img);
                });
                EXPECT_EQ(n, 1) << "courbe " << i << " element k=" << g.k << " r=" << g.reflected;
            }
        for (std::size_t a = 0; a < plan.curves.size(); ++a)
            for (std::size_t b = a + 1; b < plan.curves.size(); ++b)
                EXPECT_FALSE(same(plan.curves[a].plan, plan.curves[b].plan)) << a << " / " << b;
    }
}

TEST(TEST_cgarchi_gothic_micro_geometry, replica_counts_of_the_references)
{
    struct Case { const char *name; MicroarchSpec spec; std::size_t replicas; };
    const Case cases[] = {
        {"D4 segment", d4Segment(), 8},
        {"etoile", d4Star(), 4},       // la reflexion d'axe x laisse le segment invariant, meme sens
        {"moulinet", c4Pinwheel(), 4},
    };
    for (const Case &c : cases)
    {
        MicroarchReport report;
        const auto plan = DevelopPlan(c.spec, report);
        ASSERT_TRUE(plan.has_value()) << c.name;
        EXPECT_EQ(report.ErrorCount() + report.WarningCount(), 0u) << c.name;
        EXPECT_EQ(plan->curves.size(), c.replicas) << c.name;
        expectReplicasCoverImages(c.spec, *plan);
        EXPECT_TRUE(Validate(c.spec, report)) << c.name;
    }

    // Etoile : les quatre repliques partent de (+-4,0), (0,+-4) et finissent au centre.
    MicroarchReport report;
    const auto star = DevelopPlan(d4Star(), report);
    ASSERT_TRUE(star.has_value());
    std::set<std::pair<long, long>> starts;
    for (const PlanReplica &r : star->curves)
    {
        EXPECT_LE(dist(EndPoint(r.plan), Vector2d(0, 0)), 1e-12 * 4);
        const Vector2d s = StartPoint(r.plan);
        starts.insert({std::lround(s.x), std::lround(s.y)});
        EXPECT_FALSE(r.element.reflected);   // l'ordre (courbe, element) retient d'abord les rotations
    }
    EXPECT_EQ(starts, (std::set<std::pair<long, long>>{{4, 0}, {0, 4}, {-4, 0}, {0, -4}}));
}

// Arc centre sur le centre de symetrie, de 10 a 80 deg : la reflexion d'axe
// y = x l'envoie sur lui-meme en sens inverse. Ses deux extremites s'echangent :
// l'avertissement endpoint_height_conflict en est la consequence.
TEST(TEST_cgarchi_gothic_micro_geometry, self_reversed_replica_is_an_error)
{
    MicroarchSpec s;
    s.symmetry = d4();
    const double a = 10 * kPi / 180, b = 80 * kPi / 180;
    s.points = {groundPoint("P", 5 * std::cos(a), 5 * std::sin(a)), raisedPoint("Q", 5 * std::cos(b), 5 * std::sin(b), 5)};
    s.curves = {arc(s, "l1", 0, 1, Vector2d(0, 0), true)};
    MicroarchReport report;
    EXPECT_FALSE(DevelopPlan(s, report).has_value());
    EXPECT_EQ(report.ErrorCount(), 1u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::SelfReversedReplica), "curves[0]"), 1u);
    EXPECT_EQ(report.WarningCount(), 1u);
    EXPECT_EQ(report.Count(MicroarchWarning::EndpointHeightConflict), 1u);

    // Meme arc sous C4 : aucune reflexion, aucun diagnostic.
    s.symmetry = c4();
    MicroarchReport ok;
    EXPECT_TRUE(Validate(s, ok));
    EXPECT_EQ(ok.ErrorCount() + ok.WarningCount(), 0u);
}

// La seconde courbe de base est la rotation de 90 deg de la premiere, dans un
// sens ou dans l'autre.
TEST(TEST_cgarchi_gothic_micro_geometry, redundant_basic_curve_is_an_error)
{
    for (bool reversed : {false, true})
    {
        MicroarchSpec s = c4Pinwheel();
        // R90 : (3,-1) -> (1,3), (-1,2) -> (-2,-1) ; memes hauteurs que leurs antecedents.
        s.points.push_back(groundPoint("A1", 1, 3));
        s.points.push_back(raisedPoint("B1", -2, -1, 10));
        s.curves.push_back(reversed ? segment(s, "l2", 3, 2) : segment(s, "l2", 2, 3));
        MicroarchReport report;
        EXPECT_FALSE(DevelopPlan(s, report).has_value()) << reversed;
        EXPECT_EQ(report.ErrorCount() + report.WarningCount(), 1u) << reversed;
        EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::RedundantBasicCurve), "curves[1]"), 1u)
            << reversed;
    }

    // Une seconde courbe qui n'est l'image de rien : acceptee.
    MicroarchSpec s = c4Pinwheel();
    s.points.push_back(groundPoint("C", 6, 0));
    s.points.push_back(raisedPoint("E", 6, 3, 7));
    s.curves.push_back(segment(s, "l2", 2, 3));
    MicroarchReport report;
    const auto plan = DevelopPlan(s, report);
    ASSERT_TRUE(plan.has_value()) << report.entries.size();
    EXPECT_EQ(plan->curves.size(), 8u);
}

// Losange : A = (4,0) au sol, B = (0,4) a hauteur 6. La rotation de 90 deg envoie A sur B.
TEST(TEST_cgarchi_gothic_micro_geometry, endpoint_height_conflict_is_a_warning)
{
    MicroarchSpec s;
    s.symmetry = c4();
    s.points = {groundPoint("A", 4, 0), raisedPoint("B", 0, 4, 6)};
    s.curves = {segment(s, "l1", 0, 1)};
    MicroarchReport report;
    const auto plan = DevelopPlan(s, report);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->curves.size(), 4u);
    EXPECT_EQ(report.ErrorCount() + report.WarningCount(), 1u);
    EXPECT_EQ(countEntries(report, false, static_cast<int>(MicroarchWarning::EndpointHeightConflict), "points[1]"), 1u);

    // Sous D4, la reflexion d'axe y = x echange A et B : le segment est aussi
    // envoye sur lui-meme en sens inverse.
    s.symmetry = d4();
    MicroarchReport d4report;
    EXPECT_FALSE(DevelopPlan(s, d4report).has_value());
    EXPECT_EQ(d4report.ErrorCount(), 1u);
    EXPECT_EQ(countEntries(d4report, true, static_cast<int>(MicroarchError::SelfReversedReplica), "curves[0]"), 1u);
    EXPECT_EQ(countEntries(d4report, false, static_cast<int>(MicroarchWarning::EndpointHeightConflict), "points[1]"), 1u);

    // Memes hauteurs : pas de conflit.
    s.symmetry = c4();
    s.points[1] = groundPoint("B", 0, 4);
    MicroarchReport same;
    EXPECT_TRUE(Validate(s, same));
    EXPECT_EQ(same.WarningCount(), 0u);
}

// Specification construite en C++ : les valeurs que le lecteur JSON aurait
// refusees sont signalees par la validation structurelle.
TEST(TEST_cgarchi_gothic_micro_geometry, cpp_spec_structure_rules)
{
    {
        MicroarchSpec s = d4Segment();
        s.curves[0].start_point = 5;
        MicroarchReport r;
        EXPECT_FALSE(ValidateStructure(s, r));
        EXPECT_EQ(r.ErrorCount(), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::UnknownReference), "curves[0].start"), 1u);
    }
    {
        MicroarchSpec s = d4Segment();
        s.points[0].xy = Vector2d(std::nan(""), 1);
        MicroarchReport r;
        EXPECT_FALSE(ValidateStructure(s, r));
        EXPECT_EQ(r.ErrorCount(), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::JsonValue), "points[0].xy"), 1u);
    }
    {
        MicroarchSpec s = d4Segment();
        s.curves[0].plan = PlanSegment{Vector2d(4, 1), Vector2d(1, 6)};   // ne finit pas en B
        MicroarchReport r;
        EXPECT_FALSE(ValidateStructure(s, r));
        EXPECT_EQ(r.ErrorCount(), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::SegmentEndpointMismatch), "curves[0].end"), 1u);
        s.curves[0].plan = PlanSegment{Vector2d(4, 2), Vector2d(1, 5)};   // ne part pas de A
        MicroarchReport r2;
        EXPECT_FALSE(ValidateStructure(s, r2));
        EXPECT_EQ(r2.ErrorCount(), 1u);
        EXPECT_EQ(countEntries(r2, true, static_cast<int>(MicroarchError::SegmentEndpointMismatch), "curves[0].start"), 1u);
    }
    {
        MicroarchSpec s = d4Segment();
        s.symmetry.rotation_order = 0;
        s.tolerances.height = 0.5;
        s.points.clear();
        s.curves.clear();
        MicroarchReport r;
        EXPECT_FALSE(ValidateStructure(s, r));
        EXPECT_EQ(r.ErrorCount(), 4u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::JsonValue), "symmetry.rotation_order"), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::JsonValue), "tolerances.height"), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::JsonValue), "points"), 1u);
        EXPECT_EQ(countEntries(r, true, static_cast<int>(MicroarchError::JsonValue), "curves"), 1u);
    }
}

// L'avertissement endpoint_height_conflict n'accompagne l'inversion que si les
// deux extremites echangees ont des hauteurs differentes.
TEST(TEST_cgarchi_gothic_micro_geometry, self_reversed_with_equal_heights_has_no_warning)
{
    MicroarchSpec s;
    s.symmetry = d4();
    s.points = {groundPoint("A", 4, 0), groundPoint("B", 0, 4)};
    s.curves = {segment(s, "l1", 0, 1)};
    MicroarchReport report;
    EXPECT_FALSE(DevelopPlan(s, report).has_value());
    EXPECT_EQ(report.ErrorCount(), 1u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::SelfReversedReplica), "curves[0]"), 1u);
    EXPECT_EQ(report.WarningCount(), 0u);
}

// Centre de symetrie hors de l'origine : l'image d'un arc conserve le sens de
// parcours et reste un arc du cercle image.
TEST(TEST_cgarchi_gothic_micro_geometry, apply_arc_with_an_offset_centre)
{
    SymmetrySpec sym = d4(30.0);
    sym.center = Vector2d(2, -3);
    const PlanCurve c = referenceArc();
    const double D = 12.0;   // boite de l'arc et du centre

    // Valeurs a la main, v = p - (2,-3). R^1 : (5,0) -> (-1,0), depart (1,3) -> (-4,-4).
    // R^2 : (5,0) -> (-1,-6). R^0 Q, axe a 30 deg (2 psi = 60 deg) : v = (3,3) ->
    // (1,5 + 1,5 sqrt 3 ; 1,5 sqrt 3 - 1,5), soit (3,5 + 1,5 sqrt 3 ; -4,5 + 1,5 sqrt 3).
    const PlanCurve r1 = Apply(sym, SymmetryElement{1, false}, c);
    EXPECT_LE(dist(std::get<PlanArc>(r1).center, Vector2d(-1, 0)), 1e-12 * D);
    EXPECT_LE(dist(StartPoint(r1), Vector2d(-4, -4)), 1e-12 * D);
    EXPECT_LE(dist(std::get<PlanArc>(Apply(sym, SymmetryElement{2, false}, c)).center, Vector2d(-1, -6)), 1e-12 * D);
    const double r3 = std::sqrt(3.0);
    EXPECT_LE(dist(std::get<PlanArc>(Apply(sym, SymmetryElement{0, true}, c)).center,
                   Vector2d(3.5 + 1.5 * r3, -4.5 + 1.5 * r3)), 1e-12 * D);

    for (SymmetryElement g : Elements(sym))
    {
        const PlanCurve img = Apply(sym, g, c);
        const PlanArc &a = std::get<PlanArc>(img);
        EXPECT_LE(dist(a.center, Apply(sym, g, Vector2d(5, 0))), 1e-12 * D);
        EXPECT_NEAR(a.radius, 5.0, 1e-12 * D);
        for (double s : {0.0, Length(c) / 3, Length(c)})
            EXPECT_LE(dist(PointAt(img, s), Apply(sym, g, PointAt(c, s))), 1e-12 * D)
                << "k=" << g.k << " r=" << g.reflected << " s=" << s;
    }
}

// n = 1 avec reflexion d'axe x = 0 : un segment porte par l'axe est invariant
// (1 replique), un segment hors de l'axe en a 2.
TEST(TEST_cgarchi_gothic_micro_geometry, single_reflection_group)
{
    MicroarchSpec s;
    s.symmetry.center = Vector2d(0, 0);
    s.symmetry.rotation_order = 1;
    s.symmetry.reflection_axis_deg = 90.0;
    EXPECT_EQ(Elements(s.symmetry).size(), 2u);
    ASSERT_EQ(MirrorAxes(s.symmetry).size(), 1u);
    s.points = {groundPoint("A", 0, 1), raisedPoint("B", 0, 3, 4), groundPoint("C", 2, 0), raisedPoint("E", 3, 1, 5)};
    s.curves = {segment(s, "axis", 0, 1), segment(s, "free", 2, 3)};
    MicroarchReport report;
    const auto plan = DevelopPlan(s, report);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(report.ErrorCount() + report.WarningCount(), 0u);
    EXPECT_EQ(plan->curves.size(), 3u);
    expectReplicasCoverImages(s, *plan);
}

// Les coordonnees (+-1e308, 0) donnent une echelle infinie : degenerate_scale,
// et pas de tolerance infinie qui accepterait tout.
TEST(TEST_cgarchi_gothic_micro_geometry, infinite_scale_is_degenerate)
{
    MicroarchSpec s;
    s.symmetry = c4();
    s.points = {groundPoint("A", -1e308, 0), raisedPoint("B", 1e308, 0, 3)};
    s.curves = {segment(s, "l1", 0, 1)};
    EXPECT_TRUE(std::isinf(Scale(s)));
    MicroarchReport report;
    EXPECT_FALSE(ValidateStructure(s, report));
    EXPECT_EQ(report.ErrorCount(), 1u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::DegenerateScale), "points"), 1u);
}

// Un id qui n'est pas de l'UTF-8 valide est refuse ; l'ecriture JSON ne leve pas.
TEST(TEST_cgarchi_gothic_micro_geometry, invalid_utf8_ids_are_rejected)
{
    MicroarchSpec s = d4Segment();
    s.points[0].id = std::string("A\xFF", 2);
    s.curves[0].id = std::string("l\xC0\x81", 3);   // forme trop longue
    MicroarchReport report;
    EXPECT_FALSE(ValidateStructure(s, report));
    EXPECT_EQ(report.ErrorCount(), 2u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::JsonValue), "points[0].id"), 1u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::JsonValue), "curves[0].id"), 1u);

    // L'ecriture remplace les octets invalides au lieu de lever.
    std::string text;
    EXPECT_NO_THROW(text = SpecToJson(s));
    EXPECT_NE(text.find("\xEF\xBF\xBD"), std::string::npos);   // U+FFFD

    struct Case { std::string id; bool valid; };
    const Case cases[] = {
        {"caf\xC3\xA9 \xF0\x9F\x8F\x9B", true},   // 2 et 4 octets
        {"\xED\xA0\x80", false},                  // substitut
        {"\xF4\x90\x80\x80", false},              // au-dela de U+10FFFF
        {"\xE2\x82", false},                      // tronque
        {"\xE0\x9F\xBF", false},                  // forme trop longue sur 3 octets
    };
    for (const Case &c : cases)
    {
        MicroarchSpec t = d4Segment();
        t.points[1].id = c.id;
        MicroarchReport r;
        EXPECT_EQ(ValidateStructure(t, r), c.valid) << c.id.size();
    }
}

// Elements et MirrorAxes exigent un ordre valide : hors de [1, 64], ils rendent
// un resultat vide. Apply borne l'ordre.
TEST(TEST_cgarchi_gothic_micro_geometry, elements_require_a_valid_order)
{
    SymmetrySpec s = d4();
    s.rotation_order = SpecLimits::max_rotation_order;
    EXPECT_EQ(Elements(s).size(), static_cast<std::size_t>(2 * SpecLimits::max_rotation_order));
    EXPECT_EQ(MirrorAxes(s).size(), static_cast<std::size_t>(SpecLimits::max_rotation_order));
    for (int n : {0, -5, 65, 72, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()})
    {
        s.rotation_order = n;
        EXPECT_TRUE(Elements(s).empty()) << n;
        EXPECT_TRUE(MirrorAxes(s).empty()) << n;
    }
    // Apply : n = 72 est ramene a 64.
    SymmetrySpec c64 = d4();
    c64.rotation_order = SpecLimits::max_rotation_order;
    s.rotation_order = 72;
    const Vector2d p(4, 1);
    EXPECT_LE(dist(Apply(s, SymmetryElement{1, false}, p), Apply(c64, SymmetryElement{1, false}, p)), 0.0);
}

// Plafonds de taille d'une specification construite en C++.
TEST(TEST_cgarchi_gothic_micro_geometry, size_caps_are_enforced)
{
    MicroarchSpec s = d4Segment();
    for (std::size_t i = s.points.size(); i <= SpecLimits::max_points; ++i)
        s.points.push_back(groundPoint("P" + std::to_string(i), 0.001 * static_cast<double>(i), 7));
    MicroarchReport report;
    EXPECT_FALSE(ValidateStructure(s, report));
    EXPECT_EQ(report.ErrorCount(), 1u);
    EXPECT_EQ(countEntries(report, true, static_cast<int>(MicroarchError::JsonValue), "points"), 1u);

    s.points.pop_back();
    MicroarchReport atCap;
    EXPECT_TRUE(ValidateStructure(s, atCap));

    MicroarchSpec c = d4Segment();
    for (std::size_t i = c.curves.size(); i <= SpecLimits::max_curves; ++i)
    {
        c.curves.push_back(c.curves[0]);
        c.curves.back().id = "c" + std::to_string(i);
    }
    MicroarchReport curves;
    EXPECT_FALSE(ValidateStructure(c, curves));
    EXPECT_EQ(countEntries(curves, true, static_cast<int>(MicroarchError::JsonValue), "curves"), 1u);
}
