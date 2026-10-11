#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../src/cgmesh/parameterized_gothic.h"
#include "../src/cgmesh/print_validation.h"
#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/mesh_slicing.h"
#include "../src/cgmesh/polygon2.h"
#include "../src/cgmesh/profile2d.h"

// ===========================================================================
//  Controle de fabrication d'une Gothic Window (CheckGothicPrint)
// ===========================================================================
//
// La fenetre par defaut a 200 unites de large : 0,1 mm / u la fait de 20 mm,
// 0,5 mm / u de 100 mm. Chaque propriete de l'adaptateur a un temoin positif et
// un negatif : verdict geometrique (refus -> rien n'est mesure), echelle
// transmise (nervures minces a 20 mm, aucune a 100 mm), epaisseur transmise
// (thin_slab a 3 u et 0,1 mm / u, pas a 0,2), barres (jamais declarees
// imprimables, meme quand la plaque plate l'est), ebrasement (tranches lues sur
// le maillage, couvert ou non selon la levre et l'emboitement).

namespace
{
    // Fenetre regeneree avec ses reglages, et son maillage.
    struct Window
    {
        ParameterizedGothicWindow shape;
        std::unique_ptr<Mesh>     mesh;

        explicit Window(const GothicWindowSettings &s = GothicWindowSettings())
        {
            shape.SetSettings(s);
            Regenerate();
        }
        void Regenerate()
        {
            shape.Regenerate();
            mesh.reset(shape.TakeMesh());
        }
        GothicPrintCheck Check(const PrintConstraints &c) const { return CheckGothicPrint(shape, *mesh, c); }
    };

    PrintConstraints at(double mmPerUnit)
    {
        PrintConstraints c;
        c.mmPerUnit = mmPerUnit;
        c.maxRecordsPerKind = 1000;
        return c;
    }

    GothicWindowSettings withProfile(int profile)
    {
        GothicWindowSettings s;
        s.profile = profile;
        return s;
    }

