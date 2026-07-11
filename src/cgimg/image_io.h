#pragma once

// Import / export (serialization) logic for Img.
//
// All methods are static and take an Img by reference as the first parameter.
// ImgIO is a friend of Img (see image.h), so these helpers may call Img's
// private buffer helpers (resize_memory) and touch its state directly. This
// keeps the image container (Img) free of format-specific I/O code.
//
// Img::load / Img::save remain as thin public delegators that forward here.
//
// image.h is included (not just forward-declared) so that FILE and the PNG/JPG
// feature macros are visible to the guarded declarations below.
#include "image.h"

class ImgIO
{
public:
	static int load (Img& img, char const *filename, char const *path = nullptr);
	static int save (Img& img, char const *filename);

private:
	static int import_bmp (Img& img, const char *filename);
	static int export_bmp (Img& img, const char *filename);
	static int import_tga (Img& img, const char *filename);
	static void compute_colormap (Img& img, unsigned char **_colormap, unsigned short *_colormap_length);
	static int export_tga (Img& img, const char *filename);
	static int import_pbm (Img& img, FILE *ptr, unsigned int levels, int binary);
	static int import_pgm (Img& img, FILE *ptr, unsigned int levels, int binary);
	static int import_ppm (Img& img, FILE *ptr, unsigned int levels, int binary);
	static int import_pnm (Img& img, const char *filename);
	static int export_ppm (Img& img, const char *filename, int binary);
	static int export_pnm (Img& img, const char *filename);
#ifdef PNG
	static int import_png (Img& img, const char *filename);
	static int export_png (Img& img, const char *filename);
#endif
#ifdef JPG
	static int import_jpg (Img& img, const char *filename);   // stb : import seul (pas d'encodeur)
#endif
};
