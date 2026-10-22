#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : reseau (etape 5)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §6.5, §8 J4-a,
// §9). Positions a 1e-9 * D ; nombres de jonctions et de croisements exacts.

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";
    const char *const kC4Pinwheel = "./test/data/gothic_micro_c4_pinwheel.json";

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

    BasicCurveSpec curve(const std::string &id, const MicroarchSpec &spec, int a, int b, std::optional<Vector2d> center, bool ccw)
    {
        BasicCurveSpec c;
        c.id = id;
        c.start_point = a;
        c.end_point = b;
        c.plan = center ? PlanCurve(MakeArc(spec.points[a].xy, spec.points[b].xy, *center, ccw))
                        : PlanCurve(PlanSegment{spec.points[a].xy, spec.points[b].xy});
        return c;
    }

    MicroarchSpec loadFile(const char *path)
    {
        MicroarchSpec spec;
        MicroarchReport report;
        EXPECT_TRUE(LoadSpecFromFile(path, spec, report)) << ReportToJson(report);
        return spec;
    }

    MicroarchSpec star()
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(4, 90.0);
        spec.points = {point("A", 4, 0, std::nullopt), point("B", 0, 0, 8.0)};
        spec.curves = {curve("l", spec, 0, 1, std::nullopt, false)};
        return spec;
    }

    MicroarchSpec d4Arc()
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(4, 90.0);
        spec.points = {point("C", 1, 3, std::nullopt), point("E", 8, 4, 5 * kPi)};
        spec.curves = {curve("l", spec, 0, 1, Vector2d(5, 0), false)};
        return spec;
    }

    Network network(const MicroarchSpec &spec, MicroarchReport &report)
    {
        std::optional<Network> net = BuildNetwork(spec, report);
        EXPECT_TRUE(net.has_value()) << ReportToJson(report);
        return net ? std::move(*net) : Network{};
    }

    // Chaque position attendue est atteinte par exactement une jonction.
    void expectJunctionsAt(const Network &net, const std::vector<Vector3d> &expected, double tol)
    {
        ASSERT_EQ(net.junctions.size(), expected.size());
        for (const Vector3d &e : expected)
        {
            const auto n = std::count_if(net.junctions.begin(), net.junctions.end(),
                                         [&](const Junction &j) { return dist3(j.position, e) <= tol; });
            EXPECT_EQ(n, 1) << "(" << e.x << ", " << e.y << ", " << e.z << ")";
        }
    }

    std::string scientific(double v)
    {
        std::ostringstream os;
        os << std::scientific << std::setprecision(3) << v;
        return os.str();
    }

    double maxGap(const Network &net)
    {
        double g = 0.0;
        for (const Junction &j : net.junctions)
            g = std::max(g, j.max_gap);
        return g;
    }

    double minDz(const Network &net)
    {
        double d = std::numeric_limits<double>::infinity();
        for (const VisualCrossing &x : net.crossings)
            d = std::min(d, x.min_dz);
        return d;
    }
}

// ---------------------------------------------------------------------------
//  References
// ---------------------------------------------------------------------------

// D4 segment (§9.1) : 4 jonctions au PRC, 0 croisement, 16 extremites libres.
TEST(TEST_cgarchi_gothic_micro_network, d4_segment)
{
    MicroarchReport report;
    const Network net = network(loadFile(kD4Segment), report);
    const double D = net.tol.scale, a = 19.0 / 7, z = 30.0 / 7;
    ASSERT_EQ(net.curves.size(), 8u);
    expectJunctionsAt(net, {Vector3d(a, a, z), Vector3d(-a, a, z), Vector3d(a, -a, z), Vector3d(-a, -a, z)}, 1e-9 * D);
    for (const Junction &j : net.junctions)
    {
        ASSERT_EQ(j.incidences.size(), 2u);
        EXPECT_NE(j.incidences[0].curve, j.incidences[1].curve);
        for (const Incidence &i : j.incidences)
            EXPECT_NEAR(i.s, 15.0 / 7, 1e-9 * D);
        EXPECT_TRUE(j.prc);
        EXPECT_FALSE(j.ground);
    }
    EXPECT_TRUE(net.crossings.empty());
    ASSERT_EQ(net.free_ends.size(), 16u);
    int ground = 0, top = 0;
    for (const Incidence &e : net.free_ends)
    {
        const double ze = PointAt(net.curves[static_cast<std::size_t>(e.curve)], e.s).z;
        ground += ze == 0.0;
        top += ze == 10.0;
    }
    EXPECT_EQ(ground, 8);
    EXPECT_EQ(top, 8);
    EXPECT_EQ(report.WarningCount(), 0u) << ReportToJson(report);
}

