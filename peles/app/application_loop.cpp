// ---------------------------------------------------------------------------
//  LES INSTRUMENTS QUI AGISSENT DEPUIS LA BOUCLE, ET LE VERDICT
// ---------------------------------------------------------------------------
//
// Chacun de ces six instruments agit sur une image REELLEMENT rendue --
// basculer l'arbre, remplacer la scene, ajouter un second fichier, reselectionner,
// redimensionner la fenetre -- et `Report` lit ce qu'ils ont produit. Aucun n'a
// de sens sur une image isolee ; tous ont besoin de l'hote entier.
//
// Application est declaree dans app/application.h, commune aux quatre unites.

#include "gl_wrapper.h"

#include "application.h"
#include "application_internal.h"

#include "dump_modes.h"
#include "input_router.h"
#include "mesh_data_manager.h"
#include "model_info.h"
#include "rml_host.h"
#include "scene_renderer.h"
#include "viewport_rect.h"
#include "vmodels.h"
#include "window_glfw.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Types.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace peles
{
namespace app
{

// ---------------------------------------------------------------------------
//  La boucle, et le verdict
// ---------------------------------------------------------------------------

void Application::ChurnTreeOnce (LoopOutcome& outcome)
{
	// UN BASCULEMENT PAR IMAGE RENDUE : le vecteur lie retrecit, puis regrandit,
	// pendant que RmlUi continue a dessiner ce qu'il contient.
	if (outcome.churnToggles >= m_options.churnTree || m_ui->Model ().treeRows.empty ())
		return;

	m_ui->ToggleRow (m_ui->Model ().treeRows.front ().id);
	++outcome.churnToggles;

	const int rows       = (int)m_ui->Model ().treeRows.size ();
	outcome.churnMinRows = rows < outcome.churnMinRows ? rows : outcome.churnMinRows;
	outcome.churnMaxRows = rows > outcome.churnMaxRows ? rows : outcome.churnMaxRows;
}

void Application::ReopenIfRequested (LoopOutcome& outcome)
{
	// ⚠ APRES LA PREMIERE IMAGE RENDUE, ET C'EST TOUT L'INTERET : le maillage
	// courant est alors REELLEMENT enregistre aupres du moteur de rendu, donc la
	// destruction annoncee libere de vrais objets GL. Le contexte de la fenetre
	// est courant ici, ce que la precondition d'OpenModel exige.
	if (m_options.reopenPath.empty ())
		return;

	const std::string next = m_options.reopenPath;
	m_options.reopenPath.clear ();

	const int registeredBefore = (int)m_scene->RegisteredMeshCount ();

	// ⚠ LE RETOUR D'OpenModel EST PROPAGE, ET C'EST INDISPENSABLE. OpenModel vide
	// la scene AVANT de charger et ne la restaure pas en cas d'echec : un fichier
	// de remplacement illisible rend donc « avant = 1, apres = 0 », c'est-a-dire
	// exactement la signature d'un contrat de destruction TENU. Le verdict serait
	// vert sur une execution qui n'a pas fait ce qui etait demande -- ce que le
	// contrat de sortie de main.cpp interdit.
	const bool opened = OpenModel (next);
	if (!ReportSceneReplacement (opened, registeredBefore, (int)m_scene->RegisteredMeshCount ()))
		outcome.replacementFailed = true;
}

void Application::OpenSecondIfRequested (LoopOutcome& outcome)
{
	// ⚠ CET INSTRUMENT PASSE PAR LE CHEMIN DE L'INTERFACE, ET PAS A COTE. Il
	// appelle RequestOpen -- exactement ce qu'un double-clic sur la liste
	// appelle -- donc il exerce l'image de transition ET l'ajout a la scene. Un
	// second appel direct a AddModel mesurerait une replique du chemin reel.
	if (m_options.openSecondPath.empty ())
		return;

	const std::filesystem::path next (m_options.openSecondPath);
	m_options.openSecondPath.clear ();
	outcome.secondAttempted = true;
	outcome.secondExpected  = next.filename ().string ();

	// LE CHARGEMENT EST HONORE A L'IMAGE SUIVANTE, PAS ICI. Relever le verdict
	// maintenant le prendrait sur un etat que l'image de transition n'a pas
	// encore quitte ; c'est RecordSecondFile, appelee apres FinishPendingOpen,
	// qui mesure.
	RequestOpen (next);
}

void Application::RecordSecondFile (LoopOutcome& outcome)
{
	outcome.secondOpened     = true;
	outcome.secondModelCount = (int)m_document.Models ().size ();

	// ⚠ LA LIGNE LUE EST CELLE DU MODELE LIE, PAS CELLE DU DOCUMENT. Le critere
	// porte sur ce que les PANNEAUX decrivent : lire le document rendrait le
	// verdict vert meme si la publication vers l'interface avait ete oubliee.
	const std::vector<core::InfoRow>& rows = m_ui->Model ().infoRows;
	const std::size_t                 line = (std::size_t)core::InfoLine::File;
	if (line < rows.size ())
		outcome.secondFileLine = rows[line].value;
}

void Application::ReselectOnce (LoopOutcome& outcome)
{
	// UNE SELECTION PAR IMAGE RENDUE, ET TOUJOURS LE MEME FICHIER.
	//
	// ⚠ LE MEME FICHIER, PAS UN AUTRE, ET C'EST TOUT LE POINT : a contenu
	// identique, l'empreinte memoire doit revenir au meme chiffre. Sur deux
	// fichiers differents, une croissance serait indiscernable d'une difference
	// de taille.
	if (m_options.reselect <= 0 || m_options.modelPath.empty ())
		return;

	// ⚠ LE RELEVE EST PRIS A L'IMAGE QUI SUIT LA SELECTION, PAS A CELLE QUI LA
	// PORTE, ET C'EST UNE CONDITION DE VALIDITE. Un maillage n'est enregistre
	// aupres du moteur qu'a son PREMIER DESSIN : releve dans l'image du
	// chargement, le compteur vaudrait zero a chaque selection -- identique
	// d'une fois sur l'autre, donc vert, sans avoir rien observe.
	if (outcome.selectionSamples.size () < (std::size_t)outcome.selections)
	{
		SelectionSample sample;
		sample.cacheEntries     = (int)MeshDataManager::GetInstance ().CachedEntryCount ();
		sample.registeredMeshes = (int)m_scene->RegisteredMeshCount ();

		// ⚠ LES TROIS VECTEURS LIES, RELEVES SUR LE MODELE QUE RmlUi LIT. Les
		// deux compteurs ci-dessus sont indexes par MAILLAGE : sur un fichier a
		// 60 materiaux et un seul maillage, ils valent « 1, 1 » quel que soit le
		// nombre de materiaux, donc la grandeur que le critere nomme n'entrait
		// dans aucun des deux.
		const ui::ViewModel& model = m_ui->Model ();
		sample.infoRows            = (int)model.infoRows.size ();
		sample.meshRows            = (int)model.meshRows.size ();
		sample.materialRows        = (int)model.materialRows.size ();

		ReadProcessFootprintKb (sample.footprintKb);
		outcome.selectionSamples.push_back (sample);
		return;
	}

	if (outcome.selections >= m_options.reselect)
		return;

	if (!OpenModel (m_options.modelPath))
	{
		// L'echec laisse le compteur en arriere du nombre demande, donc le
		// verdict devient NON CONCLUANT au lieu de bon.
		m_options.reselect = outcome.selections;
		return;
	}

	++outcome.selections;

	// ⚠ L'ARBRE EST MESURE APRES LA SELECTION, PAS AVANT. Son etat de
	// completude fait partie du verdict : une mesure prise sur un arbre que le
	// plafond a ecourte ne dirait rien du cas a 98 materiaux.
	if (::Model* model = m_document.Selected ())
		outcome.selectionMaterials = core::CountMaterials (*model);

	const std::vector<core::InfoRow>& materialRows = m_ui->Model ().materialRows;
	outcome.selectionTruncated =
	    !materialRows.empty () &&
	    materialRows.back ().styleClass.find ("overflow") != std::string::npos;
}

void Application::ResizeIfRequested ()
{
	if (!m_options.resize || m_resize.issued)
		return;

	m_resize.before = m_viewRect;
	// ⚠ ARME AVANT L'APPEL : si le systeme peint pendant SetWindowPos plutot
	// qu'au PollEvents suivant, l'image du rappel doit etre comptee aussi.
	m_resize.issued = true;

	// UNE DEMANDE EGALE A LA TAILLE COURANTE NE REDIMENSIONNE RIEN : le systeme
	// n'envoie ni WM_SIZE ni WM_PAINT, et le verdict accuserait le rappel.
	if (m_options.resizeWidth == m_framebufferWidth &&
	    m_options.resizeHeight == m_framebufferHeight)
	{
		m_resize.sameAsCurrent = true;
		return;
	}

	m_window->SetSize (m_options.resizeWidth, m_options.resizeHeight);
}

void Application::RecordRefreshFrame (int framebufferWidth, int framebufferHeight)
{
	++m_refreshFrames;

	if (!m_resize.issued || m_resize.followed)
		return;

	// ⚠ COMPTEE QUELLE QUE SOIT SA TAILLE : le systeme peut borner la fenetre
	// (ecran plus petit que la demande) ou la mettre a l'echelle. Filtrer sur la
	// taille demandee ferait passer une image dessinee pour un rappel absent.
	++m_resize.refreshAfterRequest;
	m_resize.refreshWidth  = framebufferWidth;
	m_resize.refreshHeight = framebufferHeight;
	m_resize.refreshRect   = m_viewRect;
}

void Application::RecordResizeFollowUp ()
{
	// L'IMAGE RELEVEE EST LA PREMIERE QUE LA BOUCLE REND APRES SetSize : son
	// PollEvents a distribue WM_SIZE et WM_PAINT, donc l'image du rappel, s'il
	// y en a une, l'a precedee.
	if (!m_resize.issued || m_resize.followed || m_resize.sameAsCurrent)
		return;

	m_resize.followed          = true;
	m_resize.after             = m_viewRect;
	m_resize.framebufferWidth  = m_framebufferWidth;
	m_resize.framebufferHeight = m_framebufferHeight;

	const Rml::Vector2i dimensions = m_rml->Context ()->GetDimensions ();
	m_resize.contextWidth          = dimensions.x;
	m_resize.contextHeight         = dimensions.y;
}

namespace
{

bool SameRect (const ui::ViewportRect& a, const ui::ViewportRect& b)
{
	return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void PrintRect (const char* label, const ui::ViewportRect& rect)
{
	std::printf ("  %-22s x=%d y=%d l=%d h=%d\n", label, rect.x, rect.y, rect.width, rect.height);
}

} // namespace

bool Application::ReportResize () const
{
	const ResizeProbe& probe = m_resize;

	if (probe.sameAsCurrent)
	{
		std::printf ("REDIMENSIONNEMENT NON CONCLUANT : la demande %dx%d est la taille courante "
		             "du tampon ; rien n'a ete redimensionne.\n",
		             m_options.resizeWidth, m_options.resizeHeight);
		return false;
	}

	if (!probe.followed)
	{
		std::printf ("REDIMENSIONNEMENT NON CONCLUANT : aucune image rendue apres la demande de "
		             "%dx%d.\n",
		             m_options.resizeWidth, m_options.resizeHeight);
		return false;
	}

	std::printf ("redimensionnement : demande %dx%d, tampon %dx%d, contexte RmlUi %dx%d\n",
	             m_options.resizeWidth, m_options.resizeHeight, probe.framebufferWidth,
	             probe.framebufferHeight, probe.contextWidth, probe.contextHeight);
	PrintRect ("#viewport avant :", probe.before);
	PrintRect ("#viewport, rappel :", probe.refreshRect);
	PrintRect ("#viewport, apres :", probe.after);
	std::printf ("images du rappel de rafraichissement apres la demande, avant l'image "
	             "suivante : %d, la derniere a %dx%d (sur toute l'execution : %d)\n",
	             probe.refreshAfterRequest, probe.refreshWidth, probe.refreshHeight,
	             m_refreshFrames);

	std::string faults;
	const auto  fault = [&faults] (const char* text)
	{
		faults += faults.empty () ? "" : " ; ";
		faults += text;
	};

	// LES CRITERES PORTENT SUR LA TAILLE OBTENUE, pas sur la demande : c'est la
	// seule que le produit a vue.
	if (probe.contextWidth != probe.framebufferWidth ||
	    probe.contextHeight != probe.framebufferHeight)
		fault ("le contexte RmlUi n'a pas la taille du tampon");
	if (SameRect (probe.after, probe.before))
		fault ("le rectangle #viewport n'a pas suivi");
	if (probe.after.width <= 0 || probe.after.height <= 0 || probe.after.x < 0 ||
	    probe.after.y < 0 || probe.after.x + probe.after.width > probe.framebufferWidth ||
	    probe.after.y + probe.after.height > probe.framebufferHeight)
		fault ("le rectangle #viewport sort du tampon");
	if (probe.refreshAfterRequest == 0)
		fault ("aucune image rendue par le rappel de rafraichissement : pendant un "
		       "redimensionnement au bord, la fenetre ne serait pas redessinee");
	else if (probe.refreshWidth != probe.framebufferWidth ||
	         probe.refreshHeight != probe.framebufferHeight)
		fault ("l'image du rappel n'a pas la taille de l'image suivante");
	else if (!SameRect (probe.refreshRect, probe.after))
		fault ("l'image du rappel n'a pas le rectangle de l'image suivante");

	if (!faults.empty ())
	{
		std::printf ("REDIMENSIONNEMENT ECHOUE : %s.\n", faults.c_str ());
		return false;
	}

	// ⚠ TOUT A SUIVI, MAIS PAS VERS LA TAILLE DEMANDEE : fenetre bornee par
	// l'ecran, ou facteur d'echelle different de 1. Le produit est conforme sur
	// la taille obtenue ; l'instrument, lui, n'a pas mesure ce qu'on lui a
	// demande.
	if (probe.framebufferWidth != m_options.resizeWidth ||
	    probe.framebufferHeight != m_options.resizeHeight)
	{
		std::printf ("REDIMENSIONNEMENT NON CONCLUANT : tampon obtenu %dx%d au lieu de %dx%d "
		             "(fenetre bornee par l'ecran, ou facteur d'echelle different de 1) ; "
		             "tout a suivi la taille obtenue.\n",
		             probe.framebufferWidth, probe.framebufferHeight, m_options.resizeWidth,
		             m_options.resizeHeight);
		return false;
	}

	std::printf ("REDIMENSIONNEMENT OK : tampon, contexte RmlUi et rectangle #viewport suivent, "
	             "et le rappel de rafraichissement a dessine la nouvelle taille avant la "
	             "boucle.\n");
	return true;
}

int Application::ReportRmlJournal () const
{
	// LE JOURNAL DE RmlUi EST RELU AVANT TOUTE SORTIE, ET PAS SEULEMENT A LA
	// CREATION.
	//
	// ⚠ LE CONTROLE DE CreateUi NE VOIT QUE CE QUI EST REFUSE AU CHARGEMENT DU
	// DOCUMENT. Une valeur liee que RCSS ne sait pas analyser -- ce que
	// `data-style-background-color` rend possible -- n'est refusee qu'a la
	// PREMIERE MISE A JOUR, donc apres ce controle. MESURE : une pastille vide
	// produit 122 « Syntax error parsing inline property declaration
	// 'background-color: ;' » et le programme sortait en 0.
	//
	// ⚠ AUCUN CHEMIN DE SORTIE DE Run() NE REND UN CODE SANS AVOIR RELU CE
	// JOURNAL. C'est la regle, et elle existe parce que --dump-layout y
	// echappait : il appelle Update() -- le moment exact ou ces messages sont
	// emis -- puis rendait son code directement. Sur le MEME binaire, la
	// pastille vide rendait 122 avertissements et EXIT=3 sous --frames 2, et
	// EXIT=0 sans un mot sous --dump-layout, c'est-a-dire par le seul instrument
	// qui exerce la mise en page des trois panneaux.
	//
	// ⚠ MEME CODE DE SORTIE QUE LA FAUTE DE CREATION, ET IL PREVAUT : la cause
	// est de la meme famille -- un document RML ou une feuille RCSS a corriger,
	// sans recompiler -- et c'est l'information dont l'operateur a besoin.
	// MESURE DU FAUX POSITIF : sur le document du produit, `--frames 2`,
	// `--frames 60`, `--frames 2 --debugger` et `--frames 10 --churn-tree 5`
	// rendent ZERO message de niveau LT_WARNING ou superieur.
	const int rejected = m_rml->ErrorCount () + m_rml->WarningCount ();
	if (rejected == 0)
		return 0;

	std::fprintf (stderr,
	              "peles : RmlUi a refuse %d element(s) PENDANT L'EXECUTION (%d erreur(s), "
	              "%d avertissement(s)). Premier message : %s\n",
	              rejected, m_rml->ErrorCount (), m_rml->WarningCount (),
	              m_rml->FirstProblem ().c_str ());
	return 3;
}

int Application::Report (const FrameAudit& audit, const LoopOutcome& outcome)
{
	int exitCode = 0;

	// LA CONCORDANCE DU GL_VIEWPORT N'EST PAS DERRIERE UNE OPTION : elle est
	// mesuree a chaque image et son echec sort toujours. Seul son verdict VERT
	// est reserve aux executions instrumentees, ou il a un lecteur.
	if (m_viewportTracker.Diverged ())
	{
		m_viewportTracker.Report ();
		exitCode = 1;
	}
	else if (m_options.frames > 0)
	{
		m_viewportTracker.Report ();
	}

	if (m_options.stateAudit && !ReportStateAudit (audit))
		exitCode = 1;

	if (outcome.replacementFailed)
		exitCode = 1;

	if (m_options.resize && !ReportResize ())
		exitCode = 1;

	if (outcome.secondAttempted &&
	    !ReportSecondFile (outcome.secondOpened, outcome.secondFileLine,
	                       outcome.secondModelCount, outcome.secondExpected,
	                       std::filesystem::path (m_options.modelPath).filename ().string ()))
		exitCode = 1;

	if (m_options.churnTree > 0 &&
	    !ReportTreeChurn (m_options.churnTree, outcome.churnToggles, outcome.churnMinRows,
	                      outcome.churnMaxRows))
		exitCode = 1;

	// ⚠ LE NOMBRE DEMANDE EST CELUI DE L'OPTION D'ORIGINE, PAS CELUI QUE LA
	// BOUCLE A RABAISSE. ReselectOnce abaisse m_options.reselect quand une
	// ouverture echoue, precisement pour que le verdict le voie ; le lire ici
	// rendrait la comparaison toujours vraie.
	if (m_requestedReselections > 0 &&
	    !ReportSelectionMemory (m_requestedReselections, outcome.selections,
	                            outcome.selectionMaterials, outcome.selectionTruncated,
	                            outcome.selectionSamples))
		exitCode = 1;

	if (m_options.dumpStatus &&
	    !ReportLoadStatus (m_options.modelPath, m_modelLoaded, m_loadFailureName,
	                       m_loadFailureReason, m_ui->Model ().status))
		exitCode = 1;

	if (m_options.dumpTiming)
		ReportTiming (outcome.frameSeconds, outcome.updateSeconds,
		              (int)m_ui->Model ().treeRows.size (), outcome.timingExcluded);

	if (m_options.readPixel &&
	    !m_pixelProbe.Report (m_options.pixelX, m_options.pixelY, (int)m_document.Models ().size (),
	                          m_viewportTracker.HasLastActual () ? m_viewportTracker.LastActual ()
	                                                             : nullptr))
		exitCode = 1;

	if (const int journal = ReportRmlJournal ())
		exitCode = journal;

	// UNE EXECUTION ECOURTEE N'EST PAS UNE EXECUTION REUSSIE. Sans cette ligne,
	// « 120 images demandees, 3 rendues » sortirait en 0 avec une mediane prise
	// sur trois images.
	if (m_options.frames > 0 && outcome.renderedFrames < m_options.frames)
	{
		std::fprintf (stderr,
		              "peles : %d image(s) demandee(s), %d rendue(s) -- fenetre fermee ou sans "
		              "surface de dessin.\n",
		              m_options.frames, outcome.renderedFrames);
		exitCode = 1;
	}

	return exitCode;
}

} // namespace app
} // namespace peles
