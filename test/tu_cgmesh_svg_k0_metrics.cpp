// ============================================================================
//  K0 -- campagne de mesure du chemin SVG (svg_couleurs_faisabilite.md, section 12)
// ============================================================================
//
// Ce fichier ne construit rien de la fonctionnalite « import SVG colore » : il
// MESURE le fichier de reference et publie ses grandeurs, pour que le plan soit
// dimensionne sur des chiffres et non sur des estimations. Les valeurs attendues
// du document sont verifiees par EXPECT_* et jamais par ASSERT_* : un ecart doit
// laisser les autres mesures s'afficher, c'est la mesure qui tranche.
//
// Les chiffres sont ecrits sur std::cout (lisibles dans la sortie de ctest avec
// --output-on-failure ou en lancant TU directement) ET dans les proprietes de
// test, pour la sortie XML.
//
// ⚠ nanosvg est instancie une seule fois dans le depot, par import_svg.cpp qui
// definit NANOSVG_IMPLEMENTATION. L'en-tete est donc inclus ici SANS la macro.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/stroke_contours.h"

#include <cgmath/bezier_flatten.h>

#include "../extern/nanosvg/nanosvg.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

const char* kTiger     = "./test/data/svg/Ghostscript_Tiger.svg";
const char* kRose      = "./test/data/svg/rose.svg";
const char* kSpiderman = "./test/data/svg/spiderman.svg";

// ---------------------------------------------------------------------------
//  Comptes de base, lus sur l'image nanosvg
// ---------------------------------------------------------------------------

struct SvgBaseCounts
{
    int nShapes             = 0;  // NSVGshape produits par nanosvg
    int nWithFill           = 0;  // fill.type != NSVG_PAINT_NONE
    int nWithStroke         = 0;  // stroke.type != NSVG_PAINT_NONE
    int nWithBoth           = 0;
    int nWithNeither        = 0;
    int nGradients          = 0;  // fill OU stroke en LINEAR/RADIAL_GRADIENT
    int nDistinctFillColors = 0;  // couleurs distinctes parmi les fills COLOR
    int nPathsClosed        = 0;  // NSVGpath::closed != 0
    int nPathsOpen          = 0;
    int nSubPaths           = 0;
    int nEvenOdd            = 0;  // fillRule == NSVG_FILLRULE_EVENODD
    int nNotVisible         = 0;  // NSVG_FLAGS_VISIBLE absent
    int nRetained           = 0;  // formes que svg_to_contours conserve
};

bool isGradient(const NSVGpaint& p)
{
    return p.type == NSVG_PAINT_LINEAR_GRADIENT || p.type == NSVG_PAINT_RADIAL_GRADIENT;
}

SvgBaseCounts collectBaseCounts(const NSVGimage* image)
{
    SvgBaseCounts c;
    std::set<unsigned int> fillColors;

    for (const NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        ++c.nShapes;

        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);
        if (hasFill)               ++c.nWithFill;
        if (hasStroke)             ++c.nWithStroke;
        if (hasFill && hasStroke)  ++c.nWithBoth;
        if (!hasFill && !hasStroke)++c.nWithNeither;

        if (isGradient(shape->fill) || isGradient(shape->stroke)) ++c.nGradients;
        if (shape->fill.type == NSVG_PAINT_COLOR) fillColors.insert(shape->fill.color);

        if (shape->fillRule == NSVG_FILLRULE_EVENODD) ++c.nEvenOdd;
        if ((shape->flags & NSVG_FLAGS_VISIBLE) == 0) ++c.nNotVisible;

        // Le pipeline retient une forme des qu'elle a un remplissage, ou un
        // trait quand strokeToVolume est actif (import_svg.cpp).
        if (hasFill || hasStroke) ++c.nRetained;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
        {
            ++c.nSubPaths;
            if (path->closed) ++c.nPathsClosed; else ++c.nPathsOpen;
        }
    }

    c.nDistinctFillColors = (int)fillColors.size();
    return c;
}

// ---------------------------------------------------------------------------
//  Regions par forme, dans les unites du maillage produit
// ---------------------------------------------------------------------------
//
// Le chemin ci-dessous reproduit celui de svg_to_contours (import_svg.cpp) sans
// l'aplatissement final en une liste unique : c'est cette identite de forme,
// absente de l'API actuelle, que M2a consommera. Meme tolerance, meme test
// ferme/ouvert, meme epaississement des traces ouverts, meme recentrage.
//
// Biais connu : la regle EvenOdd n'est pas honoree ici -- contour_ops n'expose
// qu'une union NonZero. Le compte de formes EvenOdd est publie a part, ce qui
// rend l'ecart visible.

