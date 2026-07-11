#include "voxels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../cgimg/cgimg.h"
#include "mesh.h"
#include "voxels_minecraft.h"

//
//
//

Voxel::Voxel ()
{
	m_bActivated = false;
	//m_pData = nullptr;
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
	m_pPalette = nullptr;
}

Voxels::~Voxels ()
{
	for (unsigned int i=0; i<m_nx; i++)
	{
		for (unsigned int j=0; j<m_ny; j++)
		{
			delete [] m_pVoxels[i][j];
		}
		delete [] m_pVoxels[i];
	}
	delete [] m_pVoxels;
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
	export_palette_to_mtl ((char*)"toto_pixelart.mtl");
	return 0;
}

int Voxels::input_img (Img *img)
{
	if (img == nullptr)
			return -1;
	init (img->width(), img->height(), 1);

	m_pPalette = new Palette ();

	unsigned char r_bg = img->data()[0];
	unsigned char g_bg = img->data()[1];
	unsigned char b_bg = img->data()[2];
	for (unsigned int j=0; j<m_ny; j++)
		for (unsigned int i=0; i<m_nx; i++)
		{
			if (img->data()[4*(img->width()*j+i)]   == r_bg &&
			    img->data()[4*(img->width()*j+i)+1] == g_bg &&
			    img->data()[4*(img->width()*j+i)+2] == b_bg)
				m_pVoxels[i][j][0].m_bActivated = false;
			else
			{
				m_pVoxels[i][j][0].m_bActivated = true;
				//m_pVoxels[i][j][0].m_iLabel = img->width*j+i;
				m_pVoxels[i][j][0].m_iLabel = m_pPalette->AddColor (img->data()[4*(img->width()*j+i)] ,
										    img->data()[4*(img->width()*j+i)+1],
										    img->data()[4*(img->width()*j+i)+2],
										    img->data()[4*(img->width()*j+i)+3]);
			}
		}

	export_palette_to_mtl ((char*)"toto_pixelart.mtl");

	return 0;
}

