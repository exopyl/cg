#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>
#include <cgarchi/gothic_microarchitecture_tubes.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : visualisation tubulaire (J5)
// ===========================================================================
//
// Valeurs calculees a la main (plan d'execution, §12.4). Reference D4 segment,
// K = 6, max_angle = 5 deg : 23 echantillons par courbe, P = 184, V = K P = 1 104,
// F = 2 K P - 4 C = 2 176.

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    const char *const kD4Segment = "./test/data/gothic_micro_d4_segment.json";
    const char *const kC4Pinwheel = "./test/data/gothic_micro_c4_pinwheel.json";

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

    MicroarchSpec oneCurve(const SymmetrySpec &sym, const Vector2d &a, const Vector2d &b, std::optional<Vector2d> center,
                           bool ccw, double height)
    {
        MicroarchSpec spec;
        spec.symmetry = sym;
        spec.points = {point("A", a.x, a.y, std::nullopt), point("B", b.x, b.y, height)};
        BasicCurveSpec c;
        c.id = "l";
        c.start_point = 0;
        c.end_point = 1;
        c.plan = center ? PlanCurve(MakeArc(a, b, *center, ccw)) : PlanCurve(PlanSegment{a, b});
        spec.curves = {c};
        return spec;
    }

    MicroarchSpec loadFile(const char *path)
    {
        MicroarchSpec spec;
        MicroarchReport report;
        EXPECT_TRUE(LoadSpecFromFile(path, spec, report)) << ReportToJson(report);
        return spec;
    }

    Network network(const MicroarchSpec &spec)
    {
        MicroarchReport report;
        std::optional<Network> net = BuildNetwork(spec, report);
        EXPECT_TRUE(net.has_value()) << ReportToJson(report);
        return net ? std::move(*net) : Network{};
    }

    TubeParams params(int sides = 6, double ratio = 0.005)
    {
        TubeParams p;
        p.sides = sides;
        p.radius_ratio = ratio;
        return p;
    }

    Vector3d vertex(const Mesh &m, std::size_t i)
    {
        const std::vector<float> &v = m.GetVertices();
        return Vector3d(v[3 * i], v[3 * i + 1], v[3 * i + 2]);
    }

    // Point du dessin : sommet (relatif) + origine du maillage.
    Vector3d drawingPoint(const TubeMesh &t, std::size_t i)
    {
        const Vector3d v = vertex(*t.mesh, i);
        return Vector3d(v.x + t.origin.x, v.y + t.origin.y, v.z + t.origin.z);
    }

    // Composantes connexes de la plage de triangles de la courbe i (union-find sur les
    // sommets des triangles) : instrument independant de chi.
    long componentsOfRange(const TubeMesh &t, std::size_t i)
    {
        const std::size_t v0 = t.vertex_begin[i], n = t.vertex_begin[i + 1] - v0;
        std::vector<std::size_t> parent(n);
        for (std::size_t k = 0; k < n; ++k)
            parent[k] = k;
        auto root = [&](std::size_t k) {
            while (parent[k] != k)
                k = parent[k] = parent[parent[k]];
            return k;
        };
        std::vector<bool> used(n, false);
        for (std::size_t f = t.triangle_begin[i]; f < t.triangle_begin[i + 1]; ++f)
        {
            const auto face = t.mesh->FaceAt(static_cast<unsigned int>(f));
            const std::size_t a = static_cast<std::size_t>(face->GetVertex(0)) - v0;
            for (unsigned int k = 0; k < 3; ++k)
            {
                const std::size_t b = static_cast<std::size_t>(face->GetVertex(k)) - v0;
                used[b] = true;
                parent[root(b)] = root(a);
            }
        }
        long c = 0;
        for (std::size_t k = 0; k < n; ++k)
            c += used[k] && root(k) == k;
        return c;
    }

    // Plage de triangles de la courbe i : chaque arete orientee une fois, avec sa
    // jumelle ; rend V - E + F (2 par composante fermee de genre 0).
    long eulerOfClosedRange(const TubeMesh &t, std::size_t i)
    {
        std::vector<std::pair<long, long>> edges;
        for (std::size_t f = t.triangle_begin[i]; f < t.triangle_begin[i + 1]; ++f)
        {
            const auto face = t.mesh->FaceAt(static_cast<unsigned int>(f));
            for (int k = 0; k < 3; ++k)
                edges.emplace_back(face->GetVertex(static_cast<unsigned int>(k)), face->GetVertex(static_cast<unsigned int>((k + 1) % 3)));
        }
        std::sort(edges.begin(), edges.end());
        EXPECT_EQ(std::adjacent_find(edges.begin(), edges.end()), edges.end()) << "arete orientee en double";
        std::vector<std::pair<long, long>> reversed;
        for (const auto &e : edges)
            reversed.emplace_back(e.second, e.first);
        std::sort(reversed.begin(), reversed.end());
        EXPECT_TRUE(edges == reversed) << "arete sans sa jumelle";
        const long V = static_cast<long>(t.vertex_begin[i + 1] - t.vertex_begin[i]);
        const long F = static_cast<long>(t.triangle_begin[i + 1] - t.triangle_begin[i]);
        return V - static_cast<long>(edges.size()) / 2 + F;
    }

    double dist3(const Vector3d &a, const Vector3d &b)
    {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }

    double distanceToSegment(const Vector3d &p, const Vector3d &a, const Vector3d &b)
    {
        const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        const double len2 = dx * dx + dy * dy + dz * dz;
        double t = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy + (p.z - a.z) * dz) / len2 : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        return dist3(p, Vector3d(a.x + t * dx, a.y + t * dy, a.z + t * dz));
    }

    std::vector<Vector3d> samples(const SpaceCurve &c, const SamplingParams &sp)
    {
        std::vector<Vector3d> out;
        for (double s : SampleParameters(c, sp))
            out.push_back(PointAt(c, s));
        return out;
    }

    std::string scientific(double v)
    {
        std::ostringstream os;
        os << std::scientific << std::setprecision(3) << v;
        return os.str();
    }

    MicroarchSpec scaledSpec(MicroarchSpec spec, double k)
    {
        for (PointSpec &p : spec.points)
        {
            p.xy = Vector2d(p.xy.x * k, p.xy.y * k);
            if (p.height)
                *p.height *= k;
        }
        for (BasicCurveSpec &c : spec.curves)
            c.plan = PlanSegment{spec.points[static_cast<std::size_t>(c.start_point)].xy,
                                 spec.points[static_cast<std::size_t>(c.end_point)].xy};
        return spec;
    }
}