// Moulinet C4 (§9.2) : 4 croisements visuels, aucune jonction.
TEST(TEST_cgarchi_gothic_micro_network, pinwheel)
{
    MicroarchReport report;
    const Network net = network(loadFile(kC4Pinwheel), report);
    const double D = net.tol.scale;
    ASSERT_EQ(net.curves.size(), 4u);
    EXPECT_TRUE(net.junctions.empty());
    ASSERT_EQ(net.crossings.size(), 4u);
    const double zLow = std::sqrt(21.0), zHigh = 10 - std::sqrt(11.5);
    for (const Vector2d &e : {Vector2d(-0.2, 1.4), Vector2d(-1.4, -0.2), Vector2d(0.2, -1.4), Vector2d(1.4, 0.2)})
    {
        const auto it = std::find_if(net.crossings.begin(), net.crossings.end(),
                                     [&](const VisualCrossing &x) { return dist2(x.xy, e) <= 1e-9 * D; });
        ASSERT_NE(it, net.crossings.end()) << "(" << e.x << ", " << e.y << ")";
        ASSERT_EQ(it->groups.size(), 2u);
        EXPECT_NEAR(it->groups[0].z, zLow, 1e-9 * D);
        EXPECT_NEAR(it->groups[1].z, zHigh, 1e-9 * D);
        EXPECT_NEAR(it->min_dz, zHigh - zLow, 1e-9 * D);
        EXPECT_NEAR(it->min_dz, 2.026259, 1e-6);
    }
    EXPECT_EQ(net.free_ends.size(), 8u);
}

TEST(TEST_cgarchi_gothic_micro_network, d4_arc)
{
    MicroarchReport report;
    const Network net = network(d4Arc(), report);
    const double D = net.tol.scale, z = 10 * std::acos(0.6);   // 9,272952
    expectJunctionsAt(net, {Vector3d(5, 5, z), Vector3d(-5, 5, z), Vector3d(5, -5, z), Vector3d(-5, -5, z)}, 1e-9 * D);
    EXPECT_TRUE(net.crossings.empty());
    for (const Junction &j : net.junctions)
        EXPECT_TRUE(j.prc);
    EXPECT_EQ(net.free_ends.size(), 16u);
}

// Etoile : 6 paires de segments se touchent bout a bout en (0,0), dont 2
// colineaires ; un seul noeud de 4 incidences a s = 4, pas d'avertissement
// tangent (contact bout a bout, §6.6).
TEST(TEST_cgarchi_gothic_micro_network, star)
{
    MicroarchReport report;
    const Network net = network(star(), report);
    ASSERT_EQ(net.curves.size(), 4u);
    ASSERT_EQ(net.junctions.size(), 1u);
    const Junction &j = net.junctions[0];
    EXPECT_LE(dist3(j.position, Vector3d(0, 0, 8)), 1e-9 * net.tol.scale);
    ASSERT_EQ(j.incidences.size(), 4u);
    for (const Incidence &i : j.incidences)
        EXPECT_NEAR(i.s, 4.0, 1e-12 * net.tol.scale);
    EXPECT_FALSE(j.prc);
    EXPECT_TRUE(net.crossings.empty());
    ASSERT_EQ(net.free_ends.size(), 4u);
    for (const Incidence &e : net.free_ends)
    {
        EXPECT_EQ(e.s, 0.0);
        EXPECT_EQ(PointAt(net.curves[static_cast<std::size_t>(e.curve)], e.s).z, 0.0);
    }
    EXPECT_EQ(report.Count(MicroarchWarning::TangentialContact), 0u) << ReportToJson(report);
}

// ---------------------------------------------------------------------------
//  Proprietes
// ---------------------------------------------------------------------------