    // Boite de la rosace de la fenetre par defaut (largeur 200, emprise
    // [-116, 116] x [-260, 191,46]) : les six nervures mesurees a 20 mm
    // couvrent [-46,8, 46,8] x [33,6, 135,9] en unites.
    bool inRosetteBox(const PrintDefectRecord &d, double mmPerUnit)
    {
        const double x0 = -55 * mmPerUnit, x1 = 55 * mmPerUnit;
        const double y0 = 28 * mmPerUnit, y1 = 142 * mmPerUnit;
        return d.boxMm[0] >= x0 && d.boxMm[2] <= x1 && d.boxMm[1] >= y0 && d.boxMm[3] <= y1;
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

TEST(TEST_cgmesh_gothic_print, scope_names_are_stable)
{
    EXPECT_STREQ(GothicPrintScopeName(GothicPrintScope::NotRun), "not_run");
    EXPECT_STREQ(GothicPrintScopeName(GothicPrintScope::Complete), "complete");
    EXPECT_STREQ(GothicPrintScopeName(GothicPrintScope::ProfileNotChecked), "profile_not_checked");
}

// --- echelle transmise --------------------------------------------------------

TEST(TEST_cgmesh_gothic_print, default_window_at_20_mm_has_thin_rosette_ribs)
{
    const Window w;
    const auto t0 = std::chrono::steady_clock::now();
    const GothicPrintCheck r = w.Check(at(0.1));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    RecordProperty("check_ms_20mm", std::to_string(ms));

    EXPECT_TRUE(r.geometry.allowed);
    EXPECT_EQ(r.scope, GothicPrintScope::Complete);
    ASSERT_TRUE(r.print.evaluated);
    EXPECT_DOUBLE_EQ(r.print.mmPerUnit, 0.1);
    std::size_t ribs = 0;
    for (const PrintDefectRecord &d : r.print.records)
        if (d.kind == PrintDefect::ThinWall && inRosetteBox(d, 0.1))
            ++ribs;
    RecordProperty("thin_wall_rosette", static_cast<int>(ribs));
    EXPECT_GE(ribs, 6u);
    EXPECT_EQ(r.print.Count(PrintDefect::ThinSlab), 0u);   // 20 u -> 2 mm
    EXPECT_FALSE(r.IsPrintable());
}

TEST(TEST_cgmesh_gothic_print, default_window_at_100_mm_has_no_thin_wall_nor_closed_opening)
{
    const Window w;
    const GothicPrintCheck r = w.Check(at(0.5));
    ASSERT_TRUE(r.print.evaluated);
    EXPECT_EQ(r.scope, GothicPrintScope::Complete);
    EXPECT_EQ(r.print.Count(PrintDefect::ThinWall), 0u);
    EXPECT_EQ(r.print.Count(PrintDefect::ThinIsland), 0u);
    EXPECT_EQ(r.print.Count(PrintDefect::ClosedOpening), 0u);
    EXPECT_EQ(r.print.Count(PrintDefect::NarrowOpening), 0u);
    EXPECT_EQ(r.print.Count(PrintDefect::ThinSlab), 0u);
}

TEST(TEST_cgmesh_gothic_print, without_scale_nothing_is_measured)
{
    const Window w;
    const GothicPrintCheck r = w.Check(PrintConstraints());
    EXPECT_TRUE(r.geometry.allowed);
    EXPECT_FALSE(r.print.evaluated);
    EXPECT_EQ(r.print.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_FALSE(r.IsPrintable());
}

// --- epaisseur transmise --------------------------------------------------------

// Extrusion de 3 u : 0,3 mm a 0,1 mm / u, sous la paroi de 0,4 ; 0,6 mm a 0,2.
TEST(TEST_cgmesh_gothic_print, thin_extrusion_is_a_thin_slab)
{
    GothicWindowSettings s;
    s.zHeight = 3.f;
    const Window w(s);
    const GothicPrintCheck thin = w.Check(at(0.1));
    ASSERT_TRUE(thin.geometry.allowed) << thin.geometry.message;
    EXPECT_EQ(thin.print.Count(PrintDefect::ThinSlab), 1u);
    for (const PrintDefectRecord &d : thin.print.records)
        if (d.kind == PrintDefect::ThinSlab)
        {
            // Emprise de la baie, en mm : largeur 232 u x 0,1.
            EXPECT_NEAR(d.boxMm[2] - d.boxMm[0], 23.2, 0.1);
            EXPECT_DOUBLE_EQ(d.widthBelowMm, 0.4);
        }
    EXPECT_EQ(w.Check(at(0.2)).print.Count(PrintDefect::ThinSlab), 0u);
}

// Extrusion 0 : une feuille, plaque d'epaisseur nulle. Aucun relief n'est pose
// (Chamfer compris) : la portee reste complete.
TEST(TEST_cgmesh_gothic_print, zero_extrusion_is_a_thin_slab)
{
    for (int profile : {0, 1})
    {
        SCOPED_TRACE(profile);
        GothicWindowSettings s = withProfile(profile);
        s.zHeight = 0.f;
        const Window w(s);
        const GothicPrintCheck r = w.Check(at(0.5));
        ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
        EXPECT_EQ(r.scope, GothicPrintScope::Complete);
        EXPECT_EQ(r.print.Count(PrintDefect::ThinSlab), 1u);
    }
}

// --- verdict geometrique --------------------------------------------------------

// Geometrie refusee : rien n'est mesure, le motif est rendu. Le profil de barre
// non fini est refuse par Regenerate alors que BuildStonePolygon, qui ne le lit
// pas, reussirait ; le corps de 1e18 fait lever les deux.
TEST(TEST_cgmesh_gothic_print, refused_geometry_is_not_measured)
{
    Profile2D nanBar = rollBarProfile(4.0);
    nanBar.points[1].x = std::numeric_limits<double>::quiet_NaN();
    GothicWindowSettings huge;
    huge.bodyHeight = 1e18f;

    Window bad;
    bad.shape.SetBarProfile(&nanBar);
    bad.Regenerate();
    const Window tall(huge);
    const Window *refused[] = {&bad, &tall};
    for (const Window *w : refused)
    {
        GothicPrintCheck r;
        ASSERT_NO_THROW(r = w->Check(at(0.1)));
        EXPECT_FALSE(r.geometry.allowed);
        EXPECT_FALSE(r.geometry.message.empty());
        EXPECT_EQ(r.scope, GothicPrintScope::NotRun);
        EXPECT_FALSE(r.print.evaluated);
        EXPECT_EQ(defectTotal(r.print), 0u);
        EXPECT_TRUE(r.print.records.empty());
        EXPECT_FALSE(r.IsPrintable());
    }
}

// --- relief ---------------------------------------------------------------------

// Barres (positions Roll, Keel, Ogee du menu, port de barre, port de barre sur
// un ebrasement) : non couvertes. Pointes de lobes de 1,05 mm de partie mince a
// 100 mm : sous un seuil de pointe de 2 mm, la plaque plate passe (temoin). Les
// barres gardent les comptes de la plaque mais ne sont jamais declarees
// imprimables.
TEST(TEST_cgmesh_gothic_print, bar_relief_is_never_declared_printable)
{
    PrintConstraints c = at(0.5);
    c.maxTipLengthMm = 2.0;
    const Window flat;
    const GothicPrintCheck base = flat.Check(c);
    ASSERT_EQ(base.scope, GothicPrintScope::Complete);
    ASSERT_TRUE(base.IsPrintable()) << defectTotal(base.print);

    std::vector<std::unique_ptr<Window>> reliefs;
    for (int profile : {2, 3, 4})
        reliefs.push_back(std::make_unique<Window>(withProfile(profile)));
    const Profile2D bar = rollBarProfile(4.0);
    for (int profile : {0, 1})
    {
        reliefs.push_back(std::make_unique<Window>(withProfile(profile)));
        reliefs.back()->shape.SetBarProfile(&bar);
        reliefs.back()->Regenerate();
    }

    for (std::size_t k = 0; k < reliefs.size(); ++k)
    {
        SCOPED_TRACE(k);
        const Window &w = *reliefs[k];
        EXPECT_TRUE(w.shape.HasReliefProfile());
        EXPECT_TRUE(w.shape.HasBarRelief());
        const GothicPrintCheck r = w.Check(c);
        ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
        EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked);
        EXPECT_NE(r.reason.find("bar relief not checked"), std::string::npos) << r.reason;
        EXPECT_TRUE(r.print.evaluated);
        EXPECT_EQ(r.print.counts, base.print.counts);
        EXPECT_TRUE(r.print.slices.empty());
        EXPECT_TRUE(r.print.IsPrintable());
        EXPECT_FALSE(r.IsPrintable());
    }
    EXPECT_FALSE(flat.shape.HasReliefProfile());
    EXPECT_FALSE(flat.shape.HasBarRelief());
}

namespace
{
    std::vector<ExtrudeContour> stoneRegion(const ParameterizedGothicWindow &w)
    {
        const Polygon2 poly = w.BuildStonePolygon();
        std::vector<ExtrudeContour> out(static_cast<std::size_t>(poly.get_n_contours()));
        for (int c = 0; c < poly.get_n_contours(); ++c)
        {
            const float *p = poly.get_points(c);
            for (int i = 0; i < poly.get_n_points(c); ++i)
                out[static_cast<std::size_t>(c)].pts.push_back(Vector2f(p[2 * i], p[2 * i + 1]));
        }
        return offsetContours(out, 0.f, StrokeJoin::Round);   // union NonZero
    }

    double areaOf(const std::vector<ExtrudeContour> &r)
    {
        double a = 0.0;
        for (const ExtrudeContour &c : r)
            a += contourSignedArea(c.pts);
        return a;
    }

    double perimeterOf(const std::vector<ExtrudeContour> &r)
    {
        double l = 0.0;
        for (const ExtrudeContour &c : r)
            for (std::size_t i = 0; i < c.pts.size(); ++i)
            {
                const Vector2f &a = c.pts[i], &b = c.pts[(i + 1) % c.pts.size()];
                l += std::hypot(static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y);
            }
        return l;
    }

    std::vector<ExtrudeContour> meshSection(const Mesh &mesh, float z)
    {
        cgmesh::slicing::SliceInput in;
        in.mesh = &mesh;
        cgmesh::slicing::SliceOptions o;
        o.nesting = cgmesh::slicing::Nesting::Winding;
        const std::vector<cgmesh::slicing::SliceLayer> layers = cgmesh::slicing::sliceMeshes({in}, {z}, o);
        std::vector<ExtrudeContour> out;
        for (const cgmesh::slicing::SliceRegion &region : layers.at(0))
            out.insert(out.end(), region.contours.begin(), region.contours.end());
        return offsetContours(out, 0.f, StrokeJoin::Round);
    }

    // Ecart moyen entre deux regions : aire de la difference symetrique par
    // unite de longueur du bord de `ref`.
    double meanGap(const std::vector<ExtrudeContour> &a, const std::vector<ExtrudeContour> &ref)
    {
        return (areaOf(differenceContours(a, ref)) + areaOf(differenceContours(ref, a))) / perimeterOf(ref);
    }

    std::size_t sliceCount(const PrintSliceSummary &s, PrintDefect kind)
    {
        return s.counts[static_cast<std::size_t>(kind)];
    }
}

// Temoin de l'instrument : la coupe du maillage (sliceMeshes) redonne la region
// de pierre. Plaque plate a mi-epaisseur ; ebrasement juste sous la face avant.
// Temoin negatif : la section arriere de l'ebrasement s'en ecarte de plusieurs
// unites en moyenne (largeur de chanfrein 6 u sur les contours chanfreines).
TEST(TEST_cgmesh_gothic_print, mesh_section_of_the_plate_is_the_stone_region)
{
    const Window flat;
    const Window chamfer(withProfile(1));
    const std::vector<ExtrudeContour> region = stoneRegion(flat.shape);
    EXPECT_LT(meanGap(meshSection(*flat.mesh, 10.f), region), 1e-4);
    EXPECT_LT(meanGap(meshSection(*chamfer.mesh, 19.999f), region), 1e-3);
    EXPECT_GT(meanGap(meshSection(*chamfer.mesh, 5.f), region), 0.5);
}

// Ebrasement de la position Chamfer (largeur 6 u, profondeur 10 u, Extrusion
// 20) : le maillage a trois niveaux, 0, 10 et 20. La couche [0, 10] est
// prismatique, une tranche ; la couche [10, 20] est inclinee, son bas est la
// section arriere, son haut la face avant. La face avant (z = 20) est la region
// entiere, la section arriere est strictement plus mince. Portee complete.
TEST(TEST_cgmesh_gothic_print, chamfer_slices_are_read_on_the_mesh)
{
    const double s = 0.1;
    PrintConstraints c = at(s);
    c.maxTipLengthMm = 2.0;   // coins de 90 deg du pied du cadre admis (majorant 0,34 mm)
    const Window flat;
    const Window w(withProfile(1));
    EXPECT_TRUE(w.shape.HasReliefProfile());
    EXPECT_FALSE(w.shape.HasBarRelief());
    const GothicPrintCheck r = w.Check(c);
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::Complete) << r.reason;
    ASSERT_TRUE(r.print.evaluated);
    ASSERT_EQ(r.print.slices.size(), 3u);
    const PrintSliceSummary &back = r.print.slices[0], &front = r.print.slices[1], &sil = r.print.slices[2];
    EXPECT_NEAR(back.zLowMm, 0.0, 1e-6);
    EXPECT_NEAR(back.zHighMm, 10 * s, 1e-6);
    EXPECT_NEAR(back.zCutMm, 5 * s, 1e-6);
    // Haut seul de la couche inclinee : [coupe, face avant].
    EXPECT_NEAR(front.zHighMm, 20 * s, 1e-6);
    EXPECT_LT(front.zCutMm, 20 * s);
    EXPECT_GT(front.zCutMm, 19.99 * s);
    EXPECT_DOUBLE_EQ(front.zLowMm, front.zCutMm);
    EXPECT_TRUE(sil.silhouette);
    EXPECT_NEAR(sil.zLowMm, 0.0, 1e-6);
    EXPECT_NEAR(sil.zHighMm, 20 * s, 1e-6);

    // Face avant = region de pierre ; section arriere strictement plus mince.
    const double region = areaOf(stoneRegion(w.shape)) * s * s;
    EXPECT_NEAR(front.areaMm2 / region, 1.0, 1e-3);
    EXPECT_NEAR(sil.areaMm2 / front.areaMm2, 1.0, 1e-4);
    EXPECT_LT(back.areaMm2, 0.95 * front.areaMm2);

    // Parois : la section arriere en a au moins autant que la plaque plate, la
    // face avant autant. Vides sur la silhouette : ceux de la plaque plate.
    const GothicPrintCheck plate = flat.Check(c);
    const std::size_t flatWalls = plate.print.Count(PrintDefect::ThinWall);
    ASSERT_GT(flatWalls, 0u);
    EXPECT_GE(sliceCount(back, PrintDefect::ThinWall), flatWalls);
    EXPECT_EQ(sliceCount(front, PrintDefect::ThinWall), flatWalls);
    EXPECT_EQ(sliceCount(sil, PrintDefect::ClosedOpening), plate.print.Count(PrintDefect::ClosedOpening));
    EXPECT_EQ(sliceCount(sil, PrintDefect::NarrowOpening), plate.print.Count(PrintDefect::NarrowOpening));
    EXPECT_FALSE(r.IsPrintable());
}

// Cotes en mm a deux echelles : la couche arriere monte a 10 u. Chaque
// enregistrement porte la couche de sa tranche.
TEST(TEST_cgmesh_gothic_print, chamfer_slice_heights_are_in_millimetres)
{
    const Window w(withProfile(1));
    for (double s : {0.1, 0.5})
    {
        SCOPED_TRACE(s);
        const GothicPrintCheck r = w.Check(at(s));
        ASSERT_EQ(r.print.slices.size(), 3u);
        EXPECT_NEAR(r.print.slices[0].zHighMm, 10 * s, 1e-6);
        EXPECT_NEAR(r.print.slices[2].zHighMm, 20 * s, 1e-6);
        std::size_t stamped = 0;
        for (const PrintDefectRecord &d : r.print.records)
        {
            ASSERT_GE(d.slice, 0);
            ASSERT_LT(static_cast<std::size_t>(d.slice), r.print.slices.size());
            EXPECT_DOUBLE_EQ(d.zLowMm, r.print.slices[static_cast<std::size_t>(d.slice)].zLowMm);
            EXPECT_DOUBLE_EQ(d.zHighMm, r.print.slices[static_cast<std::size_t>(d.slice)].zHighMm);
            ++stamped;
        }
        EXPECT_GT(stamped, 0u);
    }
}

// Port d'ebrasement : chanfrein de 2 x 3 u, niveaux 0, 17 et 20.
TEST(TEST_cgmesh_gothic_print, splay_port_slices_follow_its_levels)
{
    const Profile2D splay = chamferSplayProfile(2.0, 3.0);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    EXPECT_TRUE(w.shape.HasReliefProfile());
    EXPECT_FALSE(w.shape.HasBarRelief());
    PrintConstraints c = at(0.5);
    c.maxTipLengthMm = 2.0;   // coins de 90 deg du pied du cadre admis
    const GothicPrintCheck r = w.Check(c);
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::Complete) << r.reason;
    ASSERT_EQ(r.print.slices.size(), 3u);
    EXPECT_NEAR(r.print.slices[0].zHighMm, 17 * 0.5, 1e-6);
    EXPECT_LT(r.print.slices[0].areaMm2, r.print.slices[1].areaMm2);
}

