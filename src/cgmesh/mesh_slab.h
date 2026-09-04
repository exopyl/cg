#pragma once
//
//  Emprise d'un maillage DANS UNE TRANCHE, et non sur toute sa hauteur.
//
// « Jusqu'ou va ce modele ? » n'a pas de reponse unique : un L va jusqu'a la
// pointe de son pied en bas, et pas plus loin que son montant a mi-hauteur. La
// boite englobante ne rend que la premiere, ce qui suffit a cadrer une camera
// mais pas a poser une piece CONTRE la matiere -- une fixation placee a
// mi-hauteur d'apres l'emprise totale deborde alors dans le vide.
//
// D'ou cette mesure : l'etendue en X de ce que le maillage occupe entre deux
// plans y = yLo et y = yHi.
//
// ⚠ POURQUOI LA SURFACE SUFFIT, et qu'il n'y a rien a remplir. Le point le plus
// a gauche de (solide ∩ tranche) appartient au bord de cette intersection, donc
// a la surface du solide -- une section plane a son bord sur la peau. Parcourir
// les triangles et retenir, pour chacun, ce qui tombe dans la tranche donne
// donc l'etendue EXACTE, sans reconstruire aucune section.
//
class Mesh;

// Faux -- et `xMin`/`xMax` intacts -- quand RIEN du maillage n'est dans la
// tranche. Ce n'est pas une panne : c'est la reponse, et c'est celle qui doit
// arreter l'appelant plutot que de le laisser poser une piece dans le vide.
//
// `yLo` et `yHi` sont remis dans l'ordre si besoin. Une tranche d'epaisseur
// nulle est licite : c'est la section a une cote, et elle rend l'etendue des
// aretes qui la traversent.
bool meshSlabExtentX (const Mesh& mesh, float yLo, float yHi, float& xMin, float& xMax);
