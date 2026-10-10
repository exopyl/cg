// ---------------------------------------------------------------------------
//  LES MODES « VIDER ET SORTIR », PORTES PAR L'HOTE
// ---------------------------------------------------------------------------
//
// Ces modes injectent un geste, lisent son effet et rendent un code de sortie
// sans jamais entrer dans la boucle d'images. Ils vivent ici -- et non dans
// app/dump_*.cpp -- parce qu'ils ont besoin de l'hote LUI-MEME : la file
// d'evenements, la camera, le document, les panneaux. Ce que app/dump_*.cpp
// porte, ce sont les verdicts ; ce que porte cette unite, ce sont les gestes.
//
// Application est declaree dans app/application.h, commune aux quatre unites.

#include "gl_wrapper.h"

#include "application.h"
#include "application_internal.h"

#include "view3d/camera.h"
#include "view3d/picking.h"
#include "dump_modes.h"
#include "input_router.h"
#include "model_info.h"
#include "rml_host.h"
#include "scene_renderer.h"
#include "ui_command.h"
#include "viewport_rect.h"
#include "vmodels.h"
#include "window_glfw.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <utility>
#include <vector>

namespace peles
{
namespace app
{

void Application::QueueClickSequence ()
{
	ui::MouseEvent move;
	move.kind    = ui::MouseEvent::Kind::Move;
	move.windowX = m_options.clickX;
	move.windowY = m_options.clickY;

	ui::MouseEvent press = move;
	press.kind           = ui::MouseEvent::Kind::Press;
	ui::MouseEvent release = move;
	release.kind           = ui::MouseEvent::Kind::Release;

	m_pendingEvents.push_back (move);
	m_pendingEvents.push_back (press);
	m_pendingEvents.push_back (release);

	// ⚠ LE SECOND COUPLE EST CE QUI PRODUIT LE DOUBLE-CLIC. RmlUi le synthetise
	// a partir de deux enfoncements rapproches sur le meme element ; sans lui,
	// `data-event-dblclick` -- donc l'ouverture d'un fichier -- resterait
	// inexerce.
	if (m_options.clickDouble)
	{
		m_pendingEvents.push_back (press);
		m_pendingEvents.push_back (release);
	}
}

int Application::RunClick ()
{
	// UNE IMAGE COMPLETE D'ABORD : c'est elle qui met en page les lignes des
	// panneaux. Sans mise en page, rien n'est survolable et le clic tomberait
	// dans le vide sans que l'on sache si c'est le cablage ou la geometrie qui
	// manque.
	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr, "peles : aucune image rendue. MESURE NON CONCLUANTE.\n");
		return 1;
	}

	QueueClickSequence ();

	const std::string hoverBefore   = m_input->HoverName ();
	const int         modelsBefore = (int)m_document.Models ().size ();
	ProcessInput ();

	// LES COMMANDES SONT LUES AVANT D'ETRE EXECUTEES, et c'est tout l'objet de
	// cet instrument : une commande produite mais jamais appliquee et une
	// commande jamais produite se ressemblent a l'ecran.
	const std::vector<ui::UiCommand> commands = m_ui->TakeCommands ();

	std::printf ("\n--- clic sur l'interface ---\n");
	std::printf ("%-26s : (%d, %d)%s\n", "pixel (FENETRE)", m_options.clickX, m_options.clickY,
	             m_options.clickDouble ? " (double)" : "");
	std::printf ("%-26s : %s\n", "survol apres deplacement", m_input->HoverName ().c_str ());
	std::printf ("%-26s : %s\n", "survol avant deplacement", hoverBefore.c_str ());
	std::printf ("%-26s : %d\n", "commandes produites", (int)commands.size ());

	for (const ui::UiCommand& command : commands)
	{
		const char* name = "?";
		switch (command.kind)
		{
		case ui::UiCommandKind::ToggleDirectory: name = "ToggleDirectory"; break;
		case ui::UiCommandKind::SelectFile:      name = "SelectFile"; break;
		case ui::UiCommandKind::OpenFile:        name = "OpenFile"; break;
		}
		std::printf ("  %-24s ligne %d\n", name, command.rowId);
		ApplyCommand (command);
	}


