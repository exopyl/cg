#pragma once

#include <stdio.h>
#include <vector>
#include "../cgmath/cgmath.h"
#include "palette.h"


// Les décodeurs PNG / JPEG (stb_image) sont gardés par CGIMG_WITH_PNG /
// CGIMG_WITH_JPG, définies par le CMakeLists de cgimg (options du même nom, ON par
// défaut) et PRIVATE : elles ne concernent que les .cpp de la bibliothèque.
//
// Elles s'appelaient auparavant PNG / JPG et étaient #define ici même, ce qui avait
// deux défauts : la définition CMake en devenait inerte (cet en-tête étant inclus
// avant tout #ifdef, la fonctionnalité ne pouvait pas être désactivée), et deux
// macros aux noms très communs fuyaient dans chaque unité de compilation incluant
// cgimg.

class Img
{
public:
	// Les traitements sont hors d'Img, dans des classes dediees par domaine, sur le
	// modele historique d'ImgIO : methodes statiques prenant l'Img en premier
	// parametre, declarees amies pour atteindre le tampon de pixels et les aides
	// privees (resize_memory, compute_colormap).
	//
	// Objectif : ajouter un algorithme ne doit plus imposer de modifier CET en-tete,
	// donc de recompiler tous les consommateurs de cgimg. Img ne garde que ce qui
	// releve du conteneur (pixels, palette, geometrie, E/S, colorimetrie de base).
	//
	//   ImgIO          image_io.h            import / export
	//   ImgFilter      image_filter.h        convolution, flous, colorimetrie
	//   ImgBinarize    image_binarization.h  seuillage, tramage
	//   ImgDraw        image_drawing.h       primitives 2D
	//   ImgQuantize    image_quantization.h  quantification de couleurs
	//   ImgGeodesic    image_geodesic.h      transformee geodesique
	//   ImgHistogram   image_histogram.h     histogramme, egalisation
	//   ImgTestPattern image_test_pattern.h  mires de test
	friend class ImgIO;
	friend class ImgFilter;
	friend class ImgBinarize;
	friend class ImgDraw;
	friend class ImgQuantize;
	friend class ImgGeodesic;
	friend class ImgHistogram;
	friend class ImgTestPattern;

	static int AreIdentical (Img *pImg1, Img *pImg2);

	enum grayscale_method_type
	{
		GRAYSCALE_LUMINOSITY,
		GRAYSCALE_LIGHTNESS,
		GRAYSCALE_AVERAGE
	};
	Img (unsigned int w=0, unsigned int h=0, bool use_palette=false);
	Img (const Img &img);
	Img &operator= (const Img &img);   // règle des 3/5 : copie profonde (cf. ~Img/copie)
	~Img ();

	int load (char const *filename, char const *path = nullptr);
	int save (char const *filename);

	// getters / setters
	inline unsigned int width (void) const { return m_iWidth; };
	inline unsigned int height (void) const { return m_iHeight; };

	// Raw pixel buffer (RGBA8, interleaved, width*height*4 bytes). Exposed so
	// external consumers (GL upload, memcpy, scanline walks) keep working now
	// that the members are private; the RGBA8 layout is the documented contract.
	inline unsigned char*       data (void)       { return m_pPixels; }
	inline const unsigned char* data (void) const { return m_pPixels; }
	inline bool uses_palette (void) const { return bUsePalette != 0; }

	int init_color (unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void set_pixel (unsigned int i, unsigned int j,
			unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void set_pixel_int (unsigned int i, unsigned int j, int c);
	void set_pixel_index (unsigned int i, unsigned int j, unsigned int index);
	void get_pixel (unsigned int i, unsigned int j,
			unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) const;
	int get_pixel_int (unsigned int i, unsigned int j);
	int get_pixel_index (unsigned int i, unsigned int j);
	unsigned char get_r (unsigned int i, unsigned int j);
	unsigned char get_g (unsigned int i, unsigned int j);
	unsigned char get_b (unsigned int i, unsigned int j);
	unsigned char get_a (unsigned int i, unsigned int j);
	void get_nearest_pixel (float u, float v,
				unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a);
	
	//
	// palette
	//
	int palettize (Img *pImg);
	Palette* get_palette (void);
	int flood_fill (unsigned int x, unsigned int y, unsigned char r, unsigned char g, unsigned char b);

	// Étiquetage des composantes connexes 4-connexes de couleur identique (RGB ;
	// l'alpha n'entre pas dans l'égalité). `labels` reçoit un indice de composante
	// par pixel, dans l'ordre de première rencontre en balayage raster ; `colors`,
	// s'il est fourni, reçoit la couleur de chaque composante. Renvoie le nombre de
	// composantes, ou -1 si l'image est vide.
	//
	// Complète flood_fill, qui REMPLIT sans étiqueter : il ne rend aucune carte de
	// composantes exploitable.
	int label_components (std::vector<int>& labels, std::vector<Color>* colors = nullptr) const;

	void convert_to_grayscale (grayscale_method_type grayscale_method_id = GRAYSCALE_LUMINOSITY, unsigned char nlevels = 255);

	// Mires de test -> ImgTestPattern (image_test_pattern.h)
	// Histogramme   -> ImgHistogram   (image_histogram.h)

	void invert (void);
	void contrast (float k);

	// 
	int get_mean_value (void);
	int get_median_value (void);

	int multiply (Img *pImg);

	// Filtres et colorimetrie -> ImgFilter   (image_filter.h)
	// Quantification          -> ImgQuantize (image_quantization.h)
	// Binarisation            -> ImgBinarize (image_binarization.h)
	// Geodesique              -> ImgGeodesic (image_geodesic.h)

	//
	// crop / resample
	//
	int crop (Img *pImg, int x, int y, unsigned int width, unsigned int height);
	// mode 0 : nearest neighbour
	//      1 : bilinear
	//      2 : couleur majoritaire du superpixel, pas CONSTANT (m_iWidth/width en
	//          division entière) -- les dernières colonnes/lignes de la source ne
	//          sont pas lues quand les dimensions ne se divisent pas. Historique.
	//      3 : couleur majoritaire du superpixel, bornes de bloc EXACTES
	//          [i*W/w, (i+1)*W/w) -- couvre le reste de la division, contrairement
	//          au mode 2. Égalité départagée par valeur RGB croissante, pour que le
	//          résultat ne dépende pas de l'ordre de parcours.
	int resize (unsigned int width, unsigned int height, int mode=1);
	int resize_pixel (unsigned int n);

	int copy (unsigned int x, unsigned int y, Img *pImg);
	int concatenate (Img *pImg);

	int rotate (int mode);

	int resize_canvas (unsigned int width, unsigned int height, int positioning,
			   unsigned char bg_r, unsigned char bg_g, unsigned char bg_b, unsigned char bg_a);

	// Primitives de trace + smooth_transition -> ImgDraw (image_drawing.h)

private:
	void copyFrom (const Img &img);   // copie profonde partagée (ctor de copie + operator=)
	int resize_memory (unsigned int width, unsigned int height, bool use_palette=false);
	// The per-format import_*/export_* helpers (+ compute_colormap) were moved to
	// ImgIO (image_io.h). Img::load/save remain below as thin delegators.

private:
	// Representation (RGBA8 interleaved). Accessed externally only through
	// data()/width()/height()/get_palette()/uses_palette(); friends (ImgIO) and
	// Img's own methods use them directly.
	unsigned char *m_pPixels;
	unsigned int m_iWidth, m_iHeight;

	// use a palette
	int bUsePalette;
	Palette *m_pPalette;
};
