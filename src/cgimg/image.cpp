#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "image.h"
#include "color.h"
#include "../cgmath/TVector2.h"

#include <list>
using namespace std;

int Img::AreIdentical (Img *pImg1, Img *pImg2)
{
	if (pImg1->m_iWidth != pImg2->m_iWidth && pImg1->m_iHeight != pImg2->m_iHeight)
		return -1;

	int nDifferentPixels = 0;
	for (unsigned int j=0; j<pImg1->m_iHeight; j++)
		for (unsigned int i=0; i<pImg1->m_iWidth; i++)
		{
			if (pImg1->get_pixel_int(i,j) != pImg2->get_pixel_int(i,j))
				nDifferentPixels++;
		}

	return nDifferentPixels;
}


int Img::resize_memory (unsigned int width, unsigned int height, bool use_palette)
{
	if (width == m_iWidth && height == m_iHeight)
		return 0;
	
	if (m_pPixels)
		free (m_pPixels);

	m_iWidth = width;
	m_iHeight = height;

	if (use_palette)
	{
		m_pPixels = (unsigned char*)malloc(m_iWidth*m_iHeight*sizeof(unsigned char));
		bUsePalette = true;
		m_pPalette = new Palette ();
	}
	else
	{
		m_pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
		bUsePalette = false;
		m_pPalette = NULL;
	}
	
	return (m_pPixels)? 0 : -1;
}

Img::Img (unsigned int w, unsigned int h, bool use_palette)
{
	m_iWidth = 0;
	m_iHeight = 0;
	m_pPixels = NULL;
	bUsePalette = use_palette;
	m_pPalette = NULL;
	resize_memory (w,h, use_palette);
}

Img::Img (const Img &img)
{
	m_iWidth = 0;
	m_iHeight = 0;
	m_pPixels = NULL;
	resize_memory (img.m_iWidth, img.m_iHeight, false);
	if (!img.bUsePalette)
		memcpy (m_pPixels, img.m_pPixels, 4*m_iWidth*m_iHeight*sizeof(unsigned char));
	else
	{
		for(unsigned int y=0;y<m_iHeight;y++)
			for(unsigned int x=0;x<m_iWidth;x++)
			{
				unsigned char r, g, b, a;
				img.get_pixel(x,y, &r, &g, &b, &a);
				set_pixel(x,y, r, g, b, a);
			}
	}

	bUsePalette = 0;
	m_pPalette = NULL;
}

Img::~Img ()
{
	if (m_pPixels)
		free (m_pPixels);

	if (m_pPalette)
		delete m_pPalette;
}

int Img::init_color (unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
			set_pixel (i, j, r, g, b, a);

	return 0;
}