// --- concordance : le verdict porte sur CE maillage et CES reglages ---------

namespace
{
    PrintConstraints passing()
    {
        PrintConstraints c = at(0.5);
        c.maxTipLengthMm = 2.0;   // pointes des lobes (1,05 mm) admises : la plaque plate passe
        return c;
    }

    void setWidth200(ParameterizedGothicWindow &w)
    {
        GothicWindowSettings s = w.Settings();
        s.width = 200.f;
        w.SetSettings(s);
    }
    void setExtrusion20(ParameterizedGothicWindow &w)
    {
        GothicWindowSettings s = w.Settings();
        s.zHeight = 20.f;
        w.SetSettings(s);
    }
    void setFlat(ParameterizedGothicWindow &w)
    {
        GothicWindowSettings s = w.Settings();
        s.profile = 0;
        w.SetSettings(s);
    }
    void changeThresholds(ParameterizedGothicWindow &w)
    {
        GothicFeatureThresholds t;
        t.minRosetteRadius = 2.0;
        w.SetFeatureThresholds(t);
    }
    void removeBarPort(ParameterizedGothicWindow &w)
    {
        w.SetBarProfile(nullptr);
    }
}

// Reglages, seuils ou port changes apres Regenerate : le maillage n'est plus
// celui des reglages, rien n'est mesure. Temoin : apres un nouveau Regenerate,
// la mesure a lieu.
TEST(TEST_cgmesh_gothic_print, settings_changed_since_regenerate_are_not_measured)
{
    struct Case
    {
        const char *name;
        GothicWindowSettings before;
        bool barPort;
        void (*change)(ParameterizedGothicWindow &);
    };
    GothicWindowSettings narrow;
    narrow.width = 40.f;
    GothicWindowSettings sheet;
    sheet.zHeight = 0.f;
    const std::vector<Case> cases = {
        {"largeur 40 -> 200", narrow, false, setWidth200},
        {"feuille -> Extrusion 20", sheet, false, setExtrusion20},
        {"barres -> Flat", withProfile(2), false, setFlat},
        {"seuils", GothicWindowSettings(), false, changeThresholds},
        {"port de barre retire", GothicWindowSettings(), true, removeBarPort},
    };
    const Profile2D bar = rollBarProfile(4.0);
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        Window w(c.before);
        if (c.barPort)
        {
            w.shape.SetBarProfile(&bar);
            w.Regenerate();
        }
        ASSERT_TRUE(w.shape.MatchesLastRegenerate());
        c.change(w.shape);
        EXPECT_FALSE(w.shape.MatchesLastRegenerate());
        const GothicPrintCheck r = w.Check(passing());
        EXPECT_EQ(r.scope, GothicPrintScope::NotRun);
        EXPECT_NE(r.reason.find("settings changed since last Regenerate"), std::string::npos) << r.reason;
        EXPECT_FALSE(r.print.evaluated);
        EXPECT_FALSE(r.IsPrintable());

        w.Regenerate();
        EXPECT_TRUE(w.shape.MatchesLastRegenerate());
        const GothicPrintCheck fresh = w.Check(passing());
        EXPECT_NE(fresh.scope, GothicPrintScope::NotRun) << fresh.reason;
        EXPECT_TRUE(fresh.reason.empty()) << fresh.reason;
    }
    // Reposer les memes valeurs n'est pas un changement.
    Window same;
    same.shape.SetSettings(GothicWindowSettings());
    same.shape.SetFeatureThresholds(GothicFeatureThresholds());
    EXPECT_TRUE(same.shape.MatchesLastRegenerate());
    EXPECT_TRUE(same.Check(passing()).IsPrintable());
}

