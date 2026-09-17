#pragma once
// Ce que ce fichier utilise, et non le parapluie.
//
// Il incluait `cgmesh.h`, lequel l'inclut en retour : un CYCLE, que `#pragma
// once` rend silencieux mais qui impose a tout consommateur de ce fichier les 55
// inclusions du parapluie -- zlib, l'audio, l'ONNX comprises.
#include "mesh_half_edge.h"

// Section d'un maillage par un plan, rendue comme un jeu de contours fermes.
//
// ⚠ Precondition : variete FERMEE. La fermeture de chaque contour repose sur
// `m_pair` ; un bord ou une arete non variete tronque le contour SANS le
// signaler. Le detail, et les tests qui le fixent, sont dans slicer.h.
//
// PROPRIETE de ce que rend `get_intersections` : `n_intersections == 0` <=> les
// deux pointeurs de sortie valent nullptr et il n'y a rien a liberer. Sinon
// l'appelant possede les `n_intersections` contours, le tableau qui les porte,
// et celui des comptes -- tous obtenus par malloc, donc a rendre par free.
class Cmodel3d_half_edge_clipper
{
 public:
  Cmodel3d_half_edge_clipper (Mesh_half_edge *model);
  ~Cmodel3d_half_edge_clipper ();

  void set_plane (Vector3d pt, Vector3d n);
  void get_intersections (int *n_intersections, int **n_vertices, float ***intersections);

 private:
  void get_vertex_intersection (int i, int j, Vector3d &inter);

  // half edge model
  Mesh_half_edge *model;
  
  // plane
  Vector3d n;
  float d;

  // distances between the vertices and the plane
  float *distances;
};
