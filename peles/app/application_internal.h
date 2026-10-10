#pragma once

// ---------------------------------------------------------------------------
//  CE QUE LES QUATRE UNITES D'Application PARTAGENT, ET RIEN DE PLUS
// ---------------------------------------------------------------------------
//
// ⚠ CET EN-TETE EST INTERNE A app/, ET IL EST VOLONTAIREMENT MINUSCULE. Il
// existe parce que la classe Application est repartie sur quatre unites de
// traduction -- la creation et les huit phases, la scene et les entrees, les
// instruments de boucle, les modes « vider et sortir » -- et que deux d'entre
// elles nomment le meme element de document.
//
// ⚠ IL N'EST PAS UN FOURRE-TOUT. Tout ce qui n'a qu'un seul lecteur reste dans
// l'espace anonyme de son unite : le nom du modele de donnees, le puits de
// journalisation, le seuil clic/glisser. Y remonter une constante a un seul
// lecteur ferait recompiler quatre unites pour un changement qui n'en concerne
// qu'une.

namespace peles
{
namespace app
{

// Identifiant de l'element dont la geometrie resolue donne le rectangle de la
// vue 3D. ⚠ Le C++ ne connait aucune largeur de panneau : il LIT ce que RmlUi a
// calcule, il ne le recalcule pas.
inline constexpr const char* kViewportElementId = "viewport";

} // namespace app
} // namespace peles
