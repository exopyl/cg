#ifndef __NORMALS_H__
#define __NORMALS_H__

#include "mesh_half_edge.h"

//
// THURMER
// G. Thurmer, C. A. Wuthrich, "Computing vertex normals from polygonal facets" Journal of Graphics Tools, 3 1998
//
// MAX
// Nelson Max, "Weights for Computing Vertex Normals from Facet Normals", Journal of Graphics Tools, 4(2) (1999)
//
// Comparisons :
// S. Jin, R.R. Lewis, D. West, "A comparison of algorithms for vertex normal computations", The Visual Computer, 2005 - Springer
//
class Normals
{
public:
enum MethodId {
	GOURAUD,
	THURMER,
	MAX,
	DESBRUN
   };

	Normals () {};
	~Normals () {};

	int EvalOnVertices (Mesh_half_edge *mesh, MethodId par_id);
	int EvalOnFaces (Mesh_half_edge *mesh);

	void invert_vertices_normales (Mesh_half_edge *mesh);

private:
};

#endif // __NORMALS
