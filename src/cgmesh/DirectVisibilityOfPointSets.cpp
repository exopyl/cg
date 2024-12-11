#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "DirectVisibilityOfPointSets.h"
#include "chull.h"

DirectVisibilityOfPointSets::DirectVisibilityOfPointSets ()
{
}

float* DirectVisibilityOfPointSets::ComputeVisiblePoints (void)
{
	m_pointsFlipped = (float*)malloc(3*(m_nPoints+1)*sizeof(float));
	float m_radius = 5500.0; // torus
	//float m_radius = 10000.0;

	m_pointsFlipped[0] = m_camera[0];
	m_pointsFlipped[1] = m_camera[1];
	m_pointsFlipped[2] = m_camera[2];
	for (int i=1; i<=m_nPoints; i++)
	{
		float l = sqrt ((m_points[3*i]-m_camera[0]) * (m_points[3*i]-m_camera[0]) +
						(m_points[3*i+1]-m_camera[1]) * (m_points[3*i+1]-m_camera[1]) +
						(m_points[3*i+2]-m_camera[2]) * (m_points[3*i+2]-m_camera[2])	);
		float tmp = 2*(m_radius-l);
		m_pointsFlipped[3*i]   = m_points[3*i]-m_camera[0]   + tmp*(m_points[3*i]-m_camera[0])/l;
		m_pointsFlipped[3*i+1] = m_points[3*i+1]-m_camera[1] + tmp*(m_points[3*i+1]-m_camera[1])/l;
		m_pointsFlipped[3*i+2] = m_points[3*i+2]-m_camera[2] + tmp*(m_points[3*i+2]-m_camera[2])/l;
	}

	Chull3D *ch = new Chull3D (m_pointsFlipped, m_nPoints+1);
	ch->compute ();

	int *indices;
	ch->get_convex_hull_indices (&indices, &m_nPointsVisible);
	m_pointsVisible = (float*)malloc(3*m_nPointsVisible*sizeof(float));
	for (int i=0; i<m_nPointsVisible; i++)
	{
		m_pointsVisible[3*i]   = m_points[3*indices[i]];
		m_pointsVisible[3*i+1] = m_points[3*indices[i]+1];
		m_pointsVisible[3*i+2] = m_points[3*indices[i]+2];
	}
	printf ("\n");

	delete ch;

	return NULL;
}

void DirectVisibilityOfPointSets::SetPoints (int nPoints, float *points)
{
	m_points = points;
	m_nPoints = nPoints;
}
