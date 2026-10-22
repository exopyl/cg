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

// Tube de section reguliere a nSides cotes (rayon `radius`) le long de chaque
// polyligne, reunis en un seul Mesh. Les points sont pris tels quels, sans
// normalisation.
// - Un anneau de nSides sommets par point, partage par les deux segments voisins ;
//   un coin est joint en onglet (anneau etire dans le plan bissecteur), ou en
//   biseau (deux anneaux) au-dela d'une limite d'onglet de 4. Les points confondus
//   a 1e-6 pres sont fusionnes. Une polyligne dont le dernier point revient sur le
//   premier (a moins de radius / 2, au moins quatre points) est une boucle.
// - Les extremites qui coincident (meme case de 1e-4) forment une jonction, fermee
//   par l'enveloppe convexe des anneaux qui s'y touchent (au plus 8 000 anneaux en
//   tout ; au-dela, ou si l'enveloppe echoue, par leurs capuchons).
// - caps = true : capuchon (nSides - 2 triangles, oriente vers l'exterieur) a
//   chaque extremite libre d'une polyligne ouverte ; une boucle n'en a pas.
// - nSides est ramene dans [kTubeMinSides ; kTubeMaxSides].
// - Une polyligne qui contient un point non fini est ecartee entiere ; une
//   polyligne de moins de deux points distincts ne donne rien. Seuls les points
//   d'entree sont controles : un NaN calcule a partir de points finis immenses
//   (difference ou norme qui deborde) n'est pas filtre. Les appelants du depot
//   normalisent leurs points en amont.
// - Sans aucun tube, le Mesh rendu est vide (0 sommet, 0 face).
// Une polyligne ouverte de M points distincts sans biseau donne M * nSides
// sommets et 2 nSides (M - 1) + 2 (nSides - 2) faces avec capuchons.
constexpr int kTubeMinSides = 3;
constexpr int kTubeMaxSides = 1024;
extern Mesh* CreateTube  (const std::vector<Vector3f>& polyline, float radius, int nSides = 6, bool caps = true);
extern Mesh* CreateTubes (const std::vector<std::vector<Vector3f>>& polylines, float radius, int nSides = 6, bool caps = true);

extern Mesh* CreateTeapot (void);
extern Mesh* CreateKleinBottle (int ThetaResolution, int PhiResolution);
