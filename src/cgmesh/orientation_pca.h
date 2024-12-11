#ifndef __MESH_ORIENTATION_PCA_H__
#define __MESH_ORIENTATION_PCA_H__

#include "orientation.h"

class Cmesh_orientation_pca : public Cmesh_orientation
{
 public:
  Cmesh_orientation_pca (Mesh_half_edge *model);

  /* compute PCA */
  /*
   * 0 : PCA
   * 1 : weighted vertices
   * 2 : barycenter
   * 3 : continuous PCA
   */
  virtual void compute_orientation (int id_method);

 private:
  void compute_pca                   (void);
  void compute_pca_weighted_vertices (void);
  void compute_pca_barycenter        (void);
  void compute_pca_continuous        (void);
};

#endif // __MESH_ORIENTATION_PCA_H__
