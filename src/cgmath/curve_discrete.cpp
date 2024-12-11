#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "curve_discrete.h"
#include "common.h"

CurveDiscrete::CurveDiscrete ()
{
	m_nPoints = 0;
	m_pPoints = NULL;
}

CurveDiscrete::~CurveDiscrete ()
{
	if (m_pPoints)
		free (m_pPoints);
}

int CurveDiscrete::init (int nPoints)
{
	if (m_pPoints)
		free (m_pPoints);

	m_pPoints = (float*)malloc(3*nPoints*sizeof(float));
	if (!m_pPoints)
	{
		m_nPoints = 0;
		return -1;
	}

	m_nPoints = nPoints;

	return 0;
}

int CurveDiscrete::set_position (int pi, float x, float y, float z)
{
	if (!m_pPoints || pi < 0 || pi >= m_nPoints)
		return -1;

	m_pPoints[3*pi+0] = x;
	m_pPoints[3*pi+1] = y;
	m_pPoints[3*pi+2] = z;

	return 0;
}

int CurveDiscrete::import_obj (char *filename)
{
	if (!filename)
		return -1;

	const int BUFFER_SIZE = 4096;
	char buffer[BUFFER_SIZE];
	char prefix[BUFFER_SIZE];

	FILE *ptr = fopen (filename, "r");
	if (!ptr)
	{
		printf ("Unable to open %s", filename);
		return -1;
	}
	
	unsigned int nPoints=0;
	while (!feof (ptr))
	{
		fgets (buffer, BUFFER_SIZE, ptr);
		sscanf (buffer, "%s", buffer);
		if (strcmp (buffer, "v") == 0)
			nPoints++;
	}
	rewind (ptr);

	m_nPoints = 0;
	m_pPoints = (float*)malloc(3*nPoints*sizeof(float));
	while (!feof (ptr))
	{
		fgets (buffer, BUFFER_SIZE, ptr);
		sscanf (buffer, "%s", prefix);
		if (strcmp (prefix, "v") == 0)
		{
			sscanf (buffer, "%s %f %f %f", prefix,
				&m_pPoints[3*m_nPoints],
				&m_pPoints[3*m_nPoints+1],
				&m_pPoints[3*m_nPoints+2]);
			m_nPoints++;
		}
	}

	return 0;
}

int CurveDiscrete::export_obj (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return -1;

	for (int i=0; i<m_nPoints; i++)
		fprintf (ptr, "v %f %f %f\n",
			 m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);

	fclose (ptr);

	return 0;
}

int CurveDiscrete::inverse_order (void)
{
	int i, j;
	float tmp;
	
	for (i=0; i<m_nPoints/2; i++)
	{
		for (j=0; j<3; j++)
		{
			tmp = m_pPoints[3*i+j];
			m_pPoints[3*i+j] = m_pPoints[3*(m_nPoints-1-i)+j];
			m_pPoints[3*(m_nPoints-1-i)+j] = tmp;
		}
	}

	return 0;
}

