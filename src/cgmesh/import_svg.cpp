#include "import_svg.h"

#include "clipper2/clipper.h"   // resolveShape : regles de remplissage

#include "contour_ops.h"   // unionContours, differenceContours, contourSignedArea
#include "extrude_contours.h"
#include "mesh.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>
#include <vector>

#include "stroke_contours.h"
#include <cgmath/bezier_flatten.h>

// nanosvg is a single-header library; expand its implementation here.
#define NANOSVG_IMPLEMENTATION
#include "../../extern/nanosvg/nanosvg.h"

// ============================================================================
//  Bezier flattening
// ============================================================================
//
// La subdivision vit dans cgmath/bezier_flatten.h, partagee avec les contours
// de glyphes (text_extrude.cpp). Ne reste ici que le parcours du tableau de
// points de nanosvg.

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

// Une forme masquee par le document ne porte pas NSVG_FLAGS_VISIBLE
// (nanosvg.h:106, :160) et ne produit donc pas de volume. Le drapeau est
// applique dans TOUS les parcours du fichier, estimation d'emprise comprise.
//
// Seul `display:none` le declenche, en attribut ou en style ; nanosvg ne lit pas
// `visibility` (nanosvg.h:1819-1822).
bool isVisible(const NSVGshape* shape)
{
    return (shape->flags & NSVG_FLAGS_VISIBLE) != 0;
}

// ============================================================================
//  Emprise du document, SANS aplatir
// ============================================================================
//
// Une Bezier est contenue dans l'enveloppe convexe de ses points de controle :
// l'emprise lue sur ces points CONTIENT donc celle du trace. L'ordre de
// grandeur du document est ainsi connu AVANT toute subdivision, ce qui casse la
// circularite -- recenterAndFit a besoin de la geometrie aplatie, la subdivision
// aurait besoin du facteur d'echelle.
//
// Le parcours applique les MEMES filtres que la boucle principale, pour que
// l'estimation porte sur l'encre reellement produite. L'ecart residuel ne joue
// que sur la densite de tessellation, jamais sur la position d'un sommet : cette
// valeur ne sert qu'a convertir une tolerance et une largeur.
//
// Renvoie 0 quand il n'y a rien a mesurer.
float controlHullLargestExtent(const NSVGimage* image, const SvgExtrudeOptions& opt)
{
    bool any = false;
    float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;

    for (const NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        if (!opt.ignoreShapeId.empty() && opt.ignoreShapeId == shape->id)
            continue;
        if (!isVisible(shape))
            continue;

        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);
        if (!hasFill && !(hasStroke && opt.strokeToVolume)) continue;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
            for (int i = 0; i < path->npts; ++i)
            {
                const float x = path->pts[i*2 + 0];
                const float y = path->pts[i*2 + 1];
                if (!any) { minX = maxX = x; minY = maxY = y; any = true; continue; }
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
    }

    if (!any) return 0.f;
    return std::max(maxX - minX, maxY - minY);
}

// ============================================================================
//  Peinture : lecture des champs nanosvg
// ============================================================================

