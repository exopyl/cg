#pragma once

// Transformée géodésique (distance) sur une Img.
//
// Même modèle qu'ImgIO (image_io.h) : méthode statique prenant l'Img en
// premier paramètre, classe amie d'Img.
#include "image.h"

class ImgGeodesic
{
public:
	static int apply (Img& img);
};
