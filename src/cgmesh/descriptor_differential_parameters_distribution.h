#ifndef __DIFFERENTIAL_PARAMETERS_DISTRIBUTION_H__
#define __DIFFERENTIAL_PARAMETERS_DISTRIBUTION_H__

#include "mesh_half_edge.h"

//
//
//
class Cdifferential_parameters_distribution
{
 public:
  Cdifferential_parameters_distribution (Mesh_half_edge *_model);
  ~Cdifferential_parameters_distribution ();

  enum shape_function_type {BESL, GAUSSIAN_CURVATURE, MEAN_CURVATURE, MAX_CURVATURE, MIN_CURVATURE, MPEG7};

  void compute_distribution (shape_function_type type, int n_bins);
  void normalize_distribution (void);
  void export_distribution (char *filename);

 private:
  Mesh_half_edge *model;
  
  float *histogram;
  int n_bins;
};

#endif // __DIFFERENTIAL_PARAMETERS_DISTRIBUTION_H__