//
//
//
int CurveDiscrete::generate_surface_revolution (unsigned int nSlices,
						unsigned int *_nVertices, float **_pVertices, unsigned int *_nFaces, unsigned int **_pFaces)
{
	// count the number of vertices and faces
	unsigned int nVertices = nSlices*m_nPoints;
	unsigned int nFaces = nSlices*(m_nPoints-1);
	float *pVertices = (float*)malloc(3*nVertices*sizeof(float));
	unsigned int *pFaces = (unsigned int*)malloc(4*nFaces*sizeof(unsigned int));

	// create the mesh
	unsigned int ivertex=0, iface=0;
	for (int i=0; i<m_nPoints; i++)
	{
		for (unsigned int j=0; j<nSlices; j++)
		{
			float angle = j*2*3.14159/(nSlices);
			pVertices[3*ivertex+0] = m_pPoints[3*i]*cos(angle) - m_pPoints[3*i+1]*sin(angle);
			pVertices[3*ivertex+1] = m_pPoints[3*i]*sin(angle) + m_pPoints[3*i+1]*cos(angle);
			pVertices[3*ivertex+2] = m_pPoints[3*i+2];
			ivertex++;
		}
	}

	for (int j=0; j<m_nPoints-1; j++)
	{
		for (unsigned int k=0; k<nSlices; k++)
		{
			if (j == m_nPoints-1) // close the curve
			{
				if (k == nSlices-1)
				{
					pFaces[4*iface+0] = (nSlices) * j + k;
					pFaces[4*iface+1] = (nSlices) * j;
					pFaces[4*iface+2] = 0;
					pFaces[4*iface+3] = k;
/*
					pFaces[4*iface+0] = (nSlices) * j;
					pFaces[4*iface+1] = (nSlices) * j + k;
					pFaces[4*iface+2] = k;
					pFaces[4*iface+3] = 0;
*/
				}
				else
				{
					pFaces[4*iface+0] = (nSlices) * j + k;
					pFaces[4*iface+1] = (nSlices) * j + k+1;
					pFaces[4*iface+2] = k+1;
					pFaces[4*iface+3] = k;
/*
					pFaces[4*iface+0] = (nSlices) * j + k+1;
					pFaces[4*iface+1] = (nSlices) * j + k;
					pFaces[4*iface+2] = k;
					pFaces[4*iface+3] = k+1;
*/
				}
			}
			else
			{
				if (k == nSlices-1)
				{
					pFaces[4*iface+0] = nSlices * j + k;
					pFaces[4*iface+1] = nSlices * j;
					pFaces[4*iface+2] = nSlices * (j+1);
					pFaces[4*iface+3] = nSlices * (j+1) + k;
/*
					pFaces[4*iface+0] = nSlices * j;
					pFaces[4*iface+1] = nSlices * j + k;
					pFaces[4*iface+2] = nSlices * (j+1) + k;
					pFaces[4*iface+3] = nSlices * (j+1);
*/
				}
				else
				{
					pFaces[4*iface+0] = nSlices * j + k;
					pFaces[4*iface+1] = nSlices * j + k+1;
					pFaces[4*iface+2] = nSlices * (j+1) + k+1;
					pFaces[4*iface+3] = nSlices * (j+1) + k;
/*
					pFaces[4*iface+0] = nSlices * j + k+1;
					pFaces[4*iface+1] = nSlices * j + k;
					pFaces[4*iface+2] = nSlices * (j+1) + k;
					pFaces[4*iface+3] = nSlices * (j+1) + k+1;
*/
				}
			}
			iface++;
		}
	}

	*_pVertices = pVertices;
	*_nVertices = ivertex;
	*_pFaces = pFaces;
	*_nFaces = iface;

	return 0;
}

int CurveDiscrete::generate_frame (float width, float height, unsigned int *_nVertices, float **_pVertices, unsigned int *_nFaces, unsigned int **_pFaces)
{
	// count the number of vertices and faces
	unsigned int nVertices = 4*m_nPoints;
	unsigned int nFaces = 4*m_nPoints;
	float *pVertices = (float*)malloc(3*nVertices*sizeof(float));
	unsigned int *pFaces = (unsigned int*)malloc(4*nFaces*sizeof(unsigned int));

	float angle[4] = {45.f*3.14156f/180.f,
			  ((90.f+45.f)*3.14156f/180.f),
			  ((2.f*90.f+45.f)*3.14156f/180.f),
			  ((3.f*90.f+45.f)*3.14156f/180.f)};
	float xtrans[4] = {width*0.5f, -width*0.5f, -width*0.5f, width*0.5f};
	float ytrans[4] = {height*0.5f, height*0.5f, -height*0.5f, -height*0.5f};

	// create the mesh
	unsigned int ivertex=0;
	for (int i=0; i<m_nPoints; i++)
	{
		for (unsigned int j=0; j<4; j++)
		{
			pVertices[3*ivertex+0] = xtrans[j] + m_pPoints[3*i]*cos(angle[j]) - m_pPoints[3*i+1]*sin(angle[j]);
			pVertices[3*ivertex+1] = ytrans[j] + m_pPoints[3*i]*sin(angle[j]) + m_pPoints[3*i+1]*cos(angle[j]);
			pVertices[3*ivertex+2] = m_pPoints[3*i+2];
			ivertex++;
		}
	}

	unsigned int iface=0;
	for (int j=0; j<m_nPoints; j++)
	{
		for (unsigned int i=0; i<4; i++)
		{
			if (i!=3)
			{
				pFaces[4*iface+0] = (i + 4 * j ) % nVertices;
				pFaces[4*iface+1] = (i + 4 * j + 4  ) % nVertices;
				pFaces[4*iface+2] = (i + 4 * j + 4 + 1  ) % nVertices;
				pFaces[4*iface+3] = (i + 4 * j + 1 ) % nVertices;
			}
			else
			{
				pFaces[4*iface+0] = (i + 4 * j ) % nVertices;
				pFaces[4*iface+1] = (i + 4 * j + 4 ) % nVertices;
				pFaces[4*iface+2] = (i + 4 * j + 4 + 1 -4 ) % nVertices;
				pFaces[4*iface+3] = (i + 4 * j + 1 -4 ) % nVertices;
			}
			iface++;
		}
	}

	*_pVertices = pVertices;
	*_nVertices = ivertex;
	*_pFaces = pFaces;
	*_nFaces = iface;

	return 0;
}

