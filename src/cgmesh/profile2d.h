#pragma once

// ============================================================================
//  Profile2D -- une section de moulure, valeur partagee
// ============================================================================
//
// Trois chantiers demandaient la meme chose sans se connaitre : la moulure des
// bords de champ du gothique, le biseau du texte 3D, la section balayee le long
// d'un chemin. Chacun l'avait ecrite dans son coin -- le gothique sous la forme
// d'un couple (chamW, chamD) et d'un menu a cinq positions, la these de Havemann
// sous celle d'un catalogue de sections fermees. Le type est ici, et les
// producteurs de profils cessent d'etre une enumeration figee : une famille de
// fonctions libres, qu'un appelant etend sans toucher au consommateur.
//
// UNITES ET REPERE -- c'est le point qui ne se devine pas :
//
//   u = enfoncement SOUS la face de reference, positif vers l'arriere ;
//   v = decalage dans le plan, positif vers la matiere.
//
// u est RELATIF. Le code du gothique ecrivait ses sections de barre en z ABSOLU
// (`zF + 1.2 * rb`), ce qui liait la section a la profondeur d'extrusion de la
// baie et interdisait de la produire ailleurs. Ici le profil est une forme ; son
// PLACEMENT appartient au consommateur, qui ajoute la cote de sa face.
//
// Deux familles, qui ne se substituent pas l'une a l'autre :
//
//   - profil d'EBRASEMENT (ouvert) : polyligne partant de (0, 0), consommee par
//     extrudeProfiledToMesh. Le premier point est sur la face avant, le dernier
//     donne l'anneau decale qui descend ensuite droit jusqu'au fond ;
//   - profil de BARRE (ferme) : boucle consommee par sweepProfileAlongArc* et
//     buildBayMoulding, balayee le long d'un chemin.
//
// Rien dans le type ne distingue les deux : c'est le CONSOMMATEUR qui fixe la
// lecture, exactement comme aujourd'hui. La couche nodale, elle, les separe par
// deux types de port distincts -- une barre branchee sur un ebrasement est
// refusee a la connexion plutot que silencieusement mal lue.
//
#include <vector>

#include <cgmath/TVector2.h>

class Mesh;
class Polygon2;

struct Profile2D
{
	std::vector<Vector2d> points;

	bool empty () const { return points.empty (); }
};

// --- profils d'EBRASEMENT (ouverts, premier point en (0, 0)) ----------------

// Chanfrein droit : la section que le gothique construisait a la main. Deux
// points, donc une seule pente.
Profile2D chamferSplayProfile (double width, double depth);

// Cavet : quart de cercle concave, de la face avant vers l'interieur. C'est le
// premier profil d'ebrasement a plus de deux points, et c'est ce qui distingue
// la generalisation d'un simple renommage du chanfrein.
Profile2D cavettoSplayProfile (double width, double depth, int segments = 6);

// --- profils de BARRE (fermes, u relatif a la face de reference) ------------

// Bourdon : demi-rond saillant. `segments` echantillons sur le demi-cercle.
Profile2D rollBarProfile (double radius, int segments = 12);

// Arete : nervure pointue, trois points.
Profile2D keelBarProfile (double radius);

// Doucine symetrique : deux flancs en smoothstep. `segments` par flanc.
Profile2D ogeeBarProfile (double radius, int segments = 6);

// Translate le profil de `du` en u. C'est ce qu'un consommateur applique pour
// PLACER une section de barre sur sa face avant, le profil etant relatif.
Profile2D translatedInU (const Profile2D &profile, double du);

// --- extrusion profilee ------------------------------------------------------

// Extrude `polygon` entre zBottom et zTop en balayant `profile` le long de
// CHAQUE bord de champ, au lieu d'une paroi verticale : la face avant reste
// plate a zTop, chaque arete d'ouverture s'enfonce dans la pierre en suivant le
// profil, puis descend droit jusqu'a zBottom (une ebrasure). Rend l'aspect
// « pierre taillee » (Havemann §5.4) au lieu d'une plaque extrudee a plat.
// Capots et moulure ont des sommets distincts (normales plates correctes).
//
// Le profil doit compter au moins deux points, commencer exactement a (0, 0) et
// n'avoir aucune coordonnee negative -- sinon std::runtime_error. Ce refus est
// prefere a une correction silencieuse : un profil mal oriente produirait une
// piece repliee que rien ne signalerait.
//
// GARDE-FOU HERITE, et il est la raison pour laquelle cette fonction a ete
// GENERALISEE plutot que reecrite : le decalage « vers la pierre » s'auto-
// intersecte sur un contour concave pointu. Deux mesures par contour retombent
// alors sur une paroi verticale -- un rayon caracteristique trop petit, ou une
// courbure concave cumulee trop forte.
//
// ⚠ Le polygone est pris en `const` : sa surface de lecture a ete corrigee pour
// cela (Polygon2::get_points / tesselate / get_n_*). C'est ce qui permet a un
// consommateur tenant une valeur immuable de l'appeler.
void extrudeProfiledToMesh (const Polygon2 &polygon, const Profile2D &profile, Mesh &out,
                            double zBottom, double zTop);

// Forme heritee : un chanfrein droit decrit par sa largeur et sa profondeur.
// Elle delegue a la precedente sur chamferSplayProfile (width, depth) -- c'est
// le meme chemin de production, au seul emballage des arguments pres.
//
// ⚠ Elle est CONSERVEE pour que les cinq cas de non-regression du chanfrein
// continuent de s'ecrire tels qu'ils ont ete ecrits, avant que le profil ne soit
// une valeur. Elle n'a plus d'appelant de production.
void extrudeProfiledToMesh (Polygon2 &polygon, Mesh &out,
                            double zBottom, double zTop, double chamW, double chamD);
