#include "voxels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../cgimg/cgimg.h"
#include "voxels_minecraft.h"

//
//
//

Voxel::Voxel ()
{
	m_bActivated = false;
	//m_pData = NULL;
}

Voxel::~Voxel ()
{
}

//
//
//

Voxels::Voxels (unsigned int _nx, unsigned int _ny, unsigned int _nz)
{
	init (_nx, _ny, _nz);
	m_pPalette = NULL;
}

Voxels::~Voxels ()
{
	for (unsigned int i=0; i<m_nx; i++)
	{
		for (unsigned int j=0; j<m_ny; j++)
		{
			delete [] m_pVoxels[i][j];
		}
		delete m_pVoxels[i];
	}
	delete m_pVoxels;
}

int Voxels::init (unsigned int _nx, unsigned int _ny, unsigned int _nz)
{
	m_nx = _nx;
	m_ny = _ny;
	m_nz = _nz;
	m_pVoxels = new Voxel**[m_nx];
	for (unsigned int i=0; i<m_nx; i++)
	{
		m_pVoxels[i] = new Voxel*[m_ny];
		for (unsigned int j=0; j<m_ny; j++)
		{
			m_pVoxels[i][j] = new Voxel[m_nz];
			for (unsigned int k=0; k<m_nz; k++)
			{
				m_pVoxels[i][j][k].m_bActivated = false;
				m_pVoxels[i][j][k].m_fData = 0.;
				m_pVoxels[i][j][k].m_iLabel = 0.;
			}
		}
	}

	return 1;
}

static float convert_float(float in)
{
	float out;

	char *p_in = (char *) &in;
	char *p_out = (char *) &out;
	p_out[0] = p_in[3];
	p_out[1] = p_in[2];
	p_out[2] = p_in[1];
	p_out[3] = p_in[0];
	return out;
}

int Voxels::export_palette_to_mtl (char *mtlfile)
{
	FILE *ptr = fopen (mtlfile, "w");
	for (unsigned int i=0; i<m_pPalette->m_nColors; i++)
	{
		fprintf (ptr, "newmtl material_%d\n", i);
		fprintf (ptr, "Ns 0.000000\n");
		fprintf (ptr, "Ka 0.000000 0.000000 0.000000\n");
		fprintf (ptr, "Kd %f %f %f\n", m_pPalette->m_pColors[i].r()/255., m_pPalette->m_pColors[i].g()/255., m_pPalette->m_pColors[i].b()/255.);
		fprintf (ptr, "Ks 0.000000 0.000000 0.000000\n");
		fprintf (ptr, "Ni 1.000000\n");
		fprintf (ptr, "d 1.000000\n");
		fprintf (ptr, "illum 2\n");
		fprintf (ptr, "\n");
	}
	fclose (ptr);
	return 0;
}

int Voxels::input_vxl (char *filename)
{
	FILE *ptr = fopen (filename, "r");

	char header[3];
	fscanf (ptr, "%c%c%c\n", &header[0], &header[1], &header[2]);
	//printf ("%c %c %c\n", header[0], header[1], header[2]);

	unsigned int dims[3];
	fscanf (ptr, "%d %d %d\n", &dims[0], &dims[1], &dims[2]);
	init (dims[0], dims[1], dims[2]);

	// get the colors & create the palette
	unsigned int ncolors;
	fscanf (ptr, "%d\n", &ncolors);
	m_pPalette = new Palette ();
	unsigned int r, g, b;
	for (unsigned int i=0; i<ncolors; i++)
	{
		fscanf (ptr, "%d %d %d\n", &r, &g, &b);
		m_pPalette->AddColor (r, g, b, 255);
		//printf ("%d %d %d\n", r, g, b);
	}

	unsigned int icolor;
	char buffer[125];
	for (unsigned int k=0; k<m_nz; k++)
	{
		for (unsigned int j=0; j<m_ny; j++)
		{
			for (unsigned int i=0; i<m_nx; i++)
			{
				fscanf (ptr, "%d ", &icolor);
				//printf ("%d ", icolor);

				m_pVoxels[i][j][k].m_bActivated = (icolor != 0);
				m_pVoxels[i][j][k].m_iLabel = icolor-1;
			}
		}
		char sep;
		fscanf (ptr, "%c\n", &sep); // '#' separator
	}

	fclose (ptr);
	export_palette_to_mtl ("toto_pixelart.mtl");
	return 0;
}

