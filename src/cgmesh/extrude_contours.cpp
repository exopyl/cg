#include "extrude_contours.h"

#include "mesh.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
#include "../../extern/glutess/glutess.h"
}

namespace {

// ============================================================================
//  Tessellation (glutess) — flat-region triangle list
// ============================================================================
//
// All contours of one region go into a SINGLE gluTess polygon so the winding
// rule applies across them (that is how holes get subtracted). Output is a flat
// list of vertex indices, three per emitted triangle, referencing the per-region
// index pool built below.

struct TessOut
{
	// Pool of 2D positions for the region (filled as we feed contours, plus any
	// vertex glutess introduces via COMBINE).
	std::vector<std::array<float, 2>> verts;

	// Indices of emitted triangles, three per triangle, into `verts`.
	std::vector<unsigned int> tris;

	// Per-triangle batching helpers (the GL_TRIANGLES edge-flag forces glutess
	// into triangle-list mode, so this is straightforward).
	unsigned int triBuf[3] = { 0, 0, 0 };
	int triCount = 0;

	bool combineHit = false;
};

void GLAPIENTRY tessBeginCB(GLenum /*type*/, void* userData)
{
	static_cast<TessOut*>(userData)->triCount = 0;
}

void GLAPIENTRY tessVertexCB(void* vertexData, void* userData)
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

void GLAPIENTRY tessEndCB(void* /*userData*/) {}
void GLAPIENTRY tessEdgeFlagCB(GLboolean /*flag*/, void* /*userData*/) {}

void GLAPIENTRY tessCombineCB(GLdouble newCoords[3], void* /*data*/[4],
                              GLfloat /*weight*/[4], void** outData,
                              void* userData)
{
	// For contours with edge intersections (rare but legal in SVG), glutess
	// wants to introduce a new vertex. Append it to our pool so the
	// tessellation references a valid slot instead of a poison sentinel.
	auto* ctx = static_cast<TessOut*>(userData);
	ctx->combineHit = true;
	ctx->verts.push_back({ (float)newCoords[0], (float)newCoords[1] });
	*outData = (void*)(uintptr_t)(ctx->verts.size() - 1);
}

void GLAPIENTRY tessErrorCB(GLenum errnum, void* /*userData*/)
{
	std::fprintf(stderr, "extrude_contours: glutess error 0x%x\n", (unsigned)errnum);
}

// Signed area of a closed contour (shoelace). Positive = CCW in a Y-up frame.
double signedArea(const std::vector<Vector2f>& pts)
{
	double a = 0.;
	const size_t n = pts.size();
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2f& p = pts[i];
		const Vector2f& q = pts[(i + 1) % n];
		a += (double)p.x * (double)q.y - (double)q.x * (double)p.y;
	}
	return 0.5 * a;
}

// Retire les points consecutifs QUASI CONFONDUS, et referme proprement la boucle.
//
// Pourquoi c'est indispensable : glutess ECARTE un sommet redondant -- il
// n'apparait alors dans aucun triangle. Or les parois se construisent en
// retrouvant chaque arete de contour parmi les triangles de capot (voir
// ExtrudedMeshBuilder::Append) : un sommet ecarte fait donc perdre TROIS aretes
// d'un coup, celle qui entre, celle de longueur nulle, celle qui sort -- et donc
// trois parois, silencieusement.
//
// Mesure avant correction, sur un trace dense (L-systeme « Dragon curve ») : 190
// aretes sans paroi sur 1633, et sur ces 190, le nombre dont les deux extremites
// etaient utilisees par un triangle valait exactement ZERO. Autrement dit la
// totalite des pertes venait de sommets ecartes. Les paires fautives etaient
// separees de 0 ou de 2,98e-08 -- soit un ULP de float a une magnitude de 0,3.
//
// Tolerance RELATIVE et non absolue : ces points sont IDENTIQUES en sortie de
// Clipper2, c'est la mise a l'echelle de recenterAndFit qui les separe d'un bit.
// Une comparaison exacte (meme sur les bits) les rate donc par construction.
// 1e-6 de la magnitude laisse une marge de ~30 ULP au-dessus du bruit, et reste
// trois ordres de grandeur sous la plus courte arete legitime observee (0,005).
std::vector<Vector2f> dropDuplicatePoints(const std::vector<Vector2f>& pts)
{
	float mag = 1.f;
	for (const Vector2f& p : pts)
		mag = std::max(mag, std::max(std::fabs(p.x), std::fabs(p.y)));
	const float eps = 1e-6f * mag;
	auto same = [eps](const Vector2f& a, const Vector2f& b)
	{
		return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
	};

	std::vector<Vector2f> out;
	out.reserve(pts.size());
	for (const Vector2f& p : pts)
		if (out.empty() || !same(out.back(), p))
			out.push_back(p);

	// L'arete de fermeture est implicite : un dernier point confondu avec le
	// premier produirait la meme degenerescence, en fin de boucle.
	while (out.size() >= 2 && same(out.front(), out.back()))
		out.pop_back();

	return out;
}

