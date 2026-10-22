#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_json.h>
#include <cgarchi/gothic_microarchitecture_solid.h>
#include "mesh.h"
#include "mesh_validation.h"
#include "thickness.h"

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : graphe du reseau (J7-0) et solide (J7-a)
// ===========================================================================
//
// Plan d'execution, §14. Criteres P1 a P9 de §14.3 ; instrument de P1-P5 :
// ValidateMesh avec soudure explicite de 1e-3 h, test d'auto-intersection non tronque.

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

    // Profil « aiguille » : r_start ~ 1,1e-3 (§12.6), la ou les tubes de J5 se replient.
    MicroarchSpec needle()
    {
        const double xi = 1e-4, r = std::sqrt(1 + xi * xi);
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, 0.0);
        spec.points = {point("A", 0, 0, std::nullopt), point("B", xi + r, 1, 5.0)};
        spec.curves = {curve("l", spec, 0, 1, Vector2d(xi, 1), true)};
        return spec;
    }

    // D4 segment sans point au sol : A a hauteur 2.
    MicroarchSpec d4Raised()
    {
        MicroarchSpec spec = loadFile(kD4Segment);
        spec.points[0].ground = false;
        spec.points[0].height = 2.0;
        return spec;
    }

    MicroarchSpec translated(MicroarchSpec spec, double dx, double dy)
    {
        spec.symmetry.center = Vector2d(spec.symmetry.center.x + dx, spec.symmetry.center.y + dy);
        for (PointSpec &p : spec.points)
            p.xy = Vector2d(p.xy.x + dx, p.xy.y + dy);
        for (BasicCurveSpec &c : spec.curves)
            if (const PlanSegment *s = std::get_if<PlanSegment>(&c.plan))
                c.plan = PlanSegment{Vector2d(s->start.x + dx, s->start.y + dy), Vector2d(s->end.x + dx, s->end.y + dy)};
        return spec;
    }

    Network network(const MicroarchSpec &spec)
    {
        MicroarchReport report;
        std::optional<Network> net = BuildNetwork(spec, report);
        EXPECT_TRUE(net.has_value()) << ReportToJson(report);
        return net ? std::move(*net) : Network{};
    }

    SolidParams solidParams(double ratio = 0.05, int q = 3)
    {
        SolidParams p;
        p.radius_ratio = ratio;
        p.cells_per_radius = q;
        return p;
    }

    MeshValidationReport validate(const Mesh &mesh, double cell)
    {
        MeshValidationOptions o;
        o.weldTolerance = 1e-3 * cell;
        return ValidateMesh(mesh, o);
    }

    // P1 a P5 : aucun defaut, test d'auto-intersection complet, volume positif.
    void expectSound(const MeshValidationReport &r)
    {
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
            EXPECT_EQ(r.counts[k], 0u) << MeshDefectName(static_cast<MeshDefect>(k));
        EXPECT_TRUE(r.selfIntersectionChecked);
        EXPECT_FALSE(r.selfIntersectionTruncated);
        EXPECT_GT(r.signedVolume, 0.0);
    }

    // Caracteristique d'Euler totale, comptee par le test sur les indices du maillage.
    long eulerOf(const Mesh &m)
    {
        const std::vector<unsigned int> t = m.GetTriangles();
        std::vector<std::pair<unsigned int, unsigned int>> edges;
        std::vector<bool> used(m.GetNVertices(), false);
        for (std::size_t f = 0; f + 2 < t.size(); f += 3)
            for (int k = 0; k < 3; ++k)
            {
                const unsigned int a = t[f + k], b = t[f + (k + 1) % 3];
                used[a] = true;
                edges.emplace_back(std::min(a, b), std::max(a, b));
            }
        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
        const long V = static_cast<long>(std::count(used.begin(), used.end(), true));
        return V - static_cast<long>(edges.size()) + static_cast<long>(t.size() / 3);
    }

    Vector3d drawingPoint(const SolidMesh &s, std::size_t i)
    {
        const std::vector<float> &v = s.mesh->GetVertices();
        return Vector3d(v[3 * i] + s.origin.x, v[3 * i + 1] + s.origin.y, v[3 * i + 2] + s.origin.z);
    }

    // Polylignes denses de reference (0,5 degre), en double.
    std::vector<std::vector<Vector3d>> densePolylines(const Network &net)
    {
        SamplingParams sp;
        sp.max_angle = 0.5 * kPi / 180.0;
        std::vector<std::vector<Vector3d>> out;
        for (const SpaceCurve &c : net.curves)
        {
            std::vector<Vector3d> line;
            for (double s : SampleParameters(c, sp))
                line.push_back(PointAt(c, s));
            out.push_back(std::move(line));
        }
        return out;
    }

    double distanceToPolyline(const std::vector<Vector3d> &line, const Vector3d &p)
    {
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i + 1 < line.size(); ++i)
        {
            const Vector3d &a = line[i], &b = line[i + 1];
            const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
            const double wx = p.x - a.x, wy = p.y - a.y, wz = p.z - a.z;
            const double uu = ux * ux + uy * uy + uz * uz;
            const double t = uu > 0 ? std::clamp((wx * ux + wy * uy + wz * uz) / uu, 0.0, 1.0) : 0.0;
            const double dx = wx - t * ux, dy = wy - t * uy, dz = wz - t * uz;
            best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        return best;
    }

    // Ecarts |d - r| des sommets : hors des plis (deuxieme courbe la plus proche a plus
    // de r + 2h) et hors du disque de coupe (z >= base + h) ; et partout hors du disque.
    struct Deviation
    {
        double offFolds = 0.0, everywhere = 0.0;
        std::size_t offFoldVertices = 0;
        std::vector<bool> offFold;   // par sommet
    };

    Deviation deviation(const SolidMesh &s, const std::vector<std::vector<Vector3d>> &lines, double base)
    {
        Deviation d;
        d.offFold.assign(s.mesh->GetNVertices(), false);
        for (std::size_t i = 0; i < s.mesh->GetNVertices(); ++i)
        {
            const Vector3d p = drawingPoint(s, i);
            if (p.z < base + s.cell)
                continue;
            double first = std::numeric_limits<double>::infinity(), second = first;
            for (const auto &line : lines)
            {
                const double dist = distanceToPolyline(line, p);
                if (dist < first)
                {
                    second = first;
                    first = dist;
                }
                else
                    second = std::min(second, dist);
            }
            const double e = std::fabs(first - s.radius);
            d.everywhere = std::max(d.everywhere, e);
            if (second > s.radius + 2 * s.cell)
            {
                d.offFolds = std::max(d.offFolds, e);
                ++d.offFoldVertices;
                d.offFold[i] = true;
            }
        }
        return d;
    }

    // Composante de chaque sommet du maillage (union-find sur les triangles).
    std::vector<std::size_t> vertexComponents(const Mesh &m, std::size_t &count)
    {
        const std::vector<unsigned int> t = m.GetTriangles();
        std::vector<std::size_t> parent(m.GetNVertices());
        for (std::size_t v = 0; v < parent.size(); ++v)
            parent[v] = v;
        auto root = [&](std::size_t v) {
            while (parent[v] != v)
                v = parent[v] = parent[parent[v]];
            return v;
        };
        for (std::size_t f = 0; f + 2 < t.size(); f += 3)
        {
            parent[root(t[f])] = root(t[f + 1]);
            parent[root(t[f])] = root(t[f + 2]);
        }
        std::vector<std::size_t> id(parent.size(), parent.size()), out(parent.size());
        count = 0;
        for (std::size_t v = 0; v < parent.size(); ++v)
        {
            const std::size_t r = root(v);
            if (id[r] == parent.size())
                id[r] = count++;
            out[v] = id[r];
        }
        return out;
    }

    // Champ aleatoire : reunion de spheres (graine fixe), dans le repere de la grille.
    struct Spheres
    {
        std::vector<double> c;   // x, y, z, rayon
        static float eval(float x, float y, float z, void *self)
        {
            const std::vector<double> &c = static_cast<Spheres *>(self)->c;
            double f = 2.0;
            for (std::size_t i = 0; i + 3 < c.size(); i += 4)
            {
                const double dx = x - c[i], dy = y - c[i + 1], dz = z - c[i + 2];
                f = std::min(f, std::sqrt(dx * dx + dy * dy + dz * dz) - c[i + 3]);
            }
            return static_cast<float>(f);
        }
    };

    Spheres randomSpheres(unsigned seed)
    {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> pos(8.0, 32.0), rad(1.5, 6.0);
        Spheres s;
        for (int i = 0; i < 24; ++i)
            s.c.insert(s.c.end(), {pos(rng), pos(rng), pos(rng), rad(rng)});
        return s;
    }

    std::unique_ptr<Mesh> meshOf(const GridIsoExtractor &e)
    {
        auto m = std::make_unique<Mesh>();
        m->SetVertices(static_cast<unsigned int>(e.Vertices().size() / 3), e.Vertices().data());
        std::vector<unsigned int> faces = e.Faces();
        m->SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
        return m;
    }

    // Triangles en coordonnees float, chacun tourne pour commencer par son plus petit
    // sommet (orientation gardee), puis tries : comparaison au bit pres.
    std::vector<std::array<std::uint32_t, 9>> sortedTriangles(const Mesh &m)
    {
        const std::vector<float> &v = m.GetVertices();
        const std::vector<unsigned int> t = m.GetTriangles();
        std::vector<std::array<std::uint32_t, 9>> out;
        for (std::size_t f = 0; f + 2 < t.size(); f += 3)
        {
            std::array<std::array<std::uint32_t, 3>, 3> c;
            for (int k = 0; k < 3; ++k)
                for (int a = 0; a < 3; ++a)
                    std::memcpy(&c[k][a], &v[3 * t[f + k] + a], sizeof(float));
            int first = 0;
            for (int k = 1; k < 3; ++k)
                if (c[k] < c[first])
                    first = k;
            std::array<std::uint32_t, 9> tri;
            for (int k = 0; k < 3; ++k)
                for (int a = 0; a < 3; ++a)
                    tri[3 * k + a] = c[(first + k) % 3][a];
            out.push_back(tri);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // Longueur 3D du morceau [s0, s1] d'une courbe, 4 096 cordes.
    double arcLength(const SpaceCurve &c, double s0, double s1)
    {
        double length = 0.0;
        Vector3d prev = PointAt(c, s0);
        for (int k = 1; k <= 4096; ++k)
        {
            const Vector3d next = PointAt(c, s0 + (s1 - s0) * k / 4096.0);
            length += std::sqrt((next.x - prev.x) * (next.x - prev.x) + (next.y - prev.y) * (next.y - prev.y)
                                + (next.z - prev.z) * (next.z - prev.z));
            prev = next;
        }
        return length;
    }

    // D4 segment sous une base de niveau a 5.
    MicroarchSpec d4OnBase(double base)
    {
        MicroarchSpec spec = loadFile(kD4Segment);
        spec.levels[0].base = base;
        return spec;
    }

    // D4 segment a 1e6 au-dessus de la base : A a 1e6, B a 1e6 + 10.
    MicroarchSpec d4High()
    {
        MicroarchSpec spec = loadFile(kD4Segment);
        spec.points[0].ground = false;
        spec.points[0].height = 1e6;
        spec.points[1].height = 1e6 + 10.0;
        return spec;
    }

    // Deux incidences d'une meme courbe dans une jonction (§11.2, risque 7).
    MicroarchSpec chainedSpec(double hb, double hc, double hd, double tp)
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A0", 0, 0, std::nullopt), point("A1", 4, 0, 8.0),
                       point("B0", 1, 1.5, std::nullopt), point("B1", 2, 0, hb),
                       point("C0", 2.5, -1.5, std::nullopt), point("C1", 2 + 0.9 * tp, 0, hc),
                       point("D0", 3, 1.5, std::nullopt), point("D1", 2 + 1.8 * tp, 0, hd)};
        spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 2, 3, std::nullopt, false),
                       curve("c", spec, 4, 5, std::nullopt, false), curve("d", spec, 6, 7, std::nullopt, false)};
        return spec;
    }

    Network chainedNetwork()
    {
        const double tp = 1e-7 * Scale(chainedSpec(1, 1, 1, 0));
        const Network probe = network(chainedSpec(1, 1, 1, tp));
        EXPECT_FALSE(probe.basics.empty());
        if (probe.basics.empty())
            return Network{};
        const OgeeProfile &pa = probe.basics[0].profile;
        return network(chainedSpec(HeightAt(pa, 2), HeightAt(pa, 2 + 0.9 * tp), HeightAt(pa, 2 + 1.8 * tp), tp));
    }
}

