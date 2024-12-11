#ifndef __MESH_ORIENTATION_MOMENTS_H__
#define __MESH_ORIENTATION_MOMENTS_H__

#include "orientation.h"

/*
 *
 * Code adapted from code written by Brian Mirtich
 * from the paper:
 *
 *  Brian Mirtich, "Fast and Accurate Computation of
 *  Polyhedral Mass Properties," journal of graphics
 *  tools, volume 1, number 2, 1996.
 *
 */

class Cmesh_orientation_moments : public Cmesh_orientation
{
 public:
  Cmesh_orientation_moments (Mesh_half_edge *model);

  virtual void compute_orientation ();

 private:
  double density, mass;
};

#endif /* __MESH_ORIENTATION_MOMENTS_H__ */