// Tessellate all contours into `out`, remembering each contour's edges in
// `outlineEdges` (pairs of vertex indices) so walls can be built afterwards.
void tessellateContours(const std::vector<ExtrudeContour>& contours,
                        bool normalizeOrientation,
                        ExtrudeWinding winding,
                        TessOut& out,
                        std::vector<std::pair<unsigned int, unsigned int>>& outlineEdges)
{
	GLUtesselator* tess = gluNewTess();
	if (!tess) return;
	gluTessCallback(tess, GLU_TESS_BEGIN_DATA,     (_GLUfuncptr)tessBeginCB);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA,    (_GLUfuncptr)tessVertexCB);
	gluTessCallback(tess, GLU_TESS_END_DATA,       (_GLUfuncptr)tessEndCB);
	gluTessCallback(tess, GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr)tessEdgeFlagCB);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA,   (_GLUfuncptr)tessCombineCB);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA,     (_GLUfuncptr)tessErrorCB);

	gluTessProperty(tess, GLU_TESS_WINDING_RULE,
	                (winding == ExtrudeWinding::EvenOdd) ? GLU_TESS_WINDING_ODD
	                                                     : GLU_TESS_WINDING_NONZERO);

	// Stable storage for the GLdoubles handed to gluTessVertex: glutess keeps the
	// pointers across the whole polygon, so coordPool must NOT reallocate while
	// we feed it. We push exactly one entry per contour point, so reserving the
	// total point count is an exact guarantee.
	std::vector<std::array<GLdouble, 3>> coordPool;
	{
		size_t expected = 0;
		for (const auto& c : contours) expected += c.pts.size();
		coordPool.reserve(expected);
	}

	gluTessBeginPolygon(tess, &out);
	for (const ExtrudeContour& contour : contours)
	{
		// Nettoyage AVANT tout le reste : un point redondant serait ecarte par
		// glutess et emporterait trois parois (cf. dropDuplicatePoints).
		const std::vector<Vector2f> pts = dropDuplicatePoints(contour.pts);
		const size_t n = pts.size();
		if (n < 3) continue;

		// Outer boundaries CCW, holes CW: the orientation a NONZERO fill needs
		// to subtract the holes. Only the ORDER in which we feed the points
		// changes; the point set is untouched.
		bool reverse = false;
		if (normalizeOrientation)
		{
			const bool ccw = signedArea(pts) > 0.;
			reverse = (ccw == contour.isHole);
		}

		const unsigned int contourStart = (unsigned int)out.verts.size();
		for (size_t k = 0; k < n; ++k)
		{
			const Vector2f& p = pts[reverse ? (n - 1 - k) : k];
			out.verts.push_back({ p.x, p.y });
		}

		gluTessBeginContour(tess);
		for (size_t i = 0; i < n; ++i)
		{
			// By value: the COMBINE callback appends to out.verts, so holding a
			// reference into it across a glutess call would be unsound.
			const float px = out.verts[contourStart + i][0];
			const float py = out.verts[contourStart + i][1];
			coordPool.push_back({ (GLdouble)px, (GLdouble)py, 0.0 });
			gluTessVertex(tess, coordPool.back().data(),
			              (void*)(uintptr_t)(contourStart + (unsigned int)i));
		}
		gluTessEndContour(tess);

		// Record outline edges for wall construction.
		for (unsigned int i = 0; i < (unsigned int)n; ++i)
			outlineEdges.emplace_back(contourStart + i,
			                          contourStart + ((i + 1) % (unsigned int)n));
	}
	gluTessEndPolygon(tess);

	gluDeleteTess(tess);
}

} // namespace