// ---------------------------------------------------------------------------
//  J7-0 : graphe du reseau
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_solid, network_graph_of_the_references)
{
    // J, F : jonctions et extremites libres, comptees a la main (§9).
    struct Case { const char *name; MicroarchSpec spec; std::size_t J, F, V, E, C; bool ground; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment), 4, 16, 20, 16, 4, true},
                          {"c4_pinwheel", loadFile(kC4Pinwheel), 0, 8, 8, 4, 4, true},
                          {"star", star(), 1, 4, 5, 4, 1, true},
                          {"d4_raised", d4Raised(), 4, 16, 20, 16, 4, false},
                          {"d4_on_base_5", d4OnBase(5.0), 4, 16, 20, 16, 4, true}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network net = network(c.spec);
        const NetworkGraph g = BuildNetworkGraph(net);
        EXPECT_EQ(net.junctions.size(), c.J);
        EXPECT_EQ(net.free_ends.size(), c.F);
        EXPECT_EQ(g.vertices, c.V);
        EXPECT_EQ(g.edges, c.E);
        EXPECT_EQ(g.components, c.C);
        ASSERT_EQ(g.component_touches_base.size(), c.C);
        for (std::size_t k = 0; k < c.C; ++k)
            EXPECT_EQ(g.component_touches_base[k], c.ground) << "composante " << k;
    }
}

// Deux incidences d'une meme courbe dans une jonction (§11.2, risque 7) : abscisses
// distinctes a plus de tol.parameter, donc une boucle sur la jonction.
TEST(TEST_cgarchi_gothic_micro_solid, network_graph_counts_a_chained_incidence_as_a_loop)
{
    const Network net = chainedNetwork();
    ASSERT_EQ(net.junctions.size(), 1u);
    EXPECT_EQ(net.free_ends.size(), 5u);
    const NetworkGraph g = BuildNetworkGraph(net);
    EXPECT_EQ(g.vertices, 6u);   // 1 jonction + 5 extremites libres
    EXPECT_EQ(g.edges, 6u);      // a : 3 morceaux dont une boucle ; b, c, d : 1
    EXPECT_EQ(g.components, 1u);

    // Contracte a 2r (r = 0,05 D) : la boucle, longue de ~1,8e-7 D, disparait.
    const NetworkGraph k = BuildNetworkGraph(net, 2 * 0.05 * net.tol.scale);
    EXPECT_EQ(k.vertices, 6u);
    EXPECT_EQ(k.edges, 5u);
    EXPECT_EQ(k.components, 1u);
}

// Seuil de contraction : longueur 3D, a 2r. Sur D4 segment, le morceau le plus court de
// chaque courbe (du sol a la jonction) a une longueur 3D l nettement superieure a sa
// longueur en plan. Avec l = 1,9 r il est contracte (une extremite libre identifiee a
// la jonction, 8 fois) ; avec l = 3 r, non, alors que sa longueur en plan est sous 2r.
TEST(TEST_cgarchi_gothic_micro_solid, network_graph_contracts_at_two_radii_of_3d_length)
{
    const Network net = network(loadFile(kD4Segment));
    ASSERT_EQ(net.junctions.size(), 4u);
    // Abscisse de la jonction sur la courbe 0.
    double sj = -1.0;
    for (const Junction &j : net.junctions)
        for (const Incidence &i : j.incidences)
            if (i.curve == 0)
                sj = i.s;
    ASSERT_GT(sj, 0.0);
    const SpaceCurve &c0 = net.curves[0];
    const double L = c0.profile.length;
    const double l1 = arcLength(c0, 0.0, sj), l2 = arcLength(c0, sj, L);
    const double shortest = std::min(l1, l2), plan = l1 < l2 ? sj : L - sj;
    ASSERT_GT(std::max(l1, l2), 1.1 * shortest);
    ASSERT_LT(plan, 2.0 / 3.0 * shortest);   // en plan, sous le seuil du cas a 3r
    RecordProperty("shortest_piece_3d", std::to_string(shortest));
    RecordProperty("shortest_piece_plan", std::to_string(plan));

    const NetworkGraph at19 = BuildNetworkGraph(net, 2.0 * shortest / 1.9);   // l = 1,9 r
    EXPECT_EQ(at19.vertices, 12u);
    EXPECT_EQ(at19.edges, 8u);
    EXPECT_EQ(at19.components, 4u);
    const NetworkGraph at3 = BuildNetworkGraph(net, 2.0 * shortest / 3.0);     // l = 3 r
    EXPECT_EQ(at3.vertices, 20u);
    EXPECT_EQ(at3.edges, 16u);
    EXPECT_EQ(at3.components, 4u);
}

// ---------------------------------------------------------------------------
//  J7-a : extraction (protections de la sous-classe d'ImplicitSurface)
// ---------------------------------------------------------------------------

// Champ aleatoire extrait en deux appels, un objet par appel : chaque sortie est saine.
TEST(TEST_cgarchi_gothic_micro_solid, random_field_extracted_twice_is_sound)
{
    const int lo[3] = {0, 0, 0}, hi[3] = {40, 40, 40};
    for (unsigned seed : {7u, 11u})
    {
        SCOPED_TRACE(seed);
        Spheres field = randomSpheres(seed);
        GridIsoExtractor e(&Spheres::eval, &field, std::size_t(1) << 22, std::size_t(1) << 28);
        ASSERT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::Ok);
        ASSERT_GT(e.Faces().size(), 0u);
        expectSound(validate(*meshOf(e), 1.0));
    }
}

// Un second Run sur le meme objet est refuse : l'accumulateur de l'extracteur (sommets,
// cles, faces) ne vaut que pour une grille.
// Sommet hors d'une arete de grille (injection : grille a x ~ 2^22, ou le float ne
// represente plus la fraction d'un sommet) : statut BadIndex, jamais Ok.
TEST(TEST_cgarchi_gothic_micro_solid, a_vertex_off_the_grid_edges_is_reported)
{
    Spheres field = randomSpheres(7);
    const double shift = 4194304.0;   // 2^22
    for (std::size_t i = 0; i + 3 < field.c.size(); i += 4)
        field.c[i] += shift;
    const int lo[3] = {4194304, 0, 0}, hi[3] = {4194304 + 40, 40, 40};
    GridIsoExtractor e(&Spheres::eval, &field, std::size_t(1) << 22, std::size_t(1) << 28);
    EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::BadIndex);
    EXPECT_STREQ(ExtractStatusName(GridIsoExtractor::Status::BadIndex), "bad_index");
}

TEST(TEST_cgarchi_gothic_micro_solid, an_extractor_runs_once)
{
    const int lo[3] = {0, 0, 0}, hi[3] = {40, 40, 40};
    Spheres field = randomSpheres(7);
    GridIsoExtractor e(&Spheres::eval, &field, std::size_t(1) << 22, std::size_t(1) << 28);
    ASSERT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::Ok);
    const std::size_t faces = e.Faces().size(), vertices = e.Vertices().size();
    EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::AlreadyUsed);
    EXPECT_EQ(e.Faces().size(), faces);
    EXPECT_EQ(e.Vertices().size(), vertices);
}

// Bornes avant extraction : noeuds par axe, noeuds au total. Aucun noeud n'est evalue.
// L'echec d'allocation des caches (CacheAllocation) : TU_faults.
TEST(TEST_cgarchi_gothic_micro_solid, extraction_bounds_are_checked_before_evaluation)
{
    Spheres field = randomSpheres(7);
    {
        const int lo[3] = {0, 0, 0}, hi[3] = {SolidLimits::max_nodes_per_axis, 1, 1};   // 2 049 noeuds en x
        GridIsoExtractor e(&Spheres::eval, &field, 1000, std::size_t(1) << 28);
        EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::NodesPerAxis);
        EXPECT_EQ(e.Evaluations(), 0u);
    }
    {
        const int lo[3] = {0, 0, 0}, hi[3] = {SolidLimits::max_nodes_per_axis - 1, 1, 1};   // 2 048 : accepte
        GridIsoExtractor e(&Spheres::eval, &field, 1000, std::size_t(1) << 28);
        EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::Ok);
    }
    {
        const int lo[3] = {0, 0, 0}, hi[3] = {40, 40, 40};
        GridIsoExtractor e(&Spheres::eval, &field, 1000, 41 * 41 * 41 - 1);
        EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::Nodes);
        EXPECT_EQ(e.Evaluations(), 0u);
    }
}

