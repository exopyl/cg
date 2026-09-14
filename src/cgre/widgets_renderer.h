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

extern void repere_draw (void);

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
