#include "image_relief.h"

#include "extrude_contours.h"
#include "image_vectorization.h"
#include "material.h"
#include "mesh.h"

#include "../cgimg/cgimg.h"

#include "clipper2/clipper.h"   // offset de polygones (vendored, extern/clipper2)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <utility>

namespace {

// Img::palettize() stores ONE BYTE per pixel and CLitRasterToVector reserves the
// index right after the palette for its border ring, so the palette must stay
// well under 256 entries.
const int kMaxPaletteColors = 250;

// ============================================================================
//  Despeckling of the quantized image (label domain)
// ============================================================================
//
// Everything here works on LABELS (one colour index per pixel) and only ever
// replaces a label by one already present among its neighbours. The labelling
// therefore stays a complete tiling of the image, which is what guarantees the
// extruded regions keep covering the footprint exactly -- no voids. See the
// comments on ImageReliefOptions::despecklePasses for why the alternative
// (filtering vectorized contours by area) cannot offer that.

struct LabelImage
{
	int w = 0, h = 0;
	std::vector<int>          px;      // label per pixel
	std::vector<unsigned int> rgb;     // label -> packed RGB
	int label(int x, int y) const { return px[(size_t)y * w + x]; }
	void set(int x, int y, int l)  { px[(size_t)y * w + x] = l; }
};

LabelImage toLabels(const Img& img)
{
	LabelImage L;
	L.w = (int)img.width();
	L.h = (int)img.height();
	L.px.assign((size_t)L.w * L.h, 0);

	std::map<unsigned int, int> seen;
	for (int y = 0; y < L.h; ++y)
		for (int x = 0; x < L.w; ++x)
		{
			unsigned char r = 0, g = 0, b = 0, a = 0;
			img.get_pixel((unsigned)x, (unsigned)y, &r, &g, &b, &a);
			const unsigned int key = ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
			auto it = seen.find(key);
			if (it == seen.end())
			{
				it = seen.emplace(key, (int)L.rgb.size()).first;
				L.rgb.push_back(key);
			}
			L.set(x, y, it->second);
		}
	return L;
}

void applyLabels(const LabelImage& L, Img& img)
{
	for (int y = 0; y < L.h; ++y)
		for (int x = 0; x < L.w; ++x)
		{
			const unsigned int c = L.rgb[(size_t)L.label(x, y)];
			img.set_pixel((unsigned)x, (unsigned)y,
			              (unsigned char)(c >> 16), (unsigned char)(c >> 8),
			              (unsigned char)c, 255);
		}
}

// One 3x3 majority pass. Read from a snapshot so the pass is order-independent.
// Ties keep the centre pixel, so a genuine boundary does not drift.
void majorityPass(LabelImage& L)
{
	const std::vector<int> src = L.px;
	const int nLabels = (int)L.rgb.size();
	std::vector<int> votes((size_t)nLabels, 0);

	for (int y = 0; y < L.h; ++y)
		for (int x = 0; x < L.w; ++x)
		{
			const int centre = src[(size_t)y * L.w + x];
			int touched[9], nTouched = 0;
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx)
				{
					const int xx = x + dx, yy = y + dy;
					if (xx < 0 || yy < 0 || xx >= L.w || yy >= L.h) continue;
					const int l = src[(size_t)yy * L.w + xx];
					if (votes[(size_t)l]++ == 0) touched[nTouched++] = l;
				}

			int best = centre, bestVotes = votes[(size_t)centre];
			for (int i = 0; i < nTouched; ++i)
				if (votes[(size_t)touched[i]] > bestVotes)
				{
					bestVotes = votes[(size_t)touched[i]];
					best = touched[i];
				}
			for (int i = 0; i < nTouched; ++i) votes[(size_t)touched[i]] = 0;

			L.set(x, y, best);
		}
}

