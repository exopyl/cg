#pragma once
#include "surface_implicit.h"

//
// tandem algorithm
//
// reference :
//   "Extraction and Simplification of Iso-surfaces in Tandem"
//   http://www.gipsa-lab.grenoble-inp.fr/~dominique.attali/Publications/05-sgp.pdf
//
class ImplicitSurfaceTandem : public ImplicitSurface
{
public:
	ImplicitSurfaceTandem ();
	~ImplicitSurfaceTandem ();

private:
	virtual void get_triangulation_pre (void);
	virtual void get_triangulation_post (int *nvertices, float **vertices, int *nfaces, unsigned int **faces);
};
