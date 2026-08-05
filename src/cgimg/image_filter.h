#pragma once

// Filtrage et colorimétrie d'une Img.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img pour accéder au tampon de pixels. Ces
// opérations étaient auparavant des méthodes d'Img ; les en sortir évite que
// chaque nouvel algorithme n'impose de modifier image.h, donc de recompiler
// tous les consommateurs de cgimg.
#include "image.h"

class ImgFilter
{
public:
	// Convolution 3x3 générique. `divide` == 0 -> normalisation par la somme du
	// noyau ; `decay` est ajouté au résultat de chaque canal.
	static int convolve (Img& img, float m[3][3], float divide = 0., float decay = 0.);

	static int sobel         (Img& img);
	static int gaussian_blur (Img& img);
	static int blur          (Img& img);
	static int bilateral     (Img& img);

	// Filtre de MODE : chaque pixel prend la couleur la plus fréquente de son
	// voisinage carré (2*radius+1). Égalité -> le pixel central est conservé, pour
	// qu'une frontière franche ne dérive pas. Non linéaire : convolve() ne peut pas
	// l'exprimer (un vote majoritaire n'est pas une convolution).
	//
	// Chaque passe lit un instantané de l'image, donc le résultat ne dépend pas de
	// l'ordre de balayage. L'alpha de chaque pixel est conservé.
	static int majority (Img& img, int radius = 1, int passes = 1);

	// Absorbe toute composante connexe (4-connexe, couleur identique) d'aire
	// strictement inférieure à `minArea` dans la couleur la plus fréquente sur sa
	// frontière. Répété jusqu'à `passes` fois : fusionner un mouchetis dans un petit
	// voisin peut laisser le résultat encore sous le seuil.
	//
	// Le nombre de couleurs ne peut que décroître : une région n'est jamais peinte
	// d'une couleur absente de son voisinage, donc l'image reste un pavage complet.
	static int absorb_small_regions (Img& img, int minArea, int passes = 3);

	// Colorimétrie
	static int saturate   (Img& img, float t);
	static int brightness (Img& img, float t);
	static int gamma      (Img& img, float t);
	static int sepia      (Img& img);
};
