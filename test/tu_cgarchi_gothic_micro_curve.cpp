#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : courbe 3D et echantillonnage (etape 4)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §6.4, §7, §8 J3,
// §9). Tolerance : 1e-9 * D pour une valeur composee (3D, apres racine).
// Les controles de projection comparent a des donnees saisies a la main, pas a
// PointAt lui-meme.

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";

    double dist2(const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }
    double dist3(const Vector3d &a, const Vector3d &b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

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

    MicroarchSpec oneSegment(const SymmetrySpec &sym, double x0, double y0, double x1, double y1, double height)
    {
        MicroarchSpec spec;
        spec.symmetry = sym;
        spec.points = {point("A", x0, y0, std::nullopt), point("B", x1, y1, height)};
        BasicCurveSpec c;
        c.id = "l";
        c.start_point = 0;
        c.end_point = 1;
        c.plan = PlanSegment{spec.points[0].xy, spec.points[1].xy};
        spec.curves = {c};
        return spec;
    }

    MicroarchSpec oneArc(const SymmetrySpec &sym, const Vector2d &a, const Vector2d &b, const Vector2d &center, bool ccw,
                         double height)
    {
        MicroarchSpec spec;
        spec.symmetry = sym;
        spec.points = {point("A", a.x, a.y, std::nullopt), point("B", b.x, b.y, height)};
        BasicCurveSpec c;
        c.id = "l";
        c.start_point = 0;
        c.end_point = 1;
        c.plan = MakeArc(a, b, center, ccw);
        spec.curves = {c};
        return spec;
    }

    // D4 arc (§9.3) : centre (5,0), r 5, de (1,3) a (8,4), horaire, B a 5 pi (k = 2).
    MicroarchSpec d4Arc() { return oneArc(symmetry(4, 90.0), Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), false, 5 * kPi); }

    struct Built
    {
        DevelopedPlan                 plan;
        MicroarchReport               report;
        std::vector<BasicCurveResult> basics;
        std::vector<SpaceCurve>       curves;
        double                        D = 0.0;
    };

    Built build(const MicroarchSpec &spec)
    {
        Built b;
        const std::optional<DevelopedPlan> plan = DevelopPlan(spec, b.report);
        EXPECT_TRUE(plan.has_value()) << ReportToJson(b.report);
        if (!plan)
            return b;
        b.plan = *plan;
        b.D = plan->tol.scale;
        const std::optional<std::vector<BasicCurveResult>> basics = BuildBasicCurves(spec, b.plan, b.report);
        EXPECT_TRUE(basics.has_value()) << ReportToJson(b.report);
        if (!basics)
            return b;
        b.basics = *basics;
        b.curves = BuildSpaceCurves(b.plan, b.basics);
        return b;
    }

    const SpaceCurve &replica(const Built &b, SymmetryElement g)
    {
        for (const SpaceCurve &c : b.curves)
            if (c.element.k == g.k && c.element.reflected == g.reflected)
                return c;
        ADD_FAILURE() << "replique absente (k=" << g.k << ", r=" << g.reflected << ")";
        return b.curves.front();
    }

    using Evaluator = std::function<Vector3d(double)>;

    // Projection laterale : (s, z) sur le cercle 1 de centre (rA, zA) pour s <= sC,
    // sur le cercle 2 de centre (L - rB, zB) au-dela, et sur la bonne moitie de
    // chaque cercle : du cote de zB par rapport a zA pour l'arc 1, du cote de zA
    // par rapport a zB pour l'arc 2. L'equation du cercle seule ne distingue pas
    // les deux moities. Ecart maximal.
    double lateralResidual(const Evaluator &f, const std::vector<double> &s, double L, double sC,
                           double zA, double zB, double rA, double rB)
    {
        const double sigma = zB > zA ? 1.0 : -1.0;
        double worst = 0.0;
        for (double v : s)
        {
            const double z = f(v).z;
            const bool first = v <= sC;
            const double r = first ? std::hypot(v - rA, z - zA) - rA : std::hypot(v - (L - rB), z - zB) - rB;
            const double wrongHalf = first ? std::max(0.0, -sigma * (z - zA)) : std::max(0.0, sigma * (z - zB));
            worst = std::max({worst, std::fabs(r), wrongHalf});
        }
        return worst;
    }

    // Projection XY sur un arc de plan : ecart maximal au cercle porteur saisi a
    // la main, et ecart des deux bouts de la polyligne aux bouts attendus.
    double xyArcResidual(const Evaluator &f, const std::vector<double> &s, const Vector2d &center, double radius,
                         const Vector2d &start, const Vector2d &end)
    {
        double worst = 0.0;
        for (double v : s)
        {
            const Vector3d p = f(v);
            worst = std::max(worst, std::fabs(std::hypot(p.x - center.x, p.y - center.y) - radius));
        }
        const Vector3d a = f(s.front()), b = f(s.back());
        worst = std::max(worst, dist2(Vector2d(a.x, a.y), start));
        worst = std::max(worst, dist2(Vector2d(b.x, b.y), end));
        return worst;
    }

    double polylineLengthXY(const Evaluator &f, const std::vector<double> &s)
    {
        double len = 0.0;
        for (std::size_t i = 1; i < s.size(); ++i)
        {
            const Vector3d a = f(s[i - 1]), b = f(s[i]);
            len += std::hypot(b.x - a.x, b.y - a.y);
        }
        return len;
    }

    Evaluator exact(const SpaceCurve &c) { return [&c](double s) { return PointAt(c, s); }; }

    // Tangente 3D unitaire : composante horizontale le long de la tangente du plan.
    Vector3d tangent3(const SpaceCurve &c, double s)
    {
        const Vector2d side = SideTangentAt(c.profile, s);
        const Vector2d t = TangentAt(c.plan, s);
        return Vector3d(side.x * t.x, side.x * t.y, side.y);
    }

    double angleBetween(const Vector3d &a, const Vector3d &b)
    {
        const double cx = a.y * b.z - a.z * b.y, cy = a.z * b.x - a.x * b.z, cz = a.x * b.y - a.y * b.x;
        return std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), a.x * b.x + a.y * b.y + a.z * b.z);
    }

    // Proprietes de l'echantillonnage (§7) : bornes exactes, s_C present, stricte
    // croissance, angle de chaque pas borne sur l'arc du profil et en plan.
    void expectSamplingProperties(const SpaceCurve &c, const SamplingParams &params)
    {
        const std::vector<double> s = SampleParameters(c, params);
        const OgeeProfile &p = c.profile;
        ASSERT_GE(s.size(), 3u);
        EXPECT_EQ(s.front(), 0.0);
        EXPECT_EQ(s.back(), p.length);
        EXPECT_NE(std::find(s.begin(), s.end(), p.s_prc), s.end());
        for (std::size_t i = 1; i < s.size(); ++i)
            ASSERT_GT(s[i], s[i - 1]) << "i = " << i;
        const Vector2d oA = StartCenter(p), oB = EndCenter(p);
        const auto *arc = std::get_if<PlanArc>(&c.plan);
        const double slack = 1e-9;
        for (std::size_t i = 1; i < s.size(); ++i)
        {
            const bool first = s[i] <= p.s_prc;
            const Vector2d o = first ? oA : oB;
            const double a0 = std::atan2(HeightAt(p, s[i - 1]) - o.y, s[i - 1] - o.x);
            const double a1 = std::atan2(HeightAt(p, s[i]) - o.y, s[i] - o.x);
            double da = std::fabs(a1 - a0);
            da = std::min(da, 2 * kPi - da);
            EXPECT_LE(da, params.max_angle + slack) << "profil, i = " << i;
            if (arc)
                EXPECT_LE((s[i] - s[i - 1]) / arc->radius, params.max_angle + slack) << "plan, i = " << i;
        }

        // Borne superieure : par arc du profil, ceil(dphi / m) pas pour le profil
        // plus, sur un arc de plan de rayon R, ceil(2 ds / (R m)) pour le plan, ds
        // etant la longueur en s de l'arc (r |sin phi| dphi integre, borne a 2 pres).
        const double m = params.max_angle;
        const double phi1 = kPi - std::acos(std::clamp((p.s_prc - p.r_start) / p.r_start, -1.0, 1.0));
        const double phi2 = kPi - std::acos(std::clamp((p.length - p.s_prc - p.r_end) / p.r_end, -1.0, 1.0));
        double bound = std::ceil(phi1 / m) + std::ceil(phi2 / m) + 3;
        if (arc)
            bound += std::ceil(2 * p.s_prc / (arc->radius * m)) + std::ceil(2 * (p.length - p.s_prc) / (arc->radius * m));
        EXPECT_LE(static_cast<double>(s.size()), bound) << "echantillons";
    }
}

