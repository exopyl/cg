#pragma once
//
//  Lancer de rayon sur un maillage
//
// L'accelerateur n'appartient PAS au maillage : l'appelant le construit, le
// garde aussi longtemps qu'il veut, et le reconstruit quand la geometrie change.
// C'est le contrat de BVH (bvh.h) : « Built once from a Mesh, then queried
// read-only ». Interroger un maillage ne le mute jamais.
//
// ⚠ DUREE DE VIE. L'octree ne copie pas les positions du maillage : il ne retient
// que des indices de triangles. LE MAILLAGE DOIT DONC SURVIVRE A SON OCTREE, et
// c'est a l'appelant de le garantir. La reference est nue, il n'y a aucun
// comptage.
//
// ⚠ ET IL N'Y A AUCUNE DETECTION DE PEREMPTION ICI. Rien dans ce fichier ne sait
// qu'un octree ne correspond plus a son maillage : l'octree partitionne les
// positions telles qu'elles etaient a sa construction, et un appelant qui garde
// le sien apres une edition interroge une geometrie perimee. La derive est
// SILENCIEUSE -- un indice devenu hors bornes est simplement ignore par
// Mesh::GetVertex, donc l'erreur se lit en intersections fausses ou manquantes,
// jamais en plantage.
//
// `Mesh::GetRevision()` couvre desormais les ecritures qui comptent ici, dont
// l'orientation des faces (Mesh::FlipFaces, Mesh::FaceRef::Flip) -- les faces
// arriere etant eliminees, elle decide du resultat. Un detenteur d'octree peut
// donc s'en servir comme cle pour decider quand reconstruire ; c'est a lui de le
// faire, rien ne le fait pour lui.
//
// ⚠ NE JAMAIS CONSTRUIRE UN OCTREE DANS UNE REQUETE. Le detenteur le batit quand
// il adopte ou modifie sa geometrie, pas au premier rayon : une construction
// paresseuse fait d'une lecture une mutation, ce qui est precisement le defaut
// que cette separation supprime.
//
#include <memory>

#include "../cgmath/cgmath.h"

class Mesh;
class Octree;

// Construit l'octree de lancer de rayon pour `mesh` : 100 triangles par feuille,
// profondeur maximale 5, triangulation robuste (Mesh::GetTriangles(), qui passe
// par glutess pour les faces concaves).
//
// Le maillage n'est que LU : ni ecriture, ni increment de revision.
std::unique_ptr<Octree> BuildRaycastOctree (const Mesh &mesh);

// Lancer de rayon accelere. Renvoie 1 en cas d'intersection (la plus proche), 0
// sinon ; `_t` recoit le parametre le long du rayon, ou -1 sans intersection.
//
// `octree` doit avoir ete construit depuis `mesh` par BuildRaycastOctree(). Rien
// ne le verifie.
//
// ⚠ LES FACES ARRIERE SONT ELIMINEES : le test unitaire vient de
// Triangle::GetIntersectionWithRay (cgmath/geometry.cpp, `if (b >= 0.) return 0;`),
// donc un rayon qui aborde une face par derriere ne la voit pas. C'est voulu --
// cette primitive sert le rendu -- et c'est ce qui interdit de la confondre avec
// BVH, qui ne cule pas (bvh.h). L'orientation des faces decide du resultat.
int GetIntersectionWithRay (const Mesh &mesh, const Octree &octree,
                            const Vector3f &vOrig, const Vector3f &vDirection,
                            float *_t, Vector3f &vIntersection, Vector3f &vNormal);

// Lancer de rayon non accelere : tous les triangles de toutes les faces sont
// testes. Environ 3,4 fois plus lent que la version ci-dessus sur un maillage de
// 662 triangles. Le nom est long expres -- oublier l'octree ne doit pas etre
// silencieux.
//
// Les faces a N>=4 sont triangulees en EVENTAIL depuis le coin 0, a la volee et
// sans allocation ; une face concave y est donc approximee, alors que le chemin
// accelere la traite exactement (glutess). C'est la seule difference de resultat
// entre les deux.
int GetIntersectionWithRayBruteForce (const Mesh &mesh,
                                      const Vector3f &vOrig, const Vector3f &vDirection,
                                      float *_t, Vector3f &vIntersection, Vector3f &vNormal);
