#pragma once

#include <string_view>

namespace cgarchi
{

// Textes UTF-8 des documents des monuments, embarques a la build
// (embed_file.cmake) : src/cgarchi/data/temple_hephaistos.json et
// src/cgarchi/data/temple_ares.json. Valides pour la duree du programme.
std::string_view TempleHephaistosJson ();
std::string_view TempleAresJson ();

} // namespace cgarchi
