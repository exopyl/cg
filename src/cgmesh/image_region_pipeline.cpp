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
//  Despeckling of the quantized image
// ============================================================================
//
// Les deux etages sont des primitives d'image (cgimg) : un filtre de MODE 3x3
// puis l'absorption des composantes connexes sous un seuil d'aire. Ni l'un ni
// l'autre ne cree de couleur -- ils ne remplacent une couleur que par une deja
// presente dans le voisinage --, donc l'image reste un pavage complet, ce qui
// garantit que les regions extrudees couvrent exactement l'emprise, sans trou.
// Cf. les commentaires de ImageReliefOptions::despecklePasses pour pourquoi
// l'alternative (filtrer les contours vectorises par aire) ne l'offre pas.

void despeckle(Img& img, int passes, int minRegionArea)
{
	if (passes > 0)
		img.filter_majority(/*radius=*/1, passes);
	if (minRegionArea > 0)
		img.absorb_small_regions(minRegionArea);
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

// ============================================================================
//  Pixelisation par vote majoritaire
// ============================================================================

// Sous-echantillonne vers `targetW` cellules de large, la hauteur suivant le
// rapport d'aspect. Le vote lui-meme est Img::resize mode 3 (bornes de bloc
// exactes) : ici on ne fait que deduire la hauteur et refuser l'agrandissement.
bool pixelize_to_width(Img& img, int targetW)
{
	const int W = (int)img.width();
	const int H = (int)img.height();
	if (targetW <= 0 || W <= 0 || H <= 0) return false;
	if (targetW >= W) return true;             // rien a reduire : on ne dilate pas

	// Hauteur deduite du rapport d'aspect, au moins 1 cellule.
	int targetH = (int)std::lround((double)H * (double)targetW / (double)W);
	if (targetH < 1) targetH = 1;

	return img.resize((unsigned)targetW, (unsigned)targetH, /*mode=*/3) == 0;
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

	// Le raster de cette chaine est OPAQUE par contrat : tout l'etage aval
	// (quantification, anti-mouchetis, vectorisation) raisonne sur des COULEURS, et
	// Palette::IsPresent compare l'alpha -- deux pixels de meme RGB et d'alpha
	// different y comptent pour deux couleurs, donc pour deux materiaux.
	//
	// Or le lissage bilateral perturbe l'alpha d'un LSB : sa moyenne ponderee tombe
	// a 254,9 et la conversion en entier tronque. Sur une source a deux couleurs, on
	// obtenait ainsi quatre entrees de palette (2 RGB x 2 alpha). La normalisation
	// etait jusqu'ici un effet de bord de l'anti-mouchetis, qui reecrivait tous les
	// pixels en a=255 -- donc absente des que despecklePasses et minRegionArea
	// etaient a 0. Elle est desormais explicite et inconditionnelle.
	{
		unsigned char* px = img.data();
		const size_t n = (size_t)img.width() * img.height();
		for (size_t k = 0; k < n; ++k)
			px[4 * k + 3] = 255;
	}

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
		pixelize_to_width(img, opt.pixelWidth);

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
