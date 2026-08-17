#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "orientation_edges.h"
#include "orientation_peak.h"

Cmesh_orientation_edges::Cmesh_orientation_edges (Mesh_half_edge *_mesh, int _w, int _h)
  : Cmesh_orientation(_mesh)
{
  w = _w;
  h = _h;
  mesh = nullptr;
  model3d_half_edge = _mesh;
}

Cmesh_orientation_edges::Cmesh_orientation_edges (Mesh *_mesh, int _w, int _h)
  : Cmesh_orientation(_mesh)
{
  w = _w;
  h = _h;
  mesh = _mesh;
  model3d_half_edge = nullptr;
}

void
Cmesh_orientation_edges::set_size_accumulator (int _w, int _h)
{
  w = _w;
  h = _h;
}

void
Cmesh_orientation_edges::apply_orientation (void)
{
  if (!model3d_half_edge) return;

  model3d_half_edge->m_pMesh->translate (-center[0], -center[1], -center[2]);
  model3d_half_edge->m_pMesh->transform (mrot);
}

void
Cmesh_orientation_edges::compute_orientation (float t)
{
  if (!model3d_half_edge) return;

  // Le calcul qui remplissait `m` est commente juste en dessous depuis
  // longtemps : `m` etait donc recopie dans mrot SANS avoir ete initialise.
  // L'initialiser a zero revient a laisser mrot tel que le memset le laisse.
  float m[9] = { 0 };
  memset (mrot, 0, 9*sizeof(float));
  //model3d_half_edge->orientation_edges (t, &accumulator, &accumulator_int, w, h, &m, &iphi_max, &itheta_max);
  mrot[0] = m[0]; mrot[1] = m[1]; mrot[2] = m[2];
  mrot[3] = m[3]; mrot[4] = m[4]; mrot[5] = m[5];
  mrot[6] = m[6]; mrot[7] = m[7]; mrot[8] = m[8];

  find_orientation (1);

  /* looking for the maximum */
  // Aucun appelant ne remplit l'accumulateur sur ce chemin (le calcul est
  // commente) : il est vide, et la boucle le parcourait quand meme.
  if ((int)accumulator.size() < w*h) return;

  int i;
  float max = 0.0;
  for (i=0; i<w*h; i++)
    if (accumulator[i] > max)
      max = accumulator[i];
  if (max == 0.0) return;
  
  /* output the accumulator */
  for (i=0; i<w*h; i++)
    accumulator_int[i] = (int)(255.0 - 255.0*accumulator[i]/max);
}

void
Cmesh_orientation_edges::compute_orientation2 (void)
{
  int i,n;
  assert (mesh);
  if (!mesh) return;

  float x,y,z;

  // assign() libere l'allocation precedente et initialise en une fois.
  if (w <= 0 || h <= 0) return;
  accumulator.assign ((size_t)w*h, 0.0f);
  accumulator_int.assign ((size_t)w*h, 0);

  /* fill the accumulator */
  n = mesh->GetNFaces ();
  const float *vertices = model3d_half_edge->m_pMesh->GetVertices ().data();

  int indices[3];
  for (i=0; i<n; i++)
  {
	  auto fc = model3d_half_edge->m_pMesh->FaceAt (i);
	  indices[0] = fc->GetVertex(0);
	  indices[1] = fc->GetVertex(1);
	  indices[2] = fc->GetVertex(2);
    
    float r;
    int phi_pos, theta_pos;
    for (int k=0; k<3; k++)
      {
		Vector3d v (vertices[3*indices[k]]-vertices[3*indices[(k+1)%3]],
					vertices[3*indices[k]+1]-vertices[3*indices[(k+1)%3]+1],
					vertices[3*indices[k]+2]-vertices[3*indices[(k+1)%3]+2]);
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
	// Pas d'indexation laisse tel quel (`h` la ou le reste indexe en `w*j+i`) :
	// le corriger changerait le resultat. Seules les ecritures qui sortaient du
	// tableau -- possibles des que w != h malgre les deux replis ci-dessus --
	// sont ecartees.
	{
	  const long index = (long)theta_pos*h + phi_pos;
	  if (index < 0 || index >= (long)w*h) continue;
	  accumulator[index] += r;
	}
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
Cmesh_orientation_edges::find_orientation (int id)
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
Cmesh_orientation_edges::finalize_orientation (void)
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
  // Les deux etaient declares SANS valeur et affectes seulement dans le if :
  // sans modele, `new float[2*n_vertices]` prenait une taille indeterminee.
  int n_vertices = 0;
  const float *v_orig  = nullptr;
  if (model3d_half_edge)
    {
      n_vertices = model3d_half_edge->m_pMesh->GetNVertices ();
      v_orig = model3d_half_edge->m_pMesh->GetVertices ().data();
    }
  if (n_vertices <= 0 || v_orig == nullptr)
    return;

  /* create a array with all the rotated and projected vertices */
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
Cmesh_orientation_edges::output_accumulator (char *filename)
{
  /* output */
  FILE *ptr = fopen (filename, "w");
  fprintf (ptr, "P2\n%d %d\n255\n", w, h);
  for (int i=0; i<w*h; i++)
    fprintf (ptr, "%d\n", accumulator_int[i]);
  fclose (ptr);
}