// Absorb every connected region below minArea into its most frequent
// neighbouring label. Repeated a few times: merging a speck into a small
// neighbour can leave the result still under the threshold.
//
// Chaque passe MESURE sur `L` intact et ECRIT dans une copie, comme
// majorityPass. Muter `L` au fil du balayage rendait la passe dependante de
// l'ordre : une region d'accueil rencontree APRES l'absorption d'un mouchetis
// voyait ses cellules fraichement absorbees exclues de son flood (leur `comp`
// etait deja marque), donc une taille sous-estimee -- et pouvait a son tour
// passer sous le seuil et partir dans une autre couleur.
void mergeSmallComponents(LabelImage& L, int minArea)
{
	const int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };

	for (int pass = 0; pass < 3; ++pass)
	{
		std::vector<int> comp((size_t)L.w * L.h, -1);
		std::vector<int> next = L.px;   // les fusions de CETTE passe vont ici
		bool merged = false;
		int nComp = 0;

		for (int y0 = 0; y0 < L.h; ++y0)
			for (int x0 = 0; x0 < L.w; ++x0)
			{
				if (comp[(size_t)y0 * L.w + x0] != -1) continue;
				const int mine = L.label(x0, y0);

				// flood fill the component, tallying the labels on its border
				std::vector<std::pair<int,int>> cells;
				std::map<int, int> borderVotes;
				std::vector<std::pair<int,int>> stack{ { x0, y0 } };
				comp[(size_t)y0 * L.w + x0] = nComp;
				while (!stack.empty())
				{
					const auto [x, y] = stack.back(); stack.pop_back();
					cells.emplace_back(x, y);
					for (int k = 0; k < 4; ++k)
					{
						const int xx = x + dx4[k], yy = y + dy4[k];
						if (xx < 0 || yy < 0 || xx >= L.w || yy >= L.h) continue;
						const int other = L.label(xx, yy);
						if (other != mine) { borderVotes[other]++; continue; }
						if (comp[(size_t)yy * L.w + xx] != -1) continue;
						comp[(size_t)yy * L.w + xx] = nComp;
						stack.emplace_back(xx, yy);
					}
				}
				++nComp;

				if ((int)cells.size() >= minArea || borderVotes.empty())
					continue;

				int winner = borderVotes.begin()->first, best = -1;
				for (const auto& [lab, n] : borderVotes)
					if (n > best) { best = n; winner = lab; }

				for (const auto& [x, y] : cells) next[(size_t)y * L.w + x] = winner;
				merged = true;
			}

		if (!merged) break;
		L.px.swap(next);
	}
}

void despeckle(Img& img, const ImageReliefOptions& opt)
{
	if (opt.despecklePasses <= 0 && opt.minRegionArea <= 0)
		return;

	LabelImage L = toLabels(img);
	for (int i = 0; i < opt.despecklePasses; ++i)
		majorityPass(L);
	if (opt.minRegionArea > 0)
		mergeSmallComponents(L, opt.minRegionArea);
	applyLabels(L, img);
}

// ============================================================================
//  Shrinkage: negative offset on every region contour
// ============================================================================

// Aire signee d'un contour ferme (lacet). Meme convention que
// VectorContour::area : repere image (y vers le bas), exterieur negatif.
double contourArea(const std::vector<Vector2f>& pts)
{
	double a = 0.;
	const size_t n = pts.size();
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2f& p = pts[i];
		const Vector2f& q = pts[(i + 1) % n];
		a += (double)p.x * (double)q.y - (double)q.x * (double)p.y;
	}
	return .5 * a;
}

