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
//  Entablement file d'une colonnade
// ============================================================================
//
// Coupe symetrique (conventions.plans.face_arriere_entablement
// .cas_colonnade_isolee.coupe_symetrique) : le profil de facade est reproduit
// en miroir par rapport au plan vertical des axes. Chaque sommet du profil, de
// saillie d = distance_axe + s a la hauteur y, devient le rectangle
// [-L - d, L + d] x [-d, d] (X le long de la colonnade, Y en profondeur), L
// etant la demi-longueur entre les axes des colonnes d'about : onglets a 45
// degres et retours d'equerre par construction. Rectangles consecutifs relies
// par des quads plans ; le premier et le dernier sont fermes.

struct EntablatureOptions
{
	DoricCornice doricCornice = DoricCornice::Dentils;
	// Ornements : la bande d'un membre ornement prend la saillie de son fond
	// (les elements sont des coques a part). Sans ornements : bande pleine a s.
	bool ornaments = false;
	int  curveSegments = 6;
};

// Profil de l'entablement (architrave, frise, corniche), y depuis son pied.
// Tous ses anneaux sont carres (plan lineaire).
ColumnProfile BuildEntablatureSection (const OrderData &order, const EntablatureOptions &options);

// Saillie du fond de chaque membre ornement lie (EntablatureOrnamentMember).
std::vector<BandOverride> OrnamentBackOverrides (const OrderData &order);

// `z0` : hauteur du pied de l'entablement ; `closeEnds` = false laisse le
// premier et le dernier rectangle ouverts. Leve std::invalid_argument si un
// anneau n'est pas carre.
std::unique_ptr<Mesh> SweepEntablature (const ColumnProfile &section, double halfLength, double scale, double z0 = 0.0,
                                        bool closeEnds = true);

// Volume exact du balayage (parties cubes, echelle 1).
double SweptVolume (const ColumnProfile &section, double halfLength);

} // namespace classical
} // namespace cgarchi