float clamp01(float v)
{
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

// Couleur d'un NSVGpaint, a la convention nanosvg (r | g<<8 | b<<16 | a<<24).
//
// Un degrade n'a pas de couleur unique : on rend la moyenne de ses stops,
// ponderee par la portion de rampe que chacun gouverne -- soit la moyenne exacte
// de la rampe lineaire par morceaux sur [0,1], extremites prolongees par leur
// constante. `approximated` passe alors a vrai ; il n'est jamais remis a faux,
// pour s'accumuler sur les deux peintures d'une meme forme.
unsigned int paintColor(const NSVGpaint& paint, bool& approximated)
{
    if (paint.type == NSVG_PAINT_COLOR)
        return paint.color;
    if (paint.type != NSVG_PAINT_LINEAR_GRADIENT && paint.type != NSVG_PAINT_RADIAL_GRADIENT)
        return 0u;

    const NSVGgradient* grad = paint.gradient;
    if (!grad || grad->nstops <= 0) return 0u;

    approximated = true;
    if (grad->nstops == 1) return grad->stops[0].color;

    double acc[4] = { 0.0, 0.0, 0.0, 0.0 };
    double wsum = 0.0;
    const auto add = [&acc, &wsum](unsigned int c, double w)
    {
        if (w <= 0.0) return;
        for (int k = 0; k < 4; ++k)
            acc[k] += w * (double)((c >> (8 * k)) & 0xFFu);
        wsum += w;
    };

    const int n = grad->nstops;
    add(grad->stops[0].color, (double)clamp01(grad->stops[0].offset));
    for (int i = 0; i + 1 < n; ++i)
    {
        const double o0 = (double)clamp01(grad->stops[i].offset);
        const double o1 = (double)clamp01(grad->stops[i + 1].offset);
        // Un segment de rampe vaut la moyenne de ses deux extremites : chacune
        // pese la moitie de sa longueur.
        const double half = 0.5 * (o1 - o0);
        add(grad->stops[i].color,     half);
        add(grad->stops[i + 1].color, half);
    }
    add(grad->stops[n - 1].color, 1.0 - (double)clamp01(grad->stops[n - 1].offset));

    // Des offsets degeneres (tous egaux, ou non croissants) donnent un poids
    // total nul : la moyenne simple reste definie, la ponderee non.
    if (wsum <= 0.0)
    {
        for (int k = 0; k < 4; ++k) acc[k] = 0.0;
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < 4; ++k)
                acc[k] += (double)((grad->stops[i].color >> (8 * k)) & 0xFFu);
        wsum = (double)n;
    }

    unsigned int out = 0u;
    for (int k = 0; k < 4; ++k)
    {
        double v = acc[k] / wsum + 0.5;
        if (v < 0.0)   v = 0.0;
        if (v > 255.0) v = 255.0;
        out |= ((unsigned int)v) << (8 * k);
    }
    return out;
}

// SVG distingue deux opacites, et nanosvg n'en replie qu'une : `fill-opacity` et
// `stroke-opacity` sont deja dans l'alpha de la couleur (nanosvg.h:1006-1025),
// l'opacite du GROUPE reste dans NSVGshape::opacity et les multiplie.
unsigned int withGroupOpacity(unsigned int rgba, float opacity)
{
    const double a = (double)((rgba >> 24) & 0xFFu) * (double)clamp01(opacity);
    return (rgba & 0x00FFFFFFu) | (((unsigned int)(a + 0.5)) << 24);
}

SvgShapePaint readPaint(const NSVGshape* shape, unsigned int rank, bool hasFill)
{
    SvgShapePaint paint;
    bool approximated = false;
    paint.fillRGBA   = withGroupOpacity(paintColor(shape->fill,   approximated), shape->opacity);
    paint.strokeRGBA = withGroupOpacity(paintColor(shape->stroke, approximated), shape->opacity);
    paint.hasFill    = hasFill;
    paint.isGradient = approximated;
    paint.rank       = rank;
    paint.id         = shape->id;
    return paint;
}

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

// stroke_contours.h parle en std::array<float,2> et sert aussi
// parameterized_shapes.cpp : on convertit de part et d'autre de l'appel.

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

// Un lot de contours aplatis partageant une regle de remplissage et une
// peinture : l'etat intermediaire entre la lecture du document et la resolution
// Clipper2.
struct ShapeLot
{
    std::vector<std::vector<Vector2f>> contours;
    bool          evenOdd = false;
    SvgShapePaint paint;
};

} // namespace

// Resout la regle de remplissage d'UNE forme et rend sa region. La regle
// EvenOdd/NonZero est une propriete de la forme SOURCE : la resoudre ici permet
// aux etages aval de ne porter qu'une liste PLATE de contours. Clipper2 oriente
// exterieurs et trous en sens opposes, ce que NonZero traite correctement.
static std::vector<ExtrudeContour> resolveShape(const std::vector<ExtrudeContour>& in,
                                                bool evenOdd)
{
    using namespace Clipper2Lib;

    PathsD subjects;
    subjects.reserve(in.size());
    for (const ExtrudeContour& c : in)
    {
        PathD p;
        p.reserve(c.pts.size());
        for (const Vector2f& q : c.pts)
            p.emplace_back((double)q.x, (double)q.y);
        subjects.push_back(std::move(p));
    }

    // precision 6 : deux decimales aplatiraient les details d'une forme d'une
    // unite de cote.
    const PathsD merged = Union(subjects, evenOdd ? FillRule::EvenOdd : FillRule::NonZero, 6);

    std::vector<ExtrudeContour> out;
    out.reserve(merged.size());
    for (const PathD& p : merged)
    {
        if (p.size() < 3) continue;
        ExtrudeContour c;
        c.pts.reserve(p.size());
        for (const PointD& q : p)
            c.pts.emplace_back((float)q.x, (float)q.y);
        out.push_back(std::move(c));
    }
    return out;
}

