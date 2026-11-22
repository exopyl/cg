#pragma once

//
// Decalage de profondeur du remplissage des surfaces (GL_POLYGON_OFFSET_FILL).
//
// Le remplissage recule pour que les surimpressions dessinees en GL_LINES ou
// GL_POINTS sur la surface (fil de fer, aretes de diagnostic, points) ne s'y
// enfoncent pas : ces primitives ne sont pas decalees.
//
// TERME DE PENTE NUL. Le terme de pente recule chaque face en proportion de sa
// pente en profondeur, donc deux faces non paralleles reculent de quantites
// differentes : en incidence rasante, une face recule de plusieurs centimetres
// et passe derriere une face cachee a quelques millimetres sous elle. Le terme
// constant recule toutes les faces de la meme quantite et ne change jamais leur
// ordre de profondeur.
//
// Ce que le terme constant ne fait pas : proteger le fil de fer en incidence
// rasante, ou l'ecart d'interpolation entre ligne et face croit avec la pente.
// Mesures et arbitrage : debt_sinaia.md, section 8.
//
// Aucune dependance GL : ces valeurs se testent sans contexte.
//
namespace cgre
{

//! Facteur du terme de pente de glPolygonOffset. Doit rester nul (voir plus haut).
inline constexpr float kSurfacePolygonOffsetFactor = 0.0f;

//! Terme constant de glPolygonOffset, en unites de resolution de profondeur.
//! Plus petite valeur, parmi celles mesurees, qui garde le fil de fer d'une face
//! vue de face presque entier ; au-dela, la traversee de la surface par les
//! aretes de faces cachees a quelques millimetres croit fortement.
inline constexpr float kSurfacePolygonOffsetUnits = 8.0f;

} // namespace cgre
