#pragma once

#include <vector>

#include <cgarchi/classical_orders.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Cannelures du fut : section en plan
// ============================================================================
//
// N cannelures centrees aux angles 2 pi j / N (la premiere sur +X), de
// demi-angle beta : arretes a +-beta autour de chaque centre, sur le cercle du
// fut de rayon R. Corde w = 2 R sin beta. Le nombre est constant sur toute la
// hauteur et les largeurs suivent le diametre local : beta est fixe.
//
// Regles de ornements.cannelures, encodees ici :
//   - dorique (20, sans listel, « cannelures jointives ») : beta = pi / N,
//     corde = 2 r sin(9 degres) ;
//   - ordres a listels : listel = fraction lambda du pas, lue sur au_pied
//     (listel / (pi D / N)) ; beta = (pi / N)(1 - lambda) ;
//   - A (dorique, « arc de 60 degres : rayon egal a la corde, centre au sommet
//     du triangle equilateral construit sur la corde ») : centre du cote
//     oppose a l'axe, a R cos beta + w sqrt3 / 2, rayon w ; la cannelure est
//     creuse, fleche w (1 - sqrt3 / 2) ;
//   - B et ordres a listels (« demi-cercle de diametre egal a la corde,
//     profondeur = demi-corde ») : cercle de centre le milieu de la corde
//     (R cos beta) et de rayon w / 2.
//
// Le tour echantillonne chaque anneau par une fonction rayon(theta). Le
// demi-cercle exact deborde sous l'arete, au-dela de +-beta (bande de
// asin(tan beta) - beta, 6,5e-4 rad a l'ionique, 2,0e-3 au dorique B) : il est
// ecrete a l'arete. Rayon(theta) suit le demi-cercle (racine proche de l'axe)
// pour |offset| < beta et vaut R au-dela : aretes sur le cercle du fut,
// profondeur exacte, et paroi radiale de R cos 2 beta a R a l'arete au lieu du
// surplomb. Pour des cannelures jointives (dorique B), les parois voisines
// forment une arete d'epaisseur nulle.
//
// Echantillonnage (FluteSampleOffsets) : sommets uniformes en abscisse
// curviligne sur l'arc de coupe, de son fond a son pied ; pour une section a
// paroi (demi-cercle), un meridien juste avant l'arete, a beta (1 -
// kFluteWallGap), et un a l'arete : la paroi est maillee entre ces deux
// meridiens, bande ou l'ecart a la section exacte n'est pas mesure.

constexpr double kFluteSegment60Sagitta = 0.13397459621556135;   // 1 - sqrt3/2

// Ecart relatif du meridien de pied de paroi a l'arete. Assez grand pour que,
// sur les anneaux non canneles du tour, les deux sommets restent distincts a la
// soudure de ValidateMesh (r beta 1e-2, soit 1e-3 M a l'ionique).
constexpr double kFluteWallGap = 1e-2;

// Tolerance angulaire (radians) commune a l'arete de FlutedRadius et au
// dedoublonnage des angles du tour.
constexpr double kFluteAngleTolerance = 1e-9;

// Rayon de la section cannelee a l'angle theta : R a l'arete et au-dela
// (|offset| >= halfAngle - kFluteAngleTolerance).
double FlutedRadius (double R, int count, double halfAngle, FluteSection section, double theta);

// Demi-angle des cannelures d'un ordre (constant sur la hauteur).
double FluteHalfAngle (const OrderData &order);

// Section A avec le centre « vers l'axe » pris a la lettre : rayon a l'angle
// `offset` du centre de la cannelure, sur l'arc mineur entre les aretes. Sert
// a montrer que cette lecture bombe la cannelure vers l'exterieur.
double Segment60TowardAxisRadius (double R, double halfAngle, double offset);

// Decalages angulaires >= 0 des sommets d'une cannelure de demi-angle beta
// sur l'arc de coupe : 0, m points uniformes en abscisse curviligne jusqu'au
// pied de l'arc (pour un demi-cercle, le point ou le rayon issu de l'axe a
// l'angle beta le coupe : pied de la paroi), puis, pour une section a paroi,
// beta (1 - kFluteWallGap) et beta. Croissants.
std::vector<double> FluteSampleOffsets (double halfAngle, FluteSection section, int samples);

// Demi-angles des anneaux de tete, decroissants : `rings` decalages pris
// parmi FluteSampleOffsets (beta, section, samples), repartis sur l'arc.
std::vector<double> HeadHalfAngles (double halfAngle, FluteSection section, int samples, int rings);

// Decalages >= 0 de la grille du tour pour une cannelure : sommets de la
// section pleine, plus, pour chaque anneau de tete d'une section a paroi, le
// meridien de pied de paroi. Croissants, sans doublon.
std::vector<double> FluteGridOffsets (double halfAngle, FluteSection section, int samples, int headRings);

} // namespace classical
} // namespace cgarchi