bool svg_to_shape_groups(const std::string& filename, const SvgExtrudeOptions& opt,
                         std::vector<SvgShapeGroup>& out)
{
    out.clear();
    NSVGimage* image = nsvgParseFromFile(filename.c_str(), "px", 96.0f);
    if (!image)
    {
        std::fprintf(stderr, "import_svg: failed to parse %s\n", filename.c_str());
        return false;
    }

    // `flattenTol` et `minStrokeWorldWidth` s'expriment dans les unites du
    // MAILLAGE PRODUIT (cf. import_svg.h), alors que la subdivision travaille
    // sur les coordonnees du document : on les y ramene ici, via l'emprise
    // ESTIMEE sur les points de controle.
    //
    // Sans `centerAndFit`, la sortie EST dans les unites du document : il n'y a
    // rien a convertir.
    float flattenTolSrc = opt.flattenTol;
    float minStrokeSrc  = opt.minStrokeWorldWidth;
    if (opt.centerAndFit)
    {
        const float largest = controlHullLargestExtent(image, opt);
        if (largest > 1e-9f)
        {
            flattenTolSrc = opt.flattenTol * largest;
            minStrokeSrc  = opt.minStrokeWorldWidth * largest;
        }
    }

    // Un lot par region a produire. Le decoupage est fait ici parce que la regle
    // de remplissage, comme la peinture, est une propriete de la forme SOURCE
    // que la liste de contours ne porte pas.
    std::vector<ShapeLot> lots;

    unsigned int rank = 0u;
    for (NSVGshape* shape = image->shapes; shape; shape = shape->next, ++rank)
    {
        // Decor de mise en page : ecarte avant tout traitement (cf.
        // SvgExtrudeOptions::ignoreShapeId).
        if (!opt.ignoreShapeId.empty() && opt.ignoreShapeId == shape->id)
            continue;

        // Forme masquee par le document (cf. isVisible).
        if (!isVisible(shape))
            continue;

        const bool hasFill   = (shape->fill.type   != NSVG_PAINT_NONE);
        const bool hasStroke = (shape->stroke.type != NSVG_PAINT_NONE);

        // Ni remplissage ni trait : rien a produire.
        if (!hasFill && !(hasStroke && opt.strokeToVolume)) continue;

        // La decision se prend PAR CHEMIN, sur NSVGpath::closed, et non par forme
        // sur l'attribut `fill` : un meme <g> peut melanger des contours fermes
        // et des polylignes ouvertes. `<polygon>` et `<path ... Z>` donnent
        // closed = 1, `<polyline>` et un `<path>` sans Z donnent 0
        // (nanosvg.h:2826-2833).
        //
        //   ferme + fill -> frontiere de SURFACE, tessellation (>= 3 points)
        //   ferme, sans  -> ANNEAU : le trait des deux bords de la boucle,
        //                   arete de fermeture comprise (>= 3 points)
        //   ouvert       -> LIGNE, epaissie de son stroke-width (>= 2 points)
        std::vector<std::vector<Vector2f>> filled;
        std::vector<std::vector<std::array<float, 2>>> openPaths;

        // Contours fermes dont le TRAIT est a produire en anneau. Un chemin
        // ferme et rempli alimente les deux : sa surface et son trait sont deux
        // regions distinctes de la meme forme.
        std::vector<std::vector<std::array<float, 2>>> strokeRings;

        // `strokeOnFilledShapes` ne gouverne que le trait d'une forme REMPLIE.
        // Celui d'une forme SANS remplissage est la seule geometrie qu'elle
        // produise, et releve de `strokeToVolume` seul.
        const bool ringsWanted = opt.strokeOnFilledShapes && hasStroke;

        for (const NSVGpath* path = shape->paths; path; path = path->next)
        {
            auto pts = flattenPath(path, flattenTolSrc);
            const bool closed = (path->closed != 0);
            if (closed && hasFill)
            {
                if (pts.size() < 3) continue;
                if (ringsWanted) strokeRings.push_back(toArrays(pts));
                filled.push_back(std::move(pts));
            }
            else if (closed && opt.strokeToVolume && hasStroke && pts.size() >= 3)
            {
                // Un trace FERME donne un ANNEAU : ses deux bords, arete de
                // fermeture comprise. Le seuil de trois points est celui de
                // strokeClosedToContours : une boucle a deux points n'a pas
                // d'interieur, son trait est un ruban.
                strokeRings.push_back(toArrays(pts));
            }
            else if (opt.strokeToVolume && hasStroke)
            {
                if (pts.size() < 2) continue;
                openPaths.push_back(toArrays(pts));
            }
            else if (hasFill && pts.size() >= 3)
            {
                // Ouvert mais sans trait exploitable : nanosvg referme le trace
                // pour le remplir.
                filled.push_back(std::move(pts));
            }
        }

        // Le lot de REMPLISSAGE est pousse avant ceux du trait : c'est l'ordre
        // dans lequel les regions d'une meme forme sont rendues.
        if (!filled.empty())
        {
            ShapeLot lot;
            lot.contours = std::move(filled);
            lot.evenOdd  = (shape->fillRule == NSVG_FILLRULE_EVENODD);
            lot.paint    = readPaint(shape, rank, /*hasFill=*/true);
            lots.push_back(std::move(lot));
        }

        if (!strokeRings.empty() || !openPaths.empty())
        {
            float w = shape->strokeWidth * opt.strokeScale;
            if (!(w > 0.f)) w = opt.strokeWidthFallback * opt.strokeScale;
            // Plancher de largeur : il ELARGIT, il ne supprime pas (cf.
            // SvgExtrudeOptions::minStrokeWorldWidth).
            if (w < minStrokeSrc) w = minStrokeSrc;

            // strokeToContours ne connait pas nanosvg : la traduction des
            // conventions est a la charge de l'appelant.
            const StrokeJoin join = (shape->strokeLineJoin == NSVG_JOIN_MITER) ? StrokeJoin::Miter
                                  : (shape->strokeLineJoin == NSVG_JOIN_BEVEL) ? StrokeJoin::Bevel
                                                                               : StrokeJoin::Round;
            const StrokeCap  cap  = (shape->strokeLineCap == NSVG_CAP_BUTT)   ? StrokeCap::Butt
                                  : (shape->strokeLineCap == NSVG_CAP_SQUARE) ? StrokeCap::Square
                                                                              : StrokeCap::Round;

            const auto pushStrokeLot =
                [&lots, shape, rank](const std::vector<std::vector<std::array<float, 2>>>& regions)
            {
                if (regions.empty()) return;

                std::vector<std::vector<Vector2f>> converted;
                converted.reserve(regions.size());
                for (const auto& r : regions)
                    converted.push_back(fromArrays(r));

                ShapeLot lot;
                lot.contours = std::move(converted);
                // Les contours sortis de Clipper2 portent DEJA leur orientation,
                // enveloppes et trous en sens opposes : c'est la convention de
                // NonZero, il n'y a rien a reorienter.
                lot.evenOdd = false;
                lot.paint   = readPaint(shape, rank, /*hasFill=*/false);
                lots.push_back(std::move(lot));
            };

            // L'anneau des contours fermes, puis le ruban des traces ouverts :
            // deux lots, un seul rang -- c'est le meme trait de la meme forme.
            pushStrokeLot(strokeClosedToContours(strokeRings, w, join));
            pushStrokeLot(strokeToContours(openPaths, w, join, cap));
        }
    }

    nsvgDelete(image);

    if (lots.empty())
    {
        std::fprintf(stderr, "import_svg: %s has no fillable shapes\n", filename.c_str());
        return false;
    }

    if (opt.centerAndFit)
    {
        // Fit over ALL shapes at once so their relative placement survives.
        std::vector<std::vector<Vector2f>> flat;
        for (auto& lot : lots)
            for (auto& pts : lot.contours)
                flat.push_back(std::move(pts));
        recenterAndFit(flat);
        size_t k = 0;
        for (auto& lot : lots)
            for (auto& pts : lot.contours)
                pts = std::move(flat[k++]);
    }

    out.reserve(lots.size());
    for (ShapeLot& lot : lots)
    {
        std::vector<ExtrudeContour> contours;
        contours.reserve(lot.contours.size());
        for (const auto& pts : lot.contours)
        {
            ExtrudeContour c;
            c.pts.reserve(pts.size());
            for (const Vector2f& p : pts)
                c.pts.emplace_back(p.x, opt.invertY ? -p.y : p.y);
            contours.push_back(std::move(c));
        }

        // Dernier endroit ou la regle de remplissage de la forme est connue : la
        // region rendue ne la porte pas.
        SvgShapeGroup group;
        group.contours = resolveShape(contours, lot.evenOdd);
        // Un lot qui ne resout sur aucune region ne rend pas de groupe.
        if (group.contours.empty()) continue;
        group.paint = std::move(lot.paint);
        out.push_back(std::move(group));
    }

    if (out.empty())
    {
        std::fprintf(stderr, "import_svg: %s a tessele dans le vide\n", filename.c_str());
        return false;
    }
    return true;
}