int Voxels::input_img (Img *img)
{
	if (img == NULL)
			return -1;
	init (img->width(), img->height(), 1);

	m_pPalette = new Palette ();

	unsigned char r_bg = img->m_pPixels[0];
	unsigned char g_bg = img->m_pPixels[1];
	unsigned char b_bg = img->m_pPixels[2];
	for (unsigned int j=0; j<m_ny; j++)
		for (unsigned int i=0; i<m_nx; i++)
		{
			if (img->m_pPixels[4*(img->width()*j+i)]   == r_bg &&
			    img->m_pPixels[4*(img->width()*j+i)+1] == g_bg &&
			    img->m_pPixels[4*(img->width()*j+i)+2] == b_bg)
				m_pVoxels[i][j][0].m_bActivated = false;
			else
			{
				m_pVoxels[i][j][0].m_bActivated = true;
				//m_pVoxels[i][j][0].m_iLabel = img->width*j+i;
				m_pVoxels[i][j][0].m_iLabel = m_pPalette->AddColor (img->m_pPixels[4*(img->width()*j+i)] ,
										    img->m_pPixels[4*(img->width()*j+i)+1],
										    img->m_pPixels[4*(img->width()*j+i)+2],
										    img->m_pPixels[4*(img->width()*j+i)+3]);
			}
		}

	export_palette_to_mtl ((char*)"toto_pixelart.mtl");
}

int Voxels::input_imgs (Img **imgs, unsigned int nImgs)
{
	if (imgs == NULL)
			return -1;
	init (imgs[0]->width(), imgs[0]->height(), nImgs);

	m_pPalette = new Palette ();

	for (unsigned int k=0; k<nImgs; k++)
	{
		Img *img = imgs[k];
		if (!img)
		{
			printf ("img is nill (%d/%d)\n", k, nImgs);
			continue;
		}
		printf ("imgs[%d/%d] : %d %d\n", k, nImgs, img->width(), img->height());
		unsigned char r_bg = img->m_pPixels[0];
		unsigned char g_bg = img->m_pPixels[1];
		unsigned char b_bg = img->m_pPixels[2];
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int i=0; i<m_nx; i++)
			{
				if (img->m_pPixels[4*(img->width()*j+i)] == r_bg
					&& img->m_pPixels[4*(img->width()*j+i)+1] == g_bg
					&& img->m_pPixels[4*(img->width()*j+i)+2] == b_bg)
					m_pVoxels[i][j][k].m_bActivated = false;
				else
				{
					m_pVoxels[i][j][k].m_bActivated = true;
					//m_pVoxels[i][j][k].m_iLabel = img->width*j+i;
					m_pVoxels[i][j][k].m_iLabel = m_pPalette->AddColor (img->m_pPixels[4*(img->width()*j+i)] ,
																		img->m_pPixels[4*(img->width()*j+i)+1],
																		img->m_pPixels[4*(img->width()*j+i)+2],
																		img->m_pPixels[4*(img->width()*j+i)+3]);
				}
			}
	}

	FILE *ptr = fopen ("toto_pixelart.mtl", "w");
	for (unsigned int i=0; i<m_pPalette->m_nColors; i++)
	{
		fprintf (ptr, "newmtl material_%d\n", i);
		fprintf (ptr, "Ns 0.000000\n");
		fprintf (ptr, "Ka 0.000000 0.000000 0.000000\n");
		fprintf (ptr, "Kd 0.000000 0.000000 0.000000\n");
		fprintf (ptr, "Ks %f %f %f\n", m_pPalette->m_pColors[i].r()/255., m_pPalette->m_pColors[i].g()/255., m_pPalette->m_pColors[i].b()/255.);
		fprintf (ptr, "Ni 1.000000\n");
		fprintf (ptr, "d 1.000000\n");
		fprintf (ptr, "illum 2\n");
		fprintf (ptr, "\n");
	}
	fclose (ptr);
}

