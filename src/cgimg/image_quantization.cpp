#include <stdlib.h>

#include "image.h"

#include "image_quantization_heckbert.h"
#include "image_quantization_wu.h"

//#include "octree.h"

//
// quantization
//
#define RGB(r,g,b) (unsigned short)(((b)&~7)<<7)|(((g)&~7)<<2)|((r)>>3)

// Color Image Quantization for Frame Buffer Display
// Paul S. Heckbert
// SIGGRAPH '82, July 1982, pp. 297-307
int Img::quant_heckbert (int ncolors)
{
	unsigned short Hist[32768];
	unsigned char ColMap[32768][3];

	memset (Hist, 0, 32768*sizeof(unsigned short));
	unsigned char r, g, b, a;
	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			get_pixel (i, j, &r, &g, &b, &a);
			Hist[RGB(r,g,b)]++;
		}

	unsigned short res = MedianCut(Hist, ColMap, ncolors);

	for (unsigned int j=0; j<m_iHeight; j++)
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			get_pixel (i, j, &r, &g, &b, &a);
			int ci = Hist[RGB(r,g,b)];
			set_pixel (i, j, ColMap[ci][0], ColMap[ci][1], ColMap[ci][2], a);
		}

	return 0;
}

//
// http://www.ece.mcmaster.ca/~xwu/cq.c
//
int Img::quant_wu (int ncolors)
{
	int res = MedianCut_Wu (m_pPixels, m_iWidth*m_iHeight, ncolors);

	return 0;
}