// Maillage d'une autre baie : emprise differente de la region courante.
TEST(TEST_cgmesh_gothic_print, foreign_mesh_is_not_measured)
{
    GothicWindowSettings narrow;
    narrow.width = 40.f;
    GothicWindowSettings thin;
    thin.zHeight = 10.f;
    // 2 % plus large : dans la marge admise pour un relief, pas pour une plaque.
    GothicWindowSettings wider;
    wider.width = 204.f;
    const Window own;
    const Window other(narrow);
    const Window shallow(thin);
    const Window close(wider);
    const Window *foreign[] = {&other, &shallow, &close};
    for (const Window *f : foreign)
    {
        const GothicPrintCheck r = CheckGothicPrint(own.shape, *f->mesh, passing());
        EXPECT_EQ(r.scope, GothicPrintScope::NotRun);
        EXPECT_NE(r.reason.find("mesh extent"), std::string::npos) << r.reason;
        EXPECT_FALSE(r.print.evaluated);
        EXPECT_FALSE(r.IsPrintable());
    }
    EXPECT_TRUE(own.Check(passing()).IsPrintable());
}

// Ebrasement couvert : a 100 mm, pointes des lobes admises (2 mm), la fenetre
// en Chamfer est declaree imprimable ; a 50 mm, la fente entre lancettes
// (jugee sur la silhouette) l'interdit.
TEST(TEST_cgmesh_gothic_print, chamfer_window_can_be_declared_printable)
{
    const Window w(withProfile(1));
    const GothicPrintCheck big = w.Check(passing());
    EXPECT_EQ(big.scope, GothicPrintScope::Complete) << big.reason;
    EXPECT_TRUE(big.reason.empty()) << big.reason;
    EXPECT_TRUE(big.IsPrintable()) << defectTotal(big.print);

    PrintConstraints c = at(0.25);
    c.maxTipLengthMm = 2.0;
    const GothicPrintCheck small = w.Check(c);
    EXPECT_EQ(small.scope, GothicPrintScope::Complete);
    EXPECT_EQ(small.print.Count(PrintDefect::NarrowOpening), 1u);
    ASSERT_FALSE(small.print.slices.empty());
    EXPECT_EQ(sliceCount(small.print.slices.back(), PrintDefect::NarrowOpening), 1u);
    EXPECT_FALSE(small.IsPrintable());
}

// Levre de la face avant : diedre a entre la face avant et la pente. Chanfrein
// de 10 x 2 u ; la reduction de largeur de l'extrusion (profile_reduced) ne
// laisse que 5 u au plus : a = 21,8 deg lu sur le maillage, partie mince
// 0,2 x (1 / sin 10,9 deg - 1) = 0,86 mm. Fragile sous maxTipLengthMm 0,3 :
// non couvert, la pile reste mesuree ; admise sous 2 mm : couvert.
TEST(TEST_cgmesh_gothic_print, shallow_splay_lip_is_not_covered)
{
    const Profile2D splay = chamferSplayProfile(10.0, 2.0);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    const GothicPrintCheck r = w.Check(at(0.5));
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(r.reason.find("front lip"), std::string::npos) << r.reason;
    EXPECT_FALSE(r.print.slices.empty());
    EXPECT_FALSE(r.IsPrintable());

    const GothicPrintCheck tolerant = w.Check(passing());
    EXPECT_EQ(tolerant.scope, GothicPrintScope::Complete) << tolerant.reason;
}

// Levre de 35 deg (chanfrein 4 x 2,8 u : atan 0,7 = 34,99 deg) : partie mince
// 0,2 x (1 / sin 17,5 deg - 1) = 0,47 mm, au-dela de 0,3 : non couvert. La loi
// du coin porte sur le demi-angle du diedre.
TEST(TEST_cgmesh_gothic_print, lip_of_35_degrees_is_not_covered)
{
    const Profile2D splay = chamferSplayProfile(4.0, 2.8);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    const GothicPrintCheck r = w.Check(at(0.5));
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(r.reason.find("front lip of 34.99"), std::string::npos) << r.reason;
}

// Coin de la face avant : un sommet convexe dont les deux cotes descendent en
// pente est un triedre (face avant et deux pentes), plus mince qu'une levre.
// Chanfrein de 6 x 2 u : a = 18,4 deg ; au coin de 90 deg du pied du cadre, la
// boule de 0,4 mm reste a 0,2 x (sqrt ((cot 9,2 deg / sin 45 deg)^2 + 1) - 1)
// = 1,56 mm du sommet (majorant ; 1,08 mm sur le maillage), au-dela d'un seuil de
// pointe de 1,06 mm, quand la levre seule (1,05 mm) passerait.
TEST(TEST_cgmesh_gothic_print, chamfered_front_corner_is_not_covered)
{
    const Profile2D splay = chamferSplayProfile(6.0, 2.0);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    PrintConstraints c = at(0.5);
    c.maxTipLengthMm = 1.06;
    const GothicPrintCheck r = w.Check(c);
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(r.reason.find("front corner of"), std::string::npos) << r.reason;
    EXPECT_FALSE(r.IsPrintable());
    c.maxTipLengthMm = 2.0;
    EXPECT_EQ(w.Check(c).scope, GothicPrintScope::Complete);
}

// Un coin RENTRANT de la face avant (apex d'une lancette a tete simple, exces
// 2,5 : la pierre entoure une ouverture de 62 deg) n'est pas un triedre
// fragile : le coin convexe le plus aigu reste celui de 90 deg du pied du cadre.
TEST(TEST_cgmesh_gothic_print, concave_front_corners_are_not_cones)
{
    GothicWindowSettings s = withProfile(1);
    s.lancetHead = 0;
    s.subExcess = 2.5f;
    const Window w(s);
    PrintConstraints c = at(0.5);
    c.maxTipLengthMm = 0.002;   // le motif rend toujours le coin le plus aigu
    const GothicPrintCheck r = w.Check(c);
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_NE(r.reason.find("front corner of 90 deg"), std::string::npos) << r.reason;
}

// Couche avant de 0,003 u : le retrait de 1e-4 de son epaisseur se perd dans
// l'arrondi float de 20 ; la coupe reste strictement dans la couche.
TEST(TEST_cgmesh_gothic_print, front_cut_stays_inside_a_thin_layer)
{
    const Profile2D splay = chamferSplayProfile(2.0, 0.003);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    const GothicPrintCheck r = w.Check(passing());
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    ASSERT_EQ(r.print.slices.size(), 3u);
    const PrintSliceSummary &front = r.print.slices[1];
    EXPECT_DOUBLE_EQ(front.zHighMm, 20 * 0.5);
    EXPECT_LT(front.zCutMm, 20 * 0.5);
    EXPECT_GT(front.zCutMm, 19.997 * 0.5);
}

