#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "octree.h"

Octree::Octree ()
{
	m_pFather = NULL;
	for (int i=0; i<8; i++)
		m_pChildren[i] = NULL;
	m_nPoints = 0;
	m_pPoints = NULL;
	m_nIndices = 0;
	m_pIndices = NULL;
	m_nTriangles = 0;
	m_pTriangles = NULL;
}

Octree::~Octree ()
{
	for (int i=0; i<8; i++)
		if (m_pChildren[i])
			delete m_pChildren[i];
	if (m_pPoints)
		delete m_pPoints;
	if (m_pIndices)
		delete m_pIndices;
}

void Octree::GetCenter (float center[3]) const
{
	for (int i=0; i<3; i++)
		center[i] = (m_vecMin[i]+m_vecMax[i])/2.;
}

void Octree::GetMinMax (float vecMin[3], float vecMax[3]) const
{
	memcpy (vecMin, m_vecMin, 3*sizeof(float));
	memcpy (vecMax, m_vecMax, 3*sizeof(float));
}

void Octree::ComputeBounding (float *pPoints, int nPoints, float min[3], float max[3])
{
	for (int i=0; i<3; i++)
	{
		min[i] = pPoints[i];
		max[i] = pPoints[i];
	}
	for (int i=1; i<nPoints; i++)
	{
		for (int j=0; j<3; j++)
		{
			if (pPoints[3*i+j] < min[j]) min[j] = pPoints[3*i+j];
			if (pPoints[3*i+j] > max[j]) max[j] = pPoints[3*i+j];
		}
	}
}

// basic algorithm (store positions)
int Octree::Build (float *pPoints, int nPoints,
				   unsigned int maxPoints,
				   unsigned int maxDepth, unsigned int currentDepth)
{
	if (!pPoints)
		return -1;
	
	// compute the bounding values
	ComputeBounding (pPoints, nPoints, m_vecMin, m_vecMax);
	
	if (currentDepth >= maxDepth || nPoints <= maxPoints)
	{
		m_nPoints = nPoints;
		m_pPoints = new float[3*nPoints];
		memcpy (m_pPoints, pPoints, 3*nPoints*sizeof(float));
		return 0;
	}

	// get the center of the node
	float center[3];
	GetCenter(center);

	// distribute the points
	unsigned int childPointCounts[8];
	memset(childPointCounts, 0, 8*sizeof(unsigned int));
	int *codes = new int[nPoints];
	memset (codes, 0, nPoints*sizeof(int));
	for (int i=0; i<nPoints; i++)
    {
		// get the current point
		float x = pPoints[3*i];
		float y = pPoints[3*i+1];
		float z = pPoints[3*i+2];

        if (x > center[0]) codes[i] |= 1;
        if (y > center[1]) codes[i] |= 2;
        if (z > center[2]) codes[i] |= 4;

        childPointCounts[codes[i]]++;
    }

	// call Build for the each child
    for (int i=0; i<8; i++)
    {
		// no points for this child
        if (!childPointCounts[i])
		{
			m_pChildren[i] = NULL;
			continue;
		}

 		// get the points for the current child
		float *_pPoints = (float*)malloc(3*childPointCounts[i]*sizeof(float));
		int k=0;
        for (int j=0; j<nPoints; j++)
        {
            if (codes[j] == i)
            {
                _pPoints[3*k]   = pPoints[3*j];
                _pPoints[3*k+1] = pPoints[3*j+1];
                _pPoints[3*k+2] = pPoints[3*j+2];
				k++;
            }
        }

        // recurse
        m_pChildren[i] = new Octree ();
		m_pChildren[i]->m_pFather = this;
        m_pChildren[i]->Build(_pPoints, childPointCounts[i], maxPoints, maxDepth, currentDepth+1);

        // cleaning
		free (_pPoints);
	}

	return 0;
}

void Octree::ComputeBoundingWithIndices (float *pPoints, int nPoints, unsigned int *pIndices, int nIndices, float min[3], float max[3])
{
	for (int i=0; i<3; i++)
	{
		min[i] = pPoints[3*pIndices[0]+i];
		max[i] = pPoints[3*pIndices[0]+i];
	}
	for (int i=1; i<nIndices; i++)
	{
		for (int j=0; j<3; j++)
		{
			if (pPoints[3*pIndices[i]+j] < min[j]) min[j] = pPoints[3*pIndices[i]+j];
			if (pPoints[3*pIndices[i]+j] > max[j]) max[j] = pPoints[3*pIndices[i]+j];
		}
	}
}

