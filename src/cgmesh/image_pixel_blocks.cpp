#include "image_pixel_blocks.h"

#include "extrude_contours.h"
#include "image_vectorization.h"
#include "material.h"
#include "mesh.h"

#include "../cgimg/cgimg.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

// ============================================================================
//  Segmentation en blocs connexes
// ============================================================================

struct Components
{
	int w = 0, h = 0;
	std::vector<int> id;          // par cellule : indice de composante
	int              count = 0;
	std::vector<Color> color;     // par composante : sa couleur
};

// Accesseurs de Color non const -> passage par valeur (Color est trivialement copiable).
bool sameColor(Color a, Color b)
{
	return a.r() == b.r() && a.g() == b.g() && a.b() == b.b();
}

// Etiquetage 4-connexe : Img::label_components (cgimg) fait le parcours ; on ne
// fait ici que le ranger dans la forme attendue par contourComponent (dimensions
// de la grille portees a cote de la carte d'indices).
Components labelComponents(const Img& img)
{
	Components C;
	C.w = (int)img.width();
	C.h = (int)img.height();
	C.count = img.label_components(C.id, &C.color);
	if (C.count < 0)                  // image vide
	{
		C.count = 0;
		C.id.assign((size_t)std::max(0, C.w) * (size_t)std::max(0, C.h), -1);
		C.color.clear();
	}
	return C;
}

// ============================================================================
//  Rattachement d'un contour a sa composante
// ============================================================================
//
// Le vectoriseur rend ses contours dans un repere image qui lui est propre (grille
// demi-pixel, bordure ajoutee). Plutot que de rejouer cette convention -- fragile,
// et qui deviendrait fausse au premier changement dans image_vectorization.cpp --
// on la DEDUIT : l'ensemble des contours pave l'image, donc leur bbox EST le
// rectangle image. Deux facteurs d'echelle et deux decalages suffisent alors a
// passer d'une coordonnee de contour a un indice de cellule.
struct CellMap
{
	float minX = 0.f, minY = 0.f;
	float cellW = 1.f, cellH = 1.f;
	int   w = 0, h = 0;

	// Indice de cellule contenant le point, borne aux limites de la grille.
	void cellAt(const Vector2f& p, int& cx, int& cy) const
	{
		cx = (int)std::floor((p.x - minX) / cellW);
		cy = (int)std::floor((p.y - minY) / cellH);
		cx = std::max(0, std::min(w - 1, cx));
		cy = std::max(0, std::min(h - 1, cy));
	}
};

bool buildCellMap(const std::vector<VectorLayer>& layers, int w, int h, CellMap& out)
{
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
	if (!any || w <= 0 || h <= 0) return false;
	if (maxX - minX < 1e-9f || maxY - minY < 1e-9f) return false;

	out.minX = minX;
	out.minY = minY;
	out.cellW = (maxX - minX) / (float)w;
	out.cellH = (maxY - minY) / (float)h;
	out.w = w;
	out.h = h;
	return true;
}

// Composante a laquelle appartient `contour`, ou -1 si indeterminable.
//
// Aucun raisonnement d'orientation : on sonde LES DEUX cotes de chaque arete et
// on retient celui dont la cellule porte la couleur de la couche. Ca marche
// identiquement pour un contour exterieur (l'interieur est de cette couleur) et
// pour un trou (c'est l'exterieur du trou qui l'est), et ca ne depend d'aucune
// convention de sens de parcours -- donc rien a re-verifier si le vectoriseur
// change la sienne.
int contourComponent(const VectorContour& contour, const Color& layerColor,
                     const CellMap& map, const Components& comps)
{
	const size_t n = contour.pts.size();
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2f& a = contour.pts[i];
		const Vector2f& b = contour.pts[(i + 1) % n];

		float ex = b.x - a.x, ey = b.y - a.y;
		const float len = std::sqrt(ex * ex + ey * ey);
		if (len < 1e-6f) continue;
		ex /= len; ey /= len;

		// Centre de la PREMIERE cellule bordant l'arete : `a` est un coin de
		// cellule, donc un demi-pas le long de l'arete puis un demi-pas le long de
		// la normale tombe exactement sur un centre. Prendre le milieu de l'arete
		// serait ambigu quand elle couvre un nombre pair de cellules (le point
		// atterrirait sur une frontiere).
		const float mx = a.x + 0.5f * ex * map.cellW;
		const float my = a.y + 0.5f * ey * map.cellH;

		// Normale a l'arete, dans les deux sens.
		const float nx = -ey, ny = ex;
		for (int s = -1; s <= 1; s += 2)
		{
			const Vector2f probe(mx + (float)s * 0.5f * nx * map.cellW,
			                     my + (float)s * 0.5f * ny * map.cellH);
			int cx = 0, cy = 0;
			map.cellAt(probe, cx, cy);
			const int id = comps.id[(size_t)cy * comps.w + cx];
			if (id >= 0 && sameColor(comps.color[(size_t)id], layerColor))
				return id;
		}
	}
	return -1;
}