// ---------------------------------------------------------------------------
//  J7-a : solide sur grille complete (rho = 0,05, q = 3)
// ---------------------------------------------------------------------------

// P1-P5 sur les cinq cas (le champ aleatoire : test ci-dessus).
TEST(TEST_cgarchi_gothic_micro_solid, solids_are_closed_manifold_and_outward)
{
    struct Case { const char *name; MicroarchSpec spec; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment)}, {"c4_pinwheel", loadFile(kC4Pinwheel)},
                          {"star", star()}, {"needle", needle()}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network net = network(c.spec);
        MicroarchReport report;
        const SolidMesh s = BuildSolidMesh(net, solidParams(), report);
        ASSERT_TRUE(s.mesh) << ReportToJson(report);
        EXPECT_FALSE(report.HasErrors());
        const MeshValidationReport r = validate(*s.mesh, s.cell);
        RecordProperty(std::string(c.name) + "_triangles", std::to_string(s.mesh->GetNFaces()));
        expectSound(r);
    }
}

// P6 : composantes et somme des caracteristiques d'Euler du graphe ; P9 : chaque
// composante est posee sur la base, rien dessous.
TEST(TEST_cgarchi_gothic_micro_solid, solid_topology_follows_the_network_graph)
{
    struct Case { const char *name; Network net; double base; };
    const Case cases[] = {{"d4_segment", network(loadFile(kD4Segment)), 0.0},
                          {"c4_pinwheel", network(loadFile(kC4Pinwheel)), 0.0},
                          {"star", network(star()), 0.0},
                          {"chained", chainedNetwork(), 0.0},
                          {"d4_on_base_5", network(d4OnBase(5.0)), 5.0}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network &net = c.net;
        MicroarchReport report;
        const SolidMesh s = BuildSolidMesh(net, solidParams(), report);
        ASSERT_TRUE(s.mesh) << ReportToJson(report);
        // Attendu : graphe contracte a 2r (morceaux plus courts que 2r fusionnes).
        const NetworkGraph g = BuildNetworkGraph(net, 2 * s.radius);
        const MeshValidationReport r = validate(*s.mesh, s.cell);
        EXPECT_EQ(r.components, g.components);
        EXPECT_EQ(s.euler.size(), g.components);
        const long expected = 2 * (static_cast<long>(g.vertices) - static_cast<long>(g.edges));
        EXPECT_EQ(eulerOf(*s.mesh), expected);
        long sum = 0;
        for (long chi : s.euler)
            sum += chi;
        EXPECT_EQ(sum, expected);

        std::size_t count = 0;
        const std::vector<std::size_t> comp = vertexComponents(*s.mesh, count);
        ASSERT_EQ(count, g.components);
        std::vector<double> zmin(count, std::numeric_limits<double>::infinity());
        for (std::size_t i = 0; i < s.mesh->GetNVertices(); ++i)
            zmin[comp[i]] = std::min(zmin[comp[i]], drawingPoint(s, i).z);
        for (std::size_t k = 0; k < count; ++k)
            EXPECT_NEAR(zmin[k], c.base, 1e-9 * net.tol.scale) << "composante " << k;
    }
}

// Reseau a 1e6 au-dessus de la base (1e7 pas de grille) : l'origine est relevee
// jusqu'au bas du reseau, les coordonnees de grille restent exactes en float, le
// solide est sain, rien n'est sous la base.
TEST(TEST_cgarchi_gothic_micro_solid, a_network_far_above_the_base_stays_exact)
{
    const Network net = network(d4High());
    MicroarchReport report;
    const SolidMesh s = BuildSolidMesh(net, solidParams(), report);
    ASSERT_TRUE(s.mesh) << ReportToJson(report);
    EXPECT_FALSE(report.HasErrors());
    expectSound(validate(*s.mesh, s.cell));
    EXPECT_EQ(validate(*s.mesh, s.cell).components, BuildNetworkGraph(net, 2 * s.radius).components);
    double zmin = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < s.mesh->GetNVertices(); ++i)
        zmin = std::min(zmin, drawingPoint(s, i).z);
    EXPECT_GT(zmin, 1e6 - 2 * s.radius);
}

// Coordonnees de grille au-dela de 2^23 : refus nomme (solid.grid.coordinates).
TEST(TEST_cgarchi_gothic_micro_solid, grid_coordinates_beyond_2_23_are_refused)
{
    const std::vector<std::vector<Vector3d>> line = {{Vector3d(0, 0, 0), Vector3d(1e8, 0, 0)}};
    MicroarchReport report;
    EXPECT_FALSE(MeshTubeUnion(line, 1.0, 1.0, std::nullopt, report).mesh);
    ASSERT_EQ(report.ErrorCount(), 1u) << ReportToJson(report);
    EXPECT_EQ(report.Count(MicroarchError::SolidLimitExceeded), 1u);
    EXPECT_EQ(report.entries.back().path, "solid.grid.coordinates");
    EXPECT_NE(report.entries.back().message.find("2^23"), std::string::npos) << report.entries.back().message;
    EXPECT_NE(report.entries.back().message.find("axis 0"), std::string::npos) << report.entries.back().message;
}

// MeshTubeUnion : rayon, pas et polylignes invalides sont signales, jamais ignores.
TEST(TEST_cgarchi_gothic_micro_solid, mesh_tube_union_reports_invalid_input)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::vector<Vector3d>> line = {{Vector3d(0, 0, 0), Vector3d(0, 0, 10)}};
    struct Case { const char *name; std::vector<std::vector<Vector3d>> lines; double r, h; const char *path; };
    const Case cases[] = {{"r = 0", line, 0.0, 1.0, "solid.radius"},
                          {"r < 0", line, -1.0, 1.0, "solid.radius"},
                          {"r NaN", line, nan, 1.0, "solid.radius"},
                          {"h = 0", line, 2.0, 0.0, "solid.cell"},
                          {"h NaN", line, 2.0, nan, "solid.cell"},
                          {"no polyline", {}, 2.0, 1.0, "solid.polylines"},
                          {"single point", {{Vector3d(0, 0, 0)}}, 2.0, 1.0, "solid.polylines"},
                          {"non-finite point", {{Vector3d(0, 0, 0), Vector3d(nan, 0, 1)}}, 2.0, 1.0, "solid.polylines"}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        MicroarchReport report;
        EXPECT_FALSE(MeshTubeUnion(c.lines, c.r, c.h, std::nullopt, report).mesh);
        ASSERT_EQ(report.ErrorCount(), 1u) << ReportToJson(report);
        EXPECT_EQ(report.Count(MicroarchError::SolidInvalidParameter), 1u);
        EXPECT_EQ(report.entries.back().path, c.path);
    }
}

// P7 : ecart des sommets a r, hors des plis et partout ; paroi mesuree.
TEST(TEST_cgarchi_gothic_micro_solid, solid_walls_follow_the_radius)
{
    struct Case { const char *name; MicroarchSpec spec; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment)}, {"c4_pinwheel", loadFile(kC4Pinwheel)}, {"star", star()}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network net = network(c.spec);
        MicroarchReport report;
        SolidMesh s = BuildSolidMesh(net, solidParams(), report);
        ASSERT_TRUE(s.mesh) << ReportToJson(report);
        const Deviation d = deviation(s, densePolylines(net), 0.0);
        RecordProperty(std::string(c.name) + "_deviation_off_folds_over_r", std::to_string(d.offFolds / s.radius));
        RecordProperty(std::string(c.name) + "_deviation_everywhere_over_h", std::to_string(d.everywhere / s.cell));
        EXPECT_GT(d.offFoldVertices, s.mesh->GetNVertices() / 4);
        EXPECT_LE(d.offFolds, 0.024 * s.radius);
        EXPECT_LE(d.everywhere, s.cell);

        std::vector<float> thickness;
        std::vector<char> defined;
        ASSERT_TRUE(MeshAlgoThickness::ComputeWallThickness(*s.mesh, thickness, defined));
        // Paroi : hors des plis, comme l'ecart a r ; partout, relevee seulement (les
        // plis de jonction ont des cordes courtes la ou deux tubes se separent).
        double thinnest = std::numeric_limits<double>::infinity(), anywhere = thinnest;
        for (std::size_t i = 0; i < thickness.size(); ++i)
            if (defined[i] && drawingPoint(s, i).z >= s.cell)
            {
                anywhere = std::min(anywhere, static_cast<double>(thickness[i]));
                if (d.offFold[i])
                    thinnest = std::min(thinnest, static_cast<double>(thickness[i]));
            }
        RecordProperty(std::string(c.name) + "_thinnest_wall_off_folds_over_2r", std::to_string(thinnest / (2 * s.radius)));
        RecordProperty(std::string(c.name) + "_thinnest_wall_anywhere_over_2r", std::to_string(anywhere / (2 * s.radius)));
        EXPECT_GE(thinnest, 2 * s.radius * (1 - 0.06));
    }
}

