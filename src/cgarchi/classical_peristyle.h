#pragma once

#include <vector>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Peristyle : implantation des axes des colonnes, sans maillage
// ============================================================================
//
// Repere (D2-5, celui de LayoutColonnade) : X le long de la facade, Y en
// profondeur (facade en Y < 0), Z vertical ; rectangle des axes centre sur
// l'origine. Unites : celles de l'appelant (entraxes en valeur absolue).

enum class PeristyleSide { Front, Right, Back, Left };

struct PeristyleColumn
{
	double x = 0.0, y = 0.0;                     // axe de la colonne
	// Cote auquel appartient la colonne ; un angle appartient au cote qu'il ouvre.
	PeristyleSide side = PeristyleSide::Front;
	bool corner = false;
};

struct PeristyleLayout
{
	int front = 0, flank = 0;                    // colonnes par facade, par flanc, angles compris
	double frontSpacing = 0.0, flankSpacing = 0.0;
	// Entraxe entre un axe d'angle et son voisin ; egal a l'entraxe courant sans contraction.
	double frontCornerSpacing = 0.0, flankCornerSpacing = 0.0;
	double halfWidth = 0.0, halfDepth = 0.0;     // demi-distances des axes d'angle, en X et en Y
	std::vector<PeristyleColumn> columns;        // 2 (front + flank) - 4 colonnes
};

// Reglages nommes du peristyle, avec la travee d'angle de chaque cote.
struct PeristyleSettings
{
	int    front = 0, flank = 0;                      // colonnes par cote, angles compris
	double frontSpacing = 0.0, flankSpacing = 0.0;    // entraxe courant
	double frontCornerSpacing = 0.0;                  // entraxe d'angle, facades
	double flankCornerSpacing = 0.0;                  // entraxe d'angle, flancs
};

// Borne d'interface, pas une valeur du domaine : elle exclut le debordement de
// 2 (front + flank) - 4 en int et borne le resultat a 4 092 colonnes.
constexpr int kPeristyleMaxColumnsPerSide = 1024;

// Axes d'un peristyle rectangulaire de `front` colonnes en facade et a l'arriere,
// `flank` colonnes sur chaque flanc, angles compris ; entraxe `frontSpacing` le
// long des facades, `flankSpacing` le long des flancs.
//
// Une rangee de n colonnes a l'entraxe s a ses axes en (i - (n - 1) / 2) s,
// i = 0 .. n - 1 : la meme expression que LayoutColonnade. (i - (n - 1) / 2)
// est exact (demi-entier, n <= kPeristyleMaxColumnsPerSide), seule la
// multiplication par s arrondit, et aucune addition ne la suit : la facade egale
// donc au bit pres la colonnade de meme entraxe, sur tout compilateur, et les
// axes opposes sont des negations exactes. halfWidth = (front - 1) frontSpacing
// / 2, halfDepth = (flank - 1) flankSpacing / 2.
//
// Ordre de `columns` : sens trigonometrique vu de +Z, depuis l'angle
// (-halfWidth, -halfDepth) ; facade, flanc droit, arriere, flanc gauche. Chaque
// angle figure une seule fois, comme premiere colonne de son cote.
//
// Leve std::invalid_argument, dont le message nomme l'argument, si front ou
// flank est hors de [2, kPeristyleMaxColumnsPerSide], si un entraxe n'est pas
// fini et strictement positif, ou si un entraxe fini donne une demi-distance
// non finie ou deux axes voisins egaux.
// frontCornerSpacing et flankCornerSpacing du resultat valent les entraxes.
PeristyleLayout LayoutPeristyle (int front, int flank, double frontSpacing, double flankSpacing);

// Peristyle a contraction (ou dilatation) d'angle donnee (registre TE6) : une
// seule travee d'angle par bout de rangee, la meme aux deux bouts et sur les
// deux rangees opposees. Une rangee de n colonnes, d'entraxe courant s et
// d'entraxe d'angle a != s, a ses axes interieurs en (i - (n - 1) / 2) s,
// i = 1 .. n - 2, au bit pres ceux de LayoutPeristyle (4 arguments), et ses
// deux axes d'angle en +-((n - 3) / 2 s + a), negations exactes l'un de
// l'autre. Une rangee ou a == s est celle de LayoutPeristyle (4 arguments), au
// bit pres. halfWidth et halfDepth sont les demi-distances des axes d'angle ;
// meme ordre de `columns`.
//
// Memes refus que LayoutPeristyle (4 arguments), plus, le message nommant le
// champ : un entraxe d'angle non fini ou <= 0 ; a != s avec n < 4 (la rangee
// n'a que des travees d'angle) ; un axe d'angle non fini ou confondu en double
// avec son voisin.
PeristyleLayout LayoutPeristyle (const PeristyleSettings &settings);

} // namespace classical
} // namespace cgarchi
