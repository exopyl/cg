#include "gl_wrapper.h"

#include "widgets_renderer.h"

//
// Repere des axes : trois segments colores avec une pointe de fleche.
//
// NOTE SUR UN glLineWidth RETIRE. Cette fonction posait `glLineWidth (5.0f)`
// ENTRE glBegin et glEnd, ou l'appel est interdit : il rendait GL_INVALID_OPERATION
// et etait ignore. Le repere n'a donc jamais eu l'epaisseur 5 -- il a toujours ete
// trace a l'epaisseur courante. L'appel est supprime plutot que deplace : le
// deplacer AURAIT change l'apparence, ce qui est une decision visuelle a prendre
// separement. En l'etat, le rendu est identique a l'existant et une erreur GL
// cesse d'etre poussee dans la file a chaque image (visible desormais avec
// `glcheck on`).
//
void repere_draw (void)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_COLOR_MATERIAL);

  glBegin (GL_LINES);

  glColor3f(1.0f, 0.0f, 0.0f);
  glVertex3f(-0.2f,  0.0f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.15f,  0.04f, 0.0f);
  glVertex3f( 0.2f,  0.0f, 0.0f);
  glVertex3f( 0.15f, -0.04f, 0.0f);

  glColor3f(0.0f, 1.0f, 0.0f);
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( 0.0f, -0.2f, 0.0f);
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( 0.04f,  0.15f, 0.0f);
  glVertex3f( 0.0f,  0.2f, 0.0f);
  glVertex3f( -0.04f,  0.15f, 0.0f);

  glColor3f(0.0f, 0.0f, 1.0f);
  glVertex3f( 0.0f,  0.0f,  0.2f);
  glVertex3f( 0.0f,  0.0f, -0.2f);
  glVertex3f( 0.0f,  0.0f, 0.2f);
  glVertex3f( 0.0f,  0.04f, 0.15f);
  glVertex3f( 0.0f, 0.0f, 0.2f);
  glVertex3f( 0.0f, -0.04f, 0.15f);

  glEnd ();
  glPopAttrib ();
}

//
// Grille du plan Z = 0.
//
// Reste en O(nStep^2) : chaque ligne verticale ne depend que de i et chaque
// horizontale que de j, donc chacune est emise nStep+1 fois -- 484 glVertex3f
// au lieu de 88 pour nStep = 10. Ce n'est pas du code mort, c'est un defaut
// algorithmique, laisse tel quel ici pour ne pas melanger nettoyage et
// optimisation (cf. debt_cgre.md, tableau priorise).
//
void draw_grid (float size, int nStep)
{
  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glColor3f (0.f, 0.f, 0.f);
  glBegin (GL_LINES);
  const float hSize = .5f * size;
  float x, y;
  for (int j=0; j<= nStep; j++)
    for (int i=0; i<= nStep; i++)
      {
		x = i * size / nStep;
		y = j * size / nStep;
		glVertex3f ((GLfloat)(x - hSize), -(GLfloat)(hSize), 0.f);
		glVertex3f ((GLfloat)(x - hSize), (GLfloat)(hSize), 0.f);
		glVertex3f (-(GLfloat)(hSize), (GLfloat)(y - hSize), 0.f);
		glVertex3f ((GLfloat)(hSize), (GLfloat)(y - hSize), 0.f);
      }
  glEnd ();
  glPopAttrib ();
}