// ---------------------------------------------------------------------------
//  Comptes
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_tubes, d4_segment_counts)
{
    const Network net = network(loadFile(kD4Segment));
    ASSERT_EQ(net.curves.size(), 8u);
    for (const auto &[sides, V, F] : {std::make_tuple(6, 1104u, 2176u), std::make_tuple(3, 552u, 1072u)})
    {
        SCOPED_TRACE(::testing::Message() << "K = " << sides);
        MicroarchReport report;
        const TubeMesh tubes = BuildTubeMesh(net, params(sides), report);
        ASSERT_TRUE(tubes.mesh) << ReportToJson(report);
        EXPECT_EQ(tubes.mesh->GetNVertices(), V);
        EXPECT_EQ(tubes.mesh->GetNFaces(), F);
        EXPECT_EQ(CountTubeTriangles(net, params(sides), std::numeric_limits<std::size_t>::max()), F);
        ASSERT_EQ(tubes.vertex_begin.size(), 9u);
        ASSERT_EQ(tubes.triangle_begin.size(), 9u);
        for (std::size_t i = 0; i <= 8; ++i)
        {
            EXPECT_EQ(tubes.vertex_begin[i], i * 23u * static_cast<std::size_t>(sides));
            EXPECT_EQ(tubes.triangle_begin[i], i * (F / 8));
        }
        EXPECT_EQ(report.ErrorCount() + report.WarningCount(), 0u) << ReportToJson(report);
    }
}