// Les symetries conservent les hauteurs : g^-1 renvoie le xy d'une replique sur
// celui de la courbe de base au meme s, avec le meme z.
TEST(TEST_cgarchi_gothic_micro_network, heights_are_conserved_by_the_symmetries)
{
    const MicroarchSpec spec = d4Arc();
    MicroarchReport report;
    const Network net = network(spec, report);
    const SpaceCurve &basic = net.curves.front();
    ASSERT_EQ(basic.element.k, 0);
    ASSERT_FALSE(basic.element.reflected);
    const int n = spec.symmetry.rotation_order;
    for (const SpaceCurve &c : net.curves)
    {
        const SymmetryElement inverse = c.element.reflected ? c.element : SymmetryElement{(n - c.element.k) % n, false};
        const double L = c.profile.length;
        for (double s : {0.0, 0.25 * L, c.profile.s_prc, L})
        {
            const Vector3d p = PointAt(c, s), q = PointAt(basic, s);
            EXPECT_LE(dist2(Apply(spec.symmetry, inverse, Vector2d(p.x, p.y)), Vector2d(q.x, q.y)), 1e-12 * net.tol.scale)
                << "k = " << c.element.k << ", r = " << c.element.reflected << ", s = " << s;
            EXPECT_EQ(p.z, q.z);
        }
    }
}

// Noeud mixte synthetique : deux courbes a z = 1, une a z = 2 ; une jonction et
// un croisement. Une incidence en double est fusionnee ; deux incidences d'une
// meme courbe au meme z ne font pas une jonction.
TEST(TEST_cgarchi_gothic_micro_network, mixed_node_classification)
{
    const double tp = 1e-7, th = 1e-7;
    const NodeClassification m = ClassifyNode(Vector2d(0, 0),
        {NodeIncidence{Incidence{0, 1.0}, Vector3d(0, 0, 1)},
         NodeIncidence{Incidence{1, 2.0}, Vector3d(0, 0, 1 + 0.5 * th)},
         NodeIncidence{Incidence{0, 1.0 + 0.5 * tp}, Vector3d(0, 0, 1)},
         NodeIncidence{Incidence{2, 3.0}, Vector3d(0, 0, 2)}}, tp, th);
    ASSERT_EQ(m.junctions.size(), 1u);
    ASSERT_EQ(m.junctions[0].incidences.size(), 2u);
    EXPECT_EQ(m.junctions[0].incidences[0].curve, 0);
    EXPECT_EQ(m.junctions[0].incidences[1].curve, 1);
    EXPECT_NEAR(m.junctions[0].max_gap, 0.5 * th, 1e-15);
    ASSERT_TRUE(m.crossing.has_value());
    ASSERT_EQ(m.crossing->groups.size(), 2u);
    EXPECT_EQ(m.crossing->groups[1].incidences.size(), 1u);
    EXPECT_NEAR(m.crossing->min_dz, 1 - 0.5 * th, 1e-15);

    // Ecart de 2 tol_height : deux groupes ; une seule courbe par groupe.
    const NodeClassification v = ClassifyNode(Vector2d(0, 0),
        {NodeIncidence{Incidence{0, 1.0}, Vector3d(0, 0, 1)}, NodeIncidence{Incidence{1, 2.0}, Vector3d(0, 0, 1 + 2 * th)}}, tp, th);
    EXPECT_TRUE(v.junctions.empty());
    EXPECT_TRUE(v.crossing.has_value());

    // Meme courbe, abscisses a 0,5 tol_abscissa mais z a 2 tol_height : deux points
    // distincts (pres d'une tangente laterale verticale), donc deux groupes.
    const NodeClassification t = ClassifyNode(Vector2d(0, 0),
        {NodeIncidence{Incidence{0, 1.0}, Vector3d(0, 0, 1)}, NodeIncidence{Incidence{0, 1.0 + 0.5 * tp}, Vector3d(0, 0, 1 + 2 * th)}}, tp, th);
    EXPECT_TRUE(t.crossing.has_value());

    // Meme courbe, deux abscisses, meme z : ni jonction ni croisement.
    const NodeClassification s = ClassifyNode(Vector2d(0, 0),
        {NodeIncidence{Incidence{0, 1.0}, Vector3d(0, 0, 1)}, NodeIncidence{Incidence{0, 3.0}, Vector3d(0, 0, 1)}}, tp, th);
    EXPECT_TRUE(s.junctions.empty());
    EXPECT_FALSE(s.crossing.has_value());
}

