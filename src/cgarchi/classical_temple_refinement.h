#pragma once

#include <limits>
#include <vector>

#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_krepis.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Raffinements du temple : courbure par un champ vertical, inclinaison des
//  colonnes par cisaillement a pied fixe, stations des pieces longues
// ============================================================================
//
// Repere du temple (D2-5) : X le long de la facade, Y en profondeur (facade en
// Y < 0), Z vertical ; z depuis le pied du degre le plus bas. Unites : celles
// de l'appelant.
//
// Conventions (registre RF1 a RF5, classical_orders_implementation.md section
// 10.12) : champ de Coons des quatre paraboles de bord, nul aux angles du
// stylobate, prolonge sur tout le plan (RF1) ; le meme champ pour toutes les
// pieces (RF2) ; translation, cisaillement, champ, un seul arrondi float (RF3) ;
// cisaillement a pied fixe rapporte a la hauteur de la colonne, sommet de
// l'abaque deplace de i vers l'interieur (RF4) ; pieces longues re-tessellees
// par des stations, plans verticaux X = s ou Y = s (RF5).

struct TempleRefinementSettings
{
	// Fleche de chaque bord du dessus du stylobate, au milieu du bord, nulle aux
	// angles du stylobate ; loi et surface : carreau de Coons des quatre paraboles
	// [S : README section 8, lignes des fleches ; convention de l'auteur :
	// section 8, "Ordre d'application"]
	double sagittaFront = 0.0, sagittaRight = 0.0, sagittaBack = 0.0, sagittaLeft = 0.0;
	// Deplacement horizontal du sommet de l'abaque vers l'interieur, perpendiculaire
	// a la rangee ; colonne d'angle : sur les deux axes ; cisaillement a pied fixe
	// rapporte a la hauteur de la colonne [S : README section 8, ligne
	// "Inclinaison des colonnes" ; section 4.4]
	double inclination = 0.0;
	// Ecart de corde vise par les stations : qualite de maillage, pas une valeur
	// du domaine (comme radialSegments) ; > 0 [monument : README section 8,
	// "Ordre d'application", cible e*]
	double chordDeviation = 0.0;
};

// Borne d'interface (D3-17) : segments par axe, comparee avant toute allocation.
constexpr int kRefinementMaxSegments = 1 << 12;

// Plus grande tolerance explicite de validation des contacts sous raffinement
// (D3-102 : paliers 2e-4, 5e-4, 1e-3 m ; registre RF6). Au-dela, la soudure de
// ValidateMesh confond les petites pieces (plan 3 section 25.31). Les bornes
// des reglages en derivent (registre RF5) : chordDeviation <= kRefinementMaxTolerance
// / 10 ; calotte de l'abaque, 1,1 F2 <= kRefinementMaxTolerance / 2.
constexpr double kRefinementMaxTolerance = 1e-3;

// Leve std::invalid_argument, prefixe "Raffinement : " et chemin du champ entre
// apostrophes ('refinement.sagittaFront', ...), dans cet ordre : une fleche
// (Front, Right, Back, Left) ou l'inclinaison non finie ou < 0 ;
// 'refinement.chordDeviation' non finie ou <= 0, puis au-dela de
// kRefinementMaxTolerance / 10 (l'ecart des contacts vaut jusqu'a 4,4 fois e*,
// plan 3 section 25.33 : 2 x 4,4 x 1e-4 <= 1e-3) ; puis, sur chaque axe a pas
// fini, p / 16 < kRefinementMaxTolerance (borne basse) ; puis, sur chaque axe (X puis
// Y), plus de kRefinementMaxSegments segments (len / p, ci-dessous, compare avant
// tout arrondi). `krepis` donne les demi-cotes du stylobate (steps[0]) et la
// largeur du degre le plus bas (steps.back ()).
void ValidateTempleRefinementSettings (const TempleRefinementSettings &r, const KrepisLayout &krepis);

// Champ de Coons, en double, aucune decision ; a, b : demi-cotes du stylobate :
//   0,5 ((1 + X/a) s_R (1 - (Y/b)^2) + (1 - X/a) s_L (1 - (Y/b)^2)
//      + (1 - Y/b) s_F (1 - (X/a)^2) + (1 + Y/b) s_B (1 - (X/a)^2)).
// Nul aux quatre angles (+-a, +-b) ; la fleche de chaque bord au milieu de ce
// bord ; (s_R + s_L) / 2 + (s_F + s_B) / 2 au centre. Ne valide pas.
double TempleRefinementLift (const TempleRefinementSettings &r, double a, double b, double x, double y);

// Stations d'un temple, croissantes : famille S (x, y) et famille S' (xLower,
// yLower), aux milieux de S tant qu'elles ne sont pas deplacees. pitchX,
// pitchY : pas p de chaque axe, +infini sans courbure sur cet axe.
struct RefinementStations
{
	std::vector<double> x, y;             // famille S
	std::vector<double> xLower, yLower;   // famille S'
	double pitchX = std::numeric_limits<double>::infinity ();
	double pitchY = std::numeric_limits<double>::infinity ();
};

