//
// based on :
// Marching Cubes Example Program  by Cory Bloyd (corysama@yahoo.com)
//
// For a description of the algorithm go to
// http://astronomy.swin.edu.au/pbourke/modelling/polygonise/
//
// This code is public domain.
//
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "surface_implicit.h"
#include "surface_implicit_def.h"

ImplicitSurface::ImplicitSurface ()
{
	set_bbox (-.5, -.5, -.5, .5, .5, .5);
	set_orientation (1);
	set_resolution_per_unit (32);
	set_value (48.);
	bBoundary = 0;

	eval_func = NULL;
	color_func = NULL;
	vertex_func = NULL;
	normal_func = NULL;
	face_completed_func = NULL;
	layer_completed_func = NULL;
	data = NULL;
	fValueCached = NULL;
	fIndicesCached = NULL;
	iCurrentVertex = -1;
};


typedef struct _mc_triangulation_
{
	float *vertices;
	int nvertices;
	int nverticesmax;

	int iCurrentVertexInFace;
	unsigned int *faces;
	int nfaces;
	int nfacesmax;
} mc_triangulation_t;

////////////////////////////////////////////////////////////////////////////////
//
// triangulation
//
static bool mc_get_triangulation_vertex_func (float x, float y, float z, int index, void *data)
{
	bool new_triangle_completed = false;
	mc_triangulation_t *tri = (mc_triangulation_t*)data;

	//
	// treat the current vertex
	//
	if (index >= tri->nvertices) // manage memory for tri->vertices (case when the indexed triangulation is evaluated)
	{
		if (index >= tri->nverticesmax)
		{
			while (index >= tri->nverticesmax)
				tri->nverticesmax *= 2;
			tri->vertices = (float*)realloc(tri->vertices, 3*tri->nverticesmax*sizeof(float));
		}
	}
	else if (index == -1) // add a new vertex and evaluate its associated index (case when the indexed triangulation is not evaluated)
	{
		if (tri->nvertices >= tri->nverticesmax)
		{
			tri->nverticesmax *= 2;
			tri->vertices = (float*)realloc(tri->vertices, 3*tri->nverticesmax*sizeof(float));
		}
		index = tri->nvertices;
	}

	// write the coordinates in tri->vertices
	tri->vertices[3*index]   = x;
	tri->vertices[3*index+1] = y;
	tri->vertices[3*index+2] = z;

	// update tri->nvertices
	if (index >= tri->nvertices)
		tri->nvertices = index+1;
	
	//
	// treat the current face
	//
	if (tri->iCurrentVertexInFace == 0)
	{
		if (tri->nfaces >= tri->nfacesmax)
		{
			tri->nfacesmax *= 2;
			tri->faces = (unsigned int*)realloc(tri->faces, 3*tri->nfacesmax*sizeof(unsigned int));
		}
	}

	tri->faces[3*tri->nfaces+tri->iCurrentVertexInFace] = index;
	tri->iCurrentVertexInFace = (tri->iCurrentVertexInFace+1) % 3;
	if (tri->iCurrentVertexInFace == 0) // new triangle has been completed
	{
		tri->nfaces++; // update tri

		new_triangle_completed = true;
	}
	
	return new_triangle_completed;
}

void ImplicitSurface::get_triangulation_pre (void)
{
	mc_triangulation_t *tri = (mc_triangulation_t*)malloc(sizeof(mc_triangulation_t));
	tri->nverticesmax = 100000;
	tri->vertices = (float*)malloc(3*tri->nverticesmax*sizeof(float));
	tri->nvertices = 0;

	tri->nfacesmax = 100000;
	tri->faces = (unsigned int*)malloc(3*tri->nfacesmax*sizeof(unsigned int));
	tri->nfaces = 0;

	tri->iCurrentVertexInFace = 0;

	data = (void*)tri;
}

void ImplicitSurface::get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces)
{
	mc_triangulation_t *tri = (mc_triangulation_t*)data;
	*nvertices = tri->nvertices;
	*vertices = tri->vertices;
	*nfaces = tri->nfaces;
	*faces = tri->faces;

	free (data);
	data = NULL;
}

void ImplicitSurface::get_triangulation (int *nvertices, float **vertices, int *nfaces, unsigned int **faces)
{
	// store initial data
	mc_vertex_func_t vertex_func_bak = vertex_func;
	void *data_bak = data;
	vertex_func = mc_get_triangulation_vertex_func;

	get_triangulation_pre ();
	compute (1);
	get_triangulation_post (nvertices, vertices, nfaces, faces);
	
	// restore former callbacks
	vertex_func = vertex_func_bak;
	data = data_bak;
}
//
// triangulation
//
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
//
// implicit surface
//