// Instrument : ValidateMesh (soudure 1e-3 h) et l'ecart a r voient un maillage abime.
TEST(TEST_cgarchi_gothic_micro_solid, the_instruments_see_a_damaged_solid)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport report;
    const SolidMesh s = BuildSolidMesh(net, solidParams(), report);
    ASSERT_TRUE(s.mesh) << ReportToJson(report);
    std::vector<float> v = s.mesh->GetVertices();
    std::vector<unsigned int> t = s.mesh->GetTriangles();
    const unsigned int nv = static_cast<unsigned int>(v.size() / 3);

    {   // un triangle retire : bord (P1)
        std::vector<unsigned int> u(t.begin() + 3, t.end());
        Mesh m;
        m.SetVertices(nv, v.data());
        m.SetFaces(static_cast<unsigned int>(u.size() / 3), 3, u.data());
        EXPECT_GT(validate(m, s.cell).Count(MeshDefect::BoundaryEdge), 0u);
    }
    {   // un triangle retourne : orientation (P3)
        std::vector<unsigned int> u = t;
        std::swap(u[1], u[2]);
        Mesh m;
        m.SetVertices(nv, v.data());
        m.SetFaces(static_cast<unsigned int>(u.size() / 3), 3, u.data());
        EXPECT_GT(validate(m, s.cell).Count(MeshDefect::InconsistentOrientation), 0u);
    }
    {   // tout retourne : volume negatif (P3)
        std::vector<unsigned int> u = t;
        for (std::size_t f = 0; f + 2 < u.size(); f += 3)
            std::swap(u[f + 1], u[f + 2]);
        Mesh m;
        m.SetVertices(nv, v.data());
        m.SetFaces(static_cast<unsigned int>(u.size() / 3), 3, u.data());
        const MeshValidationReport r = validate(m, s.cell);
        EXPECT_GT(r.Count(MeshDefect::InvertedShell), 0u);
        EXPECT_LT(r.signedVolume, 0.0);
    }
    {   // un sommet en trop : non reference (P5)
        std::vector<float> w = v;
        w.insert(w.end(), {0.f, 0.f, 0.f});
        Mesh m;
        m.SetVertices(nv + 1, w.data());
        m.SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
        EXPECT_EQ(validate(m, s.cell).Count(MeshDefect::UnreferencedVertex), 1u);
    }
    {   // un sommet pousse de 2h vers l'exterieur : ecart a r (P7)
        SolidMesh moved;
        moved.mesh = std::make_unique<Mesh>();
        std::vector<float> w = v;
        std::size_t far = 0;
        for (std::size_t i = 0; i < nv; ++i)
            if (w[3 * i + 2] > w[3 * far + 2])
                far = i;
        w[3 * far + 2] += static_cast<float>(2 * s.cell);
        moved.mesh->SetVertices(nv, w.data());
        moved.mesh->SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
        moved.origin = s.origin;
        moved.radius = s.radius;
        moved.cell = s.cell;
        EXPECT_GT(deviation(moved, densePolylines(net), 0.0).everywhere, s.cell);
    }
}

// Profil aiguille et champ aleatoire : P1-P5 (test ci-dessus) ; marge : un segment
// vertical de rayon r = 2h exactement met des noeuds sur l'isosurface.
TEST(TEST_cgarchi_gothic_micro_solid, nodes_on_the_isosurface_give_no_degenerate_face)
{
    const std::vector<std::vector<Vector3d>> line = {{Vector3d(0, 0, 0), Vector3d(0, 0, 10)}};
    MicroarchReport report;
    const SolidMesh s = MeshTubeUnion(line, 2.0, 1.0, std::nullopt, report);
    ASSERT_TRUE(s.mesh) << ReportToJson(report);
    // Le noeud (2, 0, z) est a distance 2 de l'axe : sur l'isosurface.
    ASSERT_EQ(s.origin.x, 0.0);
    ASSERT_EQ(s.origin.y, 0.0);
    const MeshValidationReport r = validate(*s.mesh, s.cell);
    EXPECT_EQ(r.Count(MeshDefect::DegenerateFace), 0u);
    EXPECT_EQ(r.Count(MeshDefect::NonManifoldVertex), 0u);
    expectSound(r);
}

// M26 : le dessin translate de 1e6 donne le meme solide, a 1e-5 r pres.
TEST(TEST_cgarchi_gothic_micro_solid, solid_is_independent_of_the_drawing_position)
{
    const double dx = 1e6, dy = -1e6;
    MicroarchReport r0, r1;
    const SolidMesh a = BuildSolidMesh(network(loadFile(kD4Segment)), solidParams(), r0);
    const SolidMesh b = BuildSolidMesh(network(translated(loadFile(kD4Segment), dx, dy)), solidParams(), r1);
    ASSERT_TRUE(a.mesh && b.mesh);
    ASSERT_EQ(a.mesh->GetNFaces(), b.mesh->GetNFaces());
    ASSERT_EQ(a.mesh->GetNVertices(), b.mesh->GetNVertices());
    double worst = 0.0;
    for (std::size_t i = 0; i < a.mesh->GetNVertices(); ++i)
    {
        const Vector3d p = drawingPoint(a, i), q = drawingPoint(b, i);
        worst = std::max({worst, std::fabs(q.x - dx - p.x), std::fabs(q.y - dy - p.y), std::fabs(q.z - p.z)});
    }
    RecordProperty("translated_error_over_r", std::to_string(worst / a.radius));
    EXPECT_LE(worst, 1e-5 * a.radius);
}

// Refus : un diagnostic exact par cas, code et chemin ; maillage nul.
TEST(TEST_cgarchi_gothic_micro_solid, refusals_are_reported)
{
    const Network net = network(loadFile(kD4Segment));
    struct Case { const char *name; SolidParams p; SolidLimits l; MicroarchError code; const char *path; };
    SolidLimits fewNodes;
    fewNodes.max_grid_nodes = 1000;
    const Case cases[] = {
        {"q = 1", solidParams(0.05, 1), {}, MicroarchError::SolidInvalidParameter, "solid.cells_per_radius"},
        {"q = 9", solidParams(0.05, 9), {}, MicroarchError::SolidInvalidParameter, "solid.cells_per_radius"},
        {"rho = 0", solidParams(0.0, 3), {}, MicroarchError::SolidInvalidParameter, "solid.radius_ratio"},
        {"rho > 0,1", solidParams(0.1000001, 3), {}, MicroarchError::SolidInvalidParameter, "solid.radius_ratio"},
        {"axis", solidParams(0.005, 8), {}, MicroarchError::SolidLimitExceeded, "solid.grid.nodes_per_axis"},
        {"nodes", solidParams(0.05, 3), fewNodes, MicroarchError::SolidLimitExceeded, "solid.grid.nodes"},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        MicroarchReport report;
        const SolidMesh s = BuildSolidMesh(net, c.p, report, c.l);
        EXPECT_FALSE(s.mesh);
        ASSERT_EQ(report.ErrorCount(), 1u) << ReportToJson(report);
        EXPECT_EQ(report.Count(c.code), 1u);
        EXPECT_EQ(report.entries.back().path, c.path);
    }

    // Plafond de triangles : F - 1 refuse (controle cumule), F accepte.
    MicroarchReport ok;
    const SolidMesh full = BuildSolidMesh(net, solidParams(), ok);
    ASSERT_TRUE(full.mesh);
    SolidLimits tight;
    tight.max_triangles = full.mesh->GetNFaces() - 1;
    MicroarchReport refused;
    EXPECT_FALSE(BuildSolidMesh(net, solidParams(), refused, tight).mesh);
    ASSERT_EQ(refused.ErrorCount(), 1u) << ReportToJson(refused);
    EXPECT_EQ(refused.Count(MicroarchError::SolidLimitExceeded), 1u);
    EXPECT_EQ(refused.entries.back().path, "solid.triangles");
    tight.max_triangles = full.mesh->GetNFaces();
    MicroarchReport exact;
    EXPECT_TRUE(BuildSolidMesh(net, solidParams(), exact, tight).mesh);
}

// Plafond atteint tot (refus tardif, M32 b) : les cubes restants de la couche en cache ne
// doivent pas faire prendre le refus pour un sommet hors d'une arete de grille (solid.grid).
// Briques et grille complete, plafond a F / 2 et a F / 10.
TEST(TEST_cgarchi_gothic_micro_solid, a_triangle_cap_reached_early_is_reported_as_the_cap)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport ok;
    const SolidMesh full = BuildSolidMesh(net, solidParams(), ok);
    ASSERT_TRUE(full.mesh);
    for (const bool fullGrid : {false, true})
        for (const std::size_t divisor : {2u, 10u})
        {
            SCOPED_TRACE(::testing::Message() << (fullGrid ? "full grid" : "bricks") << ", F / " << divisor);
            SolidLimits tight;
            tight.full_grid = fullGrid;
            tight.max_triangles = full.mesh->GetNFaces() / divisor;
            MicroarchReport refused;
            EXPECT_FALSE(BuildSolidMesh(net, solidParams(), refused, tight).mesh);
            ASSERT_EQ(refused.ErrorCount(), 1u) << ReportToJson(refused);
            EXPECT_EQ(refused.Count(MicroarchError::SolidLimitExceeded), 1u);
            EXPECT_EQ(refused.entries.back().path, "solid.triangles") << ReportToJson(refused);
        }
}

// ---------------------------------------------------------------------------
//  J7-b : briques
// ---------------------------------------------------------------------------

// Briques et grille complete : memes triangles, au bit pres (rho = 0,05, q = 3).
TEST(TEST_cgarchi_gothic_micro_solid, bricks_equal_the_full_grid_bit_for_bit)
{
    SolidLimits full;
    full.full_grid = true;
    struct Case { const char *name; MicroarchSpec spec; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment)}, {"c4_pinwheel", loadFile(kC4Pinwheel)},
                          {"star", star()}, {"needle", needle()}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network net = network(c.spec);
        MicroarchReport r0, r1;
        const SolidMesh b = BuildSolidMesh(net, solidParams(), r0);
        const SolidMesh g = BuildSolidMesh(net, solidParams(), r1, full);
        ASSERT_TRUE(b.mesh && g.mesh) << ReportToJson(r0) << ReportToJson(r1);
        EXPECT_EQ(g.bricks, 1u);
        EXPECT_GT(b.bricks, 1u);
        EXPECT_EQ(b.mesh->GetNVertices(), g.mesh->GetNVertices());
        EXPECT_TRUE(sortedTriangles(*b.mesh) == sortedTriangles(*g.mesh));
        expectSound(validate(*b.mesh, b.cell));
    }
    // Segment vertical a r = 2h (noeuds sur l'isosurface), sur plusieurs briques.
    const std::vector<std::vector<Vector3d>> line = {{Vector3d(0, 0, 0), Vector3d(0, 0, 100)}};
    MicroarchReport r0, r1;
    const SolidMesh b = MeshTubeUnion(line, 2.0, 1.0, std::nullopt, r0);
    const SolidMesh g = MeshTubeUnion(line, 2.0, 1.0, std::nullopt, r1, full);
    ASSERT_TRUE(b.mesh && g.mesh);
    EXPECT_GT(b.active_bricks, 2u);
    EXPECT_TRUE(sortedTriangles(*b.mesh) == sortedTriangles(*g.mesh));
    expectSound(validate(*b.mesh, b.cell));
}

