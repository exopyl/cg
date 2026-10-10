#pragma once

#include <filesystem>
#include <string>

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  L'IDENTITE D'UN FICHIER EST UN std::filesystem::path, JAMAIS UNE std::string
// ---------------------------------------------------------------------------
//
// ⚠ SOUS WINDOWS, `path::string()` CONVERTIT LE CHEMIN NATIF WIDE VERS LA PAGE
// DE CODES ANSI DE LA MACHINE, et cette conversion a trois defauts qui
// interdisent de s'en servir pour designer un fichier :
//   -- elle DEPEND DE LA MACHINE : les octets rendus changent avec la page de
//      codes active, donc une sortie destinee a etre comparee cesse d'etre
//      comparable ;
//   -- elle est DESTRUCTIVE : un nom non representable dans cette page devient
//      une suite de '?', et la chaine obtenue NE DESIGNE PLUS LE FICHIER ;
//   -- elle LEVE sur une sequence non convertible, exception qui remonte
//      jusqu'au rattrapage de dernier recours de main() sans nommer le
//      repertoire en cause.
//
// D'ou la regle tenue par peles_core : le chemin STOCKE est un
// std::filesystem::path, et une chaine n'est produite que pour AFFICHER.

// Chaine d'AFFICHAGE d'un chemin, en UTF-8.
//
// ⚠ UTF-8 ET NON LA PAGE DE CODES ANSI, pour deux raisons independantes :
// RmlUi attend de l'UTF-8 sur tout texte qu'on lui confie, et les vidages
// --dump-tree / --dump-files sont des ORACLES compares octet pour octet -- des
// octets dependant de la page de codes de la machine les rendraient
// incomparables d'une machine a l'autre.
//
// ⚠ NE LEVE PAS. Une sequence que la conversion refuse produit un repli ou
// chaque unite non-ASCII devient '?' : une chaine d'affichage degradee est un
// defaut d'affichage, alors qu'une exception jetee depuis l'enumeration d'un
// repertoire fait tomber le vidage entier.
std::string PathToUtf8 (const std::filesystem::path& path);

} // namespace core
} // namespace peles
