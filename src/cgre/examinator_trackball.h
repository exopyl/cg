#pragma once

#include <cgmath/orbit_camera.h>

#include "gl_wrapper.h"

// Adaptateur d'entree : traduit les evenements souris en manipulations de
// camera, et emet les matrices dans le pipeline GL fixe. L'etat de camera
// lui-meme -- rotation, distance -- vit dans OrbitCamera, qui est du calcul pur
// et se teste hors contexte GL.
//
// Le pivot est un ETAT, pose par les actions explicites de cadrage (set_pivot,
// appele par MyGLCanvas::FrameCamera). Il n'est jamais derive au vol : le
// recalculer a chaque image imposerait un parcours de toute la scene.
class Ctrackball
{
public:
	Ctrackball ();
	virtual ~Ctrackball () {};
	
	// change dimensions of the GL window.
	// Dimensions are clamped to at least one pixel: they divide the cursor
	// mapping, and a zero would inject a NaN that the rotation keeps for good.
	virtual void set_dimensions (GLint width, GLint height)
		{
			gl_window_width  = (width  > 0)? width  : 1;
			gl_window_height = (height > 0)? height : 1;
		};

	// mouse
	virtual void mouse_press (int button, int state, int x, int y);
	virtual void mouse_move  (int x, int y);
	
	virtual void set_camera (void);

	// The eye distance to the pivot. Both entry points clamp it to
	// [0.01, 100] x le rayon de CADRAGE, quand il est connu -- jamais le rayon
	// de profondeur : celui-ci englobe le tapis, et le tapis ne doit pas
	// contraindre le zoom.
	//
	// set_zoom / get_zoom keep the historical SIGNED form: the Z coordinate of
	// the eye, hence negative. The distance is its magnitude.
	void set_zoom (float zoom);
	float get_zoom () const { return -m_orbit.GetDistance (); }
	// One dolly step, in wheel notches: positive moves the eye closer.
	void zoom_step (float steps);

	// PANORAMIQUE, en pixels ecran (origine en haut a gauche, y vers le bas,
	// convention des evenements souris). Il DEPLACE LE PIVOT :
	//
	//     k  = 2.d.tan(fovY/2) / H        (unites monde par pixel, au pivot)
	//     c += Rt . ( -k.dx , +k.dy , 0 )
	//
	// donc un point du plan du pivot suit le curseur au pixel pres, a toute
	// distance et a tout champ. Un decalage ecran garde a cote de la matrice de
	// vue ne le ferait pas : le centre de rotation resterait la ou il etait
	// AVANT le panoramique, et tourner apres avoir amene un detail au centre le
	// ferait repartir en arc.
	//
	// H (la hauteur du viewport) sert aux DEUX axes : les pixels sont carres,
	// meme raison que le min(w,h) de tbPointToVector.
	void pan_screen (float dx, float dy);

	// Remet l'orientation a zero. Ni le pivot ni la distance n'en font partie :
	// ce sont des etats de cadrage, que l'appelant repose par set_pivot et
	// set_zoom.
	void ResetTransformations();

	// Centre d'orbite, en coordonnees monde. La camera tourne autour de lui et
	// la matrice de vue le garde sur l'axe de vue quelle que soit la rotation.
	void set_pivot (float x, float y, float z);

	// LES DEUX SPHERES DU CADRAGE, publiees en un seul geste. Elles repondent a
	// deux questions differentes et aucune ne se derive de l'autre :
	//
	//   - PROFONDEUR (`depthCenter`, `depthRadius`) : ce que les plans near/far
	//     doivent encadrer. Elle inclut le tapis de coupe quand il est affiche,
	//     et son centre suit la scene -- le figer sur l'origine du monde
	//     l'elargirait d'autant que la scene en est ecartee.
	//   - CADRAGE (`framingRadius` ; son centre EST le pivot) : l'echelle du
	//     sujet observe. Elle borne le dolly, et le tapis n'y entre PAS : il ne
	//     doit pas contraindre le zoom.
	//
	// Un seul point d'entree pour les deux. Deux appels separes laisseraient une
	// fenetre ou l'une decrit la scene courante et l'autre la precedente, et
	// rien ne le signalerait.
	//
	// Les deux plans de coupe se derivent de la distance d'oeil COURANTE a
	// chaque image, si bien que le modele ne les traverse jamais en zoomant. Un
	// rayon de profondeur <= 0 (le defaut) conserve les plans fixes .01/10.
	void set_scene_spheres (const float depthCenter[3], float depthRadius,
	                        float framingRadius);