// ---------------------------------------------------------------------------
//  Points analytiques
// ---------------------------------------------------------------------------

// Etoile : segment (4,0) -> (0,0) sous D4, B a 8 : PRC au milieu, r_A = r_B = 5.
TEST(TEST_cgarchi_gothic_micro_curve, star_points)
{
    const Built b = build(oneSegment(symmetry(4, 90.0), 4, 0, 0, 0, 8));
    ASSERT_EQ(b.curves.size(), 4u);
    const SpaceCurve &c = replica(b, SymmetryElement{0, false});
    const double tol = 1e-9 * b.D;
    EXPECT_LE(dist3(PointAt(c, 1), Vector3d(3, 0, 3)), tol);
    EXPECT_LE(dist3(PointAt(c, 3), Vector3d(1, 0, 5)), tol);
    EXPECT_LE(dist3(PointAt(c, c.profile.s_prc), Vector3d(2, 0, 4)), tol);
    // Arc 1 dans le plan vertical y = 0 : (4 - x - 5)^2 + z^2 = 25.
    for (int i = 0; i <= 20; ++i)
    {
        const Vector3d p = PointAt(c, 2.0 * i / 20);
        EXPECT_NEAR(p.y, 0.0, tol);
        EXPECT_NEAR((4 - p.x - 5) * (4 - p.x - 5) + p.z * p.z, 25.0, 25 * 1e-9) << "i = " << i;
    }
}