int Octree::BuildWithIndices (float *pPoints, int nPoints,
								unsigned int maxPoints, unsigned int maxDepth,
								unsigned int *pIndices, int nIndices, unsigned int currentDepth)
{
	if (!pPoints)
		return -1;

	// initialization
	if (currentDepth == 0 && pIndices == NULL)
	{
		nIndices = nPoints;
		pIndices = (unsigned int*)malloc(nIndices*sizeof(unsigned int));
		for (int i=0; i<nIndices; i++)
			pIndices[i] = i;
		return BuildWithIndices (pPoints, nPoints, maxPoints, maxDepth, pIndices, nIndices, currentDepth);
	}
	
	// compute the bounding values
	ComputeBoundingWithIndices (pPoints, nPoints, pIndices, nIndices, m_vecMin, m_vecMax);
	
	// terminal condition
	if (currentDepth >= maxDepth || nPoints <= maxPoints)
	{
		m_nIndices = nIndices;
		m_pIndices = new unsigned int[nIndices];
		memcpy (m_pIndices, pIndices, nIndices*sizeof(int));
		return 0;
	}

	// get the center of the node
	float center[3];
	GetCenter(center);

	// distribute the points
	unsigned int childPointCounts[8];
	memset(childPointCounts, 0, 8*sizeof(unsigned int));
	int *codes = new int[nPoints];
	memset (codes, 0, nPoints*sizeof(int));
	for (int i=0; i<nIndices; i++)
    {
		// get the current point
		int vi = 3*pIndices[i];
		float x = pPoints[vi];
		float y = pPoints[vi+1];
		float z = pPoints[vi+2];

		if (x > center[0]) codes[i] |= 1;
		if (y > center[1]) codes[i] |= 2;
		if (z > center[2]) codes[i] |= 4;

		childPointCounts[codes[i]]++;
    }

	// call Build for the each child
    for (int i=0; i<8; i++)
    {
		// no points for this child
		if (!childPointCounts[i])
		{
			m_pChildren[i] = NULL;
			continue;
		}

 		// get the indices for the current child
		unsigned int *_pIndices = (unsigned int*)malloc(childPointCounts[i]*sizeof(unsigned int));
		int k=0;
        for (int j=0; j<nIndices; j++)
                if (codes[j] == i)
                        _pIndices[k++] = pIndices[j];

        // recurse
        m_pChildren[i] = new Octree ();
		m_pChildren[i]->m_pFather = this;
        m_pChildren[i]->BuildWithIndices(pPoints, nPoints, maxPoints, maxDepth, _pIndices, childPointCounts[i], currentDepth+1);

        // cleaning
		free (_pIndices);
	}

	// cleaning
	delete[] codes;

	return 0;
}

//
// Fill the octree with triangles
//
void Octree::ComputeBoundinForTriangles (float *pPoints, int nPoints, unsigned int *pTriangles, int nTriangles, float min[3], float max[3])
{
	for (int i=0; i<3; i++)
	{
		min[i] = pPoints[3*pTriangles[0]+i];
		max[i] = pPoints[3*pTriangles[0]+i];
	}
	for (int i=1; i<3*nTriangles; i++)
	{
		for (int j=0; j<3; j++)
		{
			if (pPoints[3*pTriangles[i]+j] < min[j]) min[j] = pPoints[3*pTriangles[i]+j];
			if (pPoints[3*pTriangles[i]+j] > max[j]) max[j] = pPoints[3*pTriangles[i]+j];
		}
	}
}