// ============================================================================
//  ExtrudedMeshBuilder
// ============================================================================

bool ExtrudedMeshBuilder::Append(const std::vector<ExtrudeContour>& contours,
                                 const ExtrudeAppendOptions& opt)
{
	TessOut out;
	std::vector<std::pair<unsigned int, unsigned int>> outlineEdges;
	tessellateContours(contours, opt.normalizeOrientation, opt.winding,
	                   out, outlineEdges);

	if (out.verts.empty() || out.tris.empty())
		return false;

	const std::vector<std::array<float, 2>>& verts2D = out.verts;
	const std::vector<unsigned int>& tris = out.tris;

	const unsigned int n    = (unsigned int)verts2D.size();
	const unsigned int base = (unsigned int)(m_verts.size() / 3);

	//   [base +   0 ..  n-1] : bottom cap  (z = zBottom)
	//   [base +   n .. 2n-1] : top    cap  (z = zTop)
	//   [base +  2n .. 3n-1] : bottom wall (z = zBottom)
	//   [base +  3n .. 4n-1] : top    wall (z = zTop)
	// Disjoint blocks: see the class comment in extrude_contours.h.
	const unsigned int kBotCap  = base;
	const unsigned int kTopCap  = base + n;
	const unsigned int kBotWall = base + 2u * n;
	const unsigned int kTopWall = base + 3u * n;

	const unsigned int nBlocks = opt.emitWalls ? 4u : 2u;
	// Produit en size_t : en unsigned int il deborderait (silencieusement, donc en
	// sous-reservant) au-dela de ~357 M de sommets par Append.
	m_verts.reserve(m_verts.size() + (size_t)3u * nBlocks * n);
	auto pushBlock = [&](float z)
	{
		for (unsigned int i = 0; i < n; ++i)
		{
			m_verts.push_back(verts2D[i][0]);
			m_verts.push_back(verts2D[i][1]);
			m_verts.push_back(z);
		}
	};
	pushBlock(opt.zBottom);   // bottom cap
	pushBlock(opt.zTop);      // top cap
	if (opt.emitWalls)
	{
		pushBlock(opt.zBottom);   // bottom wall (duplicate of bottom cap)
		pushBlock(opt.zTop);      // top wall    (duplicate of top cap)
	}

	// Cap orientation. We can't assume a fixed glutess output winding: glutess
	// preserves the contour's input orientation, and an authored contour (e.g. a
	// potrace SVG with a `scale(0.1, -0.1)` transform) can wind either way. So
	// compute each triangle's signed area in world XY and pick the winding that
	// yields the desired ±Z normal per cap: bottom wants CW (normal -Z), top
	// wants CCW (+Z).
	auto emitCap = [&](unsigned int blockOff, bool wantCCW)
	{
		for (size_t t = 0; t + 2 < tris.size(); t += 3)
		{
			const unsigned int i[3] = { tris[t + 0], tris[t + 1], tris[t + 2] };
			const float x0 = verts2D[i[0]][0], y0 = verts2D[i[0]][1];
			const float x1 = verts2D[i[1]][0], y1 = verts2D[i[1]][1];
			const float x2 = verts2D[i[2]][0], y2 = verts2D[i[2]][1];
			const bool worldCCW =
				(x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0) > 0.0f;

			// Keep glutess order when it already matches the desired winding;
			// otherwise swap i[1] and i[2] to reverse.
			const unsigned int second = (worldCCW == wantCCW) ? i[1] : i[2];
			const unsigned int third  = (worldCCW == wantCCW) ? i[2] : i[1];

			m_faces.push_back({ blockOff + i[0], blockOff + second,
			                    blockOff + third, opt.materialId });
		}
	};
	emitCap(kBotCap, /*wantCCW=*/false);
	emitCap(kTopCap, /*wantCCW=*/true);

	if (!opt.emitWalls)
		return true;

	// Side walls. For each contour edge we want a normal that points AWAY from
	// the filled region — but the contour can be wound either way, so we can't
	// hard-code a winding.
	//
	// The cap tessellation knows the truth: an outline edge is a BOUNDARY edge
	// of the cap mesh, contained in exactly one cap triangle whose third vertex
	// lies on the filled side. We index every cap triangle's edges → third
	// vertex, then for each outline edge compute which side of (a→b) the third
	// vertex lies on and emit the wall winding whose cross product points to the
	// opposite side.
	auto edgeKey = [](unsigned int u, unsigned int v) -> std::uint64_t
	{
		if (u > v) std::swap(u, v);
		return (std::uint64_t(u) << 32) | std::uint64_t(v);
	};

	std::unordered_map<std::uint64_t, unsigned int> edgeThird;
	edgeThird.reserve(tris.size());
	for (size_t t = 0; t + 2 < tris.size(); t += 3)
	{
		const unsigned int i[3] = { tris[t + 0], tris[t + 1], tris[t + 2] };
		for (int k = 0; k < 3; ++k)
			edgeThird[edgeKey(i[k], i[(k + 1) % 3])] = i[(k + 2) % 3];
	}

	for (auto [a, b] : outlineEdges)
	{
		auto it = edgeThird.find(edgeKey(a, b));
		if (it == edgeThird.end())
			continue;                              // tessellation gap; skip wall
		const unsigned int c = it->second;

		const float ax = verts2D[a][0], ay = verts2D[a][1];
		const float bx = verts2D[b][0], by = verts2D[b][1];
		const float cx = verts2D[c][0], cy = verts2D[c][1];

		if (opt.wallFilter && !opt.wallFilter(Vector2f(ax, ay), Vector2f(bx, by)))
			continue;                              // caller vetoed this wall

		// crossZ = (b-a) × (c-a)._z, Y-up math: >0 means c is on the LEFT of
		// (a→b). c is on the filled side, so outward is the opposite.
		const float crossZ = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
		const bool outwardOnLeft = (crossZ < 0.0f);

		const unsigned int b_a = kBotWall + a;
		const unsigned int b_b = kBotWall + b;
		const unsigned int t_a = kTopWall + a;
		const unsigned int t_b = kTopWall + b;

		if (outwardOnLeft)
		{
			// Winding (b_a, t_b, b_b) gives a normal rotated +90° CCW from the
			// edge direction, i.e. to the LEFT of (a→b).
			m_faces.push_back({ b_a, t_b, b_b, opt.materialId });
			m_faces.push_back({ b_a, t_a, t_b, opt.materialId });
		}
		else
		{
			// Mirror winding gives the normal on the RIGHT of (a→b).
			m_faces.push_back({ b_a, b_b, t_b, opt.materialId });
			m_faces.push_back({ b_a, t_b, t_a, opt.materialId });
		}
	}

	return true;
}

