#include "image.h"

// JPEG decoding via stb_image (header-only, no libjpeg dependency). Calqué sur
// image_io_png.cpp : STB_IMAGE_STATIC garde les symboles du décodeur internes à
// cette unité de compilation pour ne pas entrer en conflit avec les copies
// compilées dans image_io_png.cpp et cgmesh (vmeshes.cpp) lorsqu'elles sont
// liées ensemble. (stb ne fournit pas d'encodeur : pas d'export JPEG ici.)
#ifdef JPG
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

int Img::import_jpg (const char *filename)
{
	int w = 0, h = 0, channels = 0;

	// Force 4 composantes : chaque pixel revient en RGBA quel que soit l'espace
	// colorimétrique du JPEG (niveaux de gris / YCbCr).
	unsigned char *data = stbi_load (filename, &w, &h, &channels, 4);
	if (!data || w <= 0 || h <= 0)
	{
		if (data) stbi_image_free (data);
		printf ("[import_jpg] failed to load %s: %s\n",
		        filename, data ? "invalid dimensions" : stbi_failure_reason ());
		return -1;
	}

	resize_memory (w, h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
		{
			const unsigned char *p = data + 4 * (y * w + x);
			set_pixel (x, y, p[0], p[1], p[2], p[3]);
		}

	stbi_image_free (data);
	return 0;
}
#endif // JPG
