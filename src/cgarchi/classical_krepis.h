#pragma once

#include <cfloat>
#include <memory>
#include <vector>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Krepis : degres rectangulaires empiles sous le stylobate, places puis
//  mailles
// ============================================================================
//
// Repere (D2-5) : X le long de la facade, Y en profondeur, Z vertical ;
// rectangles centres sur l'origine ; pied du degre le plus bas en z = 0,
// dessus du stylobate en z = stylobateTop. Unites : celles de l'appelant,
// toutes les longueurs en valeur absolue.
//
// Degres (registre KR1 a KR3, classical_orders_implementation.md section
// 10.7 ; KR1 amendee, section 10.12) : prismes droits a base rectangulaire,
// aretes vives, dessus horizontal sans raffinement (sous raffinement, grilles a
// stations, que le champ du temple deplace) ; solides pleins concatenes sans
// union, un degre pose sur le suivant par contact plan ; la krepis s'arrete au
// pied de son degre le plus bas (une euthynteria donnee par les sources entre
// comme un degre de lowerSteps).

struct KrepisStep
{
	double height = 0.0;    // hauteur du degre
	double outsetX = 0.0;   // debord en X sur le degre de dessus, de chaque cote
	double outsetY = 0.0;   // debord en Y sur le degre de dessus, de chaque cote
};

struct KrepisSettings
{
	double stylobateHalfWidth = 0.0;   // demi-cote en X du stylobate
	double stylobateHalfDepth = 0.0;   // demi-cote en Y du stylobate
	double stylobateHeight = 0.0;
	std::vector<KrepisStep> lowerSteps;   // sous le stylobate, de haut en bas
};

struct KrepisBox
{
	double halfWidth = 0.0, halfDepth = 0.0;   // demi-cotes en X et en Y
	double zBottom = 0.0, zTop = 0.0;
};

struct KrepisLayout
{
	std::vector<KrepisBox> steps;   // indice 0 : stylobate ; puis lowerSteps, vers le bas
	double stylobateTop = 0.0;      // steps[0].zTop
};

// Bornes d'interface, pas des valeurs du domaine : 64 degres au plus, stylobate
// compris, bornent le maillage a 1 024 sommets ; une longueur d'au plus
// kKrepisMaxLength garde les sommes cumulees representables en float.
constexpr int kKrepisMaxLowerSteps = 63;
constexpr double kKrepisMaxLength = 1e30;
static_assert ((kKrepisMaxLowerSteps + 1) * kKrepisMaxLength < FLT_MAX,
               "les sommes cumulees de longueurs maximales doivent rester finies en float");

// Leve std::invalid_argument, dont le message cite le champ entre apostrophes
// ('stylobateHeight', 'lowerSteps[2].outsetX', 'lowerSteps' pour le compte),
// si :
//   - une longueur (demi-cote, hauteur, debord) n'est pas finie et > 0, ou est
//     hors de [FLT_MIN, kKrepisMaxLength] ;
//   - lowerSteps compte plus de kKrepisMaxLowerSteps degres ;
//   - une hauteur est absorbee par l'arrondi en float a la cote de son pied, ou
//     un debord au demi-cote du degre de dessus.
// Ces controles garantissent des cotes et des demi-cotes cumules distincts en
// float : chaque degre a, dans le maillage, une hauteur et des debords non
// nuls. Ils ne garantissent pas la validite au sens de ValidateMesh, dont la
// tolerance de soudure est relative a la taille du maillage : une hauteur ou un
// debord plus petit que cette tolerance est accepte, et le maillage qui en
// resulte n'est pas manifold. Aucun seuil relatif n'est impose.
void ValidateKrepisSettings (const KrepisSettings &settings);

// Boites des degres. Cotes cumulees de bas en haut depuis z = 0, demi-cotes
// cumules de haut en bas depuis le stylobate. Le pied de chaque degre est, au
// bit pres, le dessus du degre de dessous ; steps[0] porte exactement les
// demi-cotes du stylobate. Valide d'abord les reglages.
KrepisLayout LayoutKrepis (const KrepisSettings &settings);

// Un extrudeToMesh par degre, dans l'ordre de layout.steps, concatenes : 16
// sommets et 12 faces par degre, sommets non partages entre degres. Valide les
// reglages avant toute ecriture dans `layout`.
std::unique_ptr<Mesh> BuildKrepisMesh (const KrepisSettings &settings, KrepisLayout *layout = nullptr);

struct RefinementStations;   // classical_temple_refinement.h

struct KrepisStepSize
{
	unsigned vertices = 0, faces = 0;
};

// Krepis a stations (raffinement, registre RF5 et KR1 amendee) : par degre, dans
// l'ordre de layout.steps, sommets non partages entre degres, un solide ferme du
// meme volume que sans stations. Dessus : grille des abscisses {-demi-cote,
// stations.x dans ]-demi-cote, demi-cote[, demi-cote} et de meme en Y
// (stations.y) ; dessous : grille de stations.xLower et stations.yLower ; quatre
// cotes verticaux en bandes entre l'arete du dessous et celle du dessus (chaque
// triangle avance d'un noeud sur l'une des deux, la plus proche en abscisse,
// celle du dessous a egalite). Avec t_x, t_y, b_x, b_y stations interieures du
// dessus et du dessous : (t_x + 2)(t_y + 2) + (b_x + 2)(b_y + 2) sommets,
// 2 (t_x + 1)(t_y + 1) + 2 (b_x + 1)(b_y + 1) + 2 (t_x + b_x + 2) + 2 (t_y + b_y
// + 2) triangles. Stations croissantes. Positions en double, arrondies une fois
// en float. Sans aucune station (quatre familles vides), la krepis de
// BuildKrepisMesh sans stations, au bit pres (16 sommets et 12 faces par degre).
// `steps`, s'il est fourni, recoit les tailles des degres, dans l'ordre. Valide
// les reglages avant toute ecriture dans `layout` et `steps`.
std::unique_ptr<Mesh> BuildKrepisMesh (const KrepisSettings &settings, const RefinementStations &stations,
                                       KrepisLayout *layout = nullptr, std::vector<KrepisStepSize> *steps = nullptr);

} // namespace classical
} // namespace cgarchi
