#pragma once

// ============================================================================
//  Operations sur des jeux de contours FERMES
// ============================================================================
//
// Trois chantiers demandaient les memes primitives sans se connaitre : le
// support du texte 3D (`text_extrude.cpp`, qui les avait ecrites en local), la
// plaque silhouette et le biseau des aretes. Elles sont ici, et les deux
// suivants n'auront pas a les reecrire.
//
// PERIMETRE : des contours FERMES, dans le repere monde final -- l'entree comme
// la sortie de `extrude_contours.h`. Pour epaissir des polylignes OUVERTES,
// c'est `stroke_contours.h` : ce n'est pas la meme operation, et l'y confondre
// donne n'importe quoi des qu'un trace se replie sur lui-meme.
//
// ⚠ ORIENTATION, le point qui ne se devine pas. `offsetContours` lit le jeu de
// contours comme une REGION : les enveloppes et les contre-formes doivent
// tourner en sens OPPOSES, faute de quoi un trou se dilate au lieu de se
// resserrer. C'est exactement la convention que rend Clipper2 -- donc celle que
// portent deja `text_to_contours` et `svg.contours`, les deux producteurs du
// catalogue (cf. la note d'orientation de `shapes/extrude.cpp`). Un contour
// fabrique a la main doit s'y conformer.
//
// ============================================================================

#include <vector>

#include <cgmath/cgmath.h>

#include "extrude_contours.h"   // ExtrudeContour
#include "stroke_contours.h"    // StrokeJoin -- meme vocabulaire Clipper2 / SVG

// Aire signee d'un contour ferme (l'arete de fermeture est implicite).
// Positive dans le sens trigonometrique. C'est la mesure qui dit le SENS d'un
// contour, donc son role -- matiere ou vide -- dans un remplissage NonZero.
float contourSignedArea (const std::vector<Vector2f>& pts);

// Emprise du jeu de contours. Rend false -- et ne touche a rien -- quand il n'y
// a aucun point : une emprise vide n'a pas de valeur par defaut honnete.
bool contoursBBox (const std::vector<ExtrudeContour>& contours,
                   float& x0, float& y0, float& x1, float& y1);

// Rectangle a coins eventuellement arrondis, trace dans le sens TRIGONOMETRIQUE
// (aire signee positive) ; l'appelant le retourne s'il lui faut l'autre sens.
// `radius` est borne a la moitie du plus petit cote. Zero, ou moins d'un
// segment par coin, rend les quatre sommets vifs.
std::vector<Vector2f> roundedRectContour (float x0, float y0, float x1, float y1,
                                          float radius, int cornerSegments = 6);

// Disque, trace dans le sens TRIGONOMETRIQUE (aire signee positive) ; l'appelant
// le retourne s'il lui faut un trou. `segments` echantillons sur le tour.
//
// C'est la premiere piece du generateur de contours PRIMITIFS que trois
// ameliorations attendent (l'anneau d'un porte-clefs, le pochoir, les oreilles
// de fixation). Elle est ecrite ici, et non dans le noeud qui s'en sert, parce
// qu'un cercle n'appartient a aucun d'eux.
std::vector<Vector2f> circleContour (float cx, float cy, float radius, int segments = 32);

// Decalage d'une region fermee : `delta` > 0 DILATE, < 0 retrecit. Les contours
// rendus portent la convention d'orientation de Clipper2 (enveloppes en sens
// positif, trous a l'inverse), soit ce qu'attend un remplissage NonZero.
//
// L'operation termine par une UNION : deux formes voisines dont les halos se
// recouvrent ressortent fondues en une seule region. C'est ce qui rend la
// « plaque silhouette » praticable sans recollage -- et c'est aussi ce qui la
// rend PIEGEUSE, d'ou le compte ci-dessous.
//
// `topLevelCount`, optionnel : nombre de contours de PREMIER NIVEAU du resultat,
// c'est-a-dire de morceaux distincts. Un halo trop etroit pour relier les
// lettres en rend un par lettre, et la piece sort en morceaux -- geometriquement
// correct, pratiquement inutilisable. C'est la seule facon de le SAVOIR avant
// l'impression, et la raison pour laquelle ce compte n'est pas laisse a
// l'appelant : il demande un PolyTree, que la sortie plate ne porte pas.
//
// Rend un jeu vide quand l'entree est vide, ou quand le retrecissement a tout
// consomme (`delta` negatif plus grand que la demi-epaisseur de la matiere) --
// ce n'est pas une panne, c'est le resultat.
std::vector<ExtrudeContour> offsetContours (const std::vector<ExtrudeContour>& in,
                                            float delta, StrokeJoin join,
                                            float miterLimit = 2.f,
                                            int* topLevelCount = nullptr);

// --- BOOLEENS 2D ------------------------------------------------------------
//
// Les trois operations d'ensemble sur des regions fermees, au-dessus de
// Clipper2. Elles rendent toutes la convention d'orientation de Clipper2 --
// enveloppes en sens positif, trous a l'inverse --, soit exactement ce qu'attend
// un remplissage NonZero, donc `shape.extrude` sans renormalisation.
//
// ⚠ CE QU'ELLES NE SONT PAS. Un booleen 2D n'est pas un booleen 3D : il opere
// sur l'EMPRISE, pas sur le volume. Le depot n'a aucun booleen sur maillages, et
// ces fonctions n'en tiennent pas lieu -- elles servent a composer la region
// AVANT extrusion, ce qui suffit tant que la piece est un empilement 2,5D.
//
// ⚠ POURQUOI PAS pour la couronne d'une arete profilee, alors que la difference
// semblait faite pour cela : une seconde passe Clipper2 recalcule la frontiere
// exterieure et recolle au passage des micro-aretes que la paroi voisine a
// gardees. Mesure sur les quatre polices difficiles du catalogue : quatre a
// douze aretes non partagees, donc une peau trouee. La couronne passe donc par
// la regle de remplissage (cf. extrude_profiled.h). Une soustraction qui
// alimente une tessellation INDEPENDANTE, elle, n'a pas ce probleme.

// Reunion : tout ce qui est dans `a` ou dans `b`. Les recouvrements fondent.
// Pour normaliser une region SEULE -- fondre ses propres recouvrements, fixer
// son orientation -- `offsetContours (in, 0)` fait deja exactement cela.
std::vector<ExtrudeContour> unionContours (const std::vector<ExtrudeContour>& a,
                                           const std::vector<ExtrudeContour>& b);

// Difference : ce qui est dans `a` et PAS dans `b`. L'ORDRE compte, et c'est la
// seule des trois dont ce soit le cas -- `a` est la matiere, `b` l'emporte-piece.
//
// C'est la region « plaque moins texte » : le capot d'un socle grave (A2), ou
// celui d'un socle a coque unique etanche (A4), la ou l'empilement de deux
// coques laisse une membrane interne.
std::vector<ExtrudeContour> differenceContours (const std::vector<ExtrudeContour>& a,
                                                const std::vector<ExtrudeContour>& b);

// Intersection : ce qui est a la fois dans `a` et dans `b`. Rend un jeu VIDE
// quand les deux regions ne se touchent pas -- ce n'est pas une panne, c'est le
// resultat, et c'est aussi la facon la moins chere de REPONDRE a « ces deux
// formes se touchent-elles ? », question qu'un porte-clef pose de lui-meme.
std::vector<ExtrudeContour> intersectionContours (const std::vector<ExtrudeContour>& a,
                                                  const std::vector<ExtrudeContour>& b);