// ============================================================================
//  Assemblage geometrique
// ============================================================================

// Cles d'aretes, pour reperer les parois internes (cf. image_to_pixel_blocks).
typedef std::pair<std::uint64_t, std::uint64_t> EdgeKey;

std::uint64_t vertexKey(const Vector2f& p, float q)
{
	const std::int32_t x = (std::int32_t)std::lround((double)p.x * q);
	const std::int32_t y = (std::int32_t)std::lround((double)p.y * q);
	return ((std::uint64_t)(std::uint32_t)x << 32) | (std::uint64_t)(std::uint32_t)y;
}

// Non orientee : les deux blocs partageant une frontiere la parcourent en sens
// opposes.
EdgeKey edgeKey(const Vector2f& a, const Vector2f& b, float q)
{
	const std::uint64_t ka = vertexKey(a, q);
	const std::uint64_t kb = vertexKey(b, q);
	return (ka <= kb) ? EdgeKey(ka, kb) : EdgeKey(kb, ka);
}

// Un bloc : son contour exterieur et ses trous, plus l'indice de palette de sa
// couleur (pour nommer son materiau).
struct Block
{
	std::vector<VectorContour> contours;
	Color color;
	int   colorIndex = -1;
};

ExtrudeAppendOptions blockOptions(const ImagePixelBlocksOptions& opt, unsigned int materialId)
{
	ExtrudeAppendOptions ao;
	// Les blocs montent depuis la face SUPERIEURE de la plaque, pas depuis z=0.
	ao.zBottom = opt.baseThickness;
	ao.zTop    = opt.baseThickness + opt.blockHeight;
	ao.materialId = materialId;
	ao.winding = ExtrudeWinding::NonZero;
	ao.normalizeOrientation = true;
	ao.emitWalls = true;
	return ao;
}

RegionFrameOptions frameOptions(const ImagePixelBlocksOptions& opt)
{
	RegionFrameOptions f;
	f.baseThickness = opt.baseThickness;
	f.margin        = opt.margin;
	f.wallThickness = opt.wallThickness;
	f.wallHeight    = opt.wallHeight;
	return f;
}

std::vector<ExtrudeContour> toExtrudeContours(const std::vector<VectorContour>& src)
{
	std::vector<ExtrudeContour> out;
	out.reserve(src.size());
	for (const VectorContour& c : src)
	{
		ExtrudeContour ec;
		ec.pts    = c.pts;
		ec.isHole = c.isHole;
		out.push_back(std::move(ec));
	}
	return out;
}

Material* makeColorMaterial(Color color, const std::string& name)
{
	auto* mat = new MaterialColor(color.r(), color.g(), color.b(), color.a());
	mat->SetName(name);
	return mat;
}

std::string blockMaterialName(int blockIndex, int colorIndex)
{
	char buf[64];
	std::snprintf(buf, sizeof(buf), "block_%04d_color_%02d", blockIndex, colorIndex);
	return std::string(buf);
}

// ============================================================================
//  Chaine complete : image -> blocs, en XY monde
// ============================================================================

struct PixelContent
{
	std::vector<Block>   blocks;
	RegionWorldTransform w;
	int                  nColors = 0;
};

RegionQuantizeOptions quantizeOptions(const ImagePixelBlocksOptions& opt)
{
	RegionQuantizeOptions qo;
	qo.maxColors        = opt.maxColors;
	qo.algo             = opt.algo;
	qo.preSmoothPasses  = opt.preSmoothPasses;
	qo.refineIterations = opt.refineIterations;
	qo.despecklePasses  = opt.despecklePasses;
	qo.minRegionArea    = opt.minRegionArea;
	qo.pixelWidth       = std::max(1, opt.pixelWidth);
	qo.workingMaxDim    = opt.workingMaxDim;
	return qo;
}

