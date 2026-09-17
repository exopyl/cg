#include "gl_wrapper.h"

#include "widgets_renderer.h"

#include <cmath>

//
// Repere des axes : trois fleches PLEINES et une bille a l'origine.
//
// SOLDE D'UNE NOTE ANTERIEURE. Cette fonction posait autrefois `glLineWidth (5)`
// ENTRE glBegin et glEnd, ou l'appel est interdit : il rendait
// GL_INVALID_OPERATION et etait ignore, si bien que le repere n'a jamais eu
// l'epaisseur annoncee. L'appel avait ete SUPPRIME plutot que deplace, en
// laissant ecrit que le deplacer « aurait change l'apparence, ce qui est une
// decision visuelle a prendre separement ».
//
// Cette decision est prise ici, et elle va plus loin que l'epaisseur : il n'y a
// plus de lignes du tout. Le repere est un maillage de triangles, et la question
// de glLineWidth ne se pose plus.
//

namespace {

// Direction d'eclairage FIXE du repere, en repere MONDE, deja normalisee.
//
// Le repere est ombre au CPU et dessine SANS eclairage GL. Ce n'est pas un
// raccourci : `toggle lighting off` ne doit pas aplatir le repere, et les lampes
// de la scene ne doivent pas le colorer. Un indicateur d'orientation qui change
// d'aspect selon l'eclairage du modele n'indique plus rien de facon fiable.
const float kRepereLight[3] = { 0.4061f, 0.3553f, 0.8629f };

// Un point du repere LOCAL d'un axe vers le repere monde. `along` court le long
// de l'axe, (u, v) sont les deux directions transverses. Ecrire la permutation
// une fois evite de tripler chaque primitive.
void axis_point (int axis, float along, float u, float v, float out[3])
{
	switch (axis)
	{
	case 0: out[0] = along; out[1] = u;     out[2] = v;     break;   // X
	case 1: out[0] = v;     out[1] = along; out[2] = u;     break;   // Y
	default: out[0] = u;    out[1] = v;     out[2] = along; break;   // Z
	}
}

void repere_tri (const float a[3], const float b[3], const float c[3],
                 const float col[3])
{
	const float e1[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] };
	const float e2[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] };
	float n[3] = { e1[1]*e2[2] - e1[2]*e2[1],
	               e1[2]*e2[0] - e1[0]*e2[2],
	               e1[0]*e2[1] - e1[1]*e2[0] };
	const float len = std::sqrt (n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
	float d = 0.f;
	if (len > 0.f)
		d = (n[0]*kRepereLight[0] + n[1]*kRepereLight[1] + n[2]*kRepereLight[2]) / len;
	if (d < 0.f) d = 0.f;
	// Plancher a 0,52 : une facette dos a la lumiere reste lisible au lieu de
	// tomber au noir. Un repere n'est pas un objet de la scene, il doit se lire.
	const float k = 0.52f + 0.48f * d;

	glColor3f (col[0]*k, col[1]*k, col[2]*k);
	glVertex3fv (a); glVertex3fv (b); glVertex3fv (c);
}

void repere_tube (int axis, float radius, float z0, float z1,
                  const float col[3], int seg)
{
	for (int i = 0; i < seg; ++i)
	{
		const float a0 = 6.2831853f * (float)i / (float)seg;
		const float a1 = 6.2831853f * (float)(i+1) / (float)seg;
		float p00[3], p10[3], p01[3], p11[3];
		axis_point (axis, z0, radius*std::cos(a0), radius*std::sin(a0), p00);
		axis_point (axis, z0, radius*std::cos(a1), radius*std::sin(a1), p10);
		axis_point (axis, z1, radius*std::cos(a0), radius*std::sin(a0), p01);
		axis_point (axis, z1, radius*std::cos(a1), radius*std::sin(a1), p11);
		repere_tri (p00, p10, p11, col);
		repere_tri (p00, p11, p01, col);
	}
}

