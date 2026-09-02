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

// ============================================================================
//  Front end: image -> colour layers in world XY
// ============================================================================

struct ReliefContent
{
	// Vectorized layers, contour points already mapped to WORLD XY (Y flipped,
	// scaled to fitSize, content centred on the origin).
	std::vector<VectorLayer> layers;

	// Demi-emprise du contenu + pas de quantification des cles d'aretes.
	RegionWorldTransform w;
};

// Etiquette de source pour les diagnostics quand l'image n'a pas de nom de
// fichier -- elle est arrivee par un port ou par un buffer.
const char* const kMemorySource = "<image en memoire>";

// Options de quantification derivees du relief. Le relief ne pixelise pas
// (pixelWidth a 0) ; il PRE-REDUIT en revanche (workingMaxDim).
RegionQuantizeOptions reliefQuantizeOptions(const ImageReliefOptions& opt)
{
	RegionQuantizeOptions qo;
	qo.maxColors        = opt.maxColors;
	qo.algo             = opt.algo;
	qo.preSmoothPasses  = opt.preSmoothPasses;
	qo.refineIterations = opt.refineIterations;
	qo.despecklePasses  = opt.despecklePasses;
	qo.minRegionArea    = opt.minRegionArea;
	qo.pixelWidth       = 0;
	qo.workingMaxDim    = opt.workingMaxDim;
	return qo;
}

// Vectorize an ALREADY QUANTIZED raster and map every contour to world XY.
//
// L'image est prise telle quelle : ni lissage, ni quantification, ni
// anti-mouchetis. Les champs correspondants de `opt` ne sont donc PAS lus ici --
// c'est l'appelant qui a decide de la palette, en amont.
bool buildContent(Img& img,
                  const ImageReliefOptions& opt,
                  ReliefContent& content,
                  const char* source)
{
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
		std::fprintf(stderr, "image_relief: %s has no colour\n", source);
		return false;
	}

	CLitRasterToVector rtv;
	const bool ok = rtv.Vectorize(&img, Color(), /*bUseMask=*/false,
	                              pPalette, opt.simplifyErr);
	if (!paletteOwnedByImg)
		delete pPalette;
	if (!ok)
	{
		std::fprintf(stderr, "image_relief: vectorization failed on %s\n", source);
		return false;
	}

	content.layers = rtv.GetLayers();
	if (content.layers.empty())
	{
		std::fprintf(stderr, "image_relief: %s vectorized to no region\n", source);
		return false;
	}

	// Recadrage / centrage / inversion de Y + retrait des regions : chaine
	// partagee (map_layers_to_world).
	if (!map_layers_to_world(content.layers, opt.fitSize, opt.shrink, content.w))
	{
		std::fprintf(stderr,
		             "image_relief: %s has no region left (shrink=%g px)\n",
		             source, opt.shrink);
		return false;
	}

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
				counts[edgeKey(c.pts[i], c.pts[(i + 1) % n], content.w.quantScale)]++;
		}
	return counts;
}

// ============================================================================
//  Geometry assembly
// ============================================================================

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

// Cadre partage (image_region_pipeline) : plaque de base + mur perimetrique.
RegionFrameOptions frameOptions(const ImageReliefOptions& opt)
{
	RegionFrameOptions f;
	f.baseThickness = opt.baseThickness;
	f.margin        = opt.margin;
	f.wallThickness = opt.wallThickness;
	f.wallHeight    = opt.wallHeight;
	return f;
}

void appendBase(ExtrudedMeshBuilder& builder, unsigned int materialId,
                const ReliefContent& content, const ImageReliefOptions& opt)
{
	region_append_base(builder, materialId, content.w, frameOptions(opt));
}

void appendWall(ExtrudedMeshBuilder& builder, unsigned int materialId,
                const ReliefContent& content, const ImageReliefOptions& opt)
{
	region_append_wall(builder, materialId, content.w, frameOptions(opt));
}

Material* makeColorMaterial(Color color, const std::string& name)
{
	auto* mat = new MaterialColor(color.r(), color.g(), color.b(), color.a());
	mat->SetName(name);
	return mat;
}

// Contenu depuis un FICHIER : quantification puis vectorisation.
bool buildContentFromFile(const std::string& filename,
                          const ImageReliefOptions& opt,
                          ReliefContent& content)
{
	// Chargement, lissage, quantification, raffinement et anti-mouchetis : chaine
	// partagee avec image_pixel_blocks (image_region_pipeline.h).
	Img img;
	if (!image_to_quantized_image(filename, reliefQuantizeOptions(opt), img))
		return false;

	return buildContent(img, opt, content, filename.c_str());
}

// ============================================================================
//  Corps communs -- contenu deja construit -> maillage(s)
// ============================================================================
//
// Extraits pour que les entrees « depuis un fichier » et « depuis une image »
// partagent tout ce qui suit la quantification. `source` ne sert qu'aux
// diagnostics.

