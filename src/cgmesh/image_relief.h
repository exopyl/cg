#pragma once

// ============================================================================
//  Image -> coloured relief by region extrusion
// ============================================================================
//
// Turn a raster image into a 3D solid: quantize to at most N colours (Wu),
// vectorize each colour region into contours (holes included), then extrude
// every region as a block of UNIFORM height rising from the top face of a base
// plate. The content is framed by a plate wider than the image (margin) bordered
// by a perimeter wall.
//
//        z
//        ^                          wallHeight
//        |        +--+  +--+           +--+   <- wall (rectangular ring)
//        |    +---+R +--+ V+---+       |  |
//        | H  |   +--+  +--+   |       |  |   <- blocks (top coplanar at
//        |    |    content     |       |  |      z = baseThickness + H)
//        +----+---------------------+--+--+--- z = baseThickness
//        |    ###########################      <- solid base plate
//        +------------------------------------ z = 0
//             |<-- content -->|<-margin->|<-wallThickness->|
//
// Uniform height + background included as a full region means the top of the
// relief is a single plane: what tells the regions apart is the COLOUR (one
// Material per colour), not the altitude.
//
// Frame: Y-up, image upright (the image Y axis is flipped). The content is
// centred on the origin in XY.
//
// Nothing here is a new algorithm — it wires together Img::quant_wu
// (cgimg/image.h), CLitRasterToVector (image_vectorization.h) and
// ExtrudedMeshBuilder (extrude_contours.h).
//
// ============================================================================

#include <string>
#include <vector>

#include "../cgimg/color.h"
#include "image_region_pipeline.h"   // QuantAlgo + chaîne partagée avec image_pixel_blocks

class Mesh;

struct ImageReliefOptions
{
	// --- Quantization / vectorization ---
	int       maxColors     = 16;            // maximum number of colours
	QuantAlgo algo          = QuantAlgo::Wu;
	float     simplifyErr   = 1.0f;          // contour simplification (px)

	// Edge-preserving smoothing passes applied BEFORE quantization
	// (Img::bilateral_filtering). 0 disables.
	//
	// This is the fidelity knob, and it addresses the CAUSE rather than the
	// symptom. Real inputs are resampled (the poster JPEG measured here carries
	// Photoshop EXIF and asymmetric 98x106 dpi), so every edge arrives
	// anti-aliased: only 32.4% of its pixels are exactly a palette colour and
	// 17.8% sit more than 20 units away from any of them. Deciding those
	// ambiguous pixels one at a time makes the region boundary wander pixel by
	// pixel — the segmentation stops following the shapes of the original.
	// Bilateral smoothing first collapses each anti-aliased ramp coherently
	// toward one side while keeping the edge itself, so the boundary lands on a
	// clean curve. Measured on that image at 4 colours: 533 -> 264 connected
	// regions, and the cheek boundaries become smooth curves instead of hatching.
	int       preSmoothPasses = 1;

	// Iterations de raffinement de Lloyd (k-means) de la palette après
	// quantification (Img::quant_refine). 0 désactive.
	//
	// Wu et Heckbert décident tous deux sur un histogramme 5 bits/canal ; le
	// raffinement rejuge chaque pixel en RGB 8 bits pleins et recalcule chaque
	// couleur comme la moyenne de ses pixels. Il réduit la MSE de façon monotone
	// et, accessoirement, EFFACE l'écart entre les deux algorithmes : mesuré à 4
	// couleurs, Wu 199 -> 187 et Heckbert 869 -> 187, soit la même palette.
	// Convergence rapide : 1 itération suffit à 4 couleurs, ~3 à 16.
	int       refineIterations = 3;

	// --- Despeckling ---
	//
	// A lossy source is the normal case and it wrecks region boundaries. JPEG
	// block ringing blurs a clean poster edge into a gradient; a hard N-colour
	// threshold then turns that gradient into 1-px dithered hatching. Measured on
	// a JPEG of a 4-colour poster (375x564, quant_wu(4)): 533 connected regions
	// where the poster has a handful, 235 of them a single pixel.
	//
	// BOTH filters below run on the LABEL image (the quantized picture), before
	// vectorization, and only ever REPLACE a label by one of its neighbours'.
	// They therefore cannot open a hole: the labelling stays a complete tiling,
	// so the extruded regions still cover the footprint exactly.
	//
	// (Filtering vectorized CONTOURS by area instead does not work: 340 of the
	// 497 small regions above straddle two colours, so no neighbour carries a
	// matching hole contour and deleting them punches real voids in the surface.)

