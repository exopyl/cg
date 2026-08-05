#include <stdlib.h>

#include "image.h"
#include "image_test_pattern.h"
#include "color.h"

// Extrait d'image.cpp : generer une mire n'est pas une responsabilite du
// conteneur d'image. Code deplace tel quel (aucun changement de comportement),
// seul le receveur change : resize_memory / set_pixel et les membres
// m_iWidth / m_iHeight sont atteints via `img`, ImgTestPattern etant classe
// amie d'Img.

void ImgTestPattern::grayscale1 (Img& img, unsigned int h)
{
	img.resize_memory (255, h);
	for (unsigned int j=0; j<img.m_iHeight; j++)
		for (unsigned int i=0; i<img.m_iWidth; i++)
			img.set_pixel (i, j, i, i, i, 255);
}

void ImgTestPattern::grayscale2 (Img& img, unsigned int size)
{
	img.resize_memory (8*size, 8*size);
	for (unsigned int j=0; j<8; j++)
		for (unsigned int i=0; i<8; i++)
		{
			unsigned char level = 4 * (8*j+i);
			for (unsigned int kj=0; kj<size; kj++)
				for (unsigned int ki=0; ki<size; ki++)
					img.set_pixel (i*size+ki, j*size+kj, level, level, level, 255);
		}
}

void ImgTestPattern::color_jet (Img& img, unsigned int w, unsigned int h)
{
	img.resize_memory (w, h);
	for (unsigned int i=0; i<img.m_iWidth; i++)
	{
		int r, g, b;
		color_jet_int ((float)i/img.m_iWidth, &r, &g, &b);
		for (unsigned int j=0; j<img.m_iHeight; j++)
			img.set_pixel (i, j, (unsigned char)r, (unsigned char)g, (unsigned char)b, 255);
	}
}
