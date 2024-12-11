#include <stdlib.h>

#include "bsp.h"

BSP::BSP ()
{
	m_pPlane = NULL;
	m_pFront = NULL;
	m_pBack = NULL;
}

BSP::~BSP ()
{
	if (m_pPlane)
		delete m_pPlane;
	if (m_pFront)
		delete m_pFront;
	if (m_pBack)
		delete m_pBack;
}

int BSP::Split_Polygon (Polygon3 *polygon, Polygon3 *front_piece, Polygon3 *back_piece)
{
	int ci = 0; // we treat the polygon as one contour
	int nPoints = polygon->GetNPoints (0);
	float *pPoints = polygon->GetPoints (0);
	int MAXPOINTS = 256;
	float *frontpts=(float*)malloc(MAXPOINTS*3*sizeof(float));
	float *backpts=(float*)malloc(MAXPOINTS*3*sizeof(float));
	int nfrontpts = 0;
	int nbackpts = 0;

	Vector3 normale;
	m_pPlane->get_normale (normale);
	normale.Normalize ();
	float sideA, sideB;
	Vector3 ptA, ptB;
	ptA.Set (pPoints[0], pPoints[1], pPoints[2]);
	sideA = m_pPlane->distance_point (ptA);
	for (int i=1; i<nPoints; i++)
	{
		ptB.Set (pPoints[3*i], pPoints[3*i+1], pPoints[3*i+2]);
		sideB = m_pPlane->distance_point (ptB);
		if (sideB > 0)
		{
			if (sideA < 0)
			{
				Vector3 v = ptB - ptA;
				float sect = - m_pPlane->distance_point (ptA) / (v * normale);
				ptA = ptA + (v * sect);

				frontpts[3*nfrontpts]   = ptA.x;
				frontpts[3*nfrontpts+1] = ptA.y;
				frontpts[3*nfrontpts+2] = ptA.z;
				nfrontpts++;

				backpts[3*nbackpts]   = ptA.x;
				backpts[3*nbackpts+1] = ptA.y;
				backpts[3*nbackpts+2] = ptA.z;
				nbackpts++;
			}
			frontpts[3*nfrontpts]   = ptB.x;
			frontpts[3*nfrontpts+1] = ptB.y;
			frontpts[3*nfrontpts+2] = ptB.z;
			nfrontpts++;
		}
		else if (sideB < 0)
		{
			if (sideA > 0)
			{
				Vector3 v = ptB - ptA;
				float sect = - m_pPlane->distance_point (ptA) / (v * normale);
				ptA = ptA + (v * sect);

				frontpts[3*nfrontpts]   = ptA.x;
				frontpts[3*nfrontpts+1] = ptA.y;
				frontpts[3*nfrontpts+2] = ptA.z;
				nfrontpts++;

				backpts[3*nbackpts]   = ptA.x;
				backpts[3*nbackpts+1] = ptA.y;
				backpts[3*nbackpts+2] = ptA.z;
				nbackpts++;
			}
			backpts[3*nbackpts]   = ptB.x;
			backpts[3*nbackpts+1] = ptB.y;
			backpts[3*nbackpts+2] = ptB.z;
			nbackpts++;
		}
		else
		{
			frontpts[3*nfrontpts]   = ptB.x;
			frontpts[3*nfrontpts+1] = ptB.y;
			frontpts[3*nfrontpts+2] = ptB.z;
			nfrontpts++;
			
			backpts[3*nbackpts]   = ptB.x;
			backpts[3*nbackpts+1] = ptB.y;
			backpts[3*nbackpts+2] = ptB.z;
			nbackpts++;
		}
		ptA = ptB;
		sideA = sideB;
	}
	if (nfrontpts > 2)
		front_piece->add_contour (0, nfrontpts, frontpts);
	if (nbackpts > 2)
		back_piece->add_contour (0, nbackpts, backpts);
	
	free (frontpts);
	free (backpts);

	return 0;
}

eClassification BSP::Classify_Polygon (Polygon3 *polygon)
{
	unsigned int ci = 0; // we treat only the first contour
	unsigned int nPoints = polygon->GetNPoints (ci);
	float *pPoints = polygon->GetPoints (ci);
	
	Vector3f pt;
	polygon->GetPoint (ci, 0, pt);
	int pos = m_pPlane->position (pt);
	for (int i=1; i<nPoints; i++)
	{
		polygon->GetPoint (ci, i, pt);
		if (pos != m_pPlane->position (pt))
			return SPANNING;
	}

	if (pos == 0)
		return COINCIDENT;
	else if (pos > 0)
		return IN_FRONT_OF;
	else //if (pos < 0)
		return IN_BACK_OF;
}