// ---------------------------------------------------------------------------
//  Geometrie : distance a l'axe, centroides des anneaux
// ---------------------------------------------------------------------------

// Chaque sommet est a r +/- 1e-5 D de la polyligne echantillonnee ; le centroide de
// l'anneau k de la courbe i est l'echantillon k, a 1e-5 D pres.
TEST(TEST_cgarchi_gothic_micro_tubes, vertices_at_radius_and_rings_centred_on_samples)
{
    // La reference, puis la meme translatee de (7, -4) : boite des echantillons hors de l'origine.
    MicroarchSpec moved = loadFile(kD4Segment);
    moved.symmetry.center = Vector2d(7, -4);
    for (PointSpec &q : moved.points)
        q.xy = Vector2d(q.xy.x + 7, q.xy.y - 4);
    for (BasicCurveSpec &c : moved.curves)
        c.plan = PlanSegment{moved.points[static_cast<std::size_t>(c.start_point)].xy,
                             moved.points[static_cast<std::size_t>(c.end_point)].xy};
    for (const MicroarchSpec &spec : {loadFile(kD4Segment), moved})
    {
    const Network net = network(spec);
    const TubeParams p = params();
    MicroarchReport report;
    const TubeMesh tubes = BuildTubeMesh(net, p, report);
    ASSERT_TRUE(tubes.mesh);
    const double D = net.tol.scale, r = tubes.radius, tol = 1e-5 * D;
    EXPECT_DOUBLE_EQ(r, 0.005 * D);
    const std::size_t K = 6;
    double worstRadius = 0.0, worstCentroid = 0.0;
    for (std::size_t i = 0; i < net.curves.size(); ++i)
    {
        const std::vector<Vector3d> axis = samples(net.curves[i], p.sampling);
        ASSERT_EQ(tubes.vertex_begin[i + 1] - tubes.vertex_begin[i], axis.size() * K);
        for (std::size_t v = tubes.vertex_begin[i]; v < tubes.vertex_begin[i + 1]; ++v)
        {
            double d = std::numeric_limits<double>::infinity();
            for (std::size_t k = 1; k < axis.size(); ++k)
                d = std::min(d, distanceToSegment(drawingPoint(tubes, v), axis[k - 1], axis[k]));
            worstRadius = std::max(worstRadius, std::fabs(d - r));
        }
        for (std::size_t k = 0; k < axis.size(); ++k)
        {
            Vector3d c(0, 0, 0);
            for (std::size_t j = 0; j < K; ++j)
            {
                const Vector3d q = drawingPoint(tubes, tubes.vertex_begin[i] + k * K + j);
                c = Vector3d(c.x + q.x / K, c.y + q.y / K, c.z + q.z / K);
            }
            worstCentroid = std::max(worstCentroid, dist3(c, axis[k]));
        }
    }
    RecordProperty(spec.symmetry.center.x == 0 ? "max_radius_error_over_D" : "moved_max_radius_error_over_D", scientific(worstRadius / D));
    RecordProperty(spec.symmetry.center.x == 0 ? "max_centroid_error_over_D" : "moved_max_centroid_error_over_D", scientific(worstCentroid / D));
    EXPECT_LE(worstRadius, tol);
    EXPECT_LE(worstCentroid, tol);
    }
}