TEST(TEST_cgarchi_gothic_micro_curve, d4_segment_points)
{
    MicroarchSpec spec;
    MicroarchReport report;
    ASSERT_TRUE(LoadSpecFromFile(kD4Segment, spec, report)) << ReportToJson(report);
    const Built b = build(spec);
    ASSERT_EQ(b.curves.size(), 8u);
    const SpaceCurve &c = replica(b, SymmetryElement{0, false});
    const double tol = 1e-9 * b.D;
    EXPECT_LE(dist3(PointAt(c, 1), Vector3d(3.4, 1.8, std::sqrt(68.0 / 7))), tol);
    EXPECT_NEAR(PointAt(c, 1).z, 3.116775, 1e-6);
    EXPECT_LE(dist3(PointAt(c, 4), Vector3d(1.6, 4.2, 10 - std::sqrt(93.0 / 7))), tol);
    EXPECT_NEAR(PointAt(c, 4).z, 6.355043, 1e-6);
    EXPECT_LE(dist3(PointAt(c, c.profile.s_prc), Vector3d(19.0 / 7, 19.0 / 7, 30.0 / 7)), tol);
    // Les repliques partagent le profil : meme hauteur au meme s.
    for (const SpaceCurve &r : b.curves)
        EXPECT_EQ(PointAt(r, 1).z, PointAt(c, 1).z);
}