struct ShapeRegion
{
    std::vector<ExtrudeContour> contours;   // region resolue, unites monde
    float bounds[4] = { 0.f, 0.f, 0.f, 0.f };  // bbox nanosvg, unites document
};

std::vector<Vector2f> flattenPath(const NSVGpath* path, float tol)
{
    std::vector<Vector2f> pts;
    if (path->npts < 2) return pts;

    pts.emplace_back(path->pts[0], path->pts[1]);
    for (int i = 0; i + 3 < path->npts; i += 3)
    {
        const Vector2f p0(path->pts[i*2 + 0], path->pts[i*2 + 1]);
        const Vector2f c0(path->pts[i*2 + 2], path->pts[i*2 + 3]);
        const Vector2f c1(path->pts[i*2 + 4], path->pts[i*2 + 5]);
        const Vector2f p1(path->pts[i*2 + 6], path->pts[i*2 + 7]);
        flattenCubic(pts, p0, c0, c1, p1, tol);
    }
    if (pts.size() >= 2)
    {
        const Vector2f& a = pts.front();
        const Vector2f& b = pts.back();
        if (std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f)
            pts.pop_back();
    }
    return pts;
}

std::vector<std::array<float, 2>> toArrays(const std::vector<Vector2f>& pts)
{
    std::vector<std::array<float, 2>> out;
    out.reserve(pts.size());
    for (const Vector2f& p : pts) out.push_back({ p.x, p.y });
    return out;
}

// Emprise des formes retenues, lue sur les bbox nanosvg. Elle convertit la
// tolerance des unites de sortie vers celles du document, comme import_svg.cpp.
float retainedExtent(const NSVGimage* image)
{
    bool any = false;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
    for (const NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        if (shape->fill.type == NSVG_PAINT_NONE && shape->stroke.type == NSVG_PAINT_NONE)
            continue;
        if (!any)
        {
            minX = shape->bounds[0]; minY = shape->bounds[1];
            maxX = shape->bounds[2]; maxY = shape->bounds[3];
            any = true;
            continue;
        }
        minX = std::min(minX, shape->bounds[0]); minY = std::min(minY, shape->bounds[1]);
        maxX = std::max(maxX, shape->bounds[2]); maxY = std::max(maxY, shape->bounds[3]);
    }
    return any ? std::max(maxX - minX, maxY - minY) : 0.f;
}

