#include <stdlib.h>

#include "image.h"

int Img::filter_sobel ()
{
	unsigned char *pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*m_iWidth*m_iHeight*sizeof(unsigned char));
	char filterx[3][3] = { {-1, -2, -1}, {0, 0, 0},  {1, 2, 1} };
	char filtery[3][3] = { {-1, 0, 1},   {-2, 0, 2}, {-1, 0, 1} };
	unsigned char mrgb[3][3][3];
	int Gx[3], Gy[3];
	unsigned char a;

	// visit the image
	int i, j;
	int iWidth = m_iWidth;
	int iHeight = m_iHeight;
	for (j=0; j<iHeight; j++)
		for (i=0; i<iWidth; i++)
		{
			// visit the neighborough
			for (int k=-1; k<2; k++)
				for (int l=-1; l<2; l++)
				{
					int ii = i+k;
					int jj = j+l;
					if (ii < 0)
						ii = -ii;
					if (ii > iWidth-1)
						ii = 2 * iWidth - 1 - ii;
					if (jj < 0)
						jj = -jj;
					if (jj > iHeight-1)
						jj = 2 * iHeight - 1 - jj;

					get_pixel (ii, jj, &mrgb[0][k+1][l+1], &mrgb[1][k+1][l+1], &mrgb[2][k+1][l+1], &a);
				}
			
			// apply the filter
			memset (Gx, 0, 3*sizeof(int));
			memset (Gy, 0, 3*sizeof(int));
			for (int k=0; k<3; k++)
				for (int l=0; l<3; l++)
					for (int m=0; m<3; m++) // for each composant r g b
					{
						Gx[m] += filterx[k][l]*mrgb[m][k][l];
						Gy[m] += filtery[k][l]*mrgb[m][k][l];
					}
			int v;
			for (int m=0; m<3; m++)
			{
				v = fabs((float)Gx[m]) + fabs((float)Gy[m]);
				pPixels[4*(j*m_iWidth+i)+m]   = (v>255)? 255 : v;
			}
			pPixels[4*(j*m_iWidth+i)+3] = 255;
		}

	free (m_pPixels);
	m_pPixels = pPixels;

	return 0;
}

int Img::filter (mat3 m, float divide, float decay)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*m_iWidth*m_iHeight*sizeof(unsigned char));

	if (divide == 0.)
	{
		for (int j=0; j<3; j++)
			for (int i=0; i<3; i++)
				divide += m[i][j];
	}
	if (divide == 0.)
		divide = 1.;
	int iWidth = m_iWidth;
	int iHeight = m_iHeight;
	for (int j=1; j<iHeight-1; j++)
		for (int i=1; i<iWidth-1; i++)
		{
			unsigned char r, g, b, a;
			float accum[4];
			memset (accum, 0, 4*sizeof(float));
			for (int jj=0; jj<3; jj++)
				for (int ii=0; ii<3; ii++)
				{
					get_pixel (i+ii-1, j+jj-1, &r, &g, &b, &a);
					accum[0] += m[ii][jj]*(float)r;
					accum[1] += m[ii][jj]*(float)g;
					accum[2] += m[ii][jj]*(float)b;
					accum[3] += m[ii][jj]*(float)a;
				}
			for (int k=0; k<4; k++)
			{
				accum[k] /= divide;
				if (accum[k] < 0)
					accum[k] = 0.;
				if (accum[k] > 255.)
					accum[k] = 255.;
				unsigned char level = (unsigned char)accum[k];
				pPixels[4*(j*m_iWidth+i)+k] = level;
			}
		}

	free (m_pPixels);
	m_pPixels = pPixels;

	return 0;
}

int Img::blur (void)
{
	mat3 m;
	mat3_init (m,
		   1., 1., 1.,
		   1., 1., 1.,
		   1., 1., 1.);
	return filter (m);
}

int Img::gaussian_blur (void)
{
//	{1.0f/273.0f, 4.0f/273.0f, 7.0f/273.0f, 4.0f/273.0f, 1.0f/273.0f},
//	{4.0f/273.0f, 16.0f/273.0f, 26.0f/273.0f, 16.0f/273.0f, 4.0f/273.0f},
//	{7.0f/273.0f, 26.0f/273.0f, 41.0f/273.0f, 26.0f/273.0f, 7.0f/273.0f},
//	{4.0f/273.0f, 16.0f/273.0f, 26.0f/273.0f, 16.0f/273.0f, 4.0f/273.0f},             
//	{1.0f/273.0f, 4.0f/273.0f, 7.0f/273.0f, 4.0f/273.0f, 1.0f/273.0f}};
	return 0;
}

