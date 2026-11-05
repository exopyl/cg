#pragma once

#include <memory>
#include <vector>

#include <cgarchi/classical_profile.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Tour sur plan : profil de colonne -> maillage ferme
// ============================================================================
//
// Chaque sommet du profil devient un anneau : un point par angle theta, a la
// distance Ring::RadiusAt (theta) de l'axe Z, a la hauteur y. Deux anneaux
// consecutifs sont relies par des quads coupes en deux triangles ; le premier
// et le dernier anneau sont fermes par un eventail depuis l'axe. Axe vertical
// Z, pied a z = 0, comme CreateCylinder.
//
// Echantillonnage : `radialSegments` angles reguliers (multiple de 8, pour que
// les coins des carres et les cornes des carres concaves soient des sommets),
// plus, si `splitAtPlanCrossings`, les angles ou deux anneaux d'une jonction
// plate se croisent (RingCrossings et leurs images par la symetrie d'ordre 8).
// A une jonction plate, un sommet de l'anneau du dessus qui coincide avec
// celui du dessous est le MEME sommet : la bande plate y degenere en triangle.
// Une jonction croisee devient ainsi la difference symetrique exacte des deux
// sections, sans recouvrement ; sans le decoupage, les deux triangles du
// secteur croise se recouvrent.
//
// Orientation : normales sortantes (volume signe positif).

struct LatheOptions
{
	int    radialSegments = 48;          // arrondi au multiple de 8 superieur, >= 8
	bool   splitAtPlanCrossings = true;
	double scale = 1.0;                  // unites monde par partie
};

// Reunion de deux jeux d'angles, croissante dans [0, 2 pi), chaque jeu
// dedoublonne a kFluteAngleTolerance : un angle de `secondary` a moins de cette
// tolerance d'un angle de `primary` est ecarte, et l'angle de `primary` reste
// exact (une arete de cannelure n'est jamais remplacee par un croisement).
std::vector<double> MergeAngles (std::vector<double> primary, const std::vector<double> &secondary);

// Angles d'echantillonnage, croissants dans [0, 2 pi) : grille reguliere ou
// grille des cannelures, puis angles de croisement (MergeAngles).
std::vector<double> LatheAngles (const ColumnProfile &profile, const LatheOptions &options);

// Leve std::invalid_argument si le profil a moins de deux sommets.
std::unique_ptr<Mesh> LatheColumnProfile (const ColumnProfile &profile, const LatheOptions &options);

} // namespace classical
} // namespace cgarchi
