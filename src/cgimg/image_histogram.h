#pragma once

// Histogramme d'une Img : calcul, rendu et égalisation.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img. Ces méthodes vivaient auparavant dans
// image.cpp ; l'analyse d'une image n'est pas une responsabilité du conteneur.
#include "image.h"

class CurveBezier;

class ImgHistogram
{
public:
	// Remplit `histogram` (256 casiers, niveaux de gris). `normalized` != 0 -> la
	// somme des casiers vaut 1.
	static void compute (Img& img, float histogram[256], int normalized = 1);

	// Construit une image de l'histogramme, de hauteur `height`. L'appelant
	// devient propriétaire de l'Img renvoyée.
	static Img* to_image (Img& img, unsigned int height);

	static void equalize (Img& img);

	// Égalisation guidée par une courbe de Bézier ; `bezier` == nullptr -> courbe
	// par défaut.
	static void equalize_bezier (Img& img, CurveBezier *bezier = nullptr);
};
