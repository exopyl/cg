#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "clipper2/clipper.h"

#include <cgarchi/architecture_gothic_region.h>
#include "../src/cgmesh/contour_validation.h"
#include "../src/cgmesh/polygon2.h"

using namespace cgarchi;   // fichier de test de cgarchi seul

// ===========================================================================
//  Region de pierre gothique : un seul booleen 2D
// ===========================================================================
//
// pierre = cadre - (U vides U U_k (fenetre_k inter U sous-vides_k)). La sortie
// est un jeu de contours deux a deux disjoints, dans la convention de Clipper2,
// que ValidateContours accepte sans defaut.

namespace
{
    GothicRing rect(double x0, double y0, double x1, double y1)
    {
        return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};   // sens trigonometrique
    }

    GothicRing reversed(GothicRing r) { return GothicRing(r.rbegin(), r.rend()); }

    double signedArea(const GothicRing &ring)
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < ring.size(); ++i)
        {
            const Vector2d &a = ring[i];
            const Vector2d &b = ring[(i + 1) % ring.size()];
            sum += a.x * b.y - b.x * a.y;
        }
        return 0.5 * sum;
    }

    double totalArea(const std::vector<GothicRing> &region)
    {
        double s = 0.0;
        for (const GothicRing &r : region)
            s += signedArea(r);
        return s;
    }

    std::size_t countOuter(const std::vector<GothicRing> &region)
    {
        std::size_t n = 0;
        for (const GothicRing &r : region)
            if (signedArea(r) > 0.0)
                ++n;
        return n;
    }

    // Grandeurs du gabarit, multiples du pas de 2^-7 de ClipperD (precision 2) :
    // l'accrochage a la grille ne les deplace pas.
    constexpr double kFrameArea = 100.0 * 200.0;

    // Gabarit de baie recursive, dans la disposition que produit le generateur :
    // une ouverture (20..80 x 20..180) percee dans le cadre, et deux
    // sous-lancettes posees sur son appui (y = 20) et contre ses jambages
    // (x = 20, x = 80), separees par un meneau.
    GothicStoneInput recursiveBay()
    {
        GothicStoneInput in;
        in.frame = rect(0, 0, 100, 200);
        GothicClippedVoids lancet;
        lancet.window = rect(20, 20, 80, 180);
        lancet.voids  = {rect(20, 20, 45, 150), rect(55, 20, 80, 150)};
        in.clippedVoids.push_back(lancet);
        return in;
    }

    // Construction remplacee : l'ouverture percee comme trou, puis la pierre
    // ouverture - sous-vides reposee dedans comme ilots. Meme entree que
    // recursiveBay(), memes appels Clipper2 que l'ancien buildBayStonePolygon.
    std::vector<GothicRing> openingWithStoneIslands(const GothicStoneInput &in)
    {
        using namespace Clipper2Lib;
        auto toPath = [](const GothicRing &r) {
            PathD p;
            for (const Vector2d &v : r) p.emplace_back(v.x, v.y);
            return p;
        };
        std::vector<GothicRing> out{in.frame};
        for (const GothicClippedVoids &g : in.clippedVoids)
        {
            PathsD subs;
            for (const GothicRing &v : g.voids) subs.push_back(toPath(v));
            const PathsD stone = Difference(PathsD{toPath(g.window)}, Union(subs, FillRule::NonZero),
                                            FillRule::NonZero, kGothicClipperPrecision);
            out.push_back(reversed(g.window));
            for (const PathD &p : stone)
            {
                GothicRing r;
                for (const auto &pt : p) r.emplace_back(pt.x, pt.y);
                out.push_back(r);
            }
        }
        return out;
    }
}

TEST(TEST_cgarchi_architecture_gothic_region, frame_minus_union_of_voids_is_one_disjoint_region)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 200);
    // Deux vides qui se recouvrent (fondus) et un vide trace en sens horaire
    // (lu comme plein malgre son sens).
    in.voids = {rect(10, 10, 40, 60), rect(30, 10, 60, 60), reversed(rect(10, 100, 90, 150))};

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    ASSERT_EQ(region.size(), 3u);
    EXPECT_EQ(countOuter(region), 1u);
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 50.0 * 50.0 - 80.0 * 50.0);

    const ContourValidationReport r = ValidateContours(gothicRegionToContours(region));
    EXPECT_TRUE(r.IsValid());
    EXPECT_EQ(r.Count(ContourDefect::ContourContact), 0u);
}