TEST(TEST_cgarchi_gothic_micro_curve, d4_arc_points)
{
    const Built b = build(d4Arc());
    const SpaceCurve &c = replica(b, SymmetryElement{0, false});
    const double tol = 1e-9 * b.D;
    // s = 1 : angle atan2(3, -4) - 1/5 en sens horaire ; z = sqrt(2 r_A - 1).
    const double theta = std::atan2(3.0, -4.0) - 0.2;
    const double rA = 12.5 * std::acos(0.6);
    EXPECT_LE(dist3(PointAt(c, 1), Vector3d(5 + 5 * std::cos(theta), 5 * std::sin(theta), std::sqrt(2 * rA - 1))), tol);
    const Vector3d p = PointAt(c, 1);
    EXPECT_NEAR(p.x, 1.675742, 1e-6);
    EXPECT_NEAR(p.y, 3.734877, 1e-6);
    EXPECT_NEAR(p.z, 4.709817, 1e-6);
    // PRC en (5,5), z_C = 2 s_C (k = 2).
    EXPECT_LE(dist3(PointAt(c, c.profile.s_prc), Vector3d(5, 5, 10 * std::acos(0.6))), tol);
}

// ---------------------------------------------------------------------------
//  Projections, verticalite, G1
// ---------------------------------------------------------------------------

// Projections des echantillons sur des donnees saisies a la main (§6.4), pour
// l'identite et deux reflexions : (k = 3) d'axe y = x, (k = 0) d'axe y.
TEST(TEST_cgarchi_gothic_micro_curve, projections_match_the_plan_and_the_profile)
{
    const Built b = build(d4Arc());
    const double D = b.D, L = 5 * kPi / 2, sC = 5 * std::acos(0.6);
    const double rA = 12.5 * std::acos(0.6), rB = 12.5 * std::asin(0.6);
    struct Expected { SymmetryElement g; Vector2d center, start, end; };
    const Expected cases[] = {
        {{0, false}, Vector2d(5, 0), Vector2d(1, 3), Vector2d(8, 4)},
        {{3, true}, Vector2d(0, 5), Vector2d(3, 1), Vector2d(4, 8)},
        {{0, true}, Vector2d(-5, 0), Vector2d(-1, 3), Vector2d(-8, 4)},
    };
    for (const Expected &e : cases)
    {
        SCOPED_TRACE(::testing::Message() << "k = " << e.g.k << ", r = " << e.g.reflected);
        const SpaceCurve &c = replica(b, e.g);
        const std::vector<double> s = SampleParameters(c, SamplingParams{});
        EXPECT_LE(xyArcResidual(exact(c), s, e.center, 5.0, e.start, e.end), 1e-12 * D);
        EXPECT_LE(lateralResidual(exact(c), s, L, sC, 0.0, 5 * kPi, rA, rB), 1e-9 * D);
        const double len = polylineLengthXY(exact(c), s);
        EXPECT_GE(len, L * (1 - 1e-3));
        EXPECT_LE(len, L);
    }
}

// Detecteurs (§6.4) : les deux fautes font echouer les controles de projection.
TEST(TEST_cgarchi_gothic_micro_curve, projection_checks_catch_the_two_faults)
{
    const Built b = build(d4Arc());
    const double D = b.D, L = 5 * kPi / 2, sC = 5 * std::acos(0.6);
    const double rA = 12.5 * std::acos(0.6), rB = 12.5 * std::asin(0.6), zB = 5 * kPi;
    const SpaceCurve &c = replica(b, SymmetryElement{0, false});
    const std::vector<double> s = SampleParameters(c, SamplingParams{});

    // Faute 1 : arc 2 evalue sur la branche haute (+sigma).
    const Evaluator upper = [&](double v) {
        Vector3d p = PointAt(c, v);
        if (v > sC)
        {
            const double t = L - v;
            p.z = zB + std::sqrt(std::max(0.0, t * (2 * rB - t)));
        }
        return p;
    };
    EXPECT_LE(lateralResidual(exact(c), s, L, sC, 0.0, zB, rA, rB), 1e-9 * D);
    EXPECT_GT(lateralResidual(upper, s, L, sC, 0.0, zB, rA, rB), 1.0);

    // Faute 2 : replique reflechie sans inverser le balayage.
    SpaceCurve wrong = replica(b, SymmetryElement{3, true});
    std::get<PlanArc>(wrong.plan).sweep = -std::get<PlanArc>(wrong.plan).sweep;
    const SpaceCurve &right = replica(b, SymmetryElement{3, true});
    EXPECT_LE(xyArcResidual(exact(right), s, Vector2d(0, 5), 5.0, Vector2d(3, 1), Vector2d(4, 8)), 1e-12 * D);
    EXPECT_GT(xyArcResidual(exact(wrong), s, Vector2d(0, 5), 5.0, Vector2d(3, 1), Vector2d(4, 8)), 1.0);
}

