#pragma once

#include <functional>
#include <string>

//
// Journalisation et controle d'erreur GL pour cgre.
//
// POURQUOI CE FICHIER. cgre etait muet dans son unique application. Les
// messages d'erreur des 19 fichiers du chemin vivant partaient sur stdout /
// stderr, or sinaia est une application wxWidgets sans console : personne ne
// les a jamais lus. Un shader qui ne compile pas s'y presentait comme un ecran
// noir sans explication.
//
// Symetriquement, glGetError n'etait appele nulle part d'exploitable : les
// quatre occurrences de capabilities_manager.cpp sont dans du code inatteignable
// ou affectees a une variable jamais lue, et la macro PRINT_OPENGL_ERROR de
// shaders_manager.cpp se reduisait a void(). Une texture refusee, un tampon a
// court de VRAM, un uniforme absent ne produisaient donc pas une erreur mais un
// rendu vide.
//
// Les nouveaux fichiers de cgre sont dans le namespace `cgre`. Le reste du
// module est encore en portee globale ; on ne convertit pas l'existant ici,
// mais tout ce qui s'ajoute part du bon pied.
//
namespace cgre
{

// ---------------------------------------------------------------------------
// Journalisation
// ---------------------------------------------------------------------------

// Puits de journalisation. L'hote y branche son propre afficheur -- pour sinaia,
// MyFrame::Log, qui ecrit dans la fenetre « Logging Window ». Tant qu'aucun
// puits n'est pose, les messages vont sur stderr : c'est le comportement utile
// pour un executable de test, et l'ancien comportement pour tout le reste.
using LogSink = std::function<void (const std::string&)>;

void SetLogSink (LogSink sink);

// Vrai si un puits a ete pose par l'hote. Utile pour ne pas construire un
// message couteux qui n'irait nulle part.
bool HasLogSink ();

void Log  (const std::string& message);
void Logf (const char* format, ...);

// ---------------------------------------------------------------------------
// Controle d'erreur GL
// ---------------------------------------------------------------------------

// CGRE_CHECK_GL(x) vide la file d'erreurs GL et journalise chaque erreur avec
// le fichier, la ligne et l'etiquette `x`.
//
// Le controle est compile dans TOUTES les configurations mais desactive par
// defaut, et garde par un booleen : sinaia s'utilise en Release, donc un
// controle reserve au Debug n'aurait servi a personne. Le cout a l'arret est
// une lecture de booleen ; a l'allumage, un glGetError par point de controle,
// ce qui peut serialiser le pipeline -- d'ou le fait de ne pas l'allumer en
// permanence.
//
// S'allume par SetGlCheckEnabled(true), ou depuis la console distante.
void SetGlCheckEnabled (bool enabled);
bool IsGlCheckEnabled ();

// N'appelez pas directement : passez par la macro, qui evite le glGetError
// quand le controle est eteint.
void CheckGlErrors (const char* label, const char* file, int line);

} // namespace cgre

#define CGRE_CHECK_GL(label)                                          \
	do {                                                              \
		if (::cgre::IsGlCheckEnabled ())                              \
			::cgre::CheckGlErrors ((label), __FILE__, __LINE__);      \
	} while (0)
