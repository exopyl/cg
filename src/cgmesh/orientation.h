#ifndef __MESH_ORIENTATION_H__
#define __MESH_ORIENTATION_H__

#include "mesh_half_edge.h"

class Cmesh_orientation
{
 public:
  //Cmesh_orientation () {};
  Cmesh_orientation (Mesh *model);
  Cmesh_orientation (Mesh_half_edge *model);
  
  virtual void compute_orientation (int id=0) = 0;
  void apply_orientation (void);
  void normalize (void);
  float *get_matrix_rotation (void);
  float *get_center (void);
  
  void dump_orientation (void);

 protected:
  Mesh *mesh;
  Mesh_half_edge *model3d_half_edge;
  float center[3];
  float mrot[9];
};

#endif // __MESH_ORIENTATION_H__
