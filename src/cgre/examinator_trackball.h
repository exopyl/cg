#pragma once

#include <cgmath/orbit_camera.h>
#include <cgmath/TMatrix4.h>
#include <cgmath/TVector3.h>

#include <vector>

#include "gl_wrapper.h"

// Adaptateur d'entree : traduit les evenements souris en manipulations de
// camera, et emet les matrices dans le pipeline GL fixe. L'etat de camera
// lui-meme -- rotation, distance -- vit dans OrbitCamera, qui est du calcul pur
// et se teste hors contexte GL.
//
// Le pivot est un ETAT, pose par set_pivot (cadrages de
// MyGLCanvas::FrameCamera, Alt + clic, `camera pivot`) et deplace par le
// panoramique. Il ne bouge jamais de lui-meme, ni a chaque image, ni au debut
// d'une rotation : de pres, ce sont
// les pas de la rotation et du panoramique qui sont reduits
// (begin_left_rotation, begin_pan), pas le centre.
class Ctrackball
{
public:
	// LOI DE ROTATION du bouton gauche.
	//
	//   - Turntable (platine) : glisser horizontal = azimut autour de
	//     l'axe vertical +Z du monde passant par le pivot ; glisser vertical =
	//     elevation, bornee a [-90, +90] deg inclus. L'horizon reste droit. La
	//     vue est une fonction du deplacement CUMULE depuis l'enfoncement : un
	//     retour au pixel d'enfoncement restitue la vue de depart au bit pres.
	//   - Trackball (libre, defaut) : sphere de Bell/Shoemake, accumulation
	//     incrementale. La rotation depend du chemin et peut introduire du roulis.
	//
	// ResetTransformations repose l'orientation, jamais le mode.
	enum class RotationMode { Turntable, Trackball };

	// Nom d'un mode : "turntable" ou "trackball". C'est la forme de la console
	// (`camera mode`) et des preferences de sinaia (/View/RotationMode).
	static const char* RotationModeName (RotationMode mode);
	// Inverse exacte de RotationModeName, casse comprise. Un nom nul ou inconnu
	// rend false et laisse `mode` intact : l'appelant y a pose son defaut.
	static bool ParseRotationMode (const char* name, RotationMode& mode);

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
	// Bornes de la distance d'oeil, celles qu'appliquent set_zoom et le dolly.
	float get_min_distance () const;
	float get_max_distance () const;

	// ROTATION RALENTIE DE PRES. Tourner autour d'un centre a la distance d
	// deplace un point de surface vise a la profondeur t de
	// cot(fovY/2).|d/t - 1| px par px de souris : 26 px a 50 cm du faite
	// d'Hephaistos, centre au milieu de la scene. Le pas angulaire du geste est
	// donc multiplie par
	//
	//     s = clamp(t / d, kSlowMin, 1)
	//
	// ce qui ramene ce deplacement a cot(fovY/2).(1 - t/d), moins de 2,5 px par
	// px sous 45 deg, tant que le plancher ne mord pas. s est evalue UNE fois,
	// au debut du glisser (begin_left_rotation), et fige jusqu'au relachement.
	// Il ne change ni le centre de rotation ni la vue.
	//
	// kSlowMin : plancher contre une rotation figee quand l'oeil touche presque
	// la surface (t -> 0). A 0,02, un quart de tour demande 22 800 px a
	// 0,00344 rad/px ; avec le centre au milieu d'Hephaistos, il ne mord qu'a
	// moins de 11 cm du faite (plan_navigation_sinaia.md, D13).
	static constexpr float kSlowMin = 0.02f;

	// Origine de t, dans l'ordre ou les cas sont examines.
	enum class SlowdownSource
	{
		Surface,    // le rayon du centre de l'image touche une surface
		SceneBox,   // rien de touche, oeil hors de la boite de scene : t = entree
		            // du rayon dans la boite
		None        // rien de touche, et oeil dans la boite, rayon qui la manque
		            // ou pas de boite : s = 1
	};