// Plafond de noeuds : sur les briques actives. D4 segment aux defauts : 8,5e8 noeuds en
// grille complete (refuse), accepte en briques ; un plafond sous les noeuds actifs refuse.
TEST(TEST_cgarchi_gothic_micro_solid, the_node_cap_counts_active_bricks)
{
    const Network net = network(loadFile(kD4Segment));
    SolidLimits full;
    full.full_grid = true;
    MicroarchReport refused;
    EXPECT_FALSE(BuildSolidMesh(net, solidParams(0.005, 3), refused, full).mesh);
    ASSERT_EQ(refused.ErrorCount(), 1u);
    EXPECT_EQ(refused.entries.back().path, "solid.grid.nodes");
    EXPECT_NE(refused.entries.back().message.find("grid nodes"), std::string::npos);

    SolidLimits tiny;
    tiny.max_grid_nodes = 35936 - 1;   // une brique de 33^3 noeuds
    MicroarchReport small;
    EXPECT_FALSE(BuildSolidMesh(net, solidParams(), small, tiny).mesh);
    ASSERT_EQ(small.ErrorCount(), 1u);
    EXPECT_EQ(small.entries.back().path, "solid.grid.nodes");
    EXPECT_NE(small.entries.back().message.find("active bricks"), std::string::npos);
}

// D4 segment aux defauts (rho = 0,005, q = 3) : P1-P7 et P9, F releve.
TEST(TEST_cgarchi_gothic_micro_solid, d4_segment_at_the_defaults)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport report;
    SolidMesh s = BuildSolidMesh(net, solidParams(0.005, 3), report);
    ASSERT_TRUE(s.mesh) << ReportToJson(report);
    RecordProperty("triangles", std::to_string(s.mesh->GetNFaces()));
    RecordProperty("bricks", std::to_string(s.bricks));
    RecordProperty("active_bricks", std::to_string(s.active_bricks));
    RecordProperty("evaluated_nodes", std::to_string(s.evaluated_nodes));
    EXPECT_NEAR(static_cast<double>(s.mesh->GetNFaces()), 4.9e5, 0.15 * 4.9e5);
    expectSound(validate(*s.mesh, s.cell));

    const NetworkGraph g = BuildNetworkGraph(net, 2 * s.radius);
    EXPECT_EQ(s.euler.size(), g.components);
    EXPECT_EQ(eulerOf(*s.mesh), 2 * (static_cast<long>(g.vertices) - static_cast<long>(g.edges)));

    std::size_t count = 0;
    const std::vector<std::size_t> comp = vertexComponents(*s.mesh, count);
    std::vector<double> zmin(count, std::numeric_limits<double>::infinity());
    for (std::size_t i = 0; i < s.mesh->GetNVertices(); ++i)
        zmin[comp[i]] = std::min(zmin[comp[i]], drawingPoint(s, i).z);
    for (std::size_t k = 0; k < count; ++k)
        EXPECT_NEAR(zmin[k], 0.0, 1e-9 * net.tol.scale) << "composante " << k;

    // P7 sur un sommet sur 50 (distance aux polylignes denses, en double).
    SolidMesh sample;
    sample.origin = s.origin;
    sample.radius = s.radius;
    sample.cell = s.cell;
    sample.mesh = std::make_unique<Mesh>();
    std::vector<float> picked;
    const std::vector<float> &v = s.mesh->GetVertices();
    for (std::size_t i = 0; i < s.mesh->GetNVertices(); i += 50)
        picked.insert(picked.end(), {v[3 * i], v[3 * i + 1], v[3 * i + 2]});
    sample.mesh->SetVertices(static_cast<unsigned int>(picked.size() / 3), picked.data());
    const Deviation d = deviation(sample, densePolylines(net), 0.0);
    RecordProperty("deviation_off_folds_over_r", std::to_string(d.offFolds / s.radius));
    RecordProperty("deviation_everywhere_over_h", std::to_string(d.everywhere / s.cell));
    EXPECT_GT(d.offFoldVertices, sample.mesh->GetNVertices() / 4);
    EXPECT_LE(d.offFolds, 0.024 * s.radius);
    EXPECT_LE(d.everywhere, s.cell);
}

// ---------------------------------------------------------------------------
//  M32 b : estimateur prealable
// ---------------------------------------------------------------------------

namespace
{
    // Etoile convergente de la revue de J7-c : un segment (4 ; 0,3) -> (0 ; 0) sous Dn, axe de
    // reflexion decale, soit 2n branches vers (0, 0, 8).
    MicroarchSpec convergingStar(int n)
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(n, 90.0 / n * 0.5 + 1.0);
        spec.points = {point("A", 4, 0.3, std::nullopt), point("B", 0, 0, 8.0)};
        spec.curves = {curve("l", spec, 0, 1, std::nullopt, false)};
        return spec;
    }

    // Reseau dense : generateur de probe6 (B courbes de base sous D64 avec reflexion).
    MicroarchSpec denseSpec(int B)
    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<double> ang(0.0, 2 * kPi), rad(1.0, 2.0), h(1.0, 3.0);
        MicroarchSpec spec;
        spec.symmetry = symmetry(64, 0.0);
        for (int i = 0; i < B; ++i)
        {
            const double a0 = ang(rng), r0 = rad(rng), a1 = a0 + 0.3, r1 = rad(rng);
            spec.points.push_back(point("P" + std::to_string(i), r0 * std::cos(a0), r0 * std::sin(a0), std::nullopt));
            spec.points.push_back(point("Q" + std::to_string(i), r1 * std::cos(a1), r1 * std::sin(a1), h(rng)));
            spec.curves.push_back(curve("l" + std::to_string(i), spec, 2 * i, 2 * i + 1, std::nullopt, false));
        }
        return spec;
    }
}

// L'estimation majore F de peu sur les references ; un cas a 83 % de son plafond n'est
// jamais refuse par elle ; un cas tres au-dela l'est, avant extraction.
TEST(TEST_cgarchi_gothic_micro_solid, the_triangle_estimate_never_refuses_a_passing_case)
{
    struct Case { const char *name; MicroarchSpec spec; };
    const Case cases[] = {{"d4_segment", loadFile(kD4Segment)}, {"c4_pinwheel", loadFile(kC4Pinwheel)}, {"star", star()}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        const Network net = network(c.spec);
        MicroarchReport report;
        const SolidMesh s = BuildSolidMesh(net, solidParams(), report);
        ASSERT_TRUE(s.mesh);
        const double F = s.mesh->GetNFaces(), estimate = static_cast<double>(EstimateSolidTriangles(net, solidParams()));
        RecordProperty(std::string(c.name) + "_estimate_over_F", std::to_string(estimate / F));
        EXPECT_GE(estimate, F);

        SolidLimits at83;
        at83.max_triangles = static_cast<std::size_t>(std::ceil(F / 0.83));
        MicroarchReport passing;
        EXPECT_TRUE(BuildSolidMesh(net, solidParams(), passing, at83).mesh) << ReportToJson(passing);

        // Plafond sous F : refus par le plafond exact, pendant l'extraction, jamais par l'estimation.
        SolidLimits low;
        low.max_triangles = static_cast<std::size_t>(F) - 1;
        MicroarchReport refused;
        EXPECT_FALSE(BuildSolidMesh(net, solidParams(), refused, low).mesh);
        ASSERT_EQ(refused.ErrorCount(), 1u);
        EXPECT_EQ(refused.entries.back().path, "solid.triangles");
        EXPECT_EQ(refused.entries.back().message.find("estimated"), std::string::npos);
    }
    // Etoile convergente de la revue (128 branches vers un sommet) : l'estimation compte
    // chaque recouvrement et depasse le plafond emule de 200 000, F reel non ; le cas passe,
    // avec l'information solid_triangle_estimate_high.
    {
        const Network net = network(convergingStar(64));
        SolidLimits cap;
        cap.max_triangles = 200000;
        MicroarchReport report;
        const SolidMesh s = BuildSolidMesh(net, solidParams(), report, cap);
        ASSERT_TRUE(s.mesh) << ReportToJson(report);
        const double estimate = static_cast<double>(EstimateSolidTriangles(net, solidParams()));
        RecordProperty("converging_star_estimate_over_F", std::to_string(estimate / s.mesh->GetNFaces()));
        EXPECT_LE(s.mesh->GetNFaces(), 200000u);
        EXPECT_GT(estimate, 2.0 * 200000);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidTriangleEstimateHigh), 1u);
    }
    // Reseau dense, 256 courbes, rho = 0,008 : F = 3 492 944 (83 % du plafond, mesure de
    // J7-b, plan §14.12). L'estimation, qui majore F de 48 %, reste sous la marge.
    const Network dense = network(denseSpec(2));
    ASSERT_EQ(dense.curves.size(), 256u);
    const double estimate = static_cast<double>(EstimateSolidTriangles(dense, solidParams(0.008, 3)));
    RecordProperty("dense_estimate", std::to_string(estimate));
    EXPECT_GT(estimate, 3492944.0);
}

// ---------------------------------------------------------------------------
//  J7-c : verdict d'imprimabilite (M31, M32)
// ---------------------------------------------------------------------------

namespace
{
    PrintConstraints printer(double mmPerUnit, double minWallMm)
    {
        PrintConstraints p;
        p.mmPerUnit = mmPerUnit;
        p.minWallMm = minWallMm;
        return p;
    }

    // Segment eloigne, du sol a 3, qui porte D a ~25 : les petits motifs ci-dessous restent
    // sous la borne de rayon de M21 (r <= 0,1 D). Composante a part, posee.
    void addFarSegment(MicroarchSpec &spec)
    {
        const int k = static_cast<int>(spec.points.size());
        spec.points.push_back(point("F0", 15, 0, std::nullopt));
        spec.points.push_back(point("F1", 25, 0, 3.0));
        spec.curves.push_back(curve("far", spec, k, k + 1, std::nullopt, false));
    }

