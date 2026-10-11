#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/print_validation.h"

// ===========================================================================
//  Validateur de fabrication, coeur 2D -- chaque detecteur prouve capable
//  d'echouer
// ===========================================================================
//
// Chaque defaut a un temoin positif juste sous son seuil (0,39 mm pour une
// paroi de 0,4) et un temoin negatif juste au-dessus (0,41). Les formes sont
// decrites en millimetres puis converties en unites a TROIS echelles, dont
// aucune ne vaut 1 : 0,1 et 2,5 mm par unite, et 1 000 (modele en metres, cas
// d'un glTF). Une conversion oubliee ou inversee manque l'un des temoins ; une
// tolerance d'arrondi qui ne suit pas l'echelle se trahit a 1 000.

namespace
{
    using Pts = std::vector<Vector2f>;

    // 1 u = 0,1 mm, 1 u = 2,5 mm, 1 u = 1 m.
    const double kScales[] = {0.1, 2.5, 1000.0};

    Pts rectMm(double x0, double y0, double x1, double y1)
    {
        return {Vector2f((float)x0, (float)y0), Vector2f((float)x1, (float)y0),
                Vector2f((float)x1, (float)y1), Vector2f((float)x0, (float)y1)};   // CCW
    }

    Pts reversed(Pts p)
    {
        std::reverse(p.begin(), p.end());
        return p;
    }

    // Deux blocs carres de cote a, relies par une barre de longueur l et de
    // largeur w centree en hauteur. CCW.
    Pts dumbbellMm(double a, double l, double w)
    {
        const double c0 = 0.5 * a - 0.5 * w, c1 = 0.5 * a + 0.5 * w;
        const double xs[] = {0, a, a, a + l, a + l, 2 * a + l, 2 * a + l, a + l, a + l, a, a, 0};
        const double ys[] = {0, 0, c0, c0, 0, 0, a, a, c1, c1, a, a};
        Pts p;
        for (int i = 0; i < 12; ++i)
            p.push_back(Vector2f((float)xs[i], (float)ys[i]));
        return p;
    }

    // Rectangle 4 x 3 surmonte d'une pointe d'angle `apexDeg`. CCW.
    Pts tipMm(double apexDeg)
    {
        const double h = 2.0 / std::tan(0.5 * apexDeg * 3.14159265358979323846 / 180.0);
        return {Vector2f(0.f, 0.f), Vector2f(4.f, 0.f), Vector2f(4.f, 3.f),
                Vector2f(2.f, (float)(3.0 + h)), Vector2f(0.f, 3.f)};
    }

    // Contours en millimetres -> region en unites a l'echelle `mmPerUnit`.
    // `offset` (en unites) eloigne la region de l'origine.
    std::vector<ExtrudeContour> toUnits(const std::vector<Pts> &mm, double mmPerUnit, double offset = 0.0)
    {
        std::vector<ExtrudeContour> out;
        for (const Pts &c : mm)
        {
            ExtrudeContour e;
            for (const Vector2f &p : c)
                e.pts.push_back(Vector2f((float)(p.x / mmPerUnit + offset), (float)(p.y / mmPerUnit + offset)));
            out.push_back(std::move(e));
        }
        return out;
    }

    PrintabilityReport check(const std::vector<Pts> &mm, double mmPerUnit,
                             PrintConstraints c = PrintConstraints())
    {
        c.mmPerUnit = mmPerUnit;
        return CheckPrintRegion(toUnits(mm, mmPerUnit), c);
    }

    // Cercle de rayon `rad` (mm), 128 segments, CCW.
    Pts discMm(double cx, double cy, double rad)
    {
        Pts p;
        for (int k = 0; k < 128; ++k)
        {
            const double a = 2.0 * 3.14159265358979323846 * k / 128;
            p.push_back(Vector2f((float)(cx + rad * std::cos(a)), (float)(cy + rad * std::sin(a))));
        }
        return p;
    }

    std::size_t defectTotal(const PrintabilityReport &r)
    {
        std::size_t n = 0;
        for (std::size_t k = 0; k < kPrintDefectCount; ++k)
            if (!IsPrintInformation(static_cast<PrintDefect>(k)))
                n += r.counts[k];
        return n;
    }
}

TEST(TEST_cgmesh_print_validation, names_are_stable)
{
    EXPECT_STREQ(PrintDefectName(PrintDefect::ThinWall), "thin_wall");
    EXPECT_STREQ(PrintDefectName(PrintDefect::ThinIsland), "thin_island");
    EXPECT_STREQ(PrintDefectName(PrintDefect::FragileTip), "fragile_tip");
    EXPECT_STREQ(PrintDefectName(PrintDefect::ClosedOpening), "closed_opening");
    EXPECT_STREQ(PrintDefectName(PrintDefect::NarrowOpening), "narrow_opening");
    EXPECT_STREQ(PrintDefectName(PrintDefect::ThinSlab), "thin_slab");
    EXPECT_STREQ(PrintDefectName(PrintDefect::DetailTooSmall), "detail_too_small");
    EXPECT_STREQ(PrintDefectName(PrintDefect::EnvelopeExceeded), "envelope_exceeded");
    EXPECT_STREQ(PrintDefectName(PrintDefect::BluntedVoidCorner), "blunted_void_corner");
    EXPECT_STREQ(PrintDefectName(PrintDefect::ScaleUnset), "scale_unset");
    EXPECT_EQ(kPrintDefectCount, 10u);
    EXPECT_TRUE(IsPrintInformation(PrintDefect::BluntedVoidCorner));
    EXPECT_TRUE(IsPrintInformation(PrintDefect::ScaleUnset));
    EXPECT_FALSE(IsPrintInformation(PrintDefect::ThinWall));
}

TEST(TEST_cgmesh_print_validation, defaults_are_the_documented_values)
{
    const PrintConstraints c;
    EXPECT_FALSE(c.mmPerUnit.has_value());
    EXPECT_DOUBLE_EQ(c.minWallMm, 0.4);
    EXPECT_DOUBLE_EQ(c.minHoleMm, 0.5);
    EXPECT_DOUBLE_EQ(c.minDetailMm, 0.3);
    EXPECT_DOUBLE_EQ(c.maxTipLengthMm, 0.3);
    EXPECT_DOUBLE_EQ(c.noiseDepthRatio, 0.25);
    EXPECT_FALSE(c.envelopeMm.has_value());
}

// --- echelle absente --------------------------------------------------------

TEST(TEST_cgmesh_print_validation, without_scale_nothing_is_measured)
{
    PrintConstraints c;   // mmPerUnit absente
    const PrintabilityReport r = CheckPrintRegion(toUnits({dumbbellMm(3, 3, 0.39)}, 0.1), c);
    EXPECT_FALSE(r.evaluated);
    EXPECT_EQ(r.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_EQ(defectTotal(r), 0u);
    EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 0u);
    EXPECT_FALSE(r.IsPrintable());
    ASSERT_EQ(r.records.size(), 1u);
    EXPECT_EQ(r.records[0].kind, PrintDefect::ScaleUnset);
}

TEST(TEST_cgmesh_print_validation, out_of_domain_constraints_throw)
{
    const std::vector<ExtrudeContour> region = toUnits({rectMm(0, 0, 3, 3)}, 1.0);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto bad = [&](auto mutate)
    {
        PrintConstraints c;
        c.mmPerUnit = 1.0;
        mutate(c);
        return c;
    };
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.mmPerUnit = 0.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([&](PrintConstraints &c) { c.mmPerUnit = nan; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.minWallMm = -1.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.minHoleMm = 0.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.maxTipLengthMm = 0.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([&](PrintConstraints &c) { c.maxTipLengthMm = nan; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.noiseDepthRatio = 1.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.noiseDepthRatio = -0.1; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.minDetailMm = 0.0; })), std::invalid_argument);
    EXPECT_THROW(CheckPrintRegion(region, bad([](PrintConstraints &c) { c.envelopeMm = std::array<double, 3>{1, 0, 1}; })), std::invalid_argument);

    std::vector<ExtrudeContour> nonFinite = region;
    nonFinite[0].pts[1].x = std::numeric_limits<float>::infinity();
    PrintConstraints c;
    c.mmPerUnit = 1.0;
    EXPECT_THROW(CheckPrintRegion(nonFinite, c), std::invalid_argument);
}

