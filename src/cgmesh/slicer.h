#pragma once
// Ce que ce fichier utilise, et non le parapluie.
//
// Il incluait `cgmesh.h`, lequel l'inclut en retour : un CYCLE, que `#pragma
// once` rend silencieux mais qui impose a tout consommateur de ce fichier les 55
// inclusions du parapluie -- zlib, l'audio, l'ONNX comprises.
#include "mesh_half_edge.h"
#include "polygon2.h"

// ============================================================================
//  PRECONDITION -- variete FERMEE, et ce n'est pas negociable
// ============================================================================
//
// Le decoupage passe entierement par la demi-arete : `Cmodel3d_half_edge_clipper`
// suit `m_pair` de face en face pour fermer chaque contour. Cette dependance
// n'est pas une commodite d'implementation, elle DEFINIT le domaine d'emploi.
//
//   - BORD (variete a bord : plaque, tube sans capuchons, maillage troue).
//     `m_pair` vaut -1 des qu'on atteint un rebord ; la marche s'arrete la et le
//     contour est rendu TRONQUE, sans que rien ne le signale. Un tube ouvert
//     coupe perpendiculairement a son axe sort juste -- la section ne touche
//     aucun rebord ; le meme tube coupe parallelement a son axe sort faux.
//
//   - NON VARIETE (arete portee par plus de deux faces). `pair_half_edges`
//     (half_edge.cpp) apparie DEUX A DEUX sur une cle non orientee : la
//     troisieme demi-arete reste sans opposee, et la marche s'y interrompt.
//     ⚠ `Mesh_half_edge::is_manifold()` ne le detecte PAS -- il examine la
//     topologie autour des SOMMETS, pas la valence des aretes.
//
//   - PLAN TANGENT / COPLANAIRE a une face, meme sur un maillage ferme. Toutes
//     les distances y valent zero, tous les produits sont <= 0, et chaque face
//     coplanaire ouvre son propre contour. La PREMIERE tranche tombe exactement
//     sur le minimum de la boite englobante : sur un cylindre capuchonne, elle
//     rend un contour par triangle du capuchon, d'aire NaN. Decaler le pas d'un
//     demi-cran, ou ignorer la tranche 0, est a la charge de l'appelant.
//
// Les trois cas sont fixes par des tests de caracterisation dans
// test/tu_cgmesh_slicer.cpp. Aucun n'est verifie a l'execution : le slicer ne
// PLANTE pas sur une entree hors domaine, il rend un resultat faux.
//
// ============================================================================

#define ALONGOX 0
#define ALONGOY 1
#define ALONGOZ 2
// Il n'y a pas d'ALONGOY : `scan_model` ne connait que Ox et Oz, et toute autre
// valeur tombe dans son `default:` vide -- zero tranche, sans diagnostic.

class Cmodel3d_half_edge_sliced
{
 public:
  Cmodel3d_half_edge_sliced (Mesh_half_edge *model, int dir, float _step_slice);
  ~Cmodel3d_half_edge_sliced ();

  // La classe POSSEDE desormais ses tranches -- le destructeur les libere. Une
  // copie par defaut dupliquerait les pointeurs bruts et menerait a une double
  // liberation. Personne ne copie de slicer aujourd'hui ; le compilateur le
  // dira si cela change, plutot que le tas a l'execution.
  Cmodel3d_half_edge_sliced (const Cmodel3d_half_edge_sliced &) = delete;
  Cmodel3d_half_edge_sliced &operator= (const Cmodel3d_half_edge_sliced &) = delete;

  Mesh_half_edge* get_model (void) { return model; };

  void get_areas (float **areas, int *size);
  void get_slice (int index, Polygon2 ***slice, int *nc);
  int  get_n_slices (void) { return n_slices; };

  float get_step_slice (void) { return step_slice; };
  float get_zmin (void) { return zmin; };
  float get_xmin (void) { return xmin; };

  void look_at_symmetry (void);

  void dump (char *prefix);
  
 private:
  int direction;

  void scan_model_along_Ox (void);
  void scan_model_along_Oz (void);
  void scan_model (int dir);

  Mesh_half_edge *model;

  float zmin, xmin;

  // slices
  int n_slices;
  Polygon2 ***slices;
  int *n_contours;
  float step_slice; // distance between two consecutive slices
};
