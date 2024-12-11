#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "orientation_edges2.h"

Cmesh_orientation_edges2::Cmesh_orientation_edges2 (Mesh *_model, int _w, int _h)
  : Cmesh_orientation(_model)
{
  w = _w;
  h = _h;
  accumulator     = NULL;
  accumulator_int = NULL;
  mesh = _model;
  model3d_half_edge = NULL;
}

Cmesh_orientation_edges2::Cmesh_orientation_edges2 (Mesh_half_edge *_model, int _w, int _h)
  : Cmesh_orientation(_model)
{
  w = _w;
  h = _h;
  accumulator     = NULL;
  accumulator_int = NULL;
  mesh = NULL;
  model3d_half_edge = _model;
}

void
Cmesh_orientation_edges2::set_size_accumulator (int _w, int _h)
{
  w = _w;
  h = _h;
}

void
Cmesh_orientation_edges2::apply_orientation (void)
{
  if (!model3d_half_edge) return;

  model3d_half_edge->translate (-center[0], -center[1], -center[2]);
  model3d_half_edge->transform (mrot);
}

void
Cmesh_orientation_edges2::compute_orientation (float t)
{
	/*
  if (!model3d_half_edge) return;

  float *m;
  model3d_half_edge->orientation_length_edges (t, &accumulator, &accumulator_int, w, h, &m, &iphi_max, &itheta_max);
  mrot[0] = m[0]; mrot[1] = m[1]; mrot[2] = m[2];
  mrot[3] = m[3]; mrot[4] = m[4]; mrot[5] = m[5];
  mrot[6] = m[6]; mrot[7] = m[7]; mrot[8] = m[8];

  find_orientation (1);

  // looking for the maximum
  int i;
  float max = 0.0;
  for (i=0; i<w*h; i++)
    if (accumulator[i] > max)
      max = accumulator[i];
  if (max == 0.0) return;
  
  // output the accumulator
  for (i=0; i<w*h; i++)
    accumulator_int[i] = (int)(255.0 - 255.0*accumulator[i]/max);
	*/
}

void
Cmesh_orientation_edges2::compute_orientation2 (void)
{
  int i,n;
  assert (model);
  if (!model) return;

  float x,y,z;

  if (accumulator) free (accumulator);
  accumulator = (float*)malloc(w*h*sizeof(float));
  assert (accumulator);
  /* init the accumulator */
  for (i=0; i<w*h; i++) accumulator[i] = 0.0;

  if (accumulator_int) free (accumulator_int);
  accumulator_int = (int*)malloc(w*h*sizeof(int));
  assert (accumulator_int);

  /* fill the accumulator */
  n = model->m_nFaces;
  float *vertices = model->m_pVertices;
  Face **faces = model->m_pFaces;

  int indices[3];
  for (i=0; i<n; i++)
  {
	  indices[0] = faces[i]->GetVertex(0);
	  indices[1] = faces[i]->GetVertex(1);
	  indices[2] = faces[i]->GetVertex(2);
    
    Vector3d v;
    float r;
    int phi_pos, theta_pos;
    for (int k=0; k<3; k++)
      {
		v.x = vertices[3*indices[k]]-vertices[3*indices[(k+1)%3]];
		v.y = vertices[3*indices[k]+1]-vertices[3*indices[(k+1)%3]+1];
		v.z = vertices[3*indices[k]+2]-vertices[3*indices[(k+1)%3]+2];
		r = v.getLength ();
		v.Normalize ();
	if (v.x < 0.0)
	  {
	    x = -v.x; y = -v.y; z = -v.z;
	  }
	else
	  {
	    x = v.x; y = v.y; z = v.z;
	  }
	if (x == 0.0)
	  phi = -3.14159/2.0;
	else
	  phi = atan (y/x);
	
	phi_pos = (int)(w*phi/3.14159 + w/2.0);
	theta = acos (z);
	theta_pos = (int) (h*theta / 3.14159);
	if (theta_pos == h) theta_pos = 0;
	if (phi_pos == w) phi_pos = 0;
	accumulator[theta_pos*h+phi_pos] += r;
      }
  }
  
  find_orientation (1);

  /* looking for the maximum */
  float max = 0;
  for (i=0; i<w*h; i++)
    if (accumulator[i] > max)
      max = accumulator[i];
  if (max == 0.0) return;

  /* output the accumulator */
  for (i=0; i<w*h; i++)
    accumulator_int[i] = (int)(255.0 - 255.0*accumulator[i]/max);
}