int Octree::BuildForTriangles (float *pPoints, int nPoints,
			      unsigned int maxTriangles, unsigned int maxDepth,
			      unsigned int *pTriangles, int nTriangles, unsigned int currentDepth)
{
	if (!pPoints || !pTriangles)
		return -1;
	
	// compute the bounding values
	ComputeBoundinForTriangles (pPoints, nPoints, pTriangles, nTriangles, m_vecMin, m_vecMax);
	
	if (currentDepth >= maxDepth || nTriangles <= maxTriangles)
	{
		m_nTriangles = nTriangles;
		m_pTriangles = new unsigned int[3*nTriangles];
		memcpy (m_pTriangles, pTriangles, 3*nTriangles*sizeof(unsigned int));
		return 0;
	}
	
	// get the center of the node
	float center[3];
	GetCenter(center);

	// distribute the triangles
	unsigned int childTriangleCounts[8];
	memset(childTriangleCounts, 0, 8*sizeof(unsigned int));
	int *codes = new int[nTriangles];
	memset (codes, 0, nTriangles*sizeof(int));
	for (int i=0; i<nTriangles; i++)
    {
		// get the current triangle
		Triangle tri;
		tri.Init (pPoints[3*pTriangles[3*i]], pPoints[3*pTriangles[3*i]+1], pPoints[3*pTriangles[3*i]+2],
				pPoints[3*pTriangles[3*i+1]], pPoints[3*pTriangles[3*i+1]+1], pPoints[3*pTriangles[3*i+1]+2],
				pPoints[3*pTriangles[3*i+2]], pPoints[3*pTriangles[3*i+2]+1], pPoints[3*pTriangles[3*i+2]+2]);

		AABox aabox0 (center[0], center[1], center[2]);
		aabox0.AddVertex (m_vecMin[0], m_vecMin[1], m_vecMin[2]);

		AABox aabox1 (center[0], center[1], center[2]);
		aabox1.AddVertex (m_vecMin[0], m_vecMax[1], m_vecMin[2]);

		AABox aabox2 (center[0], center[1], center[2]);
		aabox2.AddVertex (m_vecMax[0], m_vecMin[1], m_vecMin[2]);

		AABox aabox3 (center[0], center[1], center[2]);
		aabox3.AddVertex (m_vecMax[0], m_vecMax[1], m_vecMin[2]);

		AABox aabox4 (center[0], center[1], center[2]);
		aabox4.AddVertex (m_vecMin[0], m_vecMin[1], m_vecMax[2]);

		AABox aabox5 (center[0], center[1], center[2]);
		aabox5.AddVertex (m_vecMin[0], m_vecMax[1], m_vecMax[2]);

		AABox aabox6 (center[0], center[1], center[2]);
		aabox6.AddVertex (m_vecMax[0], m_vecMin[1], m_vecMax[2]);

		AABox aabox7 (center[0], center[1], center[2]);
		aabox7.AddVertex (m_vecMax[0], m_vecMax[1], m_vecMax[2]);

		float xmin = MIN3 (pPoints[3*pTriangles[3*i]], pPoints[3*pTriangles[3*i+1]], pPoints[3*pTriangles[3*i+2]]);
		float ymin = MIN3 (pPoints[3*pTriangles[3*i]+1], pPoints[3*pTriangles[3*i+1]+1], pPoints[3*pTriangles[3*i+2]+1]);
		float zmin = MIN3 (pPoints[3*pTriangles[3*i]+2], pPoints[3*pTriangles[3*i+1]+2], pPoints[3*pTriangles[3*i+2]+2]);
		float xmax = MAX3 (pPoints[3*pTriangles[3*i]], pPoints[3*pTriangles[3*i+1]], pPoints[3*pTriangles[3*i+2]]);
		float ymax = MAX3 (pPoints[3*pTriangles[3*i]+1], pPoints[3*pTriangles[3*i+1]+1], pPoints[3*pTriangles[3*i+2]+1]);
		float zmax = MAX3 (pPoints[3*pTriangles[3*i]+2], pPoints[3*pTriangles[3*i+1]+2], pPoints[3*pTriangles[3*i+2]+2]);

		if (aabox0.contains (tri) || aabox0.intersection (tri)) { codes[i] |= 1;   childTriangleCounts[0]++; }
		if (aabox1.contains (tri) || aabox1.intersection (tri)) { codes[i] |= 2;   childTriangleCounts[1]++; }
		if (aabox2.contains (tri) || aabox2.intersection (tri)) { codes[i] |= 4;   childTriangleCounts[2]++; }
		if (aabox3.contains (tri) || aabox3.intersection (tri)) { codes[i] |= 8;   childTriangleCounts[3]++; }
		if (aabox4.contains (tri) || aabox4.intersection (tri)) { codes[i] |= 16;  childTriangleCounts[4]++; }
		if (aabox5.contains (tri) || aabox5.intersection (tri)) { codes[i] |= 32;  childTriangleCounts[5]++; }
		if (aabox6.contains (tri) || aabox6.intersection (tri)) { codes[i] |= 64;  childTriangleCounts[6]++; }
		if (aabox7.contains (tri) || aabox7.intersection (tri)) { codes[i] |= 128; childTriangleCounts[7]++; }
    }

	// call Build for each child
    for (int i=0; i<8; i++)
    {
		// no triangle for this child
		if (!childTriangleCounts[i])
		{
			m_pChildren[i] = NULL;
			continue;
		}

 		// get the triangles for the current child
		unsigned int *_pTriangles = (unsigned int*)malloc(3*childTriangleCounts[i]*sizeof(unsigned int));
		int k=0;
        for (int j=0; j<nTriangles; j++)
                if (codes[j] & (1<<i))
				{
                        _pTriangles[k++] = pTriangles[3*j];
                        _pTriangles[k++] = pTriangles[3*j+1];
                        _pTriangles[k++] = pTriangles[3*j+2];
				}

        // recurse
        m_pChildren[i] = new Octree ();
		m_pChildren[i]->m_pFather = this;
        m_pChildren[i]->BuildForTriangles(pPoints, nPoints, maxTriangles, maxDepth, _pTriangles, childTriangleCounts[i], currentDepth+1);

        // cleaning
		free (_pTriangles);
	}

	return 0;
}