// Cavet de 6 x 10 u en 6 segments : tangent a la face avant, sa premiere
// facette fait une levre de 12,4 deg, mince sur 1,66 mm ; aux coins de 90 deg du
// pied du cadre, le majorant du triedre vaut 2,41 mm. Non couvert sous 0,3 ni
// sous 2 mm.
TEST(TEST_cgmesh_gothic_print, cavetto_splay_is_not_covered)
{
    const Profile2D splay = cavettoSplayProfile(6.0, 10.0);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    const GothicPrintCheck strict = w.Check(at(0.5));
    ASSERT_TRUE(strict.geometry.allowed) << strict.geometry.message;
    EXPECT_EQ(strict.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(strict.reason.find("front lip"), std::string::npos) << strict.reason;
    EXPECT_FALSE(strict.IsPrintable());
    const GothicPrintCheck tolerant = w.Check(passing());
    EXPECT_EQ(tolerant.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(tolerant.reason.find("front corner of"), std::string::npos) << tolerant.reason;
}

// Ebrasement dont toutes les parois retombent droites (profil plus large que
// quatre rayons caracteristiques de chaque contour) : aucune face inclinee,
// donc aucune levre, meme sous un seuil de pointe de 0,05 mm.
TEST(TEST_cgmesh_gothic_print, splay_with_vertical_walls_only_has_no_lip)
{
    const Profile2D splay = chamferSplayProfile(100.0, 3.0);
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    PrintConstraints c = at(0.5);
    c.maxTipLengthMm = 0.05;
    const GothicPrintCheck r = w.Check(c);
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::Complete) << r.reason;
    EXPECT_TRUE(r.reason.empty()) << r.reason;
}

// Profil qui rentre puis ressort : des faces inclinees regardent vers l'avant,
// les sections ne sont plus emboitees (une paroi peut pencher, sa largeur
// coupee serait optimiste).
TEST(TEST_cgmesh_gothic_print, forward_facing_splay_is_not_covered)
{
    Profile2D splay;
    splay.points = {Vector2d(0.0, 0.0), Vector2d(3.0, 4.0), Vector2d(6.0, 2.0)};
    Window w;
    w.shape.SetSplayProfile(&splay);
    w.Regenerate();
    const GothicPrintCheck r = w.Check(passing());
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_NE(r.reason.find("not nested"), std::string::npos) << r.reason;
    EXPECT_FALSE(r.IsPrintable());
}

// Les reliefs ont un maillage concordant (barres au-dessus de la face avant,
// ebrasement dans l'epaisseur) : mesures, portee complete pour l'ebrasement,
// profile_not_checked pour les barres.
TEST(TEST_cgmesh_gothic_print, relief_meshes_pass_the_extent_check)
{
    for (int profile : {1, 2, 3, 4})
    {
        SCOPED_TRACE(profile);
        const Window w(withProfile(profile));
        const GothicPrintCheck r = w.Check(passing());
        EXPECT_EQ(r.scope, profile == 1 ? GothicPrintScope::Complete : GothicPrintScope::ProfileNotChecked) << r.reason;
    }
}

// Verdict combine : une geometrie permise mais non stricte n'est jamais
// imprimable.
TEST(TEST_cgmesh_gothic_print, non_strict_geometry_is_never_printable)
{
    GothicPrintCheck c;
    c.geometry.allowed = true;
    c.geometry.strict = true;
    c.scope = GothicPrintScope::Complete;
    c.print.evaluated = true;
    c.print.mmPerUnit = 0.5;
    ASSERT_TRUE(c.IsPrintable());
    c.geometry.strict = false;
    EXPECT_FALSE(c.IsPrintable());

    // Feuille : export permis, non stricte.
    GothicWindowSettings s;
    s.zHeight = 0.f;
    const Window sheet(s);
    const GothicPrintCheck r = sheet.Check(passing());
    EXPECT_TRUE(r.geometry.allowed);
    EXPECT_FALSE(r.geometry.strict);
    EXPECT_FALSE(r.IsPrintable());
}

// Extrusion negative : plancher 0, une feuille.
TEST(TEST_cgmesh_gothic_print, negative_extrusion_is_floored_to_a_sheet)
{
    GothicWindowSettings s;
    s.zHeight = -5.f;
    const Window w(s);
    GothicPrintCheck r;
    ASSERT_NO_THROW(r = w.Check(at(0.5)));
    ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
    EXPECT_EQ(r.scope, GothicPrintScope::Complete) << r.reason;
    EXPECT_EQ(r.print.Count(PrintDefect::ThinSlab), 1u);
}

// --- debord du relief : absolu, pas proportionnel a la largeur --------------

// Le debord des barres vaut le rayon de barre, en unites monde : au defaut
// (largeur 200, Roll), rb = min(0,5 x 10, 0,45 x 10 + 1,5) = 5. Sans barre, ou
// sans extrusion, 0 ; un ebrasement n'ote que de la pierre.
TEST(TEST_cgmesh_gothic_print, relief_overhang_is_the_placed_bar_extent)
{
    EXPECT_NEAR(Window(withProfile(2)).shape.ReliefOverhang(), 5.0, 1e-3);
    // Largeur 100 : rb canonique 10, soit 5 unites monde (le maillage descend a
    // y = -265 comme a 200).
    GothicWindowSettings half = withProfile(2);
    half.width = 100.f;
    EXPECT_NEAR(Window(half).shape.ReliefOverhang(), 5.0, 1e-3);
    EXPECT_DOUBLE_EQ(Window().shape.ReliefOverhang(), 0.0);
    EXPECT_DOUBLE_EQ(Window(withProfile(1)).shape.ReliefOverhang(), 0.0);
    GothicWindowSettings sheet = withProfile(2);
    sheet.zHeight = 0.f;
    EXPECT_DOUBLE_EQ(Window(sheet).shape.ReliefOverhang(), 0.0);
    const Profile2D bar = rollBarProfile(12.0);
    Window port;
    port.shape.SetBarProfile(&bar);
    EXPECT_NEAR(port.shape.ReliefOverhang(), 12.0, 1e-3);
}

// Maillages a relief concordants, a toute largeur et pour des barres plus
// grosses que 0,05 x largeur : mesures, jamais not_run. L'ebrasement est
// couvert ou non selon la largeur (l'Extrusion de 20 u, plus epaisse en repere
// canonique a petite largeur, raidit la pente) ; non couvert, le motif nomme le
// coin et la pile reste mesuree.
TEST(TEST_cgmesh_gothic_print, consistent_relief_meshes_are_measured_at_any_size)
{
    for (float width : {20.f, 50.f, 100.f, 200.f})
        for (int profile : {1, 2, 3, 4})
        {
            SCOPED_TRACE(width);
            SCOPED_TRACE(profile);
            GothicWindowSettings s = withProfile(profile);
            s.width = width;
            const Window w(s);
            const GothicPrintCheck r = w.Check(at(0.5));
            ASSERT_TRUE(r.geometry.allowed) << r.geometry.message;
            if (profile == 1)
            {
                EXPECT_NE(r.scope, GothicPrintScope::NotRun) << r.reason;
                if (r.scope == GothicPrintScope::ProfileNotChecked)
                {
                    EXPECT_NE(r.reason.find("front corner of"), std::string::npos) << r.reason;
                }
                EXPECT_FALSE(r.print.slices.empty());
            }
            else
            {
                EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked) << r.reason;
            }
            EXPECT_TRUE(r.print.evaluated);
        }
    for (int profile : {2, 3, 4})
    {
        SCOPED_TRACE(profile);
        GothicWindowSettings s = withProfile(profile);
        s.offsetInner = 40.f;
        s.offsetOuter = 46.f;
        const Window w(s);
        const GothicPrintCheck r = w.Check(at(0.5));
        EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked) << r.reason;
    }
    for (double radius : {12.0, 20.0})
    {
        SCOPED_TRACE(radius);
        const Profile2D bar = rollBarProfile(radius);
        Window w;
        w.shape.SetBarProfile(&bar);
        w.Regenerate();
        const GothicPrintCheck r = w.Check(at(0.5));
        EXPECT_EQ(r.scope, GothicPrintScope::ProfileNotChecked) << r.reason;
    }
}

// Negatif : le maillage a barres d'une baie de 240 n'est pas celui d'une baie
// de 200 a barres, malgre la marge du relief.
TEST(TEST_cgmesh_gothic_print, foreign_relief_mesh_is_not_measured)
{
    GothicWindowSettings big = withProfile(2);
    big.width = 240.f;
    const Window own(withProfile(2));
    const Window other(big);
    const GothicPrintCheck r = CheckGothicPrint(own.shape, *other.mesh, at(0.5));
    EXPECT_EQ(r.scope, GothicPrintScope::NotRun);
    EXPECT_NE(r.reason.find("mesh extent"), std::string::npos) << r.reason;
    EXPECT_EQ(own.Check(at(0.5)).scope, GothicPrintScope::ProfileNotChecked);
}

