#pragma once

#include <vector>

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  LES BORNES D'UN PANNEAU REDIMENSIONNE PAR UN SEPARATEUR
// ---------------------------------------------------------------------------
//
// Un separateur regle l'etendue d'UN panneau -- largeur pour les panneaux
// gauche et droit, hauteur pour le panneau bas -- et c'est la vue 3D qui paie
// la difference. Le panneau et la vue se partagent donc une etendue fixe,
// `sharedExtent`, que l'appelant LIT dans la mise en page resolue : etendue du
// panneau plus etendue de la vue, le long de l'axe du separateur.
//
// ⚠ AUCUNE LARGEUR DE PANNEAU ICI. Les tailles par defaut vivent dans la
// feuille de style et nulle part ailleurs ; ce fichier ne connait que les deux
// MINIMUMS, qui sont des regles d'interface et non une mise en page.

// Etendue en dessous de laquelle un separateur ne reduit pas un panneau : un
// panneau plus etroit ne montre plus un libelle lisible.
inline constexpr int kMinPanelExtent = 120;

// Etendue que la vue 3D garde, sur l'axe du separateur, quand un panneau est
// agrandi ou que la fenetre retrecit.
inline constexpr int kMinViewportExtent = 200;

// Position du panneau par rapport a son separateur, dans l'ordre du document.
//
//   PanelBefore : le panneau precede le separateur (panneau gauche). Il
//                 GRANDIT quand le pointeur avance -- vers la droite.
//   PanelAfter  : le panneau suit le separateur (panneau droit, panneau bas).
//                 Il grandit quand le pointeur RECULE -- vers la gauche ou vers
//                 le haut.
enum class SplitterSide
{
	PanelBefore,
	PanelAfter
};

// Etendue demandee par un glisser : l'etendue du panneau au debut du geste,
// corrigee du deplacement du pointeur depuis l'enfoncement. Non bornee.
int DraggedPanelExtent (int startExtent, int pointerDelta, SplitterSide side);

// Etendue retenue pour un panneau qui demande `requested` et partage
// `sharedExtent` avec la vue 3D.
//
// ⚠ LA VUE PASSE AVANT LE PANNEAU QUAND LES DEUX MINIMUMS NE TIENNENT PAS
// ENSEMBLE. Le panneau est d'abord porte a kMinPanelExtent, puis ramene sous
// `sharedExtent - kMinViewportExtent` : dans une fenetre trop petite pour les
// deux, il descend SOUS son minimum -- jusqu'a zero -- plutot que de laisser
// la vue 3D sans surface. Le resultat n'est jamais negatif.
int ClampPanelExtent (int requested, int sharedExtent);

// ---------------------------------------------------------------------------
//  TOUS LES PANNEAUX D'UN AXE, BORNES ENSEMBLE
// ---------------------------------------------------------------------------
//
// Les panneaux gauche et droit partagent la MEME vue. Les borner un par un,
// chacun contre « lui-meme plus la vue », ignore la marge que l'autre pourrait
// ceder : dans une fenetre qui retrecit, le premier descendrait sous son
// minimum pendant que le second garde tout. Ils sont donc bornes en une seule
// passe, sur l'etendue que TOUS les panneaux de l'axe et la vue se partagent.

struct AxisPanel
{
	// Etendue courante, lue dans la mise en page.
	int extent = 0;

	// Etendue demandee par un glisser ; -1 : panneau NON REGLABLE -- taille de
	// la feuille de style, ou glisser en cours -- qui garde `extent`.
	int requested = -1;
};

// Etendues retenues, dans l'ordre de `panels`. Un panneau non reglable rend
// son etendue courante.
//
// REGLE : chaque panneau reglable vise max(demande, kMinPanelExtent). S'il en
// resulte moins de kMinViewportExtent pour la vue, le manque est pris :
//   1. sur les panneaux reglables AU-DESSUS de leur minimum, dans l'ordre de
//      `panels` -- le premier cede jusqu'a kMinPanelExtent avant que le
//      suivant ne cede quoi que ce soit ;
//   2. SEULEMENT SI CELA NE SUFFIT PAS -- la somme des minimums et de la vue
//      minimale ne tient plus -- sur les memes panneaux, sous leur minimum et
//      jusqu'a zero, dans le meme ordre.
// Ce qui manque encore est perdu par la vue : les panneaux non reglables ne
// cedent rien.
std::vector<int> ClampAxisExtents (const std::vector<AxisPanel>& panels, int sharedExtent);

// ETENDUE QUE LES PANNEAUX D'UN AXE ET LA VUE SE PARTAGENT, depuis ce que la
// mise en page a resolu : somme des etendues des panneaux, plus celle de la vue,
// moins le DEBORDEMENT du conteneur.
//
// ⚠ LE DEBORDEMENT N'EST PAS UN CAS D'ECOLE. Les panneaux ne retrecissent pas
// (`flex: 0 0 <base>`) et la vue ne descend pas sous zero : dans une fenetre
// trop petite, la vue vaut 0 et l'excedent sort du conteneur. Sans la
// correction, l'etendue partagee serait surestimee de cet excedent.
//
// `occupied` : somme des boites de MARGE des enfants du conteneur le long de
// l'axe ; `available` : boite de contenu du conteneur le long de l'axe.
int SharedAxisExtent (int panelsExtent, int viewportExtent, int occupied, int available);

} // namespace core
} // namespace peles
