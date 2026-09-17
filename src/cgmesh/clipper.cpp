#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "clipper.h"

Cmodel3d_half_edge_clipper::Cmodel3d_half_edge_clipper (Mesh_half_edge *_mesh)
{
  assert (_mesh);
  model = _mesh;
  
  n = Vector3d (0.0, 0.0, 1.0);
  d = 0.0;

  distances = (float*)malloc(model->m_pMesh->GetNVertices ()*sizeof(float));
  assert (distances);
}

Cmodel3d_half_edge_clipper::~Cmodel3d_half_edge_clipper ()
{
  if (distances) free (distances);
}

void
Cmodel3d_half_edge_clipper::set_plane (Vector3d pt, Vector3d _n)
{
  _n.Normalize ();
  n = _n;
  d = - (n * pt);
}

// Tampons EXTENSIBLES. Les deux capacites etaient FIXES -- 100 contours par
// plan, et 2048 FLOATS par contour, soit 682 points -- et AUCUNE ecriture ne les
// verifiait : un plan qui coupe plus de contours, ou un seul contour plus dense,
// ecrivait hors du tas. Un maillage de quelques dizaines de milliers de faces
// suffit a franchir la seconde.
//
// C'est le meme defaut, et le meme remede, que polygon2_tesselation.cpp
// (ensure_v / ensure_f) : doublement par realloc, et l'appelant abandonne
// proprement si la memoire manque, plutot que d'ecrire quand meme.
static int ensure_contours (int **n_vertices, float ***contours, int *cap, int need)
{
  if (need <= *cap) return 1;
  int cap2 = *cap ? *cap * 2 : 64;
  while (cap2 < need) cap2 *= 2;

  int *nv = (int*) realloc (*n_vertices, (size_t)cap2 * sizeof(int));
  if (!nv) return 0;
  *n_vertices = nv;                 // publie avant le second realloc : si celui-ci
  float **c = (float**) realloc (*contours, (size_t)cap2 * sizeof(float*));
  if (!c) return 0;                 // echoue, le bloc agrandi reste possede.
  *contours = c;

  for (int k = *cap; k < cap2; k++) { (*n_vertices)[k] = 0; (*contours)[k] = nullptr; }
  *cap = cap2;
  return 1;
}

// `need` est un nombre de FLOATS, pas de points -- trois par point.
static int ensure_points (float **pts, int *cap, int need)
{
  if (need <= *cap) return 1;
  int cap2 = *cap ? *cap * 2 : 1024;
  while (cap2 < need) cap2 *= 2;
  float *p = (float*) realloc (*pts, (size_t)cap2 * sizeof(float));
  if (!p) return 0;
  *pts = p;
  *cap = cap2;
  return 1;
}

