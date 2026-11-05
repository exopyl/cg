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
//  Colonnade : implantation et ornements d'entablement
// ============================================================================
//
// Repere : X le long de la colonnade, Y en profondeur (facade en Y < 0), Z
// vertical ; axes des colonnes en Y = 0, centres sur X = 0. Unites : parties.

struct ColonnadeLayout
{
	double axisSpacing = 0.0;      // AxisSpacing (order)
	std::vector<double> axes;      // X des axes, croissants, centres sur 0
	double halfLength = 0.0;       // demi-distance des axes d'about
};

// Leve std::invalid_argument si columns < 1.
ColonnadeLayout LayoutColonnade (const OrderData &order, int columns);

enum class OrnamentKind
{
	Triglyph, Regula, Gutta, Mutule, MutuleListel, MutuleGutta, SoffitGutta, Dentil, Modillion, CompositeBlock
};
constexpr int kOrnamentKindCount = static_cast<int> (OrnamentKind::CompositeBlock) + 1;
static_assert (kOrnamentKindCount == 10, "OrnamentKind : mettre a jour OrnamentKindName et les familles");

const char *OrnamentKindName (OrnamentKind kind);

// Face de l'entablement : facade (Y < 0), arriere (Y > 0), retours d'about.
enum class ColonnadeFace { Front, Back, Left, Right };

// Un ornement : position le long de sa face (a, centre et etendue), saillie
// depuis le plan des axes (d0 fond, d1 face ; pour un retour, depuis l'axe de
// la colonne d'about), hauteurs dans le profil de l'entablement.
struct OrnamentPlacement
{
	OrnamentKind kind = OrnamentKind::Dentil;
	ColonnadeFace face = ColonnadeFace::Front;
	double centre = 0.0;
	double a0 = 0.0, a1 = 0.0;
	double d0 = 0.0, d1 = 0.0;
	double z0 = 0.0, z1 = 0.0;
	// Gouttes : rayons du bas et du haut, centre en profondeur.
	double radiusBottom = 0.0, radiusTop = 0.0, depthCentre = 0.0;
};

// Placement sur les quatre faces de l'entablement `section`
// (BuildEntablatureSection, ornements actives).
//
// Motif de periode p et de largeur w, centre en anchor + k p, le long de
// chaque face : anchor = axe de la premiere colonne pour les ornements lies
// aux colonnes (triglyphes, mutules, modillons, denticules doriques et
// corinthiens), X = 0 pour ceux dont l'alignement n'est pas donne (denticules
// ioniques, blocs composites ; convention) ; retours ancres sur Y = 0. Regle
// d'angle : un element n'est garde que si son etendue tient entre les plans de
// fond des retours (ou des faces), si bien que l'angle [fond, face]^2 reste nu.
// Regula et gouttes suivent les triglyphes ; sous chaque mutule, un listel
// (mutules.gouttes : « sous un listel de 1/2 ») et ses gouttes ; gouttes du
// plafond de la corniche a denticules, au droit de chaque triglyphe.
std::vector<OrnamentPlacement> PlaceEntablatureOrnaments (const OrderData &order, const ColonnadeLayout &layout,
                                                          const ColumnProfile &section, DoricCornice cornice);

// Coque fermee de chaque famille (un element par composante), en unites
// monde : X, Y du placement, Z = z0 + hauteur dans le profil, le tout x scale.
// Gouttes : troncs de cone a `guttaSides` cotes. Triglyphe : prisme vertical de la
// section en dents de scie (canaux en V, demi-canaux aux bords).
std::vector<std::pair<OrnamentKind, std::unique_ptr<Mesh>>> BuildOrnamentMeshes (
	const OrderData &order, const ColonnadeLayout &layout, const std::vector<OrnamentPlacement> &placements, double z0,
	double scale, int guttaSides);

} // namespace classical
} // namespace cgarchi