TEST(TEST_cgarchi_architecture_gothic_region, clipped_voids_are_emptied_only_inside_their_window)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 200);
    GothicClippedVoids g;
    g.window = rect(20, 20, 80, 180);
    g.voids  = {rect(10, 30, 50, 60)};   // deborde la fenetre a gauche
    in.clippedVoids.push_back(g);

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 30.0 * 30.0);
    EXPECT_TRUE(ValidateContours(gothicRegionToContours(region)).IsValid());
}

// Le gabarit est discriminant : la construction remplacee y produit des aretes
// communes entre l'ouverture et ses ilots ; le booleen unique n'en produit pas,
// et la pierre recursive reste reliee au cadre.
TEST(TEST_cgarchi_architecture_gothic_region, recursive_bay_has_no_shared_edge_unlike_opening_with_islands)
{
    const GothicStoneInput in = recursiveBay();

    const ContourValidationReport before = ValidateContours(gothicRegionToContours(openingWithStoneIslands(in)));
    EXPECT_GT(before.Count(ContourDefect::ContourContact), 0u) << "gabarit non discriminant";
    EXPECT_GE(before.contactContourPairs, 1u);

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    const ContourValidationReport after = ValidateContours(gothicRegionToContours(region));
    EXPECT_TRUE(after.IsValid());
    EXPECT_EQ(after.Count(ContourDefect::ContourContact), 0u);
    EXPECT_EQ(countOuter(region), 1u) << "la pierre recursive doit rester reliee au cadre";
    EXPECT_EQ(region.size(), 3u);   // cadre + deux sous-lancettes
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 2.0 * 25.0 * 130.0);
}

// Chaque fenetre ne borne que ses propres sous-vides : un sous-vide qui deborde
// dans la fenetre voisine n'y est pas vide. Une intersection de la reunion de
// tous les sous-vides avec la reunion des fenetres viderait 15 x 50 de trop.
TEST(TEST_cgarchi_architecture_gothic_region, each_window_bounds_only_its_own_sub_voids)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 200);
    GothicClippedVoids a;
    a.window = rect(20, 20, 45, 180);
    a.voids  = {rect(30, 50, 70, 100)};   // deborde dans la fenetre b
    GothicClippedVoids b;
    b.window = rect(55, 20, 80, 180);
    b.voids  = {rect(60, 120, 70, 150)};
    in.clippedVoids = {a, b};

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 15.0 * 50.0 - 10.0 * 30.0);
    EXPECT_TRUE(ValidateContours(gothicRegionToContours(region)).IsValid());
}

// Un vide de niveau 0 qui recouvre une region bornee percee d'un ilot : la
// reunion vide la partie couverte de l'ilot, et le reste de l'ilot demeure une
// piece de pierre a part.
TEST(TEST_cgarchi_architecture_gothic_region, overlapping_void_cuts_into_a_clipped_island)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 200);
    GothicClippedVoids g;
    g.window = rect(20, 20, 80, 180);
    // Anneau carre 30..70 x 70..130 autour de l'ilot 40..60 x 80..120.
    g.voids = {rect(30, 70, 70, 80), rect(30, 120, 70, 130), rect(30, 80, 40, 120), rect(60, 80, 70, 120)};
    in.clippedVoids.push_back(g);
    in.voids = {rect(50, 90, 90, 110)};   // couvre une part de l'ilot et de l'anneau

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    // anneau 1600 + vide 800 - recouvrement 200
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 2200.0);
    EXPECT_EQ(countOuter(region), 2u);   // cadre + ilot entame
    EXPECT_TRUE(ValidateContours(gothicRegionToContours(region)).IsValid());
}