//
// CONTRAT DE PROPRIETE, qui n'etait ecrit nulle part et n'etait pas tenu :
// `*n_intersections == 0` <=> `*n_vertices` et `*intersections` valent nullptr,
// et il n'y a RIEN a liberer. Sinon l'appelant possede les `*n_intersections`
// contours, le tableau qui les porte, et celui des comptes.
//
void
Cmodel3d_half_edge_clipper::get_intersections (int *n_intersections, int **n_vertices, float ***intersections)
{
  int i, iwalk, nv = model->m_pMesh->GetNVertices (), nf = model->m_pMesh->GetNFaces ();
  const float *v = model->m_pMesh->GetVertices ().data();

  *n_intersections = 0;
  *n_vertices      = nullptr;
  *intersections   = nullptr;

  int contours_cap = 0;
  int current_n_intersections = 0;
  int *current_n_vertices = nullptr;
  float **current_intersections = nullptr;

  // Tampon du contour EN COURS, cede au tableau des qu'il est complet ; le
  // suivant repart d'un tampon neuf.
  int pts_cap = 0;
  float *pts = nullptr;

  /* compute the distances betwen the vertices and the plane */
  Vector3d v_walk;
  for (i=0; i<nv; i++)
    {
      v_walk.Set (v[3*i], v[3*i+1], v[3*i+2]);
      distances[i] = (v_walk * n) + d;
    }

  /* check if there is an intersection */
  int negative = 0;
  int positive = 0;
  for (i=0; i<nv; i++)
    {
      if (distances[i] < 0.00001) negative++;
      if (distances[i] > 0.00001) positive++;
      if (fabs(distances[i]) < 0.00001) distances[i] = 0.0;
    }
  if (negative == 0 || positive == 0)
    {
      // Les deux affectations etaient faites sur les parametres LOCAUX, pas sur
      // les sorties -- donc sans effet -- et les tampons deja alloues fuyaient.
      // Ils ne sont plus alloues a ce stade ; les sorties sont deja a nullptr.
      return;
    }

  /* check all the edges to find the intersections */
  int *visited_faces = (int*)malloc(nf*sizeof(int));
  assert (visited_faces);
  for (i=0; i<nf; i++) visited_faces[i] = 0;

  Che_mesh *cheMesh = model->GetCheMesh();
  int e, e_walk;
  int a,b,c;
  for (i=0; i<nf; i++)
    {
      if (visited_faces[i]) continue;
      visited_faces[i] = 1;

      /* is there an intersection between the plane and the current face ? */
      auto fc = model->m_pMesh->FaceAt (i);
      if (distances[fc->GetVertex(0)] * distances[fc->GetVertex(1)] > 0.0 &&
	  distances[fc->GetVertex(1)] * distances[fc->GetVertex(2)] > 0.0)
	continue; // no intersection

      iwalk = 0;

      /* look for the first edges of the intersection */
      e = cheMesh->m_edges_face[i];
      a = cheMesh->edge(e).m_v_begin;
      b = cheMesh->edge(e).m_v_end;
      c = cheMesh->edge(cheMesh->edge(e).m_he_next).m_v_end;

      if (distances[a] * distances[b] <= 0)
	{
	  if (distances[b] * distances[c] <= 0)
	    e_walk = cheMesh->edge(cheMesh->edge(e).m_he_next).m_pair;
	  else
	    e_walk = cheMesh->edge(cheMesh->edge(cheMesh->edge(e).m_he_next).m_he_next).m_pair;
	}
      else
	{
	  e = cheMesh->edge(e).m_he_next;
	  e_walk = cheMesh->edge(cheMesh->edge(e).m_he_next).m_pair;
	}
      if (e_walk < 0) continue;   // arete de bord : pas de face opposee ou marcher

      if (!ensure_points (&pts, &pts_cap, 3*(iwalk+1))) goto out_of_memory;
      get_vertex_intersection (cheMesh->edge(e).m_v_begin, cheMesh->edge(e).m_v_end, v_walk);
      pts[3*iwalk]   = v_walk.x;
      pts[3*iwalk+1] = v_walk.y;
      pts[3*iwalk+2] = v_walk.z;
      iwalk++;

      /* look for the complete intersection */
      do
	{
	  visited_faces[cheMesh->edge(e_walk).m_face] = 1;

	  /* next vertex in the intersection */
	  if (!ensure_points (&pts, &pts_cap, 3*(iwalk+1))) goto out_of_memory;
	  get_vertex_intersection (cheMesh->edge(e_walk).m_v_begin, cheMesh->edge(e_walk).m_v_end, v_walk);
	  pts[3*iwalk]   = v_walk.x;
	  pts[3*iwalk+1] = v_walk.y;
	  pts[3*iwalk+2] = v_walk.z;
	  iwalk++;

	  /* go to the next intersected edge */
	  int next1 = cheMesh->edge(e_walk).m_he_next;
	  a = cheMesh->edge(next1).m_v_begin;
	  b = cheMesh->edge(next1).m_v_end;
	  c = cheMesh->edge(cheMesh->edge(next1).m_he_next).m_v_end;
	  if (distances[a] * distances[b] <= 0)
	    e_walk = next1;
	  else
	    e_walk = cheMesh->edge(next1).m_he_next;
	  assert (e_walk >= 0);
	  e_walk = cheMesh->edge(e_walk).m_pair;
	} while (e_walk >= 0 && e_walk != e && iwalk <= cheMesh->m_ne);

      // La borne ci-dessus n'est pas arbitraire : la marche consomme une
      // demi-arete par point, donc un contour ne peut pas en compter plus qu'il
      // n'en existe. Sans elle, une topologie degeneree qui ne revient jamais sur
      // `e` bouclait -- ce qui, tampon fixe, ecrasait le tas, et tampon
      // extensible, epuiserait la memoire.

      /* publier le contour : le tampon lui est CEDE, le suivant repart neuf */
      if (!ensure_contours (&current_n_vertices, &current_intersections,
                           &contours_cap, current_n_intersections+1))
        goto out_of_memory;
      current_intersections[current_n_intersections] = pts;
      current_n_vertices[current_n_intersections] = iwalk;
      current_n_intersections++;
      pts = nullptr;
      pts_cap = 0;
    }

  free (visited_faces);
  free (pts);   // filet : `pts` est remis a nul a chaque publication, et la sortie
                // par `continue` precede toute allocation -- mais cela se DEMONTRE,
                // cela ne se lit pas, et un free de nul ne coute rien.

  if (current_n_intersections == 0)
    {
      // Contrat : zero contour, donc rien a liberer cote appelant.
      free (current_n_vertices);
      free (current_intersections);
      return;
    }

  *n_intersections = current_n_intersections;
  *n_vertices = current_n_vertices;
  *intersections = current_intersections;
  return;

 out_of_memory:
  // Rien de partiel n'est rendu : les sorties restent a zero/nullptr, posees en
  // tete de fonction. Une tranche manquante se voit ; une tranche a moitie
  // remplie passerait pour un resultat.
  free (visited_faces);
  free (pts);
  for (int k = 0; k < current_n_intersections; k++) free (current_intersections[k]);
  free (current_n_vertices);
  free (current_intersections);
}

void
Cmodel3d_half_edge_clipper::get_vertex_intersection (int i, int j, Vector3d &inter)
{
  const float *v = model->m_pMesh->GetVertices ().data();
  float t = distances[i] / (distances[i] - distances[j]);
  inter.Set ((1.0 - t) * v[3*i]   + t * v[3*j],
	      (1.0 - t) * v[3*i+1] + t * v[3*j+1],
	      (1.0 - t) * v[3*i+2] + t * v[3*j+2]);
}

