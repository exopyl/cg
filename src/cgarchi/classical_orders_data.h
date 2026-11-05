#pragma once

#include <string_view>

namespace cgarchi
{

// Texte UTF-8 de src/cgarchi/data/cinq_ordres.json, embarque a la build
// (embed_file.cmake). Valide pour la duree du programme.
std::string_view ClassicalOrdersJson ();

} // namespace cgarchi