TEST(TEST_cgmesh_print_validation, empty_region_is_printable)
{
    PrintConstraints c;
    c.mmPerUnit = 0.1;
    const PrintabilityReport r = CheckPrintRegion({}, c);
    EXPECT_TRUE(r.evaluated);
    EXPECT_EQ(defectTotal(r), 0u);
    EXPECT_TRUE(r.IsPrintable());
}

// --- paroi : col de matiere -------------------------------------------------

TEST(TEST_cgmesh_print_validation, bar_of_0_39_mm_between_blocks_is_a_thin_wall)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({dumbbellMm(3, 3, 0.39)}, s);
        EXPECT_TRUE(r.evaluated);
        EXPECT_DOUBLE_EQ(r.wallRadius, 0.2 / s);
        EXPECT_EQ(r.Count(PrintDefect::ThinWall), 1u);
        EXPECT_EQ(defectTotal(r), 1u);
        EXPECT_FALSE(r.IsPrintable());

        const auto it = std::find_if(r.records.begin(), r.records.end(),
                                     [](const PrintDefectRecord &d) { return d.kind == PrintDefect::ThinWall; });
        ASSERT_NE(it, r.records.end());
        EXPECT_EQ(it->contacts, 2u);
        EXPECT_DOUBLE_EQ(it->widthBelowMm, 0.4);
        // Le reste est la barre, ecornee a chaque bout par la calotte de
        // l'ouverture : boite en millimetres, quelle que soit l'echelle.
        EXPECT_NEAR(it->boxMm[0], 3.0, 0.2);
        EXPECT_NEAR(it->boxMm[2], 6.0, 0.2);
        EXPECT_NEAR(it->boxMm[1], 1.305, 1e-3);
        EXPECT_NEAR(it->boxMm[3], 1.695, 1e-3);
        // Profondeur plafonnee a 2 x max(0,25 x 0,2, 0,3) = 0,6 mm.
        EXPECT_DOUBLE_EQ(r.depthCapMm, 0.6);
        EXPECT_DOUBLE_EQ(it->depthMm, r.depthCapMm);
    }
}

TEST(TEST_cgmesh_print_validation, bar_of_0_41_mm_between_blocks_passes)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({dumbbellMm(3, 3, 0.41)}, s);
        EXPECT_EQ(r.Count(PrintDefect::ThinWall), 0u);
        EXPECT_EQ(defectTotal(r), 0u);
        EXPECT_TRUE(r.IsPrintable());
        EXPECT_EQ(r.Count(PrintDefect::ScaleUnset), 0u);
        // Les huit coins droits des blocs sont des pointes acceptees.
        EXPECT_EQ(r.bluntTips, 8u);
    }
}

// --- ilot : piece entierement mince ----------------------------------------

TEST(TEST_cgmesh_print_validation, detached_strip_of_0_39_mm_is_a_thin_island)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({rectMm(0, 0, 3, 3), rectMm(4, 1, 6, 1.39)}, s);
        EXPECT_EQ(r.Count(PrintDefect::ThinIsland), 1u);
        EXPECT_EQ(r.Count(PrintDefect::ThinWall), 0u);
        EXPECT_EQ(defectTotal(r), 1u);
        const auto it = std::find_if(r.records.begin(), r.records.end(),
                                     [](const PrintDefectRecord &d) { return d.kind == PrintDefect::ThinIsland; });
        ASSERT_NE(it, r.records.end());
        EXPECT_EQ(it->contacts, 0u);
        EXPECT_NEAR(it->areaMm2, 2.0 * 0.39, 1e-3);
    }
}

TEST(TEST_cgmesh_print_validation, detached_strip_of_0_41_mm_passes)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({rectMm(0, 0, 3, 3), rectMm(4, 1, 6, 1.41)}, s);
        EXPECT_EQ(defectTotal(r), 0u);
    }
}

// --- pointes ----------------------------------------------------------------

TEST(TEST_cgmesh_print_validation, tip_of_30_degrees_is_fragile)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({tipMm(30)}, s);
        EXPECT_EQ(r.Count(PrintDefect::FragileTip), 1u);
        EXPECT_EQ(defectTotal(r), 1u);
        const auto it = std::find_if(r.records.begin(), r.records.end(),
                                     [](const PrintDefectRecord &d) { return d.kind == PrintDefect::FragileTip; });
        ASSERT_NE(it, r.records.end());
        EXPECT_EQ(it->contacts, 1u);
        // Coin rectiligne de 30 degres : r (1 / sin 15 - 1) = 2,864 r, r = 0,2 mm.
        EXPECT_NEAR(it->depthMm, 0.2 * (1.0 / std::sin(15.0 * 3.14159265358979323846 / 180.0) - 1.0), 0.01);
    }
}

TEST(TEST_cgmesh_print_validation, tip_of_90_degrees_passes)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({tipMm(90)}, s);
        EXPECT_EQ(defectTotal(r), 0u);
        EXPECT_EQ(r.bluntTips, 3u);   // la pointe et les deux coins droits du pied
    }
}

// Partie mince d'une pointe rectiligne d'angle a : r (1 / sin(a / 2) - 1),
// r = 0,2 mm. 45,2 degres : 0,320 mm ; 49,2 degres : 0,280 mm, de part et
// d'autre du seuil de 0,3 mm.
TEST(TEST_cgmesh_print_validation, tip_length_threshold_brackets_0_3_mm)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        EXPECT_EQ(check({tipMm(45.2)}, s).Count(PrintDefect::FragileTip), 1u);
        EXPECT_EQ(check({tipMm(49.2)}, s).Count(PrintDefect::FragileTip), 0u);
    }
}

// Pointe de 30 degres : 0,573 mm. Coin droit : 0,083 mm, la pointe de 90
// degres comme les deux coins du pied.
TEST(TEST_cgmesh_print_validation, tip_length_is_configurable)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        PrintConstraints c;
        c.maxTipLengthMm = 0.55;
        EXPECT_EQ(check({tipMm(30)}, s, c).Count(PrintDefect::FragileTip), 1u);
        c.maxTipLengthMm = 0.6;
        EXPECT_EQ(check({tipMm(30)}, s, c).Count(PrintDefect::FragileTip), 0u);
        c.maxTipLengthMm = 0.06;
        EXPECT_EQ(check({tipMm(90)}, s, c).Count(PrintDefect::FragileTip), 3u);
    }
}

// --- vides --------------------------------------------------------------------

// Fente fermee de 4 mm dans une plaque 10 x 6. Plus etroite que le seuil, elle
// disparait entiere a l'ouverture du vide : aucun contact, donc ClosedOpening
// -- elle se bouche a l'impression. Plus large, il ne reste que ses quatre
// coins, une seule attache chacun : des informations.
TEST(TEST_cgmesh_print_validation, enclosed_slit_of_0_49_mm_is_a_closed_opening)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({rectMm(0, 0, 10, 6), reversed(rectMm(3, 3 - 0.245, 7, 3 + 0.245))}, s);
        EXPECT_DOUBLE_EQ(r.holeRadius, 0.25 / s);
        EXPECT_EQ(r.Count(PrintDefect::ClosedOpening), 1u);
        EXPECT_EQ(r.Count(PrintDefect::NarrowOpening), 0u);
        EXPECT_EQ(defectTotal(r), 1u);
    }
}

TEST(TEST_cgmesh_print_validation, enclosed_slit_of_0_51_mm_passes)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({rectMm(0, 0, 10, 6), reversed(rectMm(3, 3 - 0.255, 7, 3 + 0.255))}, s);
        EXPECT_EQ(defectTotal(r), 0u);
        EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 4u);
        EXPECT_TRUE(r.IsPrintable());
    }
}