bool buildContent(const std::string& filename,
                  const ImagePixelBlocksOptions& opt,
                  PixelContent& content)
{
	Img img;
	if (!image_to_quantized_image(filename, quantizeOptions(opt), img))
		return false;

	// Blocs connexes de couleur identique, sur la grille pixelisee.
	const Components comps = labelComponents(img);
	if (comps.count == 0)
	{
		std::fprintf(stderr, "image_pixel_blocks: %s has no region\n", filename.c_str());
		return false;
	}

	// get_palette() renvoie une palette FRAICHE sur le tas pour une image non
	// palettisee (notre cas : la quantification travaille sur le buffer RGBA).
	const bool paletteOwnedByImg = img.uses_palette();
	Palette* pPalette = img.get_palette();
	if (!pPalette || pPalette->NColors() == 0)
	{
		if (!paletteOwnedByImg) delete pPalette;
		std::fprintf(stderr, "image_pixel_blocks: %s has no colour\n", filename.c_str());
		return false;
	}
	content.nColors = pPalette->NColors();

	// simplifyErr NEGATIF + bSmooth=false : trace brut, en escalier exact sur la
	// grille. C'est tout l'interet de la brique ; lisser ou simplifier ici
	// arrondirait les marches.
	CLitRasterToVector rtv;
	const bool ok = rtv.Vectorize(&img, Color(), /*bUseMask=*/false, pPalette,
	                              /*fSimplifyErr=*/-1.f, /*bSmooth=*/false);
	if (!paletteOwnedByImg)
		delete pPalette;
	if (!ok)
	{
		std::fprintf(stderr, "image_pixel_blocks: vectorization failed on %s\n", filename.c_str());
		return false;
	}

	std::vector<VectorLayer> layers = rtv.GetLayers();
	if (layers.empty())
	{
		std::fprintf(stderr, "image_pixel_blocks: %s vectorized to no region\n", filename.c_str());
		return false;
	}

	// Transformation monde figee AVANT tout retrait (cf. region_compute_mapping).
	RegionMapping mapping;
	if (!region_compute_mapping(layers, opt.fitSize, mapping, content.w))
		return false;

	CellMap cellMap;
	if (!buildCellMap(layers, comps.w, comps.h, cellMap))
		return false;

	// Rattache chaque contour a son bloc.
	std::vector<Block> blocks((size_t)comps.count);
	for (int i = 0; i < comps.count; ++i)
		blocks[(size_t)i].color = comps.color[(size_t)i];

	for (const VectorLayer& layer : layers)
		for (const VectorContour& c : layer.contours)
		{
			const int id = contourComponent(c, layer.color, cellMap, comps);
			if (id < 0) continue;          // contour non rattachable : ignore
			blocks[(size_t)id].colorIndex = layer.colorIndex;
			blocks[(size_t)id].contours.push_back(c);
		}

	// Retrait PAR BLOC, et non par couleur : un bloc est exactement un contour
	// exterieur et ses trous, c'est-a-dire le polygone a trous qu'attend Clipper2.
	// Erodee par couleur, une couche a plusieurs blocs verrait ses blocs traites
	// ensemble, ce qui est inutilement global ici.
	for (Block& b : blocks)
	{
		if (b.contours.empty()) continue;
		region_shrink_contours(b.contours, opt.shrink);
	}

	// Passage en XY monde, et abandon des blocs vides (entierement resorbes par le
	// retrait, ou jamais rattaches).
	content.blocks.clear();
	content.blocks.reserve(blocks.size());
	for (Block& b : blocks)
	{
		if (b.contours.empty()) continue;
		for (VectorContour& c : b.contours)
			for (Vector2f& p : c.pts)
				p = mapping.apply(p);
		content.blocks.push_back(std::move(b));
	}

	if (content.blocks.empty())
	{
		std::fprintf(stderr,
		             "image_pixel_blocks: %s has no block left after shrink=%g\n",
		             filename.c_str(), opt.shrink);
		return false;
	}
	return true;
}

} // namespace

// ============================================================================
//  Points d'entree publics
// ============================================================================