void
Cmesh_orientation_edges2::find_orientation (int id)
{
  int i,j,k,l;
  switch (id)
    {
    case 0: // max
      {
	float max = 0;
	for (i=0; i<w; i++)
	  for (j=0; j<h; j++)
	    if (accumulator[w*j+i] > max)
	      {
		iphi_max   = i;
		itheta_max = j;
		max = accumulator[w*j+i];
	      }
	printf ("max -> %d %d\n", itheta_max, iphi_max);
      }
      break;
      
    case 1: //projection
      {
	/* look at the maximum of each projection */
	float *projection_phi   = (float*)malloc(w*sizeof(float));
	for (i=0; i<w; i++) projection_phi[i] = 0;
	for (i=0; i<w; i++)
	  for (j=0; j<h; j++)
	    projection_phi[i] += accumulator[w*j+i];
	iphi_max = 0;
	for (i=1; i<w; i++)
	  if (projection_phi[i] > projection_phi[iphi_max])
	    iphi_max = i;
	
	float *projection_theta = (float*)malloc(h*sizeof(float));
	for (i=0; i<h; i++) projection_theta[i] = 0;
	for (j=0; j<h; j++)
	  for (i=0; i<w; i++)
	    projection_theta[j] += accumulator[w*j+i];
	itheta_max = 0;
	for (i=1; i<h; i++)
	  if (projection_theta[i] > projection_theta[itheta_max])
	    itheta_max = i;
	printf ("projection -> %d %d\n", itheta_max, iphi_max);
      }
      break;
      
    case 2: // max neighborough
      {
	/* look at the maximum in a small neighborough */
	float max_global = 0;
	float max_local;
	for (i=0; i<w; i++)
	  for (j=0; j<h; j++)
	    {
	      max_local = 0;
	      for (k=-5; k<6; k++)
		for (l=-5; l<6; l++)
		  {
		    int offset = (w*(j+k)+(i+l)+w*h)%(w*h);
		    max_local += accumulator[offset];
		  }
	      if (max_local > max_global)
		{
		  max_global = max_local;
		  iphi_max = i;
		  itheta_max = j;
		}
	    }
	printf ("max neighborough -> %d %d\n", itheta_max, iphi_max);
      }
      break;
    case 3: // mean shift
      {
	/* mean shift to have a better estimation */
	int n_iterations = 5;
	int r = 5;
	for (i=0; i<n_iterations; i++)
	  {
	    float phi_acc   = 0;
	    float theta_acc = 0;
	    float n_points  = 0;
	    for (j=itheta_max-r; j<=itheta_max+r; j++)
	      for (k=iphi_max-r; k<=iphi_max+r; k++)
		{
		  if ((float)sqrt ((float)((iphi_max-k)*(iphi_max-k) + (itheta_max-j)*(itheta_max-j))) > r)
		    continue;
		  phi_acc   += accumulator[w*((j+h)%h)+(k+w)%w] * k;
		  theta_acc += accumulator[w*((j+h)%h)+(k+w)%w] * j;
		  n_points  += accumulator[w*((j+h)%h)+(k+w)%w];
		}
	    iphi_max   = (int)(phi_acc / n_points);
	    itheta_max = (int)(theta_acc / n_points);
	    //printf ("%d %d\n", itheta_max, iphi_max);
	  }
	printf ("mean shift -> %d %d\n", itheta_max, iphi_max);
      }
      break;
    default:
      break;
    }
  //iphimax = index%w;
  //ithetamax = (int)(index/w);
  
  /* compute the quaternion of rotation from phi and theta */
  phi = (3.14159 * iphi_max / w - 3.14159 / 2.0);
  theta = ((3.14159 * itheta_max) / h);
  //printf ("%f %f\n", 180.0*phi/3.14159, 180.0*theta/3.14159);

  finalize_orientation ();
}

