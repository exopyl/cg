#pragma once

// Export OBJ du reseau de la microarchitecture gothique : une polyligne par
// courbe, sortie seulement (M1, M3). La topologie (jonctions, croisements) est
// dans le JSON du reseau (NetworkToJson), pas dans l'OBJ.

#include <string>

#include <cgarchi/gothic_microarchitecture.h>

namespace cgarchi::microarch
{

// Pour chaque courbe du reseau : une ligne `o <id>_k<k>[_r]_<i>` (id de la courbe
// de base, element de symetrie, indice i de la courbe dans le reseau, qui rend le
// nom unique ; espaces, caracteres de controle et '#' remplaces par '_'), ses
// sommets `v x y z` a 17 chiffres significatifs, puis une seule ligne
// `l i1 ... im` (indices a partir de 1, sur tout le fichier). Les sommets ne sont
// pas fusionnes aux jonctions. Le point decimal ne depend pas de la locale.
// Echantillonnage par SampleNetwork : au-dela de son plafond (M15), chaine vide
// et erreur sample_limit_exceeded dans `report`.
std::string NetworkToObj (const Network &network, const MicroarchSpec &spec, const SamplingParams &params,
                          MicroarchReport &report);

// NetworkToObj ecrit dans `path`. Rend false, sans exception, si l'echantillonnage
// est refuse ou si le fichier ne peut pas etre ecrit ; dans ce dernier cas, erreur
// export_write_failed sur le pseudo-chemin "export.path".
bool WriteNetworkObj (const Network &network, const MicroarchSpec &spec, const SamplingParams &params,
                      const std::string &path, MicroarchReport &report);

} // namespace cgarchi::microarch
