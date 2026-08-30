#pragma once
//
//  Signature structurelle d'un noeud.
//
// La signature est ce qui indexera le cache : deux noeuds de meme type, aux
// memes parametres semantiques, alimentes par les memes sorties amont, ont la
// meme signature donc le meme resultat. Ce qui n'entre PAS dans la signature
// compte autant que ce qui y entre -- un parametre non semantique et une
// position d'ecran ne doivent invalider aucun calcul.
//
// Ce module ne connait pas le cache, et c'est deliberé : une signature
// incomplete rend un resultat faux sans jamais planter, et c'est le seul defaut
// de cette couche qui ne se voit pas a l'execution. Il se teste donc seul,
// avant qu'un cache existe pour en masquer les effets.
//
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "graph.h"
#include "param_set.h"

namespace cggraph
{

using Hash = std::uint64_t;

// 0 ne designe jamais une signature valide : c'est l'absence de signature.
static const Hash kNoSignature = 0;

Hash HashBytes (const void *data, std::size_t size, Hash seed);
Hash HashString (const std::string &text, Hash seed);
Hash HashCombine (Hash left, Hash right);

// Parcourt les entrees semantiques dans leur ordre de declaration. Une entree
// Driven hache sa source et non sa valeur : l'index du cache n'aura pas a
// changer le jour ou elle deviendra evaluable.
Hash HashParamSet (const ParamSet &params);

// Memo valable pour un etat donne du graphe, jamais au-dela : un parametre
// modifie entre deux evaluations le rendrait faux.
using SignatureMemo = std::unordered_map<NodeId, Hash>;

// Rend kNoSignature pour un noeud inconnu.
Hash Signature (const Graph &graph, NodeId id, SignatureMemo &memo);
Hash Signature (const Graph &graph, NodeId id);

} // namespace cggraph