int Voxels::input (char *filename)
{
	FILE *ptr = fopen (filename, "rb");

	unsigned int i, j, k;
	float data;
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				fread (&data, sizeof(float), 1, ptr);
				data = convert_float (data);
				
				if (i>150 && k>150)// && k>100)
					continue;

				m_pVoxels[i][j][k].m_bActivated = (data != 0.);
				m_pVoxels[i][j][k].m_fData = data;
			}
	
	fclose (ptr);
	
	return 0;
}

float Voxels::get_data (unsigned int i, unsigned int j, unsigned int k)
{
	return m_pVoxels[i][j][k].m_fData;
}

float Voxels::get_data_for_intersection (unsigned int i, unsigned int j, unsigned int k)
{
	int n=3;
	float fDataMean = 0.;
	unsigned int iNeighbours = 0;
	for (int ii=-n; ii<n; ii++)
		for (int jj=-n; jj<n; jj++)
			for (int kk=-n; kk<n; kk++)
			{
				if ((i+ii) >= 0 && (i+ii) < m_nx &&
				    (j+jj) >= 0 && (j+jj) < m_ny &&
				    (k+kk) >= 0 && (k+kk) < m_nz &&
				    m_pVoxels[i+ii][j+jj][k+kk].m_bActivated)
				{
					fDataMean += m_pVoxels[i+ii][j+jj][k+kk].m_fData;
					iNeighbours++;	
				}
			}
	if (iNeighbours == 0)
		return 0.;
	float fRes = fDataMean / (float)iNeighbours;
	return fRes;
}

void Voxels::get_extremal_values (float *_min, float *_max)
{
	float min = 0., max = 0.;
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
				if (m_pVoxels[i][j][k].m_bActivated)
				{
					if (min>m_pVoxels[i][j][k].m_fData) min = m_pVoxels[i][j][k].m_fData;
					if (max<m_pVoxels[i][j][k].m_fData) max = m_pVoxels[i][j][k].m_fData;
				}
	*_min = min;
	*_max = max;
}

void Voxels::smooth_data (int n)
{
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				if (!m_pVoxels[i][j][k].m_bActivated)
					continue;

				unsigned int iNeighbours = 0;
				float fDataMean = 0.;
				for (int ii=-n; ii<=n; ii++)
					for (int jj=-n; jj<=n; jj++)
						for (int kk=0; kk<=n; kk++)
						{
							if ((i+ii) >= 0 && (i+ii) < m_nx &&
							    (j+jj) >= 0 && (j+jj) < m_ny &&
							    (k+kk) >= 0 && (k+kk) < m_nz &&
							    m_pVoxels[i+ii][j+jj][k+kk].m_bActivated)
							{
								fDataMean += m_pVoxels[i+ii][j+jj][k+kk].m_fData;
								iNeighbours++;	
							}
						}
				//m_pVoxels[i][j][k].m_fData = fDataMean / iNeighbours;
			}
}

void Voxels::threshold_data (float threshold)
{
	float min, max;
	get_extremal_values (&min, &max);
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				if (m_pVoxels[i][j][k].m_bActivated)
				{
					if (fabs (m_pVoxels[i][j][k].m_fData) < threshold)
						m_pVoxels[i][j][k].m_bActivated = false;
					else
					{
						float data = m_pVoxels[i][j][k].m_fData;
						//data = (data<0.)? data+(data-min)/2. : data-(max-data);
						m_pVoxels[i][j][k].m_fData = data;
					}
				}
			}
}

void Voxels::inverse_activation (void)
{
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
				m_pVoxels[i][j][k].m_bActivated = !m_pVoxels[i][j][k].m_bActivated;
}


// morphologic operators

