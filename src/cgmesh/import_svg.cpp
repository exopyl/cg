#include "import_svg.h"

#include "mesh.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

extern "C" {
#include "../../extern/glutess/glutess.h"
}

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
//  Tessellation (glutess) — flat-region triangle list
// ============================================================================
//
// We feed all contours of a shape into a single gluTess polygon so the
// NONZERO winding rule applies (SVG holes are inner contours of opposite
// orientation). Output is a flat list of 2D vertex indices, three per
// emitted triangle, referencing a per-shape index table built below.

struct TessOut
{
    // Pool of unique 2D positions for the shape (filled as we feed contours).
    std::vector<std::array<float, 2>> verts;

    // Indices of emitted triangles, three per triangle. Indices reference
    // `verts`.
    std::vector<unsigned int> tris;

    // Per-triangle batching helpers (the GL_TRIANGLES edge-flag forces
    // glutess into triangle-list mode, so this is straightforward).
    unsigned int triBuf[3] = { 0, 0, 0 };
    int triCount = 0;

    bool combineHit = false;
};

void GLAPIENTRY svgTessBeginCB(GLenum /*type*/, void* userData)
{
    static_cast<TessOut*>(userData)->triCount = 0;
}

void GLAPIENTRY svgTessVertexCB(void* vertexData, void* userData)
{
    auto* ctx = static_cast<TessOut*>(userData);
    const unsigned int idx = (unsigned int)(uintptr_t)vertexData;
    ctx->triBuf[ctx->triCount++] = idx;
    if (ctx->triCount == 3)
    {
        const bool poisoned = (ctx->triBuf[0] == ~0u
                            || ctx->triBuf[1] == ~0u
                            || ctx->triBuf[2] == ~0u);
        if (!poisoned)
        {
            ctx->tris.push_back(ctx->triBuf[0]);
            ctx->tris.push_back(ctx->triBuf[1]);
            ctx->tris.push_back(ctx->triBuf[2]);
        }
        ctx->triCount = 0;
    }
}

void GLAPIENTRY svgTessEndCB(void* /*userData*/) {}
void GLAPIENTRY svgTessEdgeFlagCB(GLboolean /*flag*/, void* /*userData*/) {}

void GLAPIENTRY svgTessCombineCB(GLdouble newCoords[3], void* /*data*/[4],
                                 GLfloat /*weight*/[4], void** outData,
                                 void* userData)
{
    // For SVG paths with edge intersections (rare but legal), glutess
    // wants to introduce a new vertex. Append it to our pool so the
    // tessellation references a valid slot instead of a poison sentinel.
    auto* ctx = static_cast<TessOut*>(userData);
    ctx->combineHit = true;
    ctx->verts.push_back({ (float)newCoords[0], (float)newCoords[1] });
    *outData = (void*)(uintptr_t)(ctx->verts.size() - 1);
}

void GLAPIENTRY svgTessErrorCB(GLenum errnum, void* /*userData*/)
{
    std::fprintf(stderr, "import_svg: glutess error 0x%x\n", (unsigned)errnum);
}