// Deux trous de 2 x 2 relies par une fente de 1,5 mm : la fente est un col du
// vide, attachee aux deux trous, donc NarrowOpening.
TEST(TEST_cgmesh_print_validation, slit_of_0_49_mm_between_holes_is_a_narrow_opening)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const Pts hole = reversed(dumbbellMm(2, 1.5, 0.49));
        Pts moved;
        for (const Vector2f &p : hole)
            moved.push_back(Vector2f(p.x + 2.f, p.y + 2.f));
        const PrintabilityReport r = check({rectMm(0, 0, 9.5, 6), moved}, s);
        EXPECT_EQ(r.Count(PrintDefect::NarrowOpening), 1u);
        EXPECT_EQ(r.Count(PrintDefect::ClosedOpening), 0u);
        EXPECT_EQ(defectTotal(r), 1u);
        const auto it = std::find_if(r.records.begin(), r.records.end(),
                                     [](const PrintDefectRecord &d) { return d.kind == PrintDefect::NarrowOpening; });
        ASSERT_NE(it, r.records.end());
        EXPECT_EQ(it->contacts, 2u);
        EXPECT_DOUBLE_EQ(it->widthBelowMm, 0.5);
    }
}

TEST(TEST_cgmesh_print_validation, slit_of_0_51_mm_between_holes_passes)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const Pts hole = reversed(dumbbellMm(2, 1.5, 0.51));
        Pts moved;
        for (const Vector2f &p : hole)
            moved.push_back(Vector2f(p.x + 2.f, p.y + 2.f));
        const PrintabilityReport r = check({rectMm(0, 0, 9.5, 6), moved}, s);
        EXPECT_EQ(defectTotal(r), 0u);
        EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 8u);
    }
}

// Deux pieces separees par 0,3 mm : aucun trou de l'enveloppe, mais un passage
// entre elles qui se fermera. Les vides sont pris dans une boite, pas dans les
// seuls trous.
TEST(TEST_cgmesh_print_validation, gap_between_separate_parts_is_seen)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        EXPECT_EQ(check({rectMm(0, 0, 3, 3), rectMm(3.3, 0, 6.3, 3)}, s).Count(PrintDefect::NarrowOpening), 1u);
        EXPECT_EQ(check({rectMm(0, 0, 3, 3), rectMm(3.6, 0, 6.6, 3)}, s).Count(PrintDefect::NarrowOpening), 0u);
    }
}

// --- bruit de discretisation ------------------------------------------------

// Un sommet d'angle interieur a laisse un reste de profondeur
// r (1 / sin(a / 2) - 1). Polygone regulier de n cotes : r (1 / cos(pi / n) - 1),
// soit 4e-5 r a n = 360 -- sous la tolerance d'arrondi (1e-3 r), ecarte avant
// tout classement -- et 0,0086 r a n = 24 -- au-dessus, ecarte par le filtre de
// bruit (0,25 r). Dans les deux cas, aucun defaut ni coin de vide.
TEST(TEST_cgmesh_print_validation, discretised_disc_has_no_defect)
{
    for (int n : {360, 24})
        for (double s : kScales)
        {
            SCOPED_TRACE(n);
            SCOPED_TRACE(s);
            PrintConstraints c;
            c.mmPerUnit = s;
            std::vector<ExtrudeContour> disc(1);
            disc[0].pts = circleContour(0.f, 0.f, (float)(10 * 0.2 / s), n);
            const PrintabilityReport r = CheckPrintRegion(disc, c);
            EXPECT_EQ(defectTotal(r), 0u);
            EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 0u);
        }
}

// Le meme polygone en TROU, de rayon 10 r_trou : chaque sommet est un coin
// convexe du vide. Le compte de bruit est lu par difference avec la plaque
// pleine (ses propres restes : les coins de la boite des vides) : 24 restes
// filtres a n = 24 ; aucun a n = 360, ecartes plus tot par la tolerance
// d'arrondi. Sans le filtre, 24 coins de vide.
TEST(TEST_cgmesh_print_validation, discretised_round_hole_has_no_blunted_corner)
{
    for (int n : {360, 24})
        for (double s : kScales)
        {
            SCOPED_TRACE(n);
            SCOPED_TRACE(s);
            PrintConstraints c;
            c.mmPerUnit = s;
            const std::vector<ExtrudeContour> full = toUnits({rectMm(-4, -4, 4, 4)}, s);
            std::vector<ExtrudeContour> plate = full;
            ExtrudeContour hole;
            hole.pts = circleContour(0.f, 0.f, (float)(10 * 0.25 / s), n);
            std::reverse(hole.pts.begin(), hole.pts.end());
            plate.push_back(hole);
            const PrintabilityReport r = CheckPrintRegion(plate, c);
            const PrintabilityReport base = CheckPrintRegion(full, c);
            EXPECT_EQ(defectTotal(r), 0u);
            EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 0u);
            EXPECT_EQ(r.noisePieces - base.noisePieces, n == 24 ? 24u : 0u);
        }
}

// --- rapport ------------------------------------------------------------------

TEST(TEST_cgmesh_print_validation, records_are_capped_counts_are_not)
{
    PrintConstraints c;
    c.maxRecordsPerKind = 1;
    const PrintabilityReport r =
        check({rectMm(0, 0, 3, 3), rectMm(4, 1, 6, 1.2), rectMm(4, 2, 6, 2.2)}, 0.1, c);
    EXPECT_EQ(r.Count(PrintDefect::ThinIsland), 2u);
    EXPECT_EQ(std::count_if(r.records.begin(), r.records.end(),
                            [](const PrintDefectRecord &d) { return d.kind == PrintDefect::ThinIsland; }),
              1);
}

// --- region eloignee de l'origine -------------------------------------------

// A 3e4 unites de l'origine, l'ulp float vaut 2e-3 u ; la region doit etre lue
// autour de son centre, pas de l'origine.
TEST(TEST_cgmesh_print_validation, region_far_from_the_origin_is_measured_alike)
{
    PrintConstraints c;
    c.mmPerUnit = 0.1;
    EXPECT_EQ(CheckPrintRegion(toUnits({dumbbellMm(3, 3, 0.39)}, 0.1, 3e4), c).Count(PrintDefect::ThinWall), 1u);
    const PrintabilityReport ok = CheckPrintRegion(toUnits({dumbbellMm(3, 3, 0.41)}, 0.1, 3e4), c);
    EXPECT_EQ(defectTotal(ok), 0u);
    EXPECT_EQ(ok.bluntTips, 8u);
    const PrintabilityReport t = CheckPrintRegion(toUnits({tipMm(30)}, 0.1, 3e4), c);
    ASSERT_EQ(t.Count(PrintDefect::FragileTip), 1u);
    // Boite rendue dans le repere du modele, en millimetres.
    for (const PrintDefectRecord &d : t.records)
    {
        if (d.kind == PrintDefect::FragileTip)
        {
            EXPECT_NEAR(d.boxMm[3], 3e3 + 3.0 + 2.0 / std::tan(15.0 * 3.14159265358979323846 / 180.0), 0.01);
        }
    }
}

// --- cols courts : pincements que le reste ne voit pas ----------------------

// Barre de 0,05 mm de large et de 0,001 a 0,11 mm de long entre deux blocs.
// Les erosions des blocs sont a l + 0,398 mm l'une de l'autre : a 0,001, leurs
// dilatations se recouvrent (pincement, aucun reste) ; au-dela, le reste est
// trop peu profond pour passer le filtre de bruit, mais il relie deux masses
// distinctes. (A l = 0, les deux fentes de largeur nulle du contour disparaissent
// a l'union : la region est un rectangle plein, sans col.)
TEST(TEST_cgmesh_print_validation, short_neck_between_blocks_is_a_thin_wall)
{
    for (double l : {0.001, 0.01, 0.05, 0.11})
        for (double s : kScales)
        {
            SCOPED_TRACE(l);
            SCOPED_TRACE(s);
            const PrintabilityReport r = check({dumbbellMm(3, l, 0.05)}, s);
            EXPECT_EQ(r.Count(PrintDefect::ThinWall), 1u);
            EXPECT_FALSE(r.IsPrintable());
        }
    for (double s : kScales)
        EXPECT_EQ(check({dumbbellMm(3, 0.11, 0.41)}, s).Count(PrintDefect::ThinWall), 0u);
}

