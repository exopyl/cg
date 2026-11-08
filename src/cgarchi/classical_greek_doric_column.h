#pragma once

#include <memory>
#include <vector>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Colonne dorique grecque : fut sans base, entasis, cannelures a arete vive,
//  chapiteau a gorgerin, annelets en gradins, echine et abaque
// ============================================================================
//
// Famille distincte des cinq ordres de Vignole (D3-2) : ses reglages ne lisent
// aucune donnee de cinq_ordres.json. Unites : celles de l'appelant, toutes les
// longueurs en valeur absolue. Repere du tour : axe Z, pied du fut sur le
// stylobate en z = 0, sommet de l'abaque en z = height.
//
// Empilement, de bas en haut :
//   - fut, de z = 0 a z = shaftHeight = height - hauteur du chapiteau. Rayon
//     r(z) = droite des deux rayons + e(z). Entasis e (registre GD1) : deux
//     arcs de parabole, e(z) = f (1 - u^2), u = (z - zf) / zf sous zf et
//     (z - zf) / (shaftHeight - zf) au-dessus ; e(0) = e(shaftHeight) = 0,
//     maximum f en zf, ou le profil est parallele a la droite des rayons.
//     Cannelures jointives (demi-angle pi / N, aretes vives sur le cercle du
//     fut), section A ou B (registre GD2) ou a trois centres
//     (classical_flutes.h), de rapport profondeur / corde constant sur toute
//     la hauteur (registre GD2a) ;
//   - gorgerin (hypotrachelion), compte dans le chapiteau : cylindre cannele
//     au rayon upperDiameter / 2, les cannelures du fut continuant jusqu'a son
//     sommet, ou elles s'arretent a plat sous le premier annelet (registre
//     GD2b). A son pied, une rainure : cylindre circulaire de hauteur
//     grooveHeight, au rayon upperDiameter / 2 - grooveDepth, raccorde au fut
//     et au gorgerin par deux jonctions plates (registre GD3c) ;
//   - annelets en gradins (registre GD3a) : suite de listels jointifs
//     {hauteur, saillie}, de bas en haut ; deux listels voisins se raccordent
//     par une jonction plate ;
//   - echine (registre GD4a), en coordonnees (saillie, hauteur depuis le pied
//     de l'echine) : droite de (echinusFootProjection, 0) a la jonction
//     (echinusJunctionProjection, echinusJunctionHeight), puis levre en arc de
//     cercle convexe de rayon echinusLipRadius jusqu'a la tete
//     (echinusTopProjection, echinusHeight). Le centre de la levre est du cote
//     rentrant de la corde jonction-tete ; la jonction est une arete. La
//     saillie peut depasser celle de la tete (levre qui rentre sous l'abaque) :
//     le point le plus saillant de la levre est alors un sommet du profil ;
//   - abaque (registre GD6) : plan carre de demi-cote rayon du sommet du fut +
//     abacusProjection, de hauteur abacusHeight.
// Diametre inferieur et superieur sont libres l'un par rapport a l'autre : un
// fut evase vers le haut est accepte (registre GD7).
// Les saillies du chapiteau se comptent depuis le rayon du sommet du fut,
// upperDiameter / 2 (registre GD5).

// Listel du chapiteau.
struct GreekDoricAnnulet
{
	double height = 0.0;
	double projection = 0.0;   // saillie depuis upperDiameter / 2
};

struct GreekDoricColumnSettings
{
	double height = 0.0;             // stylobate -> sommet de l'abaque
	double lowerDiameter = 0.0;      // diametre du fut en z = 0, aux aretes
	double upperDiameter = 0.0;      // diametre du fut en z = shaftHeight, aux aretes
	double entasisSagitta = 0.0;     // fleche f de l'entasis, sur le rayon
	double entasisHeight = 0.0;      // cote zf de la fleche, depuis le stylobate

	int          fluteCount = 0;     // 0 : fut lisse ; sinon multiple de 4 (registre F5)
	FluteSection fluteSection = FluteSection::Segment60;   // A : arc de 60 degres ; B : demi-cercle ; trois centres
	double       fluteDepthRatio = 0.0;   // ThreeCentred : profondeur / corde (registre GD2a) ; ignore sinon

