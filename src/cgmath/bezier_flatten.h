#pragma once

// ============================================================================
//  Aplatissement adaptatif de Beziers 2D
// ============================================================================
//
// Subdivision de De Casteljau a t = 0.5, arretee par un critere de PLATITUDE
// mesure sur la corde, avec un garde-fou de profondeur. Le pendant Bezier de
// Arc::tessellateAdaptive (geometry.h) : meme role, meme famille.
//
// Quatre FONCTIONS LIBRES et sans etat, et non une classe : la forme est
// dictee par l'usage. Les appelants (contours SVG, contours de glyphes)
// n'aplatissent pas UNE courbe mais un FLUX de dizaines a centaines de
// segments courts, accumules dans un seul et meme contour. Les fonctions
// ajoutent donc dans un `out` fourni par l'appelant, et n'emettent PAS le
// point de depart -- charge a l'appelant de pousser le premier point du
// contour une seule fois, ce qui evite toute jointure dupliquee :
//
//     std::vector<Vector2f> contour;
//     contour.push_back (p0);                        // depart, une seule fois
//     flattenQuadratic (contour, p0, c, p1, tol);    // -> ... p1
//     flattenCubic     (contour, p1, c1, c2, p2, tol); // -> ... p2
//
// Une classe imposerait un objet et une allocation par segment, puis un
// recollement des jointures. CurveBezier (curve_bezier.h) reste la bonne
// reponse pour une courbe DURABLE avec une identite (degre quelconque,
// echantillonnage uniforme en t) ; ce n'est pas ce regime-ci.
//
// Vector2f et non Vector2d : les unites de police et les coordonnees SVG
// tiennent tres largement dans la mantisse de 24 bits d'un float, et c'est le
// type de ExtrudeContour::pts en aval -- aucune couture float/double.
//
// La TOLERANCE est homogene a une longueur, dans les MEMES unites que les
// points passes. Aplatir APRES la mise a l'echelle finale donne donc une
// finesse previsible ; aplatir avant la rend dependante de l'echelle de la
// source.
//
// ============================================================================

#include <vector>

#include "TVector2.h"

// Vrai quand le segment s'ecarte de sa corde de moins que `tol`, donc quand il
// peut etre remplace par cette corde. Les deux mesures sont homogenes entre
// elles : le vecteur teste vaut ~4x l'ecart maximal de la courbe a sa corde,
// dans les deux cas.
bool flatEnoughQuadratic (const Vector2f& p0, const Vector2f& c,
                          const Vector2f& p1, float tol);
bool flatEnoughCubic     (const Vector2f& p0, const Vector2f& c0,
                          const Vector2f& c1, const Vector2f& p1, float tol);

// Ajoutent dans `out` les points d'arrivee successifs de la subdivision, donc
// `p1` en dernier. N'emettent PAS `p0`. `depth` est le garde-fou de recursion,
// laisse a sa valeur par defaut par les appelants.
void flattenQuadratic (std::vector<Vector2f>& out,
                       const Vector2f& p0, const Vector2f& c,
                       const Vector2f& p1, float tol, int depth = 0);
void flattenCubic     (std::vector<Vector2f>& out,
                       const Vector2f& p0, const Vector2f& c0,
                       const Vector2f& c1, const Vector2f& p1, float tol,
                       int depth = 0);