Mesh* image_to_pixel_blocks(const std::string& filename,
                            const ImagePixelBlocksOptions& opt)
{
	PixelContent content;
	if (!buildContent(filename, opt, content))
		return nullptr;

	// Un materiau par COULEUR de palette (et non par bloc) : a l'ecran c'est
	// l'image qu'on veut lire, pas la decoupe. La decoupe est le sujet de
	// image_to_pixel_blocks_per_component().
	ExtrudedMeshBuilder builder;
	std::map<int, unsigned int> colorMaterial;   // indice palette -> indice materiau
	std::vector<Color> materialColors;

	// Deux blocs voisins portent chacun sa paroi sur la frontiere commune (faces
	// coincidentes, invisibles). Les compter permet de ne garder que les parois de
	// silhouette : une arete vue DEUX fois est interne. Le vectoriseur donne aux
	// regions adjacentes exactement la meme polyligne, donc l'egalite est exacte.
	std::map<EdgeKey, int> edgeCount;
	if (!opt.emitInternalWalls)
		for (const Block& b : content.blocks)
			for (const VectorContour& c : b.contours)
			{
				const size_t n = c.pts.size();
				for (size_t i = 0; i < n; ++i)
					edgeCount[edgeKey(c.pts[i], c.pts[(i + 1) % n], content.w.quantScale)]++;
			}

	for (const Block& b : content.blocks)
	{
		auto it = colorMaterial.find(b.colorIndex);
		if (it == colorMaterial.end())
		{
			it = colorMaterial.emplace(b.colorIndex, (unsigned int)materialColors.size()).first;
			materialColors.push_back(b.color);
		}
		ExtrudeAppendOptions ao = blockOptions(opt, it->second);
		if (!opt.emitInternalWalls)
		{
			const std::map<EdgeKey, int>* counts = &edgeCount;
			const float q = content.w.quantScale;
			ao.wallFilter = [counts, q](const Vector2f& a, const Vector2f& b2)
			{
				auto it2 = counts->find(edgeKey(a, b2, q));
				return it2 == counts->end() || it2->second < 2;   // arete partagee -> pas de paroi
			};
		}
		builder.Append(toExtrudeContours(b.contours), ao);
	}

	unsigned int nextMaterial = (unsigned int)materialColors.size();
	const unsigned int baseMaterial = nextMaterial;
	if (opt.emitBase)
	{
		region_append_base(builder, nextMaterial++, content.w, frameOptions(opt));
	}
	const unsigned int wallMaterial = nextMaterial;
	if (opt.emitWall)
	{
		region_append_wall(builder, nextMaterial++, content.w, frameOptions(opt));
	}

	if (builder.Empty()) return nullptr;
	Mesh* mesh = builder.Build();
	if (!mesh) return nullptr;

	char name[32];
	for (size_t i = 0; i < materialColors.size(); ++i)
	{
		std::snprintf(name, sizeof(name), "color_%02d", (int)i);
		mesh->Material_Add(makeColorMaterial(materialColors[i], name));
	}
	if (opt.emitBase)
	{
		(void)baseMaterial;
		mesh->Material_Add(makeColorMaterial(opt.baseColor, "base"));
	}
	if (opt.emitWall)
	{
		(void)wallMaterial;
		mesh->Material_Add(makeColorMaterial(opt.wallColor, "wall"));
	}
	return mesh;
}

std::vector<Mesh*> image_to_pixel_blocks_per_component(const std::string& filename,
                                                       const ImagePixelBlocksOptions& opt)
{
	std::vector<Mesh*> out;

	PixelContent content;
	if (!buildContent(filename, opt, content))
		return out;

	int index = 0;
	for (const Block& b : content.blocks)
	{
		ExtrudedMeshBuilder builder;
		builder.Append(toExtrudeContours(b.contours), blockOptions(opt, 0));
		if (builder.Empty()) { ++index; continue; }
		Mesh* mesh = builder.Build();
		if (!mesh) { ++index; continue; }
		mesh->Material_Add(makeColorMaterial(b.color, blockMaterialName(index, b.colorIndex)));
		out.push_back(mesh);
		++index;
	}

	if (out.empty())
		return out;

	// La base et le mur sont de simples rectangles : echouer a en construire un est
	// un bug, pas une condition de donnees -- on fait echouer tout l'appel plutot
	// que de livrer silencieusement un jeu de pieces sans cadre.
	if (opt.emitBase)
	{
		ExtrudedMeshBuilder builder;
		region_append_base(builder, 0, content.w, frameOptions(opt));
		Mesh* mesh = builder.Empty() ? nullptr : builder.Build();
		if (!mesh)
		{
			for (Mesh* m : out) delete m;
			return std::vector<Mesh*>();
		}
		mesh->Material_Add(makeColorMaterial(opt.baseColor, "base"));
		out.push_back(mesh);
	}
	if (opt.emitWall)
	{
		ExtrudedMeshBuilder builder;
		region_append_wall(builder, 0, content.w, frameOptions(opt));
		Mesh* mesh = builder.Empty() ? nullptr : builder.Build();
		if (!mesh)
		{
			for (Mesh* m : out) delete m;
			return std::vector<Mesh*>();
		}
		mesh->Material_Add(makeColorMaterial(opt.wallColor, "wall"));
		out.push_back(mesh);
	}
	return out;
}