// Un reglage non fini est refuse par Regenerate, qu'il soit lu par la baie ou
// non : sinon il rendrait MatchesLastRegenerate faux pour toujours (NaN differe
// de lui-meme) sur une baie construite.
TEST(TEST_cgmesh_gothic_print, non_finite_unread_setting_is_refused)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    struct Case { const char *name; float GothicWindowSettings::*field; };
    const Case cases[] = {
        {"subFoilPointed", &GothicWindowSettings::subFoilPointed},
        {"trefoilSplit", &GothicWindowSettings::trefoilSplit},
        {"trefoilFoilRadius", &GothicWindowSettings::trefoilFoilRadius},
        {"mouchetteRadius", &GothicWindowSettings::mouchetteRadius},
    };
    for (const Case &c : cases)
    {
        SCOPED_TRACE(c.name);
        GothicWindowSettings s;
        s.*c.field = nan;
        const Window w(s);
        const GothicBuildReport &report = w.shape.LastBuildReport();
        ASSERT_TRUE(report.HasErrors());
        bool named = false;
        for (const GothicDiagnostic &d : report.entries)
            named = named || (d.error && d.code == static_cast<int>(GothicBuildError::InvalidParameter)
                              && d.parameter == c.name);
        EXPECT_TRUE(named);
        const GothicPrintCheck r = w.Check(at(0.5));
        EXPECT_EQ(r.scope, GothicPrintScope::NotRun);
        EXPECT_EQ(r.reason, "geometry refused");
    }
}

// ===========================================================================
//  Garde d'export : CheckGothicExportAndPrint
// ===========================================================================
//
// Un seul verdict pour l'export : la geometrie (calculee une fois), puis le
// controle d'impression s'il a une echelle. Sans echelle, rien n'est mesure ;
// une erreur de domaine est capturee ; l'enveloppe est jugee sur le maillage
// exporte, avant la mesure 2D.

namespace
{
    std::optional<PrintConstraints> noConstraints() { return std::nullopt; }

    void expectSameReport(const PrintabilityReport &a, const PrintabilityReport &b)
    {
        EXPECT_EQ(a.counts, b.counts);
        EXPECT_EQ(a.evaluated, b.evaluated);
        EXPECT_EQ(a.mmPerUnit, b.mmPerUnit);
        EXPECT_EQ(a.wallRadius, b.wallRadius);
        EXPECT_EQ(a.holeRadius, b.holeRadius);
        EXPECT_EQ(a.depthCapMm, b.depthCapMm);
        EXPECT_EQ(a.solidPieces, b.solidPieces);
        EXPECT_EQ(a.voidPieces, b.voidPieces);
        EXPECT_EQ(a.noisePieces, b.noisePieces);
        EXPECT_EQ(a.bluntTips, b.bluntTips);
        ASSERT_EQ(a.records.size(), b.records.size());
        for (std::size_t i = 0; i < a.records.size(); ++i)
        {
            const PrintDefectRecord &p = a.records[i], &q = b.records[i];
            EXPECT_EQ(p.kind, q.kind) << i;
            EXPECT_EQ(p.boxMm, q.boxMm) << i;
            EXPECT_EQ(p.depthMm, q.depthMm) << i;
            EXPECT_EQ(p.areaMm2, q.areaMm2) << i;
            EXPECT_EQ(p.contacts, q.contacts) << i;
            EXPECT_EQ(p.widthBelowMm, q.widthBelowMm) << i;
            EXPECT_EQ(p.slice, q.slice) << i;
            EXPECT_EQ(p.zLowMm, q.zLowMm) << i;
            EXPECT_EQ(p.zHighMm, q.zHighMm) << i;
        }
        ASSERT_EQ(a.slices.size(), b.slices.size());
        for (std::size_t i = 0; i < a.slices.size(); ++i)
        {
            const PrintSliceSummary &p = a.slices[i], &q = b.slices[i];
            EXPECT_EQ(p.zLowMm, q.zLowMm) << i;
            EXPECT_EQ(p.zHighMm, q.zHighMm) << i;
            EXPECT_EQ(p.zCutMm, q.zCutMm) << i;
            EXPECT_EQ(p.areaMm2, q.areaMm2) << i;
            EXPECT_EQ(p.silhouette, q.silhouette) << i;
            EXPECT_EQ(p.counts, q.counts) << i;
        }
    }

    void expectSameCheck(const GothicPrintCheck &a, const GothicPrintCheck &b)
    {
        EXPECT_EQ(a.geometry.allowed, b.geometry.allowed);
        EXPECT_EQ(a.geometry.strict, b.geometry.strict);
        EXPECT_EQ(a.geometry.message, b.geometry.message);
        EXPECT_EQ(a.scope, b.scope);
        EXPECT_EQ(a.reason, b.reason);
        expectSameReport(a.print, b.print);
    }

    // Modele par defaut de la page gothique du maker (v1, Chamfer, 232 x 371,46 x 20 u).
    std::string readText(const char *path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    }
}

TEST(TEST_cgmesh_gothic_print, export_and_print_without_scale_runs_no_print_check)
{
    const Window w;
    PrintConstraints unscaled;
    unscaled.envelopeMm = std::array<double, 3>{1, 1, 1};
    PrintConstraints uncapped = unscaled;
    uncapped.maxRecordsPerKind = 0;
    struct Case { std::optional<PrintConstraints> c; std::size_t records; };
    const Case cases[] = {{noConstraints(), 1u}, {unscaled, 1u}, {uncapped, 0u}};
    for (const Case &k : cases)
    {
        const GothicPrintVerdict v = CheckGothicExportAndPrint(w.shape, *w.mesh, k.c);
        EXPECT_TRUE(v.ExportAllowed());
        EXPECT_TRUE(v.check.geometry.strict);
        EXPECT_EQ(v.check.scope, GothicPrintScope::NotRun);
        EXPECT_EQ(v.check.reason, "print scale not set");
        EXPECT_FALSE(v.check.print.evaluated);
        EXPECT_EQ(v.check.print.Count(PrintDefect::ScaleUnset), 1u);
        EXPECT_EQ(v.check.print.Count(PrintDefect::EnvelopeExceeded), 0u);
        EXPECT_EQ(v.check.print.records.size(), k.records);
        EXPECT_EQ(v.check.print.solidPieces + v.check.print.voidPieces, 0u);
        EXPECT_TRUE(v.printError.empty()) << v.printError;
        EXPECT_FALSE(v.printedSizeMm.has_value());
        EXPECT_EQ(v.constraints.has_value(), k.c.has_value());
        EXPECT_FALSE(v.IsPrintable());
    }
}

TEST(TEST_cgmesh_gothic_print, export_and_print_never_throws_on_bad_constraints)
{
    const Window w;
    PrintConstraints bad;
    bad.minWallMm = 0.0;
    GothicPrintVerdict unscaled, scaled;
    ASSERT_NO_THROW(unscaled = CheckGothicExportAndPrint(w.shape, *w.mesh, bad));
    EXPECT_TRUE(unscaled.ExportAllowed());
    EXPECT_NE(unscaled.printError.find("minWallMm"), std::string::npos) << unscaled.printError;
    bad.mmPerUnit = 0.1;
    ASSERT_NO_THROW(scaled = CheckGothicExportAndPrint(w.shape, *w.mesh, bad));
    EXPECT_TRUE(scaled.ExportAllowed());
    EXPECT_NE(scaled.printError.find("minWallMm"), std::string::npos) << scaled.printError;
    EXPECT_EQ(scaled.check.reason, "print check error");
    EXPECT_FALSE(scaled.IsPrintable());
}