	// L'IMAGE DE TRANSITION, PUIS LE CHARGEMENT. Meme enchainement que la boucle
	// d'images : le statut est pose, une image complete le dessine, le
	// chargement synchrone suit.
	if (!m_pendingOpenPath.empty ())
	{
		std::printf ("%-26s : %s\n", "statut a l'image de transition",
		             m_ui->Model ().status.c_str ());
		RenderFrame (nullptr, nullptr);
		FinishPendingOpen ();
	}

	std::printf ("%-26s : %s\n", "repertoire courant", m_ui->Model ().currentDirectory.c_str ());
	std::printf ("%-26s : %d\n", "lignes d'arbre", (int)m_ui->Model ().treeRows.size ());
	std::printf ("%-26s : %d\n", "lignes de fichiers", (int)m_ui->Model ().fileRows.size ());

	const std::vector<core::InfoRow>& rows = m_ui->Model ().infoRows;
	const std::size_t                 line = (std::size_t)core::InfoLine::File;
	std::printf ("%-26s : %s\n", "ligne \"File\" du panneau",
	             line < rows.size () && !rows[line].value.empty () ? rows[line].value.c_str ()
	                                                               : "(vide)");

	const bool ok = ReportClick (m_options.clickDouble, m_options.clickExpect, commands,
	                             modelsBefore, (int)m_document.Models ().size ());

	const int journal = ReportRmlJournal ();
	if (journal != 0)
		return journal;
	return ok ? 0 : 1;
}

namespace
{

// ANGLE ENTRE DEUX AXES DE VUE, EN DEGRES. Les deux sont unitaires : le produit
// scalaire est donc le cosinus, borne contre l'arrondi qui le pousse hors de
// [-1, 1].
double AngleBetweenDeg (const float a[3], const float b[3])
{
	const float dot =
	    std::max (-1.f, std::min (1.f, a[0] * b[0] + a[1] * b[1] + a[2] * b[2]));
	return std::acos ((double)dot) * 180. / 3.14159265358979;
}

} // namespace

int Application::RunDrag ()
{
	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr, "peles : aucune image rendue. MESURE NON CONCLUANTE.\n");
		return 1;
	}

	if (m_options.dragBump != GestureOwner::None)
		return RunDragBump ();

	float axisBefore[3]  = { 0.f, 0.f, 0.f };
	float pivotBefore[3] = { 0.f, 0.f, 0.f };
	m_camera->GetViewAxis (axisBefore);
	m_camera->GetPivot (pivotBefore);

	ui::MouseEvent start;
	start.kind    = ui::MouseEvent::Kind::Move;
	start.windowX = m_options.dragFromX;
	start.windowY = m_options.dragFromY;

	ui::MouseEvent press = start;
	press.kind           = ui::MouseEvent::Kind::Press;

	ui::MouseEvent move;
	move.kind    = ui::MouseEvent::Kind::Move;
	move.windowX = m_options.dragToX;
	move.windowY = m_options.dragToY;

	ui::MouseEvent release = move;
	release.kind           = ui::MouseEvent::Kind::Release;

	m_pendingEvents.push_back (start);
	m_pendingEvents.push_back (press);
	m_pendingEvents.push_back (move);
	m_pendingEvents.push_back (release);
	ProcessInput ();

	float axisAfter[3]  = { 0.f, 0.f, 0.f };
	float pivotAfter[3] = { 0.f, 0.f, 0.f };
	m_camera->GetViewAxis (axisAfter);
	m_camera->GetPivot (pivotAfter);

	// ⚠ « DANS LA VUE » SE LIT SUR LE RECTANGLE RELEVE, pas sur une largeur de
	// panneau ecrite ici : le C++ ne connait aucune largeur de panneau.
	const bool arrivalInView =
	    m_options.dragToX >= m_viewRect.x && m_options.dragToX < m_viewRect.x + m_viewRect.width &&
	    m_options.dragToY >= m_viewRect.y && m_options.dragToY < m_viewRect.y + m_viewRect.height;

	const bool ok = ReportDrag (m_options.dragFromX, m_options.dragFromY, m_options.dragToX,
	                            m_options.dragToY, arrivalInView, axisBefore, axisAfter,
	                            pivotBefore, pivotAfter);
	const int  journal = ReportRmlJournal ();
	if (journal != 0)
		return journal;
	return ok ? 0 : 1;
}

