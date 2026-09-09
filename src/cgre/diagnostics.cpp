#include "diagnostics.h"

#include "gl_wrapper.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace cgre
{

namespace
{
	LogSink g_logSink;
	bool    g_glCheckEnabled = false;

	const char* GlErrorName (GLenum error)
	{
		switch (error)
		{
		case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
		case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
		case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
		default:                               return "GL_ERROR";
		}
	}
}

void SetLogSink (LogSink sink)
{
	g_logSink = std::move (sink);
}

bool HasLogSink ()
{
	return static_cast<bool> (g_logSink);
}

void Log (const std::string& message)
{
	if (g_logSink)
		g_logSink (message);
	else
		std::fprintf (stderr, "[cgre] %s\n", message.c_str ());
}

void Logf (const char* format, ...)
{
	if (!format)
		return;

	va_list args;
	va_start (args, format);

	va_list argsCopy;
	va_copy (argsCopy, args);
	const int needed = std::vsnprintf (nullptr, 0, format, argsCopy);
	va_end (argsCopy);

	if (needed < 0)
	{
		va_end (args);
		// Erreur de formatage : on remonte le format brut plutot que de perdre
		// le message. Ne jamais lever depuis une fonction de journalisation --
		// c'est ce que faisait CapabilitiesManager::string_format, dont le throw
		// remontait jusqu'a la boucle d'evenements de wxWidgets.
		Log (std::string ("format invalide : ") + format);
		return;
	}

	std::vector<char> buffer (static_cast<size_t> (needed) + 1);
	std::vsnprintf (buffer.data (), buffer.size (), format, args);
	va_end (args);

	Log (std::string (buffer.data (), static_cast<size_t> (needed)));
}

void SetGlCheckEnabled (bool enabled)
{
	// La bascule est JOURNALISEE. L'etat ne survit pas au redemarrage et
	// n'apparait nulle part dans l'interface : sans cette trace, relire un
	// journal ne permet pas de savoir si l'absence d'erreur GL signifie « aucune
	// erreur » ou « aucun controle ». L'ambiguite a deja coute deux diagnostics.
	if (enabled != g_glCheckEnabled)
		Log (enabled ? "Controle d'erreur GL : ACTIF."
		             : "Controle d'erreur GL : inactif.");
	g_glCheckEnabled = enabled;
}

bool IsGlCheckEnabled ()
{
	return g_glCheckEnabled;
}

void CheckGlErrors (const char* label, const char* file, int line)
{
	// La file peut contenir plusieurs erreurs : la specification impose de la
	// vider par appels successifs jusqu'a GL_NO_ERROR. Une borne evite de tourner
	// indefiniment si aucun contexte n'est courant -- glGetError renvoie alors
	// GL_INVALID_OPERATION sur certains pilotes, sans jamais se vider.
	const int kMaxErrors = 16;

	for (int i = 0; i < kMaxErrors; ++i)
	{
		const GLenum error = glGetError ();
		if (error == GL_NO_ERROR)
			return;

		Logf ("GL %s (0x%04X) -- %s [%s:%d]",
		      GlErrorName (error), (unsigned)error,
		      label ? label : "?", file ? file : "?", line);
	}

	Logf ("GL : plus de %d erreurs d'affilee sur %s -- file non videe, contexte "
	      "probablement absent [%s:%d]",
	      kMaxErrors, label ? label : "?", file ? file : "?", line);
}

} // namespace cgre