TEST(TEST_cgmesh_gothic_print, export_and_print_with_scale_equals_check_gothic_print)
{
    for (int recursion : {0, 1})
        for (int profile : {0, 1, 2})
        {
            GothicWindowSettings s = withProfile(profile);
            s.recursion = recursion;
            const Window w(s);
            const PrintConstraints c = at(0.1);
            const GothicPrintVerdict v = CheckGothicExportAndPrint(w.shape, *w.mesh, c);
            SCOPED_TRACE("profile " + std::to_string(profile) + ", recursion " + std::to_string(recursion));
            EXPECT_TRUE(v.printError.empty()) << v.printError;
            expectSameCheck(v.check, w.Check(c));
            EXPECT_EQ(v.IsPrintable(), w.Check(c).IsPrintable());
        }
}

TEST(TEST_cgmesh_gothic_print, export_and_print_of_refused_geometry_measures_nothing)
{
    GothicWindowSettings huge;
    huge.bodyHeight = 1e18f;
    const Window tall(huge);
    PrintConstraints c = at(0.1);
    c.envelopeMm = std::array<double, 3>{1, 1, 1};
    const GothicPrintVerdict v = CheckGothicExportAndPrint(tall.shape, *tall.mesh, c);
    EXPECT_FALSE(v.ExportAllowed());
    EXPECT_EQ(v.check.scope, GothicPrintScope::NotRun);
    EXPECT_EQ(v.check.reason, "geometry refused");
    EXPECT_FALSE(v.check.print.evaluated);
    for (std::size_t k = 0; k < kPrintDefectCount; ++k)
        EXPECT_EQ(v.check.print.counts[k], 0u) << PrintDefectName(static_cast<PrintDefect>(k));
    EXPECT_FALSE(v.printedSizeMm.has_value());
    EXPECT_TRUE(v.printError.empty());
}

// --- enveloppe, sur le maillage exporte ---------------------------------------

TEST(TEST_cgmesh_gothic_print, page_model_envelope_fits_by_rotation_at_half_scale)
{
    ParameterizedGothicWindow page;
    ASSERT_TRUE(page.LoadFromJson(readText("./test/data/gothic-window-instance.json")));
    page.Regenerate();
    const std::unique_ptr<Mesh> mesh(page.TakeMesh());
    ASSERT_TRUE(mesh);

    const std::array<double, 3> printer = {218, 123, 250};
    auto exceeded = [&](double mmPerUnit) {
        PrintConstraints c = at(mmPerUnit);
        c.envelopeMm = printer;
        const GothicPrintVerdict v = CheckGothicExportAndPrint(page, *mesh, c);
        EXPECT_TRUE(v.printError.empty()) << v.printError;
        return v;
    };
    const GothicPrintVerdict full = exceeded(1.0);
    EXPECT_EQ(full.check.print.Count(PrintDefect::EnvelopeExceeded), 1u);
    const GothicPrintVerdict tenth = exceeded(0.1);
    EXPECT_EQ(tenth.check.print.Count(PrintDefect::EnvelopeExceeded), 0u);
    const GothicPrintVerdict half = exceeded(0.5);
    EXPECT_EQ(half.check.print.Count(PrintDefect::EnvelopeExceeded), 0u);
    // A 0,5 mm / u, les axes du modele ne logent pas la piece : seule une
    // rotation la loge.
    ASSERT_TRUE(half.printedSizeMm.has_value());
    EXPECT_NEAR((*half.printedSizeMm)[0], 116.0, 1e-3);
    EXPECT_NEAR((*half.printedSizeMm)[1], 185.7288, 1e-3);   // 371,4576 u : de y = -180 au sommet, y = 191,4576
    EXPECT_NEAR((*half.printedSizeMm)[2], 10.0, 1e-3);
    EXPECT_GT((*half.printedSizeMm)[1], printer[1]);

    // Sans enveloppe : rien.
    const GothicPrintVerdict none = CheckGothicExportAndPrint(page, *mesh, at(1.0));
    EXPECT_EQ(none.check.print.Count(PrintDefect::EnvelopeExceeded), 0u);
}

TEST(TEST_cgmesh_gothic_print, check_gothic_print_reports_the_envelope_of_the_mesh)
{
    // Plaque plate, ebrasement (pile de tranches) et barres (plaque mesuree a
    // plat) : l'enveloppe est celle du MAILLAGE, barres comprises.
    for (int profile : {0, 1, 2})
    {
        SCOPED_TRACE("profile " + std::to_string(profile));
        const Window w(withProfile(profile));
        PrintConstraints c = at(1.0);
        c.maxTipLengthMm = 2.0;
        c.envelopeMm = std::array<double, 3>{100, 100, 100};
        const GothicPrintCheck r = w.Check(c);
        EXPECT_EQ(r.print.Count(PrintDefect::EnvelopeExceeded), 1u);
        std::size_t found = 0;
        for (const PrintDefectRecord &d : r.print.records)
            if (d.kind == PrintDefect::EnvelopeExceeded)
            {
                ++found;
                EXPECT_NEAR(d.boxMm[0], -116.0, 1e-3);
                EXPECT_NEAR(d.boxMm[2], 116.0, 1e-3);
                EXPECT_NEAR(d.zLowMm, 0.0, 1e-6);
                if (profile == 2)
                {
                    EXPECT_LT(d.boxMm[1], -260.0 - 1.0);   // pied des barres
                    EXPECT_GT(d.zHighMm, 20.0 + 1.0);      // barres au-dessus de la face avant
                }
                else
                {
                    EXPECT_NEAR(d.boxMm[1], -260.0, 1e-3);
                    EXPECT_NEAR(d.zHighMm, 20.0, 1e-6);
                }
            }
        EXPECT_EQ(found, 1u);
        EXPECT_FALSE(r.IsPrintable());

        // Une enveloppe qui loge la piece n'ajoute rien.
        c.envelopeMm = std::array<double, 3>{1000, 1000, 1000};
        EXPECT_EQ(w.Check(c).print.Count(PrintDefect::EnvelopeExceeded), 0u);
    }
}

TEST(TEST_cgmesh_gothic_print, domain_error_is_captured_after_the_envelope)
{
    // 3 000 u a 1 mm / u : demi-emprise bien au-dela de 1 048,58 mm.
    GothicWindowSettings s;
    s.width = 3000.f;
    const Window w(s);
    PrintConstraints c = at(1.0);
    c.envelopeMm = std::array<double, 3>{218, 123, 250};
    ASSERT_THROW(w.Check(c), std::invalid_argument);

    GothicPrintVerdict v;
    ASSERT_NO_THROW(v = CheckGothicExportAndPrint(w.shape, *w.mesh, c));
    EXPECT_TRUE(v.ExportAllowed()) << v.check.geometry.message;
    EXPECT_NE(v.printError.find("half-extent"), std::string::npos) << v.printError;
    EXPECT_EQ(v.check.reason, "print check error");
    EXPECT_EQ(v.check.print.Count(PrintDefect::EnvelopeExceeded), 1u);
    EXPECT_FALSE(v.check.print.evaluated);
    EXPECT_FALSE(v.IsPrintable());
    ASSERT_TRUE(v.printedSizeMm.has_value());
    EXPECT_GT((*v.printedSizeMm)[0], 3000.0);
}

// --- largeur imprimee -> echelle ----------------------------------------------