int Application::RunDragBump ()
{
	// L'OEIL, LU PAR LE RAYON DU PIXEL CENTRAL. ⚠ C'EST LA SEULE GRANDEUR QUI
	// BOUGE SOUS UN DOLLY : le pivot ne bouge pas, et l'axe de vue non plus.
	// ScreenToRay rend l'origine du rayon, qui EST la position de l'oeil.
	const auto readEye = [this] (float eye[3])
	{
		float direction[3] = { 0.f, 0.f, 0.f };
		view3d::ScreenToRay (*m_camera, 0.5f * (float)m_viewRect.width,
		                     0.5f * (float)m_viewRect.height, eye, direction);
	};

	// ⚠ LA SELECTION EST REMISE A VIDE AVANT L'INJECTION, ET C'EST UNE CONDITION
	// DE VALIDITE DU DETECTEUR. `SelectFromPixel` sort sans rien faire quand le
	// modele touche EST DEJA le modele selectionne : or un fichier passe en
	// argument est selectionne par son chargement. Sans cette ligne, « aucune
	// selection » serait vrai sur une scene a un modele QUOI QUE le geste fasse,
	// et le versant `right` ne pourrait pas echouer.
	m_document.Select (nullptr);
	m_ui->ShowModelInfo (nullptr);

	float axisBefore[3] = { 0.f, 0.f, 0.f };
	float eyeBefore[3]  = { 0.f, 0.f, 0.f };
	m_camera->GetViewAxis (axisBefore);
	readEye (eyeBefore);
	const ::Model* const selectedBefore = m_document.Selected ();

	const bool                opensLeft = m_options.dragBump == GestureOwner::Left;
	const ui::MouseButton     owner  = opensLeft ? ui::MouseButton::Left : ui::MouseButton::Right;
	const ui::MouseButton     bumped = opensLeft ? ui::MouseButton::Right : ui::MouseButton::Left;

	const auto at = [] (ui::MouseEvent::Kind kind, ui::MouseButton button, int x, int y)
	{
		ui::MouseEvent event;
		event.kind    = kind;
		event.button  = button;
		event.windowX = x;
		event.windowY = y;
		return event;
	};

	const int fromX = m_options.dragFromX;
	const int fromY = m_options.dragFromY;
	const int toX   = m_options.dragToX;
	const int toY   = m_options.dragToY;

	// LA SEQUENCE, ET L'ORDRE EST LA MESURE. Le second bouton est enfonce PUIS
	// RELACHE sans que le curseur bouge : c'est la forme qui tue le geste sous
	// l'ancienne forme -- le relachement ferme l'unique slot du trackball -- et
	// c'est aussi celle dont le deplacement nul passe sous le seuil
	// clic/glisser.
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Move, owner, fromX, fromY));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Press, owner, fromX, fromY));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Press, bumped, fromX, fromY));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Release, bumped, fromX, fromY));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Move, owner, toX, toY));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Release, owner, toX, toY));
	ProcessInput ();

	float axisAfter[3] = { 0.f, 0.f, 0.f };
	float eyeAfter[3]  = { 0.f, 0.f, 0.f };
	m_camera->GetViewAxis (axisAfter);
	readEye (eyeAfter);

	const bool ok = ReportDragBump (m_options.dragBump, fromX, fromY, toX, toY,
	                                AngleBetweenDeg (axisBefore, axisAfter),
	                                m_document.Selected () != selectedBefore, eyeBefore, eyeAfter);
	const int journal = ReportRmlJournal ();
	if (journal != 0)
		return journal;
	return ok ? 0 : 1;
}

