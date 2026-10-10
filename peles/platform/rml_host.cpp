#include "rml_host.h"

#include "rml_render_interface.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

namespace peles
{
namespace platform
{

namespace
{

const char* const kContextName = "peles";

// Une seule famille de police est chargee, et elle est VERSIONNEE dans le depot
// sous licence SIL OFL 1.1 (assets/fonts/LatoLatin-Regular.LICENSE.txt).
//
// ⚠ ELLE N'EST PAS CELLE DU Rml::Debugger. Le debogueur embarque ses propres
// polices ; s'appuyer dessus lierait l'apparence de l'application a un outil de
// diagnostic optionnel, qu'on doit pouvoir retirer sans consequence.
const char* const kFontFile = "fonts/LatoLatin-Regular.ttf";

const char* const kDocumentFile = "peles.rml";

std::string Join (const std::string& directory, const char* leaf)
{
	return directory + "/" + leaf;
}

} // namespace

RmlHost::RmlHost () = default;

RmlHost::~RmlHost ()
{
	if (m_debugger)
		Rml::Debugger::Shutdown ();

	if (m_initialised)
	{
		// Rml::Shutdown libere les contextes et leurs documents : m_context et
		// m_document sont des pointeurs non proprietaires et deviennent
		// pendants ici.
		Rml::Shutdown ();
		m_context  = nullptr;
		m_document = nullptr;
	}

	// m_render et m_system sont detruits APRES ce corps, dans l'ordre inverse de
	// leur declaration -- donc apres Rml::Shutdown(). C'est le contrat.
}

std::unique_ptr<RmlHost> RmlHost::Create (const Config& config, std::string& error)
{
	std::unique_ptr<RmlHost> host (new RmlHost);

	// ⚠ `new` NU RESERVE AU SEUL CAS QUI L'IMPOSE -- le constructeur PRIVE de
	// RmlHost, ci-dessus. Les deux interfaces ont un constructeur public : rien
	// n'empeche std::make_unique, et lui seul rend la ligne exempte de fuite si
	// l'allocation suivante jette.
	host->m_system = std::make_unique<SystemInterface> (config.log, config.cursor);
	host->m_render = std::make_unique<RenderInterfaceGL2> ();
	host->m_render->SetViewport (config.framebufferWidth, config.framebufferHeight);

	Rml::SetSystemInterface (host->m_system.get ());
	Rml::SetRenderInterface (host->m_render->Interface ());

	if (!Rml::Initialise ())
	{
		// ⚠ LES DEUX POINTEURS SONT REPRIS ICI, ET C'EST LE SEUL CHEMIN OU
		// PERSONNE NE LE FAIT POUR NOUS. Sur les echecs suivants, le
		// destructeur appelle Rml::Shutdown(), qui relache les interfaces avant
		// que m_system et m_render ne meurent. Ici il n'y a rien a arreter :
		// sans ces deux lignes, RmlUi garderait deux pointeurs sur des objets
		// detruits a la sortie de fonction.
		Rml::SetSystemInterface (nullptr);
		Rml::SetRenderInterface (nullptr);
		error = "Rml::Initialise a echoue.";
		return nullptr;
	}
	host->m_initialised = true;

	const std::string fontPath = Join (config.assetsDir, kFontFile);
	if (!Rml::LoadFontFace (fontPath))
	{
		// RmlUi EXIGE au moins une fonte : sans elle, tout element textuel est
		// muet et le document se presente comme une fenetre vide. La panne doit
		// donc nommer le fichier, pas se taire.
		error = "police introuvable ou illisible : " + fontPath;
		return nullptr;
	}

	host->m_context = Rml::CreateContext (
	    kContextName, Rml::Vector2i (config.framebufferWidth, config.framebufferHeight));
	if (host->m_context == nullptr)
	{
		error = "Rml::CreateContext a rendu nullptr.";
		return nullptr;
	}

	// DEFILEMENT INSTANTANE, ET NON ANIME.
	//
	// ⚠ DEUX RAISONS, ET LA SECONDE EST LA PLUS FORTE. (1) Le defilement lisse
	// de RmlUi est integre sur le TEMPS ECOULE entre deux Update() : un
	// chargement synchrone de plusieurs secondes -- ce que VMeshesIO::load peut
	// couter sur un STEP -- tombe au milieu de l'animation et lui fait rattraper
	// tout son retard en une image. (2) Une position de defilement qui depend du
	// nombre d'images rendues n'est pas mesurable : l'instrument qui verifie que
	// la derniere ligne d'un arbre est atteignable rendrait un resultat
	// different selon la charge de la machine.
	host->m_context->SetDefaultScrollBehavior (Rml::ScrollBehavior::Instant, 1.f);

	if (config.debugger)
	{
		host->m_debugger = Rml::Debugger::Initialise (host->m_context);
		if (host->m_debugger)
		{
			Rml::Debugger::SetVisible (true);
		}
		else if (config.log)
		{
			// ⚠ L'ECHEC EST DIT, ET IL N'EST PAS FATAL. Le debogueur est un
			// outil de diagnostic optionnel : son absence ne doit pas empecher
			// l'application de tourner. Mais absorbe, il fait croire que
			// `--debugger` a pris effet alors que l'operateur ne verra rien.
			config.log ("peles : Rml::Debugger::Initialise a echoue ; --debugger reste sans "
			            "effet.");
		}
	}

	host->m_assetsDir = config.assetsDir;
	return host;
}

bool RmlHost::LoadDocument (const std::string& documentFile, std::string& error)
{
	if (m_context == nullptr)
	{
		error = "aucun contexte RmlUi : le document ne peut pas etre charge.";
		return false;
	}

	const std::string documentPath =
	    Join (m_assetsDir, documentFile.empty () ? kDocumentFile : documentFile.c_str ());
	m_document                     = m_context->LoadDocument (documentPath);
	if (m_document == nullptr)
	{
		error = "document RML introuvable ou invalide : " + documentPath;
		return false;
	}
	m_document->Show ();
	return true;
}

void RmlHost::SetDimensions (int framebufferWidth, int framebufferHeight)
{
	m_render->SetViewport (framebufferWidth, framebufferHeight);
	if (m_context != nullptr)
		m_context->SetDimensions (Rml::Vector2i (framebufferWidth, framebufferHeight));
}

void RmlHost::Update ()
{
	if (m_context != nullptr)
		m_context->Update ();
}

void RmlHost::Render ()
{
	if (m_context == nullptr)
		return;

	// BeginFrame / EndFrame encadrent le rendu de RmlUi et POSENT l'etat GL dont
	// son renderer a besoin. Ils ne le restaurent pas : la restauration est le
	// role de GlStateFence, cote hote, qui ne fait confiance a personne.
	m_render->BeginFrame ();
	m_context->Render ();
	m_render->EndFrame ();
}

Rml::Element* RmlHost::DocumentRoot () const
{
	return m_document;
}

int RmlHost::ErrorCount () const
{
	return m_system ? m_system->ErrorCount () : 0;
}

int RmlHost::WarningCount () const
{
	return m_system ? m_system->WarningCount () : 0;
}

const std::string& RmlHost::FirstProblem () const
{
	static const std::string none;
	return m_system ? m_system->FirstProblem () : none;
}

int RmlHost::ClipboardWrites () const
{
	return m_system ? m_system->ClipboardWrites () : 0;
}

const std::string& RmlHost::LastClipboardText () const
{
	static const std::string none;
	return m_system ? m_system->LastClipboardText () : none;
}

const std::string& RmlHost::LastCursorName () const
{
	static const std::string none;
	return m_system ? m_system->LastCursorName () : none;
}

} // namespace platform
} // namespace peles