int BSP::Choose_Plane (list<Polygon3*> polygons)
{
	vec3 lmin, lmax, min, max;
	std::list<Polygon3*>::iterator it = polygons.begin();
	Polygon3 *polygon = *it;
	polygon->GetBBox (lmin, lmax);
	it++;
	for (; it != polygons.end(); it++)
	{
		Polygon3 *polygon = *it;
		polygon->GetBBox (min, max);
		for (int i=0; i<3; i++)
		{
			if (min[i] < lmin[i]) lmin[i] = min[i];
			if (max[i] > lmax[i]) lmax[i] = max[i];
		}
	}
	Vector3 pt1, pt2, pt3;
	if (lmax[0]-lmin[0] >= lmax[1]-lmin[1] && lmax[0]-lmin[0] >= lmax[2]-lmin[2]) // OYZ
	{
		pt1.Set ((lmax[0]+lmin[0])/2., 0., 0.);
		pt2.Set ((lmax[0]+lmin[0])/2., 1., 0.);
		pt3.Set ((lmax[0]+lmin[0])/2., 0., 1.);
	}
	else if (lmax[1]-lmin[1] >= lmax[0]-lmin[0] && lmax[1]-lmin[1] >= lmax[2]-lmin[2]) // OXZ
	{
		pt1.Set (0., (lmax[1]+lmin[1])/2., 0.);
		pt2.Set (1., (lmax[1]+lmin[1])/2., 0.);
		pt3.Set (0., (lmax[1]+lmin[1])/2., 1.);
	}
	else if (lmax[2]-lmin[2] >= lmax[0]-lmin[0] && lmax[2]-lmin[2] >= lmax[1]-lmin[1]) // OXY
	{
		pt1.Set (0., 0., (lmax[2]+lmin[2])/2.);
		pt2.Set (1., 0., (lmax[2]+lmin[2])/2.);
		pt3.Set (0., 1., (lmax[2]+lmin[2])/2.);
	}
	m_pPlane = new Plane (pt1, pt2, pt3);

/*
	std::list<Polygon3*>::iterator it = polygons.begin();
	Polygon3 *polygon = *it;
	Vector3 pt1, pt2, pt3;
	float *pts = polygon->GetPoints (0);
	pt1.Set (pts[0], pts[1], pts[2]);
	pt2.Set (pts[3], pts[4], pts[5]);
	pt3.Set (pts[6], pts[7], pts[8]);
	m_pPlane = new Plane (pt1, pt2, pt3);
*/

	return 0;
}

int BSP::Build (list<Polygon3*> polygons)
{
	printf ("treating %d polygons\n", polygons.size());
	if (polygons.size () < 1000)
	{
		printf ("stop with %d polygons\n", polygons.size());
		m_pPolygons = polygons;
		return 0;
	}

	Choose_Plane (polygons);

	std::list<Polygon3*> front_list, back_list;
	for (std::list<Polygon3*>::iterator it = polygons.begin(); it != polygons.end(); ++it)
	{
		Polygon3 *polygon = *it;

		int result = Classify_Polygon (polygon);
		switch (result)
		{
		case COINCIDENT:
			polygons.push_back (polygon);
			break;
		case IN_BACK_OF:
			back_list.push_back (polygon);
			break;
		case IN_FRONT_OF:
			front_list.push_back (polygon);
			break;
		case SPANNING:
			Polygon3 *front_piece = new Polygon3 ();
			Polygon3 *back_piece = new Polygon3 ();
			Split_Polygon (polygon, front_piece, back_piece);
			if (back_piece->GetNContours() != 0)
				back_list.push_back (back_piece);
			if (front_piece->GetNContours() != 0)
				front_list.push_back (front_piece);
			break;
		}
	}

	if ( ! front_list.empty ())
	{
		m_pFront = new BSP ();
		m_pFront->Build (front_list);
	}
	if ( ! back_list.empty ())
	{
		m_pBack = new BSP ();
		m_pBack->Build (back_list);
	}

	return 0;
}

