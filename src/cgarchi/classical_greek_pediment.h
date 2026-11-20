#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Fronton grec : corniches rampantes en Lambda et comble plein
// ============================================================================
//
// Sur chaque facade, Front (Y < 0) et Back, une corniche rampante en Lambda ; un
// comble plein triangulaire commun aux deux facades, dont les abouts sont les
// tympans. Repere du temple (D2-5) ; z depuis le pied de l'entablement, z11 :
// dessus de l'anneau, GreekEntablatureBandBounds (entablement).back ().
// Pente 1 : r (PE2') : le haut de la corniche rampante monte de apexRise, de
// z11 a l'axe, et chaque ligne de la section lui est parallele ;
// X_e = halfWidth + entablement.bands.back ().topProjection, l'avant du
// couronnement, n'est qu'une borne.
//
// Conventions (registre PE1, PE2' a PE4', PE5, TE5 L16 a L18,
// classical_orders_implementation.md section 10.11) : section vraie polygonale
// donnee, balayee parallelement a la pente, onglet au faite sans couvercle
// (PE1) ; pente donnee par r, hauteurs normales converties en cotes par
// s = hypot (r, 1) / r, haut de la section (h = T) a z11 + apexRise dans l'axe
// (PE2') ; coupe d'angle horizontale par le plan z11, face de pied posee a plat
// sur le dessus de l'anneau (PE3') ; comble triangulaire, base sur z11 entre les
// pieds du haut de la section, pans par l'arete haute du dos, ni sima, ni
// acroteres, ni tuiles (PE4') ; deux frontons identiques, un jeu de reglages
// (PE5).
struct GreekPedimentSettings
{
	double apexRise = 0.0;       // H_t : de z11 au haut de la corniche rampante (h = T) dans l'axe
	double rakingRun = 0.0;      // r : pente 1 : r du haut et de chaque ligne de la section
	double tympanumFace = 0.0;   // u_t : nu du tympan, depuis la ligne des axes de la facade, vers l'exterieur
	std::vector<RingSectionPoint> rakingSection;   // section vraie, sens direct : u depuis le nu du tympan vers
	                                               // l'exterieur, z = h normale a la pente depuis le dessous ;
	                                               // premier sommet (0, 0), dernier (0, T), T = h maximale
};

// Valide d'abord l'entablement (ValidateGreekEntablatureSettings, exceptions
// "Entablement : " remontees telles quelles), puis leve std::invalid_argument,
// message prefixe "Fronton : " et champ entre apostrophes, dans cet ordre :
//   - 'pediment.apexRise' non fini ou <= 0 ;
//   - 'pediment.rakingRun' non fini ou <= 0 ;
//   - 'pediment.tympanumFace' non fini, ou hors de ]-innerFace, couronnement[,
//     le couronnement etant bands.back ().topProjection ;
//   - 'pediment.rakingSection' : moins de 3 sommets ; un sommet non fini
//     ('pediment.rakingSection[k]') ; premier sommet different de (0, 0) ;
//     dernier sommet d'u non nul, ou de h different du maximum des h, ou
//     maximum <= 0 ; un autre sommet d'u <= 0 ou de h < 0 ; deux sommets
//     consecutifs egaux (le dernier et le premier compris) ; polygone non
//     simple, decide par des produits jamais contractes en FMA (meme verdict sur
//     toute cible) ;
//   - 'pediment.apexRise' si H_b = apexRise - T s n'est pas > 0 : le dessous de
//     la corniche rampante ne serait pas au-dessus de z11 dans l'axe.
// Hors la simplicite et H_b, ces decisions sont des copies et des comparaisons,
// sans produit. Une section qui les passe est de sens direct (premier sommet en
// (0, 0), dernier en (0, T), u > 0 ailleurs) ; le maillage le garde par une
// std::logic_error. Son aire calculee en double peut toutefois s'annuler quand
// les produits des coordonnees passent sous le plus petit double (coordonnees de
// l'ordre de 1e-162) : une telle section se confond une fois arrondie en float,
// et le maillage la refuse par la garde float, avant la garde du sens.
void ValidateGreekPedimentSettings (const GreekPedimentSettings &pediment, const GreekEntablatureSettings &entablature);