// ---------------------------------------------------------------------------
//  Aucun NaN
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_tubes, coordinates_are_finite)
{
    const double xi = 1e-4, r = std::sqrt(1 + xi * xi);
    std::vector<MicroarchSpec> specs = {
        loadFile(kD4Segment), loadFile(kC4Pinwheel),
        oneCurve(symmetry(4, 90.0), Vector2d(1, 3), Vector2d(8, 4), Vector2d(5, 0), false, 5 * kPi),
        oneCurve(symmetry(4, 90.0), Vector2d(4, 0), Vector2d(0, 0), std::nullopt, false, 8),
        oneCurve(symmetry(1, 0.0), Vector2d(0, 0), Vector2d(xi + r, 1), Vector2d(xi, 1), true, 5.0),
        oneCurve(symmetry(1, std::nullopt), Vector2d(0, -2), Vector2d(0, 2), std::nullopt, false, 6.0)};
    for (std::size_t n = 0; n < specs.size(); ++n)
    {
        SCOPED_TRACE(::testing::Message() << "cas " << n);
        const Network net = network(specs[n]);
        MicroarchReport report;
        const TubeMesh tubes = BuildTubeMesh(net, params(), report);
        ASSERT_TRUE(tubes.mesh) << ReportToJson(report);
        ASSERT_GT(tubes.mesh->GetNVertices(), 0u);
        for (float f : tubes.mesh->GetVertices())
            ASSERT_TRUE(std::isfinite(f));
    }
}

// ---------------------------------------------------------------------------
//  Composantes fermees
// ---------------------------------------------------------------------------

// Par courbe : chaque arete orientee (a, b) une fois, et (b, a) une fois ; V - E + F = 2.
TEST(TEST_cgarchi_gothic_micro_tubes, each_curve_is_a_closed_oriented_surface)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport report;
    const TubeMesh tubes = BuildTubeMesh(net, params(), report);
    ASSERT_TRUE(tubes.mesh);
    for (std::size_t i = 0; i < net.curves.size(); ++i)
    {
        SCOPED_TRACE(::testing::Message() << "courbe " << i);
        std::map<std::pair<long, long>, int> edges;
        const std::size_t v0 = tubes.vertex_begin[i], v1 = tubes.vertex_begin[i + 1];
        for (std::size_t f = tubes.triangle_begin[i]; f < tubes.triangle_begin[i + 1]; ++f)
        {
            const auto face = tubes.mesh->FaceAt(static_cast<unsigned int>(f));
            for (int k = 0; k < 3; ++k)
            {
                const long a = face->GetVertex(static_cast<unsigned int>(k)), b = face->GetVertex(static_cast<unsigned int>((k + 1) % 3));
                ASSERT_GE(a, static_cast<long>(v0));
                ASSERT_LT(a, static_cast<long>(v1));
                ++edges[{a, b}];
            }
        }
        for (const auto &[e, count] : edges)
        {
            EXPECT_EQ(count, 1);
            const auto opposite = edges.find({e.second, e.first});
            ASSERT_NE(opposite, edges.end());
            EXPECT_EQ(opposite->second, 1);
        }
        const long V = static_cast<long>(v1 - v0), F = static_cast<long>(tubes.triangle_begin[i + 1] - tubes.triangle_begin[i]);
        const long E = static_cast<long>(edges.size()) / 2;
        EXPECT_EQ(V - E + F, 2);
    }
}

// ---------------------------------------------------------------------------
//  Invariance d'echelle
// ---------------------------------------------------------------------------

// D4 segment multiplie par 1e-5 et 1e3 (points et hauteurs) : memes V et F, sommets / D
// egaux a 1e-5.
TEST(TEST_cgarchi_gothic_micro_tubes, scale_invariance)
{
    const MicroarchSpec ref = loadFile(kD4Segment);
    const Network n0 = network(ref);
    MicroarchReport r0;
    const TubeMesh t0 = BuildTubeMesh(n0, params(), r0);
    ASSERT_TRUE(t0.mesh);
    for (double k : {1e-5, 1e3})
    {
        SCOPED_TRACE(::testing::Message() << "echelle " << k);
        const Network nk = network(scaledSpec(ref, k));
        MicroarchReport rk;
        const TubeMesh tk = BuildTubeMesh(nk, params(), rk);
        ASSERT_TRUE(tk.mesh) << ReportToJson(rk);
        ASSERT_EQ(tk.mesh->GetNVertices(), t0.mesh->GetNVertices());
        ASSERT_EQ(tk.mesh->GetNFaces(), t0.mesh->GetNFaces());
        double worst = 0.0;
        for (std::size_t v = 0; v < t0.mesh->GetNVertices(); ++v)
        {
            const Vector3d a = vertex(*t0.mesh, v), b = vertex(*tk.mesh, v);
            worst = std::max(worst, dist3(Vector3d(a.x / n0.tol.scale, a.y / n0.tol.scale, a.z / n0.tol.scale),
                                          Vector3d(b.x / nk.tol.scale, b.y / nk.tol.scale, b.z / nk.tol.scale)));
        }
        EXPECT_LE(worst, 1e-5);
    }
}