void Voxels::dilation (void)
{
	reset_labels ();
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				if (m_pVoxels[i][j][k].m_bActivated)
					continue;
				
				// 6-neighborough
				if (i!=0 && m_pVoxels[i-1][j][k].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i-1][j][k].m_fData;
					continue;
				}
				if (i!=(m_nx-1) && m_pVoxels[i+1][j][k].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;	
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i+1][j][k].m_fData;
					continue;
				}

				if (j!=0 && m_pVoxels[i][j-1][k].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;	
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i][j-1][k].m_fData;
					continue;
				}
				if (j!=(m_ny-1) && m_pVoxels[i][j+1][k].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;	
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i][j+1][k].m_fData;
					continue;
				}

				if (k!=0 && m_pVoxels[i][j][k-1].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;	
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i][j][k-1].m_fData;
					continue;
				}
				if (k!=(m_nz-1) && m_pVoxels[i][j][k+1].m_bActivated)
				{
					m_pVoxels[i][j][k].m_iLabel = 1;	
					m_pVoxels[i][j][k].m_fData = m_pVoxels[i][j][k+1].m_fData;
					continue;
				}

				// 8-neighborough
			}
	
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
				if (m_pVoxels[i][j][k].m_iLabel == 1)
					m_pVoxels[i][j][k].m_bActivated = true;
	reset_labels ();
}

void Voxels::reset_labels (void)
{
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
				m_pVoxels[i][j][k].m_iLabel = 0;
}


//
// export methods
//

int Voxels::export_slice_XY (char *prefix, unsigned int islice)
{
	unsigned int k = islice;
	float min, max;
	//get_extremal_values (&min, &max);
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
		{
			if (min>m_pVoxels[i][j][k].m_fData) min = m_pVoxels[i][j][k].m_fData;
			if (max<m_pVoxels[i][j][k].m_fData) max = m_pVoxels[i][j][k].m_fData;
		}

	char filename[256];

	// export data
	{
		sprintf (filename, "%s_%04d.csv", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		for (unsigned int i=0; i<m_nx; i++)
		{
			for (unsigned int j=0; j<m_ny; j++)
			{
				fprintf (ptr, "%f;", m_pVoxels[i][j][k].m_fData);
			}
			fprintf (ptr, "\n");
		}
		fclose (ptr);
	}

	// export pgm
	{
		sprintf (filename, "%s_%04d.pgm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P2\n%d %d\n255\n", m_nx, m_ny);
		for (unsigned int i=0; i<m_nx; i++)
			for (unsigned int j=0; j<m_ny; j++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "0\n");
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					fprintf (ptr, "%d\n", (unsigned int)(255*datan));
				}
			}
		fclose (ptr);
	}
	
	// export ppm
	{
		sprintf (filename, "%s_%04d.ppm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P3\n%d %d\n255\n", m_nx, m_ny);
		for (unsigned int i=0; i<m_nx; i++)
			for (unsigned int j=0; j<m_ny; j++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "%d %d %d\n", value, value, value);
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					float r, g, b;
					color_jet (datan, &r, &g, &b);
					fprintf (ptr, "%d %d %d\n",
						 (unsigned int)(255*r), (unsigned int)(255*g), (unsigned int)(255*b));
				}
			}
		
		fclose (ptr);
	}
	

	return 0;
}

int Voxels::export_slice_XZ (char *prefix, unsigned int islice)
{
	unsigned int j = islice;
	float min, max;
	//get_extremal_values (&min, &max);
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int k=0; k<m_nz; k++)
		{
			if (min>m_pVoxels[i][j][k].m_fData) min = m_pVoxels[i][j][k].m_fData;
			if (max<m_pVoxels[i][j][k].m_fData) max = m_pVoxels[i][j][k].m_fData;
		}

	char filename[256];

	// export data
	{
		sprintf (filename, "%s_%04d.csv", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		for (unsigned int i=0; i<m_nx; i++)
		{
			for (unsigned int k=0; k<m_nz; k++)
			{
				fprintf (ptr, "%f;", m_pVoxels[i][j][k].m_fData);
			}
			fprintf (ptr, "\n");
		}
		fclose (ptr);
	}

	// export pgm
	{
		sprintf (filename, "%s_%04d.pgm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P2\n%d %d\n255\n", m_nx, m_nz);
		for (unsigned int i=0; i<m_nx; i++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "0\n");
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					fprintf (ptr, "%d\n", (unsigned int)(255*datan));
				}
			}
		fclose (ptr);
	}
	
	// export ppm
	{
		sprintf (filename, "%s_%04d.ppm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P3\n%d %d\n255\n", m_nx, m_nz);
		for (unsigned int i=0; i<m_nx; i++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "%d %d %d\n", value, value, value);
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					float r, g, b;
					color_jet (datan, &r, &g, &b);
					fprintf (ptr, "%d %d %d\n",
						 (unsigned int)(255*r), (unsigned int)(255*g), (unsigned int)(255*b));
				}
			}
		
		fclose (ptr);
	}

	return 0;
}

