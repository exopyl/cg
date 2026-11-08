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
// 10.7) : prismes droits a base rectangulaire, aretes vives, dessus
// horizontal ; solides pleins concatenes sans union, un degre pose sur le
// suivant par contact plan ; la krepis s'arrete au pied de son degre le plus
// bas (une euthynteria donnee par les sources entre comme un degre de
// lowerSteps).

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

} // namespace classical
} // namespace cgarchi