// Erode chaque region de `shrink` PIXELS vers l'interieur (offset negatif), ce
// qui creuse un sillon entre blocs voisins au lieu de les laisser jointifs.
//
// Travaille en coordonnees IMAGE, donc en pixels de la source : c'est l'unite des
// autres parametres de l'etage vectorisation (simplifyErr, minRegionArea), et
// surtout PathsD de Clipper2 arrondit par defaut a 2 decimales -- en coordonnees
// monde (de l'ordre de 0,5) tout s'effondrerait sur une grille de 100x100.
void shrinkLayers(std::vector<VectorLayer>& layers, float shrink)
{
	if (shrink <= 0.f) return;

	using Clipper2Lib::PathD;
	using Clipper2Lib::PathsD;
	using Clipper2Lib::JoinType;
	using Clipper2Lib::EndType;

	const int kPrecision = 4;     // 1e-4 px : large, et sans risque de debordement

	for (VectorLayer& layer : layers)
	{
		PathsD paths;
		paths.reserve(layer.contours.size());
		for (const VectorContour& c : layer.contours)
		{
			if (c.pts.size() < 3) continue;
			PathD p;
			p.reserve(c.pts.size());
			for (const Vector2f& v : c.pts)
				p.emplace_back((double)v.x, (double)v.y);
			// Clipper2 attend un contour exterieur d'aire POSITIVE et un trou d'aire
			// NEGATIVE : c'est cette orientation qui lui dit de quel cote pousser la
			// matiere. Notre convention interne est l'inverse (cf. VectorContour::
			// area), donc on impose le signe explicitement plutot que de raisonner
			// sur l'enchainement des conventions.
			const bool wantPositive = !c.isHole;
			if ((Clipper2Lib::Area(p) > 0.) != wantPositive)
				std::reverse(p.begin(), p.end());
			paths.push_back(std::move(p));
		}
		if (paths.empty()) { layer.contours.clear(); continue; }

		// UN SEUL appel pour toute la couche : sur un polygone a trous, un delta
		// negatif erode le contour exterieur ET dilate les trous, c'est-a-dire
		// retire de la matiere des deux cotes. Contour par contour, les trous
		// seraient rognes au lieu d'etre elargis, et la region grossirait par
		// l'interieur.
		//
		// Miter (et non Round) : conserve les angles vifs -- ce qui va bien a une
		// affiche -- et ne rajoute aucun sommet d'arc, donc pas d'inflation du
		// maillage apres le Simplify du vectoriseur. miter_limit borne les pointes
		// aux angles rentrants (au-dela, Clipper2 retombe sur un chanfrein).
		const PathsD out = Clipper2Lib::InflatePaths(
			paths, -(double)shrink, JoinType::Miter, EndType::Polygon,
			/*miter_limit=*/2.0, kPrecision);

		std::vector<VectorContour> kept;
		kept.reserve(out.size());
		for (const PathD& p : out)
		{
			if (p.size() < 3) continue;
			VectorContour c;
			c.pts.reserve(p.size());
			// Retour a la convention interne : on renverse, ce qui inverse le signe
			// de l'aire (Clipper2 exterieur positif -> image exterieur negatif).
			for (auto it = p.rbegin(); it != p.rend(); ++it)
				c.pts.emplace_back((float)it->x, (float)it->y);
			c.area = (float)contourArea(c.pts);
			if (std::fabs(c.area) < 1e-6f)
				continue;                    // degenere
			c.isHole = (c.area > 0.f);       // idem GetLayers()
			kept.push_back(std::move(c));
		}
		layer.contours.swap(kept);
	}

	// Une couche dont toutes les regions sont plus fines que 2*shrink se resorbe
	// entierement : elle disparait, et son materiau avec.
	layers.erase(std::remove_if(layers.begin(), layers.end(),
	                            [](const VectorLayer& l) { return l.contours.empty(); }),
	             layers.end());
}

// ============================================================================
//  Front end: image -> colour layers in world XY
// ============================================================================

struct ReliefContent
{
	// Vectorized layers, contour points already mapped to WORLD XY (Y flipped,
	// scaled to fitSize, content centred on the origin).
	std::vector<VectorLayer> layers;

	// Half extents of the content footprint in world XY.
	float halfW = 0.f;
	float halfH = 0.f;

	// World unit of one quantization step for the edge keys below.
	float quantScale = 1.f;
};

