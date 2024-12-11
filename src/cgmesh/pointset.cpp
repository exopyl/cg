#include <stdio.h>
#include <stdlib.h>

#include "pointset.h"

PointSet::PointSet (unsigned int nMaxPoints)
{
	m_nMaxPoints = nMaxPoints;
	m_pPoints = (float*)malloc(3*m_nMaxPoints*sizeof(float));
	m_nPoints = 0;
}

PointSet::~PointSet ()
{
	if (m_pPoints)
		free (m_pPoints);
}

int PointSet::AddPoint (float x, float y, float z)
{
	if (m_nPoints >= m_nMaxPoints)
	{
		m_nMaxPoints *= 2;
		m_pPoints = (float*)realloc(m_pPoints, 3*m_nMaxPoints*sizeof(float));
		if (!m_pPoints)
			return -1;
	}
	int offset = 3*m_nPoints;
	m_pPoints[offset]   = x;
	m_pPoints[offset+1] = y;
	m_pPoints[offset+2] = z;
	m_nPoints++;

	return 0;
}

void PointSet::dump (void)
{
	for (int i=0; i<m_nPoints; i++)
		printf ("%f %f %f\n", m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);
}

void PointSet::export_obj (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	for (int i=0; i<m_nPoints; i++)
		fprintf (ptr, "v %f %f %f\n", m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);
}


