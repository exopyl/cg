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

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <vector>

#include "surface_implicit.h"
#include "surface_implicit_def.h"
#include "surface_implicit_triangulation.h"

ImplicitSurface::ImplicitSurface ()
{
	set_bbox (-.5, -.5, -.5, .5, .5, .5);
	set_orientation (1);
	set_resolution_per_unit (32);
	set_value (48.);
	bBoundary = 0;

	eval_func = nullptr;
	color_func = nullptr;
	vertex_func = nullptr;
	normal_func = nullptr;
	face_completed_func = nullptr;
	layer_completed_func = nullptr;
	data = nullptr;
	eval_data = nullptr;
	fValueCached = nullptr;
	fIndicesCached = nullptr;
	iCurrentVertex = -1;
};


////////////////////////////////////////////////////////////////////////////////
//
// triangulation
//
namespace
{
	std::atomic<void *(*) (std::size_t)> outputAllocate {nullptr};
	std::atomic<void (*) (void *)>       outputRelease {nullptr};
}

ImplicitOutputAllocator SetImplicitOutputAllocator (ImplicitOutputAllocator allocator)
{
	ImplicitOutputAllocator previous;
	previous.allocate = outputAllocate.exchange (allocator.allocate);
	previous.release = outputRelease.exchange (allocator.release);
	return previous;
}

void *AllocateImplicitOutput (std::size_t bytes)
{
	void *(*const allocate) (std::size_t) = outputAllocate.load ();
	return allocate ? allocate (bytes) : malloc (bytes);
}

void ReleaseImplicitOutput (void *block)
{
	void (*const release) (void *) = outputRelease.load ();
	if (release)
		release (block);
	else
		free (block);
}

bool ImplicitTriangulation::AddCorner (float x, float y, float z, int index, void *self)
{
	ImplicitTriangulation *tri = FromData (self);
	if (index < 0)
		index = tri->nvertices;
	const std::size_t i = static_cast<std::size_t> (index);
	if (3 * i + 3 > tri->vertices.size ())
		tri->vertices.resize (3 * i + 3);
	tri->vertices[3 * i]     = x;
	tri->vertices[3 * i + 1] = y;
	tri->vertices[3 * i + 2] = z;
	if (index >= tri->nvertices)
		tri->nvertices = index + 1;

	tri->faces.push_back (static_cast<unsigned int> (index));
	if (tri->faces.size () % 3 != 0)
		return false;
	++tri->nfaces;
	return true;
}

bool ImplicitTriangulation::CopyOut (int *nv, float **v, int *nf, unsigned int **f) const
{
	*nv = 0;
	*v = nullptr;
	*nf = 0;
	*f = nullptr;
	const std::size_t vertexFloats = 3 * static_cast<std::size_t> (nvertices);
	const std::size_t faceIndices = 3 * static_cast<std::size_t> (nfaces);
	float *outVertices = vertexFloats ? static_cast<float *> (AllocateImplicitOutput (vertexFloats * sizeof (float))) : nullptr;
	unsigned int *outFaces = faceIndices ? static_cast<unsigned int *> (AllocateImplicitOutput (faceIndices * sizeof (unsigned int))) : nullptr;
	if ((vertexFloats && !outVertices) || (faceIndices && !outFaces))
	{
		ReleaseImplicitOutput (outVertices);
		ReleaseImplicitOutput (outFaces);
		return false;
	}
	if (vertexFloats)
		memcpy (outVertices, vertices.data (), vertexFloats * sizeof (float));
	if (faceIndices)
		memcpy (outFaces, faces.data (), faceIndices * sizeof (unsigned int));
	*nv = nvertices;
	*v = outVertices;
	*nf = nfaces;
	*f = outFaces;
	return true;
}

void ImplicitSurface::get_triangulation_pre (void)
{
	data = new ImplicitTriangulation ();
}

bool ImplicitSurface::get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces,
                                              bool computed)
{
	std::unique_ptr<ImplicitTriangulation> tri (ImplicitTriangulation::FromData (data));
	data = nullptr;
	return computed && tri->CopyOut (nvertices, vertices, nfaces, faces);
}