// Deux disques de 1 mm de rayon qui se recouvrent de `ov` : col de
// 2 racine(1 - (1 - ov / 2)^2) mm, soit 0,063 (ov 0,001), 0,20 (0,01), 0,44
// (0,05). L'ouverture rend la reunion telle quelle : aucun reste, mais
// l'erosion se coupe en deux.
TEST(TEST_cgmesh_print_validation, overlapping_discs_pinch_is_a_thin_wall)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        for (double ov : {0.001, 0.01})
        {
            SCOPED_TRACE(ov);
            EXPECT_EQ(check({discMm(0, 0, 1), discMm(2 - ov, 0, 1)}, s).Count(PrintDefect::ThinWall), 1u);
        }
        EXPECT_EQ(check({discMm(0, 0, 1), discMm(1.95, 0, 1)}, s).Count(PrintDefect::ThinWall), 0u);
    }
}

// Meme pincement dans un vide : deux trous ronds secants.
TEST(TEST_cgmesh_print_validation, intersecting_round_holes_pinch_is_a_narrow_opening)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        for (double ov : {0.01, 0.1})
        {
            SCOPED_TRACE(ov);
            std::vector<ExtrudeContour> a(1), b(1);
            a[0].pts = discMm(3, 3, 1);
            b[0].pts = discMm(5 - ov, 3, 1);
            const std::vector<ExtrudeContour> holes = unionContours(a, b);
            ASSERT_EQ(holes.size(), 1u);
            const std::vector<Pts> mm = {rectMm(0, 0, 8, 6), reversed(holes[0].pts)};
            // ov 0,01 : col de 0,20 mm ; ov 0,1 : 0,62 mm.
            EXPECT_EQ(check(mm, s).Count(PrintDefect::NarrowOpening), ov < 0.05 ? 1u : 0u);
        }
    }
}

// --- seuils reglables : fourchettes -------------------------------------------

TEST(TEST_cgmesh_print_validation, wall_threshold_is_configurable)
{
    PrintConstraints c;
    c.minWallMm = 0.6;
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        EXPECT_EQ(check({dumbbellMm(3, 3, 0.59)}, s, c).Count(PrintDefect::ThinWall), 1u);
        EXPECT_EQ(check({dumbbellMm(3, 3, 0.61)}, s, c).Count(PrintDefect::ThinWall), 0u);
    }
}

TEST(TEST_cgmesh_print_validation, hole_threshold_is_configurable)
{
    PrintConstraints c;
    c.minHoleMm = 0.8;
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        EXPECT_EQ(check({rectMm(0, 0, 10, 6), reversed(rectMm(3, 3 - 0.395, 7, 3 + 0.395))}, s, c).Count(PrintDefect::ClosedOpening), 1u);
        EXPECT_EQ(check({rectMm(0, 0, 10, 6), reversed(rectMm(3, 3 - 0.405, 7, 3 + 0.405))}, s, c).Count(PrintDefect::ClosedOpening), 0u);
    }
}

// Trou a 24 segments : restes de 0,0086 r_trou, sous le filtre par defaut
// (0,25 r_trou), au-dessus d'un filtre de 0,005 r_trou : 24 coins de vide.
TEST(TEST_cgmesh_print_validation, noise_ratio_is_configurable)
{
    PrintConstraints c;
    c.noiseDepthRatio = 0.005;
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        std::vector<Pts> mm = {rectMm(-4, -4, 4, 4)};
        mm.push_back(reversed(circleContour(0.f, 0.f, 2.5f, 24)));
        const PrintabilityReport r = check(mm, s, c);
        EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 24u);
        EXPECT_EQ(defectTotal(r), 0u);
    }
}

// Filtre nul : la boite des vides ne doit rien produire.
TEST(TEST_cgmesh_print_validation, void_box_yields_nothing_even_without_noise_filter)
{
    PrintConstraints c;
    c.noiseDepthRatio = 0.0;
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = check({rectMm(0, 0, 3, 3)}, s, c);
        EXPECT_EQ(r.Count(PrintDefect::BluntedVoidCorner), 0u);
        EXPECT_EQ(defectTotal(r), 0u);
    }
}

// --- domaine des entrees ------------------------------------------------------

TEST(TEST_cgmesh_print_validation, inputs_outside_the_measurable_domain_throw_invalid_argument)
{
    const std::vector<ExtrudeContour> square = toUnits({rectMm(0, 0, 3, 3)}, 1.0);
    // Emprise de 3e12 mm : la precision float ne porte plus le rayon d'ouverture.
    {
        std::vector<ExtrudeContour> huge = square;
        for (Vector2f &p : huge[0].pts)
        {
            p.x *= 1e12f;
            p.y *= 1e12f;
        }
        PrintConstraints c;
        c.mmPerUnit = 1.0;
        EXPECT_THROW(CheckPrintRegion(huge, c), std::invalid_argument);
    }
    {
        PrintConstraints c;
        c.mmPerUnit = 1e38;
        EXPECT_THROW(CheckPrintRegion(square, c), std::invalid_argument);
    }
    {
        PrintConstraints c;
        c.mmPerUnit = 1.0;
        c.minWallMm = 1e30;
        EXPECT_THROW(CheckPrintRegion(square, c), std::invalid_argument);
        c.minWallMm = 1e-9;
        EXPECT_THROW(CheckPrintRegion(square, c), std::invalid_argument);
    }
}

// Piece plus petite que la resolution (3e-38 mm) : la matiere existe, elle est
// plus mince que tout seuil.
TEST(TEST_cgmesh_print_validation, part_below_resolution_is_a_thin_island)
{
    PrintConstraints c;
    c.mmPerUnit = 1e-38;
    PrintabilityReport r;
    EXPECT_NO_THROW(r = CheckPrintRegion(toUnits({rectMm(0, 0, 3, 3)}, 1.0), c));
    EXPECT_EQ(r.Count(PrintDefect::ThinIsland), 1u);
}

// Les messages de domaine donnent les valeurs en cause, pas des noms de
// constantes.
TEST(TEST_cgmesh_print_validation, domain_messages_give_the_values)
{
    auto message = [](const std::vector<ExtrudeContour> &region, const PrintConstraints &c)
    {
        try
        {
            CheckPrintRegion(region, c);
        }
        catch (const std::invalid_argument &e)
        {
            return std::string(e.what());
        }
        return std::string("no exception");
    };
    const std::vector<ExtrudeContour> square = toUnits({rectMm(0, 0, 3, 3)}, 1.0);
    {
        PrintConstraints c;
        c.mmPerUnit = 1.0;
        c.minWallMm = 1e30;
        const std::string m = message(square, c);
        EXPECT_NE(m.find("minWallMm 1e+30"), std::string::npos) << m;
        EXPECT_NE(m.find("[0.002, 1000] mm"), std::string::npos) << m;
        EXPECT_EQ(m.find("kM"), std::string::npos) << m;
    }
    {
        PrintConstraints c;
        c.mmPerUnit = 0.0;
        const std::string m = message(square, c);
        EXPECT_NE(m.find("mmPerUnit 0 "), std::string::npos) << m;
    }
    {
        // Demi-emprise 1 500 mm ; borne 0,01 x 0,2 / (16 FLT_EPSILON) = 1 048,58 mm.
        PrintConstraints c;
        c.mmPerUnit = 1000.0;
        const std::string m = message(square, c);
        EXPECT_NE(m.find("half-extent 1500 mm exceeds 1048.58 mm for minWallMm 0.4"), std::string::npos) << m;
        c.minHoleMm = 0.3;
        const std::string h = message(square, c);
        EXPECT_NE(h.find("exceeds 786.432 mm for minHoleMm 0.3"), std::string::npos) << h;
    }
}

