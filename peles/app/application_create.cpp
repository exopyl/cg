// ---------------------------------------------------------------------------
//  LA CREATION, EN CINQ ETAPES QUI NE SE REORDONNENT PAS
// ---------------------------------------------------------------------------
//
// La fenetre porte le contexte GL ; RmlUi exige un contexte COURANT a sa
// creation ; la vue 3D et le document exigent les deux ; l'arbre et la liste
// exigent le document. L'ordre est la conception, et il se lit d'un seul coup
// d'oeil ici -- ce qu'il ne ferait pas noye entre les huit phases et les gestes.
//
// Application est declaree dans app/application.h, commune aux quatre unites.

#include "gl_wrapper.h"

#include "application.h"
#include "application_internal.h"

#include "view3d/camera.h"
#include "cgimg/image.h"
#include "diagnostics.h"
#include "input_router.h"
#include "loader.h"
#include "panel_splitters.h"
#include "path_text.h"
#include "rml_host.h"
#include "scene_renderer.h"
#include "viewport_rect.h"
#include "vmodels.h"
#include "window_glfw.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace peles
{
namespace app
{

namespace
{

// Nom du modele de donnees, tel que le `data-model` du document RML le nomme.
// ⚠ UN SEUL MODELE POUR LES QUATRE ZONES : leur contenu se salit ensemble.
const char* const kDataModelName = "peles";

void LogToStderr (const std::string& message)
{
	std::fprintf (stderr, "%s\n", message.c_str ());
	std::fflush (stderr);
}

// LE SEUL std::FILE* DU PERIMETRE, LIE A UN OBJET.
//
// ⚠ LE TAMPON DE LECTURE EST UN std::vector DONT L'ALLOCATION PEUT LEVER, et
// elle est entre le fopen et le fclose. Un pointeur nu y fuirait le descripteur,
// silencieusement, sur le seul chemin ou l'icone est chargee.
//
// Un foncteur plutot que `&std::fclose` : prendre l'adresse d'une fonction de la
// bibliotheque standard n'est pas garanti par [namespace.std], et le type du
// unique_ptr ne porte alors pas le deleteur.
struct FileCloser
{
	void operator() (std::FILE* file) const noexcept
	{
		if (file != nullptr)
			std::fclose (file);
	}
};

using FileHandle = std::unique_ptr<std::FILE, FileCloser>;

} // namespace

// ---------------------------------------------------------------------------
//  Creation
// ---------------------------------------------------------------------------

bool Application::CreateWindowAndGl (const Options& options, CreateError& error)
{
	cgre::SetLogSink (&LogToStderr);

	m_window = platform::Window::Create (options.windowWidth, options.windowHeight, "peles",
	                                     error.message);
	if (!m_window)
		return false;

	std::string glDescription;
	if (!LoadGlEntries (glDescription))
	{
		error.message = glDescription;
		return false;
	}
	std::printf ("GL_VERSION = \"%s\"\n", glDescription.c_str ());

	ApplyGlobalInvariants ();

	m_window->GetFramebufferSize (m_framebufferWidth, m_framebufferHeight);

	// LES RAPPELS D'ENTREE NE FONT QU'ENREGISTRER -- voir m_pendingEvents.
	platform::InputCallbacks callbacks;
	callbacks.move = [this] (int x, int y)
	{
		// ⚠ LA DERNIERE POSITION EST CONSERVEE A PART, ET NON RELUE DANS LA FILE.
		// glfw ne livre AUCUNE coordonnee avec un bouton ni avec la molette : il
		// faut donc une position courante, et la file est videe a chaque image --
		// la relire y donnerait (0,0) des que le clic ouvre l'image.
		m_cursorX = x;
		m_cursorY = y;

		ui::MouseEvent event;
		event.kind    = ui::MouseEvent::Kind::Move;
		event.windowX = x;
		event.windowY = y;
		m_pendingEvents.push_back (event);
	};
	callbacks.button = [this] (int button, bool pressed, int modifiers)
	{
		ui::MouseEvent event;
		event.kind = pressed ? ui::MouseEvent::Kind::Press : ui::MouseEvent::Kind::Release;
		event.button    = (ui::MouseButton)button;
		event.windowX   = m_cursorX;
		event.windowY   = m_cursorY;
		event.modifiers = modifiers;
		m_pendingEvents.push_back (event);
	};
	callbacks.wheel = [this] (float notches, int modifiers)
	{
		ui::MouseEvent event;
		event.kind      = ui::MouseEvent::Kind::Wheel;
		event.windowX   = m_cursorX;
		event.windowY   = m_cursorY;
		event.notches   = notches;
		event.modifiers = modifiers;
		m_pendingEvents.push_back (event);
	};
	callbacks.leave = [this] ()
	{
		// ⚠ MISE EN FILE COMME LES QUATRE AUTRES, ET C'EST UNE CONDITION DE
		// CORRECTION. Traitee ici meme, une sortie de cadre recue dans le MEME
		// glfwPollEvents qu'un deplacement s'appliquerait AVANT lui : RmlUi
		// oublierait son survol, le deplacement le retablirait sur l'element
		// interieur, et la ligne resterait en surbrillance curseur hors fenetre.
		ui::MouseEvent event;
		event.kind = ui::MouseEvent::Kind::Leave;
		m_pendingEvents.push_back (event);
	};
	callbacks.key = [this] (int key, bool pressed, int modifiers)
	{
		ui::KeyEvent event;
		event.key       = (ui::Key)key;
		event.pressed   = pressed;
		event.modifiers = modifiers;
		m_pendingKeys.push_back (event);
	};
	callbacks.focusLost = [this] ()
	{
		// ⚠ MISE EN FILE, COMME LES AUTRES, ET L'ORDRE EST CE QUI FAIT TOUT :
		// glfw appelle ce rappel AVANT de synthetiser un relachement par bouton
		// enfonce, donc l'annulation precede les relachements dans la file et
		// les prive de destinataire.
		ui::MouseEvent event;
		event.kind = ui::MouseEvent::Kind::Cancel;
		m_pendingEvents.push_back (event);
	};
	m_window->SetInputCallbacks (std::move (callbacks));

	LoadWindowIcon (options);

	return true;
}

void Application::LoadWindowIcon (const Options& options)
{
	// DEUX TAILLES, PARCE QUE Win32 EN DEMANDE DEUX : une petite pour la barre
	// de titre, une grande pour la barre des taches. glfw choisit dans le
	// tableau celle dont la taille est la plus proche de ce que le systeme
	// reclame.
	static const char* const kIconFiles[] = { "/icon-16.png", "/icon-32.png" };

	// ⚠ LES IMAGES DOIVENT SURVIVRE A L'APPEL, donc elles sont declarees AVANT
	// le vecteur qui pointe dedans. Img::data() rend un pointeur sur le tampon
	// de l'objet : un Img temporaire laisserait un pointeur pendant.
	std::vector<Img>                            decoded (std::size (kIconFiles));
	std::vector<platform::Window::IconImage>    images;
	std::vector<std::string>                    missing;

	for (std::size_t i = 0; i < std::size (kIconFiles); ++i)
	{
		const std::string path = options.assetsDir + kIconFiles[i];

		// ⚠ LES OCTETS SONT LUS ICI ET LE FORMAT RECONNU AU CONTENU, au lieu de
		// passer par Img::load. Celui-ci choisit son decodeur sur l'EXTENSION et
		// son chemin PNG est derriere CG_HAS_ZLIB : l'icone dependrait alors
		// d'une option de construction sans rapport avec elle.
		const FileHandle file (std::fopen (path.c_str (), "rb"));
		if (!file)
		{
			missing.push_back (path);
			continue;
		}
		std::fseek (file.get (), 0, SEEK_END);
		const long size = std::ftell (file.get ());
		std::fseek (file.get (), 0, SEEK_SET);
		std::vector<unsigned char> bytes (size > 0 ? (std::size_t)size : 0);
		const std::size_t          read =
		    bytes.empty () ? 0 : std::fread (bytes.data (), 1, bytes.size (), file.get ());

		if (read != bytes.size () || decoded[i].load_from_memory (bytes.data (), read) != 0)
		{
			missing.push_back (path);
			continue;
		}

		platform::Window::IconImage image;
		image.width  = (int)decoded[i].width ();
		image.height = (int)decoded[i].height ();
		image.rgba   = decoded[i].data ();
		images.push_back (image);
	}

	for (const std::string& path : missing)
		std::fprintf (stderr, "peles : icone illisible ou absente : %s\n", path.c_str ());

	if (!images.empty ())
		m_window->SetIcon (images);
}

bool Application::CreateUi (const Options& options, CreateError& error)
{
	platform::RmlHost::Config config;
	config.assetsDir         = options.assetsDir;
	config.framebufferWidth  = m_framebufferWidth;
	config.framebufferHeight = m_framebufferHeight;
	config.log               = &LogToStderr;
	config.debugger          = options.debugger;
	// LA FENETRE EST CREEE AVANT RmlUi ET DETRUITE APRES LUI (ordre des membres
	// d'Application) : le puits ne peut pas viser une fenetre morte.
	config.cursor = [this] (const std::string& name) { m_window->SetCursor (name); };

	m_rml = platform::RmlHost::Create (config, error.message);
	if (!m_rml)
		return false;

	// LE MODELE EST LIE ENTRE LA CREATION DU CONTEXTE ET LE CHARGEMENT DU
	// DOCUMENT, ET CET ORDRE N'EST PAS NEGOCIABLE.
	//
	// ⚠ UN ELEMENT RESOUT SON ATTRIBUT `data-model` A SON ATTACHEMENT. Lier
	// apres le chargement laisse RmlUi journaliser « Could not locate data
	// model » et afficher un document VIDE -- sans echec, sans exception, et
	// avec des temps de mise en page qui donnent a croire que N lignes ne
	// coutent rien.
	m_ui = std::make_unique<ui::UiController> ();
	if (!m_ui->Bind (m_rml->Context (), kDataModelName))
	{
		error.message  = std::string ("le modele de donnees \"") + kDataModelName +
		                "\" n'a pas pu etre cree : l'arbre et la liste resteraient vides.";
		error.exitCode = 1;
		return false;
	}

	if (!m_rml->LoadDocument (options.documentFile, error.message))
		return false;

	// L'ELEMENT DE LA VUE 3D EST EXIGE ICI, ET SON ABSENCE SORT EN 3.
	//
	// ⚠ C'est la MEME FAMILLE DE PANNE qu'un repertoire d'assets introuvable :
	// le document RML est une donnee d'execution, editable sans recompiler,
	// donc une faute de frappe dans l'identifiant est un mode de panne de
	// premier plan. Exigee AVANT le chargement du fichier : il n'y a pas de
	// raison de lire un maillage pour une vue qui n'existe pas.
	ui::ViewportRect probe;
	if (ui::ReadViewportRect (m_rml->DocumentRoot (), kViewportElementId, probe) ==
	    ui::ViewportRectStatus::ElementMissing)
	{
		error.message = std::string ("aucun element d'identifiant \"") + kViewportElementId +
		                "\" dans le document RML : la vue 3D n'a aucune surface. Assets lus "
		                "depuis " +
		                options.assetsDir + ".";
		error.exitCode = 3;
		return false;
	}

	// LE JOURNAL DE RmlUi EST LU, ET IL EST BLOQUANT A LA CREATION.
	//
	// ⚠ RmlUi NE SIGNALE QUE PAR SON JOURNAL. Un `data-model` mal orthographie
	// dans le document, un `data-for` dont la source n'est pas liee, un attribut
	// `data-` inconnu : chacun produit un LT_ERROR, puis l'execution POURSUIT --
	// quatre panneaux vides, GL_VIEWPORT concordant, et EXIT=0. Le document RML
	// est une donnee d'execution, editable sans recompiler : c'est la MEME
	// famille de faute que `#viewport` absent, et elle sort par le MEME code.
	//
	// ⚠ CONTROLE GENERAL PLUTOT QUE CONTROLE DU SEUL ATTRIBUT `data-model` : il
	// porte sur tout ce que RmlUi REFUSE au chargement du document, quelle que
	// soit l'etape qui a ajoute le RML fautif.
	//
	// ⚠ MAIS SA COUVERTURE S'ARRETE LA, ET DEUX FAMILLES LUI ECHAPPENT. (1) Une
	// valeur LIEE que RCSS ne sait pas analyser n'est refusee qu'a la premiere
	// mise a jour, donc apres ce controle -- d'ou ReportRmlJournal, appele sur
	// les deux chemins de sortie de Run(). (2) Un nom sali par DirtyVariable qui
	// n'existe pas dans le modele n'est refuse par PERSONNE : RmlUi n'y met
	// qu'une assertion, compilee a rien en Release. Celle-la ne se surveille
	// pas, elle se supprime -- cf. les constantes de ui/view_model.h.
	const int rejected = m_rml->ErrorCount () + m_rml->WarningCount ();
	if (rejected > 0)
	{
		error.message = "le document RML a ete refuse par RmlUi (" +
		                std::to_string (m_rml->ErrorCount ()) + " erreur(s), " +
		                std::to_string (m_rml->WarningCount ()) +
		                " avertissement(s)), l'interface serait incomplete. Premier message : " +
		                m_rml->FirstProblem () + ". Assets lus depuis " + options.assetsDir + ".";
		error.exitCode = 3;
		return false;
	}

	// L'ARBITRE EST CREE ICI PARCE QU'IL TIENT LE CONTEXTE DE RmlUi, et que le
	// contexte meurt avec l'hote.
	m_input = std::make_unique<ui::InputRouter> (m_rml->Context (), kViewportElementId);

	// LES SEPARATEURS SONT EXIGES DANS LE DOCUMENT DU PRODUIT, ET SEULEMENT LA.
	//
	// ⚠ Meme famille de faute que `#viewport` absent : un identifiant mal
	// orthographie dans le document laisserait un separateur visible, avec son
	// curseur, qui ne redimensionne rien -- et aucun message. Une mise en page
	// d'epreuve (`--document`) n'en porte aucun, et n'en exige donc aucun.
	m_splitters      = std::make_unique<ui::PanelSplitters> ();
	const int splits = m_splitters->Attach (m_rml->DocumentRoot (), kViewportElementId);
	if (options.documentFile.empty () && splits != ui::PanelSplitters::kCount)
	{
		std::string names;
		for (const ui::PanelSplitters::Spec& spec : ui::PanelSplitters::Specs ())
			names += std::string (names.empty () ? "" : ", ") + "#" + spec.splitterId + " -> #" +
			         spec.panelId;
		error.message = std::to_string (splits) + " separateur(s) de panneau sur " +
		                std::to_string (ui::PanelSplitters::kCount) +
		                " dans le document RML ; attendus : " + names + ". Assets lus depuis " +
		                options.assetsDir + ".";
		error.exitCode = 3;
		return false;
	}

	// LE RECTANGLE EST LU UNE PREMIERE FOIS ICI, AVANT LA PREMIERE IMAGE.
	//
	// ⚠ LA SONDE CI-DESSUS NE SUFFIT PAS : elle precede tout Update(), donc la
	// geometrie qu'elle lit n'est pas resolue. Sans cette seconde lecture, TOUTE
	// entree recue avant la premiere image REELLEMENT rendue est jetee sans
	// trace -- ce n'est pas « les 16 premieres millisecondes » : une fenetre
	// creee reduite, ou une vue degeneree, ne rend aucune image du tout.
	//
	// ⚠ COUT : UN Update() SUR UN DOCUMENT QUI N'A ENCORE AUCUNE LIGNE. Les
	// vecteurs lies sont peuples par PopulateBrowser, qui court apres.
	m_rml->Update ();

	ui::ViewportRect initial;
	if (ui::ReadViewportRect (m_rml->DocumentRoot (), kViewportElementId, initial) ==
	    ui::ViewportRectStatus::Ok)
	{
		m_viewRect    = initial;
		m_hasViewRect = true;
		m_input->SetViewportRect (initial);
	}

	return true;
}

void Application::CreateSceneAndDocument (const Options& options)
{
	m_camera = std::make_unique<view3d::Camera> ();
	m_scene  = std::make_unique<view3d::SceneRenderer> (m_document);

	// LA POSE DE MESURE, ET SES TROIS TERMES SONT POSES ENSEMBLE.
	//
	// ⚠ L'ECLAIRAGE EN FAIT PARTIE AU MEME TITRE QUE L'ORIENTATION ET L'OMBRAGE.
	// Une couleur de pixel ne se compare a une valeur predite que si les trois
	// sont fixes : sous la pose de repos la normale de la face vue au centre
	// n'est pas (0,0,1), sous l'ombrage lisse elle n'est pas constante, et sous
	// l'eclairage produit -- deux lampes obliques, ambiante globale 0,12 -- la
	// couleur ne se calcule plus a la main. Laisser l'eclairage hors du
	// commutateur reviendrait a faire prescrire par l'oracle l'eclairage du
	// PRODUIT : toute restauration de fidelite au canvas historique rendrait le
	// critere rouge.
	if (options.readPixel)
	{
		m_camera->SetFrontalRotation ();
		m_scene->SetFlatShading (true);
		m_scene->SetMeasurementLighting (true);
	}

	if (options.modelPath.empty ())
		return;

	const core::LoadResult loaded = core::LoadModel (m_document, options.modelPath);
	if (loaded.model == nullptr)
	{
		ReportLoadFailure (options.modelPath, loaded.error);
		return;
	}
	m_modelLoaded = true;

	std::printf ("modele : %s (%zu maillage(s))\n", loaded.model->m_name.c_str (),
	             loaded.model->m_meshes.GetNMeshes ());
	m_camera->Frame (loaded.model->m_bbox, m_document.VisibleBounds ());

	// LES PANNEAUX DE DROITE SUIVENT LE MODELE SELECTIONNE, ET ILS LE SUIVENT
	// D'ICI. Le controleur d'interface ne connait pas le document : c'est
	// l'hote qui lui designe le modele, comme il lui designe le repertoire.
	m_ui->ShowModelInfo (m_document.Selected ());
}

void Application::PopulateBrowser (const Options& options)
{
	// L'ORDRE DES TROIS SOURCES EST CELUI DE LA PRECISION DECROISSANTE : le
	// repertoire demande explicitement, sinon celui du fichier ouvert, sinon
	// les lecteurs de la machine. Ouvrir les lecteurs alors qu'un fichier est
	// charge obligerait l'operateur a y redescendre a la main.
	// ⚠ UN std::filesystem::path DE BOUT EN BOUT. Passer par une std::string
	// intermediaire ferait subir au chemin une conversion vers la page de codes
	// ANSI sous Windows, donc une PERTE : le repertoire ouvert ne serait plus
	// celui qui a ete demande (cf. core/path_text.h).
	std::filesystem::path directory (options.browseDir);
	if (directory.empty () && !options.modelPath.empty ())
	{
		std::error_code ec;
		const std::filesystem::path parent =
		    std::filesystem::absolute (std::filesystem::path (options.modelPath), ec).parent_path ();
		if (!ec)
			directory = parent;
	}

	if (directory.empty ())
	{
		m_ui->ShowSystemRoots ();
		return;
	}

	if (!m_ui->ShowDirectory (directory))
	{
		// ⚠ LE MESSAGE EST DANS LE MODELE, DONC A L'ECRAN. Il est aussi
		// journalise : une erreur qui n'existerait qu'a l'ecran serait perdue
		// pour une execution scriptee.
		std::fprintf (stderr, "peles : %s\n", m_ui->Model ().treeStatus.c_str ());
		return;
	}

	if (options.browseDepth > 0)
		m_ui->ExpandToDepth (options.browseDepth);

	m_ui->SelectFile (options.modelPath);
}


} // namespace app
} // namespace peles