bool svg_to_contours(const std::string& filename, const SvgExtrudeOptions& opt,
                     std::vector<ExtrudeContour>& out)
{
    out.clear();

    std::vector<SvgShapeGroup> groups;
    if (!svg_to_shape_groups(filename, opt, groups))
        return false;

    for (const SvgShapeGroup& g : groups)
        out.insert(out.end(), g.contours.begin(), g.contours.end());
    return !out.empty();
}

// Aire nette d'une region orientee par Clipper2 : enveloppes positives, trous
// negatifs, la somme signee est donc l'aire reellement couverte. Accumulee en
// double, ces aires etant comparees entre elles.
static double netContourArea(const std::vector<ExtrudeContour>& region)
{
    double sum = 0.0;
    for (const ExtrudeContour& c : region)
        sum += (double)contourSignedArea(c.pts);
    return std::fabs(sum);
}

namespace {

// Emprise d'un groupe. `valid` est faux pour un groupe sans point : il ne
// couvre rien et n'est couvert par rien, et lui preter une boite par defaut le
// ferait passer pour un partenaire de tout le monde.
struct GroupBox
{
    float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
    bool  valid = false;
};

// Contact simple compris : deux boites qui se touchent par un bord comptent
// comme secantes. Le predicat doit rester un MAJORANT du recouvrement reel : un
// partenaire de trop coute du temps, un partenaire manquant rend une region
// trop grande, en silence.
bool boxesOverlap(const GroupBox& a, const GroupBox& b)
{
    if (!a.valid || !b.valid) return false;
    return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1;
}

} // namespace