// fGetOffset finds the approximate point of intersection of the surface
// between two points with the values fValue1 and fValue2
static float fGetOffset(float fValue1, float fValue2, float fValueDesired)
{
        double fDelta = fValue2 - fValue1;
        return (fDelta == 0.)? .5 : (fValueDesired - fValue1)/fDelta;
}

//vGetColor generates a color from a given position and normal of a point
static void vGetColor(vec3f &rfColor, vec3f &rfPosition, vec3f &rfNormal)
{
        float fX = rfNormal.fX;
        float fY = rfNormal.fY;
        float fZ = rfNormal.fZ;
        rfColor.fX = (fX > 0. ? fX : 0.) + (fY < 0. ? -.5*fY : 0.) + (fZ < 0. ? -.5*fZ : 0.);
        rfColor.fY = (fY > 0. ? fY : 0.) + (fZ < 0. ? -.5*fZ : 0.) + (fX < 0. ? -.5*fX : 0.);
        rfColor.fZ = (fZ > 0. ? fZ : 0.) + (fX < 0. ? -.5*fX : 0.) + (fY < 0. ? -.5*fY : 0.);
}

// finds the gradient of the scalar field at a point
// This gradient can be used as a very accurate vertex normal for lighting calculations
void ImplicitSurface::get_normal (vec3f &pt, vec3f &normal)
{
	normal.fX = eval_func (pt.fX-0.01, pt.fY, pt.fZ) - eval_func(pt.fX+0.01, pt.fY, pt.fZ);
	normal.fY = eval_func (pt.fX, pt.fY-0.01, pt.fZ) - eval_func(pt.fX, pt.fY+0.01, pt.fZ);
	normal.fZ = eval_func (pt.fX, pt.fY, pt.fZ-0.01) - eval_func(pt.fX, pt.fY, pt.fZ+0.01);

	float fLength = sqrtf( (normal.fX * normal.fX) + (normal.fY * normal.fY) + (normal.fZ * normal.fZ) );

	if(fLength != 0.)
	{
		float fScale = 1. / fLength;
		normal.fX *= fScale;
		normal.fY *= fScale;
		normal.fZ *= fScale;
	}
}

