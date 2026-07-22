#pragma once

#include "mesh.h"
#include <vector>

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

// Balaye un tube de section nSides-gone (rayon `radius`) le long d'une polyligne.
// Ne normalise rien (tube les points tels quels). Iso-fonctionnel pour l'instant :
// chaque segment est tube independamment (jointures a venir). CreateTubes fusionne
// plusieurs polylignes (branches / traits disjoints) en un seul Mesh.
// caps = true : bouche les deux extremites de chaque polyligne (couvercle
// oriente vers l'exterieur) ; ignore pour une polyligne fermee (boucle).
extern Mesh* CreateTube  (const std::vector<Vector3f>& polyline, float radius, int nSides = 6, bool caps = true);
extern Mesh* CreateTubes (const std::vector<std::vector<Vector3f>>& polylines, float radius, int nSides = 6, bool caps = true);

extern Mesh* CreateTeapot (void);
extern Mesh* CreateKleinBottle (int ThetaResolution, int PhiResolution);