// ---------------------------------------------------------------------------
//  Plafond de triangles (M20)
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_tubes, triangle_limit)
{
    const Network net = network(loadFile(kD4Segment));
    EXPECT_EQ(TubeLimits{}.max_triangles, std::size_t(1) << 23);
    TubeLimits exact;
    exact.max_triangles = 2176;
    MicroarchReport ok;
    EXPECT_TRUE(BuildTubeMesh(net, params(), ok, exact).mesh) << ReportToJson(ok);
    TubeLimits below;
    below.max_triangles = 2175;
    MicroarchReport refused;
    const TubeMesh t = BuildTubeMesh(net, params(), refused, below);
    EXPECT_FALSE(t.mesh);
    EXPECT_TRUE(t.vertex_begin.empty());
    EXPECT_TRUE(t.triangle_begin.empty());
    ASSERT_EQ(refused.ErrorCount(), 1u) << ReportToJson(refused);
    EXPECT_EQ(refused.Count(MicroarchError::TubeLimitExceeded), 1u);
    EXPECT_EQ(refused.entries.back().path, "tubes");

    // Plafond par defaut, max_angle = 1e-5 : 65 536 pas par arc du profil, S = 131 073,
    // P = 1 048 584 (sous M15), F = 12 582 976 > 8 388 608.
    TubeParams fine = params();
    fine.sampling.max_angle = 1e-5;
    EXPECT_EQ(CountTubeTriangles(net, fine, std::numeric_limits<std::size_t>::max()), 12582976u);
    MicroarchReport big;
    const TubeMesh b = BuildTubeMesh(net, fine, big);
    EXPECT_FALSE(b.mesh);
    EXPECT_TRUE(b.vertex_begin.empty());
    ASSERT_EQ(big.Count(MicroarchError::TubeLimitExceeded), 1u);
    // Refus avant toute construction : le message donne le compte prevu en entier.
    EXPECT_NE(big.entries.back().message.find("12582976 triangles"), std::string::npos) << big.entries.back().message;
}

// ---------------------------------------------------------------------------
//  Parametres
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_tubes, parameters_are_checked)
{
    const Network net = network(loadFile(kD4Segment));
    const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
    const auto diagnose = [&](const TubeParams &p) {
        MicroarchReport r;
        const TubeMesh t = BuildTubeMesh(net, p, r);
        return std::make_pair(t.mesh != nullptr, r);
    };
    const auto expectRefused = [&](const TubeParams &p, const std::string &path) {
        const auto [built, r] = diagnose(p);
        EXPECT_FALSE(built) << path;
        ASSERT_EQ(r.ErrorCount() + r.WarningCount(), 1u) << ReportToJson(r);
        EXPECT_EQ(r.Count(MicroarchError::TubeInvalidParameter), 1u);
        EXPECT_EQ(r.entries.back().path, path);
    };
    for (double ratio : {0.0, -1.0, nan, inf, 0.1 * (1 + 1e-9)})
        expectRefused(params(6, ratio), "tubes.radius_ratio");
    EXPECT_TRUE(diagnose(params(6, 0.1)).first);
    for (int sides : {2, 65})
        expectRefused(params(sides), "tubes.sides");
    EXPECT_TRUE(diagnose(params(3)).first);
    EXPECT_TRUE(diagnose(params(64)).first);
    for (double a : {0.0, nan, kPi / 8 * (1 + 1e-9)})
    {
        TubeParams p = params();
        p.sampling.max_angle = a;
        expectRefused(p, "tubes.sampling.max_angle");
    }
    TubeParams eighth = params();
    eighth.sampling.max_angle = kPi / 8;
    EXPECT_TRUE(diagnose(eighth).first);
}

