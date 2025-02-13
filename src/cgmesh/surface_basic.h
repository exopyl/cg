#pragma once

#include "mesh.h"

extern Mesh* CreateTriangle (float x1, float y1, float z1,
			     float x2, float y2, float z2,
			     float x3, float y3, float z3);
extern Mesh* CreateQuad (void);
extern Mesh* CreateGrid (unsigned int nx, unsigned int ny);
extern Mesh* CreateCube (bool bTri = false);
extern Mesh* CreateTetrahedron (void);
extern Mesh* CreateOctahedron (void);
extern Mesh* CreateDodecahedron (void);
extern Mesh* CreateDisk (unsigned int nVertices);
extern Mesh* CreateCone (float fHeight, float fRadius, unsigned int nVertices, bool bCap);
extern Mesh* CreateCylinder (float fHeight, float fRadius, unsigned int nVertices, bool bCap, bool bTriangular = 0);
extern Mesh* CreateCapsule (unsigned int n, float height, float radius);

extern Mesh* CreateTeapot (void);
extern Mesh* CreateKleinBottle (int ThetaResolution, int PhiResolution);