void Octree::Dump (void)
{
	printf ("Stats :\n");
	printf ("number of leaves : %d\n", GetNLeaves ());
	printf ("max depth : %d\n", GetMaxDepth ());
}

int Octree::GetKNeighbours (vec3 pt, float distance)
{
	if (m_nPoints)
	{
		vec3 tmp;
		int res = 0;
		for (int i=0; i<m_nPoints; i++)
		{
			vec3_init (tmp, m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);
			if (vec3_distance (pt, tmp) < distance)
				res++;
		}
		return res;
	}
	else
	{
		int neighbours = 0;
	        for (int i=0; i<8; i++)
			if (m_pChildren[i])
			{
				if (pt[0] >= m_vecMin[0]-distance &&
				    pt[0] <= m_vecMax[0]+distance &&
				    pt[1] >= m_vecMin[1]-distance &&
				    pt[1] <= m_vecMax[1]+distance &&
				    pt[2] >= m_vecMin[2]-distance &&
				    pt[2] <= m_vecMax[2]+distance)
					neighbours += m_pChildren[i]->GetKNeighbours (pt, distance);
			}
		return neighbours;
	}
}

int Octree::GetClosestPoints (vec3 pt, float distance, float **pNeighbours, unsigned int *nNeighbours)
{
	if (m_nPoints)
	{
		vec3 tmp;
		int res = 0;
		for (int i=0; i<m_nPoints; i++)
		{
			vec3_init (tmp, m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);
			if (vec3_distance (pt, tmp) < distance)
			{
				if (*pNeighbours)
					(*pNeighbours) = (float*)malloc(3*(*nNeighbours)*sizeof(float));
				else
					(*pNeighbours) = (float*)realloc((*pNeighbours), 3*(*nNeighbours)*sizeof(float));
				(*pNeighbours)[3*(*nNeighbours)]   = tmp[0];
				(*pNeighbours)[3*(*nNeighbours)+1] = tmp[1];
				(*pNeighbours)[3*(*nNeighbours)+2] = tmp[2];
				(*nNeighbours)++;
				res++;
			}
		}
		return res;
	}
	else
	{
		int neighbours = 0;
		for (int i=0; i<8; i++)
			if (m_pChildren[i])
			{
				if (pt[0] >= m_vecMin[0]-distance &&
					pt[0] <= m_vecMax[0]+distance &&
					pt[1] >= m_vecMin[1]-distance &&
					pt[1] <= m_vecMax[1]+distance &&
					pt[2] >= m_vecMin[2]-distance &&
					pt[2] <= m_vecMax[2]+distance)
					neighbours += m_pChildren[i]->GetClosestPoints (pt, distance, pNeighbours, nNeighbours);
			}
			return neighbours;
	}
}