    // Deux segments paralleles a `gap` l'un de l'autre ; le second part du sol ou de `raised`.
    MicroarchSpec parallelPair(double gap, std::optional<double> raised)
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A0", 0, 0, std::nullopt), point("A1", 0, 4, 6.0),
                       point("B0", gap, 0, raised), point("B1", gap, 4, raised ? *raised + 6.0 : 6.0)};
        spec.curves = {curve("a", spec, 0, 1, std::nullopt, false), curve("b", spec, 2, 3, std::nullopt, false)};
        addFarSegment(spec);
        return spec;
    }

    // Triangle en plan de cote 1, jonctions aux trois coins.
    MicroarchSpec triangleCell()
    {
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A", 0, 0, std::nullopt), point("B", 1, 0, 1.0), point("C", 0.5, std::sqrt(0.75), 1.5)};
        spec.curves = {curve("ab", spec, 0, 1, std::nullopt, false), curve("ac", spec, 0, 2, std::nullopt, false),
                       curve("bc", spec, 1, 2, std::nullopt, false)};
        addFarSegment(spec);
        return spec;
    }

    // Epingle : arc de rayon 0,3 balayant 340 degres, du sol a 0,3 : ses bouts sont a ~0,3
    // l'un de l'autre, a ~1,8 le long de la courbe.
    MicroarchSpec hairpin()
    {
        const double a = 340.0 * kPi / 180.0;
        MicroarchSpec spec;
        spec.symmetry = symmetry(1, std::nullopt);
        spec.points = {point("A", 0.3, 0, std::nullopt), point("B", 0.3 * std::cos(a), 0.3 * std::sin(a), 0.3)};
        spec.curves = {curve("l", spec, 0, 1, Vector2d(0, 0), true)};
        addFarSegment(spec);
        return spec;
    }

    SolidPrintVerdict verdictOf(const Network &net, const SolidParams &sp, const PrintConstraints *pc, MicroarchReport &report,
                                const SolidPrintOptions &options = SolidPrintOptions{})
    {
        MicroarchReport build;
        const SolidMesh s = BuildSolidMesh(net, sp, build);
        EXPECT_TRUE(s.mesh) << ReportToJson(build);
        if (!s.mesh)
            return SolidPrintVerdict{};
        return CheckSolidPrintable(s, net, pc, report, options);
    }
}

// Echelle (M31 a) : 2r >= minWallMm, condition exacte. 2r = 0,4 mm accepte, 0,399 refuse ;
// sans echelle, jamais imprimable.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_wall_is_two_radii_against_min_wall)
{
    const Network net = network(loadFile(kD4Segment));
    const double r = 0.05 * net.tol.scale;
    {
        const PrintConstraints pc = printer(0.4 / (2 * r), 0.4);
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(), &pc, report);
        EXPECT_TRUE(v.generator_ok) << ReportToJson(report);
        EXPECT_TRUE(v.printable) << ReportToJson(report);
        EXPECT_NEAR(v.wall_mm, 0.4, 1e-12);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidWallTooThin), 0u);
        RecordProperty("d4_off_fork_wall_over_2r", std::to_string(v.off_fork_wall.value_or(-1.0)));
        RecordProperty("d4_c_max", std::to_string(v.c_max));
    }
    {
        const PrintConstraints pc = printer(0.399 / (2 * r), 0.4);
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(), &pc, report);
        EXPECT_TRUE(v.generator_ok);
        EXPECT_FALSE(v.printable);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidWallTooThin), 1u);
        // Echelle minimale donnee dans le message : minWallMm / 2r (revue de J7-d, LOW-3).
        std::string wall;
        for (const MicroarchDiagnostic &d : report.entries)
            if (d.path == "solid.radius_ratio")
                wall = d.message;
        const std::size_t at = wall.find("minimum scale ");
        ASSERT_NE(at, std::string::npos) << wall;
        EXPECT_NEAR(std::strtod(wall.c_str() + at + 14, nullptr), 0.4 / (2 * r), 1e-9 * 0.4 / (2 * r)) << wall;
    }
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(), nullptr, report);
        EXPECT_TRUE(v.generator_ok);
        EXPECT_FALSE(v.scale_set);
        EXPECT_FALSE(v.printable);
    }
}

// Fourches (M31 a) : l'etoile a le meme verdict a q = 3 et a q = 8 ; le col est une
// information localisee, pres de la jonction.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_of_the_star_does_not_depend_on_q)
{
    const Network net = network(star());
    const double r = 0.05 * net.tol.scale;
    for (const double minWall : {0.8, 1.01})
        for (const int q : {3, 8})
        {
            SCOPED_TRACE(::testing::Message() << "minWallMm " << minWall << ", q " << q);
            const PrintConstraints pc = printer(1.0 / (2 * r), minWall);   // 2r = 1 mm
            MicroarchReport report;
            const SolidPrintVerdict v = verdictOf(net, solidParams(0.05, q), &pc, report);
            EXPECT_TRUE(v.generator_ok) << ReportToJson(report);
            EXPECT_EQ(v.printable, minWall < 1.0) << ReportToJson(report);
            ASSERT_TRUE(v.fork_neck.has_value());
            EXPECT_EQ(report.Count(MicroarchWarning::SolidForkNeck), 1u);
            // Les branches arrivent verticales a la jonction (§12.2) : elles se separent loin
            // d'elle, la ou leurs axes atteignent 2r.
            const Vector3d j = net.junctions[0].position;
            const double dx = v.fork_neck_at.x - j.x, dy = v.fork_neck_at.y - j.y, dz = v.fork_neck_at.z - j.z;
            const double away = std::sqrt(dx * dx + dy * dy + dz * dz);
            RecordProperty("star_q" + std::to_string(q) + "_neck_from_junction_over_r", std::to_string(away / r));
            EXPECT_LT(away, 10 * r);
            EXPECT_GT(v.fork_neck_at.z, 2 * r);
            RecordProperty("star_q" + std::to_string(q) + "_neck_over_2r", std::to_string(*v.fork_neck / (2 * r)));
        }
}

// Appui (P9) : D4 sans point au sol, 4 composantes flottantes ; deux segments dont un
// seul part du sol, une seule.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_reports_each_floating_component)
{
    const PrintConstraints pc = printer(1.0, 0.01);
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(network(d4Raised()), solidParams(), &pc, report);
        EXPECT_EQ(v.floating.size(), 4u);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidFloatingComponent), 4u);
        EXPECT_FALSE(v.printable);
    }
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(network(parallelPair(3.0, 1.0)), solidParams(0.01, 3), &pc, report);
        EXPECT_EQ(v.components, 3u);
        EXPECT_EQ(v.floating.size(), 1u);
        EXPECT_FALSE(v.printable);
    }
}

// Fusion (M31 b) : deux composantes du graphe soudees par le solide sont un refus ; plus
// fines, elles restent separees et l'objet est imprimable.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_refuses_a_fusion_of_components)
{
    const Network net = network(parallelPair(1.0, std::nullopt));
    const double D = net.tol.scale;
    const PrintConstraints pc = printer(1.0, 0.01);
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(0.6 / D, 3), &pc, report);   // 2r = 1,2 > 1
        EXPECT_EQ(v.expected_components, 3u);   // la paire et le segment eloigne
        EXPECT_EQ(v.components, 2u);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidUnplannedFusion), 1u);
        EXPECT_FALSE(v.printable);
    }
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(0.3 / D, 3), &pc, report);   // 2r = 0,6 < 1
        EXPECT_EQ(v.components, 3u);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidUnplannedFusion), 0u);
        EXPECT_TRUE(v.printable) << ReportToJson(report);
    }
}

// Cellule fermee (M31 b) : triangle de cote ~2,5r, information localisee, imprimable ;
// cote ~10r, aucune information.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_reports_a_closed_cell_as_information)
{
    const Network net = network(triangleCell());
    const double D = net.tol.scale;
    const PrintConstraints pc = printer(1.0, 0.01);
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(0.4 / D, 3), &pc, report);   // cote 1 = 2,5 r
        EXPECT_EQ(v.components, 2u);   // le triangle et le segment eloigne
        EXPECT_EQ(v.closed_cells.size(), 1u);    // chi 2 au lieu de 0
        EXPECT_EQ(v.cavities, 1u);               // la cellule fermee de toutes parts
        EXPECT_EQ(report.Count(MicroarchWarning::SolidClosedCell), 1u);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidSealedCavity), 1u);
        EXPECT_TRUE(v.printable) << ReportToJson(report);
    }
    {
        MicroarchReport report;
        const SolidPrintVerdict v = verdictOf(net, solidParams(0.1 / D, 3), &pc, report);   // cote 1 = 10 r
        EXPECT_TRUE(v.closed_cells.empty()) << ReportToJson(report);
        EXPECT_TRUE(v.printable) << ReportToJson(report);
    }
}

// Enveloppe : trop petite, non imprimable.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_checks_the_envelope)
{
    PrintConstraints pc = printer(1.0, 0.01);
    pc.envelopeMm = std::array<double, 3>{1.0, 1.0, 1.0};
    MicroarchReport report;
    const SolidPrintVerdict v = verdictOf(network(loadFile(kD4Segment)), solidParams(), &pc, report);
    EXPECT_TRUE(v.generator_ok);
    EXPECT_GT(v.envelope.Count(PrintDefect::EnvelopeExceeded), 0u);
    EXPECT_FALSE(v.printable);
}

// Epingle : arc de rayon 0,3 sous un tube de rayon 0,5 ; le pli d'un meme morceau est une
// fourche (auto-approche a plus de pi r le long de la courbe), pas un defaut du generateur.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_treats_a_hairpin_fold_as_a_fork)
{
    const Network net = network(hairpin());
    const double D = net.tol.scale;
    const PrintConstraints pc = printer(1.0, 0.01);
    MicroarchReport report;
    const SolidPrintVerdict v = verdictOf(net, solidParams(0.5 / D, 3), &pc, report);
    RecordProperty("hairpin_off_fork_wall_over_2r", std::to_string(v.off_fork_wall.value_or(-1.0)));
    RecordProperty("hairpin_fork_vertices", std::to_string(v.fork_vertices));
    EXPECT_TRUE(v.generator_ok) << ReportToJson(report);
    EXPECT_GT(v.fork_vertices, 0u);
    EXPECT_TRUE(v.printable) << ReportToJson(report);
}

// M33 : au-dela du seuil de verification, le verdict n'est pas rendu (« non verifie »).
TEST(TEST_cgarchi_gothic_micro_solid, verdict_beyond_the_verification_threshold_is_unverified)
{
    const PrintConstraints pc = printer(1.0, 0.01);
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport build;
    const SolidMesh s = BuildSolidMesh(net, solidParams(), build);
    ASSERT_TRUE(s.mesh);
    SolidPrintOptions limit;
    limit.max_verified_triangles = s.mesh->GetNFaces() - 1;
    MicroarchReport report;
    const SolidPrintVerdict v = CheckSolidPrintable(s, net, &pc, report, limit);
    EXPECT_FALSE(v.verified);
    EXPECT_FALSE(v.printable);
    EXPECT_EQ(report.Count(MicroarchWarning::SolidUnverified), 1u);
    limit.max_verified_triangles = s.mesh->GetNFaces();
    MicroarchReport full;
    const SolidPrintVerdict w = CheckSolidPrintable(s, net, &pc, full, limit);
    EXPECT_TRUE(w.verified);
    EXPECT_TRUE(w.printable) << ReportToJson(full);
}