// Deux courbes de base (n = 1) partent du meme point au sol : une jonction au
// sol, hors PRC.
TEST(TEST_cgarchi_gothic_micro_network, ground_junction)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(1, std::nullopt);
    spec.points = {point("A", 0, 0, std::nullopt), point("B", 4, 0, 6.0), point("C", 0, 4, 6.0)};
    spec.curves = {curve("l1", spec, 0, 1, std::nullopt, false), curve("l2", spec, 0, 2, std::nullopt, false)};
    MicroarchReport report;
    const Network net = network(spec, report);
    ASSERT_EQ(net.junctions.size(), 1u);
    EXPECT_TRUE(net.junctions[0].ground);
    EXPECT_FALSE(net.junctions[0].prc);
    EXPECT_LE(dist3(net.junctions[0].position, Vector3d(0, 0, 0)), 1e-12);
    EXPECT_EQ(net.free_ends.size(), 2u);
}

// Trois courbes au sol (n = 1) : a part de (0,0), b finit en (-f tol, 0), c en
// (+f tol, 0). Pour f = 0,9, b et c sont a 1,8 tol l'une de l'autre : les deux
// contacts (a, b) et (a, c) forment deux noeuds en plan, qui partagent
// l'incidence (a, s = 0). Un seul noeud, une seule jonction de 3 courbes.
TEST(TEST_cgarchi_gothic_micro_network, nodes_sharing_an_incidence_are_merged)
{
    for (double f : {0.4, 0.9})
    {
        SCOPED_TRACE(::testing::Message() << "f = " << f);
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A", 0, 0, std::nullopt), point("B", 2, 0, 5.0), point("C", -1, 1.5, 5.0),
                       point("Cg", 0, 0, std::nullopt), point("E", -1, -1.5, 5.0), point("Eg", 0, 0, std::nullopt)};
        const double tau = 1e-9 * Scale(spec);
        spec.points[3].xy = Vector2d(-f * tau, 0);
        spec.points[5].xy = Vector2d(f * tau, 0);
        spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 3, 2, std::nullopt, false),
                       curve("c", spec, 5, 4, std::nullopt, false)};
        MicroarchReport report;
        const Network net = network(spec, report);
        ASSERT_EQ(net.junctions.size(), 1u);
        ASSERT_EQ(net.junctions[0].incidences.size(), 3u);
        EXPECT_TRUE(net.junctions[0].ground);
        EXPECT_EQ(net.free_ends.size(), 3u);
    }
}

// Decision M16 : plafond de points de contact. La reference D4 segment en a 4
// (une paire par jonction) : acceptee a 4, refusee a 3.
TEST(TEST_cgarchi_gothic_micro_network, contact_limit)
{
    const MicroarchSpec spec = loadFile(kD4Segment);
    // Valeur par defaut fixee par la decision M16-bis.
    EXPECT_EQ(NetworkLimits{}.max_contacts, std::size_t(131072));
    MicroarchReport ok;
    EXPECT_TRUE(BuildNetwork(spec, ok, NetworkLimits{4}).has_value()) << ReportToJson(ok);
    MicroarchReport refused;
    EXPECT_FALSE(BuildNetwork(spec, refused, NetworkLimits{3}).has_value());
    ASSERT_EQ(refused.ErrorCount(), 1u) << ReportToJson(refused);
    EXPECT_EQ(refused.Count(MicroarchError::TooManyContacts), 1u);
    EXPECT_EQ(refused.entries.back().path, "curves");
}

// Decision M17 : « moulures quasi concentriques ». Deux arcs de -30 a 30 deg,
// tangents interieurement en (1,0) : rayons 1 et 1,001 (centre (-0,001 ; 0)).
// Courbure relative ~1e-3 ; zone a tol pres sqrt(2 tol / kappa) ~ 1,2e-3 D :
// recouvrement. Avec le rayon 1,003, la zone vaut ~7e-4 D : un contact tangent.
TEST(TEST_cgarchi_gothic_micro_network, tangent_zone_longer_than_the_threshold_is_an_overlap)
{
    const auto mouldings = [](double r2) {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        const double a = kPi / 6, c2 = 1 - r2;
        spec.points = {point("A", std::cos(a), -std::sin(a), std::nullopt), point("B", std::cos(a), std::sin(a), 5.0),
                       point("C", c2 + r2 * std::cos(a), -r2 * std::sin(a), std::nullopt),
                       point("E", c2 + r2 * std::cos(a), r2 * std::sin(a), 5.0)};
        spec.curves = {curve("inner", spec, 0, 1, Vector2d(0, 0), true), curve("outer", spec, 2, 3, Vector2d(c2, 0), true)};
        return spec;
    };
    for (double r2 : {1.001, 1.003})
    {
        SCOPED_TRACE(::testing::Message() << "r2 = " << r2);
        const MicroarchSpec spec = mouldings(r2);
        const double D = Scale(spec), tau = 1e-9 * D;
        const double zone = std::sqrt(2 * tau / (1.0 - 1.0 / r2));
        MicroarchReport report;
        const std::optional<Network> net = BuildNetwork(spec, report);
        if (r2 == 1.001)
        {
            ASSERT_GT(zone, 1.1e-3 * D);
            EXPECT_FALSE(net.has_value());
            EXPECT_EQ(report.Count(MicroarchError::PlanOverlap), 1u) << ReportToJson(report);
            EXPECT_EQ(report.ErrorCount(), 1u);
        }
        else
        {
            ASSERT_LT(zone, 0.9e-3 * D);
            ASSERT_TRUE(net.has_value()) << ReportToJson(report);
            EXPECT_EQ(report.Count(MicroarchWarning::TangentialContact), 2u);
            EXPECT_EQ(net->junctions.size() + net->crossings.size(), 1u);
        }
    }
}