TEST(TEST_cgarchi_gothic_micro_curve, vertical_at_the_ends_and_g1_at_the_prc)
{
    for (bool arc : {false, true})
    {
        SCOPED_TRACE(arc ? "D4 arc" : "D4 segment");
        MicroarchSpec spec = arc ? d4Arc() : oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 10);
        const Built b = build(spec);
        for (const SpaceCurve &c : b.curves)
        {
            const double L = c.profile.length, eps = 1e-8 * L;
            for (const auto &[s0, s1] : {std::make_pair(0.0, eps), std::make_pair(L, L - eps)})
            {
                const Vector3d a = PointAt(c, s0), p = PointAt(c, s1);
                const double horizontal = std::hypot(p.x - a.x, p.y - a.y);
                const double vertical = std::fabs(p.z - a.z);
                EXPECT_LE(horizontal / vertical, 1e-3) << "s = " << s0;
            }
            const double sC = c.profile.s_prc;
            EXPECT_LE(angleBetween(tangent3(c, sC), tangent3(c, std::nextafter(sC, L))), 1e-9);
        }
    }
}

// ---------------------------------------------------------------------------
//  Construction des courbes de base
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_curve, basic_curve_profiles_and_diagnostics)
{
    // Niveau a base 2 : les hauteurs du profil l'incluent.
    MicroarchSpec spec = oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 10);
    spec.levels = {LevelSpec{0, 2.0}};
    const Built b = build(spec);
    ASSERT_EQ(b.basics.size(), 1u);
    EXPECT_EQ(b.basics[0].profile.z_start, 2.0);
    EXPECT_EQ(b.basics[0].profile.z_end, 12.0);
    EXPECT_EQ(b.basics[0].profile.s_prc, b.basics[0].prc.s);
    EXPECT_EQ(b.report.WarningCount(), 0u) << ReportToJson(b.report);

    // Deux bouts au sol : profil plat, erreur sur curves[0].
    MicroarchSpec flat = oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 10);
    flat.points[1].height.reset();
    flat.points[1].ground = true;
    MicroarchReport r;
    const std::optional<DevelopedPlan> plan = DevelopPlan(flat, r);
    ASSERT_TRUE(plan.has_value()) << ReportToJson(r);
    EXPECT_FALSE(BuildBasicCurves(flat, *plan, r).has_value());
    ASSERT_EQ(r.ErrorCount(), 1u) << ReportToJson(r);
    EXPECT_EQ(r.Count(MicroarchError::ProfileFlat), 1u);
    EXPECT_EQ(r.entries.back().path, "curves[0]");

    // Deux courbes de base (n = 1) : chaque SpaceCurve porte le profil de la sienne.
    MicroarchSpec two;
    two.symmetry = symmetry(1, std::nullopt);
    two.points = {point("A", 0, 0, std::nullopt), point("B", 4, 0, 6.0),
                  point("C", 2, -1, std::nullopt), point("E", 2, 3, 6.0)};
    for (int i : {0, 2})
    {
        BasicCurveSpec c;
        c.id = "l" + std::to_string(i);
        c.start_point = i;
        c.end_point = i + 1;
        c.plan = PlanSegment{two.points[i].xy, two.points[i + 1].xy};
        two.curves.push_back(c);
    }
    const Built bt = build(two);
    ASSERT_EQ(bt.curves.size(), 2u);
    EXPECT_NE(bt.basics[0].prc.s, bt.basics[1].prc.s);
    for (const SpaceCurve &c : bt.curves)
        EXPECT_EQ(c.profile.s_prc, bt.basics[static_cast<std::size_t>(c.basic)].prc.s);
    EXPECT_EQ(bt.curves[1].basic, 1);
    // Parallele a plan.curves : meme courbe de base, meme element, meme courbe du plan.
    ASSERT_EQ(b.curves.size(), b.plan.curves.size());
    for (std::size_t i = 0; i < b.curves.size(); ++i)
    {
        EXPECT_EQ(b.curves[i].basic, b.plan.curves[i].basic);
        EXPECT_EQ(b.curves[i].element.k, b.plan.curves[i].element.k);
        EXPECT_EQ(b.curves[i].element.reflected, b.plan.curves[i].element.reflected);
        EXPECT_EQ(dist2(StartPoint(b.curves[i].plan), StartPoint(b.plan.curves[i].plan)), 0.0);
        EXPECT_EQ(dist2(EndPoint(b.curves[i].plan), EndPoint(b.plan.curves[i].plan)), 0.0);
    }
    // Une replique dont la courbe de base manque : resultat vide, pas de trou.
    EXPECT_TRUE(BuildSpaceCurves(b.plan, {}).empty());
    EXPECT_TRUE(BuildSpaceCurves(bt.plan, {bt.basics[0]}).empty());

    // k = 0,4 < 1 : construit, avec profile_not_monotonic.
    const Built low = build(oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 2));
    ASSERT_EQ(low.basics.size(), 1u);
    EXPECT_FALSE(low.basics[0].profile.monotonic);
    EXPECT_EQ(low.report.Count(MicroarchWarning::ProfileNotMonotonic), 1u);
}

