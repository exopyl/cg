#ifndef __TOPOLOGY_H__
#define __TOPOLOGY_H__

#include "../cgmath/cgmath.h"

extern void create_indexation(vec3 *pTriangles, unsigned int nPoints,
			      unsigned int **pIndexation,
			      double dVertexEpsilon,
			      const vec3 *pNormal, double dNormalEpsilon);

extern void remove_unused_vertices (vec3 **pVertices, unsigned int *nVerticesNew, unsigned int **pIndexation, unsigned int nFaces);

#endif //  __TOPOLOGY_H__