// Poche du comble (registre CE-14) : boite ouverte par le bas, de la base du
// comble (zBase = zFoot + z11) a zTop, sur l'emprise [x0, x1] x [y0, y1].
// Coordonnees dans le repere du temple, celui des sommets du fronton (zTop est
// une cote absolue, comme zBase) ; le fronton ne connait pas ce qui l'occupe.
struct RoofPocket
{
	double x0 = 0.0, x1 = 0.0;   // emprise en X, x0 < x1
	double y0 = 0.0, y1 = 0.0;   // emprise en Y, y0 < y1
	double zTop = 0.0;           // plafond, > zBase
};

// Pieces du fronton, apres une seule validation (fronton et entablement), dans
// l'ordre des parties du temple : corniche rampante Front, corniche rampante
// Back, comble (une piece, ou ses tranches avec plusieurs poches). `peristyle`
// et `zFoot` : ceux de l'anneau (SweepRectangularRing sur halfWidth, halfDepth,
// a la cote zFoot).
//
// Repere, calcule une fois : zBase = zFoot + z11, l'expression du dessus de
// l'anneau ; s = hypot (r, 1) / r ; T = h du dernier sommet ; H_b = apexRise -
// T s ; pour chaque sommet k de la section (u_k, h_k), rise[k] = H_b + h_k s,
// ridge[k] = zBase + rise[k], foot[k] = r rise[k] et y_k = -+(halfDepth +
// (tympanumFace + u_k)), stockes et relus par les trois pieces ; xTop = foot[T],
// pied du haut de la section et egout du comble. u = 0 donne exactement le plan
// du tympan, -+(halfDepth + tympanumFace).
//
// Corniche rampante (PE1, PE2', PE3') : un Lambda ferme par facade, de n
// sommets de section : sommets de faite (0, y_k, ridge[k]), communs aux deux
// bras ; sommets de pied (+-foot[k], y_k, zBase), tous dans le plan z = zBase ;
// une paroi de deux triangles par arete de section et par bras, plane (deux
// trajectoires paralleles) ; face de pied dans le plan z = zBase, de normale
// -z, triangulee par decoupe d'oreilles du polygone (u_k, h_k) en double, dont
// elle est l'image affine ; aucun couvercle au faite. 3n sommets, 6n - 4
// triangles ; sens des faces regle par bras et par facade (normales
// sortantes). Le Back est le miroir du Front : y oppose au bit pres, faces
// retournees.
//
// Comble (PE4') : prisme du triangle (-xTop, zBase), (xTop, zBase),
// (0, ridge[T]), T = n - 1 le sommet haut-dos, entre y = -(halfDepth +
// tympanumFace) et +(halfDepth + tympanumFace) ; 6 sommets, 8 triangles. Ses
// sommets sont, bit pour bit, les sommets T (pieds et faite) des deux Lambda.
//
// Precondition sur `peristyle`, verifiee : X_e > 0 et halfDepth +
// tympanumFace > 0 (le fronton est de part et d'autre du plan de symetrie et
// devant la ligne des axes ; comparaisons des sommes du repere, sans produit).
// Positions en double, arrondies une fois en float. Leve std::invalid_argument,
// prefixe "Fronton : ", dans cet ordre : halfWidth, halfDepth ou zFoot non fini ;
// 'peristyle.halfWidth' si X_e <= 0 ; 'peristyle.halfDepth' si halfDepth +
// tympanumFace <= 0 ; 'pediment.rakingRun' si xTop n'est pas <= X_e : le haut
// de la corniche rampante atteindrait z11 au-dela de l'avant du couronnement ;
// 'pediment.apexRise' si un sommet de faite, zBase + rise[k], se confond en float
// avec zBase (H_b trop petit, paroi en lame) ;
// "Fronton : sommets confondus en float" si deux sommets voisins de la face de
// pied, ou de la section de faite, se confondent une fois arrondis en float, ce
// que la validation en double ne voit pas. `afterEach`, s'il est fourni, est
// appele sur chaque piece des qu'elle est construite, avant la suivante ; une
// exception qu'il leve arrete la construction et remonte telle quelle ; aucune
// piece n'est alors rendue.
//
// Poches (`pockets`, CE-14, CE-34) : Lambda inchanges dans tous les cas ; le
// comble depend du nombre de poches. Aucune : le prisme ci-dessus. Une seule :
// une piece, decrite ci-dessous. Plusieurs : une piece par tranche (voir plus
// bas).
//
// Une poche : comble creuse d'une boite ouverte
// par le bas, de zBase a zTop sur [x0, x1] x [y0, y1]. Sommets : les 6 du
// prisme, dans le meme ordre, puis les 4 coins du bas de la poche (z = zBase)
// et les 4 du plafond (z = zTop), chacun dans l'ordre (x0, y0), (x1, y0),
// (x1, y1), (x0, y1) ; 14 sommets. Triangles : les 4 des pans, les 2 des
// abouts, la base en couronne (8), les 4 parois (8) et le plafond (2) ; 24
// triangles, normales sortantes du solide (vers le vide dans la poche). Les
// coordonnees des sommets de la poche sont des copies des bornes et de zBase,
// sans produit (rien a contracter en FMA).
//
// Plusieurs poches : comble en tranches le long de Y. Bornes des tranches :
// {-yTympanum, y0 et y1 de chaque poche, +yTympanum}, triees, sans doublon.
// Dans la tranche [y0, y1], les poches actives (celles dont l'emprise couvre la
// tranche) sont triees par x0 ; profil du dessous : hauteur zTop sur chaque
// poche, zBase ailleurs, de -xTop a xTop, deux intervalles voisins de meme
// hauteur confondus (deux poches au contact par une face, de meme plafond,
// n'ont pas de paroi entre elles). Section de la tranche : sous les pans
// (-xTop, zBase) - (0, ridge[T]) - (xTop, zBase), au-dessus du profil. Une piece
// par tranche, dans l'ordre des Y croissants, apres les deux Lambda ; chacune
// porte ses deux pans, le dessous (intervalles du profil, normale -z ; parois
// entre eux, normale vers la colonne la plus haute) et ses bouts : a un tympan,
// la section entiere ; dans un plan de coupe y = cte entre deux tranches, la seule partie
// de sa section que la voisine ne couvre pas (colonnes ou son profil est plus
// bas que celui de la voisine), jamais de face la ou les deux sections se
// recouvrent. Les tranches ne sont donc pas fermees une a une ; leur reunion,
// sommets confondus par position (au bit pres, calcules sur les memes doubles),
// est fermee : chaque arete est portee par deux faces, une dans chaque sens,
// sans T-jonction (dans un plan de coupe, tout bord de face est subdivise par
// les bornes des deux profils et par les hauteurs des colonnes qui les
// bordent) ; deux poches de tranches voisines qui ne se toucheraient que par
// une arete verticale sont refusees (voir plus bas). Sommets d'une
// piece dans l'ordre de premiere utilisation, sans doublon de position. Les
// coordonnees sont des copies des bornes, de zBase, de xTop et du faite, sans
// produit.
//
// Refus des poches, std::invalid_argument prefixe "Fronton : ", apres ceux
// ci-dessus et dans cet ordre, chaque poche dans l'ordre de la liste :
//   - 'roofPocket' : une borne non finie ;
//   - 'roofPocket.x', 'roofPocket.y' : emprise vide ou inversee (x0 >= x1,
//     y0 >= y1) ;
//   - 'roofPocket.zTop' si zTop <= zBase ;
//   - 'roofPocket.x' si x0 <= -xTop ou x1 >= xTop (bord de la base, pied d'un
//     pan) ; 'roofPocket.y' si y0 <= -yTympanum ou y1 >= yTympanum (tympan) ;
//   - 'roofPocket.zTop' si un coin du plafond n'est pas strictement sous les
//     pans : avec xm = max (-x0, x1), (zTop - zBase) xTop doit etre <
//     (ridge[T] - zBase) (xTop - xm), produits arrondis en double, jamais
//     contractes (meme verdict sur toute cible) ;
//   - 'roofPocket' si l'un de ces criteres stricts tombe une fois les
//     coordonnees arrondies en float (poche au contact du comble en float) ;
// puis, avec plusieurs poches :
//   - "'roofPocket' : les poches k et l se recouvrent" (k < l, rangs dans la
//     liste a partir de 0) si leurs emprises ont une intersection d'aire non
//     nulle ; le contact par une face est admis ; le verdict en double vaut pour
//     les bornes arrondies en float (arrondi monotone) ;
//   - "sommets confondus en float : bornes de deux poches" si deux bornes Y
//     distinctes des tranches, ou deux bornes X distinctes d'une meme tranche
//     ({-xTop, x0 et x1 des poches actives, xTop}), se confondent une fois
//     arrondies en float ;
//   - "sommets confondus en float : plafonds de deux poches au contact" si deux
//     poches d'une meme tranche se touchent par une face (x1 de l'une egal au x0
//     de l'autre) et que leurs plafonds, distincts en double, se confondent en
//     float (la paroi entre eux serait degeneree) ;
//   - puis, plan de coupe par plan de coupe, par Y croissants : le premier des
//     deux memes refus si deux bornes X distinctes des profils des deux
//     tranches voisines se confondent en float ; "'roofPocket' : deux poches
//     de tranches voisines ne se touchent que par une arete" si, a une borne
//     x_j (colonnes l0, l1 du profil inferieur et u0, u1 du superieur, de part
//     et d'autre de x_j), ]max (l0, u1), min (l1, u0)] ou ]max (l1, u0),
//     min (l0, u1)] est non vide : une cote ou chaque tranche n'est pleine
//     que d'un cote de x_j, et pas du meme (arete portee par quatre faces) ;
//     enfin le second refus si deux hauteurs distinctes des colonnes qui
//     bordent une meme borne, dans l'un ou l'autre profil, se confondent en
//     float.
std::vector<std::unique_ptr<Mesh>> BuildGreekPedimentPieceMeshes (const GreekPedimentSettings &pediment,
                                                                  const GreekEntablatureSettings &entablature,
                                                                  const PeristyleLayout &peristyle, double zFoot,
                                                                  const std::function<void (const Mesh &)> &afterEach = {},
                                                                  const std::vector<RoofPocket> &pockets = {});