void Img::set_pixel (unsigned int i, unsigned int j,
		     unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	if (i>=m_iWidth || j>=m_iHeight)
	{
		printf ("!!! [Img::set_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return;
	}
	unsigned int index = j*m_iWidth+i;
	if (bUsePalette)
	{
		m_pPixels[index] = m_pPalette->AddColor (r, g, b, a);
	}
	else
	{
		index *= 4;
		m_pPixels[index]   = r;
		m_pPixels[index+1] = g;
		m_pPixels[index+2] = b;
		m_pPixels[index+3] = a;
	}
}

void Img::set_pixel_int (unsigned int i, unsigned int j, int c)
{
	RGBc rgb;
	Color::Int2RGBc (c, rgb);
	set_pixel (i, j, rgb.r, rgb.g, rgb.b, 255);
}

void Img::set_pixel_index (unsigned int i, unsigned int j, unsigned int index)
{
	if (i>=m_iWidth || j>=m_iHeight)
	{
		printf ("!!! [Img::set_pixel_index] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return;
	}
	if (!bUsePalette)
	{
		printf ("!!! [Img::set_pixel_index] bUsePalette = %d\n", bUsePalette);
		return;
	}
	m_pPixels[j*m_iWidth+i] = index;
}

void Img::get_pixel (unsigned int i, unsigned int j,
		     unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a) const
{
	if (i>=m_iWidth || j>= m_iHeight)
	{
		printf ("!!! [Img::get_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		*r = 0; *g = 0; *b = 0; *a = 0;
		return;
	}
	unsigned int index = j*m_iWidth+i;
	if (!bUsePalette)
	{
		*r = m_pPixels[4*index];
		*g = m_pPixels[4*index+1];
		*b = m_pPixels[4*index+2];
		*a = m_pPixels[4*index+3];
	}
	else
	{
		unsigned int icolor = m_pPixels[index];
		*r = m_pPalette->m_pColors[icolor].r();
		*g = m_pPalette->m_pColors[icolor].g();
		*b = m_pPalette->m_pColors[icolor].b();
		*a = m_pPalette->m_pColors[icolor].a();
	}
}

int Img::get_pixel_int (unsigned int i, unsigned int j)
{
	RGBAc rgba;
	//unsigned char r, g, b, a;
	//get_pixel (i, j, &r, &g, &b, &a);
	//printf ("%d x %d => %d %d %d %d\n", i, j, r, g, b, a);
	get_pixel (i, j, &rgba.r, &rgba.g, &rgba.b, &rgba.a);
	//printf ("%d %d %d %d => %d\n", rgba.r, rgba.g, rgba.b, rgba.a, RGBAc2Int (rgba));
	return Color::RGBAc2Int (rgba);
}

int Img::get_pixel_index (unsigned int i, unsigned int j)
{
	if (!bUsePalette)
		return -1;
	return m_pPixels[m_iWidth*j+i];
}

unsigned char Img::get_r (unsigned int i, unsigned int j)
{
	if (i>=m_iWidth || j>= m_iHeight)
	{
		printf ("!!! [Img::get_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return 0;
	}
	return m_pPixels[4*(j*m_iWidth+i)];
}

unsigned char Img::get_g (unsigned int i, unsigned int j)
{
	if (i>=m_iWidth || j>= m_iHeight)
	{
		printf ("!!! [Img::get_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return 0;
	}
	return m_pPixels[4*(j*m_iWidth+i)+1];
}

unsigned char Img::get_b (unsigned int i, unsigned int j)
{
	if (i>=m_iWidth || j>= m_iHeight)
	{
		printf ("!!! [Img::get_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return 0;
	}
	return m_pPixels[4*(j*m_iWidth+i)+2];
}

unsigned char Img::get_a (unsigned int i, unsigned int j)
{
	if (i>=m_iWidth || j>= m_iHeight)
	{
		printf ("!!! [Img::get_pixel] %d %d in %d %d\n", i, j, m_iWidth, m_iHeight);
		return 0;
	}
	return m_pPixels[4*(j*m_iWidth+i)+3];
}

void Img::get_nearest_pixel (float u, float v,
			     unsigned char *r, unsigned char *g, unsigned char *b, unsigned char *a)
{
	unsigned int i = (unsigned int)(u*m_iWidth);
	unsigned int j = (unsigned int)(v*m_iHeight);
	get_pixel (i, j, r, g, b, a);
}

// test images
void Img::init_test_grayscale1 (unsigned int h)
{
	resize_memory (255, h);
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
			set_pixel (i, j, i, i, i, 255);
}

void Img::init_test_grayscale2 (unsigned int size)
{
	resize_memory (8*size, 8*size);
	for (unsigned int j=0; j<8; j++)
		for (unsigned int i=0; i<8; i++)
		{
			unsigned char level = 4 * (8*j+i);
			for (unsigned int kj=0; kj<size; kj++)
				for (unsigned int ki=0; ki<size; ki++)
					set_pixel (i*size+ki, j*size+kj, level, level, level, 255);
		}
}

void Img::init_test_color_jet (unsigned int w, unsigned int h)
{
	resize_memory (w, h);
	for (unsigned int i=0; i<m_iWidth; i++)
	{
		int r, g, b;
		color_jet_int ((float)i/m_iWidth, &r, &g, &b);
		for (unsigned int j=0; j<m_iHeight; j++)
			set_pixel (i, j, (unsigned char)r, (unsigned char)g, (unsigned char)b, 255);
	}
}

// http://docs.gimp.org/2.6/en/gimp-tool-desaturate.html
void Img::convert_to_grayscale (grayscale_method_type grayscale_method_id, unsigned char nlevels)
{
	int n = m_iWidth * m_iHeight;

	switch (grayscale_method_id)
	{
	case GRAYSCALE_LUMINOSITY:
	{
		float kr = 0.21;
		float kg = 0.71;
		float kb = 0.07;
		/*
		  float kr = 0.3;
		  float kg = 0.59;
		  float kb = 0.11;
		*/
		for (int i=0; i<n; i++)
		{
			unsigned char gray = (unsigned char)(kr * m_pPixels[4*i] + kg * m_pPixels[4*i+1] + kb * m_pPixels[4*i+2]);
			m_pPixels[4*i]   = gray;
			m_pPixels[4*i+1] = gray;
			m_pPixels[4*i+2] = gray;
		}
	}
	break;
	case GRAYSCALE_LIGHTNESS:
		for (int i=0; i<n; i++)
		{
			unsigned char r = m_pPixels[4*i];
			unsigned char g = m_pPixels[4*i+1];
			unsigned char b = m_pPixels[4*i+2];
			unsigned char gray = (MIN(MIN(r,g), b) + MAX(MAX(r,g),b))/2.;
			m_pPixels[4*i]   = gray;
			m_pPixels[4*i+1] = gray;
			m_pPixels[4*i+2] = gray;
		}
		break;
	case GRAYSCALE_AVERAGE:
		for (int i=0; i<n; i++)
		{
			unsigned char gray = (unsigned char)((m_pPixels[4*i] + m_pPixels[4*i+1] + m_pPixels[4*i+2])/3.);
			m_pPixels[4*i]   = gray;
			m_pPixels[4*i+1] = gray;
			m_pPixels[4*i+2] = gray;
		}
		break;
	default:
		break;
	}

     if (nlevels < 2)
	  nlevels = 2;
     if (nlevels != 255)
     {
	  for (int i=0; i<n; i++)
	  {
	       unsigned char gray = m_pPixels[4*i];
	       unsigned char interval = (int)(nlevels * gray / 256.);
	       gray = (unsigned char)(255.*interval/(nlevels-1));
	       m_pPixels[4*i]   = gray;
	       m_pPixels[4*i+1] = gray;
	       m_pPixels[4*i+2] = gray;
	  }
     }
}

void Img::get_histogram (float histogram[256], int normalized)
{
     memset (histogram, 0, 256*sizeof(float));
     
     for (unsigned int j=0; j<m_iHeight; j++)
     {
	  for (unsigned int i=0; i<m_iWidth; i++)
	  {
	       unsigned int index = j * m_iWidth + i;
	       unsigned char red = m_pPixels[4*index];
	       histogram[red] += 1.;
	  }
     }

     // normalize the histogram (sum = 1)
     if (normalized)
     {
	     float s = 1./(m_iWidth * m_iHeight);
	     for (int j=0; j<256; j++)
		     histogram[j] *= s;
     }
}

Img* Img::get_histogram_img (unsigned int height)
{
	float histogram[256];
	get_histogram (histogram, 1);

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

void Img::histogram_equalization (void)
{
     // eval the histogram
     float histogram[256];
     get_histogram (histogram);

     // eval the cumulative distribution function
     float cdf[256];
     float sum = 0.;
     for (int i=0; i<256; i++)
     {
	  sum += histogram[i];
	  cdf[i] = sum;
    }

     // image mapping
    for (unsigned int j=0; j<m_iHeight; j++)
     {
	  for (unsigned int i=0; i<m_iWidth; i++)
	  {
	       unsigned int index = j * m_iWidth + i;
	       unsigned char red = m_pPixels[4*index];
	       float grey = cdf[red];
	       m_pPixels[4*index]   = (unsigned char)(grey*255.0);
	       m_pPixels[4*index+1] = (unsigned char)(grey*255.0);
	       m_pPixels[4*index+2] = (unsigned char)(grey*255.0);
	  }
     }
}

void Img::histogram_equalization_bezier (CurveBezier *bezier)
{
	if (bezier == NULL)
		bezier = new CurveBezier();
	vec3 v0, v1, v2, v3;
	vec3_init (v0, 0., 255., 0.);
	vec3_init (v1, 10., 0., 0.);
	vec3_init (v2, 245., 0., 0.);
	vec3_init (v3, 255., 255., 0.);
	bezier->addControlPoint (v0);
	bezier->addControlPoint (v1);
	bezier->addControlPoint (v2);
	bezier->addControlPoint (v3);

     // eval the histogram
     float histogram[256];
     get_histogram (histogram);

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
	     vec3 pt;
	     bezier->eval_on_x (i, pt);
	     bezier_interpolated[i] = pt[1];
     }
     output_1array (bezier_interpolated, 256, (char*)"output.dat");
     bezier->export_interpolated ((char*)"bezier.dat", 256);

     // image mapping
     for (unsigned int j=0; j<m_iHeight; j++)
     {
	  for (unsigned int i=0; i<m_iWidth; i++)
	  {
	       unsigned int index = j * m_iWidth + i;
	       unsigned char red = m_pPixels[4*index];
	       red = (unsigned char)bezier_interpolated[red];
	       float grey = cdf[red];
	       m_pPixels[4*index]   = (unsigned char)(grey*255.0);
	       m_pPixels[4*index+1] = (unsigned char)(grey*255.0);
	       m_pPixels[4*index+2] = (unsigned char)(grey*255.0);
	  }
     }
}

void Img::invert (void)
{
	int n = m_iWidth*m_iHeight;
	for (int index=0; index<n; index++)
	{
		m_pPixels[4*index]   = 255-m_pPixels[4*index];
		m_pPixels[4*index+1] = 255-m_pPixels[4*index+1];
		m_pPixels[4*index+2] = 255-m_pPixels[4*index+2];
	}
}

void Img::contrast (float k)
{
	unsigned char min = 255;
	unsigned char max = 0;

	int n = m_iWidth*m_iHeight;
	for (int i=0; i<n; i++)
	{
		if (min > m_pPixels[4*i])
			min = m_pPixels[4*i];
		if (max < m_pPixels[4*i])
			max = m_pPixels[4*i];
	}

	unsigned char r, g, b, a;
	float T = 1.;
	float alpha = k;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			get_pixel (i, j, &r, &g, &b, &a);

			float rtmp = (float) (r-min) / (max-min);
			rtmp = filter_raised_cosine_filter (T, alpha, 1.-rtmp);
			r = (unsigned char)(255.*rtmp);

			set_pixel (i, j, r, r, r, a);
		}
}

int Img::get_mean_value (void)
{
	int sum = 0;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
			sum += m_pPixels[4*(j+m_iWidth+i)];

	return (int)(sum/(m_iWidth*m_iHeight));
}

int Img::get_median_value (void)
{
	float histogram[256];
	get_histogram (histogram, 0);

	int i = 0, n = 0;
	int n2 = m_iWidth * m_iHeight /2.;
	int median = 0;
	while (n < n2)
		n += histogram[i++];

	return i;
}

int Img::crop (Img *pImg, int x, int y, unsigned int width, unsigned int height)
{
	if (x+width > pImg->m_iWidth || y+height > pImg->m_iHeight)
		return -1;
	
	m_iWidth = 0;
	m_iHeight = 0;
	resize_memory (width, height);
	
	unsigned char r, g, b, a;
	for (unsigned int j=0; j<height; j++)
		for (unsigned int i=0; i<width; i++)
		{
			unsigned int ii = x+i;
			unsigned int jj = y+j;
			if (ii > pImg->m_iWidth-1)
				ii = 2 * pImg->m_iWidth - 1 - ii;
			if (jj > pImg->m_iHeight-1)
				jj = 2 * pImg->m_iHeight - 1 - jj;
			pImg->get_pixel (ii, jj, &r, &g, &b, &a);
			this->set_pixel (i, j, r, g, b, a);
		}
	
	m_iWidth = width;
	m_iHeight = height;
	
	return 0;
}

int Img::resize (unsigned int width, unsigned int height, int mode)
{
	if (width == m_iWidth && height == m_iHeight)
		return 0;
	
	unsigned char *pPixels = (unsigned char*)malloc(4*width*height*sizeof(unsigned char));
	if (pPixels == NULL)
		return -1;
	unsigned char r, g, b, a;
	unsigned int index;
	if (mode == 0) // nearest neighbour
	{
		for (unsigned int j=0; j<height; j++)
			for (unsigned int i=0; i<width; i++)
			{
				int x0 = i*(m_iWidth-1)/(width-1);
				int y0 = j*(m_iHeight-1)/(height-1);
				
				this->get_pixel (x0, y0, &r, &g, &b, &a);
				index = j*width + i;
				pPixels[4*index]   = r;
				pPixels[4*index+1] = g;
				pPixels[4*index+2] = b;
				pPixels[4*index+3] = a;
			}
	}
	else if (mode == 1) // bilinear sampling
	{
		for (unsigned int j=0; j<height; j++)
			for (unsigned int i=0; i<width; i++)
			{
				float x = i*(m_iWidth-1)/(width-1);
				float y = j*(m_iHeight-1)/(height-1);
				int x0 = (int)x;
				int y0 = (int)y;
				int x1 = x0 + 1;
				int y1 = y0 + 1;
				x = x - x0;
				y = y - y0;
				//
				int i00 = 4 * (y0 * m_iWidth + x0);
				int i01 = 4 * (y0 * m_iWidth + x1);
				int i10 = 4 * (y1 * m_iWidth + x0);
				int i11 = 4 * (y1 * m_iWidth + x1);
				int c00r = m_pPixels[i00];
				int c00g = m_pPixels[i00+1];
				int c00b = m_pPixels[i00+2];
				int c00a = m_pPixels[i00+3];
				int c01r = m_pPixels[i01];
				int c01g = m_pPixels[i01+1];
				int c01b = m_pPixels[i01+2];
				int c01a = m_pPixels[i01+3];
				int c10r = m_pPixels[i10];
				int c10g = m_pPixels[i10+1];
				int c10b = m_pPixels[i10+2];
				int c10a = m_pPixels[i10+3];
				int c11r = m_pPixels[i11];
				int c11g = m_pPixels[i11+1];
				int c11b = m_pPixels[i11+2];
				int c11a = m_pPixels[i11+3];

				r = 	(((int) c00r) * (1.f-x)*(1.f-y) +
					 ((int) c01r) * x      *(1.f-y) +
					 ((int) c10r) * (1.f-x)*      y  +
					 ((int) c11r) * x      *      y) + .5f;
				g = 	(((int) c00g) * (1.f-x)*(1.f-y) +
					 ((int) c01g) * x      *(1.f-y) +
					 ((int) c10g) * (1.f-x)*      y  +
					 ((int) c11g) * x      *      y) + .5f;
				b = 	(((int) c00b) * (1.f-x)*(1.f-y) +
					 ((int) c01b) * x      *(1.f-y) +
					 ((int) c10b) * (1.f-x)*      y  +
					 ((int) c11b) * x      *      y) + .5f;
				a = 	(((int) c00a) * (1.f-x)*(1.f-y) +
					 ((int) c01a) * x      *(1.f-y) +
					 ((int) c10a) * (1.f-x)*      y  +
					 ((int) c11a) * x      *      y) + .5f;
				
				unsigned char cr = (unsigned char) ((r > 255) ? 255 : ((r < 0) ? 0 : r));
				unsigned char cg = (unsigned char) ((g > 255) ? 255 : ((g < 0) ? 0 : g));
				unsigned char cb = (unsigned char) ((b > 255) ? 255 : ((b < 0) ? 0 : b));
				unsigned char ca = (unsigned char) ((a > 255) ? 255 : ((a < 0) ? 0 : a));

				index = j*width + i;
				pPixels[4*index]   = cr;
				pPixels[4*index+1] = cg;
				pPixels[4*index+2] = cb;
				pPixels[4*index+3] = ca;
			}
	}
	else if (mode == 2)
	{
		unsigned int xinterval = m_iWidth / width;
		unsigned int yinterval = m_iHeight / height;
		int *histogram = (int*)malloc(4*xinterval*yinterval*sizeof(int));
		int ncolors = 0;
		for (unsigned int j=0; j<height; j++)
			for (unsigned int i=0; i<width; i++)
			{
				ncolors = 0;
				int xoffset = i * (m_iWidth) / (width);
				int yoffset = j * (m_iHeight) / (height);
				for (unsigned int jj=0; jj<yinterval; jj++)
					for (unsigned int ii=0; ii<xinterval; ii++)
					{
						unsigned char r, g, b, a;
						int x0 = xoffset + ii;
						int y0 = yoffset + jj;
						
						this->get_pixel (x0, y0, &r, &g, &b, &a);
						int ci;
						for (ci=0; ci<ncolors; ci++)
						{
							if (r == histogram[4*ci] && g == histogram[4*ci+1] && b == histogram[4*ci+2])
							{
								histogram[4*ci+3]++;
								break;
							}
						}
						if (ci == ncolors)
						{
							histogram[4*ci]   = r;
							histogram[4*ci+1] = g;
							histogram[4*ci+2] = b;
							histogram[4*ci+3] = 0;
							ncolors++;
						}
					}
				int max = 0;
				int cimax = 0;
				for (int ci=0; ci<ncolors; ci++)
				{
					if (histogram[4*ci+3] > max)
					{
						max = histogram[4*ci+3];
						cimax = ci;
					}
				}
				index = j*width + i;
				pPixels[4*index]   = histogram[4*cimax];
				pPixels[4*index+1] = histogram[4*cimax+1];
				pPixels[4*index+2] = histogram[4*cimax+2];
				pPixels[4*index+3] = 255;
			}
	}

	m_iWidth = width;
	m_iHeight = height;
	free (m_pPixels);
	m_pPixels = pPixels;
	
	return 0;
}

int Img::resize_pixel (unsigned int n)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*n*m_iWidth*n*m_iHeight*sizeof(unsigned char));
	if (pPixels == NULL)
		return -1;

	unsigned int iWidth = m_iWidth * n;
	unsigned int iHeight = m_iHeight * n;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
			for (unsigned int k=0; k<n; k++)
				for (unsigned int l=0; l<n; l++)
				{
					unsigned int isrc = j*m_iWidth+i;
					unsigned int idst = (n*j+l)*iWidth+n*i+k;
					pPixels[4*idst]   = m_pPixels[4*isrc];
					pPixels[4*idst+1] = m_pPixels[4*isrc+1];
					pPixels[4*idst+2] = m_pPixels[4*isrc+2];
					pPixels[4*idst+3] = m_pPixels[4*isrc+3];
				}

	m_iWidth  = iWidth;
	m_iHeight = iHeight;
	free (m_pPixels);
	m_pPixels = pPixels;
	
	return 0;
}

int Img::copy (unsigned int x, unsigned int y, Img *pSrc)
{
	if (!pSrc)
		return -1;

	unsigned char r, g, b, a;
	for (unsigned int j=0; j<pSrc->m_iHeight; j++)
		for (unsigned int i=0; i<pSrc->m_iWidth; i++)
		{
			pSrc->get_pixel (i, j, &r, &g, &b, &a);
			set_pixel (x+i, y+j, r, g, b, a);
		}
	return 0;
}

int Img::concatenate (Img *pImg)
{
	if (!pImg)
		return -1;

	unsigned int w = m_iWidth;
	resize_canvas (m_iWidth+pImg->m_iWidth, m_iHeight, 8, 255, 255, 255, 255);
	copy (w, 0, pImg);
	return 0;
}

// mode : 0 = 90 right, 1 = 90 left, 2 = 180
int Img::rotate (int mode)
{
     if (mode == 0) // 90 right
     {
	  unsigned char *pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
	  for (unsigned int j=0; j<m_iHeight; j++)
	       for (unsigned int i=0; i<m_iWidth; i++)
	       {
		       // (i,j) go to (m_iHeight-1-j,i)
		       unsigned int isrc = 4*(j * m_iWidth + i);
		       unsigned int idst = 4*(i * m_iHeight + m_iHeight - 1 - j);
		       pPixels[idst]   = m_pPixels[isrc];
		       pPixels[idst+1] = m_pPixels[isrc+1];
		       pPixels[idst+2] = m_pPixels[isrc+2];
		       pPixels[idst+3] = m_pPixels[isrc+3];
	       }

	  unsigned int tmp = m_iWidth;
	  m_iWidth = m_iHeight;
	  m_iHeight = tmp;
	  free (m_pPixels);
	  m_pPixels = pPixels;
     }
     else if (mode == 1) // 90 left
     {
	  unsigned char *pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
	  for (unsigned int j=0; j<m_iHeight; j++)
	       for (unsigned int i=0; i<m_iWidth; i++)
	       {
		       // (i,j) go to (j,w-1-i)
		       unsigned int isrc = 4*(j * m_iWidth + i);
		       unsigned int idst = 4*((m_iWidth - 1 - i) * m_iHeight + j);
		       pPixels[idst]   = m_pPixels[isrc];
		       pPixels[idst+1] = m_pPixels[isrc+1];
		       pPixels[idst+2] = m_pPixels[isrc+2];
		       pPixels[idst+3] = m_pPixels[isrc+3];
	       }

	  unsigned int tmp = m_iWidth;
	  m_iWidth = m_iHeight;
	  m_iHeight = tmp;
	  free (m_pPixels);
	  m_pPixels = pPixels;
      }
     else if (mode == 2) // 180
     {
	     unsigned int w = m_iWidth;
	     unsigned int h = m_iHeight;
	     unsigned int h2 = (unsigned int)(ceil((float)m_iHeight/2.));
	     for (unsigned int j=0; j<h2; j++)
		     for (unsigned int i=0; i<w; i++)
		     {
			     unsigned char r = m_pPixels[4*(j * w + i)];
			     unsigned char g = m_pPixels[4*(j * w + i) + 1];
			     unsigned char b = m_pPixels[4*(j * w + i) + 2];
			     unsigned char a = m_pPixels [4*(j * w + i) + 3];

			     m_pPixels[4*(j * w + i)]     = m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i))];
			     m_pPixels[4*(j * w + i) + 1] = m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 1];
			     m_pPixels[4*(j * w + i) + 2] = m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 2];
			     m_pPixels[4*(j * w + i) + 3] = m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 3];

			     m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i))]	    = r;
			     m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 1] = g;
			     m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 2] = b;
			     m_pPixels[4*((m_iHeight - 1 - j) * m_iWidth + (m_iWidth - 1 - i)) + 3] = a;
		     }
     }
	 return 0;
}