// Tessellate all contours of `shape` into `out`. Each contour's points are
// added to out.verts, contour edges are remembered in `outlineEdges` (pairs
// of vertex indices) so we can build extrusion walls afterwards.
void tessellateShape(const NSVGshape* shape, float flattenTol,
                     TessOut& out,
                     std::vector<std::pair<unsigned int, unsigned int>>& outlineEdges)
{
    GLUtesselator* tess = gluNewTess();
    if (!tess) return;
    gluTessCallback(tess, GLU_TESS_BEGIN_DATA,     (_GLUfuncptr)svgTessBeginCB);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA,    (_GLUfuncptr)svgTessVertexCB);
    gluTessCallback(tess, GLU_TESS_END_DATA,       (_GLUfuncptr)svgTessEndCB);
    gluTessCallback(tess, GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr)svgTessEdgeFlagCB);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA,   (_GLUfuncptr)svgTessCombineCB);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA,     (_GLUfuncptr)svgTessErrorCB);

    if (shape->fillRule == NSVG_FILLRULE_EVENODD)
        gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    else
        gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

    // Stable storage for the GLdoubles handed to gluTessVertex (glutess
    // keeps pointers across the polygon).
    std::vector<std::array<GLdouble, 3>> coordPool;
    // Reserve a generous upper bound; we'll only push what we need.
    {
        size_t expected = 0;
        for (const NSVGpath* p = shape->paths; p; p = p->next)
            expected += (size_t)std::max(0, p->npts);
        coordPool.reserve(expected * 4);
    }

    gluTessBeginPolygon(tess, &out);
    for (const NSVGpath* path = shape->paths; path; path = path->next)
    {
        auto pts = flattenPath(path, flattenTol);
        if (pts.size() < 3) continue;

        const unsigned int contourStart = (unsigned int)out.verts.size();
        out.verts.insert(out.verts.end(), pts.begin(), pts.end());

        gluTessBeginContour(tess);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            coordPool.push_back({ (GLdouble)pts[i][0], (GLdouble)pts[i][1], 0.0 });
            gluTessVertex(tess, coordPool.back().data(),
                          (void*)(uintptr_t)(contourStart + (unsigned int)i));
        }
        gluTessEndContour(tess);

        // Record outline edges for wall construction.
        const unsigned int n = (unsigned int)pts.size();
        for (unsigned int i = 0; i < n; ++i)
            outlineEdges.emplace_back(contourStart + i,
                                       contourStart + ((i + 1) % n));
    }
    gluTessEndPolygon(tess);

    gluDeleteTess(tess);
}

// ============================================================================
//  Mesh assembly: bottom cap + top cap + side walls
// ============================================================================

Mesh* buildExtrudedMesh(const std::vector<std::array<float, 2>>& verts2D,
                       const std::vector<unsigned int>& bottomTris,
                       const std::vector<std::pair<unsigned int, unsigned int>>& outlineEdges,
                       float height,
                       bool invertY)
{
    if (verts2D.empty() || bottomTris.empty()) return nullptr;

    auto* m = new Mesh();

    const unsigned int n = (unsigned int)verts2D.size();
    const unsigned int nVertsTotal = 2 * n;
    const unsigned int nFacesTotal =
        (unsigned int)(bottomTris.size() / 3) +     // bottom cap
        (unsigned int)(bottomTris.size() / 3) +     // top cap
        (unsigned int)(outlineEdges.size() * 2);    // walls (2 tris per edge)

    // Vertex buffer: first n verts at z=0 (bottom), next n at z=height (top).
    std::vector<float> verts;
    verts.reserve(3 * nVertsTotal);
    for (unsigned int i = 0; i < n; ++i)
    {
        const float x = verts2D[i][0];
        const float y = invertY ? -verts2D[i][1] : verts2D[i][1];
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(0.0f);
    }
    for (unsigned int i = 0; i < n; ++i)
    {
        const float x = verts2D[i][0];
        const float y = invertY ? -verts2D[i][1] : verts2D[i][1];
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(height);
    }
    m->SetVertices(nVertsTotal, verts.data());

    // Faces
    m->m_nFaces = nFacesTotal;
    m->m_pFaces = new Face*[nFacesTotal];
    m->m_pFaceNormals.assign(3u * nFacesTotal, 0.0f);

    unsigned int fi = 0;

    // Bottom cap — normal points down. We use the original triangle winding
    // for the TOP cap (above) and reverse it for the bottom; that way both
    // caps end up facing outward when the SVG winding is CCW (default for
    // filled shapes flipped by invertY).
    //
    // Note: when invertY is true, the Y flip reverses orientation, so the
    // *original* glutess CCW triangles become CW in our world. We compensate
    // accordingly.
    const bool flipped = invertY;
    for (size_t t = 0; t + 2 < bottomTris.size(); t += 3)
    {
        Face* f = new Face();
        f->SetNVertices(3);
        if (flipped)
        {
            f->SetVertex(0, bottomTris[t + 0]);
            f->SetVertex(1, bottomTris[t + 2]);
            f->SetVertex(2, bottomTris[t + 1]);
        }
        else
        {
            f->SetVertex(0, bottomTris[t + 0]);
            f->SetVertex(1, bottomTris[t + 1]);
            f->SetVertex(2, bottomTris[t + 2]);
        }
        m->m_pFaces[fi++] = f;
    }

    // Top cap — opposite winding to face +Z.
    for (size_t t = 0; t + 2 < bottomTris.size(); t += 3)
    {
        Face* f = new Face();
        f->SetNVertices(3);
        const unsigned int a = bottomTris[t + 0] + n;
        const unsigned int b = bottomTris[t + 1] + n;
        const unsigned int c = bottomTris[t + 2] + n;
        if (flipped)
        {
            f->SetVertex(0, a);
            f->SetVertex(1, b);
            f->SetVertex(2, c);
        }
        else
        {
            f->SetVertex(0, a);
            f->SetVertex(1, c);
            f->SetVertex(2, b);
        }
        m->m_pFaces[fi++] = f;
    }

    // Side walls: each contour edge (a,b) becomes two triangles connecting
    // the bottom edge to the top edge.
    //   bottom:  b_a, b_b
    //   top:     t_a = b_a + n, t_b = b_b + n
    //   tri1: (b_a, b_b, t_b)
    //   tri2: (b_a, t_b, t_a)
    // Winding flipped when invertY for outward normals.
    for (auto [a, b] : outlineEdges)
    {
        const unsigned int t_a = a + n;
        const unsigned int t_b = b + n;

        Face* f1 = new Face();
        f1->SetNVertices(3);
        Face* f2 = new Face();
        f2->SetNVertices(3);
        if (flipped)
        {
            f1->SetVertex(0, a); f1->SetVertex(1, t_b); f1->SetVertex(2, b);
            f2->SetVertex(0, a); f2->SetVertex(1, t_a); f2->SetVertex(2, t_b);
        }
        else
        {
            f1->SetVertex(0, a); f1->SetVertex(1, b);   f1->SetVertex(2, t_b);
            f2->SetVertex(0, a); f2->SetVertex(1, t_b); f2->SetVertex(2, t_a);
        }
        m->m_pFaces[fi++] = f1;
        m->m_pFaces[fi++] = f2;
    }

    m->ComputeNormals();
    m->IncrementRevision();
    return m;
}

