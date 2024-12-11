#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../cgmath/cgmath.h"
#include "polygon3.h"

// constructor
Polygon3::Polygon3 ()
{
	m_nContours = 0;
	m_nPoints = NULL;
	m_pPoints = NULL;
}

// destructor
Polygon3::~Polygon3 ()
{
	if (m_pPoints)
	{
		for (int i=0; i<m_nContours; i++)
			free (m_pPoints[i]);
		free (m_pPoints);
	}
}

// edit
int Polygon3::add_contour (unsigned int index, unsigned int nPoints, float *pPoints)
{
	if (index < m_nContours) // update an existing contour
	{
		if (m_pPoints)
			m_pPoints[index] = (float*) realloc (m_pPoints[index], 3*nPoints*sizeof(float));
		else
			m_pPoints[index] = (float*) malloc (3*nPoints*sizeof(float));

		memcpy (m_pPoints[index], pPoints, 3*nPoints*sizeof(float));
		m_nPoints[index] = nPoints;
	}

	if (index >= m_nContours)
	{
		m_pPoints = (float**) realloc (m_pPoints, (index+1)*sizeof(float*));
		m_nPoints = (unsigned int*) realloc (m_nPoints, (index+1)*sizeof(unsigned int));
		m_nContours = (index+1);
		m_pPoints[index] = (float*) malloc (3*nPoints*sizeof(float));
		memcpy (m_pPoints[index], pPoints, 3*nPoints*sizeof(float));
		m_nPoints[index] = nPoints;
	}
	return 0;
}

void Polygon3::GetBBox (vec3 min, vec3 max)
{
	vec3_init (min, m_pPoints[0][0], m_pPoints[0][1], m_pPoints[0][2]);
	vec3_init (max, m_pPoints[0][0], m_pPoints[0][1], m_pPoints[0][2]);
	for (int i=0; i<m_nContours; i++)
	{
		float *pPoints = m_pPoints[i];
		for (int j=0; j<m_nPoints[i]; j++)
		{
			vec3 pt;
			vec3_init (pt, m_pPoints[i][3*j], m_pPoints[i][3*j+1], m_pPoints[i][3*j+2]);
			for (int k=0; k<3; k++)
			{
				if (pt[k] < min[k]) min[k] = pt[k];
				if (pt[k] > max[k]) max[k] = pt[k];
			}
		}
	}
}

void Polygon3::Dump (void)
{
	printf ("polygon3 : %d contours\n", m_nContours);
	for (int i=0; i<m_nContours; i++)
	{
		printf ("contour %d / %d\n", i, m_nContours);
		float *pPoints = m_pPoints[i];
		for (int j=0; j<m_nPoints[i]; j++)
			printf ("   %f %f %f\n", m_pPoints[i][3*j], m_pPoints[i][3*j+1], m_pPoints[i][3*j+2]);
	}
}
