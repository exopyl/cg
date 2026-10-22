#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/surface_basic.h"

// ===========================================================================
//  CreateTubes (surface_basic.h)
// ===========================================================================
//
// Disposition attendue d'une polyligne ouverte de M points sans biseau, K cotes :
// anneau k aux sommets [k K ; (k + 1) K), bandes entre anneaux consecutifs, puis
// capuchons dans l'ordre des centres (cles de regroupement croissantes). Les indices
// sont compares exactement ; les sommets, qui dependent de sinf et cosf de la
// bibliotheque C, a une tolerance pres.

namespace
{
    using Polyline = std::vector<Vector3f>;

    struct Tubes
    {
        std::unique_ptr<Mesh> mesh;
        int nv = 0, nf = 0;

        explicit Tubes (const std::vector<Polyline> &polylines, float radius = 0.1f, int sides = 6,
                        bool caps = true)
            : mesh(CreateTubes(polylines, radius, sides, caps))
        {
            nv = mesh->GetNVertices();
            nf = mesh->GetNFaces();
        }

        std::vector<unsigned int> faces () const
        {
            std::vector<unsigned int> f;
            for (int i = 0; i < nf; ++i)
                for (int k = 0; k < 3; ++k)
                    f.push_back(static_cast<unsigned int>(mesh->GetFaceVertex(i, k)));
            return f;
        }

        Vector3f vertex (int i) const
        {
            Vector3f v;
            mesh->GetVertex(static_cast<unsigned int>(i), v);
            return v;
        }
    };

    // Indices attendus : bandes de chaque paire d'anneaux, puis capuchon du debut
    // (ou de la fin, si startFirst est faux) et de l'autre extremite.
    std::vector<unsigned int> openLayout (int m, int k, bool startFirst = true)
    {
        std::vector<unsigned int> f;
        for (int s = 0; s + 1 < m; ++s)
        {
            const unsigned int a = s * k, b = (s + 1) * k;
            for (int j = 0; j < k; ++j)
            {
                const unsigned int j2 = (j + 1) % k;
                f.insert(f.end(), {a + j, a + j2, b + j2, a + j, b + j2, b + j});
            }
        }
        auto startCap = [&] {
            for (int j = 1; j <= k - 2; ++j)
                f.insert(f.end(), {0u, unsigned(j + 1), unsigned(j)});
        };
        auto endCap = [&] {
            const unsigned int b = (m - 1) * k;
            for (int j = 1; j <= k - 2; ++j)
                f.insert(f.end(), {b, b + j, b + j + 1});
        };
        if (startFirst)
        {
            startCap();
            endCap();
        }
        else
        {
            endCap();
            startCap();
        }
        return f;
    }

    // M points sur la droite y = y0, z = 0, de x0 a x0 + length.
    Polyline straight (float x0, float y0, float length, int m)
    {
        Polyline p;
        for (int i = 0; i < m; ++i)
            p.push_back(Vector3f(x0 + length * float(i) / float(m - 1), y0, 0.f));
        return p;
    }

    // M points sur un quart de cercle de rayon 1 (coins d'au plus 30 degres : onglets).
    Polyline arc (int m)
    {
        Polyline p;
        for (int i = 0; i < m; ++i)
        {
            const double t = 1.5707963267948966 * double(i) / double(m - 1);
            p.push_back(Vector3f(float(-std::cos(t)), float(std::sin(t)), 0.25f * float(i)));
        }
        return p;
    }
}

// Disposition d'une polyligne ouverte sans biseau, dont cgarchi depend
// (BuildTubeMesh) : V = M K, F = 2 K (M - 1) + 2 (K - 2), indices figes.
TEST(TEST_cgmesh_surface_tubes, open_polyline_layout)
{
    for (const int k : {3, 6, 64})
        for (const int m : {2, 5, 17})
            for (const bool curved : {false, true})
            {
                SCOPED_TRACE("K " + std::to_string(k) + ", M " + std::to_string(m) + (curved ? ", arc" : ", line"));
                const Tubes t({curved ? arc(m) : straight(0.f, 0.f, 2.f, m)}, 0.05f, k);
                EXPECT_EQ(t.nv, m * k);
                EXPECT_EQ(t.nf, 2 * k * (m - 1) + 2 * (k - 2));
                EXPECT_EQ(t.faces(), openLayout(m, k));
            }
}