// Load, quantize, vectorize, and map every contour to world XY.
bool buildContent(const std::string& filename,
                  const ImageReliefOptions& opt,
                  ReliefContent& content)
{
	Img img;
	if (img.load(filename.c_str()) != 0 || img.width() == 0 || img.height() == 0)
	{
		std::fprintf(stderr, "image_relief: failed to load %s\n", filename.c_str());
		return false;
	}

	// Edge-preserving smoothing FIRST: it resolves the anti-aliased ramps of a
	// resampled source before the palette decision is taken, which is what keeps
	// the region boundaries on the shapes of the original (cf. preSmoothPasses).
	for (int i = 0; i < opt.preSmoothPasses; ++i)
		img.bilateral_filtering();

	// Reference pour le raffinement : l'image APRES lissage, car c'est bien elle
	// que la segmentation doit representer (et non le bruit qu'on vient d'oter).
	Img reference;
	if (opt.refineIterations > 0)
		reference = img;

	const int nColors = std::max(2, std::min(opt.maxColors, kMaxPaletteColors));
	if (opt.algo == QuantAlgo::Heckbert)
		img.quant_heckbert(nColors);
	else
		img.quant_wu(nColors);

	// Raffinement AVANT l'anti-mouchetis : il reaffecte chaque pixel au plus
	// proche, donc l'appliquer apres reintroduirait le mouchetis qu'on vient
	// d'oter.
	if (opt.refineIterations > 0)
		img.quant_refine(reference, opt.refineIterations);

	// Clean the labelling BEFORE vectorizing: the vectorizer then never sees the
	// compression hatching, so no speck ever becomes a block and the regions
	// still tile the footprint exactly.
	despeckle(img, opt);

	// get_palette() returns the Img-owned palette for a palettized image and a
	// FRESH heap one otherwise (which is our case here: quantization works on the
	// RGBA buffer). Its colours are collected in raster-scan order, the very
	// order Img::palettize() assigns indices in, so palette index i and
	// palettized pixel value i denote the same colour inside Vectorize().
	const bool paletteOwnedByImg = img.uses_palette();
	Palette* pPalette = img.get_palette();
	if (!pPalette || pPalette->NColors() == 0)
	{
		if (!paletteOwnedByImg) delete pPalette;
		std::fprintf(stderr, "image_relief: %s has no colour\n", filename.c_str());
		return false;
	}

	CLitRasterToVector rtv;
	const bool ok = rtv.Vectorize(&img, Color(), /*bUseMask=*/false,
	                              pPalette, opt.simplifyErr);
	if (!paletteOwnedByImg)
		delete pPalette;
	if (!ok)
	{
		std::fprintf(stderr, "image_relief: vectorization failed on %s\n", filename.c_str());
		return false;
	}

	content.layers = rtv.GetLayers();
	if (content.layers.empty())
	{
		std::fprintf(stderr, "image_relief: %s vectorized to no region\n", filename.c_str());
		return false;
	}

	// La bbox est mesuree AVANT le retrait, volontairement : ainsi `shrink` ne fait
	// que creuser des sillons et ne change NI l'echelle finale ni la position du
	// cadre. Sinon, retrecir la couche de fond retrecirait l'empreinte, que le
	// recadrage a fitSize regrandirait ensuite -- le parametre se mordrait la queue.
	//
	// Single pixel -> world transform for every contour: fit the content bbox to
	// fitSize on its longest side (aspect ratio preserved), centre it on the
	// origin, and flip Y so the image is upright in a Y-up frame. Keeping this in
	// one place is what guarantees blocks, base and wall share a coordinate
	// system. The background is vectorized as a full region, so this bbox is the
	// image rectangle.
	bool any = false;
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
	for (const VectorLayer& layer : content.layers)
		for (const VectorContour& c : layer.contours)
			for (const Vector2f& p : c.pts)
			{
				if (!any) { minX = maxX = p.x; minY = maxY = p.y; any = true; continue; }
				minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
			}
	if (!any) return false;

	const float w = maxX - minX;
	const float h = maxY - minY;
	const float largest = std::max(w, h);
	if (largest < 1e-9f) return false;

	const float scale = std::max(opt.fitSize, 1e-6f) / largest;
	const float cx = 0.5f * (minX + maxX);
	const float cy = 0.5f * (minY + maxY);

	// Retrait des regions, en pixels source, une fois la bbox figee.
	shrinkLayers(content.layers, opt.shrink);
	if (content.layers.empty())
	{
		std::fprintf(stderr,
		             "image_relief: %s has no region left after shrink=%g px\n",
		             filename.c_str(), opt.shrink);
		return false;
	}

	for (VectorLayer& layer : content.layers)
		for (VectorContour& c : layer.contours)
			for (Vector2f& p : c.pts)
				p.Set((p.x - cx) * scale, -(p.y - cy) * scale);

	content.halfW = 0.5f * w * scale;
	content.halfH = 0.5f * h * scale;
	// One millionth of the footprint: fine enough that two distinct contour
	// vertices never collapse, coarse enough to absorb float noise.
	content.quantScale = 1e6f / std::max(opt.fitSize, 1e-6f);

	return true;
}

// ============================================================================
//  Internal (coincident) wall removal
// ============================================================================
//
// Two adjacent colour regions share their common boundary EXACTLY: the
// vectorizer keys every contour point on a grid point in a single shared
// coordinate map (CLitRasterToVector::m_mapCoord) and simplifies against the
// global adjacency map, so both layers get the same polyline (traversed in
// opposite directions). Counting contour edges over ALL layers therefore tells
// a silhouette edge (seen once) from an internal boundary (seen twice) exactly.

typedef std::pair<std::uint64_t, std::uint64_t> EdgeKey;

std::uint64_t vertexKey(const Vector2f& p, float q)
{
	const std::int32_t x = (std::int32_t)std::lround((double)p.x * q);
	const std::int32_t y = (std::int32_t)std::lround((double)p.y * q);
	return ((std::uint64_t)(std::uint32_t)x << 32) | (std::uint64_t)(std::uint32_t)y;
}