int Img::quant_kmean (float threshold)
{
	int n = m_iWidth*m_iHeight;
	printf ("n pixels : %d\n", n);

	// convert unsigned char to float
	float *pPixels = (float*)malloc(3*n*sizeof(float));
	for (int i=0; i<n; i++)
	{
		pPixels[3*i]   = m_pPixels[4*i]/255.;
		pPixels[3*i+1] = m_pPixels[4*i+1]/255.;
		pPixels[3*i+2] = m_pPixels[4*i+2]/255.;
	}
	FILE *ptr = fopen ("colors.asc", "w");
	for (int i=0; i<n; i++)
	{
		fprintf (ptr, "%f %f %f %d %d %d 1. 0. 0.\n",
			 pPixels[3*i], pPixels[3*i+1], pPixels[3*i+2],
			 (int)(255*pPixels[3*i]), (int)(255*pPixels[3*i+1]), (int)(255*pPixels[3*i+2]));
	}
	fclose (ptr);

	// select random colors for clusters
	srand (5);
	int nclusters = 64;
	float *pClusters = (float*)malloc(3*nclusters*sizeof(float));
	for (int i=0; i<nclusters; i++)
	{
		int ind = (int)((float)n*rand()/RAND_MAX);
		pClusters[3*i]   = pPixels[3*ind];
		pClusters[3*i+1] = pPixels[3*ind+1];
		pClusters[3*i+2] = pPixels[3*ind+2];
	}
	ptr = fopen ("clusters1.asc", "w");
	for (int i=0; i<nclusters; i++)
	{
		fprintf (ptr, "%f %f %f %d %d %d 1. 0. 0.\n",
			 pClusters[3*i], pClusters[3*i+1], pClusters[3*i+2],
			 (int)(255*pClusters[3*i]), (int)(255*pClusters[3*i+1]), (int)(255*pClusters[3*i+2]));
	}
	fclose (ptr);

	//
	int *pInCluster1 = (int*)malloc(n*sizeof(int));
	int *pInCluster2 = (int*)malloc(n*sizeof(int));
	memset (pInCluster1, 0, n*sizeof(int));
	memset (pInCluster2, 0, n*sizeof(int));

	float *pAccum = (float*)malloc(3*nclusters*sizeof(float));
	int *pPopulation = (int*)malloc(nclusters*sizeof(int));
	float *pClustersDistances = (float*)malloc(nclusters*sizeof(float));
	//int *pClustersIndices = (int*)malloc(nclusters*sizeof(int));

	// loop
	bool bStillMoving = true;
	int ite = 0;
	while (bStillMoving && ite < 20)//for (int ite=0; ite<10; ite++)
	{
		printf ("Iteration %d\n", ite);

		memset (pAccum, 0, 3*nclusters*sizeof(float));
		memset (pPopulation, 0, nclusters*sizeof(int));
		memset (pClustersDistances, 0, nclusters*sizeof(float));
		for (int i=0; i<n; i++)
		{
			vec3 c, mean;
			vec3_init (c, pPixels[3*i], pPixels[3*i+1], pPixels[3*i+2]);
			for (int j=0; j<nclusters; j++)
			{
				vec3_init (mean, pClusters[3*j], pClusters[3*j+1], pClusters[3*j+2]);
				pClustersDistances[j] = vec3_distance (c, mean);

			}
			int ci = 0;
			for (int j=1; j<nclusters; j++)
			{
				if (pClustersDistances[ci]>pClustersDistances[j])
				{
					ci = j;
					pInCluster2[i] = j;
				}
			}
			pAccum[3*ci]   += c[0];
			pAccum[3*ci+1] += c[1];
			pAccum[3*ci+2] += c[2];
			pPopulation[ci]++;
		}

		for (int i=0; i<nclusters; i++)
		{
			if (pPopulation[i] == 0)
				continue;
			pClusters[3*i]   = pAccum[3*i]/pPopulation[i];
			pClusters[3*i+1] = pAccum[3*i+1]/pPopulation[i];
			pClusters[3*i+2] = pAccum[3*i+2]/pPopulation[i];
			//printf ("%f %f %f\n", pClusters[3*i], pClusters[3*i+1], pClusters[3*i+2]);
		}

		// still moving ?
		bStillMoving = false;
		for (int i=0; i<nclusters; i++)
		{
			if (pInCluster1[i] != pInCluster2[i])
			{
				bStillMoving = true;
				break;
			}
		}
		memcpy (pInCluster1, pInCluster2, nclusters*sizeof(int));
		ite++;
	}

	for (int i=0; i<n; i++)
	{
		vec3 c, cnew;
		float dmin;
		vec3_init (c, pPixels[3*i], pPixels[3*i+1], pPixels[3*i+2]);
		vec3_init (cnew, pClusters[0], pClusters[1], pClusters[2]);
		dmin = vec3_distance (c, cnew);
		for (int j=1; j<nclusters; j++)
		{
			vec3 cwalk;
			vec3_init (cwalk, pClusters[3*j], pClusters[3*j+1], pClusters[3*j+2]);
			float d = vec3_distance (c, cwalk);
			if (d < dmin)
				vec3_copy (cnew, cwalk);
		}
		m_pPixels[4*i]   = (int)255.*cnew[0];
		m_pPixels[4*i+1] = (int)255.*cnew[1];
		m_pPixels[4*i+2] = (int)255.*cnew[2];
		m_pPixels[4*i+3] = 255;
	}

	ptr = fopen ("clusters2.asc", "w");
	for (int i=0; i<nclusters; i++)
	{
		fprintf (ptr, "%f %f %f %d %d %d 1. 0. 0.\n",
			 pClusters[3*i], pClusters[3*i+1], pClusters[3*i+2],
			 (int)(255*pClusters[3*i]), (int)(255*pClusters[3*i+1]), (int)(255*pClusters[3*i+2]));
	}
	fclose (ptr);

	// look for the final number of colors
	//int *pIndices = (int*)malloc(m_iWidth*m_iHeight*sizeof(int));
	unsigned char *pPalette = (unsigned char*)malloc(3*m_iWidth*m_iHeight*sizeof(unsigned char));
	int ncolors = 0;
	for (int i=0; i<3*nclusters; i++)
		pPalette[i] = (int)(255.*pClusters[i]);
	for (int i=0; i<nclusters-1; i++)
	{
		int r = pPalette[3*i];
		int g = pPalette[3*i+1];
		int b = pPalette[3*i+2];
		for (int j=i+1; j<nclusters;j++)
		{
			int r2 = pPalette[3*j];
			int g2 = pPalette[3*j+1];
			int b2 = pPalette[3*i+2];
			if (r==r2 && g==g2 && b==b2)
			{
				pPalette[3*j]   = pPalette[3*(nclusters-1)];
				pPalette[3*j+1] = pPalette[3*(nclusters-1)+1];
				pPalette[3*j+2] = pPalette[3*(nclusters-1)+2];
				nclusters--;
			}
		}
	}
	printf ("found %d colors\n", nclusters);
	//for (int i=0; i<nclusters; i++)
	//	printf ("%d %d %d\n", pPalette[3*i], pPalette[3*i+1], pPalette[3*i+2]);

	// cleaning
	//free (pPixels);
	//delete pOctree;

	return 0;
}
