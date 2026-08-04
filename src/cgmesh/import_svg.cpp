#include "import_svg.h"

#include "extrude_contours.h"
#include "mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "stroke_contours.h"

// nanosvg is a single-header library; expand its implementation here.
#define NANOSVG_IMPLEMENTATION
#include "../../extern/nanosvg/nanosvg.h"

// ============================================================================
//  Bezier flattening
// ============================================================================

namespace {

// Standard "flatness" measure for a cubic Bezier: maximum perpendicular
// distance of the control points to the chord (x0,y0)-(x3,y3). Below the
// tolerance, the segment can be approximated by a straight chord.
bool cubicFlatEnough(float x0, float y0, float x1, float y1,
                     float x2, float y2, float x3, float y3,
                     float tol)
{
    const float ux = 3.0f * x1 - 2.0f * x0 - x3;
    const float uy = 3.0f * y1 - 2.0f * y0 - y3;
    const float vx = 3.0f * x2 - 2.0f * x3 - x0;
    const float vy = 3.0f * y2 - 2.0f * y3 - y0;
    const float du = ux * ux + uy * uy;
    const float dv = vx * vx + vy * vy;
    return std::max(du, dv) <= tol * tol;
}

void flattenCubic(std::vector<std::array<float, 2>>& out,
                  float x0, float y0, float x1, float y1,
                  float x2, float y2, float x3, float y3,
                  float tol, int depth)
{
    if (depth > 12 || cubicFlatEnough(x0, y0, x1, y1, x2, y2, x3, y3, tol))
    {
        out.push_back({ x3, y3 });
        return;
    }

    // De Casteljau subdivision at t=0.5
    const float x01  = (x0 + x1) * 0.5f, y01  = (y0 + y1) * 0.5f;
    const float x12  = (x1 + x2) * 0.5f, y12  = (y1 + y2) * 0.5f;
    const float x23  = (x2 + x3) * 0.5f, y23  = (y2 + y3) * 0.5f;
    const float x012 = (x01 + x12) * 0.5f, y012 = (y01 + y12) * 0.5f;
    const float x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
    const float xmid = (x012 + x123) * 0.5f, ymid = (y012 + y123) * 0.5f;

    flattenCubic(out, x0, y0, x01, y01, x012, y012, xmid, ymid, tol, depth + 1);
    flattenCubic(out, xmid, ymid, x123, y123, x23, y23, x3, y3, tol, depth + 1);
}

// Flatten one NSVGpath into a list of 2D points (the first point is the
// start, then one point per Bezier segment endpoint after subdivision).
// Returns empty if the path has fewer than 2 Bezier points.
std::vector<std::array<float, 2>> flattenPath(const NSVGpath* path, float tol)
{
    std::vector<std::array<float, 2>> pts;
    if (path->npts < 2) return pts;

    pts.push_back({ path->pts[0], path->pts[1] });

    // Each cubic segment uses 6 floats (cp1x, cp1y, cp2x, cp2y, x, y),
    // appended after the starting (x0,y0).
    for (int i = 0; i + 3 < path->npts; i += 3)
    {
        const float x0 = path->pts[i*2 + 0], y0 = path->pts[i*2 + 1];
        const float x1 = path->pts[i*2 + 2], y1 = path->pts[i*2 + 3];
        const float x2 = path->pts[i*2 + 4], y2 = path->pts[i*2 + 5];
        const float x3 = path->pts[i*2 + 6], y3 = path->pts[i*2 + 7];
        flattenCubic(pts, x0, y0, x1, y1, x2, y2, x3, y3, tol, 0);
    }

    // Remove a trailing duplicate of the start (some SVG authoring tools
    // close paths by repeating the first vertex).
    if (pts.size() >= 2)
    {
        const auto& a = pts.front();
        const auto& b = pts.back();
        if (std::fabs(a[0] - b[0]) < 1e-6f && std::fabs(a[1] - b[1]) < 1e-6f)
            pts.pop_back();
    }
    return pts;
}

// ============================================================================
//  Pixel space -> world XY
// ============================================================================
//
// The extrusion primitive (extrude_contours.h) expects contours already in
// final world XY, so the recentering / fitting / Y flip all happen here, on the
// flattened contour points, before anything is tessellated. A uniform positive
// scale followed by a Y flip is orientation-consistent, and the primitive
// derives cap and wall orientation from the world geometry itself, so doing this
// before rather than after tessellation is equivalent.

// Recenter on the XY bbox and scale so the longest XY dimension equals 1.0
// (consistent with the other parameterized geometries in sinaia).
void recenterAndFit(std::vector<std::vector<std::array<float, 2>>>& shapes)
{
    bool any = false;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
    for (const auto& pts : shapes)
        for (const auto& v : pts)
        {
            if (!any) { minX = maxX = v[0]; minY = maxY = v[1]; any = true; continue; }
            minX = std::min(minX, v[0]); maxX = std::max(maxX, v[0]);
            minY = std::min(minY, v[1]); maxY = std::max(maxY, v[1]);
        }
    if (!any) return;

    const float cx = 0.5f * (minX + maxX);
    const float cy = 0.5f * (minY + maxY);
    const float largestXY = std::max(maxX - minX, maxY - minY);
    if (largestXY < 1e-9f) return;
    const float scale = 1.0f / largestXY;

    for (auto& pts : shapes)
        for (auto& v : pts)
        {
            v[0] = (v[0] - cx) * scale;
            v[1] = (v[1] - cy) * scale;
        }
}

} // namespace

Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt)
{
    NSVGimage* image = nsvgParseFromFile(filename.c_str(), "px", 96.0f);
    if (!image)
    {
        std::fprintf(stderr, "import_svg: failed to parse %s\n", filename.c_str());
        return nullptr;
    }

    // One entry per fillable shape: its contours flattened to polylines. Kept
    // per shape because the fill rule (and hence the winding rule handed to the
    // tessellator) is a per-shape property.
    std::vector<std::vector<std::vector<std::array<float, 2>>>> shapeContours;
    std::vector<bool> shapeEvenOdd;

    for (NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        // Decor de mise en page : ecarte avant tout traitement (cf.
        // SvgExtrudeOptions::ignoreShapeId).
        if (!opt.ignoreShapeId.empty() && opt.ignoreShapeId == shape->id)
            continue;

        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);

        // Ni remplissage ni trait : rien a produire.
        if (!hasFill && !(hasStroke && opt.strokeToVolume)) continue;

        // La decision se prend PAR CHEMIN, sur NSVGpath::closed, et non par forme
        // sur l'attribut `fill` : un meme <g> peut melanger des contours fermes et
        // des polylignes ouvertes. nanosvg renseigne ce drapeau fidelement --
        // `<polygon>` et `<path ... Z>` donnent closed = 1, `<polyline>` et un
        // `<path>` sans Z donnent 0 (nanosvg.h:2826-2833).
        //
        //   ferme  -> frontiere de SURFACE, tessellation (>= 3 points)
        //   ouvert -> LIGNE, epaissie de son stroke-width (>= 2 points)
        //
        // C'est ce qui manquait : sans lui, une polyligne ouverte etait refermee
        // d'office pour etre remplie. Sur un trace qui se replie sur lui-meme
        // (courbe du dragon) le remplissage degenere en damier, faute de pouvoir
        // designer un interieur.
        std::vector<std::vector<std::array<float, 2>>> filled;
        std::vector<std::vector<std::array<float, 2>>> openPaths;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
        {
            auto pts = flattenPath(path, opt.flattenTol);
            const bool closed = (path->closed != 0);
            if (closed && hasFill)
            {
                if (pts.size() < 3) continue;
                filled.push_back(std::move(pts));
            }
            else if (opt.strokeToVolume && hasStroke)
            {
                if (pts.size() < 2) continue;
                openPaths.push_back(std::move(pts));
            }
            else if (hasFill && pts.size() >= 3)
            {
                // Ouvert mais sans trait exploitable : on retombe sur l'ancien
                // comportement, nanosvg refermant le trace pour le remplir.
                filled.push_back(std::move(pts));
            }
        }

        if (!filled.empty())
        {
            shapeContours.push_back(std::move(filled));
            shapeEvenOdd.push_back(shape->fillRule == NSVG_FILLRULE_EVENODD);
        }

        if (!openPaths.empty())
        {
            float w = shape->strokeWidth * opt.strokeScale;
            if (!(w > 0.f)) w = opt.strokeWidthFallback * opt.strokeScale;

            // Traduction des conventions nanosvg vers l'API partagee : c'est a
            // l'appelant de la faire, strokeToContours ne connait pas nanosvg.
            const StrokeJoin join = (shape->strokeLineJoin == NSVG_JOIN_MITER) ? StrokeJoin::Miter
                                  : (shape->strokeLineJoin == NSVG_JOIN_BEVEL) ? StrokeJoin::Bevel
                                                                               : StrokeJoin::Round;
            const StrokeCap  cap  = (shape->strokeLineCap == NSVG_CAP_BUTT)   ? StrokeCap::Butt
                                  : (shape->strokeLineCap == NSVG_CAP_SQUARE) ? StrokeCap::Square
                                                                              : StrokeCap::Round;
            auto ribbons = strokeToContours(openPaths, w, join, cap);
            if (!ribbons.empty())
            {
                shapeContours.push_back(std::move(ribbons));
                // Les contours sortis de Clipper2 portent leur propre orientation :
                // enveloppes en sens positif, trous en sens inverse -- exactement la
                // convention de NonZero, qui soustrait donc les trous (l'interieur
                // d'une boucle du trace) sans qu'on ait a reorienter.
                shapeEvenOdd.push_back(false);
            }
        }
    }

    nsvgDelete(image);

    if (shapeContours.empty())
    {
        std::fprintf(stderr, "import_svg: %s has no fillable shapes\n", filename.c_str());
        return nullptr;
    }

    if (opt.centerAndFit)
    {
        // Fit over ALL shapes at once so their relative placement survives.
        std::vector<std::vector<std::array<float, 2>>> flat;
        for (auto& contours : shapeContours)
            for (auto& pts : contours)
                flat.push_back(std::move(pts));
        recenterAndFit(flat);
        size_t k = 0;
        for (auto& contours : shapeContours)
            for (auto& pts : contours)
                pts = std::move(flat[k++]);
    }

    ExtrudedMeshBuilder builder;
    for (size_t s = 0; s < shapeContours.size(); ++s)
    {
        std::vector<ExtrudeContour> contours;
        contours.reserve(shapeContours[s].size());
        for (const auto& pts : shapeContours[s])
        {
            ExtrudeContour c;
            c.pts.reserve(pts.size());
            for (const auto& p : pts)
                c.pts.emplace_back(p[0], opt.invertY ? -p[1] : p[1]);
            contours.push_back(std::move(c));
        }

        ExtrudeAppendOptions ao;
        ao.zBottom = 0.0f;
        ao.zTop    = opt.height;
        ao.winding = shapeEvenOdd[s] ? ExtrudeWinding::EvenOdd
                                     : ExtrudeWinding::NonZero;
        // SVG holes are inner contours of opposite orientation as AUTHORED, so
        // the orientation must be taken at face value — rewinding here would
        // fill the holes in.
        ao.normalizeOrientation = false;
        builder.Append(contours, ao);
    }

    if (builder.Empty())
    {
        std::fprintf(stderr, "import_svg: %s tessellated to nothing\n", filename.c_str());
        return nullptr;
    }

    return builder.Build();
}