// Decision M19 : les groupes de hauteur restent chaines (§6.5), mais une jonction
// dont les incidences s'etalent de plus de tol.height est signalee. Quatre segments
// partent de (0,0) a z = {0 ; 0,9 ; 1,8 ; 2,7} tol.height : une jonction, max_gap =
// 2,7 tol.height, un avertissement. A z = {0 ; 0,5} tol.height : aucun.
TEST(TEST_cgarchi_gothic_micro_network, junction_height_spread_warning)
{
    const auto star4 = [](const std::vector<double> &factors) {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        for (std::size_t k = 0; k < factors.size(); ++k)
        {
            const double a = 0.5 * kPi * static_cast<double>(k);
            spec.points.push_back(point("O" + std::to_string(k), 0, 0, 1.0));
            spec.points.push_back(point("F" + std::to_string(k), 2 * std::cos(a), 2 * std::sin(a), 5.0));
        }
        const double th = 1e-7 * Scale(spec);
        for (std::size_t k = 0; k < factors.size(); ++k)
        {
            spec.points[2 * k].ground = factors[k] == 0.0;
            spec.points[2 * k].height = factors[k] == 0.0 ? std::optional<double>() : std::optional<double>(factors[k] * th);
            spec.curves.push_back(curve("l" + std::to_string(k), spec, static_cast<int>(2 * k), static_cast<int>(2 * k + 1),
                                        std::nullopt, false));
        }
        return spec;
    };
    MicroarchReport spread;
    const Network a = network(star4({0.0, 0.9, 1.8, 2.7}), spread);
    ASSERT_EQ(a.junctions.size(), 1u);
    EXPECT_EQ(a.junctions[0].incidences.size(), 4u);
    EXPECT_NEAR(a.junctions[0].max_gap, 2.7 * a.tol.height, 1e-12 * a.tol.scale);
    EXPECT_EQ(spread.Count(MicroarchWarning::JunctionHeightSpread), 1u) << ReportToJson(spread);
    EXPECT_EQ(spread.entries.back().path, "curves[0]");

    MicroarchReport tight;
    const Network b = network(star4({0.0, 0.5}), tight);
    ASSERT_EQ(b.junctions.size(), 1u);
    EXPECT_EQ(tight.Count(MicroarchWarning::JunctionHeightSpread), 0u) << ReportToJson(tight);
}

// a : (0,0) au sol -> (1,0) a 5 ; b finit au sol en (0,0) ; c part de (1e-7, 0) a la
// hauteur de a en ce point (cas du relecteur, probe9). a@0 et a@1e-7 sont a moins
// de tol.parameter en s, mais leurs z different de ~sqrt(2 r 1e-7) > tol.height
// (tangente laterale verticale au depart) : deux points. Deux jonctions, aucun croisement.
TEST(TEST_cgarchi_gothic_micro_network, close_incidences_of_one_curve_stay_distinct)
{
    const double s0 = 1e-7;
    const auto make = [&](double hC) {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A0", 0, 0, std::nullopt), point("A1", 1, 0, 5.0), point("B1", -1, 1, 5.0),
                       point("C0", s0, 0, hC), point("C1", 0.5, 1.2, 5.0)};
        spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 2, 0, std::nullopt, false),
                       curve("c", spec, 3, 4, std::nullopt, false)};
        return spec;
    };
    MicroarchReport first;
    const Network probe = network(make(1.0), first);
    ASSERT_FALSE(probe.basics.empty());
    const double zA = HeightAt(probe.basics[0].profile, s0);
    MicroarchReport report;
    const Network net = network(make(zA), report);
    EXPECT_EQ(net.junctions.size(), 2u);
    EXPECT_TRUE(net.crossings.empty());
}

