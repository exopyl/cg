#pragma once

#include <string_view>

namespace cgarchi
{

// Texte UTF-8 de src/cgarchi/data/temple_hephaistos.json, embarque a la build
// (embed_file.cmake). Valide pour la duree du programme.
std::string_view TempleHephaistosJson ();

} // namespace cgarchi
