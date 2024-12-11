#ifndef __MESH_ORIENTATION_EDGES2_H__
#define __MESH_ORIENTATION_EDGES2_H__

#include "orientation.h"

class Cmesh_orientation_edges2 : public Cmesh_orientation
{
 public:
  Cmesh_orientation_edges2 (Mesh *model, int w, int h);
  Cmesh_orientation_edges2 (Mesh_half_edge *model, int w, int h);

  void apply_orientation (void);
  void output_model      (char *filename);

  int  *get_accumulator (void) { return accumulator_int; };
  void set_iphi_itheta (int _iphi, int _itheta)
  {
    iphi_max = _iphi;
    itheta_max = _itheta;
    phi = (3.14159 * iphi_max / w - 3.14159 / 2.0);
    theta = ((3.14159 * itheta_max) / h);
    finalize_orientation ();
  };

  void get_iphi_itheta (int *piphi, int *pitheta)
  {
    *piphi = iphi_max;
    *pitheta = itheta_max;
  };

  void set_size_accumulator (int w, int h);
  virtual void compute_orientation  (float t);
  void compute_orientation2 (void);
  void find_orientation (int id);

  void output_accumulator (char *filename);

 private:
  void finalize_orientation (void);

 private:
  Mesh_half_edge *model;

  int w, h;
  float *accumulator;
  int *accumulator_int;

  float phi, theta;
  int iphi_max, itheta_max;
};

#endif /* __MESH_ORIENTATION_EDGES2_H__ */
