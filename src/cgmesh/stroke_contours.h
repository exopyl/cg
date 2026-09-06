#pragma once

// ============================================================================
//  Trace au trait -> contours fermes
// ============================================================================
//
// Epaissit des traces d'une largeur donnee et rend les contours fermes qui en
// resultent, prets pour la tessellation puis l'extrusion (extrude_contours.h).
// Deux familles, selon que le trace est OUVERT -- un ruban a extremites -- ou
// FERME -- un anneau.
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

// Variante FERMEE : chaque contour est trace des deux cotes de sa boucle, ce qui
// rend un ANNEAU -- bord exterieur decale de +width/2, bord interieur de
// -width/2 -- la ou strokeToContours rend un ruban a extremites. C'est le trait
// d'une forme fermee au sens SVG.
//
// L'arete de fermeture est IMPLICITE : ne PAS repeter le premier point en fin de
// contour. C'est la convention d'ExtrudeContour (extrude_contours.h) et celle que
// Clipper2 attend pour un chemin ferme ; un point repete y serait de toute facon
// retire.
//
// Un contour de moins de TROIS points est ignore : une boucle a deux points n'a
// pas d'interieur, donc pas d'anneau -- son trait est un ruban, et releve de
// strokeToContours.
//
// Pas de StrokeCap : une boucle fermee n'a pas d'extremite.
//
// L'orientation d'entree est indifferente, le decalage etant symetrique. Les
// TROUS sont traces sans traitement particulier : chaque contour est decale
// independamment, si bien qu'un exterieur et ses trous recoivent chacun leur
// anneau -- c'est ce que fait le SVG, qui trace chaque sous-chemin.
//
// Meme convention de sortie que ci-dessus : orientation Clipper2, prete pour
// NonZero.
std::vector<std::vector<std::array<float, 2>>>
strokeClosedToContours (const std::vector<std::vector<std::array<float, 2>>>& contours,
                        float width, StrokeJoin join);