	double hypotrachelionHeight = 0.0;   // gorgerin, rainure comprise ; 0 : sans gorgerin
	double grooveHeight = 0.0;           // rainure au pied du gorgerin ; 0 : sans rainure
	double grooveDepth = 0.0;            // depuis upperDiameter / 2 ; fond sous celui des cannelures

	std::vector<GreekDoricAnnulet> annulets;   // de bas en haut ; vide : sans annelet

	double echinusHeight = 0.0;
	double echinusFootProjection = 0.0;        // pied de la droite
	double echinusJunctionHeight = 0.0;        // fin de la droite, depuis le pied de l'echine
	double echinusJunctionProjection = 0.0;
	double echinusTopProjection = 0.0;         // sous l'abaque
	double echinusLipRadius = 0.0;             // arc convexe de la jonction a la tete

	double abacusHeight = 0.0;
	double abacusProjection = 0.0;

	int curveSegments = 6;           // segments de la levre de l'echine
	int shaftSegments = 24;          // intervalles reguliers du fut, plus la cote zf
	int fluteSamples = 6;            // segments d'arc par demi-cannelure
	int radialSegments = 48;         // fut lisse : angles du tour (arrondi au multiple de 8)
};

// Bornes d'interface, pas des valeurs du domaine : elles bornent le maillage
// (plan_implementation_03_revision.md, section 11, charge).
constexpr int kGreekDoricMaxFlutes = 48;
constexpr int kGreekDoricMaxAnnulets = 8;
constexpr int kGreekDoricMaxCurveSegments = 32;
constexpr int kGreekDoricMaxShaftSegments = 128;
constexpr int kGreekDoricMaxFluteSamples = 16;
constexpr int kGreekDoricMaxRadialSegments = 1024;
// Longueur maximale, en unites de l'appelant : les sommes de longueurs du fut
// et du chapiteau restent representables en float.
constexpr double kGreekDoricMaxLength = 1e30;
// Fond minimal d'une cannelure, rapporte au rayon des aretes (4 cannelures en
// section B ont leur fond sur l'axe).
constexpr double kGreekDoricMinFluteBottomRatio = 0.01;

// Fond d'une cannelure en son milieu, rapporte au rayon des aretes, pour
// fluteCount cannelures jointives de demi-angle b = pi / fluteCount (corde
// 2 sin b) : section A, cos b - 2 sin b (1 - sqrt3 / 2) ; section B,
// cos b - sin b ; trois centres, cos b - 2 sin b q, q = fluteDepthRatio
// (profondeur / corde), ignore pour A et B. Leve std::invalid_argument si
// fluteCount < 1 ou si section est hors de l'enumeration ; ne valide ni q ni
// le domaine etoile (ValidateGreekDoricColumnSettings le fait).
double GreekDoricFluteBottomRatio (int fluteCount, FluteSection section, double fluteDepthRatio);

