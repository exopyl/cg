// ============================================================================
//  K1 -- svg_to_shape_groups (svg_couleurs_faisabilite.md, section 12)
// ============================================================================
//
// Ce fichier tient l'invariant qui fonde la piste A1 : `svg_to_contours` EST la
// concatenation de `svg_to_shape_groups`. La version plate est derivee, donc
// elle ne peut pas diverger -- a condition que quelqu'un le verifie point par
// point, sur des fichiers reels, et non seulement par lecture.
//
// Le detecteur d'egalite est lui-meme sous test (`the_detector_*`) : un
// comparateur qui n'a jamais echoue ne prouve rien. Ces trois cas positifs
// couvrent les trois facons de casser l'invariant -- une coordonnee, l'ordre,
// le nombre.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

// Tolerance de comparaison. Les deux chemins produisent les MEMES flottants --
// c'est la meme fonction qui les calcule -- donc l'egalite est attendue exacte ;
// la tolerance ne fait qu'eviter qu'un futur passage par un intermediaire
// arrondi transforme cette suite en detecteur de bruit.
const float kTol = 1e-6f;

SvgExtrudeOptions defaultOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

std::vector<ExtrudeContour> concatenate(const std::vector<SvgShapeGroup>& groups)
{
    std::vector<ExtrudeContour> flat;
    for (const SvgShapeGroup& g : groups)
        flat.insert(flat.end(), g.contours.begin(), g.contours.end());
    return flat;
}

// Compare NOMBRE, ORDRE et coordonnees. Le premier ecart est rendu avec son
// indice de contour et de point : un message qui dit seulement « different »
// oblige a instrumenter la suite a la main.
::testing::AssertionResult sameContours(const std::vector<ExtrudeContour>& expected,
                                        const std::vector<ExtrudeContour>& actual)
{
    if (expected.size() != actual.size())
        return ::testing::AssertionFailure()
               << "nombre de contours : attendu " << expected.size()
               << ", obtenu " << actual.size();

    for (size_t c = 0; c < expected.size(); ++c)
    {
        const std::vector<Vector2f>& a = expected[c].pts;
        const std::vector<Vector2f>& b = actual[c].pts;
        if (a.size() != b.size())
            return ::testing::AssertionFailure()
                   << "contour " << c << " : attendu " << a.size()
                   << " points, obtenu " << b.size();

        for (size_t i = 0; i < a.size(); ++i)
        {
            const float dx = std::fabs(a[i].x - b[i].x);
            const float dy = std::fabs(a[i].y - b[i].y);
            if (dx > kTol || dy > kTol)
                return ::testing::AssertionFailure()
                       << "contour " << c << ", point " << i << " : attendu ("
                       << a[i].x << ", " << a[i].y << "), obtenu ("
                       << b[i].x << ", " << b[i].y << ")"
                       << " -- ecart (" << dx << ", " << dy << ")";
        }
    }
    return ::testing::AssertionSuccess();
}

const char* kFixtures[] = {
    "./test/data/svg/square.svg",
    "./test/data/svg/triangle.svg",
    "./test/data/svg/rose.svg",
    "./test/data/svg/nazca.svg",
    "./test/data/svg/batman.svg",
    "./test/data/svg/spiderman.svg",
    "./test/data/svg/Ghostscript_Tiger.svg",
};

// Groupes d'une fixture connue pour porter plusieurs formes : support des cas
// positifs du detecteur.
std::vector<SvgShapeGroup> roseGroups()
{
    std::vector<SvgShapeGroup> groups;
    EXPECT_TRUE(svg_to_shape_groups("./test/data/svg/rose.svg", defaultOptions(), groups));
    return groups;
}

} // namespace

// ---------------------------------------------------------------------------
//  L'invariant de la piste A1
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_svg_k1, groups_concatenate_to_the_flat_contour_list)
{
    // Une fixture mono-groupe ne dit rien de l'ORDRE : le compte de fixtures
    // multi-groupes est donc verifie, sans quoi la suite pourrait passer au vert
    // en n'exercant que des cas ou l'ordre n'existe pas.
    size_t multiGroupFixtures = 0;

    for (const char* path : kFixtures)
    {
        const SvgExtrudeOptions opt = defaultOptions();

        std::vector<ExtrudeContour> flat;
        ASSERT_TRUE(svg_to_contours(path, opt, flat)) << path;

        std::vector<SvgShapeGroup> groups;
        ASSERT_TRUE(svg_to_shape_groups(path, opt, groups)) << path;

        EXPECT_TRUE(sameContours(flat, concatenate(groups))) << path;
        if (groups.size() > 1u) ++multiGroupFixtures;
    }

    EXPECT_GE(multiGroupFixtures, 3u);
}