// ---------------------------------------------------------------------------
//  Avertissement : rayon qui cache un croisement
// ---------------------------------------------------------------------------

// Moulinet avec B a la hauteur 6 : k = 1,2, r_A = r_B = 3,05 ; h(2) = sqrt(8,2),
// h(4) = 6 - sqrt(5,1), min_dz = 0,8781178, seuil min_dz / (2 D) = 0,0878118.
TEST(TEST_cgarchi_gothic_micro_tubes, radius_hiding_a_crossing_is_reported)
{
    MicroarchSpec spec = loadFile(kC4Pinwheel);
    spec.points[1].height = 6.0;
    const Network net = network(spec);
    ASSERT_EQ(net.crossings.size(), 4u);
    EXPECT_NEAR(net.crossings[0].min_dz, 6 - std::sqrt(5.1) - std::sqrt(8.2), 1e-9);
    MicroarchReport below;
    EXPECT_TRUE(BuildTubeMesh(net, params(6, 0.0878), below).mesh);
    EXPECT_EQ(below.WarningCount(), 0u) << ReportToJson(below);
    MicroarchReport above;
    EXPECT_TRUE(BuildTubeMesh(net, params(6, 0.0879), above).mesh);
    ASSERT_EQ(above.WarningCount(), 1u) << ReportToJson(above);
    EXPECT_EQ(above.Count(MicroarchWarning::TubeRadiusHidesCrossing), 1u);
    EXPECT_EQ(above.entries.back().path, "tubes.radius_ratio");
}

// Prefiltre : profil « aiguille » (r_start ~ 1,1e-3, E ~ 5,5) et max_angle = 2e-4. Sur
// l'arc 1 du profil, ~3 000 pas consecutifs valent ~1e-5 dans le repere canonique :
// au-dessus du seuil de 1e-6 de CreateTubes, sous une cellule de sa grille (1e-4) ; le
// prefiltre (4e-4) les retire. Chaque courbe reste une surface
// fermee orientee (aucune enveloppe ajoutee au milieu d'un tube), et le nombre de
// triangles ne depasse pas le majorant.
TEST(TEST_cgarchi_gothic_micro_tubes, prefilter_keeps_tubes_closed_on_dense_samples)
{
    const double xi = 1e-4, r = std::sqrt(1 + xi * xi);
    const Network net = network(oneCurve(symmetry(1, 0.0), Vector2d(0, 0), Vector2d(xi + r, 1), Vector2d(xi, 1), true, 5.0));
    TubeParams p = params();
    p.sampling.max_angle = 2e-4;
    MicroarchReport report;
    const TubeMesh tubes = BuildTubeMesh(net, p, report);
    ASSERT_TRUE(tubes.mesh) << ReportToJson(report);
    // Compte exact (meme prefiltre) ; inferieur a 2 K P - 4 C : des echantillons ont ete retires.
    EXPECT_EQ(tubes.mesh->GetNFaces(), CountTubeTriangles(net, p, std::numeric_limits<std::size_t>::max()));
    const std::size_t P = CountNetworkSamples(net, p.sampling, std::numeric_limits<std::size_t>::max());
    EXPECT_LT(tubes.mesh->GetNFaces(), 2 * 6 * P - 4 * net.curves.size());
    for (std::size_t i = 0; i + 1 < tubes.triangle_begin.size(); ++i)
    {
        SCOPED_TRACE(::testing::Message() << "courbe " << i);
        std::vector<std::pair<long, long>> edges;
        for (std::size_t f = tubes.triangle_begin[i]; f < tubes.triangle_begin[i + 1]; ++f)
        {
            const auto face = tubes.mesh->FaceAt(static_cast<unsigned int>(f));
            for (int k = 0; k < 3; ++k)
                edges.emplace_back(face->GetVertex(static_cast<unsigned int>(k)), face->GetVertex(static_cast<unsigned int>((k + 1) % 3)));
        }
        std::sort(edges.begin(), edges.end());
        EXPECT_EQ(std::adjacent_find(edges.begin(), edges.end()), edges.end()) << "arete orientee en double";
        std::vector<std::pair<long, long>> reversed;
        reversed.reserve(edges.size());
        for (const auto &e : edges)
            reversed.emplace_back(e.second, e.first);
        std::sort(reversed.begin(), reversed.end());
        EXPECT_TRUE(edges == reversed) << "arete sans sa jumelle";
        const long V = static_cast<long>(tubes.vertex_begin[i + 1] - tubes.vertex_begin[i]);
        const long F = static_cast<long>(tubes.triangle_begin[i + 1] - tubes.triangle_begin[i]);
        EXPECT_EQ(V - static_cast<long>(edges.size()) / 2 + F, 2);
    }
}

