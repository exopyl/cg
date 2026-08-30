#pragma once
//
//  Identite de source -- ce qu'un noeud source verse a sa signature.
//
// Un noeud source depend d'un contenu qui n'apparait dans aucun de ses
// parametres : le CHEMIN ne dit rien du fichier. Sans identite de contenu, le
// cache resservirait l'ancien resultat apres modification du fichier, sans
// jamais planter -- la seule voie qui mene encore le cache du graphe a un
// resultat faux.
//
// Deux regimes, et ils repondent a deux questions differentes :
//
//  - fichier NATIF relu a chaque calcul : mtime + taille, un stat, hash sur
//    demande. Hacher pour decider s'il faut relire OBLIGE a lire, donc annule
//    exactement l'economie que le cache existe pour produire. Le stat est la
//    porte bon marche, le hash la confirmation -- payee quand on a une raison de
//    douter de l'horloge, sur un fichier qu'on allait lire ;
//  - ressource EN MEMOIRE : hash du buffer, toujours, jamais un stat. Il n'y a
//    ni fichier ni horloge a interroger -- sous WebAssembly le chemin temporaire
//    est souvent supprime avant le premier calcul, et le mtime d'un systeme de
//    fichiers en memoire n'a pas la semantique attendue.
//
#include <cstddef>
#include <cstdint>
#include <string>

namespace cggraph_nodes
{

struct FileIdentity
{
	bool exists = false;
	std::uint64_t mtime = 0;   // secondes
	std::uint64_t size = 0;
};

// N'ouvre PAS le fichier.
FileIdentity StatFile (const std::string &path);

// Ouvre et lit le fichier entier. Rend false s'il est illisible.
bool HashFile (const std::string &path, std::uint64_t &hash);

std::uint64_t HashBuffer (const void *data, std::size_t size);

// Formes textuelles versees au parametre de signature. Elles sont distinctes
// par construction : une identite par stat et une identite par hash ne doivent
// pas pouvoir se confondre, sans quoi passer de l'une a l'autre laisserait le
// cache servir l'entree de l'autre regime.
std::string StatKey (const FileIdentity &identity);
std::string HashKey (std::uint64_t hash);

// Les seize chiffres hexadecimaux, sans prefixe : ce qui entre dans un NOM DE
// FICHIER. HashKey ne convient pas la -- son prefixe porte un deux-points, que
// Windows refuse dans un nom.
std::string HexDigits (std::uint64_t hash);

} // namespace cggraph_nodes