// Sommets d'une droite le long de x : anneau k centre sur le k-ieme point, dans le
// plan (y, z), j-ieme sommet a l'angle 2 pi j / K depuis +y.
TEST(TEST_cgmesh_surface_tubes, straight_tube_vertices)
{
    const int k = 6, m = 3;
    const float r = 0.5f;
    const Tubes t({straight(1.f, 2.f, 4.f, m)}, r, k);
    ASSERT_EQ(t.nv, m * k);
    for (int s = 0; s < m; ++s)
        for (int j = 0; j < k; ++j)
        {
            const double a = 2.0 * 3.14159265358979323846 * j / k;
            const Vector3f v = t.vertex(s * k + j);
            EXPECT_NEAR(v.x, 1.0 + 2.0 * s, 1e-6);
            EXPECT_NEAR(v.y, 2.0 + r * std::cos(a), 1e-6);
            EXPECT_NEAR(v.z, r * std::sin(a), 1e-6);
        }
}

// Boucle fermee (dernier point sur le premier) : une bande par cote, aucun capuchon.
TEST(TEST_cgmesh_surface_tubes, closed_loop_has_no_cap)
{
    const int k = 6;
    const Polyline square = {Vector3f(0.f, 0.f, 0.f), Vector3f(1.f, 0.f, 0.f), Vector3f(1.f, 1.f, 0.f),
                             Vector3f(0.f, 1.f, 0.f), Vector3f(0.f, 0.f, 0.f)};
    const Tubes t({square}, 0.1f, k);
    EXPECT_EQ(t.nv, 4 * k);
    EXPECT_EQ(t.nf, 2 * k * 4);
}

// Deux polylignes a bout commun : la jonction est fermee par une enveloppe, dont les
// sommets s'ajoutent aux anneaux ; les deux bouts libres ont leur capuchon.
TEST(TEST_cgmesh_surface_tubes, shared_end_gets_a_hull)
{
    const int k = 6;
    const Polyline a = {Vector3f(0.f, 0.f, 0.f), Vector3f(1.f, 0.f, 0.f)};
    const Polyline b = {Vector3f(1.f, 0.f, 0.f), Vector3f(1.f, 1.f, 0.f)};
    const Tubes t({a, b}, 0.1f, k);
    const int bands = 2 * 2 * k, freeCaps = 2 * (k - 2);
    EXPECT_GT(t.nv, 4 * k);
    EXPECT_GT(t.nf, bands + freeCaps);
}

// Deux tubes disjoints gardent leur disposition loin de l'origine : memes comptes,
// memes indices, sommets egaux a ceux de l'origine translates (a l'ulp de x0 pres).
TEST(TEST_cgmesh_surface_tubes, far_tubes_keep_their_layout)
{
    const int k = 6;
    struct Case { float x0, scale; };
    const Case cases[] = {{0.f, 1.f}, {3e5f, 1.f}, {-3e5f, 1.f}, {1e9f, 1e3f}};
    const Tubes ref({straight(0.f, 0.f, 20.f, 2), straight(0.f, 10.f, 20.f, 2)}, 1.f, k);
    ASSERT_EQ(ref.nv, 2 * 2 * k);
    ASSERT_EQ(ref.nf, 2 * (2 * k + 2 * (k - 2)));
    for (const Case &c : cases)
    {
        SCOPED_TRACE("x0 " + std::to_string(c.x0));
        const float s = c.scale;
        const Tubes t({straight(c.x0, 0.f, 20.f * s, 2), straight(c.x0, 10.f * s, 20.f * s, 2)}, s, k);
        EXPECT_EQ(t.nv, ref.nv);
        EXPECT_EQ(t.nf, ref.nf);
        EXPECT_EQ(t.faces(), ref.faces());
        if (t.nv != ref.nv)
            continue;
        const double ulp = std::fabs(std::nextafter(std::fabs(c.x0) + 20.f * s, 2e30f) - (std::fabs(c.x0) + 20.f * s));
        for (int i = 0; i < t.nv; ++i)
        {
            const Vector3f v = t.vertex(i), w = ref.vertex(i);
            EXPECT_NEAR(v.x, c.x0 + s * w.x, std::max(1e-5 * s, ulp));
            EXPECT_NEAR(v.y, s * w.y, 1e-5 * s);
            EXPECT_NEAR(v.z, s * w.z, 1e-5 * s);
        }
    }
}