	// Passes of a 3x3 majority (mode) filter. This is what removes the 1-px
	// hatching: a thin structure loses the vote against its surroundings whatever
	// its length, which an area threshold cannot achieve. 0 disables.
	int       despecklePasses = 1;

	// After the majority passes, any connected region still smaller than this
	// (in px of the source image) is absorbed into its most frequent neighbouring
	// colour. Catches compact blobs the 3x3 window is too small to outvote.
	// 0 disables.
	int       minRegionArea = 12;

	// --- Retrait des régions (offset négatif) ---
	//
	// Après vectorisation, chaque polygone de contour est érodé de `shrink` px
	// vers l'intérieur (Clipper2 InflatePaths, delta négatif). Deux blocs voisins
	// cessent d'être jointifs : il reste entre eux un sillon de 2*shrink, qui
	// dessine les régions comme des pièces rapportées et laisse une tolérance
	// d'assemblage à l'impression. 0 désactive (défaut : géométrie inchangée).
	//
	// En PIXELS de la source, comme simplifyErr et minRegionArea. La bbox du
	// contenu est mesurée AVANT le retrait, donc ce paramètre n'affecte ni
	// l'échelle finale ni la position du cadre.
	//
	// Conséquences voulues : une région plus fine que 2*shrink disparaît (et sa
	// couche avec, si elle n'a rien d'autre) ; les blocs ne partageant plus
	// d'arête, emitInternalWalls n'a plus rien à dédupliquer et tous les murs
	// sont émis — ce sont désormais les flancs visibles des sillons.
	float     shrink = 0.0f;

	// --- XY scaling ---
	// The image footprint (content) is scaled so its largest XY dimension equals
	// fitSize, preserving the image aspect ratio. Every length below is expressed
	// in that same world unit.
	float     fitSize       = 1.0f;

	// --- Relief geometry ---
	float     blockHeight   = 0.10f;         // uniform block height H above the base
	float     baseThickness = 0.05f;         // base plate thickness
	float     margin        = 0.05f;         // gap between content and the INNER wall face
	float     wallThickness = 0.03f;         // perimeter wall thickness
	float     wallHeight    = 0.10f;         // wall height (default = blockHeight)

	// --- Output ---
	// Blocks are closed solids, so two adjacent blocks each carry their own wall
	// on the shared boundary (coincident, hidden faces). Set false to drop those
	// internal walls: the vectorizer gives adjacent regions the exact same
	// boundary polyline, so shared edges are detected exactly and the outer
	// silhouette walls are kept. Lighter, cleaner mesh; blocks are then no longer
	// closed on their own.
	bool      emitInternalWalls = true;

	// --- Base / wall materials ---
	Color     baseColor = Color(160, 160, 160);   // neutral greys
	Color     wallColor = Color(120, 120, 120);
};

// Multi-colour output (default): ONE mesh carrying one Material per colour, plus
// one for the base and one for the wall, so GetNMaterials() == nColours + 2.
// Faces are stamped with their material id; Mesh::BuildPolygonRenderData()
// groups them into materialRanges on its own (single VBO, one draw per material).
// Returns nullptr on failure (missing file, nothing vectorized).
Mesh* image_to_relief(const std::string& filename, const ImageReliefOptions& opt);

// One mesh per colour: each colour is a separate solid.
//
// Order: the colour meshes first, in palette-index order, then the base, then the
// wall — the base and wall are ALWAYS the last two entries. Each mesh carries a
// single Material at index 0.
//
// The vector is always DENSE (never a null entry), so a colour layer that
// tessellates to nothing is OMITTED: index i does NOT necessarily denote colour i.
// Read the palette index from each colour mesh's Material(0) name, which is
// "color_NN" with NN the palette index — that mapping is always exact. (Returning
// nulls to hold the positions was the alternative; it just moves the trap into
// every caller's loop.)
//
// Returns an empty vector on failure. The base and the wall are plain rectangles,
// so failing to build one of them is a bug rather than a data condition: it fails
// the whole call instead of silently shipping a frameless relief.
// Caller owns every returned Mesh.
std::vector<Mesh*> image_to_relief_per_color(const std::string& filename,
                                             const ImageReliefOptions& opt);
