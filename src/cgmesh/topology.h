#pragma once
#include "../cgmath/cgmath.h"

extern void create_indexation(Vector3f *pTriangles, unsigned int nPoints,
			      unsigned int **pIndexation,
			      double dVertexEpsilon,
			      const Vector3f *pNormal, double dNormalEpsilon);

extern void remove_unused_vertices (Vector3f **pVertices, unsigned int *nVerticesNew, unsigned int **pIndexation, unsigned int nFaces);