// ============================================================================
//  Public entry point
// ============================================================================

void recenterAndFit(std::vector<std::array<float, 2>>& verts, float height)
{
    if (verts.empty()) return;
    float minX = verts[0][0], minY = verts[0][1];
    float maxX = verts[0][0], maxY = verts[0][1];
    for (const auto& v : verts)
    {
        minX = std::min(minX, v[0]); maxX = std::max(maxX, v[0]);
        minY = std::min(minY, v[1]); maxY = std::max(maxY, v[1]);
    }
    const float cx = 0.5f * (minX + maxX);
    const float cy = 0.5f * (minY + maxY);
    const float w  = maxX - minX;
    const float h  = maxY - minY;
    const float largestXY = std::max(w, h);
    if (largestXY < 1e-9f) return;
    const float scale = 1.0f / largestXY;
    for (auto& v : verts)
    {
        v[0] = (v[0] - cx) * scale;
        v[1] = (v[1] - cy) * scale;
    }
    (void)height; // height stays in caller-supplied units relative to fit XY
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

    // Aggregate all fillable shapes into one big tessellation pool. Each
    // shape contributes its own contours (and outline edges); we keep the
    // index spaces separate by offsetting on append.
    std::vector<std::array<float, 2>> allVerts;
    std::vector<unsigned int> allTris;
    std::vector<std::pair<unsigned int, unsigned int>> allEdges;

    for (NSVGshape* shape = image->shapes; shape; shape = shape->next)
    {
        if (shape->fill.type == NSVG_PAINT_NONE) continue;

        TessOut out;
        std::vector<std::pair<unsigned int, unsigned int>> shapeEdges;
        tessellateShape(shape, opt.flattenTol, out, shapeEdges);

        if (out.tris.empty()) continue;

        const unsigned int base = (unsigned int)allVerts.size();
        allVerts.insert(allVerts.end(), out.verts.begin(), out.verts.end());
        for (unsigned int t : out.tris) allTris.push_back(base + t);
        for (auto [a, b] : shapeEdges) allEdges.emplace_back(base + a, base + b);
    }

    nsvgDelete(image);

    if (allVerts.empty() || allTris.empty())
    {
        std::fprintf(stderr, "import_svg: %s has no fillable shapes\n", filename.c_str());
        return nullptr;
    }

    if (opt.centerAndFit)
        recenterAndFit(allVerts, opt.height);

    return buildExtrudedMesh(allVerts, allTris, allEdges, opt.height, opt.invertY);
}