bool ImplicitSurface::get_triangulation (int *nvertices, float **vertices, int *nfaces, unsigned int **faces)
{
	*nvertices = 0;
	*vertices = nullptr;
	*nfaces = 0;
	*faces = nullptr;

	const mc_vertex_func_t vertexFuncBak = vertex_func;
	void *const dataBak = data;
	bool prepared = false, computed = false;
	try
	{
		get_triangulation_pre ();
		prepared = true;
		vertex_func = &ImplicitTriangulation::AddCorner;
		computed = compute (1);
	}
	catch (const std::bad_alloc &)
	{
		computed = false;
	}
	catch (...)
	{
		vertex_func = vertexFuncBak;
		if (prepared)
			get_triangulation_post (nvertices, vertices, nfaces, faces, false);
		data = dataBak;
		throw;
	}
	vertex_func = vertexFuncBak;
	const bool ok = prepared && get_triangulation_post (nvertices, vertices, nfaces, faces, computed);
	data = dataBak;
	return ok;
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
	normal.fX = eval_func (pt.fX-0.01, pt.fY, pt.fZ, eval_data) - eval_func(pt.fX+0.01, pt.fY, pt.fZ, eval_data);
	normal.fY = eval_func (pt.fX, pt.fY-0.01, pt.fZ, eval_data) - eval_func(pt.fX, pt.fY+0.01, pt.fZ, eval_data);
	normal.fZ = eval_func (pt.fX, pt.fY, pt.fZ-0.01, eval_data) - eval_func(pt.fX, pt.fY, pt.fZ+0.01, eval_data);

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
	auto resX = static_cast<unsigned int>(resolution[0]);
	auto resY = static_cast<unsigned int>(resolution[1]);
	unsigned int aiTable[12] = {
		3U * ((resX +1U)*iY + iX),
		3U * ((resX +1U)*iY + iX+1U)+1U,
		3U * ((resX +1U)*(iY+1U) + iX),
		3U *((resX +1U)*iY + iX)+1U,
		3U *((resX +1U)*(resY +1U)) + 3U *((resX +1U)*iY + iX),
		3U *((resX +1U)*(resY +1U)) + 3U *((resX +1U)*iY + iX+1U)+1U,
		3U *((resX +1U)*(resY +1U)) + 3U *((resX +1U)*(iY+1U) + iX),
		3U *((resX +1U)*(resY +1U)) + 3U *((resX +1U)*iY + iX)+1U,
		3U *((resX +1U)*iY + iX)+2U,
		3U *((resX +1U)*iY + iX+1U)+2U,
		3U *((resX +1U)*(iY+1U) + (iX+1U))+2U,
		3U *((resX +1U)*(iY+1U) + iX)+2U
	};

    // Make a local copy of the values at the cube's corners
	if (fValueCached) // use cache
	{
		const std::size_t row = static_cast<std::size_t> (resX) + 1U;
		const std::size_t layer = row * (static_cast<std::size_t> (resY) + 1U);
		unsigned int iCacheOrder[2] = {iZ % 2U, (iZ+1U) % 2U};
		for(iVertex = 0; iVertex < 8; iVertex++)
		{
			const std::size_t index = iCacheOrder[a2fVertexOffset[iVertex][2]]*layer +
				(iY + a2fVertexOffset[iVertex][1])*row +
				iX + a2fVertexOffset[iVertex][0];
			afCubeValue[iVertex] = fValueCached[index];
		}
	}
	else
		for(iVertex = 0; iVertex < 8; iVertex++)
			afCubeValue[iVertex] = eval_func (fX + a2fVertexOffset[iVertex][0]*fStepX,
							  fY + a2fVertexOffset[iVertex][1]*fStepY,
							  fZ + a2fVertexOffset[iVertex][2]*fStepZ, eval_data);

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

				if (normal_func || color_func)
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
						color_func (sColor.fX, sColor.fY, sColor.fZ, eval_data);
				}
				if (normal_func)
					normal_func (asEdgeNorm[iVertex].fX, asEdgeNorm[iVertex].fY, asEdgeNorm[iVertex].fZ, eval_data);
				bool bNewTriangle = vertex_func (asEdgeVertex[iVertex].fX, asEdgeVertex[iVertex].fY, asEdgeVertex[iVertex].fZ, fIndicesCached[indexEdge], data);
				if (bNewTriangle && face_completed_func)
					face_completed_func (data);
               }
        }
}