// Rien a tuber : un maillage vide, et non un triangle factice.
TEST(TEST_cgmesh_surface_tubes, empty_input_gives_an_empty_mesh)
{
    const Vector3f p(1.f, 2.f, 3.f);
    const std::vector<std::vector<Polyline>> inputs = {
        {}, {{}}, {{p}}, {{p, p, p}}, {{p}, {p, p}},
    };
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        SCOPED_TRACE("input " + std::to_string(i));
        const Tubes t(inputs[i]);
        EXPECT_EQ(t.nv, 0);
        EXPECT_EQ(t.nf, 0);
    }
}

// Une polyligne qui contient un point non fini est ecartee entiere : les autres
// donnent exactement le meme maillage que sans elle.
TEST(TEST_cgmesh_surface_tubes, a_polyline_with_a_non_finite_point_is_skipped)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const Polyline good = straight(0.f, 0.f, 2.f, 4);
    const Tubes ref({good});
    ASSERT_GT(ref.nf, 0);
    const std::vector<Polyline> bad = {
        {Vector3f(nan, 0.f, 0.f), Vector3f(1.f, 5.f, 0.f), Vector3f(2.f, 5.f, 0.f)},
        {Vector3f(0.f, 5.f, 0.f), Vector3f(1.f, nan, 0.f), Vector3f(2.f, 5.f, 0.f)},
        {Vector3f(0.f, 5.f, 0.f), Vector3f(1.f, 5.f, 0.f), Vector3f(2.f, 5.f, inf)},
        {Vector3f(-inf, 5.f, 0.f), Vector3f(1.f, 5.f, 0.f)},
    };
    for (std::size_t i = 0; i < bad.size(); ++i)
    {
        SCOPED_TRACE("bad polyline " + std::to_string(i));
        for (const bool badFirst : {true, false})
        {
            const Tubes t(badFirst ? std::vector<Polyline>{bad[i], good} : std::vector<Polyline>{good, bad[i]});
            ASSERT_EQ(t.nv, ref.nv);
            ASSERT_EQ(t.nf, ref.nf);
            EXPECT_EQ(t.faces(), ref.faces());
            for (int v = 0; v < t.nv; ++v)
            {
                const Vector3f a = t.vertex(v), b = ref.vertex(v);
                EXPECT_EQ(a.x, b.x);
                EXPECT_EQ(a.y, b.y);
                EXPECT_EQ(a.z, b.z);
            }
        }
        const Tubes alone({bad[i]});
        EXPECT_EQ(alone.nv, 0);
        EXPECT_EQ(alone.nf, 0);
    }
}

// nSides est ramene dans [kTubeMinSides ; kTubeMaxSides].
TEST(TEST_cgmesh_surface_tubes, side_count_range)
{
    static_assert(kTubeMinSides == 3 && kTubeMaxSides == 1024, "documented range");
    const int m = 3;
    struct Case { int requested, used; };
    const Case cases[] = {{-5, 3}, {0, 3}, {2, 3}, {3, 3}, {1024, 1024}, {1025, 1024}, {4096, 1024}};
    for (const Case &c : cases)
    {
        SCOPED_TRACE("nSides " + std::to_string(c.requested));
        const Tubes t({straight(0.f, 0.f, 2.f, m)}, 0.1f, c.requested);
        EXPECT_EQ(t.nv, m * c.used);
        EXPECT_EQ(t.nf, 2 * c.used * (m - 1) + 2 * (c.used - 2));
    }
}