// M25 : courbe plus courte que le rayon (cas du relecteur). D = 1000, r = 5 ; un arc de
// centre (500, 0), r = 0,3, de (500,3 ; 0) au sol a (500 ; 0,3) a la hauteur 1 : ses
// bouts sont a ~1,04 en 3D, sous r/2. CreateTubes le souderait en boucle (F = 472, avec
// enveloppe) ; il est coupe en morceaux ouverts : compte exact, chaque morceau ferme.
TEST(TEST_cgarchi_gothic_micro_tubes, curve_shorter_than_the_radius_stays_open)
{
    MicroarchSpec spec;
    spec.symmetry = symmetry(1, std::nullopt);
    // Second arc court, de bouts a ~2,04 : entre r/4 et r/2, il distingue le seuil de soudure.
    spec.points = {point("A", 0, 10, std::nullopt), point("B", 1000, 10, 30.0), point("C", 500.3, 0, std::nullopt),
                   point("E", 500, 0.3, 1.0), point("G", 300.3, 0, std::nullopt), point("H", 300, 0.3, 2.0)};
    BasicCurveSpec line;
    line.id = "long";
    line.start_point = 0;
    line.end_point = 1;
    line.plan = PlanSegment{spec.points[0].xy, spec.points[1].xy};
    BasicCurveSpec arc;
    arc.id = "court";
    arc.start_point = 2;
    arc.end_point = 3;
    arc.plan = MakeArc(spec.points[2].xy, spec.points[3].xy, Vector2d(500, 0), true);
    BasicCurveSpec mid = arc;
    mid.id = "moyen";
    mid.start_point = 4;
    mid.end_point = 5;
    mid.plan = MakeArc(spec.points[4].xy, spec.points[5].xy, Vector2d(300, 0), true);
    spec.curves = {line, arc, mid};
    const Network net = network(spec);
    MicroarchReport report;
    const TubeMesh tubes = BuildTubeMesh(net, params(), report);
    ASSERT_TRUE(tubes.mesh) << ReportToJson(report);
    EXPECT_NEAR(tubes.radius, 0.005 * net.tol.scale, 1e-12);
    EXPECT_EQ(tubes.mesh->GetNFaces(), CountTubeTriangles(net, params(), std::numeric_limits<std::size_t>::max()));
    // Courbe longue : un seul tube ouvert, chi = 2.
    EXPECT_EQ(eulerOfClosedRange(tubes, 0), 2);
    // Courbes courtes : c morceaux ouverts, chacun ferme par deux capuchons, donc chi = 2c
    // avec c compte par union-find (un morceau soude en boucle donnerait chi = 2c - 2) ;
    // et V = K (S + c - 1), c morceaux partageant c - 1 echantillons (le prefiltre ne
    // retire rien ici : pas canonique ~8e-3 > 4e-4). "court" : c = 18 (mesure de M25).
    for (std::size_t i : {std::size_t(1), std::size_t(2)})
    {
        SCOPED_TRACE(::testing::Message() << "courbe " << i);
        const long c = componentsOfRange(tubes, i);
        EXPECT_GE(c, 2);
        EXPECT_EQ(eulerOfClosedRange(tubes, i), 2 * c);
        const std::size_t S = SampleParameters(net.curves[i], params().sampling).size();
        EXPECT_EQ(tubes.vertex_begin[i + 1] - tubes.vertex_begin[i], 6 * (S + static_cast<std::size_t>(c) - 1));
        if (i == 1)
            EXPECT_EQ(c, 18);
    }
    EXPECT_NE(tubes.triangle_begin[2] - tubes.triangle_begin[1], 472u);
}

