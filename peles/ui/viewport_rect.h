#pragma once

namespace Rml
{
class Element;
}

namespace peles
{
namespace ui
{

// Rectangle dans le repere de RmlUi : origine EN HAUT A GAUCHE, unites pixels
// du tampon de dessin.
struct ViewportRect
{
	int x      = 0;
	int y      = 0;
	int width  = 0;
	int height = 0;
};

// Rectangle dans le repere d'OpenGL : origine EN BAS A GAUCHE. Le type est
// distinct de ViewportRect parce que les deux ne sont pas interchangeables ;
// les confondre produit une vue retournee que rien ne signale.
struct GlViewport
{
	int x      = 0;
	int y      = 0;
	int width  = 0;
	int height = 0;
};

// ⚠ LA CONVERSION DE REPERE VIT ICI, ET NULLE PART AILLEURS. Un second site se
// desynchroniserait du premier sans qu'aucun test ne le dise.
GlViewport ToGlViewport (const ViewportRect& rect, int framebufferHeight);

// ---------------------------------------------------------------------------
//  REPERE FENETRE -> REPERE DE LA VUE
// ---------------------------------------------------------------------------
//
// ⚠ LE PIEGE QU'ON NE VOIT PAS A L'OEIL, ET LE SEUL SITE OU IL EST EVITE.
// `Ctrackball::set_dimensions` et `mouse_move` attendent des coordonnees dans le
// repere de la VUE. L'hote historique n'avait rien a convertir : son canvas
// ETAIT la fenetre de rendu, donc `event.GetX()` convenait. Ici glfw donne des
// coordonnees FENETRE et la vue est un rectangle du cadre.
//
// Omettre cette soustraction produit une orbite qui SAUTE et un picking DECALE
// DE LA LARGEUR DU PANNEAU GAUCHE -- et aucun pixel ne parait faux. C'est
// pourquoi les deux consommateurs -- l'arbitre d'entrees et le vidage de rayon
// -- passent par cette fonction et non par une soustraction ecrite chez eux :
// un seul site a corriger, un seul site a neutraliser pour eprouver le
// detecteur.
// ⚠ EN FLOTTANTS, ET C'EST LE VIDAGE DE RAYON QUI L'IMPOSE : le pixel CENTRAL
// d'un rectangle de largeur impaire n'est pas entier, et l'arrondir ferait
// echouer le critere de colinearite pour une raison qui n'est pas celle qu'il
// mesure. L'arbitre, lui, arrondit a l'entier le plus proche -- le trackball ne
// prend que des entiers.
void WindowToView (const ViewportRect& rect, float windowX, float windowY, float& viewX,
                   float& viewY);

// ⚠ LES DEUX ECHECS NE SONT PAS DE MEME NATURE, ET LES CONFONDRE DERRIERE UN
// SEUL `false` REND LE REPLI MUET.
//
//   ElementMissing  : FAUTE DE CONFIGURATION. Le document est une donnee
//                     d'execution, editable sans recompiler : une faute de
//                     frappe dans l'identifiant se traite au meme rang qu'un
//                     repertoire d'assets introuvable, pas par un repli.
//   DegenerateSize  : PAS UNE FAUTE. L'element existe et n'occupe aucune
//                     surface : il n'y a rien a dessiner, et surtout pas en
//                     plein cadre -- un plein cadre serait indiscernable du
//                     succes tant que la vue occupe tout le cadre.
enum class ViewportRectStatus
{
	Ok,
	ElementMissing,
	DegenerateSize
};

// Lit la geometrie RESOLUE de l'element d'identifiant `id` sous `root`.
//
// ⚠ On lit le layout de RmlUi, on ne le recalcule pas : le C++ ne connait
// aucune largeur de panneau. L'appel doit suivre Rml::Context::Update(), sans
// quoi la geometrie lue est celle de l'image precedente.
//
// `rect` n'est ecrit que sur Ok. L'appelant decide de la suite, cette fonction
// n'impose aucun repli.
ViewportRectStatus ReadViewportRect (Rml::Element* root, const char* id, ViewportRect& rect);

} // namespace ui
} // namespace peles