// Leve std::invalid_argument, dont le message nomme le champ, si :
//   - height, lowerDiameter, upperDiameter, echinusHeight,
//     echinusJunctionHeight, echinusLipRadius, abacusHeight ne sont pas finis
//     et > 0 ;
//   - entasisSagitta, hypotrachelionHeight, grooveHeight, grooveDepth,
//     echinusFootProjection, echinusJunctionProjection, echinusTopProjection,
//     la saillie d'un annelet ne sont pas finis et >= 0 ; la hauteur d'un
//     annelet n'est pas finie et > 0 (champs nommes annulets[k].height,
//     annulets[k].projection) ;
//   - une longueur non nulle est hors de [FLT_MIN, kGreekDoricMaxLength] ;
//   - height ne depasse pas la hauteur du chapiteau (fut de hauteur nulle), ou
//     donne deux anneaux du fut confondus en float ;
//   - la hauteur d'un membre du chapiteau (rainure, reste du gorgerin, annelet,
//     droite et levre de l'echine, abaque) est absorbee par l'arrondi en float
//     a sa cote ;
//   - entasisHeight n'est pas fini dans ]0, shaftHeight[, ou se confond en
//     float avec 0 ou shaftHeight ;
//   - fluteCount n'est ni 0 ni un multiple de 4 de [4, kGreekDoricMaxFlutes],
//     ou donne avec fluteSection un fond de cannelure a moins de
//     kGreekDoricMinFluteBottomRatio fois le rayon de l'axe (4 cannelures B) ;
//   - fluteSection est hors de l'enumeration ;
//   - fluteCount > 0 et fluteSection = ThreeCentred, et fluteDepthRatio n'est
//     pas fini dans ]0, 1/2[, ou ne donne pas avec fluteCount une section
//     etoilee depuis l'axe (IsThreeCentredFluteStarShaped, demi-angle
//     pi / fluteCount) ;
//   - annulets compte plus de kGreekDoricMaxAnnulets listels ;
//   - grooveHeight depasse hypotrachelionHeight ; grooveDepth
//     est non nulle sans rainure, nulle avec une rainure, ou au moins egale a
//     upperDiameter / 2 ; le fond de la rainure est au-dessus du fond des
//     cannelures au sommet du fut (limite de representation : un anneau a la
//     fois circulaire et cannele n'existe pas) ;
//   - echinusJunctionHeight n'est pas dans ]0, echinusHeight[ ;
//   - echinusLipRadius est inferieur a la demi-distance de la jonction a la
//     tete (aucun arc) ;
//   - le centre de la levre n'est pas plus rentrant que la jonction et que la
//     tete (la hauteur ne serait pas monotone le long de la levre) ;
//   - abacusProjection est inferieure a la saillie maximale de l'echine (le
//     cercle le plus saillant de l'echine est inscrit dans l'abaque) ;
//   - curveSegments, shaftSegments, fluteSamples, radialSegments sont hors de
//     [1, max], [2, max], [1, max], [8, max].
void ValidateGreekDoricColumnSettings (const GreekDoricColumnSettings &settings);

// Profil de la colonne, en unites de l'appelant, de y = 0 a y = height. Les
// sommets du fut ont member = -1 ; les membres sont ceux du bloc "chapiteau",
// de bas en haut : "rainure" (si grooveHeight > 0), "gorgerin" (si
// hypotrachelionHeight > grooveHeight), un "annelet" par listel, "echine",
// "abaque". shaft : foot = vifBottom = 0, vifTop = top = shaftHeight, sans
// conge. flutes : grille du tour si fluteCount > 0, sans anneau de tete,
// headBottom au sommet du gorgerin. Les sommets consecutifs identiques sont
// fusionnes. Valide d'abord les reglages.
ColumnProfile BuildGreekDoricColumnProfile (const GreekDoricColumnSettings &settings);

// Demi-etendues de la colonne selon X, egales selon Y (symetrie d'ordre 4 du
// tour), en unites de l'appelant : valeurs exactes du profil, en double ; le
// maillage, en float, s'en ecarte d'au plus un arrondi float (en Y, le tour
// passe par sin (pi / 2 - pi / N) et non par cos (pi / N)). Anneau par anneau :
// cercle, rayon ; carre, demi-cote ; anneau cannele de rayon R aux aretes, une
// cannelure centree sur chaque axe (registre GD8) : R cos (demi-angle), soit
// R cos (pi / N), chaque section restant sous sa corde (la section a trois
// centres par la validation). Le maillage etant lineaire entre deux anneaux, le maximum sur
// les anneaux est celui du maillage ; le point le plus saillant de l'echine est
// un sommet du profil.
struct GreekDoricColumnHalfExtents
{
	double foot = 0.0;      // anneau du pied, z = 0 (pose sur le stylobate)
	double overall = 0.0;   // maximum sur tous les anneaux (voisinage des colonnes)
};

// Valide d'abord les reglages, comme BuildGreekDoricColumnProfile.
GreekDoricColumnHalfExtents ComputeGreekDoricColumnHalfExtents (const GreekDoricColumnSettings &settings);

// Maillage ferme de la colonne : LatheColumnProfile du profil, echelle 1,
// decoupage aux croisements de plan. Si `profile` est fourni, il recoit le
// profil. Valide les reglages avant toute ecriture dans `profile`.
std::unique_ptr<Mesh> BuildGreekDoricColumnMesh (const GreekDoricColumnSettings &settings,
                                                 ColumnProfile *profile = nullptr);

} // namespace classical
} // namespace cgarchi