// Controle de discretisation sans objet : etoile de 32 branches a q = 2, tous les sommets
// sont en zone de fourche. La paroi est absente, pas 0.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_says_when_the_discretization_check_has_nothing_to_measure)
{
    const PrintConstraints pc = printer(1.0, 0.01);
    MicroarchReport report;
    const SolidPrintVerdict v = verdictOf(network(convergingStar(16)), solidParams(0.05, 2), &pc, report);
    RecordProperty("star16_off_fork_vertices", std::to_string(v.off_fork_vertices));
    EXPECT_EQ(v.off_fork_vertices, 0u);
    EXPECT_FALSE(v.off_fork_wall.has_value());
    EXPECT_EQ(report.Count(MicroarchWarning::SolidDiscretizationUnchecked), 1u);
}

// Composante retournee mais contenue dans aucune autre : un defaut du generateur, pas une
// cavite. D4 segment, une des quatre croix (en zone de fourche a sa jonction) retournee.
TEST(TEST_cgarchi_gothic_micro_solid, an_inverted_component_outside_any_other_is_a_defect)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport build;
    const SolidMesh s = BuildSolidMesh(net, solidParams(), build);
    ASSERT_TRUE(s.mesh);
    std::size_t count = 0;
    const std::vector<std::size_t> comp = vertexComponents(*s.mesh, count);
    ASSERT_EQ(count, 4u);
    std::vector<unsigned int> t = s.mesh->GetTriangles();
    for (std::size_t f = 0; f + 2 < t.size(); f += 3)
        if (comp[t[f]] == 0)
            std::swap(t[f + 1], t[f + 2]);
    SolidMesh flipped;
    flipped.mesh = std::make_unique<Mesh>();
    std::vector<float> v = s.mesh->GetVertices();
    flipped.mesh->SetVertices(static_cast<unsigned int>(v.size() / 3), v.data());
    flipped.mesh->SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
    flipped.origin = s.origin;
    flipped.radius = s.radius;
    flipped.cell = s.cell;
    const PrintConstraints pc = printer(1.0, 0.01);
    MicroarchReport report;
    const SolidPrintVerdict verdict = CheckSolidPrintable(flipped, net, &pc, report);
    EXPECT_EQ(verdict.cavities, 0u);
    EXPECT_FALSE(verdict.generator_ok);
    EXPECT_GE(report.Count(MicroarchError::SolidGeneratorDefect), 1u);
    EXPECT_EQ(report.Count(MicroarchWarning::SolidSealedCavity), 0u);
    EXPECT_FALSE(verdict.printable);
}

// Scission : le solide d'une paire de segments contre le reseau ou un pont les relie ; le
// solide a plus de composantes que le graphe, refus distinct de la fusion.
TEST(TEST_cgarchi_gothic_micro_solid, verdict_refuses_a_split_distinctly_from_a_fusion)
{
    MicroarchSpec bridged = parallelPair(3.0, std::nullopt);
    const int a0 = 0, b1 = 3;
    bridged.curves.push_back(curve("bridge", bridged, a0, b1, std::nullopt, false));
    const Network joined = network(bridged);
    const Network apart = network(parallelPair(3.0, std::nullopt));
    const double D = joined.tol.scale;
    MicroarchReport build;
    const SolidMesh s = BuildSolidMesh(apart, solidParams(0.3 / D, 3), build);
    ASSERT_TRUE(s.mesh);
    const PrintConstraints pc = printer(1.0, 0.01);
    MicroarchReport report;
    const SolidPrintVerdict v = CheckSolidPrintable(s, joined, &pc, report);
    EXPECT_EQ(v.expected_components, 2u);
    EXPECT_EQ(v.components, 3u);
    EXPECT_TRUE(v.split);
    EXPECT_FALSE(v.fusion);
    EXPECT_EQ(report.Count(MicroarchWarning::SolidUnplannedSplit), 1u);
    EXPECT_EQ(report.Count(MicroarchWarning::SolidUnplannedFusion), 0u);
    EXPECT_FALSE(v.printable);
}

namespace
{
    // Cube unite [0, 1]^3, normales exterieures. Chaque face est coupee par une diagonale qui
    // passe par son centre : un rayon axial issu du centre du cube la traverse sur une arete.
    void unitCube(std::vector<float> &v, std::vector<unsigned int> &t)
    {
        v.clear();
        for (int i = 0; i < 8; ++i)
        {
            v.push_back(static_cast<float>(i & 1));
            v.push_back(static_cast<float>((i >> 1) & 1));
            v.push_back(static_cast<float>((i >> 2) & 1));
        }
        t = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5, 0, 1, 5, 0, 5, 4, 2, 6, 7, 2, 7, 3};
    }

    // Octaedre de centre c et de demi-diagonale a, normales exterieures si outward.
    void appendOctahedron(std::vector<float> &v, std::vector<unsigned int> &t, const Vector3d &c, double a, bool outward)
    {
        const unsigned int k = static_cast<unsigned int>(v.size() / 3);
        const double d[6][3] = {{a, 0, 0}, {-a, 0, 0}, {0, a, 0}, {0, -a, 0}, {0, 0, a}, {0, 0, -a}};
        for (const auto &p : d)
        {
            v.push_back(static_cast<float>(c.x + p[0]));
            v.push_back(static_cast<float>(c.y + p[1]));
            v.push_back(static_cast<float>(c.z + p[2]));
        }
        const unsigned int f[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for (const auto &x : f)
        {
            t.push_back(k + x[0]);
            t.push_back(k + (outward ? x[1] : x[2]));
            t.push_back(k + (outward ? x[2] : x[1]));
        }
    }

    double signedVolumeOf(const std::vector<float> &v, const std::vector<unsigned int> &t, const std::vector<std::size_t> &comp,
                          std::size_t c)
    {
        double vol = 0.0;
        for (std::size_t f = 0; f + 2 < t.size(); f += 3)
            if (comp[t[f]] == c)
            {
                const float *a = &v[3 * t[f]], *b = &v[3 * t[f + 1]], *d = &v[3 * t[f + 2]];
                vol += (double(a[0]) * (double(b[1]) * d[2] - double(b[2]) * d[1]) - double(a[1]) * (double(b[0]) * d[2] - double(b[2]) * d[0])
                        + double(a[2]) * (double(b[0]) * d[1] - double(b[1]) * d[0])) / 6.0;
            }
        return vol;
    }
}

// Enroulement (contre-revue LOW-N2) : un rayon qui passe sur une arete commune a deux triangles
// la compterait deux fois avec des bornes barycentriques inclusives ; la direction est ecartee.
TEST(TEST_cgarchi_gothic_micro_solid, winding_number_never_counts_an_edge_twice)
{
    std::vector<float> v;
    std::vector<unsigned int> t;
    unitCube(v, t);
    const Vector3d centre(0.5, 0.5, 0.5);
    const double k = 1.0 / std::sqrt(3.0);
    const Vector3d up(0, 0, 1), side(1, 0, 0), diagonal(k, k, k);
    // +z sort par la diagonale du dessus, +x par celle de la face x = 1, (1,1,1) par un sommet.
    EXPECT_FALSE(WindingNumber(v, t, {}, centre, {up}).has_value());
    EXPECT_FALSE(WindingNumber(v, t, {}, centre, {diagonal}).has_value());
    EXPECT_EQ(WindingNumber(v, t, {}, centre, {up, side, WindingDirections()[0], WindingDirections()[1]}), std::optional<long>(1));
    EXPECT_EQ(WindingNumber(v, t, {}, centre), std::optional<long>(1));
    EXPECT_EQ(WindingNumber(v, t, {}, Vector3d(2.0, 0.5, 0.5)), std::optional<long>(0));
    // Retourne : -1 ; triangles exclus : 0.
    std::vector<unsigned int> flipped = t;
    for (std::size_t f = 0; f + 2 < flipped.size(); f += 3)
        std::swap(flipped[f + 1], flipped[f + 2]);
    EXPECT_EQ(WindingNumber(v, flipped, {}, centre), std::optional<long>(-1));
    EXPECT_EQ(WindingNumber(v, t, std::vector<bool>(t.size() / 3, false), centre), std::optional<long>(0));
    // Surface ouverte (dessus retire) : deux directions propres en desaccord, aucun resultat.
    std::vector<unsigned int> openCube(t.begin(), t.begin() + 6);
    openCube.insert(openCube.end(), t.begin() + 12, t.end());
    const Vector3d throughTop(0.0137, 0.0071, 0.9998), throughSide(0.9998, 0.0137, 0.0071);
    EXPECT_FALSE(WindingNumber(v, openCube, {}, centre, {throughTop, throughSide}).has_value());
    EXPECT_EQ(WindingNumber(v, openCube, {}, centre, {throughSide, throughSide}), std::optional<long>(1));
}