TEST(TEST_cgmesh_print_validation, scale_unset_record_obeys_the_record_cap)
{
    PrintConstraints c;
    c.maxRecordsPerKind = 0;
    const PrintabilityReport r = CheckPrintRegion(toUnits({rectMm(0, 0, 3, 3)}, 1.0), c);
    EXPECT_EQ(r.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_TRUE(r.records.empty());
}

// --- cout ---------------------------------------------------------------------

// Grille de 60 x 60 cellules de 1 mm, barreaux de 0,2 mm, dans un cadre de
// 2 mm : un seul morceau du reste a 3 600 trous.
TEST(TEST_cgmesh_print_validation, dense_lattice_is_checked_in_well_under_a_second)
{
    const int n = 60;
    const double w = 0.2, l = n * 1.0 + w;
    std::vector<ExtrudeContour> region(1);
    region[0].pts = rectMm(-2, -2, l + 2, l + 2);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            ExtrudeContour h;
            h.pts = reversed(rectMm(i + w, j + w, i + 1.0, j + 1.0));
            region.push_back(h);
        }
    PrintConstraints c;
    c.mmPerUnit = 1.0;
    const auto t0 = std::chrono::steady_clock::now();
    const PrintabilityReport r = CheckPrintRegion(region, c);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    RecordProperty("check_ms_lattice_60", std::to_string(ms));
    EXPECT_GE(r.Count(PrintDefect::ThinWall), 1u);
#if defined(NDEBUG) && !defined(CG_SANITIZERS_ENABLED) && !defined(__SANITIZE_ADDRESS__)
    // Borne de temps sur build optimisee sans sanitizer seulement : en Debug ou
    // sous ASan (CG_ENABLE_SANITIZERS, qui garde NDEBUG en Release), le cout est
    // mesure (propriete) mais pas asserte.
    EXPECT_LT(ms, 1000.0);
#endif
}

// --- extrusion droite : epaisseur de la plaque ------------------------------

// Plaque de 3 x 3 mm, epaisse de 0,39 puis 0,41 mm : de part et d'autre de
// minWallMm, quelle que soit l'echelle. L'epaisseur est donnee en unites.
TEST(TEST_cgmesh_print_validation, extrusion_thinner_than_a_wall_is_a_thin_slab)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        PrintConstraints c;
        c.mmPerUnit = s;
        const std::vector<ExtrudeContour> plate = toUnits({rectMm(0, 0, 3, 3)}, s);
        const PrintabilityReport thin = CheckPrintExtrusion(plate, 0.39 / s, c);
        EXPECT_EQ(thin.Count(PrintDefect::ThinSlab), 1u);
        EXPECT_EQ(defectTotal(thin), 1u);
        EXPECT_FALSE(thin.IsPrintable());
        const auto it = std::find_if(thin.records.begin(), thin.records.end(),
                                     [](const PrintDefectRecord &d) { return d.kind == PrintDefect::ThinSlab; });
        ASSERT_NE(it, thin.records.end());
        EXPECT_DOUBLE_EQ(it->widthBelowMm, 0.4);
        EXPECT_NEAR(it->boxMm[0], 0.0, 1e-3);
        EXPECT_NEAR(it->boxMm[1], 0.0, 1e-3);
        EXPECT_NEAR(it->boxMm[2], 3.0, 1e-3);
        EXPECT_NEAR(it->boxMm[3], 3.0, 1e-3);

        const PrintabilityReport thick = CheckPrintExtrusion(plate, 0.41 / s, c);
        EXPECT_EQ(defectTotal(thick), 0u);
        EXPECT_TRUE(thick.IsPrintable());
        // Une feuille d'epaisseur nulle est une plaque mince.
        EXPECT_EQ(CheckPrintExtrusion(plate, 0.0, c).Count(PrintDefect::ThinSlab), 1u);
    }
}

// Le seuil de plaque est minWallMm, reglable.
TEST(TEST_cgmesh_print_validation, slab_threshold_follows_min_wall)
{
    PrintConstraints c;
    c.mmPerUnit = 0.1;
    c.minWallMm = 0.6;
    const std::vector<ExtrudeContour> plate = toUnits({rectMm(0, 0, 3, 3)}, 0.1);
    EXPECT_EQ(CheckPrintExtrusion(plate, 5.9, c).Count(PrintDefect::ThinSlab), 1u);
    EXPECT_EQ(CheckPrintExtrusion(plate, 6.1, c).Count(PrintDefect::ThinSlab), 0u);
}

// Les defauts de la region s'ajoutent a celui de la plaque.
TEST(TEST_cgmesh_print_validation, extrusion_keeps_the_region_defects)
{
    PrintConstraints c;
    c.mmPerUnit = 2.5;
    const PrintabilityReport r = CheckPrintExtrusion(toUnits({dumbbellMm(3, 3, 0.39)}, 2.5), 0.1, c);
    EXPECT_EQ(r.Count(PrintDefect::ThinWall), 1u);
    EXPECT_EQ(r.Count(PrintDefect::ThinSlab), 1u);
}

TEST(TEST_cgmesh_print_validation, slab_record_obeys_the_record_cap)
{
    PrintConstraints c;
    c.mmPerUnit = 0.1;
    c.maxRecordsPerKind = 0;
    const PrintabilityReport r = CheckPrintExtrusion(toUnits({rectMm(0, 0, 3, 3)}, 0.1), 1.0, c);
    EXPECT_EQ(r.Count(PrintDefect::ThinSlab), 1u);
    EXPECT_TRUE(r.records.empty());
}

// Les messages ne dependent pas de la locale globale : point decimal, sans
// separateur de milliers.
TEST(TEST_cgmesh_print_validation, domain_messages_ignore_the_global_locale)
{
    std::locale comma;
    bool found = false;
    for (const char *name : {"fr_FR.UTF-8", "fr_FR", "fr-FR", "French_France.1252", "de_DE.UTF-8", "de-DE"})
    {
        try
        {
            comma = std::locale(name);
            found = true;
            break;
        }
        catch (const std::runtime_error &)
        {
        }
    }
    if (!found)
        GTEST_SKIP() << "no locale with a decimal comma on this system";
    const std::locale previous = std::locale::global(comma);
    std::string m, h;
    try
    {
        PrintConstraints c;
        c.mmPerUnit = 1000.0;
        CheckPrintRegion(toUnits({rectMm(0, 0, 3, 3)}, 1.0), c);
    }
    catch (const std::invalid_argument &e)
    {
        m = e.what();
    }
    try
    {
        PrintConstraints c;
        c.mmPerUnit = 1.0;
        CheckPrintExtrusion(toUnits({rectMm(0, 0, 3, 3)}, 1.0), -1.5, c);
    }
    catch (const std::invalid_argument &e)
    {
        h = e.what();
    }
    std::locale::global(previous);
    EXPECT_NE(m.find("half-extent 1500 mm exceeds 1048.58 mm for minWallMm 0.4"), std::string::npos) << m;
    EXPECT_NE(h.find("height -1.5 "), std::string::npos) << h;
}