// Au-dela du plafond M15, CountTubeTriangles rend SIZE_MAX sans parcourir les
// echantillons : D64 avec reflexion, une courbe de base (128 repliques), max_angle =
// 1e-7, soit 128 x 131 073 = 16 777 344 echantillons > 1e7.
TEST(TEST_cgarchi_gothic_micro_tubes, count_is_bounded_by_the_sample_limit)
{
    MicroarchSpec spec = oneCurve(symmetry(64, 0.0), Vector2d(4, 1), Vector2d(1, 5), std::nullopt, false, 10.0);
    const Network net = network(spec);
    ASSERT_EQ(net.curves.size(), 128u);
    TubeParams p = params();
    p.sampling.max_angle = 1e-7;
    EXPECT_EQ(CountTubeTriangles(net, p, std::numeric_limits<std::size_t>::max()), std::numeric_limits<std::size_t>::max());
}

// M26 : origine locale. La reference D4 segment decalee de 1e6 et de 1e8 (points et
// centre de symetrie) : sommets relatifs egaux a ceux du dessin non decale, a une
// precision independante du decalage ; origine decalee du meme vecteur.
TEST(TEST_cgarchi_gothic_micro_tubes, mesh_origin_keeps_precision_far_from_zero)
{
    const MicroarchSpec ref = loadFile(kD4Segment);
    const Network n0 = network(ref);
    MicroarchReport r0;
    const TubeMesh t0 = BuildTubeMesh(n0, params(), r0);
    ASSERT_TRUE(t0.mesh);
    for (double shift : {1e6, 1e8})
    {
        SCOPED_TRACE(::testing::Message() << "decalage " << shift);
        MicroarchSpec moved = ref;
        moved.symmetry.center = Vector2d(shift, shift);
        for (PointSpec &q : moved.points)
            q.xy = Vector2d(q.xy.x + shift, q.xy.y + shift);
        for (BasicCurveSpec &c : moved.curves)
            c.plan = PlanSegment{moved.points[static_cast<std::size_t>(c.start_point)].xy,
                                 moved.points[static_cast<std::size_t>(c.end_point)].xy};
        const Network n = network(moved);
        MicroarchReport r;
        const TubeMesh t = BuildTubeMesh(n, params(), r);
        ASSERT_TRUE(t.mesh) << ReportToJson(r);
        ASSERT_EQ(t.mesh->GetNVertices(), t0.mesh->GetNVertices());
        double worst = 0.0;
        for (std::size_t v = 0; v < t0.mesh->GetNVertices(); ++v)
            worst = std::max(worst, dist3(vertex(*t.mesh, v), vertex(*t0.mesh, v)));
        RecordProperty(shift == 1e6 ? "shift_1e6_vertex_error_over_r" : "shift_1e8_vertex_error_over_r", scientific(worst / t.radius));
        EXPECT_LE(worst, 1e-3 * t.radius);
        const double ulp = std::nextafter(shift, 2 * shift) - shift;
        EXPECT_NEAR(t.origin.x, t0.origin.x + shift, 8 * ulp);
        EXPECT_NEAR(t.origin.y, t0.origin.y + shift, 8 * ulp);
        EXPECT_NEAR(t.origin.z, t0.origin.z, 1e-12 * n0.tol.scale);
    }
}
