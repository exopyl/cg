#pragma once

#include <string_view>
#include <vector>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Partitions 1D : bornes d'une suite de longueurs, parties egales d'un
//  intervalle, intervalles libres entre des parties centrees
// ============================================================================
//
// Fonctions pures, en double, sans unite. Chaque refus leve
// std::invalid_argument, message prefixe "Partition : " et nommant `field`
// entre apostrophes, suivi de l'indice fautif entre crochets quand il y en a un.
// Aucun operateur de repetition a periode imposee (arrondi d'un nombre de
// repetitions) : il arrive avec son premier consommateur (D3-50).

// Bornes cumulees, somme courante de gauche a droite : b[0] = start,
// b[k + 1] = b[k] + lengths[k] (lengths.size () + 1 bornes). Leve si start
// n'est pas fini, si une longueur n'est pas finie ou est <= 0, ou si une borne
// n'est pas finie (debordement).
std::vector<double> CumulativeBounds (double start, const std::vector<double> &lengths, std::string_view field);

// n parties egales de [lo, hi] (n + 1 bornes) : b[0] = lo et b[n] = hi
// exactement, b[k] = ((n - k) lo + k hi) / n entre les deux. Symetrique au bit
// pres, au signe d'un zero pres : EqualParts (-hi, -lo, n) est l'oppose renverse
// de EqualParts (lo, hi, n), sauf qu'une borne interieure nulle vaut +0 des
// deux cotes (x + (-x) = +0 en arrondi au plus pres) ; les bornes extremes sont
// lo et hi eux-memes (EqualParts (-0.3, -0.0, n).back () vaut -0). Pour n = 2,
// b[1] = (lo + hi) / 2. Leve si n < 1, si une borne n'est pas finie, si
// lo >= hi, ou si une borne calculee n'est pas finie (debordement).
// Evaluation stricte : chaque produit est arrondi en double avant la somme,
// jamais contracte en multiplication-addition fusionnee (FMA), quels que soient
// le compilateur, la cible et ses drapeaux : la symetrie ci-dessus tient donc
// sur toute cible. Les bornes elles-memes sont identiques sous MSVC, GCC et Clang
// avec les drapeaux du depot (sans -ffast-math), sur x86-64 SSE2 et aarch64.
std::vector<double> EqualParts (double lo, double hi, int n, std::string_view field);

struct Interval
{
	double lo = 0.0, hi = 0.0;
};

// Intervalles libres entre des parties de largeur `width` centrees en
// `centres` : [c[k] + width / 2, c[k + 1] - width / 2], k = 0 .. n - 2. Leve si
// width n'est pas finie ou est <= 0, si un centre n'est pas fini, si les
// centres ne sont pas strictement croissants, ou si un intervalle est vide ou
// de longueur nulle (indice de l'intervalle).
std::vector<Interval> GapsBetweenCentred (const std::vector<double> &centres, double width, std::string_view field);

} // namespace classical
} // namespace cgarchi