int Voxels::export_slice_YZ (char *prefix, unsigned int islice)
{
	unsigned int i = islice;
	float min, max;
	//get_extremal_values (&min, &max);
	for (unsigned int j=0; j<m_ny; j++)
		for (unsigned int k=0; k<m_nz; k++)
		{
			if (min>m_pVoxels[i][j][k].m_fData) min = m_pVoxels[i][j][k].m_fData;
			if (max<m_pVoxels[i][j][k].m_fData) max = m_pVoxels[i][j][k].m_fData;
		}

	char filename[256];

	// export data
	{
		sprintf (filename, "%s_%04d.csv", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		for (unsigned int j=0; j<m_ny; j++)
		{
			for (unsigned int k=0; k<m_nz; k++)
			{
				fprintf (ptr, "%f;", m_pVoxels[i][j][k].m_fData);
			}
			fprintf (ptr, "\n");
		}
		fclose (ptr);
	}

	// export pgm
	{
		sprintf (filename, "%s_%04d.pgm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P2\n%d %d\n255\n", m_ny, m_nz);
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "0\n");
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					fprintf (ptr, "%d\n", (unsigned int)(255*datan));
				}
			}
		fclose (ptr);
	}
	
	// export ppm
	{
		sprintf (filename, "%s_%04d.ppm", prefix, islice);
		FILE *ptr = fopen (filename, "w");
		fprintf (ptr, "P3\n%d %d\n255\n", m_ny, m_nz);
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
			{
				unsigned int value = 0;
				if (m_pVoxels[i][j][k].m_bActivated == 0.)
					fprintf (ptr, "%d %d %d\n", value, value, value);
				else
				{
					float data = m_pVoxels[i][j][k].m_fData;
					float datan = (data - min) / (max - min);
					float r, g, b;
					color_jet (datan, &r, &g, &b);
					fprintf (ptr, "%d %d %d\n",
						 (unsigned int)(255*r), (unsigned int)(255*g), (unsigned int)(255*b));
				}
			}
		
		fclose (ptr);
	}
	

	return 0;
}

