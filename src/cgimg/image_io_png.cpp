#include "image.h"
#include "image_io.h"
#ifdef CGIMG_WITH_PNG

// PNG decoding via stb_image (header-only, cross-platform, no libpng
// dependency). STB_IMAGE_STATIC keeps the implementation symbols internal to
// this translation unit so they don't clash with the copy compiled into
// cgmesh (vmeshes.cpp) when both libraries are linked into the same binary.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

int ImgIO::import_png (Img& img, const char *filename)
{
	int w = 0, h = 0, channels = 0;

	// Force 4 components so every pixel comes back as RGBA, regardless of the
	// file's colour type (greyscale / palette / RGB / RGBA).
	unsigned char *data = stbi_load (filename, &w, &h, &channels, 4);
	if (!data || w <= 0 || h <= 0)
	{
		if (data) stbi_image_free(data);
		printf ("[import_png] failed to load %s: %s\n",
		        filename, data ? "invalid dimensions" : stbi_failure_reason ());
		return -1;
	}

	// Le code de retour etait ignore : sur echec d'allocation, les dimensions
	// sont posees mais le tampon reste nul, et les set_pixel qui suivent
	// ecrivaient dedans.
	if (img.resize_memory (w, h) != 0)
	{
		stbi_image_free (data);
		return -1;
	}
	// Calcul de l'offset en size_t, et non en int : stb accepte jusqu'a 2^24 par
	// cote, donc `4 * (y * w + x)` debordait un int 32 bits bien avant cette limite
	// -- un debordement signe, comportement indefini, avant meme d'indexer.
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			const unsigned char *p = data + 4 * ((size_t)y * (size_t)w + (size_t)x);
			img.set_pixel (x, y, p[0], p[1], p[2], p[3]);
		}
	}

	stbi_image_free (data);
	return 0;
}

int ImgIO::export_png (Img& img, const char *filename)
{
	// Only the stb_image decoder is vendored (no stb_image_write), so PNG
	// writing is not available here. Save as .bmp / .tga / .ppm instead, or
	// add stb_image_write.h to enable this path.
	printf ("[export_png] PNG writing is not supported (no encoder available)\n");
	return -1;
}

#endif // CGIMG_WITH_PNG