// Sans centerAndFit les coordonnees restent celles du document : l'invariant ne
// doit pas dependre du recentrage global, qui est justement le point du
// decoupage ou il serait le plus facile de le perdre.
TEST(TEST_cgmesh_svg_k1, the_invariant_holds_without_center_and_fit)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.centerAndFit = false;

    for (const char* path : kFixtures)
    {
        std::vector<ExtrudeContour> flat;
        ASSERT_TRUE(svg_to_contours(path, opt, flat)) << path;

        std::vector<SvgShapeGroup> groups;
        ASSERT_TRUE(svg_to_shape_groups(path, opt, groups)) << path;

        EXPECT_TRUE(sameContours(flat, concatenate(groups))) << path;
    }
}

// Le recentrage-ajustement porte sur TOUTES les formes a la fois, sans quoi
// chaque groupe serait normalise pour lui-meme et leur placement relatif
// disparaitrait. Une fixture multi-formes tient dans [-0.5, 0.5] et touche les
// deux bornes sur sa plus grande dimension.
TEST(TEST_cgmesh_svg_k1, center_and_fit_spans_all_groups_at_once)
{
    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups("./test/data/svg/rose.svg", defaultOptions(), groups));
    ASSERT_GT(groups.size(), 1u);

    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    for (const SvgShapeGroup& g : groups)
        for (const ExtrudeContour& c : g.contours)
            for (const Vector2f& p : c.pts)
            {
                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
            }

    EXPECT_GE(minX, -0.5f - kTol);
    EXPECT_LE(maxX,  0.5f + kTol);
    EXPECT_GE(minY, -0.5f - kTol);
    EXPECT_LE(maxY,  0.5f + kTol);
    EXPECT_NEAR(std::max(maxX - minX, maxY - minY), 1.0f, 1e-4f);

    // Aucun groupe pris isolement ne remplit l'emprise : c'est ce qui distingue
    // un ajustement global d'un ajustement groupe par groupe.
    bool someGroupIsSmaller = false;
    for (const SvgShapeGroup& g : groups)
    {
        float gMinX = 1e30f, gMaxX = -1e30f, gMinY = 1e30f, gMaxY = -1e30f;
        for (const ExtrudeContour& c : g.contours)
            for (const Vector2f& p : c.pts)
            {
                gMinX = std::min(gMinX, p.x); gMaxX = std::max(gMaxX, p.x);
                gMinY = std::min(gMinY, p.y); gMaxY = std::max(gMaxY, p.y);
            }
        if (std::max(gMaxX - gMinX, gMaxY - gMinY) < 0.99f) someGroupIsSmaller = true;
    }
    EXPECT_TRUE(someGroupIsSmaller);
}

// ---------------------------------------------------------------------------
//  Le detecteur sait echouer -- trois cas positifs
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_svg_k1, the_detector_rejects_a_moved_point)
{
    std::vector<SvgShapeGroup> groups = roseGroups();
    ASSERT_FALSE(groups.empty());
    const std::vector<ExtrudeContour> reference = concatenate(groups);

    ASSERT_FALSE(groups.back().contours.empty());
    ASSERT_FALSE(groups.back().contours.back().pts.empty());
    groups.back().contours.back().pts.back().x += 1e-3f;

    EXPECT_FALSE(sameContours(reference, concatenate(groups)));
}

TEST(TEST_cgmesh_svg_k1, the_detector_rejects_swapped_groups)
{
    std::vector<SvgShapeGroup> groups = roseGroups();
    ASSERT_GT(groups.size(), 1u);
    const std::vector<ExtrudeContour> reference = concatenate(groups);

    std::swap(groups[0], groups[1]);

    EXPECT_FALSE(sameContours(reference, concatenate(groups)));
}

