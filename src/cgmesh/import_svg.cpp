#include "import_svg.h"

#include "extrude_contours.h"
#include "mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "stroke_contours.h"
#include "../cgmath/bezier_flatten.h"

// nanosvg is a single-header library; expand its implementation here.
#define NANOSVG_IMPLEMENTATION
#include "../../extern/nanosvg/nanosvg.h"

// ============================================================================
//  Bezier flattening
// ============================================================================
//
// L'algorithme lui-meme vit desormais dans cgmath/bezier_flatten.h : meme
// subdivision de De Casteljau, meme critere de platitude, meme garde-fou de
// profondeur -- il est simplement partage avec les contours de glyphes
// (text_extrude.cpp), qui en ont besoin en variante QUADRATIQUE. Ne reste ici
// que ce qui est propre a nanosvg : le parcours du tableau de points.

namespace {

// Flatten one NSVGpath into a list of 2D points (the first point is the
// start, then one point per Bezier segment endpoint after subdivision).
// Returns empty if the path has fewer than 2 Bezier points.
std::vector<Vector2f> flattenPath(const NSVGpath* path, float tol)
{
    std::vector<Vector2f> pts;
    if (path->npts < 2) return pts;

    pts.emplace_back(path->pts[0], path->pts[1]);

    // Each cubic segment uses 6 floats (cp1x, cp1y, cp2x, cp2y, x, y),
    // appended after the starting (x0,y0).
    for (int i = 0; i + 3 < path->npts; i += 3)
    {
        const Vector2f p0(path->pts[i*2 + 0], path->pts[i*2 + 1]);
        const Vector2f c0(path->pts[i*2 + 2], path->pts[i*2 + 3]);
        const Vector2f c1(path->pts[i*2 + 4], path->pts[i*2 + 5]);
        const Vector2f p1(path->pts[i*2 + 6], path->pts[i*2 + 7]);
        flattenCubic(pts, p0, c0, c1, p1, tol);
    }

    // Remove a trailing duplicate of the start (some SVG authoring tools
    // close paths by repeating the first vertex).
    if (pts.size() >= 2)
    {
        const Vector2f& a = pts.front();
        const Vector2f& b = pts.back();
        if (std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f)
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
void recenterAndFit(std::vector<std::vector<Vector2f>>& shapes)
{
    bool any = false;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
    for (const auto& pts : shapes)
        for (const auto& v : pts)
        {
            if (!any) { minX = maxX = v.x; minY = maxY = v.y; any = true; continue; }
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
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
            v.x = (v.x - cx) * scale;
            v.y = (v.y - cy) * scale;
        }
}

// ============================================================================
//  Frontiere avec strokeToContours
// ============================================================================
//
// stroke_contours.h parle en std::array<float,2> et sert aussi
// parameterized_shapes.cpp : changer sa signature deborderait de ce fichier. On
// convertit donc de part et d'autre de l'appel, sur les seuls traces ouverts.

std::vector<std::array<float, 2>> toArrays(const std::vector<Vector2f>& pts)
{
    std::vector<std::array<float, 2>> out;
    out.reserve(pts.size());
    for (const Vector2f& p : pts) out.push_back({ p.x, p.y });
    return out;
}

std::vector<Vector2f> fromArrays(const std::vector<std::array<float, 2>>& pts)
{
    std::vector<Vector2f> out;
    out.reserve(pts.size());
    for (const auto& p : pts) out.emplace_back(p[0], p[1]);
    return out;
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
    std::vector<std::vector<std::vector<Vector2f>>> shapeContours;
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
        std::vector<std::vector<Vector2f>> filled;
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
                openPaths.push_back(toArrays(pts));
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
            const auto ribbons = strokeToContours(openPaths, w, join, cap);
            if (!ribbons.empty())
            {
                std::vector<std::vector<Vector2f>> converted;
                converted.reserve(ribbons.size());
                for (const auto& r : ribbons)
                    converted.push_back(fromArrays(r));
                shapeContours.push_back(std::move(converted));
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
        std::vector<std::vector<Vector2f>> flat;
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
            for (const Vector2f& p : pts)
                c.pts.emplace_back(p.x, opt.invertY ? -p.y : p.y);
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