// positionning :
// 1 2 3
// 8 0 4
// 7 6 5
int Img::resize_canvas (unsigned int width, unsigned int height, int positioning,
			unsigned char bg_r, unsigned char bg_g, unsigned char bg_b, unsigned char bg_a)
{
	Img *pClone = new Img (*this);

	unsigned int w1 = m_iWidth;
	unsigned int h1 = m_iHeight;
	
	unsigned int w2 = width;
	unsigned int h2 = height;
	
	// resize the image
	resize_memory (width, height);

	// init the background
	init_color (bg_r, bg_g, bg_b, bg_a);

	// copy the clone in the new image
	unsigned int i1=0, j1=0, i2=0, j2=0;
	if (positioning == 1)
	{
		i1 = (w2 > w1)? 0 : 0;
		j1 = (h2 > h1)? 0 : (h1-h2);
		i2 = 0;
		j2 = (h2 > h1)? (h2-h1) : 0;
	}
	else if (positioning == 2)
	{
		i1 = (w2 > w1)? 0 : (w1-w2)/2.;
		j1 = (h2 > h1)? 0 : (h1-h2);
		i2 = (w2 > w1)? (w2-w1)/2. : 0;
		j2 = (h2 > h1)? (h2-h1) : 0;
	}
	else if (positioning == 3)
	{
		i1 = (w2 > w1)? 0 : (w1-w2);
		j1 = (h2 > h1)? 0 : (h1-h2);
		i2 = (w2 > w1)? (w2-w1) : 0;
		j2 = (h2 > h1)? (h2-h1) : 0;
	}
	else if (positioning == 4)
	{
		i1 = (w2 > w1)? 0 : (w1-w2);
		j1 = (h2 > h1)? 0 : (h1-h2)/2.;
		i2 = (w2 > w1)? (w2-w1) : 0;
		j2 = (h2 > h1)? (h2-h1)/2. : 0;
	}
	else if (positioning == 5)
	{
		i1 = (w2 > w1)? 0 : (w1-w2);
		j1 = (h2 > h1)? 0 : 0;
		i2 = (w2 > w1)? (w2-w1) : 0;
		j2 = 0;
	}
	else if (positioning == 6)
	{
		i1 = (w2 > w1)? 0 : (w1-w2)/2.;
		j1 = (h2 > h1)? 0 : 0;
		i2 = (w2 > w1)? (w2-w1)/2. : 0;
		j2 = 0;
	}
	else if (positioning == 7)
	{
		i1 = (w2 > w1)? 0 : 0;
		j1 = (h2 > h1)? 0 : 0;
		i2 = 0;
		j2 = 0;
	}
	else if (positioning == 8)
	{
		i1 = (w2 > w1)? 0 : 0;
		j1 = (h2 > h1)? 0 : (h1-h2)/2.;
		i2 = 0;
		j2 = (h2 > h1)? (h2-h1)/2. : 0;
	}
	else //(positioning == 0 and all others choices) // ok
	{
		i1 = (w2 > w1)? 0 : (w1-w2)/2.;
		j1 = (h2 > h1)? 0 : (h1-h2)/2.;
		i2 = (w2 > w1)? (w2-w1)/2. : 0;
		j2 = (h2 > h1)? (h2-h1)/2. : 0;
	}
	unsigned int wcopy = (w2 > w1)? w1 : w2;
	unsigned int hcopy = (h2 > h1)? h1 : h2;
	unsigned char r, g, b, a;
	for (unsigned int j=0; j<hcopy; j++)
		for (unsigned int i=0; i<wcopy; i++)
		{
			pClone->get_pixel (i1+i, j1+j, &r, &g, &b, &a);
			set_pixel (i2+i, j2+j, r, g, b, a);
		}
	
	delete pClone;

	return 0;
}

