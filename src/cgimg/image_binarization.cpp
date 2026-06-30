#include <stdlib.h>

#include "image.h"

//
// binarization
//
int Img::bin_threshold (int threshold)
{
	unsigned int i,j;
	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
			{
				unsigned char value = (m_pPixels[4*(j*m_iWidth+i)] > threshold)? 255 : 0;
				set_pixel(i, j, value, value, value, 255);
			}
	
	return 0;
}

int Img::bin_random(int methodId)
{
	unsigned int i,j,k;
	float law[256];
	switch (methodId)
	{
	case 0: // GAUSSIAN
	{
		int mean = get_median_value ();
		float sigma = 20.;
		float sigma2 = sigma * sigma;
		for (i=0; i<256; i++)
			law[i] = exp( - 0.5 * ((float)(i-mean))*((float)(i-mean))/sigma2);
	}
		break;
	default:
		break;
	}

	// array of thresholds
	unsigned int nthresholds = 0;
	for (i=0; i<256; i++)
		nthresholds += (unsigned int)(100*law[i]);
	unsigned char *thresholds = (unsigned char*)malloc(nthresholds*sizeof(unsigned char));
	for (k=0, i=0; i<256; i++)
		for (j=0; j<(unsigned int)(100*law[i]); j++)
			thresholds[k++] = i;

	// binarization
	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
			{
				unsigned char threshold = thresholds[(int)(nthresholds * ((float)rand()/RAND_MAX))];
				unsigned char value = (m_pPixels[4*(j*m_iWidth+i)] > threshold)? 255 : 0;
				set_pixel(i, j, value, value, value, 255);
			}

	// clean
	free (thresholds);
	
	return 0;
}

//
// A threshold selection method from gray-scale histogram
// N. Otsu
// IEEE Trans. on Syst. Man and Cyber., vol. 8, pp. 62–66, 1978
//
static float sig(float histo[256], int k, int mu, int taille_x, int taille_y)
{
	double tmp, w, tmp2;
	double N = (double)taille_x*taille_y;
	int i;
	
	w = 0;
	tmp = 0;
	for (i=0 ; i<=k; i++)
	{
		w += histo[i];
		tmp += (i+1)*histo[i];
	}
	w /= N;
	tmp /= N;
	tmp2 = (mu*w-tmp)*(mu*w-tmp)*(w*(1-w));
	
	return (float)tmp2;
}

int Img::bin_otsu (void)
{
	unsigned int i,j,k;

	// get the histogram
	float histogram[256];
	get_histogram (histogram, 0);
	
	// muT computation
	float muT = 0.;
	for (i=0;i<256;i++)
		muT += (i+1)*histogram[i];
	muT /= (float)(m_iWidth*m_iHeight);
	
	// sigma computation
	float sigma[256];
	for (i=1; i<255; i++)
		sigma[i] = sig(histogram,i,muT,m_iWidth,m_iHeight);

	// looking for the max
	i=0; 
	k=0;
	for (j=1; j<255;j++)
		if (i+1 < sigma[j])
		{ 
			k=j; i=sigma[j];
		}

	// binarization
	for (i=0; i<m_iWidth; i++)
		for (j=0; j<m_iHeight; j++)
			if (m_pPixels[4*(j*m_iWidth+i)] >= k)
				set_pixel (i, j, 255, 255, 255, 255);
			else
				set_pixel (i, j, 0, 0, 0, 255);
	
	return 0;
}

int Img::bin_floyd_steinberg (void)
{
	const int w = (int)m_iWidth, h = (int)m_iHeight;
	const int threshold = 128;

	// Tampon de travail signé : accumule l'erreur diffusée, qui peut sortir de [0,255].
	float *buffer = (float*)malloc((size_t)w*h*sizeof(float));
	if (!buffer)
		return -1;
	for (int j=0; j<h; j++)
		for (int i=0; i<w; i++)
			buffer[w*j+i] = (float)m_pPixels[4*(j*w+i)];   // canal rouge (image supposée en gris)

	// Balayage raster (ligne par ligne, gauche->droite) : seuillage + diffusion
	// d'erreur Floyd-Steinberg (7/16 droite, 3/16 bas-gauche, 5/16 bas, 1/16 bas-droite),
	// avec contrôle des bornes pour ne pas écrire hors du tampon.
	for (int j=0; j<h; j++)
		for (int i=0; i<w; i++)
		{
			const float         oldv = buffer[w*j+i];
			const unsigned char newv = (oldv < threshold) ? 0 : 255;
			set_pixel (i, j, newv, newv, newv, 255);
			const float err = oldv - (float)newv;

			if (i+1 < w)         buffer[w*j     + (i+1)] += err * 7.f/16.f;
			if (j+1 < h)
			{
				if (i > 0)       buffer[w*(j+1) + (i-1)] += err * 3.f/16.f;
				                 buffer[w*(j+1) +  i   ] += err * 5.f/16.f;
				if (i+1 < w)     buffer[w*(j+1) + (i+1)] += err * 1.f/16.f;
			}
		}

	free (buffer);
	return 0;
}

int Img::bin_dithering (unsigned char *pattern, int psize)
{
	unsigned int i, j;
	for (j=0; j<m_iHeight; j+=psize)
		for (i=0; i<m_iWidth; i+=psize)
		{
			// get the mean value
			int mean=0;
			for (int pj=0; pj<psize; pj++)
				for (int pi=0; pi<psize; pi++)
					mean += m_pPixels[4*(m_iWidth*(j+pj)+i+pi)];
			mean /= (psize*psize);

			// get the threshold in the pattern
			unsigned char threshold = mean / (255./(psize*psize));

			// apply the pattern
			for (int pj=0; pj<psize; pj++)
				for (int pi=0; pi<psize; pi++)
					if (pattern[pj*psize+pi] < threshold)
						set_pixel (i+pi, j+pj, 255, 255, 255, 255);
					else
						set_pixel (i+pi, j+pj, 0, 0, 0, 255);
			
		}
		return 0;
}

int Img::bin_screening (Img *pPattern)
{
	unsigned int i, j;
	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
		{
			unsigned char r, g, b, a;
			pPattern->get_pixel (i%pPattern->m_iWidth, j%pPattern->m_iHeight, &r, &g, &b, &a);
			unsigned threshold = r;
			get_pixel (i, j, &r, &g, &b, &a);

			if (r > threshold)
				set_pixel (i, j, 255, 255, 255, 255);
			else
				set_pixel (i, j, 0, 0, 0, 255);
		}
		return 0;
}