struct RefinementStations;   // classical_temple_refinement.h

// Pieces a stations (raffinement, registre RF5) : memes pieces, meme ordre,
// memes refus, puis les suivants. Stations de la trajectoire k (sommet k de la
// section, dans un bras) : les stations.x de ]-foot[k], foot[k][, hors de
// x = 0 ; a chacune, la cote zBase + rise[k] ((foot[k] - |x|) / foot[k]) est
// calculee une fois dans le repere et relue par les deux Lambda et, pour
// k = T, par le comble (D3-90). Paroi entre deux trajectoires voisines en
// fermeture eclair, abscisse -|x| du pied au faite, le sommet de la trajectoire
// k + 1 d'abord a egalite ; face de pied sans station. Lambda : 3n + P sommets,
// 6n - 4 + 2P triangles, P le nombre de stations de toutes les trajectoires des
// deux bras ; ses aretes haut-dos sont coupees aux stations de la trajectoire T
// et restent communes avec le comble. Comble, avec m les stations de la
// trajectoire T (celles de ]-xTop, xTop[) et n_y les abscisses de {-yTympanum,
// stations.yLower dans ]-yTympanum, yTympanum[, yTympanum} : base en grille
// ({-xTop, stations, xTop} x n_y) a zBase ; pans en grille, en X le pied, les
// stations et le faite, aux cotes de la trajectoire T, leurs bords +-xTop etant
// des sommets de la base ; abouts en fermeture eclair entre la base et le haut.
// (2m + 3) n_y sommets, 2 (2m + 3) (n_y - 1) + 2 (2m + 1) triangles. Refus en
// plus, std::invalid_argument prefixe "Fronton : " : stations.x ou
// stations.yLower non finies ou non strictement croissantes ; une station
// confondue en float avec sa voisine, un pied ou le faite d'une trajectoire.
// Sans station dans ]-xTop, xTop[ ni dans ]-yTympanum, yTympanum[, les pieces de
// la fonction sans stations, au bit pres. Une liste de poches non vide est
// refusee apres la validation des reglages, avant les refus du maillage :
// "Fronton : 'roofPocket' ne se combine pas avec les stations du raffinement".
std::vector<std::unique_ptr<Mesh>> BuildGreekPedimentPieceMeshes (const GreekPedimentSettings &pediment,
                                                                  const GreekEntablatureSettings &entablature,
                                                                  const PeristyleLayout &peristyle, double zFoot,
                                                                  const RefinementStations &stations,
                                                                  const std::function<void (const Mesh &)> &afterEach = {},
                                                                  const std::vector<RoofPocket> &pockets = {});

} // namespace classical
} // namespace cgarchi