Mesh* reliefFromContent(const ReliefContent& content, const ImageReliefOptions& opt,
                        const char* source, const Img* texture)
{
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

	// TEXTURE : toutes les couches partagent le materiau 0, celui de l'image.
	// Les couches restent des couches -- c'est toujours la quantification qui
	// decide de leur decoupe et de leur hauteur --, seule leur couleur change de
	// source. Une couleur par couche n'aurait plus de sens : elles portent
	// toutes la meme texture.
	const bool textured = texture != nullptr;
	const unsigned int nColorMaterials = textured ? 1u : nLayers;

	ExtrudedMeshBuilder builder;
	for (unsigned int i = 0; i < nLayers; ++i)
		appendLayer(builder, content.layers[i], textured ? 0u : i, opt, pInternalEdges,
		            content.w.quantScale);
	// Les ids de materiau suivent l'ordre d'enregistrement plus bas ; un cadre
	// desactive ne consomme donc pas son id.
	unsigned int nextMaterial = nColorMaterials;
	if (opt.emitBase) appendBase(builder, nextMaterial++, content, opt);
	if (opt.emitWall) appendWall(builder, nextMaterial++, content, opt);

	Mesh* m = builder.Build();
	if (!m)
	{
		std::fprintf(stderr, "image_relief: %s produced no geometry\n", source);
		return nullptr;
	}

	char name[32];
	if (textured)
	{
		// Echec de la texture : on retombe sur l'aplat de la premiere couche
		// plutot que de laisser les faces pointer sur un materiau absent.
		Material* mat = region_make_texture_material(*texture, "source");
		if (mat != nullptr) m->Material_Add(mat);
		else if (nLayers > 0) m->Material_Add(makeColorMaterial(content.layers[0].color, "color_00"));
		else m->Material_Add(makeColorMaterial(opt.baseColor, "color_00"));

		// UV sur tout le maillage. La base et le mur debordent du contenu, mais
		// gardent leur materiau : leurs UV hors [0, 1] ne sont pas echantillonnes.
		region_apply_planar_uvs(*m, content.w);
	}
	else
	{
		for (unsigned int i = 0; i < nLayers; ++i)
		{
			std::snprintf(name, sizeof(name), "color_%02u", i);
			m->Material_Add(makeColorMaterial(content.layers[i].color, name));
		}
	}
	if (opt.emitBase) m->Material_Add(makeColorMaterial(opt.baseColor, "base"));
	if (opt.emitWall) m->Material_Add(makeColorMaterial(opt.wallColor, "wall"));

	m->computebbox();
	return m;
}

std::vector<Mesh*> reliefLayersFromContent(const ReliefContent& content,
                                           const ImageReliefOptions& opt,
                                           const char* source)
{
	std::vector<Mesh*> meshes;

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
		appendLayer(builder, content.layers[i], 0, opt, pInternalEdges, content.w.quantScale);
		Mesh* m = builder.Build();
		if (!m)
		{
			std::fprintf(stderr,
			             "image_relief: %s layer %zu tessellated to nothing, omitted\n",
			             source, i);
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

	if ((opt.emitBase && !appendFrame(appendBase, opt.baseColor, "base")) ||
	    (opt.emitWall && !appendFrame(appendWall, opt.wallColor, "wall")))
	{
		std::fprintf(stderr, "image_relief: %s failed to build the base/wall frame\n",
		             source);
		for (Mesh* m : meshes) delete m;   // pas de demi-resultat
		meshes.clear();
		return meshes;
	}

	return meshes;
}

} // namespace

// ============================================================================
//  Public entry points
// ============================================================================
//
// Quatre entrees pour deux operations, chacune declinee DEPUIS UN FICHIER et
// DEPUIS UNE IMAGE DEJA QUANTIFIEE.
//
// La seconde forme existe pour les appelants qui ne detiennent pas de fichier et
// qui ont deja decide de la palette en amont : un noeud de graphe dont l'image
// arrive par un port, la cible WebAssembly ou les octets viennent du JavaScript.
// Elle NE QUANTIFIE PAS -- voir la note de image_relief.h sur les champs de `opt`
// qu'elle ne lit pas.

Mesh* image_to_relief(const std::string& filename, const ImageReliefOptions& opt,
                      bool textureFromSource)
{
	ReliefContent content;
	if (!buildContentFromFile(filename, opt, content))
		return nullptr;

	// Seconde lecture du fichier : la premiere n'a rendu que l'image QUANTIFIEE,
	// dont les aplats sont ce qu'on cherche a depasser. Img::load rend 0 en
	// succes ; une texture illisible retombe sur les aplats.
	Img sourceImg;
	if (textureFromSource && sourceImg.load(filename.c_str()) == 0
	    && sourceImg.width() > 0 && sourceImg.height() > 0)
		return reliefFromContent(content, opt, filename.c_str(), &sourceImg);

	return reliefFromContent(content, opt, filename.c_str(), nullptr);
}

Mesh* image_to_relief(const Img& quantized, const ImageReliefOptions& opt,
                      const Img* texture)
{
	// COPIE DELIBEREE. CLitRasterToVector::Vectorize palettise son entree, donc la
	// MODIFIE. Une image qui arrive ici par un lien de graphe est partagee entre
	// tous les consommateurs de ce lien : la muter les corromprait tous.
	Img work = quantized;

	ReliefContent content;
	if (!buildContent(work, opt, content, kMemorySource))
		return nullptr;

	return reliefFromContent(content, opt, kMemorySource, texture);
}

std::vector<Mesh*> image_to_relief_per_color(const std::string& filename,
                                             const ImageReliefOptions& opt)
{
	ReliefContent content;
	if (!buildContentFromFile(filename, opt, content))
		return std::vector<Mesh*>();

	return reliefLayersFromContent(content, opt, filename.c_str());
}

std::vector<Mesh*> image_to_relief_per_color(const Img& quantized,
                                             const ImageReliefOptions& opt)
{
	Img work = quantized;   // meme motif que ci-dessus

	ReliefContent content;
	if (!buildContent(work, opt, content, kMemorySource))
		return std::vector<Mesh*>();

	return reliefLayersFromContent(content, opt, kMemorySource);
}