// Ilot retourne dans une cavite (contre-revue LOW-N1) : triangle de cote 2,5r (une cavite V dans
// la matiere A) plus un octaedre retourne B au milieu de V. B a un enroulement 0 (A +1, V -1) :
// ce n'est pas une cavite, son InvertedShell est un defaut. Une parite des seules composantes de
// volume positif le prendrait pour une seconde cavite, toleree, et soustraite au test d'appui.
TEST(TEST_cgarchi_gothic_micro_solid, an_inverted_island_inside_a_cavity_is_not_a_cavity)
{
    const Network net = network(triangleCell());
    const double D = net.tol.scale;
    MicroarchReport build;
    const SolidMesh s = BuildSolidMesh(net, solidParams(0.4 / D, 3), build);
    ASSERT_TRUE(s.mesh);
    std::vector<float> v = s.mesh->GetVertices();
    std::vector<unsigned int> t = s.mesh->GetTriangles();
    std::size_t count = 0;
    const std::vector<std::size_t> comp = vertexComponents(*s.mesh, count);
    std::size_t cavity = count;
    for (std::size_t c = 0; c < count; ++c)
        if (signedVolumeOf(v, t, comp, c) < 0.0)
            cavity = c;
    ASSERT_LT(cavity, count);
    // B au centre des sommets de V, demi-diagonale le quart de leur distance minimale au centre.
    double gx = 0.0, gy = 0.0, gz = 0.0;
    std::size_t n = 0;
    for (std::size_t i = 0; i < comp.size(); ++i)
        if (comp[i] == cavity)
        {
            gx += v[3 * i];
            gy += v[3 * i + 1];
            gz += v[3 * i + 2];
            ++n;
        }
    const Vector3d g(gx / n, gy / n, gz / n);
    double reach = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < comp.size(); ++i)
        if (comp[i] == cavity)
            reach = std::min(reach, std::sqrt((v[3 * i] - g.x) * (v[3 * i] - g.x) + (v[3 * i + 1] - g.y) * (v[3 * i + 1] - g.y)
                                              + (v[3 * i + 2] - g.z) * (v[3 * i + 2] - g.z)));
    ASSERT_GT(reach, 0.0);
    // Precondition : g est dans le vide de V (A +1, V -1).
    ASSERT_EQ(WindingNumber(v, t, {}, g), std::optional<long>(0));
    const std::size_t solidFaces = t.size() / 3;
    appendOctahedron(v, t, g, 0.25 * reach, false);
    std::vector<bool> onlyB(t.size() / 3, false);
    for (std::size_t f = solidFaces; f < onlyB.size(); ++f)
        onlyB[f] = true;
    ASSERT_EQ(WindingNumber(v, t, onlyB, g), std::optional<long>(-1));   // B retourne

    SolidMesh withIsland;
    withIsland.mesh = std::make_unique<Mesh>();
    withIsland.mesh->SetVertices(static_cast<unsigned int>(v.size() / 3), v.data());
    withIsland.mesh->SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
    withIsland.origin = s.origin;
    withIsland.radius = s.radius;
    withIsland.cell = s.cell;
    const PrintConstraints pc = printer(1.0, 0.01);
    MicroarchReport report;
    const SolidPrintVerdict verdict = CheckSolidPrintable(withIsland, net, &pc, report);
    EXPECT_EQ(verdict.cavities, 1u) << ReportToJson(report);
    EXPECT_EQ(report.Count(MicroarchWarning::SolidSealedCavity), 1u);
    EXPECT_EQ(verdict.validation.counts[static_cast<std::size_t>(MeshDefect::InvertedShell)], 2u);
    EXPECT_FALSE(verdict.generator_ok);
    EXPECT_GE(report.Count(MicroarchError::SolidGeneratorDefect), 1u);
    EXPECT_FALSE(verdict.printable);
    // Sans l'ilot, la meme cavite seule est toleree.
    MicroarchReport plain;
    const SolidPrintVerdict alone = CheckSolidPrintable(s, net, &pc, plain);
    EXPECT_EQ(alone.cavities, 1u);
    EXPECT_TRUE(alone.generator_ok) << ReportToJson(plain);
}

// Enroulement, cas limites (revue de J7-d, LOW-4) : rayon dans le plan d'un triangle, impact a
// 1e-6 d'une arete (compte normalement), depart sur un triangle.
TEST(TEST_cgarchi_gothic_micro_solid, winding_number_handles_grazing_near_edge_and_on_surface)
{
    // Triangle seul dans z = 0 ; deux rayons du plan qui le traversent : ambigus, aucun resultat.
    const std::vector<float> flat = {0, 0, 0, 2, 0, 0, 0, 2, 0};
    const std::vector<unsigned int> one = {0, 1, 2};
    const double l = std::sqrt(1.01);
    EXPECT_FALSE(WindingNumber(flat, one, {}, Vector3d(-1.0, 0.5, 0.0), {Vector3d(1, 0, 0), Vector3d(1 / l, 0.1 / l, 0)}).has_value());
    // Le meme triangle vu d'au-dessus : traverse normalement (-1, entree contre la normale +z).
    EXPECT_EQ(WindingNumber(flat, one, {}, Vector3d(0.5, 0.5, 1.0), {Vector3d(0, 0, -1), Vector3d(0, 0, -1)}), std::optional<long>(-1));

    std::vector<float> v;
    std::vector<unsigned int> t;
    unitCube(v, t);
    // Sortie par le dessus a 1e-6 (barycentrique) de sa diagonale : impact propre, compte 1.
    const Vector3d nearA(2e-6, 0.0, 1.0), nearB(3e-6, 0.0, 1.0);
    const auto unit = [](const Vector3d &d) {
        const double n = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        return Vector3d(d.x / n, d.y / n, d.z / n);
    };
    EXPECT_EQ(WindingNumber(v, t, {}, Vector3d(0.5, 0.5, 0.5), {unit(nearA), unit(nearB)}), std::optional<long>(1));
    // Depart sur la face du dessus, hors de sa diagonale : point de la surface, aucun resultat.
    EXPECT_FALSE(WindingNumber(v, t, {}, Vector3d(0.25, 0.75, 1.0)).has_value());
}

namespace
{
    // Solide de D4 (rho = 0,05) augmente de `extra` (sommets dans le repere du maillage, triangles
    // indexes a partir de 0 dans `extra`).
    SolidMesh withExtra(const SolidMesh &s, const std::vector<float> &ev, const std::vector<unsigned int> &et)
    {
        std::vector<float> v = s.mesh->GetVertices();
        std::vector<unsigned int> t = s.mesh->GetTriangles();
        const unsigned int k = static_cast<unsigned int>(v.size() / 3);
        v.insert(v.end(), ev.begin(), ev.end());
        for (unsigned int i : et)
            t.push_back(k + i);
        SolidMesh out;
        out.mesh = std::make_unique<Mesh>();
        out.mesh->SetVertices(static_cast<unsigned int>(v.size() / 3), v.data());
        out.mesh->SetFaces(static_cast<unsigned int>(t.size() / 3), 3, t.data());
        out.origin = s.origin;
        out.radius = s.radius;
        out.cell = s.cell;
        return out;
    }

    void appendCube(std::vector<float> &v, std::vector<unsigned int> &t, const Vector3d &lo, double side, bool outward)
    {
        std::vector<float> cv;
        std::vector<unsigned int> ct;
        unitCube(cv, ct);
        const unsigned int k = static_cast<unsigned int>(v.size() / 3);
        for (std::size_t i = 0; i < cv.size(); i += 3)
        {
            v.push_back(static_cast<float>(lo.x + side * cv[i]));
            v.push_back(static_cast<float>(lo.y + side * cv[i + 1]));
            v.push_back(static_cast<float>(lo.z + side * cv[i + 2]));
        }
        for (std::size_t f = 0; f + 2 < ct.size(); f += 3)
        {
            t.push_back(k + ct[f]);
            t.push_back(k + (outward ? ct[f + 1] : ct[f + 2]));
            t.push_back(k + (outward ? ct[f + 2] : ct[f + 1]));
        }
    }
}

// Coquilles retournees dont l'enroulement n'est pas 1 (revue de J7-d, MEDIUM-2 et LOW-4) : ce
// sont des defauts, jamais des cavites.
// - Enroulement ambigu (nullopt) : le premier sommet de l'octaedre retourne B est sur un triangle
//   ouvert voisin ; toute direction part d'un triangle.
// - Enroulement 2 : B dans deux cubes positifs emboites.
TEST(TEST_cgarchi_gothic_micro_solid, inverted_shells_with_an_ambiguous_or_doubled_winding_are_defects)
{
    const Network net = network(loadFile(kD4Segment));
    MicroarchReport build;
    const SolidMesh s = BuildSolidMesh(net, solidParams(), build);
    ASSERT_TRUE(s.mesh);
    float top = -1e30f;
    for (std::size_t i = 0; i < s.mesh->GetVertices().size(); i += 3)
        top = std::max(top, s.mesh->GetVertices()[i]);
    const PrintConstraints pc = printer(1.0, 0.01);
    {
        const Vector3d c(top + 10.0, 0.0, 5.0);
        const double a = 0.3;
        std::vector<float> ev;
        std::vector<unsigned int> et;
        appendOctahedron(ev, et, c, a, false);
        // Triangle ouvert dans le plan x = c.x + a, qui contient le premier sommet de B en son interieur.
        const unsigned int k = static_cast<unsigned int>(ev.size() / 3);
        const float x = static_cast<float>(c.x + a);
        ev.insert(ev.end(), {x, static_cast<float>(c.y - 1), static_cast<float>(c.z - 1), x, static_cast<float>(c.y + 2),
                             static_cast<float>(c.z - 1), x, static_cast<float>(c.y - 1), static_cast<float>(c.z + 2)});
        et.insert(et.end(), {k, k + 1, k + 2});
        std::vector<bool> onlyOpen(et.size() / 3, false);
        onlyOpen.back() = true;
        ASSERT_FALSE(WindingNumber(ev, et, onlyOpen, Vector3d(c.x + a, c.y, c.z)).has_value());   // precondition
        const SolidMesh m = withExtra(s, ev, et);
        MicroarchReport report;
        const SolidPrintVerdict v = CheckSolidPrintable(m, net, &pc, report);
        EXPECT_EQ(v.cavities, 0u) << ReportToJson(report);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidSealedCavity), 0u);
        EXPECT_GE(report.Count(MicroarchError::SolidGeneratorDefect), 1u);
        EXPECT_FALSE(v.generator_ok);
    }
    {
        const Vector3d c(top + 10.0, 0.0, 5.0);
        std::vector<float> ev;
        std::vector<unsigned int> et;
        appendOctahedron(ev, et, c, 0.2, false);
        appendCube(ev, et, Vector3d(c.x - 1.0, c.y - 1.0, c.z - 1.0), 2.0, true);
        appendCube(ev, et, Vector3d(c.x - 2.0, c.y - 2.0, c.z - 2.0), 4.0, true);
        std::vector<bool> cubes(et.size() / 3, true);
        for (std::size_t f = 0; f < 8; ++f)
            cubes[f] = false;
        ASSERT_EQ(WindingNumber(ev, et, cubes, Vector3d(c.x + 0.2, c.y, c.z)), std::optional<long>(2));   // precondition
        const SolidMesh m = withExtra(s, ev, et);
        MicroarchReport report;
        const SolidPrintVerdict v = CheckSolidPrintable(m, net, &pc, report);
        EXPECT_EQ(v.cavities, 0u) << ReportToJson(report);
        EXPECT_EQ(report.Count(MicroarchWarning::SolidSealedCavity), 0u);
        EXPECT_GE(report.Count(MicroarchError::SolidGeneratorDefect), 1u);
        EXPECT_FALSE(v.generator_ok);
    }
}