void svg_subtract_overlaps(std::vector<SvgShapeGroup>& groups, SvgOverlapStats* stats)
{
    const auto tStart = std::chrono::steady_clock::now();

    const size_t n = groups.size();

    // Les formes d'origine sont mises de cote : chacune sert de soustracteur a
    // celles du dessous, longtemps apres que sa propre region a remplace son
    // contenu dans `groups`.
    //
    // L'union de chaque forme avec rien la NORMALISE, et cette passe est
    // l'hypothese qui rend la suite exacte : sans elle, concatener plusieurs
    // formes en un seul clip laisserait leurs nombres de tours s'annuler sous
    // NonZero, et un trou du dessus repercerait la region du dessous (cf.
    // import_svg.h).
    std::vector<std::vector<ExtrudeContour>> shapes(n);
    std::vector<GroupBox> boxes(n);
    for (size_t k = 0; k < n; ++k)
    {
        shapes[k] = unionContours(groups[k].contours, {});
        GroupBox& b = boxes[k];
        b.valid = contoursBBox(shapes[k], b.x0, b.y0, b.x1, b.y1);
    }

    unsigned int fullyCovered = 0u;
    unsigned int partiallyCovered = 0u;
    unsigned int overlapPairs = 0u;
    double inputArea = 0.0;
    double keptArea  = 0.0;

    // Contours des formes sus-jacentes qui recouvrent la forme courante. Le
    // balayage va du dernier groupe au premier : l'ordre des groupes est celui
    // du document, donc du peintre, et le dernier est au-dessus.
    std::vector<ExtrudeContour> covering;

    for (size_t k = n; k-- > 0; )
    {
        const double shapeArea = netContourArea(shapes[k]);
        inputArea += shapeArea;

        covering.clear();
        for (size_t j = k + 1; j < n; ++j)
        {
            if (!boxesOverlap(boxes[k], boxes[j])) continue;
            ++overlapPairs;
            covering.insert(covering.end(), shapes[j].begin(), shapes[j].end());
        }

        groups[k].contours = differenceContours(shapes[k], covering);

        if (groups[k].contours.empty())
        {
            // Une forme d'aire nulle n'a rien perdu : la compter ici ferait
            // passer une degenerescence pour un effet du recouvrement.
            if (shapeArea > 0.0) ++fullyCovered;
        }
        else
        {
            const double kept = netContourArea(groups[k].contours);
            keptArea += kept;
            // Seuil RELATIF : la difference realigne ses sommets sur la grille de
            // Clipper2, donc l'egalite exacte n'est pas un critere (cf.
            // SvgOverlapStats::partiallyCoveredGroups).
            if (shapeArea > 0.0 && kept < shapeArea * (1.0 - 1e-6))
                ++partiallyCovered;
        }
    }

    if (stats)
    {
        stats->fullyCoveredGroups = fullyCovered;
        stats->partiallyCoveredGroups = partiallyCovered;
        stats->inputArea          = inputArea;
        stats->keptArea           = keptArea;
        stats->removedArea        = inputArea - keptArea;
        stats->overlapPairs       = overlapPairs;
        stats->candidatePairs     = (unsigned int)(n * (n - 1) / 2);
        stats->sweepMs = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - tStart).count();
    }
}

