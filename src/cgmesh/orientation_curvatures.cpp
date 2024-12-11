#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "orientation_curvatures.h"
#include "regions_vertices.h"
#include "../cgmath/cgmath.h"

Cmesh_orientation_curvatures::Cmesh_orientation_curvatures (Mesh_half_edge *mesh, int _w, int _h)
  : Cmesh_orientation(mesh)
{
  w = _w;
  h = _h;
  accumulator     = NULL;
  accumulator_int = NULL;

  /*
  // get the tensor
  MeshAlgoTensorEvaluator *pTensorEvaluator = new MeshAlgoTensorEvaluator();
  pTensorEvaluator->Init (model);
  pTensorEvaluator->Evaluate (TENSOR_TAUBIN);
  */

  //mrot = (float*)malloc(16*sizeof(float));
  for (int i=0; i<9; i++) mrot[i] = 0.0;
  mrot[0] = mrot[4] = mrot[8] = 1.0;
}

void
Cmesh_orientation_curvatures::set_size_accumulator (int _w, int _h)
{
  w = _w;
  h = _h;
}

/**
*
* t1 : threshold
* t2 : epsilon
*
*/
void
Cmesh_orientation_curvatures::compute_orientation (float t1, float t2)
{
  int i,n;
  if (model3d_half_edge == NULL) return;

  printf ("t1 = %f\t t2 = %f\n", t1, t2);
  
  // select the vertices with high curvatures
  Cregions_vertices *model_region_vertices = new Cregions_vertices (model3d_half_edge);

  Vector3 *directions;
  model_region_vertices->get_directions_lines (&directions, &n, t1, t2);

  delete model_region_vertices;

  /* get the principal directions */
  //model_region_vertices->init_lines (t1, t2);
  //model_region_vertices->get_principal_direction_max_from_selected_regions (&directions, &n);
  if (n == 0)
    {
      printf ("no vertex selected\n");
      return;
    }
  printf ("%d vertices selected\n", n);
  /*
  FILE *ptr1 = fopen ("directions_max.txt", "w");
  fprintf (ptr1, "%d\n", n);
  for (i=0; i<n; i++)
    fprintf (ptr1, "%f %f %f\n", directions[i][0], directions[i][1], directions[i][2]);
  fclose (ptr1);
  */
  // re-oriente the directions
  for (i=0; i<n; i++)
    if (directions[i].x < 0)
      {
		directions[i].x *= -1.0;
		directions[i].y *= -1.0;
		directions[i].z *= -1.0;
      }

  // init the accumulator
  float x,y,z;
  accumulator = (int*)malloc(w*h*sizeof(int));
  accumulator = (int*)memset(accumulator, 0, w*h*sizeof(int));

  accumulator_int = (int*)malloc(w*h*sizeof(int));
  accumulator_int = (int*)memset(accumulator_int, 0, w*h*sizeof(int));

  // fill the accumulator
  for (i=0; i<n; i++)
    {
      x = directions[i].x;
      y = directions[i].y;
      z = directions[i].z;
      if (x==0.0) continue;
      float phi = atan (y/x);
      int phi_pos = (int)(w*phi/3.14159 + w/2.0);
      float theta = acos (z);
      int theta_pos = (int) (h*theta / 3.14159);
      //printf ("%d %d\t - > %f %f %f\n", phi_pos, theta_pos, x, y, z);
      accumulator[theta_pos*h+phi_pos]++;
    }

  find_orientation (1);

  // looking for the maximum
  int max = 0;
  for (i=0; i<w*h; i++)
    if (accumulator[i] > max)
      max = accumulator[i];

  // output the accumulator
  for (i=0; i<w*h; i++)
    accumulator_int[i] = 255 - 255*accumulator[i]/max;
}

void
Cmesh_orientation_curvatures::find_orientation (int id)
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
	int *projection_phi   = (int*)malloc(w*sizeof(int));
	for (i=0; i<w; i++) projection_phi[i] = 0;
	for (i=0; i<w; i++)
	  for (j=0; j<h; j++)
	    projection_phi[i] += accumulator[w*j+i];
	iphi_max = 0;
	for (i=1; i<w; i++)
	  if (projection_phi[i] > projection_phi[iphi_max])
	    iphi_max = i;
	
	int *projection_theta = (int*)malloc(h*sizeof(int));
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
	int max_global = 0;;
	int max_local;
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
	    int phi_acc   = 0;
	    int theta_acc = 0;
	    int n_points  = 0;
	    for (j=itheta_max-r; j<=itheta_max+r; j++)
	      for (k=iphi_max-r; k<=iphi_max+r; k++)
		{
		  if (sqrt ((float)((iphi_max-k)*(iphi_max-k) + (itheta_max-j)*(itheta_max-j))) > r)
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
Cmesh_orientation_curvatures::finalize_orientation (void)
{
  int i;

  Vector3d ox (1.0, 0.0, 0.0);
  Vector3d dir (sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta));
  printf ("principal axis: ");

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
  int n_vertices = model3d_half_edge->m_nVertices;
  float *v_orig = model3d_half_edge->m_pVertices;
  
  /* create a array with all the rotated and projected vertices */
  //float *v = (float*)malloc(2*n_vertices*sizeof(float));
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

  /* compute the PCA */
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
Cmesh_orientation_curvatures::output_accumulator (char *filename)
{
  FILE *ptr = fopen (filename, "w");
  fprintf (ptr, "P2\n%d %d\n255\n", w, h);
  for (int i=0; i<w*h; i++)
    fprintf (ptr, "%d\n", accumulator_int[i]);
  fclose (ptr);
}