TEST(TEST_cgarchi_architecture_gothic_region, a_void_crossing_the_frame_cuts_a_notch)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 200);
    in.voids = {rect(-10, 80, 30, 120)};

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    ASSERT_EQ(region.size(), 1u);
    EXPECT_EQ(region[0].size(), 8u);
    EXPECT_DOUBLE_EQ(totalArea(region), kFrameArea - 30.0 * 40.0);
    EXPECT_TRUE(ValidateContours(gothicRegionToContours(region)).IsValid());
}

// Un sommet pose sur une arete droite donnerait a la tessellation un triangle
// d'aire nulle : la sortie n'en garde aucun.
TEST(TEST_cgarchi_architecture_gothic_region, output_has_no_collinear_vertex)
{
    GothicStoneInput in;
    in.frame = {{0, 0}, {100, 0}, {100, 50}, {100, 200}, {0, 200}};
    in.voids = {{{20, 20}, {40, 20}, {60, 20}, {60, 60}, {20, 60}}};

    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    ASSERT_EQ(region.size(), 2u);
    EXPECT_EQ(region[0].size(), 4u);
    EXPECT_EQ(region[1].size(), 4u);
}

TEST(TEST_cgarchi_architecture_gothic_region, coordinates_snap_to_the_clipper_grid)
{
    // ClipperD (precision 2) accroche a 2^-7 : 100.006 devient 12801/128
    // (100.0078125), la ou une grille de 10^-2 rendrait 100.01.
    GothicStoneInput in;
    in.frame = rect(0, 0, 100.006, 200);
    const std::vector<GothicRing> region = buildGothicStoneRegion(in);
    ASSERT_EQ(region.size(), 1u);
    double maxX = 0.0;
    for (const Vector2d &v : region[0])
        maxX = std::max(maxX, v.x);
    EXPECT_DOUBLE_EQ(maxX, 12801.0 / 128.0);
}

TEST(TEST_cgarchi_architecture_gothic_region, degenerate_inputs)
{
    EXPECT_TRUE(buildGothicStoneRegion(GothicStoneInput{}).empty());

    GothicStoneInput swallowed;
    swallowed.frame = rect(0, 0, 10, 10);
    swallowed.voids = {rect(-1, -1, 11, 11)};
    EXPECT_TRUE(buildGothicStoneRegion(swallowed).empty());

    EXPECT_THROW(buildGothicStoneRegion(swallowed, 9), std::invalid_argument);
    EXPECT_THROW(buildGothicStoneRegion(swallowed, -9), std::invalid_argument);
}

// Un rayon de garde negatif est refuse, meme sur une region valide.
TEST(TEST_cgarchi_architecture_gothic_region, negative_guard_radius_throws)
{
    EXPECT_THROW(buildGothicStoneRegion(recursiveBay(), kGothicClipperPrecision, -1), std::invalid_argument);
    EXPECT_NO_THROW(buildGothicStoneRegion(recursiveBay(), kGothicClipperPrecision, 0));
}

TEST(TEST_cgarchi_architecture_gothic_region, adapters_preserve_rings_order_and_role)
{
    const std::vector<GothicRing> region = buildGothicStoneRegion(recursiveBay());

    const std::vector<ExtrudeContour> contours = gothicRegionToContours(region);
    ASSERT_EQ(contours.size(), region.size());
    Polygon2 poly = gothicRegionToPolygon2(region);
    ASSERT_EQ(poly.get_n_contours(), static_cast<int>(region.size()));
    for (std::size_t i = 0; i < region.size(); ++i)
    {
        EXPECT_EQ(contours[i].isHole, signedArea(region[i]) < 0.0);
        ASSERT_EQ(poly.get_n_points(static_cast<unsigned>(i)), static_cast<int>(region[i].size()));
        const float *pts = poly.get_points(static_cast<unsigned>(i));
        EXPECT_FLOAT_EQ(pts[0], static_cast<float>(region[i][0].x));
        EXPECT_FLOAT_EQ(pts[1], static_cast<float>(region[i][0].y));
    }
    EXPECT_TRUE(ValidateContours(poly).IsValid());
}

