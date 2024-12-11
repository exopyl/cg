#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "topology.h"

typedef struct PointInfo
{
	vec3 point;
	unsigned int index;
	float dist2;
} PointInfo;

static int _create_indexation_compare (const void * a, const void * b)
{
	int iRes = ( (*((PointInfo*)a)).dist2 - (*((PointInfo*)b)).dist2 > 0. )? 1 : -1;
	return iRes;
}

void create_indexation(vec3 *pTriangles, unsigned int nPoints,
		       unsigned int **pIndexation,
		       double dVertexEpsilon,
		       const vec3 *pNormal, double dNormalEpsilon)
{
	if (nPoints == 0 || !pTriangles || !pIndexation)
		return;

	unsigned int i, j, index;

	PointInfo *pPoints = new PointInfo[nPoints];
	if(pPoints == NULL)
		return;

	bool *pOk = new bool[nPoints];
	if(pOk == NULL)
		return;

	for(i=0 ; i<nPoints ; i++)
		pOk[i] = false;

	// create a first point
	vec3 firstPoint;
	for (i=0; i<3; i++)
		firstPoint[i] = .12*pTriangles[0][i] + .12;

	for(i=0; i<nPoints; i++)
	{
		for (unsigned int j=0; j<3; j++)
			pPoints[i].point[j] = pTriangles[i][j];
		pPoints[i].index = i;
		pPoints[i].dist2 = vec3_distance2 (pTriangles[i], firstPoint);
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
					if(vec3_distance (pPoints[i].point, pPoints[j].point) < dVertexEpsilon)
					{
						if(pNormal)
						{
							vec3 v; // pNormal[pPoints[i].index]^pNormal[pPoints[j].index]
							v[0] = pNormal[pPoints[i].index][1] * pNormal[pPoints[j].index][2]
								- pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][1];

							v[1] = pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][0]
								- pNormal[pPoints[i].index][0] * pNormal[pPoints[j].index][2];

							v[2] = pNormal[pPoints[i].index][1] * pNormal[pPoints[j].index][2]
								- pNormal[pPoints[i].index][2] * pNormal[pPoints[j].index][1];

							float fLength = vec3_length (v);
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
void remove_unused_vertices (vec3 **pVertices, unsigned int *nVerticesNew, unsigned int **pIndexation, unsigned int nFaces)
{
	unsigned int i=0;
	unsigned int nVertices = 3*nFaces;

	int *pNewIndices = (int*)malloc(nVertices*sizeof(int));
	if (pNewIndices == NULL)
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
	*pVertices = (vec3*)realloc((*pVertices), 3*(*nVerticesNew)*sizeof(vec3*));

	// update the indexation with the new indices
	for (i=0; i<3*nFaces; i++)
		(*pIndexation)[i] = pNewIndices[(*pIndexation)[i]];
}