std::vector<ShapeRegion> buildShapeRegions(const NSVGimage* image,
                                           const SvgExtrudeOptions& opt)
{
    const float extent = retainedExtent(image);
    const float tolSrc = (extent > 1e-9f) ? opt.flattenTol * extent : opt.flattenTol;

    struct Raw { std::vector<std::vector<Vector2f>> polys; float bounds[4]; };
    std::vector<Raw> raws;

    for (const NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);
        if (!hasFill && !(hasStroke && opt.strokeToVolume)) continue;

        std::vector<std::vector<Vector2f>> filled;
        std::vector<std::vector<std::array<float, 2>>> openPaths;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
        {
            auto pts = flattenPath(path, tolSrc);
            const bool closed = (path->closed != 0);
            if (closed && hasFill)
            {
                if (pts.size() < 3) continue;
                filled.push_back(std::move(pts));
            }
            else if (opt.strokeToVolume && hasStroke)
            {
                if (pts.size() < 2) continue;
                openPaths.push_back(toArrays(pts));
            }
            else if (hasFill && pts.size() >= 3)
            {
                filled.push_back(std::move(pts));
            }
        }

        if (!openPaths.empty())
        {
            float w = shape->strokeWidth * opt.strokeScale;
            if (!(w > 0.f)) w = opt.strokeWidthFallback * opt.strokeScale;

            const StrokeJoin join = (shape->strokeLineJoin == NSVG_JOIN_MITER) ? StrokeJoin::Miter
                                  : (shape->strokeLineJoin == NSVG_JOIN_BEVEL) ? StrokeJoin::Bevel
                                                                               : StrokeJoin::Round;
            const StrokeCap  cap  = (shape->strokeLineCap == NSVG_CAP_BUTT)   ? StrokeCap::Butt
                                  : (shape->strokeLineCap == NSVG_CAP_SQUARE) ? StrokeCap::Square
                                                                              : StrokeCap::Round;
            for (const auto& ribbon : strokeToContours(openPaths, w, join, cap))
            {
                std::vector<Vector2f> pts;
                pts.reserve(ribbon.size());
                for (const auto& p : ribbon) pts.emplace_back(p[0], p[1]);
                filled.push_back(std::move(pts));
            }
        }

        if (filled.empty()) continue;

        Raw r;
        r.polys = std::move(filled);
        r.bounds[0] = shape->bounds[0]; r.bounds[1] = shape->bounds[1];
        r.bounds[2] = shape->bounds[2]; r.bounds[3] = shape->bounds[3];
        raws.push_back(std::move(r));
    }

    // Recentrage-ajustement global, sur la geometrie aplatie exacte.
    if (opt.centerAndFit)
    {
        bool any = false;
        float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
        for (const Raw& r : raws)
            for (const auto& poly : r.polys)
                for (const Vector2f& v : poly)
                {
                    if (!any) { minX = maxX = v.x; minY = maxY = v.y; any = true; continue; }
                    minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
                    minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
                }
        const float largest = std::max(maxX - minX, maxY - minY);
        if (any && largest > 1e-9f)
        {
            const float cx = 0.5f * (minX + maxX);
            const float cy = 0.5f * (minY + maxY);
            const float scale = 1.f / largest;
            for (Raw& r : raws)
                for (auto& poly : r.polys)
                    for (Vector2f& v : poly)
                    {
                        v.x = (v.x - cx) * scale;
                        v.y = (v.y - cy) * scale;
                    }
        }
    }

    std::vector<ShapeRegion> out;
    out.reserve(raws.size());
    for (const Raw& r : raws)
    {
        std::vector<ExtrudeContour> contours;
        contours.reserve(r.polys.size());
        for (const auto& poly : r.polys)
        {
            ExtrudeContour c;
            c.pts.reserve(poly.size());
            for (const Vector2f& p : poly)
                c.pts.emplace_back(p.x, opt.invertY ? -p.y : p.y);
            contours.push_back(std::move(c));
        }

        ShapeRegion sr;
        // Union NonZero : la normalisation d'orientation que resolveShape
        // applique a chaque forme (import_svg.cpp).
        sr.contours = unionContours(contours, {});
        sr.bounds[0] = r.bounds[0]; sr.bounds[1] = r.bounds[1];
        sr.bounds[2] = r.bounds[2]; sr.bounds[3] = r.bounds[3];
        if (!sr.contours.empty()) out.push_back(std::move(sr));
    }
    return out;
}

// Aire nette d'une region orientee par Clipper2 : enveloppes positives, trous
// negatifs, donc la somme signee est l'aire couverte.
double netArea(const std::vector<ExtrudeContour>& region)
{
    double sum = 0.0;
    for (const ExtrudeContour& c : region) sum += (double)contourSignedArea(c.pts);
    return std::fabs(sum);
}

bool bboxOverlap(const float a[4], const float b[4])
{
    return !(a[2] < b[0] || b[2] < a[0] || a[3] < b[1] || b[3] < a[1]);
}

// ---------------------------------------------------------------------------
//  Publication
// ---------------------------------------------------------------------------

std::string fmt(double v, int digits = 6)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, v);
    return std::string(buf);
}

void printHeader(const char* title)
{
    std::cout << "\n=== " << title << " ===\n";
}

void printLine(const char* label, const std::string& measured, const char* expected)
{
    std::cout << "  " << std::left << std::setw(46) << label
              << std::right << std::setw(14) << measured
              << "   attendu: " << expected << "\n";
}

void printLine(const char* label, long long measured, const char* expected = "-")
{
    printLine(label, std::to_string(measured), expected);
}

void dumpBaseCounts(const char* file, const SvgBaseCounts& c, bool tigerExpectations)
{
    printHeader(file);
    printLine("nShapes",                          c.nShapes,             tigerExpectations ? "239 (240 balises)" : "-");
    printLine("nDistinctFillColors",              c.nDistinctFillColors, tigerExpectations ? "38"  : "-");
    printLine("nGradients",                       c.nGradients,          tigerExpectations ? "0"   : "-");
    printLine("formes avec fill",                 c.nWithFill);
    printLine("formes avec stroke",               c.nWithStroke);
    printLine("formes avec fill ET stroke",       c.nWithBoth);
    printLine("formes sans fill ni stroke",       c.nWithNeither);
    printLine("formes retenues par le pipeline",  c.nRetained);
    printLine("sous-chemins",                     c.nSubPaths);
    printLine("sous-chemins fermes",              c.nPathsClosed);
    printLine("sous-chemins ouverts",             c.nPathsOpen);
    printLine("formes fillRule == EVENODD",       c.nEvenOdd);
    printLine("formes SANS NSVG_FLAGS_VISIBLE",   c.nNotVisible);
}

} // namespace