// ---------------------------------------------------------------------------
//  Garde morphologique
// ---------------------------------------------------------------------------
//
// Le booleen ne separe pas des vides tangents : deux vides qui se touchent en un
// point, ou un vide portant un cheveu plus fin qu'un pas de grille, laissent des
// contours en contact ou pinces. La garde ouvre puis ferme la reunion des vides
// sur la grille de Clipper2 (rayon kGothicGuardGridSteps pas, jonctions rondes) ;
// elle n'est appliquee que si la region calculee sans elle a un defaut de
// contour. Chaque gabarit est verifie discriminant : sans garde, il est
// invalide.

namespace
{
    // Cercle trace en sens trigonometrique, premier sommet a l'angle 0 ; n pair
    // donne aussi un sommet a l'angle pi.
    GothicRing circle(double cx, double cy, double r, int n = 72)
    {
        GothicRing ring;
        for (int i = 0; i < n; ++i)
        {
            const double t = 2.0 * 3.14159265358979323846 * i / n;
            ring.emplace_back(cx + r * std::cos(t), cy + r * std::sin(t));
        }
        return ring;
    }

    ContourValidationReport validateRegion(const std::vector<GothicRing> &region)
    {
        return ValidateContours(gothicRegionToContours(region));
    }

    struct GuardOutcome
    {
        ContourValidationReport unguarded;
        ContourValidationReport guarded;
        bool                    applied = false;
        GothicGuardOutcome      outcome = GothicGuardOutcome::NotNeeded;
    };

    GuardOutcome runGuard(const GothicStoneInput &in)
    {
        GuardOutcome out;
        out.unguarded = validateRegion(buildGothicStoneRegion(in, kGothicClipperPrecision, 0));
        out.guarded   = validateRegion(buildGothicStoneRegion(in, kGothicClipperPrecision,
                                                              kGothicGuardGridSteps, &out.applied,
                                                              &out.outcome));
        return out;
    }
}

// Deux vides circulaires tangents en un sommet commun.
TEST(TEST_cgarchi_architecture_gothic_region, guard_separates_tangent_circles)
{
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 100);
    in.voids = {circle(30, 50, 15), circle(60, 50, 15)};

    const GuardOutcome g = runGuard(in);
    EXPECT_GT(g.unguarded.Count(ContourDefect::ContourContact), 0u) << "gabarit non discriminant";
    EXPECT_TRUE(g.applied);
    EXPECT_EQ(g.outcome, GothicGuardOutcome::Applied);
    EXPECT_TRUE(g.guarded.IsValid());
    EXPECT_EQ(g.guarded.Count(ContourDefect::ContourContact), 0u);
}

// Un vide portant un cheveu d'un pas de grille g = 1/128 : le sommet
// V = (45 - g, 30 + g) est a 2 g^2 / 10 = 1,2e-5 de l'arete (40, 30)-(50, 30 + 2 g),
// sous la tolerance de validation (1,4e-4).
TEST(TEST_cgarchi_architecture_gothic_region, guard_removes_a_hair_on_a_void)
{
    constexpr double g = 1.0 / 128.0;
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 100);
    in.voids = {rect(20, 20, 40, 40),
                {{40, 30}, {50, 30 + 2 * g}, {45 - g, 30 + g}, {40, 30 + g}}};

    const GuardOutcome r = runGuard(in);
    EXPECT_GT(r.unguarded.Count(ContourDefect::SelfIntersection), 0u) << "gabarit non discriminant";
    EXPECT_TRUE(r.applied);
    EXPECT_EQ(r.outcome, GothicGuardOutcome::Applied);
    EXPECT_TRUE(r.guarded.IsValid());
    EXPECT_EQ(r.guarded.Count(ContourDefect::SelfIntersection), 0u);
}

// Un seul vide a deux lobes tangents : anneau en huit, chaque boucle en sens
// trigonometrique, les deux lobes se touchant au sommet (45, 50).
TEST(TEST_cgarchi_architecture_gothic_region, guard_separates_tangent_lobes)
{
    GothicRing eight;
    const GothicRing left  = circle(30, 50, 15);   // demarre en (45, 50)
    const GothicRing right = circle(60, 50, 15);
    const std::size_t half = right.size() / 2;     // sommet (45, 50) du cercle droit
    for (const Vector2d &v : left) eight.push_back(v);
    for (std::size_t i = 0; i < right.size(); ++i) eight.push_back(right[(half + i) % right.size()]);
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 100);
    in.voids = {eight};

    const GuardOutcome g = runGuard(in);
    EXPECT_GT(g.unguarded.Count(ContourDefect::SelfIntersection), 0u) << "gabarit non discriminant";
    EXPECT_TRUE(g.applied);
    EXPECT_EQ(g.outcome, GothicGuardOutcome::Applied);
    EXPECT_TRUE(g.guarded.IsValid());
    EXPECT_EQ(g.guarded.Count(ContourDefect::SelfIntersection), 0u);
}