// T-jonctions interieures (probe12) : a de (0,0) a (2,0) ; b et c finissent sur
// l'interieur de a, en x = 1 -/+ f tol.point, a la hauteur de a. Les abscisses de a
// different de 2 f tol.point (pas de recalage a l'interieur) : un seul noeud, une
// jonction de 3 courbes, pour f = 0,4 comme pour f = 0,9.
TEST(TEST_cgarchi_gothic_micro_network, interior_t_junctions_close_in_abscissa_are_one_node)
{
    for (double f : {0.4, 0.9})
    {
        SCOPED_TRACE(::testing::Message() << "f = " << f);
        const auto make = [&](double hb, double hc, double tau) {
            MicroarchSpec spec;
            spec.symmetry = symmetry(1, std::nullopt);
            spec.points = {point("A0", 0, 0, std::nullopt), point("A1", 2, 0, 5.0), point("B0", -0.5, 1.5, 1.0),
                           point("B1", 1 - f * tau, 0, hb), point("C0", 2.5, 1.5, 1.0), point("C1", 1 + f * tau, 0, hc)};
            spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 2, 3, std::nullopt, false),
                           curve("c", spec, 4, 5, std::nullopt, false)};
            return spec;
        };
        const double tau = 1e-9 * Scale(make(2, 3, 0));
        MicroarchReport first;
        const Network probe = network(make(2, 3, tau), first);
        ASSERT_FALSE(probe.basics.empty());
        const double hb = HeightAt(probe.basics[0].profile, 1 - f * tau), hc = HeightAt(probe.basics[0].profile, 1 + f * tau);
        MicroarchReport report;
        const Network net = network(make(hb, hc, tau), report);
        ASSERT_EQ(net.junctions.size(), 1u);
        EXPECT_EQ(net.junctions[0].incidences.size(), 3u);
        EXPECT_TRUE(net.crossings.empty());
    }
}

// Triple tangence en P = (0,1) (probe12) : A (centre (0,0), r 1), B (centre (0,2),
// r 1, tangence exterieure), C (centre (0,9 tol.point ; 0,5), r 0,5, tangence
// interieure), a des hauteurs differentes en P. Les points de contact calcules
// par paire sont a quelques tol.point les uns des autres : un seul croisement, a
// trois groupes de hauteur.
TEST(TEST_cgarchi_gothic_micro_network, triple_tangency_is_one_crossing)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(1, std::nullopt);
    const double tau = 1e-9 * 3.2;
    const auto arcPoints = [&](const Vector2d &c, double r, double a0, double a1, const std::string &i0,
                               const std::string &i1, double h) {
        spec.points.push_back(point(i0, c.x + r * std::cos(a0), c.y + r * std::sin(a0), std::nullopt));
        spec.points.push_back(point(i1, c.x + r * std::cos(a1), c.y + r * std::sin(a1), h));
    };
    const Vector2d cA(0, 0), cB(0, 2), cC(0.9 * tau, 0.5);
    arcPoints(cA, 1, kPi / 2 + 0.6, kPi / 2 - 0.6, "A0", "A1", 4.0);
    arcPoints(cB, 1, -kPi / 2 - 0.6, -kPi / 2 + 0.6, "B0", "B1", 6.0);
    arcPoints(cC, 0.5, kPi / 2 + 1.0, kPi / 2 - 1.0, "C0", "C1", 8.0);
    spec.curves = {curve("A", spec, 0, 1, cA, false), curve("B", spec, 2, 3, cB, true), curve("C", spec, 4, 5, cC, false)};
    MicroarchReport report;
    const Network net = network(spec, report);
    EXPECT_TRUE(net.junctions.empty());
    ASSERT_EQ(net.crossings.size(), 1u);
    EXPECT_EQ(net.crossings[0].groups.size(), 3u);
    EXPECT_LE(dist2(net.crossings[0].xy, Vector2d(0, 1)), 1e-8);
}