// performs the Marching Cubes algorithm on a single cube
void ImplicitSurface::compute_cube (unsigned int iX, unsigned int iY, unsigned iZ)
{
	float fX = min[0]+iX*step[0];
	float fY = min[1]+iY*step[1];
	float fZ = min[2]+iZ*step[2];
	float fStepX = step[0];
	float fStepY = step[1];
	float fStepZ = step[2];

	int iCorner, iVertex, iVertexTest, iEdge, iTriangle, iFlagIndex, iEdgeFlags;
	float fOffset;
	vec3f sColor;
	float afCubeValue[8];
	vec3f asEdgeVertex[12];
	vec3f asEdgeNorm[12];
	int aiTable[12] = {
		3*((resolution[0]+1)*iY + iX),
		3*((resolution[0]+1)*iY + iX+1)+1,
		3*((resolution[0]+1)*(iY+1) + iX),
		3*((resolution[0]+1)*iY + iX)+1,
		3*((resolution[0]+1)*(resolution[1]+1)) + 3*((resolution[0]+1)*iY + iX),
		3*((resolution[0]+1)*(resolution[1]+1)) + 3*((resolution[0]+1)*iY + iX+1)+1,
		3*((resolution[0]+1)*(resolution[1]+1)) + 3*((resolution[0]+1)*(iY+1) + iX),
		3*((resolution[0]+1)*(resolution[1]+1)) + 3*((resolution[0]+1)*iY + iX)+1,
		3*((resolution[0]+1)*iY + iX)+2,
		3*((resolution[0]+1)*iY + iX+1)+2,
		3*((resolution[0]+1)*(iY+1) + (iX+1))+2,
		3*((resolution[0]+1)*(iY+1) + iX)+2
	};

    // Make a local copy of the values at the cube's corners
	if (fValueCached) // use cache
	{
		int iCacheOrder[2] = {iZ % 2, (iZ+1) % 2};
		for(iVertex = 0; iVertex < 8; iVertex++)
		{
			int index = iCacheOrder[a2fVertexOffset[iVertex][2]]*(resolution[1]+1)*(resolution[0]+1) +
				(iY + a2fVertexOffset[iVertex][1])*(resolution[0]+1) +
				iX + a2fVertexOffset[iVertex][0];
			afCubeValue[iVertex] = fValueCached[index];
		}
	}
	else
		for(iVertex = 0; iVertex < 8; iVertex++)
			afCubeValue[iVertex] = eval_func (fX + a2fVertexOffset[iVertex][0]*fStepX,
							  fY + a2fVertexOffset[iVertex][1]*fStepY,
							  fZ + a2fVertexOffset[iVertex][2]*fStepZ);

    // Find which vertices are inside of the surface and which are outside
    iFlagIndex = 0;
	for(iVertexTest = 0; iVertexTest < 8; iVertexTest++)
		if(afCubeValue[iVertexTest] <= fValue)
			iFlagIndex |= 1<<iVertexTest;

        // Find which edges are intersected by the surface
        iEdgeFlags = aiCubeEdgeFlags[iFlagIndex];

        // If the cube is entirely inside or outside of the surface, then there will be no intersections
        if(iEdgeFlags == 0) 
                return;

        // Find the point of intersection of the surface with each edge
        // Then find the normal to the surface at those points
        for(iEdge = 0; iEdge < 12; iEdge++)
        {
            // if there is an intersection on this edge
            if(iEdgeFlags & (1<<iEdge))
            {
				int indexEdge = aiTable[iEdge];
				if (fIndicesCached[indexEdge] == -1)
				{
					iCurrentVertex++;
					fIndicesCached[indexEdge] = iCurrentVertex;
				}

				fOffset = fGetOffset(afCubeValue[ a2iEdgeConnection[iEdge][0] ],
					afCubeValue[ a2iEdgeConnection[iEdge][1] ], fValue);

				asEdgeVertex[iEdge].fX = fX + (a2fVertexOffset[a2iEdgeConnection[iEdge][0]][0] + fOffset*a2fEdgeDirection[iEdge][0])*fStepX;
				asEdgeVertex[iEdge].fY = fY + (a2fVertexOffset[a2iEdgeConnection[iEdge][0]][1] + fOffset*a2fEdgeDirection[iEdge][1])*fStepY;
				asEdgeVertex[iEdge].fZ = fZ + (a2fVertexOffset[a2iEdgeConnection[iEdge][0]][2] + fOffset*a2fEdgeDirection[iEdge][2])*fStepZ;

				get_normal (asEdgeVertex[iEdge], asEdgeNorm[iEdge]);
			}
		}

        // Draw the triangles that were found.  There can be up to five per cube
        for(iTriangle = 0; iTriangle < 5; iTriangle++)
        {
			if(a2iTriangleConnectionTable[iFlagIndex][3*iTriangle] < 0)
				break;

			for(iCorner = 0; iCorner < 3; iCorner++)
			{
				iVertex = a2iTriangleConnectionTable[iFlagIndex][3*iTriangle+orientation[iCorner]];
				int indexEdge = aiTable[iVertex];

				if (color_func)
				{
					vGetColor(sColor, asEdgeVertex[iVertex], asEdgeNorm[iVertex]);
					if (color_func)
						color_func (sColor.fX, sColor.fY, sColor.fZ);
				}
				if (normal_func)
					normal_func (asEdgeNorm[iVertex].fX, asEdgeNorm[iVertex].fY, asEdgeNorm[iVertex].fZ);
				bool bNewTriangle = vertex_func (asEdgeVertex[iVertex].fX, asEdgeVertex[iVertex].fY, asEdgeVertex[iVertex].fZ, fIndicesCached[indexEdge], data);
				if (bNewTriangle && face_completed_func)
					face_completed_func (data);
               }
        }
}