int Voxels::triangulate (char *filename)
{
	// 0 : interpolation on texture 1D
	// 1 : texture 2D
	unsigned int eTextureMode = 1;

	// 0 : use m_iLabel as a material id
	// 1 : 
	unsigned int eMaterialMode = 0;

	unsigned int i,j,k;
	unsigned int nx = m_nx;
	unsigned int ny = m_ny;
	unsigned int nz = m_nz;
	float xmin, xmax, ymin, ymax, zmin, zmax;
	//xmin = 0.0; xmax = 1.0; ymin = 0.0; ymax = 1.0; zmin = 0.0; zmax = 1.0;
	xmin = 0.0; xmax = nx; ymin = 0.0; ymax = ny; zmin = 0.0; zmax = nz; // pixelart

	unsigned int index, index1, index2, index3, index4;
	unsigned int nv = (nx+1)*(ny+1)*(nz+1);
	float *v = (float*)malloc(3*nv*sizeof(float));
	unsigned int nf = 5*nv;
	int *f = (int*)malloc(4*nf*sizeof(int)); // indices vertices
	float *t = NULL;
	if (eTextureMode == 0)
		t = (float*)malloc(4*nf*sizeof(float)); // texture coordinates
	else if (eTextureMode == 1)
		t = (float*)malloc(8*nf*sizeof(float)); // texture coordinates
	unsigned int *m = (unsigned int*)malloc(4*nf*sizeof(unsigned int)); // material
	printf ("Waiting for %d vertices and %d faces...\n", nv, nf);
	
	// look for extremal values
	float min = 0., max = 0.;
	get_extremal_values (&min, &max);
	printf ("extremal values : %f -> %f\n", min, max);

	// vertices
	for (i=0; i<=nx; i++)
	{
		for (j=0; j<=ny; j++)
		{
			for (k=0; k<=nz; k++)
			{
				index = (nx+1)*(ny+1)*k + (nx+1)*j + i;
				v[3*index]   = xmin + i*(xmax-xmin)/nx;
				v[3*index+1] = ymin + j*(ymax-ymin)/ny;
				v[3*index+2] = zmin + k*(zmax-zmin)/nz;
			}
		}
	}

	// faces
	int fwalk = 0;
	for (i=0; i<nx; i++)
	{
		for (j=0; j<ny; j++)
		{
			for (k=0; k<nz; k++)
			{
				if (m_pVoxels[i][j][k].m_bActivated)
				{
					if (i == 0 || !m_pVoxels[i-1][j][k].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*k + (nx+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i;
						index3 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i;

						f[4*fwalk]   = index3;
						f[4*fwalk+1] = index4;
						f[4*fwalk+2] = index2;
						f[4*fwalk+3] = index1;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i, j, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i, j, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i, j+1, k);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+1];

						fwalk ++;
					}
					
					if (i==nx-1 || !m_pVoxels[i+1][j][k].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*k + (nx+1)*j + i+1;
						index2 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i+1;
						index4 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i+1;

						f[4*fwalk]   = index1;
						f[4*fwalk+1] = index2;
						f[4*fwalk+2] = index4;
						f[4*fwalk+3] = index3;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i+1, j, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i+1, j+1, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i+1, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i+1, j, k+1);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+1];

						fwalk ++;
					}
					
					if (j==0 || !m_pVoxels[i][j-1][k].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*k + (nx+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (nx+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i+1;

						f[4*fwalk]   = index1;
						f[4*fwalk+1] = index2;
						f[4*fwalk+2] = index4;
						f[4*fwalk+3] = index3;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i, j, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i+1, j, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i+1, j, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i, j, k+1);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+2];

						fwalk ++;
					}
					
					if (j==ny-1 || !m_pVoxels[i][j+1][k].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i;
						index2 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i+1;

						f[4*fwalk]   = index2;
						f[4*fwalk+1] = index1;
						f[4*fwalk+2] = index3;
						f[4*fwalk+3] = index4;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i+1, j+1, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i, j+1, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i+1, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+0];

						fwalk ++;
					}
					if (k==0 || !m_pVoxels[i][j][k-1].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*k + (nx+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (nx+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*k + (nx+1)*(j+1) + i+1;

						f[4*fwalk]   = index1;
						f[4*fwalk+1] = index3;
						f[4*fwalk+2] = index4;
						f[4*fwalk+3] = index2;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i, j, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i, j+1, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i+1, j+1, k);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i+1, j, k);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+1];

						fwalk ++;
					}
					
					if (k==nz-1 || !m_pVoxels[i][j][k+1].m_bActivated)
					{
						index1 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i;
						index2 = (nx+1)*(ny+1)*(k+1) + (nx+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (nx+1)*(j+1) + i+1;

						f[4*fwalk]   = index2;
						f[4*fwalk+1] = index4;
						f[4*fwalk+2] = index3;
						f[4*fwalk+3] = index1;

						if (eTextureMode == 0)
						{
							t[4*fwalk]   = get_data_for_intersection (i, j, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+1] = get_data_for_intersection (i+1, j, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+2] = get_data_for_intersection (i+1, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
							t[4*fwalk+3] = get_data_for_intersection (i, j+1, k+1);//m_pVoxels[i][j][k].m_fData;
						}
						else if (eTextureMode == 1)
						{
							t[8*fwalk]   = 0.;
							t[8*fwalk+1] = 0.;
							t[8*fwalk+2] = 0.;
							t[8*fwalk+3] = 1.;
							t[8*fwalk+4] = 1.;
							t[8*fwalk+5] = 1.;
							t[8*fwalk+6] = 1.;
							t[8*fwalk+7] = 0.;
						}

						if (eMaterialMode == 0)
							m[fwalk] = m_pVoxels[i][j][k].m_iLabel;
						else
							m[fwalk] = block_to_texture_id[3*m_pVoxels[i][j][k].m_iLabel+1];

						fwalk ++;
					}
				}
			}
		}
	}

	nf = fwalk;
	printf ("final number of faces : %d\n", nf);

	// delete useless vertices
	char *v_used = (char*)malloc(nv*sizeof(char));
	v_used = (char*)memset((void*)v_used, 0, nv*sizeof(char));
	for (i=0; i<4*nf; i++)
	{
		if (f[i] > nv)
			printf ("Warning : %d > %d\n", f[i], nv);
		v_used[f[i]] = 1;
	}
	
	int *new_indices = (int*)malloc(nv*sizeof(int));
	int iwalk = 0;
	for (i=0; i<nv; i++)
		if (v_used[i])
			new_indices[i] = iwalk++;
	int nv2 = iwalk;
	
	float *v2 = (float*)malloc(3*nv2*sizeof(float));
	iwalk = 0;
	for (i=0; i<nv; i++)
	{
		if (v_used[i])
		{
			v2[3*iwalk]   = v[3*i];
			v2[3*iwalk+1] = v[3*i+1];
			v2[3*iwalk+2] = v[3*i+2];
			iwalk++;
		}
	}
	
	int *f2 = (int*)malloc(4*nf*sizeof(int));
	for (i=0; i<4*nf; i++) f2[i] = new_indices[f[i]];
	
	free (v);
	v = v2;
	nv = nv2;
	free (f);
	f = f2;

	/* output the mesh in obj file */
	FILE *out = fopen (filename, "w");
	
	/* materials */

	if (0) // nbt minecraft
	{
		FILE *ptr = fopen ("toto_nbt.mtl", "w");
		for (unsigned int i=0; i<256; i++)
		{
			fprintf (ptr, "newmtl material_%d\n", i);
			fprintf (ptr, "map_Kd ./blocks/block%04d.ppm\n", i);
			fprintf (ptr, "\n");
		}
		fclose (ptr);

	}

	//fprintf (out, "mtllib toto_sun.mtl\n");
	fprintf (out, "mtllib toto_pixelart.mtl\n");
	//fprintf (out, "mtllib toto_nbt.mtl\n");

	/* vertices */
	for (unsigned int i=0; i<nv; i++)
		fprintf (out, "v %f %f %f\n", v[3*i], v[3*i+1], v[3*i+2]);

	/* create faces */
	//min/=6.;
	//max/=6.;
	printf ("values : %f -> %f\n", min, max);
	if (max == min)
	{
		max = 1.;
		min = 0.;
	}

	if (0) // sun || nbt
	{
		/* materials */
		fprintf (out, "usemtl material_1\n");

		unsigned int face = 0;
		for (unsigned int i=0; i<nf; i++)
		{
			if (0)
			{
				fprintf (out, "vt %f 1.\n", (t[4*i]-min)/(max-min));
				fprintf (out, "vt %f 1.\n", (t[4*i+1]-min)/(max-min));
				fprintf (out, "vt %f 1.\n", (t[4*i+2]-min)/(max-min));
				fprintf (out, "vt %f 1.\n", (t[4*i+3]-min)/(max-min));
			}
			else
			{
				if (eTextureMode == 0)
				{
					fprintf (out, "vt %f 1.\n", (t[4*i]<0.)? 0.1 : 0.9);
					fprintf (out, "vt %f 1.\n", (t[4*i+1]<0.)? 0.1 : 0.9);
					fprintf (out, "vt %f 1.\n", (t[4*i+2]<0.)? 0.1 : 0.9);
					fprintf (out, "vt %f 1.\n", (t[4*i+3]<0.)? 0.1 : 0.9);
				}
				else if (eTextureMode == 1)
				{
					fprintf (out, "vt %f %f\n", t[8*i], t[8*i+1]);
					fprintf (out, "vt %f %f\n", t[8*i+2], t[8*i+3]);
					fprintf (out, "vt %f %f\n", t[8*i+4], t[8*i+5]);
					fprintf (out, "vt %f %f\n", t[8*i+6], t[8*i+7]);
				}
			}
			if (v[3*f[4*i]+1] == v[3*f[4*i+1]+1] && v[3*f[4*i+1]+1] == v[3*f[4*i+2]+1])
				fprintf (out, "usemtl material_%d\n", m[i]);
			else
				fprintf (out, "usemtl material_%d\n", m[i]+1);
			fprintf (out, "f %d/%d %d/%d %d/%d %d/%d\n",
				 1+f[4*i], -4,
				 1+f[4*i+1], -3,
				 1+f[4*i+2], -2,
				 1+f[4*i+3], -1);
			face++;
		}
	}
	else // pixelart
	{
		unsigned int face = 0;
		for (unsigned int i=0; i<nf; i++)
		{
			fprintf (out, "usemtl material_%d\n", m[i]);
			fprintf (out, "f %d %d %d %d\n", 1+f[4*i], 1+f[4*i+1], 1+f[4*i+2], 1+f[4*i+3]);
			face++;
		}
	}
	fclose (out);

	free (m);
	free (t);
	free (v);

	return 0;
}