// Limite connue (§6.5 point 3, §11.2 risque 7), caracterisee ici : b, c et d
// finissent sur l'interieur de a, en x = 2, 2 + 0,9 tp et 2 + 1,8 tp (tp =
// tol.parameter), chacune a la hauteur de a ; la pente de a y vaut ~0,75, donc
// dz ~ 0,675 tp <= tol.height entre contacts voisins. L'union-find chaine les trois
// incidences de a en un seul noeud ; la deduplication garde a@2 et a@(2 + 1,8 tp),
// a plus de tol.parameter l'une de l'autre, dans la meme jonction.
TEST(TEST_cgarchi_gothic_micro_network, chained_incidences_of_one_curve_in_one_junction)
{
    const auto make = [](double hb, double hc, double hd, double tp) {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A0", 0, 0, std::nullopt), point("A1", 4, 0, 8.0),
                       point("B0", 1, 1.5, std::nullopt), point("B1", 2, 0, hb),
                       point("C0", 2.5, -1.5, std::nullopt), point("C1", 2 + 0.9 * tp, 0, hc),
                       point("D0", 3, 1.5, std::nullopt), point("D1", 2 + 1.8 * tp, 0, hd)};
        spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 2, 3, std::nullopt, false),
                       curve("c", spec, 4, 5, std::nullopt, false), curve("d", spec, 6, 7, std::nullopt, false)};
        return spec;
    };
    const double tp = 1e-7 * Scale(make(1, 1, 1, 0));
    MicroarchReport first;
    const Network probe = network(make(1, 1, 1, tp), first);
    ASSERT_FALSE(probe.basics.empty());
    const OgeeProfile &pa = probe.basics[0].profile;
    MicroarchReport report;
    const Network net = network(make(HeightAt(pa, 2), HeightAt(pa, 2 + 0.9 * tp), HeightAt(pa, 2 + 1.8 * tp), tp), report);
    ASSERT_EQ(net.junctions.size(), 1u);
    std::vector<double> sa;
    for (const Incidence &i : net.junctions[0].incidences)
        if (net.curves[static_cast<std::size_t>(i.curve)].basic == 0)
            sa.push_back(i.s);
    ASSERT_EQ(sa.size(), 2u);
    EXPECT_GT(std::fabs(sa[1] - sa[0]), net.tol.parameter);
}

// Arc de centre (0,0), r = 3, de 0 a 120 deg sous C4 : la rotation de 90 deg
// le recouvre sur 30 deg.
TEST(TEST_cgarchi_gothic_micro_network, plan_overlap_is_an_error)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(4, std::nullopt);
    spec.points = {point("A", 3, 0, std::nullopt), point("B", 3 * std::cos(2 * kPi / 3), 3 * std::sin(2 * kPi / 3), 5.0)};
    spec.curves = {curve("l", spec, 0, 1, Vector2d(0, 0), true)};
    MicroarchReport report;
    EXPECT_FALSE(BuildNetwork(spec, report).has_value());
    ASSERT_EQ(report.ErrorCount(), 1u) << ReportToJson(report);
    EXPECT_EQ(report.Count(MicroarchError::PlanOverlap), 1u);
    EXPECT_EQ(report.entries.back().path, "curves[0]");
}

// Contact tangent qui n'est pas bout a bout : avertissement sur les deux courbes
// de base. Segment (0,5) -> (10,5) tangent en (5,5) a l'arc de reference.
TEST(TEST_cgarchi_gothic_micro_network, tangential_contact_warning)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(1, std::nullopt);
    spec.points = {point("A", 0, 5, std::nullopt), point("B", 10, 5, 5.0),
                   point("C", 1, 3, std::nullopt), point("E", 8, 4, 5 * kPi)};
    spec.curves = {curve("s", spec, 0, 1, std::nullopt, false), curve("a", spec, 2, 3, Vector2d(5, 0), false)};
    MicroarchReport report;
    const Network net = network(spec, report);
    EXPECT_EQ(report.Count(MicroarchWarning::TangentialContact), 2u) << ReportToJson(report);
    // PRC de chaque courbe au contact (palier 2) ; hauteurs differentes : un croisement.
    EXPECT_TRUE(net.junctions.empty());
    EXPECT_EQ(net.crossings.size(), 1u);
}