	// Rayons publies, pour l'instrumentation. Le centre de profondeur n'est plus
	// l'origine du monde : un instrument qui le supposerait mesurerait faux.
	float get_depth_radius   () const { return m_depthRadius; };
	float get_framing_radius () const { return m_framingRadius; };
	void  get_depth_center   (float center[3]) const;

	// ------------------------------------------------------------------
	// LECTURE DE L'ETAT, pour l'instrumentation
	// ------------------------------------------------------------------
	// Les deux matrices sont rendues telles que set_camera les envoie a GL,
	// et non relues par glGetFloatv. Un instrument qui interroge l'etat GL
	// mesure ce que la derniere peinture y a laisse : ResetProjectionMode
	// remet la modelview a l'identite, et Refresh(false) PLANIFIE une peinture
	// sans l'executer. Il heriterait donc des defauts qu'il doit detecter.
	//
	// set_camera n'est plus qu'un emetteur : elle appelle ces deux fonctions.
	// Une seule source, donc l'instrument ne peut pas deriver du rendu.
	const OrbitCamera& camera () const { return m_orbit; };
	// Champ vertical du frustum, en DEGRES -- pour l'affichage et les scripts.
	// La source est OrbitCamera::GetFovY, en radians : la projection et le rayon
	// de picking la lisent la. Une valeur en dur ici en ferait une seconde, et
	// les deux operations cesseraient d'etre exactement inverses.
	float get_fov_y_deg () const { return m_orbit.GetFovY () * (180.f / 3.14159265358979f); };
	// Plans de coupe EFFECTIFS de la prochaine image : ceux derives du rayon de
	// scene s'il est connu, sinon les valeurs de repli.
	void get_clip_planes (float *zNear, float *zFar) const;
	// Matrices de la prochaine image, en COLONNE-MAJEUR (m[4*colonne + ligne]).
	void get_projection_matrix (float m[16]) const;
	void get_modelview_matrix  (float m[16]) const;

	// Rayon monde du PICKING pour un point du plan image donne en NDC
	// (-1 en bas a gauche, +1 en haut a droite, convention OpenGL).
	//
	// Meme source et meme ratio d'aspect que get_projection_matrix, donc
	// exactement son inverse. Rien n'est relu dans GL : un survol qui tombe
	// entre un redimensionnement et la peinture suivante, ou un picking
	// scripte apres un Refresh(false) non encore honore, desprojetterait
	// sinon avec la modelview de l'image precedente -- voire avec l'identite,
	// que ResetProjectionMode vient d'y charger.
	void get_pick_ray (float ndcX, float ndcY,
	                   float origin[3], float direction[3]) const;

	// Orientation courante, en COLONNE-MAJEUR comme l'attend glMultMatrixf.
	void get_matrix (GLfloat m[4][4]);
	void get_matrix (GLfloat *m);
	// Impose l'orientation au lieu de l'accumuler. Point d'entree des reglages
	// deterministes (harnais de captures), qui posent un point de vue exact.
	void set_rotation (const float m[16]);

	void getCameraPosition	(float *x, float *y, float *z);
	
private:
	void tbPointToVector (int x, int y, float v[3]);
	void set_distance (float distance);
	// Ratio largeur / hauteur de la prochaine image. Une fenetre reduite
	// rapporte une hauteur nulle : sans garde, le ratio vaut NaN et empoisonne
	// aussi bien la projection que le rayon de picking.
	float aspect_ratio () const;
	bool has_valid_dimensions () const
		{
			return gl_window_width > 0 && gl_window_height > 0;
		};

public:
	GLint gl_window_width;
	GLint gl_window_height;
	
	GLfloat   tb_lastposition[3];

	int       tb_state;
	int       tb_button;
	int       lastX, lastY;

	// Perspective clip planes: fallback used until a depth sphere is published.
	float     m_zNear, m_zFar;

private:
	OrbitCamera m_orbit;

	// Sphere de PROFONDEUR : ce que near/far doivent encadrer.
	float     m_depthCenter[3];
	float     m_depthRadius;
	// Sphere de CADRAGE : rayon seul, son centre est le pivot. Borne le dolly.
	float     m_framingRadius;
};