// ---------------------------------------------------------------------------
//  Echantillonnage
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_curve, sampling_properties)
{
    // Le D4 arc a h = 1e4 (k ~ 1270) a un profil presque vertical : un pas en
    // angle du profil y deplace s tres peu, ce que la borne superieure verifie.
    for (const MicroarchSpec &spec : {d4Arc(), oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 10),
                                      oneSegment(symmetry(4, 90.0), 4, 1, 1, 5, 2),
                                      oneArc(symmetry(4, 90.0), Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), false, 1e4)})
    {
        const Built b = build(spec);
        for (double maxAngle : {0.087266462599716, 0.01, 0.5})
            for (const SpaceCurve &c : b.curves)
            {
                SCOPED_TRACE(::testing::Message() << "max_angle = " << maxAngle << ", k = " << c.element.k);
                expectSamplingProperties(c, SamplingParams{maxAngle});
            }
    }
    // max_angle invalide : seulement 0, s_C, L.
    const Built b = build(d4Arc());
    const SpaceCurve &c = b.curves.front();
    for (double bad : {0.0, -1.0, std::nan("")})
    {
        const std::vector<double> s = SampleParameters(c, SamplingParams{bad});
        ASSERT_EQ(s.size(), 3u);
        EXPECT_EQ(s[1], c.profile.s_prc);
    }
}

// Profil « aiguille » (revue de J2-c, M13) : arc de plan qui part de (0,0) presque
// tangent a l'axe miroir x, xi = 1e-4 ; PRC a u ~ 1,3e-4, r_start ~ 1,1e-3.
TEST(TEST_cgarchi_gothic_micro_curve, needle_profile_sampling)
{
    const double xi = 1e-4, r = std::sqrt(1 + xi * xi);
    const Built b = build(oneArc(symmetry(1, 0.0), Vector2d(0, 0), Vector2d(xi + r, 1), Vector2d(xi, 1), true, 5.0));
    ASSERT_EQ(b.basics.size(), 1u);
    EXPECT_EQ(b.report.Count(MicroarchWarning::PrcNearEndpoint), 1u) << ReportToJson(b.report);
    EXPECT_LT(b.basics[0].profile.r_start, 2e-3);
    for (const SpaceCurve &c : b.curves)
    {
        expectSamplingProperties(c, SamplingParams{});
        expectSamplingProperties(c, SamplingParams{0.01});
    }
}