// ============================================================================
//  Comptes de base
// ============================================================================

TEST(TEST_cgmesh_svg_k0, tiger_base_counts)
{
    NSVGimage* image = nsvgParseFromFile(kTiger, "px", 96.0f);
    ASSERT_NE(image, nullptr) << "le fichier de reference doit parser : " << kTiger;
    ASSERT_NE(image->shapes, nullptr);

    const SvgBaseCounts c = collectBaseCounts(image);
    nsvgDelete(image);

    dumpBaseCounts(kTiger, c, true);

    RecordProperty("nShapes",              c.nShapes);
    RecordProperty("nDistinctFillColors",  c.nDistinctFillColors);
    RecordProperty("nGradients",           c.nGradients);
    RecordProperty("nWithFill",            c.nWithFill);
    RecordProperty("nWithStroke",          c.nWithStroke);
    RecordProperty("nWithBoth",            c.nWithBoth);
    RecordProperty("nWithNeither",         c.nWithNeither);
    RecordProperty("nSubPaths",            c.nSubPaths);
    RecordProperty("nPathsClosed",         c.nPathsClosed);
    RecordProperty("nPathsOpen",           c.nPathsOpen);
    RecordProperty("nEvenOdd",             c.nEvenOdd);
    RecordProperty("nNotVisible",          c.nNotVisible);

    ASSERT_GT(c.nShapes, 0);

    // Valeurs annoncees par la mesure textuelle du document : EXPECT, pour que
    // toutes les grandeurs soient publiees meme en cas d'ecart.
    //
    // 239 formes pour 240 balises <path> : un sous-chemin de moins de quatre
    // points est ecarte (nanosvg.h:1051) et une forme sans sous-chemin n'est pas
    // creee (nanosvg.h:966). Le Tigre contient un `d="M-65.4,9z"`, moveto seul.
    EXPECT_EQ(c.nShapes,             239);
    EXPECT_EQ(c.nDistinctFillColors, 38);
    EXPECT_EQ(c.nGradients,          0);
}

TEST(TEST_cgmesh_svg_k0, reference_files_base_counts)
{
    for (const char* file : { kRose, kSpiderman })
    {
        NSVGimage* image = nsvgParseFromFile(file, "px", 96.0f);
        ASSERT_NE(image, nullptr) << file;

        const SvgBaseCounts c = collectBaseCounts(image);
        nsvgDelete(image);

        dumpBaseCounts(file, c, false);
        EXPECT_GT(c.nShapes, 0) << file;
    }
}

// ============================================================================
//  Grandeurs propres a M2a : aires, recouvrement, paires de bbox
// ============================================================================

