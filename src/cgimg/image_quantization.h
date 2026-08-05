#pragma once

// Quantification de couleurs d'une Img.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img (accès à la palette et au drapeau
// bUsePalette).
#include "image.h"

class ImgQuantize
{
public:
	static int heckbert (Img& img, int ncolors);
	static int wu       (Img& img, int ncolors);
	static int kmean    (Img& img, float threshold);

	// Raffinement de Lloyd (k-means) de la palette de `img`, deja quantifiee,
	// contre `reference` = l'image AVANT quantification (memes dimensions). Chaque
	// iteration reaffecte chaque pixel a la couleur de palette la plus proche en RGB
	// 8 bits, puis recalcule chaque couleur comme la moyenne des pixels de reference
	// qui lui sont affectes : c'est exactement le critere que minimise l'erreur
	// quadratique, donc la MSE decroit de facon monotone.
	//
	// Utile apres wu() comme apres heckbert(). Les deux prennent leurs decisions sur
	// un histogramme 5 bits/canal (Wu affecte via ses boites 32^3, Heckbert via le
	// centroide de cube) ; le raffinement rejuge en 8 bits pleins.
	// Mesures sur une affiche 4 couleurs rechantillonnee (375x564) :
	//   n=4  Wu 199.0 -> 187.3 | Heckbert 868.8 -> 187.3 (les deux convergent)
	//   n=8  Wu  84.2 ->  75.8 | Heckbert 452.4 ->  70.7
	//   n=16 Wu  36.4 ->  29.3 | Heckbert 105.8 ->  36.2
	// Convergence : 1 iteration suffit a n=4, ~3 a n=16.
	//
	// Renvoie le nombre de couleurs de la palette, ou -1 si les images ne sont pas
	// compatibles (dimensions differentes, ou image palettisee).
	static int refine (Img& img, const Img &reference, int iterations = 3);
};