	// Ce que la couche d'entree a trouve sur le rayon du CENTRE EXACT de
	// l'image, get_pick_ray (0, 0) : le picking et la boite de scene vivent
	// hors de cgre. Composee par ChooseSlowdownScene.
	struct SlowdownScene
	{
		bool  surfaceHit   = false;
		float surfaceDepth = 0.f;                // distance oeil -> surface
		bool  hasBox       = false;
		float boxMin[3]    = { 0.f, 0.f, 0.f };
		float boxMax[3]    = { 0.f, 0.f, 0.f };
	};

	// Impact d'UN modele sur le rayon du centre de l'image.
	struct SlowdownImpact
	{
		float t       = 0.f;     // distance oeil -> impact
		float z       = 0.f;     // cote monde de l'impact
		bool  surface = false;   // true : une face est touchee ; false : entree
		                         // dans la boite d'un modele sans faces (nuage
		                         // de points, lignes)
	};

	// Plan de coupe de mesh_renderer, { 0, 0, -1, z } dans le repere du monde :
	// le demi-espace VISIBLE est z' <= z.
	struct SlowdownClip
	{
		bool  active = false;
		float z      = 0.f;
	};

	struct SlowdownSceneChoice
	{
		SlowdownScene scene;
		int  impact     = -1;      // indice de l'impact retenu ; -1 : aucun
		bool clippedHit = false;   // au moins une surface ecartee par la coupe
	};

	// REGLE DU t DE RALENTISSEMENT, sans picking ni GL :
	//   - surface retenue : l'impact le plus proche parmi ceux qui touchent une
	//     FACE et sont dans le demi-espace visible. Un modele sans faces ne
	//     donne pas de surface : l'entree dans sa boite vaut 0 des que l'oeil y
	//     est, ce qui figerait la rotation et masquerait une surface derriere ;
	//   - boite de secours : la boite des modeles visibles LIMITEE au
	//     demi-espace visible (zmax <- min(zmax, z de coupe)) ; vide apres
	//     coupe, pas de boite.
	// `hasBox` faux : boxMin et boxMax ne sont pas lus.
	static SlowdownSceneChoice ChooseSlowdownScene (const std::vector<SlowdownImpact>& impacts,
	                                                bool hasBox, const float boxMin[3],
	                                                const float boxMax[3],
	                                                const SlowdownClip& clip);

	struct RotationSlowdown
	{
		SlowdownSource source = SlowdownSource::None;
		float t = 0.f;   // distance retenue ; 0 pour None
		float d = 0.f;   // distance oeil -> centre de rotation
		float s = 1.f;
	};

	// clamp(t / d, kSlowMin, 1). t ou d non fini, t negatif ou d nul : 1.
	static float SlowdownFactor (float t, float d);

	// Evalue s sur l'etat courant, sans rien modifier (console `camera slow`).
	RotationSlowdown evaluate_rotation_slowdown (const SlowdownScene& scene) const;

	// PREMIER TEMPS D'UN GLISSER GAUCHE, au franchissement du seuil de clic,
	// entre l'enfoncement et le premier mouse_move : evalue s et le fige pour
	// le geste en cours. Ni le pivot, ni la distance, ni l'orientation ne
	// changent.
	RotationSlowdown begin_left_rotation (const SlowdownScene& scene);

	// Facteur du geste gauche en cours ou du dernier enfoncement gauche, dans
	// [kSlowMin, 1]. Remis a 1 a chaque enfoncement du bouton gauche ; conserve
	// par rebase_left_gesture et apres le relachement, pour l'instrumentation.
	float get_rotation_slowdown () const { return m_rotationSlowdown; };

	// PANORAMIQUE RALENTI DE PRES. pan_screen deplace le pivot d'un pas calcule
	// au plan du pivot : un point de surface a la profondeur t glisse alors de
	// d/t px par px de curseur (11,9 a 50 cm du faite d'Hephaistos, pivot au
	// centre de la scene). Le glisser du bouton du milieu multiplie donc son
	// pas par le facteur s de la rotation, meme regle et meme scene
	// (evaluate_rotation_slowdown) : le point vise suit le curseur a 1 px par
	// px tant que kSlowMin.d <= t <= d.
	//
	// PREMIER TEMPS D'UN GLISSER DU MILIEU, au franchissement du seuil de clic,
	// entre l'enfoncement et le premier mouse_move : evalue s et le fige pour le
	// geste. Ni le pivot, ni la distance, ni l'orientation ne changent.
	RotationSlowdown begin_pan (const SlowdownScene& scene);
	// Facteur du glisser du milieu en cours ou du dernier enfoncement du
	// milieu. Remis a 1 a chaque enfoncement du bouton du milieu, conserve
	// apres le relachement. Ni la molette ni le dolly ne le lisent.
	float get_pan_slowdown () const { return m_panSlowdown; };

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
	//
	// C'est le pas brut, sans facteur : le glisser du bouton du milieu
	// l'appelle avec (s.dx, s.dy), s etant get_pan_slowdown.
	void pan_screen (float dx, float dy);