void repere_cone (int axis, float radius, float z0, float z1,
                  const float col[3], int seg)
{
	float tip[3], base[3];
	axis_point (axis, z1, 0.f, 0.f, tip);
	axis_point (axis, z0, 0.f, 0.f, base);
	for (int i = 0; i < seg; ++i)
	{
		const float a0 = 6.2831853f * (float)i / (float)seg;
		const float a1 = 6.2831853f * (float)(i+1) / (float)seg;
		float p0[3], p1[3];
		axis_point (axis, z0, radius*std::cos(a0), radius*std::sin(a0), p0);
		axis_point (axis, z0, radius*std::cos(a1), radius*std::sin(a1), p1);
		repere_tri (p0, p1, tip, col);     // flanc
		repere_tri (p1, p0, base, col);    // fond, enroulement inverse
	}
}

void repere_sphere (float radius, const float col[3], int nu, int nv)
{
	for (int i = 0; i < nv; ++i)
	{
		const float t0 = 3.14159265f * (float)i / (float)nv;
		const float t1 = 3.14159265f * (float)(i+1) / (float)nv;
		for (int k = 0; k < nu; ++k)
		{
			const float p0 = 6.2831853f * (float)k / (float)nu;
			const float p1 = 6.2831853f * (float)(k+1) / (float)nu;
			float a[3], b[3], c[3], d[3];
			const float pts[4][2] = { {t0,p0}, {t1,p0}, {t1,p1}, {t0,p1} };
			float *out[4] = { a, b, c, d };
			for (int q = 0; q < 4; ++q)
			{
				const float t = pts[q][0], p = pts[q][1];
				out[q][0] = radius * std::sin(t) * std::cos(p);
				out[q][1] = radius * std::sin(t) * std::sin(p);
				out[q][2] = radius * std::cos(t);
			}
			repere_tri (a, b, c, col);
			repere_tri (a, c, d, col);
		}
	}
}

} // namespace

float repere_length (float radius)
{
	// `!(radius > 0)` plutot que `radius <= 0` : cette forme attrape aussi NaN,
	// qu'un cadrage sur une scene degeneree peut produire.
	if (!(radius > 0.f))
		return 0.2f;
	return 0.2f * radius;
}

//
// Trois fleches PLEINES -- hampe cylindrique, tete conique -- et une bille pale
// a l'origine, le tout ombre au CPU contre une lumiere fixe.
//
// Ce qui remplace : six segments de droite avec deux barbes chacun. A plat, sans
// volume, ils se confondaient avec les aretes du modele et ne disaient pas de
// quel cote pointait un axe vu de face.
//
// PROPORTIONS, en fractions de la demi-longueur : hampe de rayon 0,030 sur les
// 76 premiers pour cent, cone de rayon 0,082 sur le quart restant, bille de
// rayon 0,072. Ce sont les valeurs de la maquette ; elles se relisent d'un coup
// d'oeil et se retouchent sans toucher a la geometrie.
//
// ⚠ AXES POSITIFS SEULS, la ou l'ancien tracait de -L a +L. Une fleche indique
// un sens ; la moitie negative n'ajoutait rien et encombrait l'origine, que la
// bille marque desormais.
//
void repere_draw (float length)
{
	if (!(length > 0.f))
		length = 0.2f;

	const float shaftR = 0.030f * length;
	const float headR  = 0.082f * length;
	const float headZ  = 0.760f * length;
	const float ballR  = 0.072f * length;

	static const float kAxisColor[3][3] = {
		{ 0.902f, 0.243f, 0.188f },   // X rouge
		{ 0.220f, 0.729f, 0.345f },   // Y vert
		{ 0.243f, 0.486f, 0.957f },   // Z bleu
	};
	static const float kBallColor[3] = { 0.925f, 0.933f, 0.953f };

	glPushAttrib (GL_ALL_ATTRIB_BITS);
	glDisable (GL_LIGHTING);       // ombrage fait au CPU, cf. kRepereLight
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_COLOR_MATERIAL);
	glDisable (GL_CULL_FACE);      // volumes fermes : le tampon de profondeur suffit
	glEnable  (GL_DEPTH_TEST);     // sans quoi les fleches se traversent

	glBegin (GL_TRIANGLES);
	for (int axis = 0; axis < 3; ++axis)
	{
		repere_tube (axis, shaftR, 0.f,   headZ,  kAxisColor[axis], 16);
		repere_cone (axis, headR,  headZ, length, kAxisColor[axis], 18);
	}
	repere_sphere (ballR, kBallColor, 16, 12);
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