//
// "Bilateral Filtering for Gray and Color Images"
// http://users.soe.ucsc.edu/~manduchi/Papers/ICCV98.pdf
//
int Img::bilateral_filtering (void)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*m_iWidth*m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*m_iWidth*m_iHeight*sizeof(unsigned char));

	int n = 6;
	float sigmaD = 5.;
	float sigmaR = 100.;

	float sigmaD2inv = 1. / (sigmaD*sigmaD);
	float sigmaR2inv = 1. / (sigmaR*sigmaR);
	unsigned char ro, go, bo, ao;
	unsigned char r, g, b, a;
	int i, ii, iii, j, jj, jjj;
	int iWidth = m_iWidth;
	int iHeight = m_iHeight;
	for (j=0; j<iHeight; j++)
		for (i=0; i<iWidth; i++)
		{
			get_pixel (i, j, &ro, &go, &bo, &ao);
			float accum[4] = {0., 0., 0., 0.};
			float norm = 0.;
			for (jj=-n/2; jj<=n/2; jj++)
				for (ii=-n/2; ii<=n/2; ii++)
				{
					iii = i + ii;
					jjj = j + jj;
					if (iii < 0)
						iii -= iii;
					if (iii > iWidth-1)
						iii = 2 * iWidth - 1 - iii;
					if (jjj < 0)
						jjj -= jjj;
					if (jjj > iHeight-1)
						jjj = 2 * iHeight - 1 - jjj;
						
						
					get_pixel (iii, jjj, &r, &g, &b, &a);

					// closeness function
					float c = exp (-0.5*(ii*ii+jj*jj)*sigmaD2inv);

					// similarity function
					float s = exp (-0.5*((r-ro)*(r-ro)+(g-go)*(g-go)+(b-bo)*(b-bo))*sigmaR2inv);
					
					//printf ("%f %f\n", c, s);
					norm += c*s;

					accum[0] += c*s*(float)r;
					accum[1] += c*s*(float)g;
					accum[2] += c*s*(float)b;
					accum[3] += c*s*(float)a;
				}
			//printf ("%f %f %f\n", accum[0], accum[1], accum[2]);
			for (int k=0; k<4; k++)
			{
				accum[k] /= norm;
				if (accum[k] < 0)
					accum[k] = 0.;
				if (accum[k] > 255.)
					accum[k] = 255.;
				unsigned char level = (unsigned char)accum[k];
				pPixels[4*(j*m_iWidth+i)+k] = level;
			}
		}

	free (m_pPixels);
	m_pPixels = pPixels;

	return 0;
}

int Img::saturate (float t)
{
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			unsigned char r, g, b, a, avg;

			get_pixel (i, j, &r, &g, &b, &a);
			avg = ( r + g + b ) / 3;

			r = CLAMP ((avg + t * (r - avg)), 0, 255);
			g = CLAMP ((avg + t * (g - avg)), 0, 255);
			b = CLAMP ((avg + t * (b - avg)), 0, 255);
		
			set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int Img::brightness (float t)
{
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			unsigned char r, g, b, a;

			get_pixel (i, j, &r, &g, &b, &a);

			r = CLAMP (t * r, 0, 255);
			g = CLAMP (t * g, 0, 255);
			b = CLAMP (t * b, 0, 255);
		
			set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int Img::gamma (float t)
{
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			unsigned char r, g, b, a;

			get_pixel (i, j, &r, &g, &b, &a);

			r = CLAMP (pow(r, t), 0, 255);
			g = CLAMP (pow(g, t), 0, 255);
			b = CLAMP (pow(b, t), 0, 255);
		
			set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int Img::sepia (void)
{
	unsigned int w = m_iWidth;
	unsigned int h = m_iHeight;
	for (unsigned int j=0; j<h; j++)
		for (unsigned int i=0; i<w; i++)
		{
			unsigned char r, g, b, a;

			get_pixel (i, j, &r, &g, &b, &a);

			set_pixel (i, j,
				   CLAMP ((r * 0.393) + (g * 0.769) + (b * 0.189), 0, 255),
				   CLAMP ((r * 0.349) + (g * 0.686) + (b * 0.168), 0, 255),
				   CLAMP ((r * 0.272) + (g * 0.534) + (b * 0.131), 0, 255),
				   a);

			set_pixel (i, j,
				   CLAMP (r + 40, 0, 255),
				   CLAMP (g + 20, 0, 255),
				   CLAMP (b - 20, 0, 255),
				   a);

		}
		return 0;
}