TEST(TEST_cgmesh_print_validation, extrusion_without_scale_or_matter_has_no_slab)
{
    PrintConstraints c;
    const PrintabilityReport unset = CheckPrintExtrusion(toUnits({rectMm(0, 0, 3, 3)}, 0.1), 0.0, c);
    EXPECT_FALSE(unset.evaluated);
    EXPECT_EQ(unset.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_EQ(unset.Count(PrintDefect::ThinSlab), 0u);

    c.mmPerUnit = 0.1;
    const PrintabilityReport empty = CheckPrintExtrusion({}, 0.0, c);
    EXPECT_EQ(empty.Count(PrintDefect::ThinSlab), 0u);
    EXPECT_TRUE(empty.IsPrintable());
}

TEST(TEST_cgmesh_print_validation, extrusion_height_outside_its_domain_throws)
{
    PrintConstraints c;
    c.mmPerUnit = 0.1;
    const std::vector<ExtrudeContour> plate = toUnits({rectMm(0, 0, 3, 3)}, 0.1);
    EXPECT_THROW(CheckPrintExtrusion(plate, -1.0, c), std::invalid_argument);
    EXPECT_THROW(CheckPrintExtrusion(plate, std::numeric_limits<double>::quiet_NaN(), c), std::invalid_argument);
    EXPECT_THROW(CheckPrintExtrusion(plate, std::numeric_limits<double>::infinity(), c), std::invalid_argument);
}

// ===========================================================================
//  Pile de tranches 2,5D (CheckPrintSlices)
// ===========================================================================
//
// Formes en millimetres, cotes en millimetres, converties en unites aux trois
// echelles. La matiere se juge par tranche, les vides sur la silhouette.

namespace
{
    PrintSlice sliceMm(double zLow, double zHigh, double zCut, const std::vector<Pts> &mm, double mmPerUnit)
    {
        PrintSlice s;
        s.zLow = zLow / mmPerUnit;
        s.zHigh = zHigh / mmPerUnit;
        s.zCut = zCut / mmPerUnit;
        s.region = toUnits(mm, mmPerUnit);
        return s;
    }

    PrintabilityReport checkSlices(const std::vector<PrintSlice> &slices, double mmPerUnit,
                                   PrintConstraints c = PrintConstraints())
    {
        c.mmPerUnit = mmPerUnit;
        return CheckPrintSlices(slices, c);
    }

    // Plaque 10 x 6 percee d'une fente fermee de largeur w, longueur 4, centree.
    std::vector<Pts> slottedPlateMm(double w)
    {
        return {rectMm(0, 0, 10, 6), reversed(rectMm(3, 3 - 0.5 * w, 7, 3 + 0.5 * w))};
    }

    std::size_t sliceCount(const PrintSliceSummary &s, PrintDefect kind)
    {
        return s.counts[static_cast<std::size_t>(kind)];
    }
}

// Barre de 0,39 mm dans la tranche basse, de 0,6 dans la haute : un ThinWall,
// enregistre sur la tranche basse avec sa couche en mm. La silhouette (reunion)
// a la barre de 0,6 : la matiere ne s'y juge pas.
TEST(TEST_cgmesh_print_validation, slices_report_matter_per_slice_with_z_in_mm)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const std::vector<PrintSlice> stack = {sliceMm(0, 2, 1, {dumbbellMm(3, 2, 0.39)}, s),
                                               sliceMm(2, 4, 3, {dumbbellMm(3, 2, 0.6)}, s)};
        const PrintabilityReport r = checkSlices(stack, s);
        ASSERT_TRUE(r.evaluated);
        EXPECT_EQ(r.Count(PrintDefect::ThinWall), 1u);
        EXPECT_EQ(defectTotal(r), 1u);
        ASSERT_EQ(r.slices.size(), 3u);
        EXPECT_EQ(sliceCount(r.slices[0], PrintDefect::ThinWall), 1u);
        EXPECT_EQ(sliceCount(r.slices[1], PrintDefect::ThinWall), 0u);
        EXPECT_NEAR(r.slices[0].zLowMm, 0.0, 1e-6);
        EXPECT_NEAR(r.slices[0].zHighMm, 2.0, 1e-6);
        EXPECT_NEAR(r.slices[0].zCutMm, 1.0, 1e-6);
        EXPECT_NEAR(r.slices[1].zCutMm, 3.0, 1e-6);
        EXPECT_NEAR(r.slices[0].areaMm2, 18.0 + 2 * 0.39, 1e-3);
        EXPECT_FALSE(r.slices[0].silhouette);
        EXPECT_TRUE(r.slices[2].silhouette);
        EXPECT_NEAR(r.slices[2].zLowMm, 0.0, 1e-6);
        EXPECT_NEAR(r.slices[2].zHighMm, 4.0, 1e-6);
        bool seen = false;
        for (const PrintDefectRecord &d : r.records)
            if (d.kind == PrintDefect::ThinWall)
            {
                seen = true;
                EXPECT_EQ(d.slice, 0);
                EXPECT_NEAR(d.zLowMm, 0.0, 1e-6);
                EXPECT_NEAR(d.zHighMm, 2.0, 1e-6);
                EXPECT_GE(d.boxMm[0], 2.9);    // la barre, x dans [3, 5] mm
                EXPECT_LE(d.boxMm[2], 5.1);
            }
        EXPECT_TRUE(seen);
    }
}

// Fente de 2 mm en bas, de 0,3 mm en haut : la silhouette n'a que la fente de
// 0,3, un ClosedOpening enregistre sur elle. La tranche basse seule n'a pas de
// defaut.
TEST(TEST_cgmesh_print_validation, voids_are_judged_on_the_silhouette)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = checkSlices({sliceMm(0, 1, 0.5, slottedPlateMm(2.0), s),
                                                  sliceMm(1, 2, 1.5, slottedPlateMm(0.3), s)}, s);
        EXPECT_EQ(r.Count(PrintDefect::ClosedOpening), 1u);
        EXPECT_EQ(defectTotal(r), 1u);
        ASSERT_EQ(r.slices.size(), 3u);
        EXPECT_EQ(sliceCount(r.slices[2], PrintDefect::ClosedOpening), 1u);
        for (const PrintDefectRecord &d : r.records)
        {
            if (d.kind == PrintDefect::ClosedOpening)
            {
                EXPECT_EQ(d.slice, 2);
            }
        }
        EXPECT_EQ(defectTotal(checkSlices({sliceMm(0, 1, 0.5, slottedPlateMm(2.0), s)}, s)), 0u);
    }
}

// Fente de 0,3 mm en bas, plaque pleine en haut : une poche fermee par la
// matiere de l'autre tranche n'est pas une ouverture. La meme section seule,
// region 2D, a son ClosedOpening (temoin).
TEST(TEST_cgmesh_print_validation, void_covered_by_another_slice_is_not_an_opening)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = checkSlices({sliceMm(0, 1, 0.5, slottedPlateMm(0.3), s),
                                                  sliceMm(1, 2, 1.5, {rectMm(0, 0, 10, 6)}, s)}, s);
        EXPECT_TRUE(r.evaluated);
        EXPECT_EQ(defectTotal(r), 0u);
        EXPECT_TRUE(r.IsPrintable());
        EXPECT_EQ(check(slottedPlateMm(0.3), s).Count(PrintDefect::ClosedOpening), 1u);
    }
}

// La meme barre mince dans deux tranches : comptee deux fois, une par tranche ;
// le plafond d'enregistrements vaut pour le rapport fusionne.
TEST(TEST_cgmesh_print_validation, slice_counts_are_merged_and_records_capped_once)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const std::vector<PrintSlice> stack = {sliceMm(0, 1, 0.5, {dumbbellMm(3, 2, 0.39)}, s),
                                               sliceMm(1, 2, 1.5, {dumbbellMm(3, 2, 0.39)}, s)};
        const PrintabilityReport r = checkSlices(stack, s);
        EXPECT_EQ(r.Count(PrintDefect::ThinWall), 2u);
        ASSERT_EQ(r.slices.size(), 3u);
        EXPECT_EQ(sliceCount(r.slices[0], PrintDefect::ThinWall), 1u);
        EXPECT_EQ(sliceCount(r.slices[1], PrintDefect::ThinWall), 1u);
        PrintConstraints one;
        one.maxRecordsPerKind = 1;
        const PrintabilityReport capped = checkSlices(stack, s, one);
        EXPECT_EQ(capped.Count(PrintDefect::ThinWall), 2u);
        std::size_t kept = 0;
        for (const PrintDefectRecord &d : capped.records)
            kept += d.kind == PrintDefect::ThinWall;
        EXPECT_EQ(kept, 1u);
    }
}

