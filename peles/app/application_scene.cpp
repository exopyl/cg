// ---------------------------------------------------------------------------
//  LA SCENE ET LES ENTREES
// ---------------------------------------------------------------------------
//
// Ce qui met un modele dans le document, et ce qui remet un evenement a son
// destinataire. Les deux vont ensemble parce qu'ils se rejoignent sur un seul
// point : un clic dans la vue SELECTIONNE, donc il touche le document.
//
// L'ordre des huit phases, lui, vit dans application.cpp : le melanger a
// quarante lignes de gestes le noierait.
//
// Application est declaree dans app/application.h, commune aux quatre unites.

#include "gl_wrapper.h"

#include "application.h"
#include "application_internal.h"

#include "view3d/camera.h"
#include "view3d/picking.h"
#include "input_router.h"
#include "loader.h"
#include "path_text.h"
#include "rml_host.h"
#include "scene_renderer.h"
#include "ui_command.h"
#include "viewport_rect.h"
#include "vmodels.h"
#include "window_glfw.h"

#include <cstdio>
#include <filesystem>
#include <utility>
#include <vector>

namespace peles
{
namespace app
{

namespace
{

	// SEUIL CLIC / GLISSER, EN PIXELS AU CARRE.
	//
	// Trois pixels, comme l'hote historique. En deca, le relachement SELECTIONNE ;
	// au-dela, le geste etait une rotation de la vue et ne selectionne pas -- sans
	// quoi tout orbite finirait par designer ce qui se trouve sous le curseur a
	// l'arrivee.
const int kClickSlopSquared = 9;

} // namespace

void Application::ReportLoadFailure (const std::string& name, const std::string& reason)
{
	// ⚠ DEUX CANAUX, ET AUCUN DES DEUX NE REMPLACE L'AUTRE. `stderr` sert
	// l'operateur d'un lancement en ligne de commande et la CI ; la ZONE DE
	// STATUT sert l'utilisateur de l'application, qui ne voit pas la console --
	// et l'application n'en a pas, le lot ne portant pas RemoteConsole.
	//
	// ⚠ LES DEUX PORTENT LE NOM ET LA RAISON. Un statut qui dirait seulement
	// « echec de chargement » laisserait l'utilisateur devant une vue vide sans
	// moyen de savoir lequel de ses fichiers a echoue ni pourquoi.
	std::fprintf (stderr, "peles : %s : %s\n", name.c_str (), reason.c_str ());
	m_ui->SetStatus (name + " : " + reason);

	m_loadFailureName   = name;
	m_loadFailureReason = reason;
	m_modelLoaded       = false;
}

bool Application::OpenModel (const std::string& path)
{
	// ⚠ LA DESTRUCTION PRECEDE LE CHARGEMENT, ET ELLE EXIGE LE CONTEXTE GL
	// COURANT : Clear annonce chaque maillage, l'observateur rend ses cles au
	// moteur, et cette restitution libere de vrais tampons GL. Cette fonction
	// n'est appelee que depuis la boucle d'images, ou le contexte l'est.
	//
	// ⚠ LES PANNEAUX SONT VIDES AVANT QUE LA GEOMETRIE NE MEURE. Ils ne
	// tiennent que des COPIES, donc aucune adresse ne pend ; ce qu'ils cessent
	// de decrire est une scene qui n'existe plus.
	m_ui->ShowModelInfo (nullptr);
	m_document.Clear ();

	const core::LoadResult loaded = core::LoadModel (m_document, path);
	if (loaded.model == nullptr)
	{
		ReportLoadFailure (path, loaded.error);
		return false;
	}

	std::printf ("modele : %s (%zu maillage(s))\n", loaded.model->m_name.c_str (),
	             loaded.model->m_meshes.GetNMeshes ());
	m_camera->Frame (loaded.model->m_bbox, m_document.VisibleBounds ());
	m_ui->SelectFile (path);
	m_ui->ShowModelInfo (m_document.Selected ());
	return true;
}

bool Application::AddModel (const std::filesystem::path& path)
{
	// ⚠ RIEN N'EST DETRUIT ICI, ET C'EST LA DIFFERENCE AVEC OpenModel. Ouvrir un
	// second fichier depuis la liste l'AJOUTE a la scene : c'est ce qui rend le
	// survol et la selection a la souris utiles, et c'est aussi le seul etat ou
	// la selection du modele decrit par les panneaux peut etre fausse.
	//
	// ⚠ LA CONVERSION VERS UNE CHAINE ETROITE EST SUBIE, PAS CHOISIE.
	// `VMeshesIO::load` prend un `const char*` : c'est la frontiere ou l'identite
	// du chemin cesse d'etre portee par un std::filesystem::path. Elle est faite
	// ICI, au dernier moment, et nulle part plus tot.
	const core::LoadResult loaded = core::LoadModel (m_document, path.string ());
	if (loaded.model == nullptr)
	{
		ReportLoadFailure (core::PathToUtf8 (path.filename ()), loaded.error);
		return false;
	}

	std::printf ("modele : %s (%zu maillage(s))\n", loaded.model->m_name.c_str (),
	             loaded.model->m_meshes.GetNMeshes ());

	// LE CADRAGE SUIT LE MODELE OUVERT, LA PROFONDEUR SUIT LA SCENE ENTIERE.
	m_camera->Frame (loaded.model->m_bbox, m_document.VisibleBounds ());
	m_ui->SelectFile (path);
	m_ui->ShowModelInfo (m_document.Selected ());
	return true;
}

void Application::RequestOpen (const std::filesystem::path& path)
{
	if (path.empty ())
		return;

	m_pendingOpenPath = path;
	m_ui->SetStatus ("Chargement de " + core::PathToUtf8 (path.filename ()) + " ...");
}

bool Application::FinishPendingOpen ()
{
	if (m_pendingOpenPath.empty ())
		return false;

	const std::filesystem::path path = m_pendingOpenPath;
	m_pendingOpenPath.clear ();

	// ⚠ LE STATUT EST EFFACE PAR LE SUCCES SEUL. Un echec laisse son propre
	// message, pose par AddModel : l'effacer inconditionnellement ferait
	// disparaitre la seule trace visible d'un fichier illisible.
	if (!AddModel (path))
		return false;

	m_ui->SetStatus (std::string ());
	return true;
}

void Application::ApplyCommand (const ui::UiCommand& command)
{
	switch (command.kind)
	{
	case ui::UiCommandKind::ToggleDirectory:
		m_ui->ActivateDirectoryRow (command.rowId);
		return;

	case ui::UiCommandKind::SelectFile:
		m_ui->SelectFileRow (command.rowId);
		return;

	case ui::UiCommandKind::OpenFile:
		RequestOpen (m_ui->SelectFileRow (command.rowId));
		return;
	}
}

void Application::ExecuteUiCommands ()
{
	for (const ui::UiCommand& command : m_ui->TakeCommands ())
		ApplyCommand (command);
}

void Application::SetHovered (::Model* model)
{
	if (m_document.Hovered () == model)
		return;
	m_document.Hover (model);
}

void Application::HoverFromPixel (int viewX, int viewY)
{
	SetHovered (view3d::PickModel (m_document, *m_camera, (float)viewX, (float)viewY).model);
}

void Application::SelectFromPixel (int viewX, int viewY)
{
	::Model* const hit =
	    view3d::PickModel (m_document, *m_camera, (float)viewX, (float)viewY).model;
	if (hit == nullptr || hit == m_document.Selected ())
		return;

	m_document.Select (hit);

	// LA LIGNE DE LA LISTE SUIT LA SELECTION DE LA VUE. C'est le remplacement de
	// RefreshModelSelection, et il va dans le sens DESCENDANT : la vue n'appelle
	// pas l'interface, c'est l'hote qui rapproche les deux.
	m_ui->SelectFile (hit->m_path);
	m_ui->ShowModelInfo (m_document.Selected ());
}

void Application::BeginViewportGesture (const ui::MouseEvent& event, int viewX, int viewY)
{
	// TOUT ENFONCEMENT EFFACE LA SURBRILLANCE DE SURVOL, sans quoi la boite
	// jaune resterait affichee pendant tout le glisser.
	SetHovered (nullptr);

	// ⚠ UN SEUL BOUTON OUVRE LE GESTE, ET LES SUIVANTS SONT IGNORES ICI.
	// L'arbitre a deja ferme l'etat mixte de SON cote -- le proprietaire est
	// fige au premier enfoncement -- mais la camera, elle, n'a qu'un seul slot
	// d'etat : un second BeginDrag reinitialiserait le glisser en cours, et le
	// EndDrag du second bouton le TUERAIT (cf. view3d/camera.cpp, EndDrag).
	if (event.button != m_input->GestureButton ())
		return;

	if (event.button == ui::MouseButton::Left && (event.modifiers & ui::modifier::kAlt) != 0)
	{
		// ALT + CLIC GAUCHE : poser le pivot sur la surface sous le curseur,
		// SANS toucher a la distance et SANS engager le trackball.
		m_altPivotGesture = true;
		const view3d::PivotPick pick =
		    view3d::SetPivotFromPixel (*m_camera, m_document, (float)viewX, (float)viewY);
		m_altPivotStatus = pick.status;
		if (pick.status == view3d::PivotPickStatus::NoHit)
			m_ui->SetStatus ("Alt+clic : aucune surface sous le curseur, pivot inchange.");
		return;
	}

	if (event.button == ui::MouseButton::Left)
	{
		m_pressX = viewX;
		m_pressY = viewY;
		m_camera->BeginDrag (view3d::Camera::Drag::Orbit, viewX, viewY);
	}
	else if (event.button == ui::MouseButton::Right)
	{
		m_camera->BeginDrag (view3d::Camera::Drag::Dolly, viewX, viewY);
	}
	else
	{
		m_camera->BeginDrag (view3d::Camera::Drag::Pan, viewX, viewY);
	}
}

void Application::EndViewportGesture (const ui::MouseEvent& event, int viewX, int viewY)
{
	// ⚠ SEUL LE BOUTON QUI A OUVERT LE GESTE LE FERME. Sans cette garde, le
	// relachement d'un second bouton appellerait EndDrag -- qui pose
	// `tb_state = RELEASED` quel que soit le bouton -- et le glisser en cours
	// mourrait sur place ; et un clic gauche relache pendant un dolly
	// SELECTIONNERAIT un modele, son deplacement etant nul.
	if (event.button != m_input->GestureButton ())
		return;

	if (m_altPivotGesture)
	{
		// Le trackball n'a jamais ete engage, et le geste ne selectionne pas
		// non plus : il ne change que le pivot.
		m_altPivotGesture = false;
		return;
	}

	m_camera->EndDrag (viewX, viewY);

	if (event.button != ui::MouseButton::Left)
		return;

	const int dx = viewX - m_pressX;
	const int dy = viewY - m_pressY;
	if (dx * dx + dy * dy <= kClickSlopSquared)
		SelectFromPixel (viewX, viewY);
}

void Application::CancelViewportGesture ()
{
	SetHovered (nullptr);

	if (m_altPivotGesture)
	{
		// Le trackball n'a jamais ete engage : il n'y a rien a relacher.
		m_altPivotGesture = false;
		return;
	}

	// ⚠ LE TRACKBALL EST RELACHE A LA DERNIERE POSITION CONNUE DU GESTE, ET
	// AUCUNE SELECTION N'EST FAITE. C'est toute la difference avec
	// EndViewportGesture : le seuil clic/glisser n'est pas consulte, donc un
	// appui immobile interrompu par un Alt+Tab ne selectionne rien.
	m_camera->EndDrag (m_gestureViewX, m_gestureViewY);
}

void Application::ApplyViewportGesture (const ui::MouseEvent& event, int viewX, int viewY)
{
	// LA DERNIERE POSITION DU GESTE, RELEVEE SUR TOUT EVENEMENT QUI EN PORTE
	// UNE : l'annulation, elle, n'en porte aucune.
	m_gestureViewX = viewX;
	m_gestureViewY = viewY;

	switch (event.kind)
	{
	case ui::MouseEvent::Kind::Press:
		BeginViewportGesture (event, viewX, viewY);
		return;

	case ui::MouseEvent::Kind::Release:
		EndViewportGesture (event, viewX, viewY);
		return;

	case ui::MouseEvent::Kind::Move:
	{
		if (!m_input->Holding ())
		{
			HoverFromPixel (viewX, viewY);
			return;
		}
		if (m_altPivotGesture)
			return; // Alt + clic gauche VISE, il ne fait pas tourner la vue
		m_camera->DragTo (viewX, viewY);
		return;
	}

	case ui::MouseEvent::Kind::Wheel:
		m_camera->ZoomStep (ui::ClampNotches (event.notches));
		return;

	// La sortie de cadre n'est pas un geste de la vue : l'arbitre l'a deja
	// remise a RmlUi, et la capture ne se rompt pas.
	case ui::MouseEvent::Kind::Leave: return;

	// L'annulation est traitee par ProcessInput, qui n'a pas de coordonnees a
	// convertir a lui remettre.
	case ui::MouseEvent::Kind::Cancel: return;
	}
}

void Application::ProcessInput ()
{
	std::vector<ui::MouseEvent> events;
	events.swap (m_pendingEvents);

	std::vector<ui::KeyEvent> keys;
	keys.swap (m_pendingKeys);

	// ⚠ AUCUN EVENEMENT DE SOURIS N'EST ARBITRE AVANT LA PREMIERE LECTURE DE
	// GEOMETRIE. Le clavier, lui, ECHAPPE A CETTE GARDE : il n'est pas arbitre
	// et ne porte aucune coordonnee, donc il n'y a rien a convertir -- et une
	// touche jetee ici ne reviendrait jamais.
	// La conversion fenetre -> vue soustrait l'origine du rectangle : sans
	// rectangle, elle rendrait des coordonnees decalees de la largeur du panneau
	// gauche, c'est-a-dire exactement le defaut que le lot mesure.
	//
	// ⚠ LE RECTANGLE EST LU DES LA CREATION DE L'INTERFACE, PAS SEULEMENT A LA
	// PHASE 3 : cette garde ne s'applique donc qu'aux etats ou AUCUNE mise en
	// page valide n'existe -- fenetre reduite, vue degeneree. Elle jette
	// silencieusement, et c'est assume : il n'y a aucun repere dans lequel
	// interpreter le pixel.
	if (!m_hasViewRect)
	{
		RouteKeys (keys);
		return;
	}

	for (const ui::MouseEvent& event : events)
	{
		const ui::InputRouter::Decision decision = m_input->Route (event);

		// RELEVE DANS LE CHEMIN DU PRODUIT, ET NON RECALCULE PAR L'INSTRUMENT :
		// c'est le proprietaire REELLEMENT retenu, pas une seconde lecture du
		// survol qui pourrait differer.
		if (event.kind == ui::MouseEvent::Kind::Press)
			m_lastPressOwner = decision.owner;

		// ⚠ TRAITEE AVANT L'AIGUILLAGE PAR PROPRIETAIRE, parce qu'elle ne porte
		// aucune coordonnee : la passer a ApplyViewportGesture y ferait entrer
		// un (0,0) qui deviendrait la derniere position du geste.
		if (event.kind == ui::MouseEvent::Kind::Cancel)
		{
			if (decision.owner == ui::InputRouter::Owner::Viewport)
				CancelViewportGesture ();
			continue;
		}

		if (decision.owner == ui::InputRouter::Owner::Viewport)
		{
			ApplyViewportGesture (event, decision.viewX, decision.viewY);
			continue;
		}

		// LE SURVOL 3D S'EFFACE DES QUE LE CURSEUR QUITTE LA VUE. Le laisser en
		// place ferait persister une boite jaune autour d'un modele que le
		// curseur ne designe plus.
		if (event.kind == ui::MouseEvent::Kind::Move)
			SetHovered (nullptr);
	}

	RouteKeys (keys);
}

void Application::RouteKeys (const std::vector<ui::KeyEvent>& keys)
{
	// LE CLAVIER, APRES LA SOURIS ET SANS ARBITRAGE.
	//
	// ⚠ APRES, PARCE QUE LE FOCUS PEUT VENIR D'UN CLIC DE LA MEME IMAGE : un
	// clic sur la ligne « Path » suivi d'un Ctrl+A dans le meme glfwPollEvents
	// n'aurait aucun element focalise si l'ordre etait inverse.
	for (const ui::KeyEvent& key : keys)
		m_input->RouteKey (key);
}


} // namespace app
} // namespace peles
