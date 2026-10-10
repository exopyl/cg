// ORDRE D'INCLUSION CONTRAINT : gl_wrapper.h est le SEUL en-tete GL de cette
// unite et il vient en premier. Il est tire par examinator_trackball.h, mais le
// nommer ici rend la contrainte visible a qui ajoute une inclusion.
#include "gl_wrapper.h"

#include "camera.h"

#include "examinator_trackball.h"

#include <cgmath/orbit_camera.h>

#include <memory>

namespace peles
{
namespace view3d
{

namespace
{

// Rotation monde -> oeil identite : l'oeil est sur +Z et regarde vers -Z, le
// haut de l'image est +Y. Colonne-majeur, comme l'attend set_rotation.
const float kIdentityRotation[16] = { 1.f, 0.f, 0.f, 0.f,
                                      0.f, 1.f, 0.f, 0.f,
                                      0.f, 0.f, 1.f, 0.f,
                                      0.f, 0.f, 0.f, 1.f };

// LES TROIS EFFETS, TRADUITS EN BOUTONS DU TRACKBALL.
//
// ⚠ LA TABLE EST ICI ET NULLE PART AILLEURS. Les constantes LEFT_BUTTON,
// RIGHT_BUTTON et MIDDLE_BUTTON sont des macros de gl_wrapper.h, donc du cote
// cgre du pare-feu : les laisser paraitre dans l'en-tete de la camera y ferait
// entrer glad. Le sens de chaque bouton se lit dans Ctrackball::mouse_move --
// gauche orbite, milieu panoramique, droit dolly.
int TrackballButton (Camera::Drag drag)
{
	switch (drag)
	{
	case Camera::Drag::Dolly: return RIGHT_BUTTON;
	case Camera::Drag::Pan:   return MIDDLE_BUTTON;
	case Camera::Drag::Orbit: break;
	}
	return LEFT_BUTTON;
}

} // namespace

Camera::Camera ()
    : m_trackball (std::make_unique<Ctrackball> ())
{
	// Vue trois-quarts par defaut, qui est la pose de repos du trackball lui-
	// meme. L'identite regarderait un monde Z-up a la verticale.
	m_trackball->ResetTransformations ();
}

Camera::~Camera () = default;

void Camera::SetViewSize (int width, int height)
{
	m_trackball->set_dimensions (width, height);
}

int Camera::ViewWidth () const
{
	return (int)m_trackball->gl_window_width;
}

int Camera::ViewHeight () const
{
	return (int)m_trackball->gl_window_height;
}

void Camera::SetFrontalRotation ()
{
	m_trackball->set_rotation (kIdentityRotation);
}

void Camera::Frame (const BoundingBox& target, const BoundingBox& scene)
{
	float targetMin[3] = { 0.f, 0.f, 0.f };
	float targetMax[3] = { 0.f, 0.f, 0.f };
	if (!target.GetMinMax (targetMin, targetMax))
		return;

	const BoundingSphere framing = BoundingSphereOfBox (targetMin, targetMax);
	if (framing.radius <= 0.f)
		return;

	// LA PLAGE DE PROFONDEUR SE LIT SUR LA SCENE, LE CADRAGE SUR LA CIBLE. Une
	// scene vide -- ou reduite au sujet -- retombe sur la cible, faute de mieux.
	float depthMin[3] = { 0.f, 0.f, 0.f };
	float depthMax[3] = { 0.f, 0.f, 0.f };
	if (!scene.GetMinMax (depthMin, depthMax))
	{
		for (int i = 0; i < 3; ++i)
		{
			depthMin[i] = targetMin[i];
			depthMax[i] = targetMax[i];
		}
	}
	const BoundingSphere depth = BoundingSphereOfBox (depthMin, depthMax);

	// ⚠ LES DEUX SPHERES PARTENT EN PREMIER, ET L'ORDRE EST LA CORRECTION.
	// set_zoom borne la distance d'oeil contre le rayon de CADRAGE : publier les
	// spheres apres lui bornerait le nouveau cadrage avec l'echelle de la scene
	// precedente, et un petit modele charge apres un grand resterait hors de
	// portee du zoom.
	m_trackball->set_scene_spheres (depth.center, depth.radius, framing.radius);

	// ⚠ LA LOI DE CADRAGE N'EST PAS REECRITE ICI, ELLE EST APPELEE.
	// d = r / sin(fovY/2), majoree de 20 %, vit dans OrbitCamera::Frame. Une
	// camera de travail l'applique, et son resultat est publie au trackball, qui
	// y ajoute ses bornes de dolly. Une seconde ecriture de la meme formule
	// derive de la premiere sans qu'aucune mesure ne le dise.
	// ⚠ LE CHAMP SE LIT EN RADIANS A LA SOURCE, ET NON PAR get_fov_y_deg().
	// Cette derniere convertit en degres depuis OrbitCamera::GetFovY ; repasser
	// en radians ici ferait rad -> deg -> rad, donc un arrondi que get_pick_ray,
	// qui lit la MEME grandeur, ne ferait pas -- les deux operations cesseraient
	// d'etre exactement inverses (examinator_trackball.h:116-120).
	OrbitCamera framer;
	framer.SetFovY (m_trackball->camera ().GetFovY ());
	framer.Frame (targetMin, targetMax);

	const TVector3<float>& pivot = framer.GetPivot ();
	m_trackball->set_pivot (pivot.x, pivot.y, pivot.z);
	// set_zoom prend la coordonnee Z SIGNEE de l'oeil, donc l'oppose de la
	// distance.
	m_trackball->set_zoom (-framer.GetDistance ());
}

void Camera::Apply ()
{
	m_trackball->set_camera ();
}

void Camera::BeginDrag (Drag drag, int viewX, int viewY)
{
	m_trackball->mouse_press (TrackballButton (drag), PRESSED, viewX, viewY);
}

void Camera::DragTo (int viewX, int viewY)
{
	m_trackball->mouse_move (viewX, viewY);
}

void Camera::EndDrag (int viewX, int viewY)
{
	// ⚠ LA CAMERA N'A QU'UN SEUL GESTE A LA FOIS, ET C'EST UNE CONTRAINTE QUI
	// REMONTE A L'APPELANT. Le trackball n'a qu'un slot d'etat -- `tb_state` et
	// `tb_button` -- et `mouse_press` pose `tb_state = RELEASED` POUR TOUT
	// BOUTON. Appeler EndDrag sur un bouton qui n'a pas ouvert le geste tue donc
	// le geste en cours, et `mouse_move` sort aussitot.
	//
	// REGLE : l'hote n'appelle BeginDrag/EndDrag que pour le bouton qui a ouvert
	// le geste. Le bouton n'est pas transmis ici parce que le trackball n'en
	// ferait rien -- la valeur passee est celle du geste le plus courant.
	m_trackball->mouse_press (LEFT_BUTTON, RELEASED, viewX, viewY);
}

void Camera::ZoomStep (float notches)
{
	m_trackball->zoom_step (notches);
}

void Camera::GetPickRay (float ndcX, float ndcY, float origin[3], float direction[3]) const
{
	m_trackball->get_pick_ray (ndcX, ndcY, origin, direction);
}

void Camera::GetViewAxis (float axis[3]) const
{
	if (axis == nullptr)
		return;

	// L'oeil est en `c + d.r3`, ou r3 est la troisieme LIGNE de la partie 3x3 de
	// la rotation : la direction de visee est donc `-r3`, deja unitaire puisque
	// la rotation est orthonormale. La deduire de la difference pivot - oeil
	// ferait passer par une soustraction de grands nombres proches, donc par une
	// perte de precision que rien n'oblige a payer.
	const TMatrix4<float>& rotation = m_trackball->camera ().GetRotation ();
	for (int i = 0; i < 3; ++i)
		axis[i] = -rotation.at (2, i);
}

void Camera::GetPivot (float pivot[3]) const
{
	if (pivot == nullptr)
		return;
	const TVector3<float>& center = m_trackball->camera ().GetPivot ();
	pivot[0] = center.x;
	pivot[1] = center.y;
	pivot[2] = center.z;
}

void Camera::SetPivot (const float pivot[3])
{
	if (pivot == nullptr)
		return;
	m_trackball->set_pivot (pivot[0], pivot[1], pivot[2]);
}

} // namespace view3d
} // namespace peles