// Couleur d'un groupe : celle de son remplissage, sauf pour une region issue
// d'un TRAIT quand le reglage demande la couleur du trait (cf.
// SvgExtrudeOptions::strokeUsesStrokeColor).
static unsigned int groupColor(const SvgShapePaint& paint, bool strokeUsesStrokeColor)
{
    if (!paint.hasFill && strokeUsesStrokeColor)
        return paint.strokeRGBA;
    return paint.fillRGBA;
}

// Materiau d'une couleur empaquetee a la convention nanosvg (r | g<<8 | b<<16 |
// a<<24). L'alpha est STOCKE : MaterialColor le porte, meme si aucun etage aval
// ne le transmet.
static MaterialColor* makeColorMaterial(unsigned int rgba, unsigned int index)
{
    auto* m = new MaterialColor((unsigned char)( rgba        & 0xFFu),
                                (unsigned char)((rgba >>  8) & 0xFFu),
                                (unsigned char)((rgba >> 16) & 0xFFu),
                                (unsigned char)((rgba >> 24) & 0xFFu));
    char name[32];
    std::snprintf(name, sizeof(name), "svg_color_%02u", index);
    m->SetName(name);
    return m;
}

Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt,
                          SvgExtrudeMapping* mapping)
{
    if (mapping)
    {
        mapping->materialOfGroup.clear();
        mapping->facesOfGroup.clear();
        mapping->gradientGroups = 0u;
        mapping->overlap = SvgOverlapStats();
    }

    std::vector<SvgShapeGroup> groups;
    if (!svg_to_shape_groups(filename, opt, groups))
        return nullptr;

    // La marqueterie precede la palette : elle peut vider un groupe, et un
    // groupe vide ne doit pas faire entrer sa couleur dans la table.
    if (opt.overlapPolicy == SvgExtrudeOptions::OverlapPolicy::Subtract)
        svg_subtract_overlaps(groups, mapping ? &mapping->overlap : nullptr);

    if (mapping)
        for (const SvgShapeGroup& g : groups)
            if (g.paint.isGradient)
                ++mapping->gradientGroups;

    ExtrudeAppendOptions ao;
    ao.zBottom = 0.0f;
    ao.zTop    = opt.height;
    // NonZero et normalizeOrientation faux : les contours viennent de Clipper2,
    // qui a deja oriente exterieurs et trous en sens opposes. Les reorienter
    // d'apres l'aire signee reboucherait les contre-formes.
    ao.winding = ExtrudeWinding::NonZero;
    ao.normalizeOrientation = false;

    ExtrudedMeshBuilder builder;

    // Couleur -> identifiant de materiau, et la palette dans l'ordre des
    // identifiants. Les identifiants sont attribues dans l'ordre de PREMIERE
    // rencontre, si bien que l'ordre d'insertion dans le Mesh est celui de ce
    // vecteur.
    std::map<unsigned int, unsigned int> materialOfColor;
    std::vector<unsigned int>            palette;

    // Un Append par groupe des que la palette OU la marqueterie le demande.
    //
    // La palette l'exige parce qu'un Append porte UN materiau. La marqueterie
    // l'exige independamment de la couleur : elle rend les regions ADJACENTES,
    // et un polygone glutess unique les refondrait en une seule -- leurs aretes
    // de frontiere communes cesseraient d'etre des aretes de triangle de capot,
    // et les parois qui s'appuient dessus seraient abandonnees en silence
    // (« tessellation gap », extrude_contours.cpp), laissant un solide ouvert
    // au volume signe sans valeur. C'est la condition posee par
    // contour_ops.h:104.
    const bool perGroupAppend =
        opt.perShapeMaterials
        || opt.overlapPolicy == SvgExtrudeOptions::OverlapPolicy::Subtract;

    if (perGroupAppend)
    {
        for (const SvgShapeGroup& g : groups)
        {
            unsigned int materialId = (unsigned int)MATERIAL_NONE;
            bool isNewColor = false;
            unsigned int rgba = 0u;

            if (opt.perShapeMaterials)
            {
                rgba = groupColor(g.paint, opt.strokeUsesStrokeColor);
                const auto it = materialOfColor.find(rgba);
                isNewColor = (it == materialOfColor.end());
                if (isNewColor)
                {
                    materialId = (unsigned int)palette.size();
                    materialOfColor.emplace(rgba, materialId);
                    palette.push_back(rgba);
                }
                else
                {
                    materialId = it->second;
                }
            }

            ao.materialId = materialId;
            unsigned int firstFace = 0u, faceCount = 0u;
            const bool emitted = builder.Append(g.contours, ao, &firstFace, &faceCount);

            // Un materiau sans face n'entre pas dans la palette : il figurerait
            // dans la table du Mesh sans apparaitre dans aucun MaterialRange.
            if (!emitted && isNewColor)
            {
                materialOfColor.erase(rgba);
                palette.pop_back();
            }

            if (mapping)
            {
                mapping->materialOfGroup.push_back(emitted ? materialId
                                                           : (unsigned int)MATERIAL_NONE);
                mapping->facesOfGroup.emplace_back(firstFace, faceCount);
            }
        }
    }
    else
    {
        // Chemin monolithique : UNE tessellation pour tout le document, sur la
        // concatenation des groupes -- exactement ce que svg_to_contours rend. La
        // regle NONZERO joue donc entre formes, et aucune face ne se rattache a
        // un groupe, d'ou une correspondance de faces vide (cf.
        // SvgExtrudeMapping).
        std::vector<ExtrudeContour> contours;
        for (const SvgShapeGroup& g : groups)
            contours.insert(contours.end(), g.contours.begin(), g.contours.end());

        builder.Append(contours, ao);

        if (mapping)
            mapping->materialOfGroup.assign(groups.size(), (unsigned int)MATERIAL_NONE);
    }

    if (builder.Empty())
    {
        std::fprintf(stderr, "import_svg: %s a tessele dans le vide\n", filename.c_str());
        return nullptr;
    }

    Mesh* mesh = builder.Build();
    if (!mesh)
        return nullptr;

    for (size_t i = 0; i < palette.size(); ++i)
        mesh->Material_Add(makeColorMaterial(palette[i], (unsigned int)i));

    return mesh;
}
