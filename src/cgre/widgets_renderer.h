#pragma once

//
// Widgets de scene : ce que le moteur dessine EN PLUS des maillages.
//
// Ce fichier exposait dix-huit fonctions ; deux seulement avaient un appelant
// (`repere_draw` et `draw_grid`, depuis sinaia/wxOpenGLCanvas.cpp). Les seize
// autres ont ete retirees, et il vaut la peine de dire pourquoi : plusieurs
// d'entre elles -- draw_sphere, draw_teapot, draw_point, draw_plane, draw_arc --
// avaient une signature publique, un corps qui empilait et depilait l'etat GL,
// et un appel a GLUT COMMENTE au milieu. Elles ne dessinaient donc rien. C'etait
// une API qui ment : LightRenderer::DisplayLight appelait draw_sphere en croyant
// afficher les lampes.
//
// `screenshot` est partie avec elles : jamais appelee (sinaia a sa propre
// commande, RemoteConsole.cpp), et affligee d'un depassement de tas -- elle
// allouait 3*w*h octets la ou glReadPixels en ecrit ceil(3*w/4)*4 par ligne,
// GL_PACK_ALIGNMENT valant 4 par defaut.
//

//
// REPERE ADAPTATIF des axes.
//
// Les demi-longueurs etaient FIGEES a 0,2 unite monde. C'est le meme defaut que
// celui corrige sur la grille juste dessous, et il a la meme consequence : sur
// un modele importe sans normalisation qui mesure 1500 unites, le repere est un
// point invisible a l'origine ; sur un modele de 0,1 unite, il le traverse de
// part en part.
//
// La LOI est plus simple que celle de la grille, et deliberement : le repere
// n'est pas une reference de MESURE, seulement un indicateur d'orientation. Il
// n'a donc pas a s'accrocher a une decade -- une fraction du rayon de cadrage
// suffit.
//
// La fraction retenue, 0,2, REPRODUIT EXACTEMENT l'apparence actuelle quand le
// rayon de cadrage vaut 1, c'est-a-dire le cas normalise pour lequel les 0,2
// d'origine avaient ete regles. C'est donc une generalisation de l'existant,
// pas une refonte de son apparence.
//
// ⚠ Le repere reste trace a l'ORIGINE du monde, la ou la grille se centre sur le
// pivot. C'est voulu : un repere qui suivrait le pivot ne designerait plus
// l'origine, et n'apprendrait rien. La consequence est qu'un modele loin de
// l'origine peut le laisser hors champ -- ce qui est l'information juste.
//
// Calcul pur : ni GL, ni etat global. Un rayon nul ou negatif -- scene vide, ou
// cadrage pas encore calcule -- rend la longueur historique.
extern float repere_length (float radius);

// `length` est la DEMI-longueur : chaque axe va de -length a +length, la pointe
// de fleche etant au bout positif.
extern void repere_draw (float length = 0.2f);

//
// GRILLE ADAPTATIVE du plan Z = 0.
//
// La grille etait figee a un carre de 2 unites centre sur l'origine. Dans un
// monde ou un modele importe sans normalisation mesure 1500 unites, elle est
// un point ; dans un modele de 0,1 unite, elle deborde de sept fois le cadre.
// C'est ce qui obligeait les appelants a gonfler la bbox de cadrage de
// +-(2,2,1) -- un rembourrage qui sacrifiait le modele pour sauver la grille.
//
// La LOI retenue : la maille est une PUISSANCE DE DIX, la decade la plus proche
// du dixieme du diametre de cadrage, et le centre est accroche sur un multiple
// de cette maille. Le monde de l'application est metrique (la base de coupe
// mesure 450 x 300 mm) : une maille de 10 mm se lit, une maille de 3,7 mm ne
// dirait rien. La grille reste ainsi une reference de mesure, a toute echelle,
// et non un simple fond.
//
struct GridLayout
{
	float size    = 2.f;   // cote total du carre, en unites monde
	int   steps   = 10;    // nombre de mailles par cote (pair)
	float centerX = 0.f;   // centre dans le plan Z = 0, accroche a la maille
	float centerY = 0.f;
};

// Calcul pur : ni GL, ni etat global. `radius` est le rayon de la sphere de
// CADRAGE (demi-diagonale de la bbox autour du pivot), (pivotX, pivotY) le
// pivot projete dans le plan de la grille. Un rayon nul ou negatif rend la
// disposition par defaut.
extern GridLayout grid_layout (float radius, float pivotX, float pivotY);

extern void draw_grid (float size = 2.f, int step = 10,
                       float centerX = 0.f, float centerY = 0.f);
