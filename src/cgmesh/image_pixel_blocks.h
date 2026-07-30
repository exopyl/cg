#pragma once

// ============================================================================
//  Image -> blocs pixelisés, un objet 3D par bloc connexe
// ============================================================================
//
// Variante « pixel art » de image_relief.h. Même chaîne géométrique, une étape
// de plus et deux réglages inversés :
//
//   source -> quantification (Wu, sur l'image pleine)
//          -> PIXELISATION par vote majoritaire vers `pixelWidth` cellules
//          -> segmentation en blocs CONNEXES de couleur identique
//          -> extrusion, un maillage par bloc
//
// Deux choix structurent le résultat :
//
// 1. On quantifie AVANT de pixeliser, et la pixelisation vote au lieu de
//    moyenner. Un sous-échantillonnage par moyenne traverserait les bords et
//    fabriquerait des teintes absentes de la palette — c'est le défaut que
//    Gerstner et al. (Pixelated Image Abstraction, NPAR 2012) identifient dans
//    la chaîne naïve « réduire puis quantifier ». Le vote choisit parmi des
//    couleurs de palette, donc n'en crée aucune.
//
// 2. Les contours sont tracés SANS lissage (Vectorize(..., bSmooth=false)) et
//    sans simplification. Un bloc pixelisé n'a que des arêtes axiales ; les
//    arrondir détruirait précisément ce qu'on cherche à montrer.
//
// La sortie « par composante » donne des pièces séparables : chaque zone
// contiguë d'une même couleur est un solide à part, que l'on peut imprimer,
// trier et assembler. `shrink` creuse le sillon qui leur donne le jeu
// d'assemblage.
//
// ============================================================================

#include <string>
#include <vector>

#include "../cgimg/color.h"
#include "image_region_pipeline.h"   // QuantAlgo + chaîne partagée

class Mesh;

struct ImagePixelBlocksOptions
{
	// --- Pixelisation ---
	// Largeur de sortie en CELLULES (la hauteur suit le rapport d'aspect).
	// Exprimée en largeur cible et non en facteur de réduction, pour que le
	// nombre de blocs — donc le poids du maillage et le temps de calcul — ne
	// dépende pas de la résolution de la source.
	int       pixelWidth    = 64;

	// Plus grande dimension de travail. La source y est ramenée AVANT le lissage
	// et la quantification : pour une sortie de 64 cellules, faire tourner le
	// filtrage bilatéral et Wu sur 12 Mpx est du gaspillage, le vote majoritaire
	// ne peut pas voir plus de détail que ce que la grille échantillonne.
	// 0 désactive.
	int       workingMaxDim = 1024;

	// --- Palette ---
	int       maxColors        = 8;
	QuantAlgo algo             = QuantAlgo::Wu;
	int       preSmoothPasses  = 1;
	int       refineIterations = 3;

	// --- Nettoyage, en CELLULES de sortie (pas en pixels source) ---
	// Désactivés par défaut : le vote majoritaire fait déjà l'essentiel du
	// débruitage, et sur une grille de 64 de large une cellule isolée est un
	// détail légitime de pixel art, pas un mouchetis à effacer.
	int       despecklePasses = 0;
	int       minRegionArea   = 0;

	// --- Retrait des régions (offset négatif), en cellules ---
	// Deux blocs voisins cessent d'être jointifs : il reste entre eux un sillon
	// de 2*shrink, qui les dessine comme des pièces rapportées et laisse une
	// tolérance d'assemblage à l'impression.
	float     shrink = 0.f;

	// --- Échelle et géométrie (mêmes conventions que ImageReliefOptions) ---
	float     fitSize       = 1.0f;
	float     blockHeight   = 0.10f;
	float     baseThickness = 0.05f;
	float     margin        = 0.05f;
	float     wallThickness = 0.03f;
	float     wallHeight    = 0.10f;

	// --- Sortie ---
	bool      emitBase          = true;
	bool      emitWall          = true;
	bool      emitInternalWalls = true;

	Color     baseColor = Color(160, 160, 160);
	Color     wallColor = Color(120, 120, 120);
};

// Maillage d'AFFICHAGE : un seul Mesh, un Material par couleur de palette (plus
// la base et le mur), donc l'image reste lisible à l'écran.
// GetNMaterials() == nCouleurs + emitBase + emitWall.
// Renvoie nullptr en cas d'échec (fichier illisible, rien à vectoriser).
Mesh* image_to_pixel_blocks(const std::string& filename,
                            const ImagePixelBlocksOptions& opt);

// Sortie de FABRICATION : un Mesh par BLOC CONNEXE, puis la base, puis le mur —
// la base et le mur sont TOUJOURS les dernières entrées quand ils sont demandés.
//
// Chaque maillage de bloc porte un unique Material d'indice 0, nommé
// "block_NNNN_color_NN" : NNNN est l'indice du bloc, NN l'indice de palette de sa
// couleur. Le vecteur étant dense (jamais d'entrée nulle), ce nom est le SEUL
// lien fiable entre la position dans le vecteur et le bloc d'origine.
//
// Renvoie un vecteur vide en cas d'échec. L'appelant possède chaque Mesh.
std::vector<Mesh*> image_to_pixel_blocks_per_component(const std::string& filename,
                                                       const ImagePixelBlocksOptions& opt);
