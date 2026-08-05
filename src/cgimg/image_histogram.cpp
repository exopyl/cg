#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "image.h"
#include "image_histogram.h"
#include "../cgmath/cgmath.h"

// Extrait d'image.cpp : l'analyse d'une image n'est pas une responsabilite du
// conteneur. Code deplace tel quel (aucun changement de comportement), seul le
// receveur change : les anciens membres m_pPixels / m_iWidth / m_iHeight sont
// desormais atteints via `img`, ImgHistogram etant classe amie d'Img.

void ImgHistogram::compute (Img& img, float histogram[256], int normalized)
{
     memset (histogram, 0, 256*sizeof(float));

     for (unsigned int j=0; j<img.m_iHeight; j++)
     {
	  for (unsigned int i=0; i<img.m_iWidth; i++)
	  {
	       unsigned int index = j * img.m_iWidth + i;
	       unsigned char red = img.m_pPixels[4*index];
	       histogram[red] += 1.;
	  }
     }

     // normalize the histogram (sum = 1)
     if (normalized)
     {
	     float s = 1./(img.m_iWidth * img.m_iHeight);
	     for (int j=0; j<256; j++)
		     histogram[j] *= s;
     }
}

Img* ImgHistogram::to_image (Img& img, unsigned int height)
{
	float histogram[256];
	compute (img, histogram, 1);

	Img *histo = new Img (256, height);
	for (unsigned int j=0; j<histo->m_iHeight; j++)
		for (unsigned int i=0; i<histo->m_iWidth; i++)
		{
			if (histogram[i] > (float)j*0.01/histo->m_iHeight)
				histo->set_pixel (i, histo->m_iHeight-1-j, 255, 255, 255, 255);
			else
				histo->set_pixel (i, histo->m_iHeight-1-j, 0, 0, 0, 255);
		}

	return histo;
}

void ImgHistogram::equalize (Img& img)
{
     // eval the histogram
     float histogram[256];
     compute (img, histogram);

     // eval the cumulative distribution function
     float cdf[256];
     float sum = 0.;
     for (int i=0; i<256; i++)
     {
	  sum += histogram[i];
	  cdf[i] = sum;
    }

     // image mapping
    for (unsigned int j=0; j<img.m_iHeight; j++)
     {
	  for (unsigned int i=0; i<img.m_iWidth; i++)
	  {
	       unsigned int index = j * img.m_iWidth + i;
	       unsigned char red = img.m_pPixels[4*index];
	       float grey = cdf[red];
	       img.m_pPixels[4*index]   = (unsigned char)(grey*255.0);
	       img.m_pPixels[4*index+1] = (unsigned char)(grey*255.0);
	       img.m_pPixels[4*index+2] = (unsigned char)(grey*255.0);
	  }
     }
}

void ImgHistogram::equalize_bezier (Img& img, CurveBezier *bezier)
{
	if (bezier == nullptr)
		bezier = new CurveBezier();
	bezier->addControlPoint (0.f, 255.f, 0.f);
	bezier->addControlPoint (10.f, 0.f, 0.f);
	bezier->addControlPoint (245.f, 0.f, 0.f);
	bezier->addControlPoint (255.f, 255.f, 0.f);

     // eval the histogram
     float histogram[256];
     compute (img, histogram);

     // eval the cumulative distribution function
     float cdf[256];
     float sum = 0.;
     for (int i=0; i<256; i++)
     {
	  sum += histogram[i];
	  cdf[i] = sum;
     }

     float *bezier_interpolated = (float*)malloc(256*sizeof(float));
     for (int i=0; i<256; i++)
     {
	     Vector3f pt;
	     bezier->eval_on_x ((float)i, pt);
	     bezier_interpolated[i] = pt.y;
     }
     output_1array (bezier_interpolated, 256, "output.dat");

     bezier->export_interpolated ((char*)"bezier.dat", 256);

     // image mapping
     for (unsigned int j=0; j<img.m_iHeight; j++)
     {
	  for (unsigned int i=0; i<img.m_iWidth; i++)
	  {
	       unsigned int index = j * img.m_iWidth + i;
	       unsigned char red = img.m_pPixels[4*index];
	       red = (unsigned char)bezier_interpolated[red];
	       float grey = cdf[red];
	       img.m_pPixels[4*index]   = (unsigned char)(grey*255.0);
	       img.m_pPixels[4*index+1] = (unsigned char)(grey*255.0);
	       img.m_pPixels[4*index+2] = (unsigned char)(grey*255.0);
	  }
     }
}