TEST(TEST_cgmesh_svg_k0, tiger_overlap_metrics)
{
    SvgExtrudeOptions opt;   // valeurs par defaut = celles du chemin de production
    opt.height = 1.0f;

    NSVGimage* image = nsvgParseFromFile(kTiger, "px", 96.0f);
    ASSERT_NE(image, nullptr);

    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<ShapeRegion> regions = buildShapeRegions(image, opt);
    const auto t1 = std::chrono::steady_clock::now();
    nsvgDelete(image);

    ASSERT_FALSE(regions.empty());

    double totalArea = 0.0;
    for (const ShapeRegion& r : regions) totalArea += netArea(r.contours);

    // Union accumulee : la moitie « Union » du balayage de M2a, sur le fichier
    // de reference. La duree est un ordre de grandeur, pas la mesure de K4.
    const auto t2 = std::chrono::steady_clock::now();
    std::vector<ExtrudeContour> covered;
    for (const ShapeRegion& r : regions) covered = unionContours(covered, r.contours);
    const auto t3 = std::chrono::steady_clock::now();

    const double unionArea = netArea(covered);
    const double overlapRatio = (totalArea > 0.0) ? 1.0 - unionArea / totalArea : 0.0;

    // Paires de bbox secantes, sur NSVGshape::bounds. Le contact simple compte
    // comme une intersection : le chiffre est un MAJORANT du nombre de paires
    // dont les regions se recouvrent reellement.
    long long pairs = 0;
    int coveredByAbove = 0;
    for (size_t i = 0; i < regions.size(); ++i)
    {
        bool hasAbove = false;
        for (size_t j = i + 1; j < regions.size(); ++j)
            if (bboxOverlap(regions[i].bounds, regions[j].bounds))
            {
                ++pairs;
                hasAbove = true;
            }
        if (hasAbove) ++coveredByAbove;
    }

    const long long buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    const long long unionMs = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

    printHeader("Tigre -- grandeurs M2a (unites monde, objet ajuste a 1.0)");
    printLine("regions de forme produites",           (long long)regions.size());
    printLine("contours de l'union",                  (long long)covered.size());
    printLine("aire totale (recouvrements comptes)",  fmt(totalArea),    "-");
    printLine("aire de l'union",                      fmt(unionArea),    "-");
    printLine("taux de recouvrement 1 - union/total", fmt(overlapRatio), "-");
    printLine("formes coupant une forme superieure",  (long long)coveredByAbove);
    printLine("paires de bbox secantes",              pairs, "<= 28680");
    printLine("ms -- aplatissement + resolution",     buildMs);
    printLine("ms -- union accumulee (Clipper2)",     unionMs);

    RecordProperty("nRegions",           (int)regions.size());
    RecordProperty("totalArea",          fmt(totalArea));
    RecordProperty("unionArea",          fmt(unionArea));
    RecordProperty("overlapRatio",       fmt(overlapRatio));
    RecordProperty("shapesCutByAbove",   coveredByAbove);
    RecordProperty("bboxPairs",          (int)pairs);
    RecordProperty("flattenResolveMs",   (int)buildMs);
    RecordProperty("unionAccumulateMs",  (int)unionMs);

    EXPECT_FALSE(covered.empty());
    EXPECT_GT(unionArea, 0.0);
    EXPECT_LE(unionArea, totalArea * 1.001);
}

// ============================================================================
//  Volumetrie (reserve R4)
// ============================================================================

TEST(TEST_cgmesh_svg_k0, tiger_volumetry)
{
    SvgExtrudeOptions opt;
    opt.height       = 1.0f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;

    std::vector<ExtrudeContour> contours;
    ASSERT_TRUE(svg_to_contours(kTiger, opt, contours));

    long long points = 0;
    for (const ExtrudeContour& c : contours) points += (long long)c.pts.size();

    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
    ASSERT_NE(m, nullptr);

    const long long nv = (long long)m->GetNVertices();
    const long long nf = (long long)m->GetNFaces();

    printHeader("Tigre -- volumetrie a flattenTol = 0.005");
    printLine("contours rendus par svg_to_contours", (long long)contours.size());
    printLine("points de contour",                   points,  "6e3 - 2e4");
    printLine("sommets du maillage",                 nv,      "2.4e4 - 8e4");
    printLine("faces du maillage",                   nf,      "2.4e4 - 8e4");

    RecordProperty("nContours",     (int)contours.size());
    RecordProperty("nContourPoints",(int)points);
    RecordProperty("nVertices",     (int)nv);
    RecordProperty("nFaces",        (int)nf);

    EXPECT_GT(points, 0);
    EXPECT_GT(nv, 0);
    EXPECT_GT(nf, 0);
}

TEST(TEST_cgmesh_svg_k0, reference_files_volumetry)
{
    for (const char* file : { kRose, kSpiderman })
    {
        SvgExtrudeOptions opt;
        opt.height       = 1.0f;
        opt.flattenTol   = 0.005f;
        opt.centerAndFit = true;
        opt.invertY      = true;

        std::vector<ExtrudeContour> contours;
        ASSERT_TRUE(svg_to_contours(file, opt, contours)) << file;

        long long points = 0;
        for (const ExtrudeContour& c : contours) points += (long long)c.pts.size();

        std::unique_ptr<Mesh> m(import_svg_extruded(file, opt));
        ASSERT_NE(m, nullptr) << file;

        printHeader(file);
        printLine("contours rendus par svg_to_contours", (long long)contours.size());
        printLine("points de contour",                   points);
        printLine("sommets du maillage",                 (long long)m->GetNVertices());
        printLine("faces du maillage",                   (long long)m->GetNFaces());
    }
}