static float minpx = 0.;
static float minpy = 0.;
static float minpz = 0.;
static float maxpx = 1.;
static float maxpy = 1.;
static float maxpz = 1.;
static float cube_vertices[24] = {minpx, minpy, minpz,
				  maxpx, minpy, minpz,
				  maxpx, maxpy, minpz,
				  minpx, maxpy, minpz,
				  minpx, minpy, maxpz,
				  maxpx, minpy, maxpz,
				  maxpx, maxpy, maxpz,
				  minpx, maxpy, maxpz }; 
static int cube_faces[6*4] = {0, 3, 2, 1,
			      4, 5, 6, 7,
			      1, 2, 6, 5,
			      2, 3, 7, 6,
			      3, 0, 4, 7,
			      0, 1, 5, 4 };

void print_cube (FILE *out, unsigned int i, unsigned j, unsigned int k, float tex)
{
	fprintf (out, "g cube%2d%2d%2d\n", i, j, k);
	//fprintf (out, "usemtl material_%f\n", tex);
	fprintf (out, "usemtl material_%d\n", (unsigned int)tex);

	// vertices
	for (unsigned int ii=0; ii<8; ii++)
	{
		fprintf (out, "v %f %f %f\n",
			 i + cube_vertices[3*ii],
			 j + cube_vertices[3*ii+1],
			 k + cube_vertices[3*ii+2]);
		fprintf (out, "vt %f %f\n",
			 tex,
			 0.);
	}

	// faces
	for (unsigned int ii=0; ii<6; ii++)
		fprintf (out, "f %d/%d %d/%d %d/%d %d/%d\n",
			 1 + cube_faces[4*ii] -9,
			 1 + cube_faces[4*ii] -9,
			 1 + cube_faces[4*ii+1] -9,
			 1 + cube_faces[4*ii+1] -9,
			 1 + cube_faces[4*ii+2] -9,
			 1 + cube_faces[4*ii+2] -9,
			 1 + cube_faces[4*ii+3] -9,
			 1 + cube_faces[4*ii+3] -9 );
}

int Voxels::export_cubes (char *filename)
{
	FILE *out = fopen (filename, "w");
	fprintf (out, "mtllib toto_pixelart.mtl\n");
	for (unsigned int i=0; i<m_nx; i++)
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int k=0; k<m_nz; k++)
				if (m_pVoxels[i][j][k].m_bActivated)
					print_cube (out, i, j, k, m_pVoxels[i][j][k].m_iLabel);
	fclose (out);

	return 0;
}

