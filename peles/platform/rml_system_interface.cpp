#include "rml_system_interface.h"

#include "window_glfw.h"

// ⚠ GLFW_INCLUDE_NONE est pose par la configuration : sans lui, glfw3.h tire
// son propre en-tete GL.
#include <GLFW/glfw3.h>

namespace peles
{
namespace platform
{

namespace
{

const char* LevelName (Rml::Log::Type type)
{
	switch (type)
	{
	case Rml::Log::LT_ALWAYS:  return "RmlUi";
	case Rml::Log::LT_ERROR:   return "RmlUi ERREUR";
	case Rml::Log::LT_ASSERT:  return "RmlUi ASSERTION";
	case Rml::Log::LT_WARNING: return "RmlUi avertissement";
	case Rml::Log::LT_INFO:    return "RmlUi info";
	case Rml::Log::LT_DEBUG:   return "RmlUi debug";
	default:                   return "RmlUi";
	}
}

} // namespace

SystemInterface::SystemInterface (LogSink sink, CursorSink cursor)
    : m_sink (std::move (sink)), m_cursor (std::move (cursor))
{
}

double SystemInterface::GetElapsedTime ()
{
	// L'horloge de glfw, et non std::chrono : RmlUi anime et temporise sur cette
	// valeur, et l'hote mesure ses images sur la meme. Deux horloges
	// divergeraient sans que rien ne le signale.
	return Window::ElapsedSeconds ();
}

bool SystemInterface::LogMessage (Rml::Log::Type type, const Rml::String& message)
{
	const std::string line = std::string (LevelName (type)) + " : " + std::string (message);

	if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
		++m_errors;
	else if (type == Rml::Log::LT_WARNING)
		++m_warnings;

	if (m_firstProblem.empty () && m_errors + m_warnings > 0)
		m_firstProblem = line;

	if (m_sink)
		m_sink (line);
	// `true` poursuit l'execution. Rendre `false` sur LT_ASSERT ferait rompre
	// RmlUi sur une assertion, ce qui n'est pas le comportement voulu d'un hote
	// qui journalise deja tout.
	return true;
}

void SystemInterface::SetClipboardText (const Rml::String& text)
{
	// ⚠ LA FENETRE PASSEE A glfw EST IGNOREE DEPUIS 3.3 : `glfwSetClipboardString`
	// ne fait que verifier l'initialisation puis delegue a la plateforme
	// (`input.c`, `_glfwPlatformSetClipboardString`). Cette interface n'a donc
	// pas besoin de connaitre la fenetre -- et elle ne le doit pas : RmlUi
	// l'appelle aussi depuis des chemins ou aucune fenetre n'est en vue.
	//
	// ⚠ glfw AFFIRME QUE LA CHAINE EST NON NULLE. Une chaine vide est licite, un
	// pointeur nul ne l'est pas.
	glfwSetClipboardString (nullptr, text.c_str ());

	++m_clipboardWrites;
	m_lastClipboardText = std::string (text);
}

void SystemInterface::SetMouseCursor (const Rml::String& cursor_name)
{
	m_lastCursorName = std::string (cursor_name);
	if (m_cursor)
		m_cursor (m_lastCursorName);
}

void SystemInterface::GetClipboardText (Rml::String& text)
{
	const char* const content = glfwGetClipboardString (nullptr);
	// ⚠ LES DEUX SENS SONT SURCHARGES ENSEMBLE, ET C'EST UNE CONDITION DE
	// COHERENCE. Surcharger la seule ecriture laisserait la lecture puiser dans
	// le tampon INTERNE de Rml::SystemInterface, qui ne recoit plus rien : un
	// texte copie irait au systeme et un collage rendrait la chaine vide.
	text = content != nullptr ? content : "";
}

} // namespace platform
} // namespace peles