	// Repose l'orientation par defaut : la platine d'azimut -30 deg et
	// d'elevation 20 deg dans le monde Z-up (RotationFromTurntable), +Z vers le
	// haut de l'image. Ce n'est PAS l'identite -- l'identite regarde la scene a
	// la verticale, depuis +Z.
	//
	// Ni le pivot ni la distance n'en font partie : ce sont des etats de
	// cadrage, que l'appelant repose par set_pivot et set_zoom.
	void ResetTransformations();

	// Centre d'orbite, en coordonnees monde. La camera tourne autour de lui et
	// la matrice de vue le garde sur l'axe de vue quelle que soit la rotation.
	// Point d'entree unique des cadrages et des choix de l'utilisateur (Alt +
	// clic, `camera pivot`).
	void set_pivot (float x, float y, float z);

	// Mode de rotation du bouton gauche. Passer de Trackball a Turntable
	// redresse la vue (straighten_horizon) ; reposer le mode courant ne change
	// rien. Un glisser gauche en cours est reancre sur son dernier pixel : il
	// continue depuis l'etat courant, sans saut (voir rebase_left_gesture).
	void         set_rotation_mode (RotationMode mode);
	RotationMode get_rotation_mode () const { return m_rotationMode; };

	// Retire le roulis : R <- RotationFromTurntable(TurntableFromRotation(R)).
	// Le pivot, la distance et la direction de vue sont conserves, pole compris ;
	// a l'ecran, l'image tourne autour de son centre de -RollOfRotation(R), et
	// ne bouge pas a la verticale exacte.
	void straighten_horizon ();

	// Pas angulaire, dans les deux modes : s.kRotationRadiansPerSide / min(w, h)
	// radians par pixel, s etant le facteur du geste (get_rotation_slowdown).
	// A s = 1, c'est le pas du trackball au centre de sa sphere : changer de
	// mode ne change pas la sensibilite.
	static constexpr float kRotationRadiansPerSide = 2.f;

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
	// Un evenement de glisser gauche en mode platine (voir RotationMode).
	void turntable_move (int x, int y);
	// ECRITURE EXTERIEURE PENDANT UN GLISSER GAUCHE (orientation, pivot, mode,
	// facteur de ralentissement) : le geste repart du dernier pixel recu, sur
	// l'etat qui vient d'etre ecrit. Le facteur s du geste est conserve.
	// Sans cela, l'ancrage de la platine ecraserait l'ecriture au pas suivant,
	// et la sphere du trackball tournerait depuis un point perime. Sans glisser
	// gauche en cours, ne fait rien.
	void rebase_left_gesture ();
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

	RotationMode m_rotationMode;

	// Voir get_rotation_slowdown.
	float        m_rotationSlowdown;
	// Voir get_pan_slowdown.
	float        m_panSlowdown;

	// GESTE PLATINE. L'ancrage est pris au premier deplacement d'un glisser
	// gauche ; chaque evenement reconstruit R depuis le deplacement cumule
	// depuis le pixel d'ancrage, sans accumulation.
	int             m_pressX, m_pressY;       // pixel d'enfoncement du bouton gauche
	int             m_lastX, m_lastY;         // dernier pixel du glisser gauche
	bool            m_anchorValid;
	int             m_anchorX, m_anchorY;     // pixel d'ancrage
	double          m_anchorAzimuth;          // radians
	double          m_anchorElevation;        // radians
	TMatrix4<float> m_anchorRotation;         // R restituee au deplacement nul
	TVector3<float> m_anchorPivot;            // c restitue au deplacement nul
};
