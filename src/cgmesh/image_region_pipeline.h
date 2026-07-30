#pragma once

// ============================================================================
//  Tronc commun des briques « image -> régions extrudées »
// ============================================================================
//
// Deux briques partagent cette chaîne :
//
//   image_relief.h        image -> relief coloré (contours lissés, une plaque)
//   image_pixel_blocks.h  image -> blocs pixelisés (contours en escalier, N objets)
//
// Ce qu'elles partagent, et qui vit ici :
//
//   1. image_to_quantized_image() -- de la source au raster quantifié, prêt à
//      vectoriser. C'est là que l'ORDRE des étapes est fixé, et cet ordre porte
//      des décisions non évidentes (cf. le commentaire de la fonction). Deux
//      copies divergeraient au premier réglage ; d'où l'extraction.
//   2. map_layers_to_world() -- pixels source -> XY monde (recadrage, centrage,
//      inversion de Y), plus le retrait optionnel des régions.
//   3. append_base() / append_wall() -- le cadre (plaque + mur périmétrique).
//
// ============================================================================

#include <string>
#include <vector>

#include "extrude_contours.h"      // ExtrudeContour, ExtrudedMeshBuilder
#include "image_vectorization.h"   // VectorLayer

class Img;

// Wu is the one to use. Heckbert (median cut) is kept for comparison only: it
// cuts at the MEDIAN of a box's longest axis, which on a bimodal distribution
// slices through the middle of the mass instead of passing BETWEEN the clusters,
// and it averages over a 5-bit histogram where Wu accumulates its moments on the
// original 8-bit values. Measured MSE against the source on a resampled
// 4-colour poster: Wu 199 / Heckbert 869 at 4 colours, still 36 vs 106 at 16 —
// 2.9x to 5.4x worse across the range. Visible as a washed-out palette (red
// drifting to brown, light blue dulled).
enum class QuantAlgo { Wu, Heckbert };

// Img::palettize() stores ONE BYTE per pixel and CLitRasterToVector reserves the
// index right after the palette for its border ring, so the palette must stay
// well under 256 entries.
extern const int kMaxPaletteColors;

// ---------------------------------------------------------------------------
//  1. Source -> raster quantifié
// ---------------------------------------------------------------------------
struct RegionQuantizeOptions
{
	int       maxColors        = 16;
	QuantAlgo algo             = QuantAlgo::Wu;

	// Passes de lissage préservant les bords (Img::bilateral_filtering) AVANT
	// quantification. 0 désactive. Voir ImageReliefOptions::preSmoothPasses pour
	// la mesure qui justifie ce réglage.
	int       preSmoothPasses  = 1;

	// Itérations de raffinement de Lloyd de la palette (Img::quant_refine).
	int       refineIterations = 3;

	// Anti-mouchetis sur l'image de LABELS. Exprimé en pixels de l'image sur
	// laquelle il s'applique -- donc en CELLULES de sortie quand pixelWidth != 0.
	int       despecklePasses  = 1;
	int       minRegionArea    = 12;

	// Pixelisation : largeur cible en cellules. 0 = pas de pixelisation.
	//
	// Appliquée APRÈS la quantification, par VOTE MAJORITAIRE : chaque cellule
	// prend la couleur la plus représentée de son bloc source. Aucune couleur
	// nouvelle n'est donc créée et la palette reste exactement celle qu'a décidée
	// Wu. Un sous-échantillonnage par MOYENNE ferait l'inverse : il moyennerait à
	// travers les bords et fabriquerait des teintes absentes de la palette, ce que
	// la littérature (Gerstner et al. 2012) donne comme le défaut central de la
	// chaîne naïve « réduire puis quantifier ».
	int       pixelWidth       = 0;

	// Pré-réduction de la source avant tout traitement : si sa plus grande
	// dimension dépasse cette valeur, l'image est ramenée à cette taille
	// (bilinéaire). 0 désactive.
	//
	// Pour une sortie de 64 cellules, faire tourner le filtrage bilatéral et Wu
	// sur 12 Mpx est du gaspillage : le vote majoritaire ne peut de toute façon
	// pas voir plus de détail que ce que la grille de sortie échantillonne.
	int       workingMaxDim    = 0;
};

