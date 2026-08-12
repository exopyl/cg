#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "orientation_curvatures.h"
#include "orientation_peak.h"
#include "regions_vertices.h"
#include "../cgmath/cgmath.h"

Cmesh_orientation_curvatures::Cmesh_orientation_curvatures (Mesh_half_edge *mesh, int _w, int _h)
  : Cmesh_orientation(mesh)
{
  w = _w;
  h = _h;
  iphi_max   = 0;
  itheta_max = 0;
  phi        = 0.0f;
  theta      = 0.0f;

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
  int i;
  if (model3d_half_edge == nullptr) return;

  printf ("t1 = %f\t t2 = %f\n", t1, t2);

  // select the vertices with high curvatures
  // unique_ptr : le delete manuel etait correct sur ce chemin, mais il devenait
  // une fuite des qu'une sortie s'inserait entre les deux -- ce que fait
  // desormais le garde ci-dessous.
  std::unique_ptr<Cregions_vertices> model_region_vertices
    (new Cregions_vertices (model3d_half_edge));

  // `directions` et `n` etaient declares SANS valeur, et get_directions_lines a
  // son corps entierement commente : elle ne les ecrit jamais. Le `if (n == 0)`
  // qui suit lisait donc une valeur indeterminee, et la boucle pouvait
  // dereferencer un pointeur non initialise. Les initialiser rend la sortie
  // deterministe -- « no vertex selected », ce que la fonction vide implique.
  Vector3 *directions = nullptr;
  int n = 0;
  model_region_vertices->get_directions_lines (&directions, &n, t1, t2);

  model_region_vertices.reset();

  /* get the principal directions */
  //model_region_vertices->init_lines (t1, t2);
  //model_region_vertices->get_principal_direction_max_from_selected_regions (&directions, &n);
  if (n == 0 || directions == nullptr)
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
  // assign() remplace le couple malloc + memset, et libere l'allocation
  // precedente : les deux malloc d'origine fuyaient a chaque appel.
  float x,y,z;
  if (w <= 0 || h <= 0) return;
  accumulator.assign ((size_t)w*h, 0);
  accumulator_int.assign ((size_t)w*h, 0);

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
      // Le pas d'indexation est laisse tel quel -- il vaut `h` la ou tout le
      // reste du fichier indexe en `w*j+i`, ce qui est faux des que w != h, mais
      // le corriger changerait ce que l'algorithme calcule. En revanche
      // phi_pos et theta_pos ne sont pas bornes -- (int)(w*phi/PI + w/2) atteint
      // w -- donc l'increment sortait du tableau. On ecarte ces echantillons
      // plutot que de les replier : toute ecriture jusqu'ici valide est
      // conservee a l'identique, seules celles qui corrompaient la memoire
      // disparaissent.
      const long index = (long)theta_pos*h + phi_pos;
      if (index < 0 || index >= (long)w*h)
        continue;
      accumulator[index]++;
    }

  find_orientation (1);

  // looking for the maximum
  int max = 0;
  for (i=0; i<w*h; i++)
    if (accumulator[i] > max)
      max = accumulator[i];

  // Accumulateur reste vide : la division entiere par max plantait. Les classes
  // soeurs (orientation_edges) portent deja cette garde.
  if (max == 0) return;

  // output the accumulator
  for (i=0; i<w*h; i++)
    accumulator_int[i] = 255 - 255*accumulator[i]/max;
}

void
Cmesh_orientation_curvatures::find_orientation (int id)
{
  // Le corps -- une centaine de lignes dupliquees a l'identique dans
  // orientation_edges.cpp et orientation_edges2.cpp -- vit desormais dans
  // orientation_peak.h. Le mean shift (id == 3) raffine l'estimation courante,
  // d'ou le passage du couple (iphi_max, itheta_max) en entree.
  const orientation_peak::Peak peak =
    orientation_peak::find (accumulator, w, h, id, { iphi_max, itheta_max });
  iphi_max   = peak.iphi;
  itheta_max = peak.itheta;

  /* compute the quaternion of rotation from phi and theta */
  phi = (3.14159 * iphi_max / w - 3.14159 / 2.0);
  theta = ((3.14159 * itheta_max) / h);

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

  // Tailles connues a la compilation : des tableaux locaux suffisent, et il n'y
  // a plus rien a liberer -- donc plus de chemin de sortie qui puisse l'oublier.
  float m9_tmp1[9];
  float m16[16];
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
  // find_orientation() est publique et mene ici : sans ce garde, un appel direct
  // sans modele dereferencait un pointeur nul, et n_vertices == 0 divisait par
  // zero au calcul du centre.
  if (model3d_half_edge == nullptr)
    return;
  int n_vertices = model3d_half_edge->m_pMesh->m_nVertices;
  float *v_orig = model3d_half_edge->m_pMesh->m_pVertices.data();
  if (n_vertices <= 0 || v_orig == nullptr)
    return;
  
  /* create a array with all the rotated and projected vertices */
  //float *v = (float*)malloc(2*n_vertices*sizeof(float));
  std::vector<float> v (2*(size_t)n_vertices);
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

  float m9_tmp2[9];

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
  // m9_final etait alloue, mis a zero, puis libere sans avoir jamais ete relu.

  mrot[0] = m9_tmp2[0]*m9_tmp1[0] + m9_tmp2[1]*m9_tmp1[3] + m9_tmp2[2]*m9_tmp1[6];
  mrot[1] = m9_tmp2[0]*m9_tmp1[1] + m9_tmp2[1]*m9_tmp1[4] + m9_tmp2[2]*m9_tmp1[7];
  mrot[2] = m9_tmp2[0]*m9_tmp1[2] + m9_tmp2[1]*m9_tmp1[5] + m9_tmp2[2]*m9_tmp1[8];
  mrot[3] = m9_tmp2[3]*m9_tmp1[0] + m9_tmp2[4]*m9_tmp1[3] + m9_tmp2[5]*m9_tmp1[6];
  mrot[4] = m9_tmp2[3]*m9_tmp1[1] + m9_tmp2[4]*m9_tmp1[4] + m9_tmp2[5]*m9_tmp1[7];
  mrot[5] = m9_tmp2[3]*m9_tmp1[2] + m9_tmp2[4]*m9_tmp1[5] + m9_tmp2[5]*m9_tmp1[8];
  mrot[6] = m9_tmp2[6]*m9_tmp1[0] + m9_tmp2[7]*m9_tmp1[3] + m9_tmp2[8]*m9_tmp1[6];
  mrot[7] = m9_tmp2[6]*m9_tmp1[1] + m9_tmp2[7]*m9_tmp1[4] + m9_tmp2[8]*m9_tmp1[7];
  mrot[8] = m9_tmp2[6]*m9_tmp1[2] + m9_tmp2[7]*m9_tmp1[5] + m9_tmp2[8]*m9_tmp1[8];

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
