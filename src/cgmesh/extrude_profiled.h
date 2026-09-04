#pragma once

// ============================================================================
//  Contours 2D -> solide a ARETE PROFILEE (chanfrein, biseau, conge)
// ============================================================================
//
// La variante de `extrude_contours.h` ou la paroi n'est pas droite : l'arete
// SUPERIEURE suit un profil, puis la paroi descend d'aplomb jusqu'au fond.
//
// POURQUOI PAS `extrudeProfiledToMesh` (profile2d.h), qui fait deja cela pour la
// baie gothique -- et c'est la raison d'etre de ce fichier. Celle-la decale ses
// anneaux PAR SOMMET, le long des normales, ce qui garde une correspondance un
// pour un entre niveaux mais s'auto-intersecte sur un contour concave pointu.
// Elle s'en protege en RETOMBANT SUR UNE PAROI DROITE des que la courbure
// concave cumulee d'un contour depasse ~45 degres (profile2d.cpp, `concaveTurn >
// 0.8`) ou que le rayon caracteristique est trop petit. Sur des glyphes, ce
// seuil est franchi par un `E`, un `M`, le moindre empattement : la majorite des
// lettres sortiraient SANS arete profilee, en silence.
//
// Ici les anneaux sont des OFFSETS CLIPPER2 (`offsetContours`), qui gerent
// nativement l'auto-intersection, la scission d'un contour en deux et la
// disparition d'une forme trop mince. Le prix est qu'il n'y a plus de
// correspondance entre niveaux : la surface entre deux anneaux est tessellee
// comme une COURONNE -- l'anneau interieur fourni A L'ENVERS, que le NonZero du
// tessellateur retire --, et chaque sommet recoit la cote de l'anneau dont il
// provient. Pas de `differenceContours` ici, et c'est mesure : une seconde passe
// Clipper2 recolle des micro-aretes que la paroi garde, ce qui ouvrait la peau
// sur les quatre polices difficiles du catalogue.
//
// ============================================================================
//  SENS DU DECALAGE -- decide, pas subi
// ============================================================================
//
//   Inward  (DEFAUT) -- l'arete est RENTREE. L'emprise au sol reste NOMINALE :
//                       30 mm demandes = 30 mm mesures. C'est la convention que
//                       les cotes affichees et la hauteur de lettre exigent
//                       (decision D1 du dossier de faisabilite).
//   Outward          -- la face du dessus reste nominale et la matiere GROSSIT
//                       vers le bas, jusqu'a +v_max. L'emprise croit donc
//                       d'autant. C'est le comportement de `ExtrudeGeometry` de
//                       three.js, donc de stltext.com -- non par choix de sa
//                       part, mais parce que c'est ce que fait sa bibliotheque.
//
// ⚠ Dans les deux modes, seule l'arete du DESSUS est profilee. La reference
// traite ses deux faces (son Z vaut `depth + 2 x bevel`) ; ici le fond reste
// plat, ce qu'une piece posee sur un plateau demande de toute facon.
//
// ============================================================================
//  CE QUE LE PROFIL DOIT ETRE, et ce qu'on en fait
// ============================================================================
//
// Un profil d'EBRASEMENT (`Profile2D`, famille ouverte de profile2d.h) : une
// polyligne partant de (0, 0), u et v croissants. u = enfoncement sous la face
// de reference, v = decalage dans le plan.
//
// La LECTURE est celle du consommateur, et elle est ici : l'anneau du niveau k
// est le contour nominal decale de `v_k - v_max` (mode Inward), et il siege a
// `zTop - u_k`. Autrement dit le premier point du profil donne la face du
// dessus -- rentree de tout `v_max` -- et le dernier donne le plan ou la paroi
// redevient d'aplomb. Consequence a connaitre : **c'est le CAVET
// (`cavettoSplayProfile`) qui rend un conge convexe** sous cette lecture, la
// courbe quittant la face du dessus tangentiellement. Le verifier plutot que le
// deduire : cf. tu_cgmesh_extrude_profiled.cpp.
//
// ============================================================================

#include <cstddef>
#include <vector>

#include "extrude_contours.h"   // ExtrudeContour

struct Profile2D;
class Mesh;

struct ProfiledExtrudeOptions
{
	float zBottom = 0.f;
	float zTop    = 1.f;

	enum class Direction { Inward, Outward };
	Direction direction = Direction::Inward;

	// Estampille chaque face (cf. Mesh::Material_Add). Le defaut correspond a
	// MaterialType::MATERIAL_NONE.
	unsigned int materialId = (unsigned int)-1;
};

// Ce que la passe a REELLEMENT execute -- de quoi dire a l'utilisateur ce que
// son reglage a coute, au lieu de le lui laisser deviner sur l'ecran.
struct ProfiledExtrudeStats
{
	// Niveaux d'anneaux, soit un par point du profil. Deux pour un chanfrein ou
	// un biseau, `segments + 1` pour un cavet.
	std::size_t rings = 0;

	// Couronnes effectivement tessellees. Une de moins que les anneaux, moins
	// celles qui se sont trouvees vides (deux anneaux identiques).
	std::size_t bands = 0;

	// Formes PERDUES par le decalage : le nombre de morceaux de l'anneau le plus
	// rentre, compare a celui du contour nominal. Un delie plus mince que deux
	// fois le profil DISPARAIT -- geometriquement correct, et la seule chose que
	// l'utilisateur doive savoir avant d'imprimer. Zero quand rien n'a ete perdu.
	std::size_t vanishedPieces = 0;

	// Sommets qu'aucun des deux anneaux d'une couronne ne reclame -- des points
	// ajoutes par le tessellateur (callback COMBINE). Ils recoivent la cote de
	// l'anneau le plus proche. Reste a zero sur les cas mesures ; s'il grimpe,
	// c'est le signe que la classification par coordonnee ne suffit plus.
	std::size_t steinerPoints = 0;
};

// Un profil de LARGEUR NULLE est licite : c'est la limite, et elle rend une
// extrusion droite. Une interface peut donc offrir « arete vive » en mettant la
// largeur a zero, sans avoir a debrancher quoi que ce soit -- ce qu'une page a
// graphe fixe ne sait pas faire.
//
// Rend false -- sans toucher a `out` -- quand rien de sense ne peut etre
// construit : contours vides, profil de moins de deux points, profil qui ne part
// pas de (0, 0) ou a coordonnee negative, profil plus PROFOND que la piece,
// hauteur nulle, ou anneau du dessus entierement consomme par le decalage (un
// profil plus large que la moitie de la piece).
bool extrudeProfiledContours (const std::vector<ExtrudeContour>& contours,
                              const Profile2D& profile,
                              const ProfiledExtrudeOptions& opt,
                              Mesh& out,
                              ProfiledExtrudeStats* stats = nullptr);