int Img::multiply (Img *pImg)
{
	if (!pImg || pImg->m_iWidth != m_iWidth || pImg->m_iHeight != m_iHeight)
		return -1;

	unsigned char r, g, b, a;
	unsigned char r2, g2, b2, a2;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			get_pixel (i, j, &r, &g, &b, &a);
			pImg->get_pixel (i, j, &r2, &g2, &b2, &a2);
			
			set_pixel (i, j,
				   (unsigned char)(r*r2/255.),
				   (unsigned char)(g*g2/255.),
				   (unsigned char)(b*b2/255.),
				   (unsigned char)(a*a2/255.));
		}
		return 0;
}


int Img::palettize (Img *pImg)
{
	if (m_pPixels)
		free (m_pPixels);
	m_iWidth = 0;
	m_iHeight = 0;
	resize_memory (pImg->width(), pImg->height(), true);

	unsigned char r, g, b, a;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			pImg->get_pixel (i, j, &r, &g, &b, &a);
			m_pPixels[m_iWidth*j + i] = m_pPalette->AddColor (r, g, b, a);
		}

	return 0;
}

Palette* Img::get_palette (void)
{
	Palette *pPalette;
	if (bUsePalette)
		pPalette = m_pPalette;
	else
	{
		pPalette = new Palette ();
		unsigned char r, g, b, a;
		for (unsigned int j=0; j<m_iHeight; j++)
			for (unsigned int i=0; i<m_iWidth; i++)
			{
				get_pixel (i, j, &r, &g, &b, &a);
				pPalette->AddColor (r, g, b, a);
			}
	}

	return pPalette;
}

