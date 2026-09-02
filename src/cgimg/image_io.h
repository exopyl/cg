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
// image.h is included (not just forward-declared) so that FILE is visible. The
// CGIMG_WITH_PNG / CGIMG_WITH_JPG guards below come from cgimg's CMakeLists
// (PRIVATE definitions): this header is included only by cgimg's own .cpp files,
// so every translation unit that sees these declarations also sees the macros.
#include "image.h"

class ImgIO
{
public:
	static int load (Img& img, char const *filename, char const *path = nullptr);
	static int save (Img& img, char const *filename);

	// Cf. Img::load_from_memory. Implante dans image_io_memory.cpp, qui porte sa
	// PROPRE copie de stb_image : ni import_png ni import_jpg ne peuvent la
	// partager, leurs unites compilant stb avec STB_IMAGE_STATIC pour ne pas
	// entrer en conflit avec la copie de cgmesh (vmeshes.cpp). Consolider les
	// trois en une seule unite est un point ouvert de l'audit de dette
	// (debt_cgimg.md, « 2 copies de stb_image dans cgimg ») ; ce n'est pas fait
	// ici pour ne pas toucher au chemin de decodage JPEG dont depend maker.
	static int load_from_memory (Img& img, const unsigned char *data, size_t size);

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
#ifdef CGIMG_WITH_PNG
	static int import_png (Img& img, const char *filename);
	static int export_png (Img& img, const char *filename);
#endif
#ifdef CGIMG_WITH_JPG
	static int import_jpg (Img& img, const char *filename);   // stb : import seul (pas d'encodeur)
#endif
};