TEST(TEST_cgmesh_gothic_print, printed_width_is_the_x_extent_of_the_exported_mesh)
{
    const Window w;   // emprise x [-116, 116]
    const double s = GothicMmPerUnitForPrintedWidth(*w.mesh, 50.0);
    EXPECT_NEAR(s, 50.0 / 232.0, 1e-12);
    const GothicPrintVerdict v = CheckGothicExportAndPrint(w.shape, *w.mesh, at(s));
    ASSERT_TRUE(v.printedSizeMm.has_value());
    EXPECT_NEAR((*v.printedSizeMm)[0], 50.0, 1e-9);

    // Width change la piece, pas la largeur imprimee : l'echelle suit.
    GothicWindowSettings wide;
    wide.width = 300.f;
    const Window w300(wide);
    const double s300 = GothicMmPerUnitForPrintedWidth(*w300.mesh, 50.0);
    EXPECT_LT(s300, s);
    const GothicPrintVerdict v300 = CheckGothicExportAndPrint(w300.shape, *w300.mesh, at(s300));
    ASSERT_TRUE(v300.printedSizeMm.has_value());
    EXPECT_NEAR((*v300.printedSizeMm)[0], 50.0, 1e-9);

    EXPECT_THROW(GothicMmPerUnitForPrintedWidth(*w.mesh, 0.0), std::invalid_argument);
    EXPECT_THROW(GothicMmPerUnitForPrintedWidth(*w.mesh, std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
    const Mesh empty;
    EXPECT_THROW(GothicMmPerUnitForPrintedWidth(empty, 50.0), std::invalid_argument);
}

// --- garde d'export a largeur imprimee (maker) --------------------------------
//
// L'echelle vient de la largeur imprimee de la piece, jamais du document de
// l'imprimante ; les seuils et l'enveloppe, de l'imprimante.

namespace
{
    PrintConstraints printerWithScale(double mmPerUnit)
    {
        PrintConstraints c;
        c.mmPerUnit = mmPerUnit;
        c.envelopeMm = std::array<double, 3>{1, 1, 1};
        return c;
    }
}

TEST(TEST_cgmesh_gothic_print, at_width_without_width_ignores_the_printer_scale)
{
    const Window w;
    const GothicPrintVerdict v = CheckGothicExportAndPrintAtWidth(w.shape, *w.mesh, printerWithScale(1.0), std::nullopt);
    EXPECT_TRUE(v.ExportAllowed());
    EXPECT_EQ(v.check.scope, GothicPrintScope::NotRun);
    EXPECT_EQ(v.check.reason, "print scale not set");
    EXPECT_EQ(v.check.print.Count(PrintDefect::ScaleUnset), 1u);
    EXPECT_EQ(v.check.print.Count(PrintDefect::EnvelopeExceeded), 0u);
    EXPECT_TRUE(v.printError.empty()) << v.printError;
    EXPECT_FALSE(v.printedSizeMm.has_value());
    ASSERT_TRUE(v.constraints.has_value());
    EXPECT_FALSE(v.constraints->mmPerUnit.has_value());
    ASSERT_TRUE(v.constraints->envelopeMm.has_value());
    EXPECT_EQ((*v.constraints->envelopeMm)[0], 1.0);
}

TEST(TEST_cgmesh_gothic_print, at_width_equals_export_and_print_at_the_derived_scale)
{
    for (int profile : {0, 1, 2})
    {
        SCOPED_TRACE("profile " + std::to_string(profile));
        const Window w(withProfile(profile));
        PrintConstraints printer = printerWithScale(1.0);
        printer.envelopeMm = std::array<double, 3>{218, 123, 250};
        PrintConstraints scaled = printer;
        scaled.mmPerUnit = GothicMmPerUnitForPrintedWidth(*w.mesh, 30.0);
        const GothicPrintVerdict v = CheckGothicExportAndPrintAtWidth(w.shape, *w.mesh, printer, 30.0);
        const GothicPrintVerdict ref = CheckGothicExportAndPrint(w.shape, *w.mesh, scaled);
        EXPECT_TRUE(v.printError.empty()) << v.printError;
        expectSameCheck(v.check, ref.check);
        ASSERT_TRUE(v.printedSizeMm.has_value());
        EXPECT_NEAR((*v.printedSizeMm)[0], 30.0, 1e-9);
        ASSERT_TRUE(v.constraints.has_value() && v.constraints->mmPerUnit.has_value());
        EXPECT_EQ(*v.constraints->mmPerUnit, *scaled.mmPerUnit);
    }
}

TEST(TEST_cgmesh_gothic_print, at_width_keeps_the_printed_width_when_width_changes)
{
    const Window w;
    GothicWindowSettings wide;
    wide.width = 300.f;
    const Window w300(wide);
    const PrintConstraints printer;
    const GothicPrintVerdict a = CheckGothicExportAndPrintAtWidth(w.shape, *w.mesh, printer, 50.0);
    const GothicPrintVerdict b = CheckGothicExportAndPrintAtWidth(w300.shape, *w300.mesh, printer, 50.0);
    ASSERT_TRUE(a.printedSizeMm.has_value() && b.printedSizeMm.has_value());
    EXPECT_NEAR((*a.printedSizeMm)[0], 50.0, 1e-9);
    EXPECT_NEAR((*b.printedSizeMm)[0], 50.0, 1e-9);
    ASSERT_TRUE(a.constraints && a.constraints->mmPerUnit && b.constraints && b.constraints->mmPerUnit);
    EXPECT_NEAR(*a.constraints->mmPerUnit, 50.0 / 232.0, 1e-12);
    EXPECT_LT(*b.constraints->mmPerUnit, *a.constraints->mmPerUnit);
}

TEST(TEST_cgmesh_gothic_print, invalid_printed_width_never_blocks_an_allowed_export)
{
    const Window w;
    const double widths[] = {0.0, -5.0, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()};
    for (double width : widths)
    {
        SCOPED_TRACE("width " + std::to_string(width));
        GothicPrintVerdict v;
        ASSERT_NO_THROW(v = CheckGothicExportAndPrintAtWidth(w.shape, *w.mesh, PrintConstraints(), width));
        EXPECT_TRUE(v.ExportAllowed());
        EXPECT_EQ(v.check.scope, GothicPrintScope::NotRun);
        EXPECT_EQ(v.check.reason, "print check error");
        EXPECT_EQ(v.printError.rfind("printed width: ", 0), 0u) << v.printError;
        EXPECT_FALSE(v.check.print.evaluated);
        EXPECT_FALSE(v.printedSizeMm.has_value());
        EXPECT_FALSE(v.IsPrintable());
    }
}

TEST(TEST_cgmesh_gothic_print, at_width_of_refused_geometry_measures_nothing)
{
    GothicWindowSettings huge;
    huge.bodyHeight = 1e18f;
    const Window tall(huge);
    for (double width : {50.0, 0.0})
    {
        const GothicPrintVerdict v = CheckGothicExportAndPrintAtWidth(tall.shape, *tall.mesh, PrintConstraints(), width);
        EXPECT_FALSE(v.ExportAllowed());
        EXPECT_EQ(v.check.reason, "geometry refused");
        EXPECT_TRUE(v.printError.empty()) << v.printError;
        EXPECT_FALSE(v.printedSizeMm.has_value());
    }
}

TEST(TEST_cgmesh_gothic_print, at_width_uses_the_printer_thresholds)
{
    ParameterizedGothicWindow page;
    ASSERT_TRUE(page.LoadFromJson(readText("./test/data/gothic-window-instance.json")));
    page.Regenerate();
    const std::unique_ptr<Mesh> mesh(page.TakeMesh());
    ASSERT_TRUE(mesh);

    PrintConstraints tolerant;
    tolerant.maxTipLengthMm = 2.0;
    const GothicPrintVerdict loose = CheckGothicExportAndPrintAtWidth(page, *mesh, tolerant, 100.0);
    EXPECT_EQ(loose.check.scope, GothicPrintScope::Complete) << loose.check.reason;
    EXPECT_TRUE(loose.IsPrintable());

    const GothicPrintVerdict strict = CheckGothicExportAndPrintAtWidth(page, *mesh, PrintConstraints(), 100.0);
    EXPECT_EQ(strict.check.scope, GothicPrintScope::ProfileNotChecked);
    EXPECT_FALSE(strict.IsPrintable());
}