// Hauteur de la pile : 0,39 mm -> ThinSlab sur la silhouette ; 0,41 -> rien.
TEST(TEST_cgmesh_print_validation, stack_lower_than_a_wall_is_a_thin_slab)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const std::vector<Pts> plate = {rectMm(0, 0, 10, 6)};
        const PrintabilityReport thin = checkSlices({sliceMm(0, 0.2, 0.1, plate, s),
                                                     sliceMm(0.2, 0.39, 0.3, plate, s)}, s);
        EXPECT_EQ(thin.Count(PrintDefect::ThinSlab), 1u);
        ASSERT_EQ(thin.slices.size(), 3u);
        EXPECT_EQ(sliceCount(thin.slices[2], PrintDefect::ThinSlab), 1u);
        const PrintabilityReport thick = checkSlices({sliceMm(0, 0.2, 0.1, plate, s),
                                                      sliceMm(0.2, 0.41, 0.3, plate, s)}, s);
        EXPECT_TRUE(thick.evaluated);
        EXPECT_EQ(thick.Count(PrintDefect::ThinSlab), 0u);
        EXPECT_TRUE(thick.IsPrintable());
    }
}

TEST(TEST_cgmesh_print_validation, slice_stack_without_scale_or_slices)
{
    const std::vector<PrintSlice> stack = {sliceMm(0, 1, 0.5, {dumbbellMm(3, 2, 0.39)}, 0.1)};
    const PrintabilityReport unset = CheckPrintSlices(stack, PrintConstraints());
    EXPECT_FALSE(unset.evaluated);
    EXPECT_EQ(unset.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_EQ(defectTotal(unset), 0u);
    EXPECT_TRUE(unset.slices.empty());

    const PrintabilityReport empty = checkSlices({}, 0.1);
    EXPECT_TRUE(empty.evaluated);
    EXPECT_TRUE(empty.slices.empty());
    EXPECT_TRUE(empty.IsPrintable());
}

TEST(TEST_cgmesh_print_validation, slice_heights_outside_their_domain_throw)
{
    const std::vector<Pts> plate = {rectMm(0, 0, 10, 6)};
    PrintSlice inverted = sliceMm(1, 0, 0.5, plate, 1.0);
    PrintSlice outside = sliceMm(0, 1, 2, plate, 1.0);
    PrintSlice notFinite = sliceMm(0, 1, 0.5, plate, 1.0);
    notFinite.zHigh = std::numeric_limits<double>::quiet_NaN();
    for (const PrintSlice &bad : {inverted, outside, notFinite})
    {
        try
        {
            checkSlices({sliceMm(0, 1, 0.5, plate, 1.0), bad}, 1.0);
            ADD_FAILURE() << "no exception";
        }
        catch (const std::invalid_argument &e)
        {
            EXPECT_NE(std::string(e.what()).find("CheckPrintSlices: slice 1"), std::string::npos) << e.what();
        }
    }
}

// Chaque section est lue sous NonZero : un contour seul, tourne dans le sens
// horaire, est de la matiere. La silhouette est la reunion des sections, pas la
// somme des enroulements : deux rectangles de sens opposes au meme endroit ne
// s'annulent pas.
TEST(TEST_cgmesh_print_validation, silhouette_is_the_union_of_the_sections)
{
    for (double s : kScales)
    {
        SCOPED_TRACE(s);
        const PrintabilityReport r = checkSlices({sliceMm(0, 1, 0.5, {reversed(rectMm(0, 0, 10, 6))}, s),
                                                  sliceMm(1, 2, 1.5, {rectMm(0, 0, 10, 6)}, s)}, s);
        ASSERT_EQ(r.slices.size(), 3u);
        EXPECT_NEAR(r.slices[0].areaMm2, 60.0, 1e-3);
        EXPECT_NEAR(r.slices[1].areaMm2, 60.0, 1e-3);
        EXPECT_NEAR(r.slices[2].areaMm2, 60.0, 1e-3);
        EXPECT_TRUE(r.IsPrintable());
    }
}

// Cotes finies qui debordent une fois converties en mm ; point non fini dans
// une section.
TEST(TEST_cgmesh_print_validation, slice_heights_overflowing_in_millimetres_throw)
{
    const std::vector<Pts> plate = {rectMm(0, 0, 10, 6)};
    PrintSlice huge = sliceMm(0, 1, 0.5, plate, 10.0);
    huge.zHigh = 1e308;
    try
    {
        checkSlices({huge}, 10.0);
        ADD_FAILURE() << "no exception";
    }
    catch (const std::invalid_argument &e)
    {
        EXPECT_NE(std::string(e.what()).find("CheckPrintSlices: slice 0"), std::string::npos) << e.what();
        EXPECT_NE(std::string(e.what()).find("1e+308"), std::string::npos) << e.what();
    }
    PrintSlice nanPoint = sliceMm(0, 1, 0.5, plate, 1.0);
    nanPoint.region[0].pts[1].x = std::numeric_limits<float>::quiet_NaN();
    try
    {
        checkSlices({sliceMm(0, 1, 0.5, plate, 1.0), nanPoint}, 1.0);
        ADD_FAILURE() << "no exception";
    }
    catch (const std::invalid_argument &e)
    {
        EXPECT_NE(std::string(e.what()).find("CheckPrintSlices: non-finite point"), std::string::npos) << e.what();
    }
}

// ===========================================================================
//  Enveloppe d'impression (CheckPrintEnvelope)
// ===========================================================================
//
// Une boite proche du modele par defaut de la page (232 x 371,5 x 20 u, centree
// en x, pied a y = -260) face a une enveloppe utile de 218 x 123 x 250 mm :
// depassee a 1 mm / u, logee a 0,1 mm / u, et a 0,5 mm / u (116 x 185,75 x
// 10 mm) logee SEULEMENT en tournant la piece (185,75 sur x, 116 sur y).

namespace
{
    PrintExtent pageModelBox()
    {
        PrintExtent e;
        e.lo = {-116.0, -260.0, 0.0};
        e.hi = {116.0, 111.5, 20.0};
        return e;
    }

    PrintConstraints withEnvelope(double mmPerUnit, std::array<double, 3> env)
    {
        PrintConstraints c;
        c.mmPerUnit = mmPerUnit;
        c.envelopeMm = env;
        return c;
    }
}

TEST(TEST_cgmesh_print_validation, envelope_is_exceeded_when_no_rotation_fits)
{
    const PrintabilityReport r = CheckPrintEnvelope(pageModelBox(), withEnvelope(1.0, {218, 123, 250}));
    EXPECT_TRUE(r.evaluated);
    EXPECT_EQ(r.Count(PrintDefect::EnvelopeExceeded), 1u);
    ASSERT_EQ(r.records.size(), 1u);
    const PrintDefectRecord &d = r.records[0];
    EXPECT_EQ(d.kind, PrintDefect::EnvelopeExceeded);
    EXPECT_DOUBLE_EQ(d.boxMm[0], -116.0);
    EXPECT_DOUBLE_EQ(d.boxMm[1], -260.0);
    EXPECT_DOUBLE_EQ(d.boxMm[2], 116.0);
    EXPECT_DOUBLE_EQ(d.boxMm[3], 111.5);
    EXPECT_DOUBLE_EQ(d.zLowMm, 0.0);
    EXPECT_DOUBLE_EQ(d.zHighMm, 20.0);
    EXPECT_EQ(d.slice, -1);
    EXPECT_FALSE(r.IsPrintable());
}

TEST(TEST_cgmesh_print_validation, envelope_is_read_in_millimetres)
{
    // 23,2 x 37,15 x 2 mm : logee. Sans conversion, 232 x 371,5 x 20 ne l'est pas.
    const PrintabilityReport r = CheckPrintEnvelope(pageModelBox(), withEnvelope(0.1, {218, 123, 250}));
    EXPECT_TRUE(r.evaluated);
    EXPECT_EQ(r.Count(PrintDefect::EnvelopeExceeded), 0u);
    EXPECT_TRUE(r.records.empty());
    EXPECT_TRUE(r.IsPrintable());

    // Boite enregistree en mm : 2,5 mm / u.
    const PrintabilityReport big = CheckPrintEnvelope(pageModelBox(), withEnvelope(2.5, {218, 123, 250}));
    ASSERT_EQ(big.records.size(), 1u);
    EXPECT_DOUBLE_EQ(big.records[0].boxMm[2], 290.0);
    EXPECT_DOUBLE_EQ(big.records[0].zHighMm, 50.0);
}

TEST(TEST_cgmesh_print_validation, envelope_fits_only_by_rotation)
{
    // 116 x 185,75 x 10 mm : y depasse 123 sur les axes du modele.
    const PrintabilityReport r = CheckPrintEnvelope(pageModelBox(), withEnvelope(0.5, {218, 123, 250}));
    EXPECT_EQ(r.Count(PrintDefect::EnvelopeExceeded), 0u);

    // Les six orientations de l'enveloppe logent la meme tige.
    PrintExtent rod;
    rod.hi = {1.0, 2.0, 30.0};
    const std::array<double, 3> orientations[] = {{30, 2, 1}, {2, 30, 1}, {1, 2, 30},
                                                  {2, 1, 30}, {30, 1, 2}, {1, 30, 2}};
    for (const std::array<double, 3> &env : orientations)
        EXPECT_EQ(CheckPrintEnvelope(rod, withEnvelope(1.0, env)).Count(PrintDefect::EnvelopeExceeded), 0u)
            << env[0] << " " << env[1] << " " << env[2];
    // Une cote trop courte sur un seul axe suffit a depasser.
    EXPECT_EQ(CheckPrintEnvelope(rod, withEnvelope(1.0, {30, 2, 0.999})).Count(PrintDefect::EnvelopeExceeded), 1u);
}

TEST(TEST_cgmesh_print_validation, envelope_equality_fits)
{
    PrintExtent e;
    e.lo = {10.0, 20.0, 30.0};
    e.hi = {110.0, 70.0, 40.0};   // 100 x 50 x 10
    EXPECT_EQ(CheckPrintEnvelope(e, withEnvelope(1.0, {10, 100, 50})).Count(PrintDefect::EnvelopeExceeded), 0u);
    EXPECT_EQ(CheckPrintEnvelope(e, withEnvelope(1.0, {10, 99.999, 50})).Count(PrintDefect::EnvelopeExceeded), 1u);
}

TEST(TEST_cgmesh_print_validation, envelope_without_envelope_or_scale_reports_nothing)
{
    PrintConstraints scaleOnly;
    scaleOnly.mmPerUnit = 1.0;
    const PrintabilityReport a = CheckPrintEnvelope(pageModelBox(), scaleOnly);
    EXPECT_TRUE(a.evaluated);
    EXPECT_DOUBLE_EQ(a.mmPerUnit, 1.0);
    EXPECT_EQ(a.Count(PrintDefect::EnvelopeExceeded), 0u);
    EXPECT_TRUE(a.records.empty());

    PrintConstraints envelopeOnly;
    envelopeOnly.envelopeMm = std::array<double, 3>{1, 1, 1};
    const PrintabilityReport b = CheckPrintEnvelope(pageModelBox(), envelopeOnly);
    EXPECT_FALSE(b.evaluated);
    for (std::size_t k = 0; k < kPrintDefectCount; ++k)
        EXPECT_EQ(b.counts[k], 0u) << PrintDefectName(static_cast<PrintDefect>(k));
    EXPECT_TRUE(b.records.empty());
}

TEST(TEST_cgmesh_print_validation, envelope_record_obeys_the_record_cap)
{
    PrintConstraints c = withEnvelope(1.0, {218, 123, 250});
    c.maxRecordsPerKind = 0;
    const PrintabilityReport r = CheckPrintEnvelope(pageModelBox(), c);
    EXPECT_EQ(r.Count(PrintDefect::EnvelopeExceeded), 1u);
    EXPECT_TRUE(r.records.empty());
}

TEST(TEST_cgmesh_print_validation, envelope_domain_is_checked)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    PrintExtent inverted = pageModelBox();
    std::swap(inverted.lo[2], inverted.hi[2]);
    EXPECT_THROW(CheckPrintEnvelope(inverted, withEnvelope(1.0, {1, 1, 1})), std::invalid_argument);
    PrintExtent notFinite = pageModelBox();
    notFinite.hi[1] = nan;
    EXPECT_THROW(CheckPrintEnvelope(notFinite, withEnvelope(1.0, {1, 1, 1})), std::invalid_argument);
    PrintExtent huge;
    huge.hi = {1e300, 1.0, 1.0};
    EXPECT_THROW(CheckPrintEnvelope(huge, withEnvelope(1e10, {1, 1, 1})), std::invalid_argument);
    EXPECT_THROW(CheckPrintEnvelope(pageModelBox(), withEnvelope(1.0, {1, 0, 1})), std::invalid_argument);
    PrintConstraints badWall = withEnvelope(1.0, {1, 1, 1});
    badWall.minWallMm = 0.0;
    EXPECT_THROW(CheckPrintEnvelope(pageModelBox(), badWall), std::invalid_argument);
    // Sans echelle, le domaine des contraintes est controle aussi.
    PrintConstraints unscaled;
    unscaled.minWallMm = 0.0;
    EXPECT_THROW(CheckPrintEnvelope(pageModelBox(), unscaled), std::invalid_argument);
}

