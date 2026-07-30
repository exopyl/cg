#include "image_region_pipeline.h"

#include "extrude_contours.h"

#include "../cgimg/cgimg.h"

#include "clipper2/clipper.h"   // offset de polygones (vendored, extern/clipper2)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

const int kMaxPaletteColors = 250;

namespace {

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

void despeckle(Img& img, int passes, int minRegionArea)
{
	if (passes <= 0 && minRegionArea <= 0)
		return;

	LabelImage L = toLabels(img);
	for (int i = 0; i < passes; ++i)
		majorityPass(L);
	if (minRegionArea > 0)
		mergeSmallComponents(L, minRegionArea);
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

} // namespace

// Erode un polygone a trous de `shrink` PIXELS vers l'interieur (offset negatif),
// ce qui creuse un sillon entre regions voisines au lieu de les laisser jointives.
//
// Travaille en coordonnees IMAGE, donc en pixels de l'image vectorisee : c'est
// l'unite des autres parametres de l'etage vectorisation (simplifyErr,
// minRegionArea), et surtout PathsD de Clipper2 arrondit par defaut a 2 decimales
// -- en coordonnees monde (de l'ordre de 0,5) tout s'effondrerait sur une grille
// de 100x100.
void region_shrink_contours(std::vector<VectorContour>& contours, float shrink)
{
	if (shrink <= 0.f) return;

	using Clipper2Lib::PathD;
	using Clipper2Lib::PathsD;
	using Clipper2Lib::JoinType;
	using Clipper2Lib::EndType;

	const int kPrecision = 4;     // 1e-4 px : large, et sans risque de debordement

	PathsD paths;
	paths.reserve(contours.size());
	for (const VectorContour& c : contours)
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
	if (paths.empty()) { contours.clear(); return; }

	// Miter (et non Round) : conserve les angles vifs -- ce qui va bien a une
	// affiche comme a du pixel art -- et ne rajoute aucun sommet d'arc, donc pas
	// d'inflation du maillage. miter_limit borne les pointes aux angles rentrants
	// (au-dela, Clipper2 retombe sur un chanfrein).
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
	contours.swap(kept);
}

// ============================================================================
//  Pixelisation par vote majoritaire
// ============================================================================

bool pixelize_majority(Img& img, int targetW)
{
	const int W = (int)img.width();
	const int H = (int)img.height();
	if (targetW <= 0 || W <= 0 || H <= 0) return false;
	if (targetW >= W) return true;             // rien a reduire : on ne dilate pas

	// Hauteur deduite du rapport d'aspect, au moins 1 cellule.
	int targetH = (int)std::lround((double)H * (double)targetW / (double)W);
	if (targetH < 1) targetH = 1;

	Img out((unsigned)targetW, (unsigned)targetH);

	for (int j = 0; j < targetH; ++j)
	{
		// Bornes EXACTES du bloc source : la derniere cellule couvre le reste de la
		// division. Img::resize mode 2 utilise un pas constant W/w, ce qui laisse
		// tomber les dernieres colonnes (400 px en 64 blocs n'en couvre que 384).
		const int y0 = (int)((long long)j * H / targetH);
		const int y1 = (int)((long long)(j + 1) * H / targetH);

		for (int i = 0; i < targetW; ++i)
		{
			const int x0 = (int)((long long)i * W / targetW);
			const int x1 = (int)((long long)(i + 1) * W / targetW);

			std::map<unsigned int, int> votes;
			for (int y = y0; y < y1; ++y)
				for (int x = x0; x < x1; ++x)
				{
					unsigned char r = 0, g = 0, b = 0, a = 0;
					img.get_pixel((unsigned)x, (unsigned)y, &r, &g, &b, &a);
					votes[((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b]++;
				}
			if (votes.empty()) continue;       // bloc vide (ne devrait pas arriver)

			// Depart determine par la valeur RGB la plus BASSE a egalite : std::map
			// itere en ordre croissant et on ne remplace que sur un compte
			// STRICTEMENT superieur. Sans cette regle, deux couleurs a egalite
			// feraient dependre le resultat de l'ordre de parcours.
			unsigned int bestColor = votes.begin()->first;
			int bestCount = -1;
			for (const auto& [color, n] : votes)
				if (n > bestCount) { bestCount = n; bestColor = color; }

			out.set_pixel((unsigned)i, (unsigned)j,
			              (unsigned char)(bestColor >> 16),
			              (unsigned char)(bestColor >> 8),
			              (unsigned char)bestColor, 255);
		}
	}

	img = out;
	return true;
}

// ============================================================================
//  Source -> raster quantifie
// ============================================================================

bool image_to_quantized_image(const std::string& filename,
                              const RegionQuantizeOptions& opt,
                              Img& out)
{
	Img img;
	if (img.load(filename.c_str()) != 0 || img.width() == 0 || img.height() == 0)
	{
		std::fprintf(stderr, "image_region_pipeline: failed to load %s\n", filename.c_str());
		return false;
	}

	// Pre-reduction AVANT le lissage : c'est le seul endroit ou elle economise du
	// travail sur TOUTE la chaine (bilateral et Wu sont les deux etages couteux).
	if (opt.workingMaxDim > 0)
	{
		const unsigned int maxDim = std::max(img.width(), img.height());
		if (maxDim > (unsigned int)opt.workingMaxDim)
		{
			const double k = (double)opt.workingMaxDim / (double)maxDim;
			const unsigned int nw = std::max(1u, (unsigned int)std::lround(img.width()  * k));
			const unsigned int nh = std::max(1u, (unsigned int)std::lround(img.height() * k));
			img.resize(nw, nh, /*mode=*/1);   // bilineaire
		}
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

	// Pixelisation APRES la quantification : le vote majoritaire choisit parmi des
	// couleurs de palette, donc n'en cree aucune. L'inverse (reduire puis
	// quantifier) moyennerait a travers les bords et fabriquerait des teintes
	// absentes de la palette.
	if (opt.pixelWidth > 0)
		pixelize_majority(img, opt.pixelWidth);

	// Clean the labelling BEFORE vectorizing: the vectorizer then never sees the
	// compression hatching, so no speck ever becomes a block and the regions
	// still tile the footprint exactly.
	//
	// APRES la pixelisation : les seuils s'expriment alors dans l'unite de l'image
	// qu'on va vectoriser (cellules de sortie), et non en pixels source ou
	// minRegionArea=12 effacerait des blocs entiers d'une grille 64 de large.
	despeckle(img, opt.despecklePasses, opt.minRegionArea);

	out = img;
	return true;
}

// ============================================================================
//  Couches vectorisees -> XY monde
// ============================================================================

bool region_compute_mapping(const std::vector<VectorLayer>& layers, float fitSize,
                            RegionMapping& m, RegionWorldTransform& w)
{
	// Single pixel -> world transform for every contour: fit the content bbox to
	// fitSize on its longest side (aspect ratio preserved), centre it on the
	// origin, and flip Y so the image is upright in a Y-up frame. Keeping this in
	// one place is what guarantees blocks, base and wall share a coordinate
	// system. The background is vectorized as a full region, so this bbox is the
	// image rectangle.
	bool any = false;
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
	for (const VectorLayer& layer : layers)
		for (const VectorContour& c : layer.contours)
			for (const Vector2f& p : c.pts)
			{
				if (!any) { minX = maxX = p.x; minY = maxY = p.y; any = true; continue; }
				minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
			}
	if (!any) return false;

	const float bw = maxX - minX;
	const float bh = maxY - minY;
	const float largest = std::max(bw, bh);
	if (largest < 1e-9f) return false;

	m.scale = std::max(fitSize, 1e-6f) / largest;
	m.cx = 0.5f * (minX + maxX);
	m.cy = 0.5f * (minY + maxY);

	w.halfW = 0.5f * bw * m.scale;
	w.halfH = 0.5f * bh * m.scale;
	// One millionth of the footprint: fine enough that two distinct contour
	// vertices never collapse, coarse enough to absorb float noise.
	w.quantScale = 1e6f / std::max(fitSize, 1e-6f);
	return true;
}

bool map_layers_to_world(std::vector<VectorLayer>& layers,
                         float fitSize, float shrink,
                         RegionWorldTransform& out)
{
	RegionMapping m;
	if (!region_compute_mapping(layers, fitSize, m, out))
		return false;

	// Retrait des regions, en pixels image, une fois la bbox figee. Une couche
	// dont toutes les regions sont plus fines que 2*shrink se resorbe entierement :
	// elle disparait, et son materiau avec.
	if (shrink > 0.f)
	{
		for (VectorLayer& layer : layers)
			region_shrink_contours(layer.contours, shrink);
		layers.erase(std::remove_if(layers.begin(), layers.end(),
		                            [](const VectorLayer& l) { return l.contours.empty(); }),
		             layers.end());
		if (layers.empty())
			return false;
	}

	for (VectorLayer& layer : layers)
		for (VectorContour& c : layer.contours)
			for (Vector2f& p : c.pts)
				p = m.apply(p);

	return true;
}

// ============================================================================
//  Cadre
// ============================================================================

ExtrudeContour region_make_rect(float hx, float hy, bool isHole)
{
	ExtrudeContour c;
	c.isHole = isHole;
	c.pts.emplace_back(-hx, -hy);
	c.pts.emplace_back( hx, -hy);
	c.pts.emplace_back( hx,  hy);
	c.pts.emplace_back(-hx,  hy);
	return c;
}

void region_append_base(ExtrudedMeshBuilder& builder, unsigned int materialId,
                        const RegionWorldTransform& w, const RegionFrameOptions& f)
{
	const float outer = f.margin + f.wallThickness;
	ExtrudeAppendOptions ao;
	ao.zBottom = 0.f;
	ao.zTop    = f.baseThickness;
	ao.materialId = materialId;
	ao.normalizeOrientation = true;
	std::vector<ExtrudeContour> rect;
	rect.push_back(region_make_rect(w.halfW + outer, w.halfH + outer, false));
	builder.Append(rect, ao);
}

void region_append_wall(ExtrudedMeshBuilder& builder, unsigned int materialId,
                        const RegionWorldTransform& w, const RegionFrameOptions& f)
{
	// Rectangular ring: outer boundary = edge of the base, inner boundary =
	// content + margin (so `margin` is the gap between the content and the INNER
	// wall face). Sits on the base like the blocks do.
	ExtrudeAppendOptions ao;
	ao.zBottom = f.baseThickness;
	ao.zTop    = f.baseThickness + f.wallHeight;
	ao.materialId = materialId;
	ao.normalizeOrientation = true;

	const float inner = f.margin;
	const float outer = f.margin + f.wallThickness;
	std::vector<ExtrudeContour> ring;
	ring.push_back(region_make_rect(w.halfW + outer, w.halfH + outer, false));
	ring.push_back(region_make_rect(w.halfW + inner, w.halfH + inner, true));
	builder.Append(ring, ao);
}