// Charge, lisse, quantifie, raffine, pixelise et nettoie. `out` reçoit le raster
// quantifié (non palettisé : la quantification travaille sur le buffer RGBA).
// Renvoie false si l'image est illisible ou vide.
bool image_to_quantized_image(const std::string& filename,
                              const RegionQuantizeOptions& opt,
                              Img& out);

// Sous-échantillonnage par vote majoritaire vers `targetW` cellules de large (la
// hauteur suit le rapport d'aspect). Exposé pour les tests.
//
// Les bornes de bloc sont exactes -- [i*W/w, (i+1)*W/w) -- donc la dernière
// colonne/ligne couvre bien le reste de la division. Départage déterministe par
// valeur RGB croissante : sans lui, deux couleurs à égalité feraient dépendre le
// résultat de l'ordre de parcours.
bool pixelize_majority(Img& img, int targetW);

// ---------------------------------------------------------------------------
//  2. Couches vectorisées -> XY monde
// ---------------------------------------------------------------------------
struct RegionWorldTransform
{
	float halfW      = 0.f;   // demi-emprise du contenu, en unités monde
	float halfH      = 0.f;
	float quantScale = 1.f;   // unité monde d'un pas de quantification (clés d'arêtes)
};

// Transformation affine pixels image -> XY monde, figée sur la bbox du contenu.
struct RegionMapping
{
	float cx = 0.f, cy = 0.f;   // centre de la bbox, en pixels image
	float scale = 1.f;
	Vector2f apply(const Vector2f& p) const
	{
		// Y inversé : image vers le bas, monde vers le haut.
		return Vector2f((p.x - cx) * scale, -(p.y - cy) * scale);
	}
};

// Calcule la transformation depuis la bbox des contours : recadrage sur `fitSize`
// (plus grand côté, rapport d'aspect préservé) et centrage sur l'origine.
//
// À appeler AVANT tout retrait, volontairement : ainsi `shrink` ne fait que
// creuser des sillons et ne change NI l'échelle finale NI la position du cadre.
// Sinon, rétrécir la couche de fond rétrécirait l'empreinte, que le recadrage
// regrandirait ensuite -- le paramètre se mordrait la queue.
bool region_compute_mapping(const std::vector<VectorLayer>& layers, float fitSize,
                            RegionMapping& m, RegionWorldTransform& w);

// Érode d'un offset négatif UN polygone à trous (un contour extérieur et ses
// trous), en pixels image.
//
// Un seul appel Clipper2 pour tout l'ensemble : sur un polygone à trous, un delta
// négatif érode le contour extérieur ET dilate les trous, c'est-à-dire retire de
// la matière des deux côtés. Contour par contour, les trous seraient rognés au
// lieu d'être élargis, et la région grossirait par l'intérieur.
void region_shrink_contours(std::vector<VectorContour>& contours, float shrink);

// Enchaîne les trois : mapping, retrait par COUCHE, application. Utilisé par le
// relief, dont l'unité de retrait est la couche de couleur. (image_pixel_blocks
// retire par BLOC CONNEXE, et compose donc les briques ci-dessus lui-même.)
bool map_layers_to_world(std::vector<VectorLayer>& layers,
                         float fitSize, float shrink,
                         RegionWorldTransform& out);

// ---------------------------------------------------------------------------
//  3. Cadre : plaque de base + mur périmétrique
// ---------------------------------------------------------------------------
struct RegionFrameOptions
{
	float baseThickness = 0.05f;
	float margin        = 0.05f;   // jeu entre le contenu et la face INTERNE du mur
	float wallThickness = 0.03f;
	float wallHeight    = 0.10f;
};

// Rectangle centré sur l'origine, CCW. L'extrudeur normalise l'orientation
// depuis isHole, donc ce drapeau seul exprime l'intention.
ExtrudeContour region_make_rect(float hx, float hy, bool isHole);

void region_append_base(ExtrudedMeshBuilder& builder, unsigned int materialId,
                        const RegionWorldTransform& w, const RegionFrameOptions& f);

void region_append_wall(ExtrudedMeshBuilder& builder, unsigned int materialId,
                        const RegionWorldTransform& w, const RegionFrameOptions& f);