// ===========================================================================
//  Comptes d'affichage (PrintDisplayCounts)
// ===========================================================================

TEST(TEST_cgmesh_print_validation, display_counts_take_the_maximum_per_slice)
{
    // Deux tranches identiques portant la meme barre de 0,3 mm : comptee dans
    // chacune (2), affichee une fois.
    const double s = 0.1;
    PrintConstraints c;
    c.mmPerUnit = s;
    std::vector<PrintSlice> stack(2);
    for (std::size_t i = 0; i < 2; ++i)
    {
        stack[i].zLow = 10.0 * static_cast<double>(i);
        stack[i].zHigh = stack[i].zLow + 10.0;
        stack[i].zCut = stack[i].zLow + 5.0;
        stack[i].region = toUnits({dumbbellMm(3.0, 2.0, 0.3)}, s);
    }
    const PrintabilityReport r = CheckPrintSlices(stack, c);
    ASSERT_EQ(r.Count(PrintDefect::ThinWall), 2u);
    const std::array<std::size_t, kPrintDefectCount> shown = PrintDisplayCounts(r);
    EXPECT_EQ(shown[static_cast<std::size_t>(PrintDefect::ThinWall)], 1u);

    // Un compte qu'aucune tranche ne porte s'ajoute tel quel.
    PrintabilityReport merged = r;
    ++merged.counts[static_cast<std::size_t>(PrintDefect::EnvelopeExceeded)];
    EXPECT_EQ(PrintDisplayCounts(merged)[static_cast<std::size_t>(PrintDefect::EnvelopeExceeded)], 1u);
    EXPECT_EQ(PrintDisplayCounts(merged)[static_cast<std::size_t>(PrintDefect::ThinWall)], 1u);

    // Hors pile : les comptes.
    const PrintabilityReport flat = check({dumbbellMm(3.0, 2.0, 0.3)}, s);
    ASSERT_EQ(flat.Count(PrintDefect::ThinWall), 1u);
    EXPECT_EQ(PrintDisplayCounts(flat), flat.counts);
}

// ===========================================================================
//  Largeur imprimee -> echelle (MmPerUnitForPrintedWidth)
// ===========================================================================

TEST(TEST_cgmesh_print_validation, printed_width_gives_the_scale)
{
    EXPECT_DOUBLE_EQ(MmPerUnitForPrintedWidth(50.0, 232.0), 50.0 / 232.0);
    EXPECT_DOUBLE_EQ(MmPerUnitForPrintedWidth(232.0, 232.0), 1.0);
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double w : {0.0, -1.0, nan, inf})
        EXPECT_THROW(MmPerUnitForPrintedWidth(w, 232.0), std::invalid_argument) << w;
    for (double u : {0.0, -1.0, nan, inf})
        EXPECT_THROW(MmPerUnitForPrintedWidth(50.0, u), std::invalid_argument) << u;
    EXPECT_THROW(MmPerUnitForPrintedWidth(1e308, 1e-308), std::invalid_argument);   // quotient infini
    EXPECT_THROW(MmPerUnitForPrintedWidth(1e-300, 1e300), std::invalid_argument);   // quotient nul
}
