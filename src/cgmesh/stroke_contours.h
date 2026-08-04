#pragma once

// ============================================================================
//  Trace au trait -> contours fermes
// ============================================================================
//
// Epaissit des polylignes OUVERTES d'une largeur donnee et rend les contours
// fermes qui en resultent, prets pour la tessellation puis l'extrusion
// (extrude_contours.h).
//
// C'est la seule operation qui ait un sens sur un trace : une polyligne n'a pas
// de surface, et la « remplir » en la refermant d'office donne n'importe quoi des
// qu'elle se replie sur elle-meme -- une courbe du dragon ainsi remplie degenere
// en damier, faute de pouvoir designer un interieur.
//
// Extrait de import_svg.cpp, ou il etait local : le meme besoin existe pour les
// traces qui ne viennent pas d'un fichier SVG (L-systemes), et l'interface ne
// mentionne donc plus nanosvg. C'est a l'appelant de traduire ses propres
// conventions vers StrokeJoin / StrokeCap.

#include <array>
#include <vector>

// Traitement des coins et des extremites, calque sur les possibilites de
// Clipper2 -- et, ce n'est pas un hasard, sur `stroke-linejoin` /
// `stroke-linecap` de SVG, qui expriment la meme chose.
enum class StrokeJoin { Round, Miter, Bevel };
enum class StrokeCap  { Round, Square, Butt };

// `polylines` : suites de points, NON fermees. Une polyligne de moins de deux
// points est ignoree -- un point isole n'a aucune direction, donc aucune
// epaisseur.
//
// `width` est la largeur TOTALE du trait (le rayon vaut la moitie). Une largeur
// nulle ou negative rend un resultat vide.
//
// Les recouvrements sont resolus : Clipper2 termine son offset par une union, ce
// qui rend cette voie praticable sur un trace qui se touche lui-meme des milliers
// de fois. Les contours rendus portent la convention d'orientation de Clipper2 --
// enveloppes en sens positif, trous en sens inverse -- soit exactement ce
// qu'attend un remplissage NonZero.
std::vector<std::vector<std::array<float, 2>>>
strokeToContours (const std::vector<std::vector<std::array<float, 2>>>& polylines,
                  float width, StrokeJoin join, StrokeCap cap);