int Application::RunFocusLoss ()
{
	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr,
		              "peles : aucune image rendue : la vue n'a pas de surface. MESURE NON "
		              "CONCLUANTE.\n");
		return 1;
	}

	const int x = m_options.focusLossX;
	const int y = m_options.focusLossY;

	const auto at = [x, y] (ui::MouseEvent::Kind kind)
	{
		ui::MouseEvent event;
		event.kind    = kind;
		event.button  = ui::MouseButton::Left;
		event.windowX = x;
		event.windowY = y;
		return event;
	};

	// ⚠ LA SELECTION EST REMISE A VIDE AVANT CHAQUE MOITIE. SelectFromPixel sort
	// sans rien faire quand le modele touche EST DEJA selectionne, et un fichier
	// passe en argument l'est par son chargement : sans cette remise, les deux
	// moitiees rendraient « rien de selectionne » quoi qu'il arrive.
	const auto clearSelection = [this] ()
	{
		m_document.Select (nullptr);
		m_ui->ShowModelInfo (nullptr);
	};

	// TEMOIN : la sequence NORMALE, sur le meme pixel. Elle doit selectionner.
	clearSelection ();
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Move));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Press));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Release));
	ProcessInput ();
	const bool controlSelected = m_document.Selected () != nullptr;
	const bool ownerWasViewport = m_lastPressOwner == ui::InputRouter::Owner::Viewport;

	// LA SEQUENCE DE LA PERTE DE FOCUS, DANS L'ORDRE QUE glfw PRODUIT :
	// l'annulation PUIS le relachement synthetique. `_glfwInputWindowFocus`
	// appelle le rappel de focus avant de synthetiser les relachements ; les
	// injecter dans l'ordre inverse mesurerait une sequence qui n'arrive pas.
	clearSelection ();
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Move));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Press));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Cancel));
	m_pendingEvents.push_back (at (ui::MouseEvent::Kind::Release));
	ProcessInput ();

	const bool cancelSelected = m_document.Selected () != nullptr;

	return ReportFocusLoss (x, y, ownerWasViewport, controlSelected, cancelSelected,
	                        m_input->Holding ())
	           ? 0
	           : 1;
}

int Application::RunDumpCopy ()
{
	// ⚠ LES TOUCHES PASSENT PAR LA FILE DE L'HOTE ET PAR ProcessInput, comme
	// celles d'un clavier reel. Les remettre directement a l'arbitre sauterait
	// le drainage de la file, et l'instrument ne pourrait plus echouer sur
	// l'absence de ce drainage.
	const KeySink sendKey = [this] (const ui::KeyEvent& event)
	{
		m_pendingKeys.push_back (event);
		ProcessInput ();
	};

	return DumpCopy (*m_rml, *m_window, *m_input, sendKey, kViewportElementId,
	                 m_framebufferWidth, m_framebufferHeight, m_options.copySelector.c_str ());
}

int Application::RunDumpPick ()
{
	// ⚠ UNE IMAGE COMPLETE D'ABORD, ET C'EST TOUT LE POINT DE CET INSTRUMENT.
	// Les dimensions que le picking emploie sont posees par la PHASE 5 ; le
	// rectangle auquel elles se comparent est lu par la PHASE 3. Les poser ici
	// reviendrait a mesurer l'instrument, et le produit pouvait alors etre faux
	// sans qu'aucun des six criteres ne rougisse.
	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr,
		              "peles : aucune image rendue : la vue n'a pas de surface. MESURE NON "
		              "CONCLUANTE.\n");
		return 1;
	}

	return DumpPick (m_viewRect, *m_camera, m_framebufferWidth, m_framebufferHeight,
	                 m_options.pickX, m_options.pickY);
}

