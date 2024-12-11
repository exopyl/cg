#ifndef __DISTRIBUTION_AROUND_AXIS_H__
#define __DISTRIBUTION_AROUND_AXIS_H__

#include "mesh_half_edge.h"

class Cdistribution_around_axis
{
 public:
  Cdistribution_around_axis (Mesh_half_edge *model);
  ~Cdistribution_around_axis () {};

  enum orientation_type {ORIENTATION_PCA, ORIENTATION_WEIGHTED_VERTICES, ORIENTATION_BARYCENTER, ORIENTATION_CPCA};

  /* frist order */
  void compute_first_order_distributions  (orientation_type type);

  float *get_lengths    (void) { return lengths;    };
  float *get_dmeans     (void) { return dmeans;     };
  float *get_variances  (void) { return variances;  };
  float *get_deviations (void) { return deviations; };

  /* second order */
  /*
   * Reference
   * "Nefertiti: a query by content system for three-dimensional model and image databases management"
   * Eric Paquet, Marc Rioux
   * Image and Vision Computing, 17, pp 157-166, 1999
   */
  void compute_first_order_distributions_paquet  (orientation_type type, int npoints, int nbins);
  void compute_second_order_distributions_paquet (orientation_type type, int npoints, int nbins);
  void normalize_distributions (void);

  void export_histogram1k  (char *filename);
  void export_histogram2k1 (char *filename);
  void export_histogram2k2 (char *filename);
  void export_histogram3k  (char *filename);

  float  *get_histogram1k  (void) { return histogram1k; };
  float  *get_histogram2k1 (void) { return histogram2k1; };
  float  *get_histogram2k2 (void) { return histogram2k2; };
  float **get_histogram3k  (void) { return histogram3k; };

 private:
  float *cumulative_areas;
  int n_cumulative_areas;
  float total_area;
  void compute_cumulative_areas (void);
  int  find_area_position (float random_area, int istart, int iend);
  int select_random_point (vec3 point); // return the id of the face containing the point

  void compute_length_dmean_variance_deviation (vec3 axis, float *_length, float *_dmean, float *_variance, float *_deviation);

  Mesh_half_edge *model;
  int nv, nf;
  float *v;
  Face **f;

  /* first order distributions */
  float lengths[3], dmeans[3], variances[3], deviations[3];

  /* second order distributions */
  int npoints;
  int nbins;
  float *histogram1k;
  float *histogram2k1, *histogram2k2;
  float **histogram3k;
};

#endif /* __MODEL3D_FEATURES_DISTRIBUTION_AROUND_AXIS_H__ */
