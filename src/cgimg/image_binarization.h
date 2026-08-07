#pragma once

// Binarisation / tramage d'une Img.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img.
#include "image.h"

class ImgBinarize
{
public:
	static int threshold       (Img& img, int threshold);
	static int random          (Img& img, int methodId);
	static int floyd_steinberg (Img& img);
	static int otsu            (Img& img);
	static int dithering       (Img& img, unsigned char *pattern, int psize);
	static int screening       (Img& img, const Img &pattern);
};