TEST(TEST_cgmesh_svg_k1, the_detector_rejects_a_missing_group)
{
    std::vector<SvgShapeGroup> groups = roseGroups();
    ASSERT_GT(groups.size(), 1u);
    const std::vector<ExtrudeContour> reference = concatenate(groups);

    groups.pop_back();

    EXPECT_FALSE(sameContours(reference, concatenate(groups)));
}

// ---------------------------------------------------------------------------
//  Peinture
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_svg_k1, a_filled_shape_carries_its_fill_colour)
{
    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups("./test/data/svg/square.svg", defaultOptions(), groups));
    ASSERT_EQ(groups.size(), 1u);

    // square.svg : fill="#888", opaque. Empaquetage nanosvg : r | g<<8 | b<<16 | a<<24.
    const SvgShapePaint& paint = groups[0].paint;
    EXPECT_TRUE(paint.hasFill);
    EXPECT_FALSE(paint.isGradient);
    EXPECT_EQ(paint.fillRGBA & 0xFFu,         0x88u);
    EXPECT_EQ((paint.fillRGBA >>  8) & 0xFFu, 0x88u);
    EXPECT_EQ((paint.fillRGBA >> 16) & 0xFFu, 0x88u);
    EXPECT_EQ((paint.fillRGBA >> 24) & 0xFFu, 0xFFu);
    EXPECT_EQ(paint.rank, 0u);
}

// Le rang est celui du document, donc croissant dans l'ordre des groupes, et
// partage par les deux groupes d'une meme forme.
TEST(TEST_cgmesh_svg_k1, ranks_follow_document_order)
{
    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups("./test/data/svg/Ghostscript_Tiger.svg",
                                    defaultOptions(), groups));
    ASSERT_GT(groups.size(), 1u);

    for (size_t i = 1; i < groups.size(); ++i)
        EXPECT_GE(groups[i].paint.rank, groups[i - 1].paint.rank)
            << "groupe " << i;

    // Le Tigre a 13 formes au trait seul (K0) : au moins un groupe sans
    // remplissage, donc issu d'un trait epaissi.
    size_t strokeGroups = 0;
    for (const SvgShapeGroup& g : groups)
        if (!g.paint.hasFill) ++strokeGroups;
    EXPECT_GT(strokeGroups, 0u);

    // Aucun degrade dans ce fichier (K0) : la moyenne des stops n'y est jamais
    // employee.
    for (const SvgShapeGroup& g : groups)
        EXPECT_FALSE(g.paint.isGradient);
}

// ---------------------------------------------------------------------------
//  NSVG_FLAGS_VISIBLE
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_svg_k1, a_hidden_shape_is_not_extruded)
{
    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups("./test/data/svg/hidden_shape.svg",
                                    defaultOptions(), groups));

    // hidden_shape.svg porte trois rects, dont deux en display:none.
    EXPECT_EQ(groups.size(), 1u);
    ASSERT_EQ(groups[0].contours.size(), 1u);
    EXPECT_TRUE(groups[0].paint.hasFill);
}

// Oracle geometrique : les formes masquees debordent de l'emprise du carre
// visible. Si elles etaient extrudees, le recentrage-ajustement global les
// prendrait en compte et le carre visible sortirait a une autre echelle.
TEST(TEST_cgmesh_svg_k1, hiding_a_shape_leaves_the_visible_one_untouched)
{
    const SvgExtrudeOptions opt = defaultOptions();

    std::vector<ExtrudeContour> visibleOnly;
    ASSERT_TRUE(svg_to_contours("./test/data/svg/square.svg", opt, visibleOnly));

    std::vector<ExtrudeContour> withHidden;
    ASSERT_TRUE(svg_to_contours("./test/data/svg/hidden_shape.svg", opt, withHidden));

    EXPECT_TRUE(sameContours(visibleOnly, withHidden));
}

TEST(TEST_cgmesh_svg_k1, a_hidden_shape_adds_no_geometry_to_the_mesh)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.height = 0.5f;

    std::unique_ptr<Mesh> m(import_svg_extruded("./test/data/svg/hidden_shape.svg", opt));
    ASSERT_NE(m, nullptr);

    // Meme compte que square.svg (tu_cgmesh_svg.cpp) : un seul rectangle extrude.
    EXPECT_EQ(m->GetNVertices(), 16u);
    EXPECT_EQ(m->GetNFaces(),    12u);
}
