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
extern void draw_grid (float size = 2., int step = 10);