// Stations nominales. Par axe, h le demi-cote du stylobate et s la plus grande
// fleche dont la parabole varie le long de cet axe (max (s_F, s_B) en X,
// max (s_R, s_L) en Y, plus le terme croise ramene a l'axe : |s_R - s_L| a / b
// en X, |s_F - s_B| b / a en Y) : pas p = h sqrt (4 e / s), longueur len = 2 L, L le
// demi-cote du degre le plus bas, N = max (1, ceil (len / p)) segments ; S aux
// (len k) / N - L, k = 1 .. N - 1 ; S' aux (len (2k - 1)) / (2N) - L, k = 1 .. N.
// Operations isolees, sans produit ajoute : meme N sur toute cible. s = 0 :
// p infini, N = 1, aucune station, ni de S ni de S' : l'axe n'a pas de courbure.
// Valide d'abord.
RefinementStations NominalRefinementStations (const TempleRefinementSettings &r, const KrepisLayout &krepis);

// Seconde famille d'interdites (registre RF5) : coordonnees de sommets d'une
// autre piece rencontres par les noeuds des stations S (pied de colonne sur le
// stylobate, dessus d'abaque sous l'anneau, bout de l'egout du fronton), par
// axe, chacune avec son ecart exige, strictement. Un noeud ne se soude a un
// sommet que s'il en est proche sur les deux axes : ecarter la station d'un seul
// axe suffit.
struct ClearanceCoordinate
{
	double at = 0.0, distance = 0.0;
};

struct StationClearance
{
	std::vector<ClearanceCoordinate> x, y;
};

// Ecart de la seconde famille : deux fois la plus grande tolerance de soudure
// sous raffinement (registre RF5) ; un noeud a plus de cet ecart d'un sommet sur
// un axe ne s'y soude a aucune tolerance de D3-102.
constexpr double kRefinementClearance = 2.0 * kRefinementMaxTolerance;

// Longueur du plus long bloc de la reunion des intervalles fermes [at - distance,
// at + distance] de `coordinates` (non triees) ; 0 sans coordonnee. Une fenetre
// plus longue ne tient dans aucun bloc : elle contient un point hors de tous.
double LongestClearanceBlock (const std::vector<ClearanceCoordinate> &coordinates);

// Deplace chaque station de `nominal`, dans une fenetre de +-p / 4, a plus de
// p / 16 de toute coordonnee interdite de son axe : candidats, la station, f -+
// p / 8 pour chaque interdite f, et le milieu de deux interdites consecutives ;
// retenu, le candidat valide le plus proche, le plus petit a egalite. S est
// deplacee d'abord ; S' ensuite, S deplacee comptee parmi ses interdites : aucun
// noeud de S' ne coincide avec un noeud de S. Les comptes ne changent pas.
// Sans candidat valide (interdites plus serrees que 2 p / 16), repli : le point
// de la fenetre le plus eloigne des interdites (milieu d'un intervalle libre ou
// bord de fenetre), le plus proche de la station a egalite, puis le plus petit.
// Plancher de ce repli (registre RF5) : le plus grand de kRefinementMaxTolerance
// et de 8 FLT_EPSILON fois la plus grande coordonnee en jeu ; en dessous,
// std::invalid_argument "Raffinement : 'refinement.chordDeviation'". Jamais
// d'autre exception.
// `clearance` (S seulement) : contrainte dure, candidat et repli hors des blocs
// fermes [at - distance, at + distance] de son axe ; candidats ajoutes, les
// bouts de chaque bloc reculees du plus grand ecart de l'axe et le milieu de
// deux blocs consecutifs ;
// repli cherche dans les seuls morceaux de la fenetre hors des blocs, rentre de
// min (plancher, longueur / 4) a un bout de bloc ; sans morceau, refus
// 'refinement.chordDeviation'.
RefinementStations DisplaceRefinementStations (const RefinementStations &nominal, std::vector<double> forbiddenX,
                                               std::vector<double> forbiddenY, const StationClearance &clearance = {});

// Coupe des aretes horizontales de `section` dans leur largeur (registre RF5,
// D3-108) : chaque arete (u_k, z) -> (u_k+1, z) dont z n'est pas dans
// `protectedZ` recoit N_e - 1 sommets, N_e = max (1, ceil (w / pitch)),
// w = |u_k+1 - u_k|, aux u_k + (du j) / N_e, deplaces comme les stations
// (DisplaceRefinementStations) au pas w / N_e, a plus de w / (16 N_e) de
// `forbiddenU` et des deux bouts de l'arete. Aretes verticales et inclinees
// inchangees ; ordre de la section conserve. pitch = +infini : la section,
// inchangee. Les bouts de toutes les aretes sont des interdites. Plancher du
// repli : 8 FLT_EPSILON max (coordinateMagnitude, |u|), coordinateMagnitude
// etant l'echelle des sommets construits (demi-distance + max |u| pour
// l'anneau). Leve
// std::invalid_argument, prefixe "Raffinement : ", si pitch est NaN ou <= 0, ou
// si une arete demande plus de kRefinementMaxSegments segments ; deplacement,
// repli et plancher de DisplaceRefinementStations (refus
// 'refinement.chordDeviation').
std::vector<RingSectionPoint> RefineRingSectionWidth (const std::vector<RingSectionPoint> &section, double pitch,
                                                      const std::vector<double> &protectedZ,
                                                      std::vector<double> forbiddenU, double coordinateMagnitude = 0.0);

} // namespace classical
} // namespace cgarchi
