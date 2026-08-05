#pragma once

// Tracé de primitives 2D dans une Img.
//
// Même modèle qu'ImgIO (image_io.h) : méthodes statiques prenant l'Img en
// premier paramètre, classe amie d'Img.
//
// NOTE (dette connue, cf. debt_cgimg.md) : le canal vert est déclaré
// `unsigned int` alors que r, b et a sont en `unsigned char`. Incohérence
// reprise telle quelle depuis les anciennes méthodes d'Img pour que cette
// extraction reste un déplacement pur, sans changement de comportement.
#include "image.h"

class ImgDraw
{
public:
	static int horizontal_line (Img& img, unsigned int y, unsigned int xstart, unsigned int xend,
	                            unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	static int line (Img& img, unsigned int xtart, unsigned int ystart,
	                 unsigned int xend, unsigned int yend,
	                 unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	static int circle (Img& img, unsigned int x0, unsigned int y0, unsigned int radius,
	                   unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	static int disk (Img& img, unsigned int x0, unsigned int y0, unsigned int radius,
	                 unsigned char r, unsigned int g, unsigned char b, unsigned char a);
	static int ellipse (Img& img, unsigned int x0, unsigned int y0,
	                    unsigned int radiusx, unsigned int radiusy,
	                    unsigned char r, unsigned int g, unsigned char b, unsigned char a);

	// Adoucit les transitions entre noir et blanc.
	static int smooth_transition (Img& img, int l);
};