int Img::flood_fill (unsigned int x, unsigned int y, unsigned char r, unsigned char g, unsigned char b)
{
	if(x >= m_iWidth || y >= m_iHeight)
		return 1;

	RGBc rgb;
	rgb.r = r;
	rgb.g = g;
	rgb.b = b;
	int cou = Color::RGBc2Int (rgb);
	//printf ("cou %d\n", cou);
	
	int couToReplace = get_pixel_int (x, y);
	//printf ("couToReplace %d\n", couToReplace);

	list<Vector2i> listPointToCheck;
	listPointToCheck.push_back(Vector2i(x,y));
	while(!listPointToCheck.empty())
	{
		Vector2i back = listPointToCheck.front();
		x = back[0];
		y = back[1];

		listPointToCheck.pop_front();
		if(get_pixel_int (x, y) == cou)
			continue;

		// search the start
		int xStart = x;
		while( (xStart-1) >=0 && get_pixel_int (xStart-1, y) == couToReplace)
			xStart--;

		// search the end
		unsigned int xEnd = x;
		while( (xEnd+1) < m_iWidth && get_pixel_int (xEnd+1, y) == couToReplace)
			xEnd++;

		//
		for(unsigned int xCurrent=xStart; xCurrent<=xEnd; xCurrent++)
		{
			set_pixel (xCurrent, y, r, g, b, 255);
			if(y>0)
			{
				if(get_pixel_int (xCurrent, y-1) == couToReplace)
					listPointToCheck.push_back(Vector2i(xCurrent,y-1));
			}
			if(y<m_iHeight-1)
			{
				if(get_pixel_int (xCurrent, y+1) == couToReplace)
					listPointToCheck.push_back(Vector2i(xCurrent,y+1));
			}
		}
	}

	return 0;
}
