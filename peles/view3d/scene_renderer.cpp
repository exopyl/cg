// ORDRE D'INCLUSION CONTRAINT. gl_wrapper.h impose <windows.h> puis glad/wgl.h
// puis glad/gl.h, et glad/gl.h ne declare pas seulement des pointeurs : il
// REDEFINIT CHAQUE ENTREE GL PAR MACRO (`#define glPopAttrib glad_glPopAttrib`,
// src/cgre/glad/gl.h:4345-4346). Dans une unite qui touche cgre, gl_wrapper.h
// est le SEUL en-tete GL, et il vient en premier.
#include "gl_wrapper.h"

#include "scene_renderer.h"

// PAS DE PREFIXE view3d/ ICI : la racine de peles n'est PAS sur le chemin
// d'inclusion de cette cible, et c'est ce qui empeche une unite de view3d/
// d'atteindre app/. La forme non qualifiee resout vers view3d/camera.h par la
// regle « repertoire du fichier incluant d'abord », avant le camera.h de cgmesh.
#include "camera.h"
#include "document.h"

#include "cgre.h"
#include "mesh.h"
#include "vmodels.h"

namespace peles
{
namespace view3d
{

namespace
{

// Les deux ETATS D'ECLAIRAGE, et ce qui les separe.
//
// ⚠ AUCUN glEnable(GL_LIGHTING) ICI. Les deux fonctions POSENT DES TERMES ;
// l'allumage suit `rendering_properties_s::light`, dans DrawModels, comme dans
// DrawGL (sinaia/wxOpenGLCanvas.cpp:1374-1377). Deux decisions au lieu d'une
// finiraient par diverger.
//
// ⚠ ELLES POSENT LES POSITIONS AVEC LA MODELVIEW A L'IDENTITE.
// glLightfv(GL_POSITION) transforme sa valeur par la matrice courante : posee
// apres Camera::Apply, une direction de lampe tournerait avec la vue.
//
// ⚠ AUCUN TERME N'EST LAISSE A SA VALEUR PAR DEFAUT DANS NI L'UNE NI L'AUTRE.
// Le programme de surface de cgre somme INCONDITIONNELLEMENT les contributions
// des lampes 0 et 1 (src/cgre/surface_program.cpp : deux appels a
// lightContribution), sans consulter glIsEnabled : un canal laisse a son defaut
// entrerait dans la couleur sans figurer dans aucune des deux descriptions.

const GLfloat kBlack[4] = { 0.f, 0.f, 0.f, 1.f };
const GLfloat kWhite[4] = { 1.f, 1.f, 1.f, 1.f };

// L'ECLAIRAGE PRODUIT : le portage de sinaia/wxOpenGLCanvas.cpp InitGL
// (:1930-1965), terme pour terme.
//
// Une ambiante globale basse, une lampe principale un peu sous la pleine
// intensite venue du haut-avant-gauche, une lampe d'appoint plus faible du cote
// oppose. Les deux sont DIRECTIONNELLES (w = 0), donc l'ombrage ne depend ni de
// l'echelle du modele ni de sa position hors origine.
//
// ⚠ GL_LIGHT_MODEL_TWO_SIDE EST PORTE, et il n'est pas decoratif. Les OBJ
// importes ont souvent des enroulements incoherents, ce qui laisse des faces a
// normale rentrante ; sans lui, elles sortent quasi noires. Sur le chemin
// nominal il est INERTE -- le fragment shader retourne N lui-meme quand
// !gl_FrontFacing -- mais il mord des que le programme de surface est coupe,
// c'est-a-dire exactement sur le chemin de repli.
void SetupSceneLighting ()
{
	static const GLfloat kGlobalAmbient[4] = { 0.12f, 0.12f, 0.12f, 1.f };
	static const GLfloat kKeyDiffuse[4]    = { 0.75f, 0.75f, 0.75f, 1.f };
	static const GLfloat kKeyDir[4]        = { -0.4f, 0.6f, 1.f, 0.f };
	static const GLfloat kFillDiffuse[4]   = { 0.28f, 0.28f, 0.30f, 1.f };
	static const GLfloat kFillDir[4]       = { 0.7f, 0.25f, 0.4f, 0.f };

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glLightModelfv (GL_LIGHT_MODEL_AMBIENT, kGlobalAmbient);
	glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	glLightfv (GL_LIGHT0, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT0, GL_DIFFUSE, kKeyDiffuse);
	glLightfv (GL_LIGHT0, GL_SPECULAR, kWhite);
	glLightfv (GL_LIGHT0, GL_POSITION, kKeyDir);
	glEnable (GL_LIGHT0);

	glLightfv (GL_LIGHT1, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT1, GL_DIFFUSE, kFillDiffuse);
	glLightfv (GL_LIGHT1, GL_SPECULAR, kBlack);
	glLightfv (GL_LIGHT1, GL_POSITION, kFillDir);
	glEnable (GL_LIGHT1);
}

// L'ECLAIRAGE DE MESURE : celui sous lequel une couleur de pixel se calcule a la
// main.
//
// Une lampe DIRECTIONNELLE unique orientee (0,0,1,0), donc colineaire a l'axe de
// vue : le produit N.L de la face avant vaut exactement 1, et la couleur d'une
// face axiale vue de face se reduit a la diffuse du materiau. Ambiante globale,
// ambiante et speculaire de lampe toutes NOIRES, seconde lampe eteinte ET remise
// a zero.
//
// ⚠ GL_LIGHT_MODEL_TWO_SIDE A GL_FALSE EST DELIBERE ICI : l'oracle porte sur la
// face AVANT, et un retournement de normale sur une face arriere ajouterait a la
// prediction un cas qu'elle ne couvre pas.
void SetupMeasurementLighting ()
{
	static const GLfloat kLightDir[4] = { 0.f, 0.f, 1.f, 0.f };

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glLightModelfv (GL_LIGHT_MODEL_AMBIENT, kBlack);
	glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightfv (GL_LIGHT0, GL_POSITION, kLightDir);
	glLightfv (GL_LIGHT0, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT0, GL_DIFFUSE, kWhite);
	glLightfv (GL_LIGHT0, GL_SPECULAR, kBlack);
	glEnable (GL_LIGHT0);

	glLightfv (GL_LIGHT1, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT1, GL_DIFFUSE, kBlack);
	glLightfv (GL_LIGHT1, GL_SPECULAR, kBlack);
	glDisable (GL_LIGHT1);
}

// COMMUN AUX DEUX BRANCHES, et hors d'elles pour que leur difference se lise
// d'un coup d'oeil.
//
// ⚠ InitGL pose glEnable(GL_COLOR_MATERIAL) ; il n'est PAS porte, et c'est
// voulu. GL_COLOR_MATERIAL n'existe plus sous programme lie
// (src/cgre/surface_program.cpp : la diffuse vient de gl_FrontMaterial ou de la
// couleur de sommet, jamais de glColor), et MaterialRenderer le coupe de toute
// facon a chaque maillage. Le laisser allume ne changerait rien sur le chemin
// nominal et changerait la couleur sur le chemin de repli.
void SetupCommonLightingState ()
{
	glDisable (GL_COLOR_MATERIAL);
	glDisable (GL_TEXTURE_2D);
}

} // namespace

SceneRenderer::SceneRenderer (core::Document& document)
    : m_document (document)
{
	m_document.AddObserver (this);
}

SceneRenderer::~SceneRenderer ()
{
	m_document.RemoveObserver (this);

	// Ce qui reste enregistre a cet instant appartient a un document qui, lui,
	// survit : c'est ici que la table du renderer doit etre rendue, et pas
	// ailleurs. Un contexte GL COURANT est exige -- RemoveMesh libere les
	// tampons du maillage.
	for (Mesh* mesh : m_registered)
		MeshRenderer::getInstance ()->RemoveMesh (mesh);
	m_registered.clear ();
}

void SceneRenderer::SetFlatShading (bool flat)
{
	m_flatShading = flat;
}

void SceneRenderer::SetMeasurementLighting (bool measurement)
{
	m_measurementLighting = measurement;
}

void SceneRenderer::OnMeshReleasing (Mesh* mesh)
{
	const std::set<Mesh*>::iterator entry = m_registered.find (mesh);
	if (entry == m_registered.end ())
		return;

	MeshRenderer::getInstance ()->RemoveMesh (mesh);
	m_registered.erase (entry);
}

void SceneRenderer::DrawModels ()
{
	rendering_properties_s properties;
	rendering_properties_init (properties);
	properties.smooth = m_flatShading ? 0 : 1;

	// ⚠ UNE SEULE DECISION D'ECLAIRAGE, ET C'EST `properties.light`. cgre la lit
	// pour l'uniforme `uLighting` du programme de surface
	// (src/cgre/mesh_renderer.cpp:665) ; le pipeline fixe, lui, obeit a
	// glEnable(GL_LIGHTING). DrawGL les accorde en une ligne
	// (sinaia/wxOpenGLCanvas.cpp:1374-1377) ; les separer ici ferait dessiner le
	// chemin de repli eclaire sous un `light` a zero.
	if (properties.light)
		glEnable (GL_LIGHTING);
	else
		glDisable (GL_LIGHTING);

	MeshRenderer* const renderer = MeshRenderer::getInstance ();

	for (const std::unique_ptr<Model>& model : m_document.Models ())
	{
		if (!model || !model->m_visible)
			continue;

		for (Mesh* mesh : model->m_meshes.GetMeshes ())
		{
			if (mesh == nullptr)
				continue;

			// CG_RENDERING_VBO : les tampons vivent cote serveur et sont
			// redessines depuis la VRAM. VBOManager les reteleverse quand
			// Mesh::GetRevision change, donc une edition de geometrie reste
			// visible sans intervention ici.
			const int id = renderer->GetMeshId (mesh, CG_RENDERING_VBO);
			m_registered.insert (mesh);
			renderer->SetProperties (id, properties);
			renderer->Draw (id);
		}
	}
}

void SceneRenderer::DrawHoverHighlight ()
{
	Model* const hovered = m_document.Hovered ();
	if (hovered == nullptr || !hovered->m_visible)
		return;

	// ⚠ PAS DE ComputeBBox ICI. La boite est calculee A L'IMPORT
	// (core/loader.cpp), et la geometrie est statique ensuite -- le BVH du modele
	// en depend deja. La recalculer parcourrait tous les sommets du modele
	// survole A CHAQUE IMAGE, pour un resultat identique.
	if (hovered->m_bbox.IsEmpty ())
		return;

	float mn[3] = { 0.f, 0.f, 0.f };
	float mx[3] = { 0.f, 0.f, 0.f };
	hovered->m_bbox.GetMinMax (mn, mx);

	// La couleur et l'epaisseur de trait sont de l'etat GLOBAL : sans la pile
	// d'attributs, elles fuiraient vers le maillage suivant, qui apparaitrait
	// jaune.
	glPushAttrib (GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT);
	glDisable (GL_LIGHTING);
	// Sur un modele texture, la texture encore liee modulerait la couleur des
	// aretes et le melange la confondrait avec le fond : les deux sont coupes
	// pour un jaune constant quel que soit le modele survole.
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glColor3f (1.f, 1.f, 0.f);
	glLineWidth (2.f);

	const float x0 = mn[0], y0 = mn[1], z0 = mn[2];
	const float x1 = mx[0], y1 = mx[1], z1 = mx[2];
	glBegin (GL_LINES);
	glVertex3f (x0, y0, z0); glVertex3f (x1, y0, z0);
	glVertex3f (x1, y0, z0); glVertex3f (x1, y1, z0);
	glVertex3f (x1, y1, z0); glVertex3f (x0, y1, z0);
	glVertex3f (x0, y1, z0); glVertex3f (x0, y0, z0);

	glVertex3f (x0, y0, z1); glVertex3f (x1, y0, z1);
	glVertex3f (x1, y0, z1); glVertex3f (x1, y1, z1);
	glVertex3f (x1, y1, z1); glVertex3f (x0, y1, z1);
	glVertex3f (x0, y1, z1); glVertex3f (x0, y0, z1);

	glVertex3f (x0, y0, z0); glVertex3f (x0, y0, z1);
	glVertex3f (x1, y0, z0); glVertex3f (x1, y0, z1);
	glVertex3f (x1, y1, z0); glVertex3f (x1, y1, z1);
	glVertex3f (x0, y1, z0); glVertex3f (x0, y1, z1);
	glEnd ();

	glPopAttrib ();
}

void SceneRenderer::Draw (Camera& camera, int x, int y, int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	glViewport (x, y, width, height);
	// Le ciseau borne aussi ce que le pipeline fixe peut ecrire HORS du
	// rectangle -- notamment les fragments d'une geometrie qui deborde.
	glScissor (x, y, width, height);
	glEnable (GL_SCISSOR_TEST);

	glEnable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	// ⚠ ETEINTS DANS LES DEUX BRANCHES D'ECLAIRAGE, ET CE N'EST PAS L'ORACLE QUI
	// L'IMPOSE. GL_MULTISAMPLE n'a rien a activer : la fenetre est demandee avec
	// GLFW_SAMPLES = 0 (platform/window_glfw.cpp), donc il n'existe aucun tampon
	// multi-echantillon -- l'eteindre rend cette absence explicite au lieu de la
	// laisser dependre du pilote. Le tramage, lui, joue sur le bit de poids faible
	// de la couleur ecrite : sur un tampon a 8 bits par canal il n'apporte rien et
	// il ferait dependre la couleur d'un pixel de sa POSITION.
	glDisable (GL_MULTISAMPLE);
	glDisable (GL_DITHER);

	if (m_measurementLighting)
		SetupMeasurementLighting ();
	else
		SetupSceneLighting ();
	SetupCommonLightingState ();

	// GL_NORMALIZE reste ALLUME derriere cette fonction, comme dans le canvas
	// dont ce chemin est issu : une mise a l'echelle non uniforme laisserait
	// sinon des normales de longueur fausse et un eclairage qui varie avec le
	// zoom. Il est confine par la cloture d'etat posee autour de cette phase,
	// qui le rend a sa valeur d'entree.
	glEnable (GL_NORMALIZE);

	// ⚠ LES DIMENSIONS DU RECTANGLE, PAS CELLES DE LA FENETRE.
	camera.SetViewSize (width, height);
	camera.Apply ();

	glShadeModel (m_flatShading ? GL_FLAT : GL_SMOOTH);

	DrawModels ();
	DrawHoverHighlight ();
}

} // namespace view3d
} // namespace peles
