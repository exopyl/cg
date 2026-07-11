#pragma once

#include <stdio.h>
#include "../cgmath/cgmath.h"
#include "palette.h"


#define PNG
#define JPG       // JPEG import via stb_image (header-only, sans dépendance libjpeg)

class Img
{
public:
	// Import / export (serialization) logic lives in ImgIO (image_io.h), a friend
	// so it can call Img's private helpers (resize_memory, compute_colormap).
	friend class ImgIO;

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

	void convert_to_grayscale (grayscale_method_type grayscale_method_id = GRAYSCALE_LUMINOSITY, unsigned char nlevels = 255);

	// test images
	void init_test_grayscale1 (unsigned int h);
	void init_test_grayscale2 (unsigned int size);
	void init_test_color_jet (unsigned int w, unsigned int h);

	//
	// histogram
	//
	void get_histogram (float histogram[256], int normalized = 1);
	Img* get_histogram_img (unsigned int height);
	void histogram_equalization (void);
	void histogram_equalization_bezier (CurveBezier *bezier = nullptr);
	void invert (void);
	void contrast (float k);

	// 
	int get_mean_value (void);
	int get_median_value (void);

	int multiply (Img *pImg);

	// filters
	int filter (float m[3][3], float divide = 0., float decay = 0.);
	int filter_sobel (void);
	int gaussian_blur (void);
	int blur (void);
	int bilateral_filtering (void);
	int saturate (float t);
	int brightness (float t);
	int gamma (float t);
	int sepia (void);

	//
	// quantization
	//
	int quant_heckbert (int ncolors);
	int quant_wu (int ncolors);
	int quant_kmean (float threshold);

	//
	// binarization
	//
	int bin_threshold (int threshold);
	int bin_random (int methodId);
	int bin_floyd_steinberg (void);
	int bin_otsu (void);
	int bin_dithering (unsigned char *pattern, int psize);
	int bin_screening (Img *pPattern);

	// geodesic
	int geodesic (void);

	//
	// crop / resample
	//
	int crop (Img *pImg, int x, int y, unsigned int width, unsigned int height);
	int resize (unsigned int width, unsigned int height, int mode=1); // 0 : nearest neighbour / 1 : bilinear / 2 : most current color in superpixel
	int resize_pixel (unsigned int n);

	int copy (unsigned int x, unsigned int y, Img *pImg);
	int concatenate (Img *pImg);

	int rotate (int mode);

	int resize_canvas (unsigned int width, unsigned int height, int positioning,
			   unsigned char bg_r, unsigned char bg_g, unsigned char bg_b, unsigned char bg_a);

	//
	// drawing functions
	//
	int draw_horizontal_line (unsigned int y, unsigned int xstart, unsigned int xend,
				  unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	int draw_line (unsigned int xtart, unsigned int ystart, unsigned int xend, unsigned int yend,
		       unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	int draw_circle (unsigned int x0, unsigned int y0, unsigned int radius,
			 unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	int draw_disk (unsigned int x0, unsigned int y0, unsigned int radius,
		       unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	int draw_ellipse (unsigned int x0, unsigned int y0,
			  unsigned int radiusx, unsigned int radiusy,
			  unsigned char r, unsigned int g, unsigned char b, unsigned char a);

	// smooth the transitions between black and white
	int smooth_transition (int l);

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