int Octree::GetClosestIndicesPoints (float *pVertices, vec3 pt, float distance, unsigned int **pNeighbours, unsigned int *nNeighbours)
{
	if (m_nIndices)
	{
		vec3 tmp;
		int res = 0;
		for (int i=0; i<m_nIndices; i++)
		{
			unsigned int vi = m_pIndices[i];
			vec3_init (tmp, pVertices[3*vi], pVertices[3*vi+1], pVertices[3*vi+2]);
			if (vec3_distance (pt, tmp) < distance)
			{
				if (*pNeighbours)
					(*pNeighbours) = (unsigned int*)realloc((*pNeighbours), ((*nNeighbours)+1)*sizeof(unsigned int));
				else
					(*pNeighbours) = (unsigned int*)malloc((*nNeighbours)*sizeof(unsigned int));
				(*pNeighbours)[(*nNeighbours)] = vi;
				(*nNeighbours)++;
				res++;
			}
		}
		return res;
	}
	else
	{
		int neighbours = 0;
		for (int i=0; i<8; i++)
			if (m_pChildren[i])
			{
				if (pt[0] >= m_vecMin[0]-distance &&
					pt[0] <= m_vecMax[0]+distance &&
					pt[1] >= m_vecMin[1]-distance &&
					pt[1] <= m_vecMax[1]+distance &&
					pt[2] >= m_vecMin[2]-distance &&
					pt[2] <= m_vecMax[2]+distance)
					neighbours += m_pChildren[i]->GetClosestIndicesPoints (pVertices, pt, distance, pNeighbours, nNeighbours);
			}
			return neighbours;
	}
}

int Octree::GetSumNeighbours (vec3 pt, float distance, vec3 accum)
{
	if (m_nPoints)
	{
		vec3 tmp;
		int res = 0;
		for (int i=0; i<m_nPoints; i++)
		{
			vec3_init (tmp, m_pPoints[3*i], m_pPoints[3*i+1], m_pPoints[3*i+2]);
			if (vec3_distance (pt, tmp) <= distance)
			{
				vec3_addition (accum, accum, tmp);
				res++;
			}
		}
		return res;
	}
	else
	{
		int neighbours = 0;
	        for (int i=0; i<8; i++)
			if (m_pChildren[i])
			{
				if (pt[0] >= m_vecMin[0]-distance &&
				    pt[0] <= m_vecMax[0]+distance &&
				    pt[1] >= m_vecMin[1]-distance &&
				    pt[1] <= m_vecMax[1]+distance &&
				    pt[2] >= m_vecMin[2]-distance &&
				    pt[2] <= m_vecMax[2]+distance)
					neighbours += m_pChildren[i]->GetSumNeighbours (pt, distance, accum);
			}
		return neighbours;
	}
}

int Octree::GetNLeaves (void)
{
	if (IsLeaf())
		return 1;
	else
	{
		int res = 0;
		for (int i=0; i<8; i++)
			if (m_pChildren[i])
				res += m_pChildren[i]->GetNLeaves ();
		return res;
	}
}

bool Octree::IsLeaf (void)
{
	for (int i=0; i<8; i++)
		if (m_pChildren[i])
			return false;

	return true;
}

int Octree::GetMaxDepth (void)
{
	if (IsLeaf())
		return 0;
	else
	{
		int res = 0;
		for (int i=0; i<8; i++)
			if (m_pChildren[i])
			{
				 int tmp = 1+m_pChildren[i]->GetMaxDepth ();
				 if (res < tmp)
					 res = tmp;
			}
		return res;
	}
}

/*
//
// iterator
//
OctreeIt::OctreeIt (const Octree *pOctree)
{
	m_pOctree = pOctree;
	m_pOctreeCurrentOctree = m_pOctree;
}

OctreeIt::~OctreeIt ()
{
}

Octree* OctreeIt::First ()
{
	if (m_nIndices)
		return m_pOctree;

	for (int i=0; i<8; i++)
		if (m_pOctree->GetChildren()[i])
			return m_pOctree->GetChildren()[i])->First ();

	return NULL;
}

Octree* OctreeIt::Next ()
{
	if (m_pCurrentNode->m_pFather)
	{
		int i=0;
		for (i=0; i<8; i++)
		{
			if (m_pChildren[i] == m_pCurrentNode)
			{
				i++;
				break;
			}
			if (i<8)
			{

				return m_pChildren[i]->First();
			}
			else

		}
	}
}

bool OctreeIt::Last ()
{
	return (m_pCurrentNode->m_pFather);
}
*/