// Mesures de la reserve R1 (§6.5, point 6), criteres de §8 J4-a :
// max_gap <= 1e-12 D et min_dz >= 1e3 tol.height. Les incidences aux bouts sont
// recalees a 0 ou L (M14) : a l'etoile, ou la tangente laterale est verticale, une
// abscisse a 1 ulp du bout donnerait sqrt(2 r ulp) ~ 1,7e-8 D en z. Les valeurs
// sont enregistrees dans la sortie XML de gtest.
TEST(TEST_cgarchi_gothic_micro_network, tolerance_margins_on_the_references)
{
    struct Case { const char *name; MicroarchSpec spec; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment)}, {"pinwheel", loadFile(kC4Pinwheel)},
                          {"d4_arc", d4Arc()}, {"star", star()}};
    for (const Case &c : cases)
    {
        MicroarchReport report;
        const Network net = network(c.spec, report);
        const double gap = maxGap(net), dz = minDz(net);
        RecordProperty(std::string(c.name) + "_max_gap_over_D", scientific(gap / net.tol.scale));
        RecordProperty(std::string(c.name) + "_min_dz_over_tol_height", scientific(dz / net.tol.height));
        EXPECT_LE(gap, 1e-12 * net.tol.scale) << c.name;
        EXPECT_GE(dz, 1e3 * net.tol.height) << c.name;
    }
}

// ---------------------------------------------------------------------------
//  Echantillonnage du reseau
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_network, sample_network)
{
    MicroarchReport report;
    const Network net = network(d4Arc(), report);
    MicroarchReport sampling;
    const std::vector<std::vector<Vector3d>> lines = SampleNetwork(net, SamplingParams{}, sampling);
    EXPECT_EQ(sampling.ErrorCount(), 0u);
    ASSERT_EQ(lines.size(), net.curves.size());
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const SpaceCurve &c = net.curves[i];
        const std::vector<double> s = SampleParameters(c, SamplingParams{});
        ASSERT_EQ(lines[i].size(), s.size());
        EXPECT_EQ(dist3(lines[i].front(), PointAt(c, 0.0)), 0.0);
        EXPECT_EQ(dist3(lines[i].back(), PointAt(c, c.profile.length)), 0.0);
    }
}

// Plafond global (M15) : n segments verticaux disjoints sans symetrie, max_angle
// = 1e-9. Chaque arc du profil atteint le plafond de pas : 131 073 points par
// courbe. 76 courbes : 9 961 548 points, acceptes ; 77 : 10 092 621, refuses.
TEST(TEST_cgarchi_gothic_micro_network, sample_network_point_limit)
{
    const auto segments = [](int n) {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        for (int i = 0; i < n; ++i)
        {
            spec.points.push_back(point("P" + std::to_string(i), i, 0, std::nullopt));
            spec.points.push_back(point("Q" + std::to_string(i), i, 1, 2.0));
            spec.curves.push_back(curve("l" + std::to_string(i), spec, 2 * i, 2 * i + 1, std::nullopt, false));
        }
        return spec;
    };
    const SamplingParams fine{1e-9};
    const std::size_t cap = SamplingLimits::max_network_points;

    MicroarchReport r76;
    const Network n76 = network(segments(76), r76);
    std::size_t expected = 0;
    for (const SpaceCurve &c : n76.curves)
        expected += SampleParameters(c, fine).size();
    EXPECT_EQ(expected, 76u * 131073u);
    EXPECT_EQ(CountNetworkSamples(n76, fine, std::numeric_limits<std::size_t>::max()), expected);
    ASSERT_LE(expected, cap);
    MicroarchReport s76;
    const std::vector<std::vector<Vector3d>> lines = SampleNetwork(n76, fine, s76);
    EXPECT_EQ(s76.ErrorCount(), 0u);
    ASSERT_EQ(lines.size(), 76u);
    std::size_t total = 0;
    for (const std::vector<Vector3d> &l : lines)
        total += l.size();
    EXPECT_EQ(total, expected);

    MicroarchReport r77;
    const Network n77 = network(segments(77), r77);
    EXPECT_GT(CountNetworkSamples(n77, fine, std::numeric_limits<std::size_t>::max()), cap);
    EXPECT_EQ(CountNetworkSamples(n77, fine, cap), cap + 1);   // arret des que le plafond est depasse
    MicroarchReport s77;
    EXPECT_TRUE(SampleNetwork(n77, fine, s77).empty());
    ASSERT_EQ(s77.ErrorCount(), 1u);
    EXPECT_EQ(s77.Count(MicroarchError::SampleLimitExceeded), 1u);
    EXPECT_EQ(s77.entries.back().path, "sampling.max_angle");
}