namespace
{
	// Pendant compute : boite et resolution elargies d'un bord (set_boundary), caches
	// designes par les pointeurs membres. Retabli a la sortie, exception comprise.
	class ComputeScope
	{
	public:
		ComputeScope (int boundary, float *min, float *max, int *resolution, const float *step,
		              float *&values, int *&indices, float *valueData, int *indexData)
			: m_boundary (boundary), m_min (min), m_max (max), m_resolution (resolution), m_step (step),
			  m_values (values), m_indices (indices)
		{
			if (m_boundary)
			{
				m_resolution[0] += 2;
				m_resolution[1] += 2;
				m_resolution[2] += 3;
				m_min[0] -= m_step[0];
				m_max[0] += m_step[0];
				m_min[1] -= m_step[1];
				m_max[1] += m_step[1];
				m_min[2] -= 2*m_step[2];
				m_max[2] += m_step[2];
			}
			m_values = valueData;
			m_indices = indexData;
		}
		~ComputeScope ()
		{
			m_values = nullptr;
			m_indices = nullptr;
			if (m_boundary)
			{
				m_resolution[0] -= 2;
				m_resolution[1] -= 2;
				m_resolution[2] -= 3;
				m_min[0] += m_step[0];
				m_max[0] -= m_step[0];
				m_min[1] += m_step[1];
				m_max[1] -= m_step[1];
				m_min[2] += 2*m_step[2];
				m_max[2] -= m_step[2];
			}
		}
		ComputeScope (const ComputeScope &) = delete;
		ComputeScope &operator= (const ComputeScope &) = delete;

	private:
		int          m_boundary;
		float       *m_min, *m_max;
		int         *m_resolution;
		const float *m_step;
		float      *&m_values;
		int        *&m_indices;
	};
}

// iterates over the entire dataset, calling compute_cube on each cube
bool ImplicitSurface::compute (int bUsecache)
{
	iCurrentVertex = -1;

	const long long grow[3] = {bBoundary ? 2 : 0, bBoundary ? 2 : 0, bBoundary ? 3 : 0};
	const long long r0 = static_cast<long long> (resolution[0]) + grow[0];
	const long long r1 = static_cast<long long> (resolution[1]) + grow[1];
	const long long r2 = static_cast<long long> (resolution[2]) + grow[2];
	if (r0 < 1 || r1 < 1 || r2 < 1 || (r0 + 1) * (r1 + 1) > static_cast<long long> (kMaxLayerNodes))
		return false;
	const std::size_t layer = static_cast<std::size_t> ((r0 + 1) * (r1 + 1));

	// Caches : valeurs des noeuds sur deux couches, indices des sommets sur les trois aretes
	// issues de chaque noeud de deux couches.
	std::vector<float> valueCache;
	std::vector<int>   indexCache;
	try
	{
		if (bUsecache)
			valueCache.resize (2 * layer);
		indexCache.resize (6 * layer);
	}
	catch (const std::bad_alloc &)
	{
		return false;
	}
	const ComputeScope scope (bBoundary, min, max, resolution, step, fValueCached, fIndicesCached,
	                          bUsecache ? valueCache.data () : nullptr, indexCache.data ());

	const std::size_t row = static_cast<std::size_t> (resolution[0]) + 1;
	int iX, iY, iZ;
	int i, j, k;
	for (iZ=0; iZ<resolution[2]; iZ++)
	{
		// initialize the cache
		if (bUsecache)
		{
			std::size_t index = 0;
			std::size_t Zoffset = layer;
			if (iZ == 0)
			{
				for (k=0; k<2; k++)
					for (j=0; j<=resolution[1]; j++)
						for(i = 0; i <= resolution[0]; i++)
						{
							index = k*Zoffset + j*row + i;
							fValueCached[index] = eval_func (min[0]+i*step[0],
											 min[1]+j*step[1],
											 min[2]+k*step[2], eval_data);

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
				std::size_t XYoffset = 0;
				if (iZ % 2) Zoffset = 0; // use a double buffer for the value cached
				for (j=0; j<=resolution[1]; j++)
					for (i=0; i<=resolution[0]; i++)
					{
						XYoffset = j*row + i;

						// update next layer
						fValueCached[Zoffset + XYoffset] = eval_func (min[0]+i*step[0],
											      min[1]+j*step[1],
											      min[2]+(iZ+1)*step[2], eval_data);
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
		{
			const std::size_t Zoffset = 3*layer;
			if (iZ == 0)
				std::fill (indexCache.begin (), indexCache.end (), -1);
			else
			{
				for (j=0; j<=resolution[1]; j++)
					for (i=0; i<=resolution[0]; i++)
					{
						const std::size_t index = j*row + i;

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
	return true;
}
