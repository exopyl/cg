#pragma once

// En-tete interne du module microarchitecture gothique : partage entre le coeur
// et le lecteur JSON, hors de l'API publique.

#include <string>

#include <cgarchi/gothic_microarchitecture.h>

namespace cgarchi::microarch::detail
{

// `loaded` : la specification vient du lecteur JSON, qui a deja signale ses
// valeurs illisibles, ses tableaux vides ou trop longs et ses id vides. Sont
// alors sautes sans diagnostic : une coordonnee ou une hauteur non finie (NaN :
// valeur illisible), un indice de point negatif (courbe non construite), un id
// de niveau negatif (niveau illisible, qui rend inconnue la liste des niveaux),
// un tableau vide et un id vide.
bool ValidateStructure (const MicroarchSpec &spec, MicroarchReport &report, bool loaded);

// Suite d'octets UTF-8 bien formee (RFC 3629 : ni forme trop longue, ni
// substitut, ni point de code au-dela de U+10FFFF).
bool IsValidUtf8 (const std::string &text);

} // namespace cgarchi::microarch::detail
