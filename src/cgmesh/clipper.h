#pragma once
// Ce que ce fichier utilise, et non le parapluie.
//
// Il incluait `cgmesh.h`, lequel l'inclut en retour : un CYCLE, que `#pragma
// once` rend silencieux mais qui impose a tout consommateur de ce fichier les 55
// inclusions du parapluie -- zlib, l'audio, l'ONNX comprises.
#include "mesh_half_edge.h"

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