int Application::RunAltPivot ()
{
	// UNE IMAGE COMPLETE D'ABORD, ET ELLE EST INDISPENSABLE : c'est la phase 3
	// qui lit le rectangle de la vue, et la phase 5 qui pose les dimensions de la
	// camera. Sans elle, l'arbitrage n'aurait aucun rectangle a soustraire et le
	// picking aucun repere.
	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr,
		              "peles : aucune image rendue : la vue n'a pas de surface, il n'y a rien a "
		              "viser. MESURE NON CONCLUANTE.\n");
		return 1;
	}

	float before[3] = { 0.f, 0.f, 0.f };
	m_camera->GetPivot (before);

	// ⚠ LES DEUX RELEVES SONT REMIS A LEUR VALEUR « RIEN N'A COURU ». Sans cela,
	// un geste qui n'atteint jamais la vue laisserait l'issue d'un geste
	// precedent, et le verdict conclurait sur une mesure qu'il n'a pas prise.
	m_altPivotStatus = view3d::PivotPickStatus::NoView;
	m_lastPressOwner = ui::InputRouter::Owner::None;

	// LA SEQUENCE EST CELLE D'UNE SOURIS : un deplacement pose le survol, puis
	// l'enfoncement porte le drapeau Alt.
	ui::MouseEvent move;
	move.kind    = ui::MouseEvent::Kind::Move;
	move.windowX = m_options.altPivotX;
	move.windowY = m_options.altPivotY;

	ui::MouseEvent press;
	press.kind      = ui::MouseEvent::Kind::Press;
	press.button    = ui::MouseButton::Left;
	press.windowX   = m_options.altPivotX;
	press.windowY   = m_options.altPivotY;
	press.modifiers = ui::modifier::kAlt;

	ui::MouseEvent release = press;
	release.kind           = ui::MouseEvent::Kind::Release;

	m_pendingEvents.push_back (move);
	m_pendingEvents.push_back (press);
	m_pendingEvents.push_back (release);
	ProcessInput ();

	float after[3] = { 0.f, 0.f, 0.f };
	m_camera->GetPivot (after);

	const bool ok =
	    ReportAltPivot (m_options.altPivotX, m_options.altPivotY,
	                    m_lastPressOwner == ui::InputRouter::Owner::Viewport, m_altPivotStatus,
	                    before, after);
	const int  journal = ReportRmlJournal ();
	if (journal != 0)
		return journal;
	return ok ? 0 : 1;
}

bool Application::RunDumpMode (int& exitCode)
{
	// ⚠ LES VIDAGES GEOMETRIQUES SUIVENT LA MEME REGLE QUE --dump-layout : ils
	// appellent Update(), donc ils emettent exactement les messages que le
	// controle de creation ne peut pas voir, et AUCUN chemin de sortie ne rend un
	// code sans avoir relu le journal.
	if (m_options.dumpPick)
	{
		const int code    = RunDumpPick ();
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.altPivot)
	{
		exitCode = RunAltPivot ();
		return true;
	}

	if (m_options.click)
	{
		exitCode = RunClick ();
		return true;
	}

	if (m_options.drag)
	{
		exitCode = RunDrag ();
		return true;
	}

	if (m_options.dragSplitter)
	{
		const int code    = RunDragSplitter ();
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.inputScript)
	{
		const int code = DumpInputScript (*m_rml, *m_input, kViewportElementId,
		                                  m_framebufferWidth, m_framebufferHeight);
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.focusLoss)
	{
		const int code    = RunFocusLoss ();
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.dumpCopy)
	{
		const int code    = RunDumpCopy ();
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.dumpScroll)
	{
		const int code =
		    DumpScroll (*m_rml, *m_input, kViewportElementId, m_framebufferWidth,
		                m_framebufferHeight, m_options.scrollContainer.c_str (),
		                m_options.scrollContent.c_str ());
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : code;
		return true;
	}

	if (m_options.dumpLayout)
	{
		const int layout = DumpLayout (*m_rml,
		                               m_options.layoutElement.empty ()
		                                   ? kViewportElementId
		                                   : m_options.layoutElement.c_str (),
		                               m_options.layoutWidth, m_options.layoutHeight);

		// ⚠ LE JOURNAL EST RELU ICI AUSSI, ET C'EST LE CHEMIN QUI EN A LE PLUS
		// BESOIN. DumpLayout appelle Update(), donc il declenche exactement les
		// messages que le controle de creation ne peut pas voir ; et
		// --layout-element est le SEUL instrument qui etablisse qu'un panneau a
		// du contenu mis en page. Sans cette relecture, l'unique preuve de mise
		// en page etait aussi le seul verdict a ne rendre aucun compte.
		//
		// LE JOURNAL PREVAUT, comme dans Report.
		const int journal = ReportRmlJournal ();
		exitCode          = journal != 0 ? journal : layout;
		return true;
	}

	return false;
}

} // namespace app
} // namespace peles