Mesh* ExtrudedMeshBuilder::Build(void)
{
	if (m_verts.empty() || m_faces.empty())
		return nullptr;

	// Drop the vertices no face references. Each Append reserves four full blocks
	// up front, so a vetoed wall (ExtrudeAppendOptions::wallFilter) leaves its two
	// wall corners behind; keeping them would ship a mesh half made of orphans
	// with zero normals. With no veto nothing is dropped and the numbering is
	// unchanged.
	const unsigned int nIn = (unsigned int)(m_verts.size() / 3);
	std::vector<unsigned int> remap(nIn, ~0u);
	for (const Tri& t : m_faces)
	{
		remap[t.a] = 0;
		remap[t.b] = 0;
		remap[t.c] = 0;
	}

	std::vector<float> verts;
	verts.reserve(m_verts.size());
	unsigned int nOut = 0;
	for (unsigned int i = 0; i < nIn; ++i)
	{
		if (remap[i] == ~0u) continue;
		remap[i] = nOut++;
		verts.push_back(m_verts[3*i + 0]);
		verts.push_back(m_verts[3*i + 1]);
		verts.push_back(m_verts[3*i + 2]);
	}

	auto* m = new Mesh();
	m->SetVertices(nOut, verts.data());

	m->m_nFaces = (unsigned int)m_faces.size();
	m->m_pFaces = new Face*[m_faces.size()];
	for (size_t i = 0; i < m_faces.size(); ++i)
	{
		const Tri& t = m_faces[i];
		Face* f = new Face();
		f->SetNVertices(3);
		f->SetVertex(0, remap[t.a]);
		f->SetVertex(1, remap[t.b]);
		f->SetVertex(2, remap[t.c]);
		f->SetMaterialId(t.materialId);
		m->m_pFaces[i] = f;
	}

	m->ComputeNormals();
	m->IncrementRevision();
	return m;
}
