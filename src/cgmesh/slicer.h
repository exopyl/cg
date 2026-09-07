#pragma once
// Ce que ce fichier utilise, et non le parapluie.
//
// Il incluait `cgmesh.h`, lequel l'inclut en retour : un CYCLE, que `#pragma
// once` rend silencieux mais qui impose a tout consommateur de ce fichier les 55
// inclusions du parapluie -- zlib, l'audio, l'ONNX comprises.
#include "mesh_half_edge.h"
#include "polygon2.h"

#define ALONGOX 0
#define ALONGOY 1
#define ALONGOZ 2

class Cmodel3d_half_edge_sliced
{
 public:
  Cmodel3d_half_edge_sliced (Mesh_half_edge *model, int dir, float _step_slice);
  ~Cmodel3d_half_edge_sliced ();

  Mesh_half_edge* get_model (void) { return model; };

  void get_areas (float **areas, int *size);
  void get_slice (int index, Polygon2 ***slice, int *nc);
  int  get_n_slices (void) { return n_slices; };

  float get_step_slice (void) { return step_slice; };
  float get_zmin (void) { return zmin; };
  float get_xmin (void) { return xmin; };

  void look_at_symmetry (void);

  void dump (char *prefix);
  
 private:
  int direction;

  void scan_model_along_Ox (void);
  void scan_model_along_Oz (void);
  void scan_model (int dir);

  Mesh_half_edge *model;

  float zmin, xmin;

  // slices
  int n_slices;
  Polygon2 ***slices;
  int *n_contours;
  float step_slice; // distance between two consecutive slices
};
