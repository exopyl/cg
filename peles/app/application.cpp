// ORDRE D'INCLUSION CONTRAINT : gl_wrapper.h est le SEUL en-tete GL de cette
// unite et il vient en premier. Les en-tetes de RmlUi inclus ensuite n'en
// tirent aucun -- le renderer GL2, qui lui en tire un, est enferme derriere
// platform/rml_render_interface.h.
//
// ⚠ NOMINMAX est pose par la configuration, pas ici : gl_wrapper.h tire
// <windows.h>, qui definit min et max comme macros, et les en-tetes de RmlUi
// appellent std::min et std::max -- l'expansion produirait des erreurs de
// syntaxe dont le diagnostic pointerait l'en-tete tiers, pas la cause.
#include "gl_wrapper.h"

#include "application.h"
#include "application_internal.h"

#include "view3d/camera.h"
#include "diagnostics.h"
#include "dump_modes.h"
#include "gl_state_fence.h"
#include "input_router.h"
#include "panel_splitters.h"
#include "rml_host.h"
#include "scene_renderer.h"
#include "viewport_rect.h"
#include "vmodels.h"
#include "window_glfw.h"

#include <memory>

namespace peles
{
namespace app
{

namespace
{

// ---------------------------------------------------------------------------
//  LES DEUX VOCABULAIRES D'ENTREE, RELIES PAR LE COMPILATEUR
// ---------------------------------------------------------------------------
//
// ⚠ peles_platform NE LIE PAS peles_ui, ET NE LE LIERA PAS : le pare-feu
// d'inclusion l'interdit. Les deux cibles ont donc necessairement leur propre
// encodage des boutons, des modificateurs ET DES TOUCHES. Cette unite est la
// SEULE qui voie les deux en-tetes ; c'est donc ici, et nulle part ailleurs, que
// leur coincidence peut cesser d'etre une convention pour devenir une
// verification. SEIZE lignes : 3 + 3 + 10.
//
// Sans ces lignes, une divergence d'un seul bit ferait paner la vue au lieu de
// l'orbiter, ou ferait poser le pivot sur un Ctrl + clic. Aucun diagnostic.
static_assert ((int)ui::MouseButton::Left == platform::kMouseButtonLeft,
               "le bouton gauche n'a pas le meme code des deux cotes");
static_assert ((int)ui::MouseButton::Right == platform::kMouseButtonRight,
               "le bouton droit n'a pas le meme code des deux cotes");
static_assert ((int)ui::MouseButton::Middle == platform::kMouseButtonMiddle,
               "le bouton du milieu n'a pas le meme code des deux cotes");
static_assert (ui::modifier::kShift == platform::kModShift,
               "Maj n'a pas le meme bit des deux cotes");
static_assert (ui::modifier::kControl == platform::kModControl,
               "Ctrl n'a pas le meme bit des deux cotes");
static_assert (ui::modifier::kAlt == platform::kModAlt,
               "Alt n'a pas le meme bit des deux cotes");

// LES DIX TOUCHES, VERIFIEES COMME LES SIX AUTRES ENTREES.
//
// ⚠ LE PONT EST UN CAST NU : `event.key = (ui::Key)key` dans
// app/application_create.cpp. Un cast ne verifie rien -- il produit une valeur
// d'enumeration quelle que soit la correspondance -- donc sans ces dix lignes
// une divergence entre les `kKey*` de platform/window_glfw.h et `ui::Key` ferait
// router Fin pour Debut, ou C pour A. Aucun diagnostic, et le seul symptome
// visible serait une copie qui ne copie pas.
//
// ⚠ ET C'EST AUSSI CE QUI TIENT LA GARANTIE D-5. La ligne « Path » n'est pas
// modifiable parce que SEULES ces dix touches sont routees ; une renumerotation
// silencieuse d'un des deux cotes y ferait entrer une touche modifiante.
static_assert ((int)ui::Key::A == platform::kKeyA, "A n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::C == platform::kKeyC, "C n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::Left == platform::kKeyLeft,
               "Gauche n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::Right == platform::kKeyRight,
               "Droite n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::Up == platform::kKeyUp, "Haut n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::Down == platform::kKeyDown, "Bas n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::Home == platform::kKeyHome,
               "Debut n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::End == platform::kKeyEnd, "Fin n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::PageUp == platform::kKeyPageUp,
               "PagePrec n'a pas le meme code des deux cotes");
static_assert ((int)ui::Key::PageDown == platform::kKeyPageDown,
               "PageSuiv n'a pas le meme code des deux cotes");

} // namespace

Application::Application () = default;

Application::~Application () = default;

std::unique_ptr<Application> Application::Create (const Options& options, CreateError& error)
{
	// ⚠ `new` NU ASSUME : le constructeur est PRIVE, donc std::make_unique ne
	// peut pas l'appeler. Le pointeur est confie a un unique_ptr sur la ligne
	// meme, et aucune autre ligne ne s'intercale.
	std::unique_ptr<Application> application (new Application);
	application->m_options              = options;
	application->m_requestedReselections = options.reselect;

	if (!application->CreateWindowAndGl (options, error))
		return nullptr;
	if (!application->CreateUi (options, error))
		return nullptr;
	application->CreateSceneAndDocument (options);
	application->PopulateBrowser (options);

	// SYNCHRONISATION VERTICALE COUPEE DES QU'UN NOMBRE D'IMAGES EST DEMANDE.
	// Avec elle, la mediane mesuree serait la periode de l'ecran et non le cout
	// de l'image : l'instrument rendrait 16,6 ms quelle que soit la charge, donc
	// exactement la valeur que le critere cherche a ne pas depasser.
	application->m_window->SetSwapInterval (options.frames > 0 ? 0 : 1);

	return application;
}

// ---------------------------------------------------------------------------
//  L'image, et ses huit phases
// ---------------------------------------------------------------------------

bool Application::RenderFrame (FrameAudit* audit, double* updateSeconds)
{
	if (audit != nullptr)
		audit->BeginFrame ();

	// --- Phase 1 : evenements, puis ARBITRAGE --------------------------------
	//
	// ⚠ L'ARBITRAGE EMPLOIE LE RECTANGLE DE L'IMAGE PRECEDENTE, et il ne peut pas
	// faire autrement : l'evenement arrive AVANT l'Update() qui recalculera la
	// mise en page. Un redimensionnement introduit donc un retard d'UNE image sur
	// le picking, et rien de plus.
	m_window->PollEvents ();

	// ⚠ LES DIMENSIONS SONT POSEES AVANT L'ARBITRAGE, ET CE BLOC NE DEPEND
	// D'AUCUN EVENEMENT. Apres lui, RmlUi arbitrerait son survol sur les
	// dimensions de l'image precedente -- un second retard, qui s'ajoute a celui
	// du rectangle et que rien n'oblige a payer.
	int framebufferWidth  = 0;
	int framebufferHeight = 0;
	SyncFramebufferSize (framebufferWidth, framebufferHeight);

	ProcessInput ();

	// LES COMMANDES SONT EXECUTEES AVANT Update(), et c'est ce qui rend l'image
	// de transition possible : le statut pose ici est dessine par l'image en
	// cours, et le chargement n'a lieu qu'apres elle.
	ExecuteUiCommands ();

	if (framebufferWidth <= 0 || framebufferHeight <= 0)
	{
		// Fenetre reduite : il n'y a pas de surface ou dessiner. Rien n'est
		// emis, et surtout pas un glViewport de taille nulle. L'image n'est pas
		// rendue, donc elle ne compte pas.
		//
		// ⚠ LA GARDE EST APRES L'ARBITRAGE, ET NON AVANT : les evenements
		// restent draines, donc la file ne grossit pas pendant que la fenetre
		// est reduite, et aucune commande d'interface n'est perdue. Rien de ce
		// qui court ici n'appelle GL.
		return false;
	}

	return DrawFrame (framebufferWidth, framebufferHeight, audit, updateSeconds,
	                  m_options.readPixel);
}

void Application::SyncFramebufferSize (int& framebufferWidth, int& framebufferHeight)
{
	m_window->GetFramebufferSize (framebufferWidth, framebufferHeight);
	if (framebufferWidth == m_framebufferWidth && framebufferHeight == m_framebufferHeight)
		return;

	m_framebufferWidth  = framebufferWidth;
	m_framebufferHeight = framebufferHeight;
	// La taille remise a RmlUi est celle du TAMPON DE DESSIN, pas de la
	// fenetre : le lot 1 n'emploie aucune unite `dp`, donc les deux reperes
	// doivent coincider.
	m_rml->SetDimensions (framebufferWidth, framebufferHeight);
}

void Application::RenderRefreshFrame ()
{
	// ⚠ APPELEE DEPUIS glfwPollEvents, DONC TOUJOURS DANS LA PHASE 1 D'UNE
	// IMAGE EN COURS : la boucle modale de redimensionnement de Win32 est
	// elle-meme imbriquee dans le glfwPollEvents de RenderFrame. Elle ne fait
	// que ce qui ne depend d'aucun evenement : dimensions, puis phases 2 a 8.
	// Les evenements deja en file restent a l'image qui les a recus, qui les
	// arbitrera apres PollEvents ; les traiter ici ferait executer une commande
	// d'interface depuis un rappel du systeme.
	if (m_inRefreshFrame)
		return;
	m_inRefreshFrame = true;

	// ⚠ ELLE NE DESSINE QUE SI LA TAILLE DIFFERE DE CELLE DE LA DERNIERE IMAGE
	// DESSINEE. Une fenetre restauree ou decouverte -- WM_PAINT sous Win32,
	// Expose sous X11, qui n'est pas filtre sur son compte -- garde sa taille,
	// et l'image de la boucle qui suit le PollEvents la redessine de toute
	// facon : dessiner ici paierait une image complete et un echange de plus.
	// Pendant un redimensionnement, la boucle est suspendue et chaque pas
	// change la taille.
	int framebufferWidth  = 0;
	int framebufferHeight = 0;
	SyncFramebufferSize (framebufferWidth, framebufferHeight);
	const bool resized =
	    framebufferWidth != m_drawnWidth || framebufferHeight != m_drawnHeight;
	if (resized && framebufferWidth > 0 && framebufferHeight > 0)
	{
		DrawFrame (framebufferWidth, framebufferHeight, nullptr, nullptr, false);
		RecordRefreshFrame (framebufferWidth, framebufferHeight);
	}

	m_inRefreshFrame = false;
}

bool Application::DrawFrame (int framebufferWidth, int framebufferHeight, FrameAudit* audit,
                             double* updateSeconds, bool readPixel)
{
	// --- Phase 2 : mise a jour de l'interface --------------------------------
	const double updateStarted = platform::Window::ElapsedSeconds ();
	m_rml->Update ();

	// LES BORNES DES PANNEAUX REDIMENSIONNES SONT REAPPLIQUEES SUR LA MISE EN
	// PAGE QUI VIENT D'ETRE RESOLUE, ET AVANT LA PHASE 3. Une fenetre plus petite
	// peut ne plus laisser kMinViewportExtent a la vue : le panneau est alors
	// reduit, et la mise en page refaite dans la MEME image -- la phase 3 ne lit
	// jamais un rectangle que la correction va changer. Sans panneau
	// redimensionne, Enforce rend false d'emblee et rien n'est refait.
	for (int pass = 0; pass < ui::PanelSplitters::kCount && m_splitters->Enforce (); ++pass)
		m_rml->Update ();

	if (updateSeconds != nullptr)
		*updateSeconds = platform::Window::ElapsedSeconds () - updateStarted;

	// --- Phase 3 : lecture du rectangle de la vue ----------------------------
	// APRES Update(), jamais avant : la geometrie lue serait celle de l'image
	// precedente.
	ui::ViewportRect             rect;
	const ui::ViewportRectStatus status =
	    ui::ReadViewportRect (m_rml->DocumentRoot (), kViewportElementId, rect);

	if (status == ui::ViewportRectStatus::ElementMissing && !m_viewportLossReported)
	{
		// L'element etait la a l'initialisation : sa disparition en cours de
		// route est un defaut du document, pas un etat normal. Une seule ligne,
		// sans quoi le journal serait illisible a 60 images par seconde.
		m_viewportLossReported = true;
		cgre::Logf ("peles : l'element \"%s\" a disparu du document ; la vue 3D n'est plus "
		            "dessinee.",
		            kViewportElementId);
	}

	// LE RECTANGLE EST REMIS A L'ARBITRE ICI, ET SEULEMENT S'IL EST VALIDE. Un
	// rectangle absent ou degenere laisse le precedent en place : le remplacer
	// par (0,0,0,0) ferait passer des coordonnees FENETRE pour des coordonnees
	// de vue le temps d'une image.
	if (status == ui::ViewportRectStatus::Ok)
	{
		m_viewRect    = rect;
		m_hasViewRect = true;
		m_input->SetViewportRect (rect);
	}

	// --- Phase 4 : L'HOTE EFFACE, PAS cgre -----------------------------------
	glViewport (0, 0, framebufferWidth, framebufferHeight);
	glDisable (GL_SCISSOR_TEST);
	glClearColor (0.12f, 0.12f, 0.14f, 1.f);
	glClearStencil (0);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// --- Phase 5 : la vue 3D, sous cloture -----------------------------------
	// ⚠ Rien a dessiner N'EST PAS un repli sur le plein cadre : dessiner la vue
	// partout serait indiscernable du succes.
	const bool hasViewport = status == ui::ViewportRectStatus::Ok;
	if (hasViewport)
	{
		const ui::GlViewport view = ui::ToGlViewport (rect, framebufferHeight);
		const GlStateFence   fence (audit != nullptr ? &audit->scene : nullptr);
		m_scene->Draw (*m_camera, view.x, view.y, view.width, view.height);
		// RELEVE A L'INTERIEUR DE LA CLOTURE : GL_VIEWPORT fait partie de la
		// pile d'attributs, donc le depilement l'aurait deja ramene au plein
		// cadre et la comparaison porterait sur autre chose que la vue.
		m_viewportTracker.Check (view);
	}

	// --- Phase 6 : retour au plein cadre -------------------------------------
	// ⚠ RENORMALISATION, PAS MESURE : la cloture de la phase 5 a deja restaure
	// ces trois etats, et c'est le releve de la phase 5 qui l'etablit -- pas ces
	// trois lignes, qui masqueraient un defaut de cloture si elles etaient la
	// seule chose entre les deux phases.
	glDisable (GL_SCISSOR_TEST);
	glDisable (GL_DEPTH_TEST);
	glViewport (0, 0, framebufferWidth, framebufferHeight);

	// --- Phase 7 : l'interface, sous cloture ---------------------------------
	{
		const GlStateFence fence (audit != nullptr ? &audit->ui : nullptr);
		m_rml->Render ();
	}

	if (readPixel && hasViewport)
		m_pixelProbe.Read (m_options.pixelX, m_options.pixelY);

	// --- Phase 8 : echange ---------------------------------------------------
	m_window->SwapBuffers ();

	m_drawnWidth  = framebufferWidth;
	m_drawnHeight = framebufferHeight;

	return hasViewport;
}

int Application::RunLoop ()
{
	LoopOutcome outcome;
	outcome.churnMinRows = (int)m_ui->Model ().treeRows.size ();
	outcome.churnMaxRows = outcome.churnMinRows;

	// Collecte CONDITIONNELLE : sans --dump-timing, ces vecteurs croitraient de
	// huit octets par image pendant toute une session interactive pour n'etre
	// jamais lus.
	if (m_options.dumpTiming && m_options.frames > 0)
	{
		outcome.frameSeconds.reserve ((size_t)m_options.frames);
		outcome.updateSeconds.reserve ((size_t)m_options.frames);
	}

	FrameAudit  audit;
	FrameAudit* auditPointer = m_options.stateAudit ? &audit : nullptr;

	// LE RAPPEL EST RETIRE A TOUTE SORTIE DE LA BOUCLE, exception comprise : il
	// capture `this`, et une image demandee par le systeme pendant la
	// destruction des membres dessinerait avec des objets morts.
	struct RefreshCallbackGuard
	{
		platform::Window& window;
		~RefreshCallbackGuard () { window.SetRefreshCallback (nullptr); }
	} refreshGuard { *m_window };

	int frame = 0;
	while (!m_window->ShouldClose ())
	{
		if (m_options.frames > 0 && frame >= m_options.frames)
			break;

		++frame;
		double       update   = 0.;
		const int    refreshed = m_refreshFrames;
		const double started   = platform::Window::ElapsedSeconds ();
		const bool   rendered  = RenderFrame (auditPointer, &update);
		const double elapsed   = platform::Window::ElapsedSeconds () - started;

		// ⚠ UNE IMAGE NON RENDUE N'EST NI UNE IMAGE AUDITEE NI UNE MESURE DE
		// TEMPS. Elle a couru, mais elle n'a rien produit : la compter rendrait
		// un AUDIT OK sans comparaison et une mediane de ~0 ms, tous deux verts
		// et tous deux faux.
		if (!rendered)
			continue;

		++outcome.renderedFrames;

		// ⚠ UNE IMAGE QUI A CONTENU UNE IMAGE DU RAPPEL DE RAFRAICHISSEMENT N'EST
		// PAS UNE MESURE DE TEMPS : son PollEvents a rendu une image entiere, donc
		// elle en a dessine deux.
		if (m_options.dumpTiming && m_refreshFrames == refreshed)
		{
			outcome.frameSeconds.push_back (elapsed);
			outcome.updateSeconds.push_back (update);
		}
		else if (m_options.dumpTiming)
		{
			++outcome.timingExcluded;
		}

		// LE RAPPEL DE RAFRAICHISSEMENT N'EST POSE QUE PENDANT LA BOUCLE D'IMAGES,
		// ET APRES SA PREMIERE IMAGE.
		//
		// ⚠ PAS DANS LES MODES « VIDER ET SORTIR » : ils appellent aussi
		// RenderFrame, donc PollEvents, et une image intercalee par le systeme y
		// courrait entre deux evenements injectes.
		//
		// ⚠ PAS AVANT LA PREMIERE IMAGE : le WM_PAINT de l'apparition de la
		// fenetre serait honore dans la phase 1 de l'image 1, et c'est l'image du
		// rappel qui paierait la premiere mise en page -- le cout que « Update(),
		// 1re image » de --dump-timing mesure. L'image 1 le remplace, et elle suit
		// immediatement.
		if (outcome.renderedFrames == 1)
			m_window->SetRefreshCallback ([this] { RenderRefreshFrame (); });

		// ⚠ LE CHARGEMENT DIFFERE EST HONORE EN PREMIER, ET APRES UNE IMAGE
		// REELLEMENT RENDUE. C'est la definition meme de l'image de transition :
		// le statut a ete pose en phase 1, l'image vient de le dessiner, le
		// chargement synchrone peut commencer.
		if (FinishPendingOpen () && outcome.secondAttempted && !outcome.secondOpened)
			RecordSecondFile (outcome);

		RecordResizeFollowUp ();
		ResizeIfRequested ();

		ChurnTreeOnce (outcome);
		ReopenIfRequested (outcome);
		OpenSecondIfRequested (outcome);
		ReselectOnce (outcome);

		// ⚠ BIAIS CONNU DE CET INSTRUMENT, ET IL EST STRUCTUREL. La comparaison
		// est INTRA-IMAGE : un etat qui fuit reste fuit, donc le releve « avant »
		// de l'image suivante le contient deja et les deux coincident a nouveau.
		// Un defaut de cloture se voit sur la PREMIERE image et se masque
		// ensuite -- le compte d'images divergentes ne doit donc pas se lire
		// comme un taux.
		if (auditPointer != nullptr)
			audit.Accumulate (frame);
	}

	return Report (audit, outcome);
}

int Application::Run ()
{
	// ⚠ DEUX CHEMINS, ET UN SEUL EST LA BOUCLE D'IMAGES. Les modes « vider et
	// sortir » rendent leur code sans jamais y entrer ; main les a deja rendus
	// mutuellement exclusifs.
	int exitCode = 0;
	if (RunDumpMode (exitCode))
		return exitCode;

	return RunLoop ();
}

} // namespace app
} // namespace peles