void
Cmesh_orientation_edges2::finalize_orientation (void)
{
  int i;

  Vector3d ox (1.0, 0.0, 0.0);
  Vector3d dir (sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));
  //printf ("principal axis: ");
  //v3d_dump (dir);

  Vector3 axis;
  axis.x = dir.y*ox.z - dir.z*ox.y;
  axis.y = dir.z*ox.x - dir.x*ox.z;
  axis.z = dir.x*ox.y - dir.y*ox.x;
  axis.Normalize ();
  float teta = acos(ox.x*dir.x + ox.y*dir.y + ox.z*dir.z);
  Quaternion q (axis, teta);

  float *m9_tmp1  = (float*)malloc(9*sizeof(float));
  float *m16 = (float*)malloc(16*sizeof(float));
  assert (m9_tmp1 && m16);
  q.get_matrix_rotation (m16);
  m9_tmp1[0] = m16[0];   m9_tmp1[1] = m16[1];   m9_tmp1[2] = m16[2];
  m9_tmp1[3] = m16[4];   m9_tmp1[4] = m16[5];   m9_tmp1[5] = m16[6];
  m9_tmp1[6] = m16[8];   m9_tmp1[7] = m16[9];   m9_tmp1[8] = m16[10];

  if (m9_tmp1[0] < 0.0)
    {
      m9_tmp1[0] *= -1.0;
      m9_tmp1[1] *= -1.0;
      m9_tmp1[2] *= -1.0;
    }
  if (m9_tmp1[4] < 0.0)
    {
      m9_tmp1[3] *= -1.0;
      m9_tmp1[4] *= -1.0;
      m9_tmp1[5] *= -1.0;
    }
  if (m9_tmp1[8] < 0.0)
    {
      m9_tmp1[6] *= -1.0;
      m9_tmp1[7] *= -1.0;
      m9_tmp1[8] *= -1.0;
    }

  /*
   * rotate the vertices, project them and compute PCA for the final rotation
   */
  int n_vertices;
  float *v_orig;
  if (model)
    {
      n_vertices = model->m_nVertices;
      v_orig = model->m_pVertices;
    }
  if (model3d_half_edge)
    {
      n_vertices = model3d_half_edge->m_nVertices;
      v_orig = model3d_half_edge->m_pVertices;
    }
  
  /* create a array with all the rotated and projected vertices */
  float *v = new float[2*n_vertices];
  assert (v);
  for (i=0; i<n_vertices; i++)
    {
      Vector3 tmp (v_orig[3*i], v_orig[3*i+1], v_orig[3*i+2]);
      q.rotate (tmp, tmp);
      v[2*i]   = tmp.y;
      v[2*i+1] = tmp.z;
    }

  /* compute the center */
  float yc, zc;
  yc = zc = 0.0;
  for (i=0; i<n_vertices; i++)
    {
      yc += v[2*i];
      zc += v[2*i+1];
    }
  yc /= n_vertices;
  zc /= n_vertices;

  // compute the PCA
  double y, z;
  double yy, zz, yz;
  yy = zz = yz = 0.0;
  for (i=0; i<n_vertices; i++)
    {
      y = v[2*i] - yc;
      z = v[2*i+1] - zc;
      yy += y*y;
      zz += z*z;
      yz += y*z;
    }

  Matrix2 m2 (yy, yz, yz, zz);
  Vector2 ev1, ev2, evalues;
  m2.SolveEigensystem (ev1, ev2, evalues);

  float *m9_tmp2 = (float*)malloc(9*sizeof(float));
  assert (m9_tmp2);

  m9_tmp2[0] = 1.0;
  m9_tmp2[1] = m9_tmp2[2] = 0.0;

  m9_tmp2[3] = 0.0;
  m9_tmp2[4] = ev1.x;
  m9_tmp2[5] = ev1.y;
  if (m9_tmp2[4] < 0.0)
    {
      m9_tmp2[4] *= -1.0;
      m9_tmp2[5] *= -1.0;
    }

  m9_tmp2[6] = 0.0;
  m9_tmp2[7] = ev2.x;
  m9_tmp2[8] = ev2.y;
  if (m9_tmp2[8] < 0.0)
    {
      m9_tmp2[7] *= -1.0;
      m9_tmp2[8] *= -1.0;
    }

  /* compose the final matrix of rotation */
  float *m9_final = (float*)malloc(9*sizeof(float));
  assert (m9_final);
  m9_final = (float*)memset(m9_final, 0, 9*sizeof(float));

  mrot[0] = m9_tmp2[0]*m9_tmp1[0] + m9_tmp2[1]*m9_tmp1[3] + m9_tmp2[2]*m9_tmp1[6];
  mrot[1] = m9_tmp2[0]*m9_tmp1[1] + m9_tmp2[1]*m9_tmp1[4] + m9_tmp2[2]*m9_tmp1[7];
  mrot[2] = m9_tmp2[0]*m9_tmp1[2] + m9_tmp2[1]*m9_tmp1[5] + m9_tmp2[2]*m9_tmp1[8];
  mrot[3] = m9_tmp2[3]*m9_tmp1[0] + m9_tmp2[4]*m9_tmp1[3] + m9_tmp2[5]*m9_tmp1[6];
  mrot[4] = m9_tmp2[3]*m9_tmp1[1] + m9_tmp2[4]*m9_tmp1[4] + m9_tmp2[5]*m9_tmp1[7];
  mrot[5] = m9_tmp2[3]*m9_tmp1[2] + m9_tmp2[4]*m9_tmp1[5] + m9_tmp2[5]*m9_tmp1[8];
  mrot[6] = m9_tmp2[6]*m9_tmp1[0] + m9_tmp2[7]*m9_tmp1[3] + m9_tmp2[8]*m9_tmp1[6];
  mrot[7] = m9_tmp2[6]*m9_tmp1[1] + m9_tmp2[7]*m9_tmp1[4] + m9_tmp2[8]*m9_tmp1[7];
  mrot[8] = m9_tmp2[6]*m9_tmp1[2] + m9_tmp2[7]*m9_tmp1[5] + m9_tmp2[8]*m9_tmp1[8];

  for (i=0; i<9; i++)
    ;//mrot[i] = m9_tmp1[i];
}

void
Cmesh_orientation_edges2::output_model (char *filename)
{
  /*
  if (he)
    he->export_obj (filename);
  */
}

void
Cmesh_orientation_edges2::output_accumulator (char *filename)
{
  /* output */
  FILE *ptr = fopen (filename, "w");
  fprintf (ptr, "P2\n%d %d\n255\n", w, h);
  for (int i=0; i<w*h; i++)
    fprintf (ptr, "%d\n", accumulator_int[i]);
  fclose (ptr);
}