// Undirected: the two layers sharing a boundary traverse it in opposite ways.
EdgeKey edgeKey(const Vector2f& a, const Vector2f& b, float q)
{
	const std::uint64_t ka = vertexKey(a, q);
	const std::uint64_t kb = vertexKey(b, q);
	return (ka <= kb) ? EdgeKey(ka, kb) : EdgeKey(kb, ka);
}

std::map<EdgeKey, int> countLayerEdges(const ReliefContent& content)
{
	std::map<EdgeKey, int> counts;
	for (const VectorLayer& layer : content.layers)
		for (const VectorContour& c : layer.contours)
		{
			const size_t n = c.pts.size();
			for (size_t i = 0; i < n; ++i)
				counts[edgeKey(c.pts[i], c.pts[(i + 1) % n], content.quantScale)]++;
		}
	return counts;
}

// ============================================================================
//  Geometry assembly
// ============================================================================

// Rectangle centred on the origin, CCW. Orientation is normalized from isHole
// by the extruder, so `isHole` alone expresses the intent.
ExtrudeContour makeRect(float hx, float hy, bool isHole)
{
	ExtrudeContour c;
	c.isHole = isHole;
	c.pts.emplace_back(-hx, -hy);
	c.pts.emplace_back( hx, -hy);
	c.pts.emplace_back( hx,  hy);
	c.pts.emplace_back(-hx,  hy);
	return c;
}

ExtrudeAppendOptions blockOptions(const ImageReliefOptions& opt, unsigned int materialId)
{
	ExtrudeAppendOptions ao;
	// Relief: blocks rise from the TOP face of the base plate, not from z=0.
	ao.zBottom = opt.baseThickness;
	ao.zTop    = opt.baseThickness + opt.blockHeight;
	ao.materialId = materialId;
	ao.winding = ExtrudeWinding::NonZero;
	// Contour orientation comes from the vectorizer's tracing direction, which we
	// classify into outer/hole in VectorContour::isHole — so let the extruder
	// rewind from that flag rather than trusting the raw winding.
	ao.normalizeOrientation = true;
	return ao;
}

void appendLayer(ExtrudedMeshBuilder& builder,
                 const VectorLayer& layer,
                 unsigned int materialId,
                 const ImageReliefOptions& opt,
                 const std::map<EdgeKey, int>* internalEdges,
                 float quantScale)
{
	std::vector<ExtrudeContour> contours;
	contours.reserve(layer.contours.size());
	for (const VectorContour& c : layer.contours)
	{
		ExtrudeContour ec;
		ec.pts    = c.pts;
		ec.isHole = c.isHole;
		contours.push_back(std::move(ec));
	}

	ExtrudeAppendOptions ao = blockOptions(opt, materialId);
	if (internalEdges)
	{
		const std::map<EdgeKey, int>* counts = internalEdges;
		ao.wallFilter = [counts, quantScale](const Vector2f& a, const Vector2f& b)
		{
			auto it = counts->find(edgeKey(a, b, quantScale));
			return it == counts->end() || it->second < 2;   // shared edge -> no wall
		};
	}
	builder.Append(contours, ao);
}

void appendBase(ExtrudedMeshBuilder& builder,
                unsigned int materialId,
                const ReliefContent& content,
                const ImageReliefOptions& opt)
{
	const float outer = opt.margin + opt.wallThickness;
	ExtrudeAppendOptions ao;
	ao.zBottom = 0.f;
	ao.zTop    = opt.baseThickness;
	ao.materialId = materialId;
	ao.normalizeOrientation = true;
	std::vector<ExtrudeContour> rect;
	rect.push_back(makeRect(content.halfW + outer, content.halfH + outer, false));
	builder.Append(rect, ao);
}

void appendWall(ExtrudedMeshBuilder& builder,
                unsigned int materialId,
                const ReliefContent& content,
                const ImageReliefOptions& opt)
{
	// Rectangular ring: outer boundary = edge of the base, inner boundary =
	// content + margin (so `margin` is the gap between the content and the INNER
	// wall face). Sits on the base like the blocks do.
	ExtrudeAppendOptions ao;
	ao.zBottom = opt.baseThickness;
	ao.zTop    = opt.baseThickness + opt.wallHeight;
	ao.materialId = materialId;
	ao.normalizeOrientation = true;

	const float inner = opt.margin;
	const float outer = opt.margin + opt.wallThickness;
	std::vector<ExtrudeContour> ring;
	ring.push_back(makeRect(content.halfW + outer, content.halfH + outer, false));
	ring.push_back(makeRect(content.halfW + inner, content.halfH + inner, true));
	builder.Append(ring, ao);
}