// A la demande : une region sans defaut sort inchangee, garde non appliquee.
TEST(TEST_cgarchi_architecture_gothic_region, guard_leaves_a_valid_region_unchanged)
{
    const GothicStoneInput in = recursiveBay();
    bool applied = true;
    GothicGuardOutcome outcome = GothicGuardOutcome::Insufficient;
    const std::vector<GothicRing> guarded   = buildGothicStoneRegion(in, kGothicClipperPrecision,
                                                                     kGothicGuardGridSteps, &applied, &outcome);
    const std::vector<GothicRing> unguarded = buildGothicStoneRegion(in, kGothicClipperPrecision, 0);
    EXPECT_FALSE(applied);
    EXPECT_EQ(outcome, GothicGuardOutcome::NotNeeded);
    ASSERT_EQ(guarded.size(), unguarded.size());
    for (std::size_t i = 0; i < guarded.size(); ++i)
    {
        ASSERT_EQ(guarded[i].size(), unguarded[i].size());
        for (std::size_t k = 0; k < guarded[i].size(); ++k)
        {
            EXPECT_EQ(guarded[i][k].x, unguarded[i][k].x);
            EXPECT_EQ(guarded[i][k].y, unguarded[i][k].y);
        }
    }
}

// Un vide tangent au cadre, en un sommet : la difference pince la pierre contre
// le cadre. Les deux traitements ne portent que sur la reunion des vides, et
// aucun ne decolle le vide du cadre : la garde est insuffisante, la region
// rendue est celle de l'ouverture suivie de la fermeture, toujours invalide.
// Un second vide porte un cheveu (gabarit de guard_removes_a_hair_on_a_void) :
// l'ouverture le retire, la fermeture seule le garde. L'absence de sommet de
// la region dans la bande du cheveu designe donc la region rendue.
TEST(TEST_cgarchi_architecture_gothic_region, guard_insufficient_when_a_void_touches_the_frame)
{
    constexpr double g = 1.0 / 128.0;
    GothicStoneInput in;
    in.frame = rect(0, 0, 100, 100);
    in.voids = {circle(50, 15, 15),   // sommet (50, 0) sur le bord bas du cadre
                rect(20, 60, 40, 80),
                {{40, 70}, {50, 70 + 2 * g}, {45 - g, 70 + g}, {40, 70 + g}}};

    const GuardOutcome r = runGuard(in);
    EXPECT_FALSE(r.unguarded.IsValid()) << "gabarit non discriminant";
    EXPECT_TRUE(r.applied);
    EXPECT_EQ(r.outcome, GothicGuardOutcome::Insufficient);
    EXPECT_FALSE(r.guarded.IsValid());

    auto hairVertices = [](const std::vector<GothicRing> &region) {
        std::size_t n = 0;
        for (const GothicRing &ring : region)
            for (const Vector2d &v : ring)
                n += (v.x > 41.0 && v.x <= 51.0 && v.y > 69.5 && v.y < 70.5);
        return n;
    };
    const std::vector<GothicRing> unguarded = buildGothicStoneRegion(in, kGothicClipperPrecision, 0);
    const std::vector<GothicRing> guarded   = buildGothicStoneRegion(in, kGothicClipperPrecision,
                                                                     kGothicGuardGridSteps);
    EXPECT_GT(hairVertices(unguarded), 0u) << "gabarit non discriminant : pas de cheveu";
    EXPECT_EQ(hairVertices(guarded), 0u) << "la region rendue garde le cheveu : ce n'est pas l'ouverture suivie de la fermeture";
}