// iterates over the entire dataset, calling compute_cube on each cube
void ImplicitSurface::compute (int bUsecache)
{
    int iX, iY, iZ;
	int i, j, k;

	if (bBoundary)
	{
		resolution[0] += 2;
		resolution[1] += 2;
		resolution[2] += 3;
		min[0] -= step[0];
		max[0] += step[0];
		min[1] -= step[1];
		max[1] += step[1];
		min[2] -= 2*step[2];
		max[2] += step[2];
	}

	// allocate the caches
	if (bUsecache)
	{
		fValueCached = (float*)malloc((resolution[0]+1)*(resolution[1]+1)*2*sizeof(float));
		if (!fValueCached)
			bUsecache = 0;
	}

	int bUseIndexedFaces = 1;
	int nIndicesCached = (resolution[0]+1)*(resolution[1]+1) * 3 * 2; // 3 edges per vertex & 2 layers of vertices
	if (bUseIndexedFaces)
	{
		fIndicesCached = (int*)malloc(nIndicesCached*sizeof(int));
		if (!fIndicesCached)
			bUseIndexedFaces = 0;
	}

	for (iZ=0; iZ<resolution[2]; iZ++)
	{
		// initialize the cache
		if (bUsecache)
		{
			int index = 0;
			int Zoffset = (resolution[1]+1)*(resolution[0]+1);
			if (iZ == 0)
			{
				for (k=0; k<2; k++)
					for (j=0; j<=resolution[1]; j++)
						for(i = 0; i <= resolution[0]; i++)
						{
							index = k*Zoffset + j*(resolution[0]+1) + i;
							fValueCached[index] = eval_func (min[0]+i*step[0],
											 min[1]+j*step[1],
											 min[2]+k*step[2]);

							if (bBoundary)
							{
								if (iZ==0 || iZ==resolution[2]-1 || j==0 || j==resolution[1] || i==0 || i==resolution[0])
								{
									fValueCached[index] = 1000000;
									if (orientation[1] == 1)
										fValueCached[index] *= -1000000.;
								}
							}
						}
			}
			else
			{
				int XYoffset = 0;
				if (iZ % 2) Zoffset = 0; // use a double buffer for the value cached
				for (j=0; j<=resolution[1]; j++)
					for (i=0; i<=resolution[0]; i++)
					{
						XYoffset = j*(resolution[0]+1) + i;

						// copy layer iZ+1 on layer iZ
						//fValueCached[XYoffset] = fValueCached[Zoffset + XYoffset];

						// update next layer
						fValueCached[Zoffset + XYoffset] = eval_func (min[0]+i*step[0],
											      min[1]+j*step[1],
											      min[2]+(iZ+1)*step[2]);
						if (bBoundary)
						{
							if (iZ==0 || iZ==resolution[2]-1 || j==0 || j==resolution[1] || i==0 || i==resolution[0])
							{
								fValueCached[Zoffset + XYoffset] = 1000000;
								if (orientation[1] == 1)
									fValueCached[Zoffset + XYoffset] *= -1000000.;
							}
						}
					}
			}
		}

		// initialize the indexed faces cache
		if (bUseIndexedFaces)
		{
//			int index = 0;
			int Zoffset = 3*(resolution[0]+1)*(resolution[1]+1);
			if (iZ == 0)
				memset (fIndicesCached, -1, nIndicesCached*sizeof(int));
			else
			{
//				if (iZ % 2) Zoffset = 0; // use a double buffer
				for (j=0; j<=resolution[1]; j++)
					for (i=0; i<=resolution[0]; i++)
					{
						int index = j*(resolution[0]+1) + i;

						// copy layer 2 on layer 0
						fIndicesCached[3*index]   = fIndicesCached[Zoffset + 3*index];
						fIndicesCached[3*index+1] = fIndicesCached[Zoffset + 3*index+1];
						fIndicesCached[3*index+2] = fIndicesCached[Zoffset + 3*index+2];
						
						// update next layers
						fIndicesCached[Zoffset + 3*index]   = -1;
						fIndicesCached[Zoffset + 3*index+1] = -1;
						fIndicesCached[Zoffset + 3*index+2] = -1;
					}
			}
		}

		// compute a layer
		for(iY = 0; iY < resolution[1]; iY++)
			for(iX = 0; iX < resolution[0]; iX++)
				compute_cube (iX, iY, iZ);

		if (layer_completed_func)
			layer_completed_func (data);
	}

	// free the cache
	if (bUseIndexedFaces)
	{
		free (fIndicesCached);
		fIndicesCached = NULL;
	}
	if (bUsecache)
	{
		free (fValueCached);
		fValueCached = NULL;
	}

	// restore
	if (bBoundary)
	{
		resolution[0] -= 2;
		resolution[1] -= 2;
		resolution[2] -= 3;
		min[0] += step[0];
		max[0] -= step[0];
		min[1] += step[1];
		max[1] -= step[1];
		min[2] += 2*step[2];
		max[2] -= step[2];
	}
}
