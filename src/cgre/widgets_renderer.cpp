#include "gl_wrapper.h"

#include "widgets_renderer.h"

#include <cmath>

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

namespace {

// Gardes de la disposition : la loi ci-dessous produit deja un nombre de
// mailles dans [4, 32] (le rapport maille/rayon est borne par la decade), donc
// ces bornes ne mordent jamais sur une entree saine. Elles protegent contre un
// rayon NaN ou denormalise.
constexpr int   kGridMinSteps  = 4;
constexpr int   kGridMaxSteps  = 40;
constexpr float kGridMinRadius = 1e-6f;

} // namespace

GridLayout grid_layout (float radius, float pivotX, float pivotY)
{
  GridLayout g;
  if (!(radius > kGridMinRadius))   // la negation attrape aussi NaN
    return g;

  // Maille : decade la plus proche du dixieme du diametre, soit 0,2 x rayon.
  // L'arrondi de l'exposant garde la maille a moins d'un facteur sqrt(10) de
  // cette cible, donc la grille compte toujours entre 4 et 32 mailles.
  const float cell = std::pow (10.f, std::floor (std::log10 (0.2f * radius) + 0.5f));
  if (!(cell > 0.f))
    return g;

  // Nombre PAIR de mailles : le centre tombe alors sur une ligne de la grille.
  int steps = 2 * (int)std::ceil (radius / cell);
  if (steps < kGridMinSteps) steps = kGridMinSteps;
  if (steps > kGridMaxSteps) steps = kGridMaxSteps;

  g.steps = steps;
  g.size  = steps * cell;
  // Accrochage du centre sur la maille. Sans lui les lignes tomberaient a des
  // cotes quelconques, et la grille perdrait ce qui en fait une reference.
  g.centerX = std::floor (pivotX / cell + 0.5f) * cell;
  g.centerY = std::floor (pivotY / cell + 0.5f) * cell;
  return g;
}

//
// Grille du plan Z = 0, centree sur (centerX, centerY).
//
// Les deux familles de lignes sont emises SEPAREMENT. La forme d'origine les
// emettait depuis une double boucle, donc chaque ligne nStep+1 fois -- 484
// glVertex3f pour nStep = 10, et 6724 pour le nStep = 40 que la grille
// adaptative rend desormais atteignable. Le trace est identique : les lignes
// surnumeraires etaient superposees a l'identique, sans fusion.
//
void draw_grid (float size, int nStep, float centerX, float centerY)
{
  if (nStep < 1)
    return;

  glPushAttrib (GL_ALL_ATTRIB_BITS);
  glColor3f (0.f, 0.f, 0.f);
  glBegin (GL_LINES);
  const float hSize = .5f * size;
  for (int i=0; i<= nStep; i++)
    {
      const float t = i * size / nStep - hSize;
      glVertex3f ((GLfloat)(centerX + t), (GLfloat)(centerY - hSize), 0.f);
      glVertex3f ((GLfloat)(centerX + t), (GLfloat)(centerY + hSize), 0.f);
      glVertex3f ((GLfloat)(centerX - hSize), (GLfloat)(centerY + t), 0.f);
      glVertex3f ((GLfloat)(centerX + hSize), (GLfloat)(centerY + t), 0.f);
    }
  glEnd ();
  glPopAttrib ();
}
