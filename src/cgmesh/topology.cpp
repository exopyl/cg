#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "topology.h"

typedef struct PointInfo
{
	Vector3f point;
	unsigned int index;
	float dist2;
} PointInfo;

static int _create_indexation_compare (const void * a, const void * b)
{
	int iRes = ( (*((PointInfo*)a)).dist2 - (*((PointInfo*)b)).dist2 > 0. )? 1 : -1;
	return iRes;
}

void create_indexation(Vector3f *pTriangles, unsigned int nPoints,
		       unsigned int **pIndexation,
		       double dVertexEpsilon,
		       const Vector3f *pNormal, double dNormalEpsilon)
{
	if (nPoints == 0 || !pTriangles || !pIndexation)
		return;

	unsigned int i, j, index;

	PointInfo *pPoints = new PointInfo[nPoints];
	if(pPoints == nullptr)
		return;

	bool *pOk = new bool[nPoints];
	if(pOk == nullptr)
		return;

	for(i=0 ; i<nPoints ; i++)
		pOk[i] = false;

	// create a first point
	Vector3f firstPoint;
	for (i=0; i<3; i++)
		firstPoint[i] = .12*pTriangles[0][i] + .12;

	for(i=0; i<nPoints; i++)
	{
		for (unsigned int j=0; j<3; j++)
			pPoints[i].point[j] = pTriangles[i][j];
		pPoints[i].index = i;
		pPoints[i].dist2 = ((firstPoint) - (pTriangles[i])).getLength2 ();
	}
	qsort(pPoints, nPoints, sizeof(PointInfo), _create_indexation_compare);

	for(i=0; i<nPoints; i++)
	{
		if(pOk[i] == true)
			continue;

		index = pPoints[i].index;
		(*pIndexation)[index] = index;
		pOk[i] = true;
		j = i+1;
		while(j < nPoints)
		{
			float fDistance = fabs (pPoints[i].dist2 - pPoints[j].dist2);
			if(fDistance < dVertexEpsilon)
			{
				if(pOk[j] == false)
				{
					if((pPoints[i].point).getDistance (pPoints[j].point) < dVertexEpsilon)
					{
						if(pNormal)
						{
							Vector3f v; // pNormal[pPoints[i].index]^pNormal[pPoints[j].index]
							v[0] = pNormal[pPoints[i].index][1] * pNormal[pPoints[j].index][2]
								- pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][1];

							v[1] = pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][0]
								- pNormal[pPoints[i].index][0] * pNormal[pPoints[j].index][2];

							v[2] = pNormal[pPoints[i].index][1] * pNormal[pPoints[j].index][2]
								- pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][1];

							float fLength = (v).getLength ();
							if(fLength < dNormalEpsilon)
							{
								(*pIndexation)[pPoints[j].index] = pPoints[i].index;
								pOk[j] = true;
							}
						}
						else
						{
							(*pIndexation)[pPoints[j].index] = pPoints[i].index;
							pOk[j] = true;
						}
					}
				}
			}
			else
				break;

			j++;
		}
	}
	delete[] pOk;
	delete[] pPoints;
}

//
//
//
void remove_unused_vertices (Vector3f **pVertices, unsigned int *nVerticesNew, unsigned int **pIndexation, unsigned int nFaces)
{
	unsigned int i=0;
	unsigned int nVertices = 3*nFaces;

	int *pNewIndices = (int*)malloc(nVertices*sizeof(int));
	if (pNewIndices == nullptr)
		return;

	// init
	for (i=0; i<nVertices; i++)
		pNewIndices[i] = -1;

	// identify the vertices used
	for (i=0; i<3*nFaces; i++)
		if (pNewIndices[(*pIndexation)[i]] == -1)
			pNewIndices[(*pIndexation)[i]] = (*pIndexation)[i];

	// repack the indices and the vertices
	(*nVerticesNew) = 0;
	for (i=0; i<nVertices; i++)
	{
		if (pNewIndices[i] != -1)
		{
			pNewIndices[i] = (*nVerticesNew);

			(*pVertices)[(*nVerticesNew)][0] = (*pVertices)[i][0];
			(*pVertices)[(*nVerticesNew)][1] = (*pVertices)[i][1];
			(*pVertices)[(*nVerticesNew)][2] = (*pVertices)[i][2];

			(*nVerticesNew)++;
		}
	}
	*pVertices = (Vector3f*)realloc((*pVertices), 3*(*nVerticesNew)*sizeof(Vector3f*));

	// update the indexation with the new indices
	for (i=0; i<3*nFaces; i++)
		(*pIndexation)[i] = pNewIndices[(*pIndexation)[i]];
}