Material* makeColorMaterial(Color color, const std::string& name)
{
	auto* mat = new MaterialColor(color.r(), color.g(), color.b(), color.a());
	mat->SetName(name);
	return mat;
}

} // namespace

// ============================================================================
//  Public entry points
// ============================================================================

Mesh* image_to_relief(const std::string& filename, const ImageReliefOptions& opt)
{
	ReliefContent content;
	if (!buildContent(filename, opt, content))
		return nullptr;

	std::map<EdgeKey, int> internalEdges;
	const std::map<EdgeKey, int>* pInternalEdges = nullptr;
	if (!opt.emitInternalWalls)
	{
		internalEdges  = countLayerEdges(content);
		pInternalEdges = &internalEdges;
	}

	// Material ids are assigned up front so they match the order the materials
	// are registered on the baked Mesh below: one per colour layer, then base,
	// then wall.
	const unsigned int nLayers = (unsigned int)content.layers.size();

	ExtrudedMeshBuilder builder;
	for (unsigned int i = 0; i < nLayers; ++i)
		appendLayer(builder, content.layers[i], i, opt, pInternalEdges, content.quantScale);
	appendBase(builder, nLayers,      content, opt);
	appendWall(builder, nLayers + 1u, content, opt);

	Mesh* m = builder.Build();
	if (!m)
	{
		std::fprintf(stderr, "image_relief: %s produced no geometry\n", filename.c_str());
		return nullptr;
	}

	char name[32];
	for (unsigned int i = 0; i < nLayers; ++i)
	{
		std::snprintf(name, sizeof(name), "color_%02u", i);
		m->Material_Add(makeColorMaterial(content.layers[i].color, name));
	}
	m->Material_Add(makeColorMaterial(opt.baseColor, "base"));
	m->Material_Add(makeColorMaterial(opt.wallColor, "wall"));

	m->computebbox();
	return m;
}

std::vector<Mesh*> image_to_relief_per_color(const std::string& filename,
                                             const ImageReliefOptions& opt)
{
	std::vector<Mesh*> meshes;

	ReliefContent content;
	if (!buildContent(filename, opt, content))
		return meshes;

	std::map<EdgeKey, int> internalEdges;
	const std::map<EdgeKey, int>* pInternalEdges = nullptr;
	if (!opt.emitInternalWalls)
	{
		internalEdges  = countLayerEdges(content);
		pInternalEdges = &internalEdges;
	}

	// Ordre documente (cf. image_relief.h) : les couleurs dans l'ordre d'index de
	// palette, puis la base, puis le mur. Un layer qui ne tesselle rien est OMIS
	// (vecteur toujours dense) ; son index de palette reste lisible dans le nom du
	// materiau, seule correspondance fiable.
	char name[32];
	for (size_t i = 0; i < content.layers.size(); ++i)
	{
		ExtrudedMeshBuilder builder;
		appendLayer(builder, content.layers[i], 0, opt, pInternalEdges, content.quantScale);
		Mesh* m = builder.Build();
		if (!m)
		{
			std::fprintf(stderr,
			             "image_relief: %s layer %zu tessellated to nothing, omitted\n",
			             filename.c_str(), i);
			continue;
		}
		std::snprintf(name, sizeof(name), "color_%02zu", i);
		m->Material_Add(makeColorMaterial(content.layers[i].color, name));
		m->computebbox();
		meshes.push_back(m);
	}

	// Base et mur : de simples rectangles. Un echec ici est un bug, pas une
	// condition de donnees -- on echoue franchement plutot que de livrer un relief
	// sans cadre, ce qui casserait en silence le "toujours les deux derniers".
	auto appendFrame = [&](void (*build)(ExtrudedMeshBuilder&, unsigned int,
	                                     const ReliefContent&, const ImageReliefOptions&),
	                       Color colour, const char* matName) -> bool
	{
		ExtrudedMeshBuilder builder;
		build(builder, 0, content, opt);
		Mesh* m = builder.Build();
		if (!m) return false;
		m->Material_Add(makeColorMaterial(colour, matName));
		m->computebbox();
		meshes.push_back(m);
		return true;
	};

	if (!appendFrame(appendBase, opt.baseColor, "base") ||
	    !appendFrame(appendWall, opt.wallColor, "wall"))
	{
		std::fprintf(stderr, "image_relief: %s failed to build the base/wall frame\n",
		             filename.c_str());
		for (Mesh* m : meshes) delete m;   // pas de demi-resultat
		meshes.clear();
		return meshes;
	}

	return meshes;
}
