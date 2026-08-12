#pragma once
#include <memory>
#include <vector>

#include "orientation.h"

//
//
//
class Cmesh_orientation_curvatures : public Cmesh_orientation
{
 public:
  Cmesh_orientation_curvatures (Mesh_half_edge *model, int w, int h);

  void set_size_accumulator (int w, int h);
  virtual void compute_orientation (float t1, float t2);
  void find_orientation (int id);
  
  int  *get_accumulator (void) { return accumulator_int.data(); };
  void get_phi_theta (float *pphi, float *ptheta)
  {
    *pphi = phi;
    *ptheta = theta;
  };
  void get_iphi_itheta (int *piphi, int *pitheta)
  {
    *piphi = iphi_max;
    *pitheta = itheta_max;
  };

  void output_accumulator (char *filename);

 private:
  void finalize_orientation (void);

 private:
  int w, h;
  // Etaient deux `int*` malloc'es a chaque compute_orientation et JAMAIS
  // liberes : la classe n'a pas de destructeur. std::vector rend celui-ci
  // inutile et supprime la fuite par la meme occasion.
  std::vector<int> accumulator;
  std::vector<int> accumulator_int;

  float phi, theta;
  int iphi_max, itheta_max;
};