int Voxels::input_imgs (Img **imgs, unsigned int nImgs)
{
	if (imgs == nullptr)
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
		unsigned char r_bg = img->data()[0];
		unsigned char g_bg = img->data()[1];
		unsigned char b_bg = img->data()[2];
		for (unsigned int j=0; j<m_ny; j++)
			for (unsigned int i=0; i<m_nx; i++)
			{
				if (img->data()[4*(img->width()*j+i)] == r_bg
					&& img->data()[4*(img->width()*j+i)+1] == g_bg
					&& img->data()[4*(img->width()*j+i)+2] == b_bg)
					m_pVoxels[i][j][k].m_bActivated = false;
				else
				{
					m_pVoxels[i][j][k].m_bActivated = true;
					//m_pVoxels[i][j][k].m_iLabel = img->width*j+i;
					m_pVoxels[i][j][k].m_iLabel = m_pPalette->AddColor (img->data()[4*(img->width()*j+i)] ,
																		img->data()[4*(img->width()*j+i)+1],
																		img->data()[4*(img->width()*j+i)+2],
																		img->data()[4*(img->width()*j+i)+3]);
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

	return 0;
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
				m_pVoxels[i][j][k].m_fData = fDataMean / iNeighbours;
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

//
// Shared voxel-surface extraction core
// ------------------------------------
// Single source of truth for (a) the vertex-grid index formula, (b) the vertex
// positions and (c) the exposed-face enumeration — consumed by both
// triangulate() (OBJ+materials+UV) and ToMesh() (in-memory mesh+colours). The
// former divergence (a wrong (ny+1) stride in ToMesh only) is now impossible.
//
unsigned int Voxels::gridIndex (unsigned int i, unsigned int j, unsigned int k) const
{
	// row-major over the (nx+1, ny+1, nz+1) vertex grid
	return (m_nx + 1) * (m_ny + 1) * k + (m_nx + 1) * j + i;
}

void Voxels::buildVertexGrid (float* v) const
{
	for (unsigned int i = 0; i <= m_nx; i++)
	for (unsigned int j = 0; j <= m_ny; j++)
	for (unsigned int k = 0; k <= m_nz; k++)
	{
		const unsigned int idx = gridIndex (i, j, k);
		v[3*idx]     = (float)i;   // grid spans [0,nx]x[0,ny]x[0,nz]
		v[3*idx + 1] = (float)j;
		v[3*idx + 2] = (float)k;
	}
}

void Voxels::forEachSurfaceQuad (const std::function<void(const VoxelSurfaceFace&)>& emit) const
{
	const unsigned int nx = m_nx, ny = m_ny, nz = m_nz;
	VoxelSurfaceFace q;
	auto set = [&](int n, unsigned int x, unsigned int y, unsigned int z)
	{ q.corner[n][0] = x; q.corner[n][1] = y; q.corner[n][2] = z; };

	for (unsigned int i = 0; i < nx; i++)
	for (unsigned int j = 0; j < ny; j++)
	for (unsigned int k = 0; k < nz; k++)
	{
		if (!m_pVoxels[i][j][k].m_bActivated)
			continue;
		q.i = i; q.j = j; q.k = k;

		// Corners CCW as seen from OUTSIDE (outward normal via (c1-c0)x(c2-c0)).
		if (i == 0    || !m_pVoxels[i-1][j][k].m_bActivated)
		{ q.face=0; set(0,i,j,k);   set(1,i,j,k+1);   set(2,i,j+1,k+1);   set(3,i,j+1,k);   emit(q); }
		if (i == nx-1 || !m_pVoxels[i+1][j][k].m_bActivated)
		{ q.face=1; set(0,i+1,j,k); set(1,i+1,j+1,k); set(2,i+1,j+1,k+1); set(3,i+1,j,k+1); emit(q); }
		if (j == 0    || !m_pVoxels[i][j-1][k].m_bActivated)
		{ q.face=2; set(0,i,j,k);   set(1,i+1,j,k);   set(2,i+1,j,k+1);   set(3,i,j,k+1);   emit(q); }
		if (j == ny-1 || !m_pVoxels[i][j+1][k].m_bActivated)
		{ q.face=3; set(0,i,j+1,k); set(1,i,j+1,k+1); set(2,i+1,j+1,k+1); set(3,i+1,j+1,k); emit(q); }
		if (k == 0    || !m_pVoxels[i][j][k-1].m_bActivated)
		{ q.face=4; set(0,i,j,k);   set(1,i,j+1,k);   set(2,i+1,j+1,k);   set(3,i+1,j,k);   emit(q); }
		if (k == nz-1 || !m_pVoxels[i][j][k+1].m_bActivated)
		{ q.face=5; set(0,i,j,k+1); set(1,i+1,j,k+1); set(2,i+1,j+1,k+1); set(3,i,j+1,k+1); emit(q); }
	}
}

int Voxels::triangulate (char *filename)
{
	// 0 : interpolation on texture 1D
	// 1 : texture 2D
	unsigned int eTextureMode = 1;

	// 0 : use m_iLabel as a material id
	// 1 : 
	unsigned int eMaterialMode = 0;

	unsigned int i;
	unsigned int nx = m_nx;
	unsigned int ny = m_ny;
	unsigned int nz = m_nz;

	unsigned int nv = (nx+1)*(ny+1)*(nz+1);
	float *v = (float*)malloc(3*nv*sizeof(float));
	unsigned int nf = 5*nv;
	int *f = (int*)malloc(4*nf*sizeof(int)); // indices vertices
	float *t = nullptr;
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
	buildVertexGrid (v);

	// faces — exposed-surface extraction via the shared core. eTextureMode==1 /
	// eMaterialMode==0 are hard-set above, so only that path is emitted here
	// (fixed atlas UVs per corner slot; material id = voxel label). The old
	// per-face-hand-coded blocks (incl. the never-taken eTextureMode==0 /
	// block_to_texture_id branches) are gone. Winding is outward, identical to
	// before; the "horizontal face -> material+1" tweak in the OBJ output below
	// is invariant to corner order, so the exported OBJ is unchanged.
	int fwalk = 0;
	forEachSurfaceQuad ([&](const VoxelSurfaceFace& q)
	{
		f[4*fwalk]   = (int)gridIndex (q.corner[0][0], q.corner[0][1], q.corner[0][2]);
		f[4*fwalk+1] = (int)gridIndex (q.corner[1][0], q.corner[1][1], q.corner[1][2]);
		f[4*fwalk+2] = (int)gridIndex (q.corner[2][0], q.corner[2][1], q.corner[2][2]);
		f[4*fwalk+3] = (int)gridIndex (q.corner[3][0], q.corner[3][1], q.corner[3][2]);

		t[8*fwalk]   = 0.f; t[8*fwalk+1] = 0.f;
		t[8*fwalk+2] = 0.f; t[8*fwalk+3] = 1.f;
		t[8*fwalk+4] = 1.f; t[8*fwalk+5] = 1.f;
		t[8*fwalk+6] = 1.f; t[8*fwalk+7] = 0.f;

		m[fwalk] = m_pVoxels[q.i][q.j][q.k].m_iLabel;
		fwalk++;
	});

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

//
// Triangulate the activated voxels into an in-memory Mesh.
// For each activated voxel, emit 2 triangles per face that borders an
// inactive voxel or the grid boundary. Unused vertices are compacted out.
//
// This is the pure-geometry counterpart of triangulate(filename), which
// additionally emits texture coordinates and materials to an OBJ file.
//
void Voxels::set_palette (const unsigned char* rgb, unsigned int ncolors)
{
	if (m_pPalette) delete m_pPalette;
	m_pPalette = new Palette ();
	for (unsigned int i = 0; i < ncolors; i++)
		m_pPalette->AddColor (rgb[3*i], rgb[3*i+1], rgb[3*i+2], 255);
}

Mesh* Voxels::ToMesh (void)
{
	unsigned int nx = m_nx;
	unsigned int ny = m_ny;
	unsigned int nz = m_nz;
	if (nx == 0 || ny == 0 || nz == 0)
		return nullptr;

	// grid of (nx+1)*(ny+1)*(nz+1) vertex positions (shared builder)
	unsigned int nv = (nx + 1) * (ny + 1) * (nz + 1);
	float *v = (float*)malloc(3 * nv * sizeof(float));
	buildVertexGrid (v);

	// Optional per-FACE colour (voxel label -> palette). When a palette is
	// present (KVX import) the mesh is emitted UNWELDED — 3 fresh vertices per
	// triangle, each carrying that triangle's colour — so every face is flat
	// coloured. Welded/shared grid vertices would make a face inherit colours
	// from neighbouring voxels (visible "décalage"). The procedural Menger path
	// has no palette and keeps the welded/compacted mesh.
	const bool doColor = (m_pPalette != nullptr);
	float curCol[3] = { 0.5f, 0.5f, 0.5f };   // colour of the voxel being emitted

	// upper bound on triangle count: 6 faces per voxel * 2 triangles
	unsigned int maxFaces = 12 * nx * ny * nz;
	int *f = (int*)malloc(3 * maxFaces * sizeof(int));
	unsigned int nf = 0;
	float *tcol = doColor ? (float*)malloc(3 * maxFaces * sizeof(float)) : nullptr;  // per-triangle RGB

	// exposed faces via the shared surface extractor: 2 triangles per quad,
	// (c0,c1,c2) + (c0,c2,c3), outward-facing (corners are CCW seen from outside)
	forEachSurfaceQuad ([&](const VoxelSurfaceFace& q)
	{
		if (doColor)
		{
			const unsigned int label = m_pVoxels[q.i][q.j][q.k].m_iLabel;
			if (label < m_pPalette->m_nColors)
			{
				curCol[0] = m_pPalette->m_pColors[label].r() / 255.f;
				curCol[1] = m_pPalette->m_pColors[label].g() / 255.f;
				curCol[2] = m_pPalette->m_pColors[label].b() / 255.f;
			}
		}

		const unsigned int c0 = gridIndex (q.corner[0][0], q.corner[0][1], q.corner[0][2]);
		const unsigned int c1 = gridIndex (q.corner[1][0], q.corner[1][1], q.corner[1][2]);
		const unsigned int c2 = gridIndex (q.corner[2][0], q.corner[2][1], q.corner[2][2]);
		const unsigned int c3 = gridIndex (q.corner[3][0], q.corner[3][1], q.corner[3][2]);

		f[3*nf]       = (int)c0; f[3*nf+1]     = (int)c1; f[3*nf+2]     = (int)c2;
		f[3*(nf+1)]   = (int)c0; f[3*(nf+1)+1] = (int)c2; f[3*(nf+1)+2] = (int)c3;

		if (tcol)
			for (unsigned int t = nf; t < nf + 2; ++t)
			{ tcol[3*t] = curCol[0]; tcol[3*t+1] = curCol[1]; tcol[3*t+2] = curCol[2]; }

		nf += 2;
	});

	Mesh *mesh = nullptr;

	if (doColor)
	{
		// UNWELDED: 3 fresh vertices per triangle, each carrying that triangle's
		// (voxel's) colour. Every face is therefore flat-coloured — no bleed.
		const unsigned int nvo = 3 * nf;
		float *vo = (float*)malloc(3 * nvo * sizeof(float));
		float *co = (float*)malloc(3 * nvo * sizeof(float));
		for (unsigned int t = 0; t < nf; ++t)
			for (int c = 0; c < 3; ++c)
			{
				const unsigned int g = (unsigned int)f[3*t + c];   // shared grid vertex
				const unsigned int o = 3*t + c;                    // fresh output vertex
				vo[3*o]   = v[3*g];    vo[3*o+1] = v[3*g+1];  vo[3*o+2] = v[3*g+2];
				co[3*o]   = tcol[3*t]; co[3*o+1] = tcol[3*t+1]; co[3*o+2] = tcol[3*t+2];
			}

		mesh = new Mesh(nvo, nf);
		mesh->SetVertices(nvo, vo);
		mesh->m_pVertexColors.assign(co, co + 3 * nvo);
		for (unsigned int t = 0; t < nf; ++t)
		{
			mesh->m_pFaces[t] = new Face();
			mesh->m_pFaces[t]->SetTriangle(3*t, 3*t+1, 3*t+2);
		}

		free(vo);
		free(co);
	}
	else
	{
		// WELDED: drop unused vertices and re-index faces (compact, no colours)
		char *v_used = (char*)calloc(nv, sizeof(char));
		for (unsigned int i = 0; i < 3 * nf; i++)
			v_used[f[i]] = 1;

		int *new_indices = (int*)malloc(nv * sizeof(int));
		int iwalk = 0;
		for (unsigned int i = 0; i < nv; i++)
			if (v_used[i])
				new_indices[i] = iwalk++;
		unsigned int nv2 = (unsigned int)iwalk;

		float *v2 = (float*)malloc(3 * nv2 * sizeof(float));
		iwalk = 0;
		for (unsigned int i = 0; i < nv; i++)
			if (v_used[i])
			{
				v2[3*iwalk]   = v[3*i];
				v2[3*iwalk+1] = v[3*i+1];
				v2[3*iwalk+2] = v[3*i+2];
				iwalk++;
			}

		int *f2 = (int*)malloc(3 * nf * sizeof(int));
		for (unsigned int i = 0; i < 3 * nf; i++)
			f2[i] = new_indices[f[i]];

		mesh = new Mesh(nv2, nf);
		mesh->SetVertices(nv2, v2);
		for (unsigned int i = 0; i < nf; i++)
		{
			mesh->m_pFaces[i] = new Face();
			mesh->m_pFaces[i]->SetTriangle(f2[3*i], f2[3*i+1], f2[3*i+2]);
		}

		free(v_used);
		free(new_indices);
		free(v2);
		free(f2);
	}

	free(v);
	free(f);
	if (tcol) free(tcol);

	return mesh;
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

