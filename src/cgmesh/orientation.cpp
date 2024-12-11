#include "orientation.h"

Cmesh_orientation::Cmesh_orientation (Mesh *_model)
{
  mesh = _model;
  model3d_half_edge = NULL;
  center[0] = center[1] = center[2] = 0.0;
  mrot[0] = mrot[4] = mrot[8] = 1.0;
  mrot[1] = mrot[2] = mrot[3] = mrot[5] = mrot[6] = mrot[7] = 0.0;
}

Cmesh_orientation::Cmesh_orientation (Mesh_half_edge *_model)
{
  mesh = NULL;
  model3d_half_edge = _model;
  center[0] = center[1] = center[2] = 0.0;
  mrot[0] = mrot[4] = mrot[8] = 1.0;
  mrot[1] = mrot[2] = mrot[3] = mrot[5] = mrot[6] = mrot[7] = 0.0;
}

void
Cmesh_orientation::apply_orientation (void)
{
  int nv;
  float *v;
  if (mesh)
  {
	  mesh->translate (-center[0], -center[1], -center[2]);
	  mesh->transform (mrot);
  }
  if (model3d_half_edge)
  {
	  model3d_half_edge->translate (-center[0], -center[1], -center[2]);
	  model3d_half_edge->transform (mrot);
  }
}

void
Cmesh_orientation::normalize (void)
{
  int i;
  int nv;
  float *v;
  if (mesh)
  {
	  nv = mesh->m_nVertices;
	  v = mesh->m_pVertices;
  }
  if (model3d_half_edge)
  {
	  nv = model3d_half_edge->m_nVertices;
	  v = model3d_half_edge->m_pVertices;
  }
  if (nv < 1 || !v)
	  return;

  float xmin = v[0], xmax = v[0];
  float ymin = v[1], ymax = v[1];
  float zmin = v[2], zmax = v[2];
  for (i=1; i<nv; i++)
    {
      if (xmin > v[3*i]) xmin = v[3*i];
      if (xmax < v[3*i]) xmax = v[3*i];

      if (ymin > v[3*i+1]) ymin = v[3*i+1];
      if (ymax < v[3*i+1]) ymax = v[3*i+1];

      if (zmin > v[3*i+2]) zmin = v[3*i+2];
      if (zmax < v[3*i+2]) zmax = v[3*i+2];
    }
  for (i=0; i<nv; i++)
    {
      v[3*i]   = (2*v[3*i] - xmin - xmax) / (xmax - xmin);
      v[3*i+1] = (2*v[3*i+1] - ymin - ymax) / (ymax - ymin);
      v[3*i+2] = (2*v[3*i+2] - zmin - zmax) / (zmax - zmin);
    }
}

float*
Cmesh_orientation::get_matrix_rotation (void)
{
  return mrot;
}

float*
Cmesh_orientation::get_center (void)
{
	return center;
}

void
Cmesh_orientation::dump_orientation (void)
{
  printf ("( %12.6f , %12.6f , %12.6f )\n\n", center[0], center[1], center[2]);
  printf ("[ %12.6f , %12.6f , %12.6f ]\n", mrot[0], mrot[1], mrot[2]);
  printf ("[ %12.6f , %12.6f , %12.6f ]\n", mrot[3], mrot[4], mrot[5]);
  printf ("[ %12.6f , %12.6f , %12.6f ]\n", mrot[6], mrot[7], mrot[8]);
}

