#pragma once

// Mires de test : remplissent une Img d'un motif connu.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img. Sorties d'image.cpp car générer une
// mire n'est pas une responsabilité du conteneur d'image.
#include "image.h"

class ImgTestPattern
{
public:
	// Dégradé horizontal de niveaux de gris, hauteur `h`.
	static void grayscale1 (Img& img, unsigned int h);

	// Damier / rampe de niveaux de gris de côté `size`.
	static void grayscale2 (Img& img, unsigned int size);

	// Fausse couleur « jet » sur w x h : dégradé continu, beaucoup de couleurs.
	static void color_jet (Img& img, unsigned int w, unsigned int h);
};
