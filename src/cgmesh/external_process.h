#pragma once

// ============================================================================
//  Lancement d'un programme externe, sans shell
// ============================================================================
//
// Un programme est lance a partir d'un TABLEAU d'arguments, jamais d'une ligne
// de commande assemblee puis confiee a un interpreteur : un chemin contenant
// des espaces, des guillemets ou des metacaracteres du shell y arrive tel
// quel, et ne peut pas devenir une seconde commande.
//
//   - Windows : CreateProcessW, la ligne de commande etant construite selon
//     les regles d'analyse de CommandLineToArgvW / du CRT (QuoteWindowsArgument).
//     Le processus est place dans un job : l'annulation tue aussi ses
//     descendants (openscad.com, par exemple, n'est qu'un lanceur de
//     openscad.exe).
//   - POSIX : fork + execv, avec son propre groupe de processus pour la meme
//     raison. Seules des fonctions async-signal-safe sont appelees entre fork
//     et execv.
//
// La sortie standard et la sortie d'erreur sont reunies dans un fichier
// temporaire, puis rendues comme une chaine : un fichier ne peut pas se
// remplir comme un tube, donc un programme bavard ne peut pas bloquer
// l'attente.
//
// Hors du build WebAssembly : aucun processus ne se lance dans un navigateur
// (absent de la liste EMSCRIPTEN de src/cgmesh/CMakeLists.txt).
//
// ============================================================================

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace cgmesh {
namespace process {

// Repertoire temporaire au nom unique, cree a la construction et supprime
// recursivement a la destruction, y compris quand la suppression echoue
// partiellement (l'erreur est avalee : un destructeur ne leve pas).
class TemporaryDirectory
{
public:
	// Cree <root>/cg-<prefix>-<aleatoire>, `root` valant par defaut le
	// repertoire temporaire du systeme ; sous POSIX, droits 0700. Leve
	// std::filesystem::filesystem_error si `root` est inutilisable.
	explicit TemporaryDirectory (const std::string& prefix, const std::filesystem::path& root = {});
	~TemporaryDirectory ();

	TemporaryDirectory (const TemporaryDirectory&) = delete;
	TemporaryDirectory& operator= (const TemporaryDirectory&) = delete;

	const std::filesystem::path& path () const { return m_path; }

private:
	std::filesystem::path m_path;
};

struct ProcessRequest
{
	std::filesystem::path    executable;        // chemin du programme ; aucune recherche dans le PATH ici
	std::vector<std::string> arguments;         // argv[1..], en UTF-8
	std::filesystem::path    workingDirectory;  // vide : celui de l'appelant

	// 0 : pas de limite.
	std::chrono::milliseconds timeout {0};

	// Interroge a chaque tour d'attente ; vrai => le processus est tue. Appele
	// sur le thread de l'appelant, ce qui permet a une interface de traiter
	// ses evenements pendant l'attente.
	std::function<bool ()> shouldCancel;

	std::chrono::milliseconds pollInterval {50};

	// Au-dela, seule la FIN de la sortie est conservee : c'est la qu'un
	// programme ecrit son diagnostic d'echec.
	std::size_t maxOutputBytes = 1u << 20;

	// Repertoire ou creer le repertoire temporaire de capture ; vide : celui
	// du systeme.
	std::filesystem::path temporaryRoot;

	// La sortie s'ecrit sur disque pendant l'execution : au-dela, le programme
	// est tue (OutputLimitExceeded). 0 : pas de limite.
	std::size_t maxCapturedBytes = 64u << 20;
};

enum class ProcessOutcome
{
	Exited,         // le programme s'est termine ; exitCode est significatif
	LaunchFailed,   // le programme n'a pas pu etre demarre ; voir error
	TimedOut,       // tue a l'echeance de timeout
	Cancelled,      // tue a la demande de shouldCancel
	OutputLimitExceeded,   // tue quand sa sortie a depasse maxCapturedBytes
};

const char* ProcessOutcomeName (ProcessOutcome outcome);

struct ProcessResult
{
	ProcessOutcome outcome  = ProcessOutcome::LaunchFailed;
	int            exitCode = -1;   // POSIX : 128 + numero du signal si le programme a ete tue par un signal
	std::string    output;          // stdout et stderr reunis, dans l'ordre d'ecriture
	std::string    error;           // diagnostic systeme quand outcome == LaunchFailed
};

// Bloque jusqu'a la fin du programme, l'echeance ou l'annulation. Ne leve pas.
ProcessResult RunProcess (const ProcessRequest& request);

// Recherche `name` dans les repertoires de la variable d'environnement PATH.
// Rend un chemin vide si aucun fichier regulier de ce nom n'y figure. `name`
// doit porter son extension sous Windows (pas d'application de PATHEXT).
std::filesystem::path FindInSystemPath (const std::string& name);

// Valeur d'une variable d'environnement, vide si absente. UTF-8 sous Windows.
std::string GetEnvironmentVariableUtf8 (const char* name);

#ifdef _WIN32
// Met un argument entre guillemets selon les regles que CommandLineToArgvW et
// le CRT appliquent a la relecture, de sorte que l'argument reconstruit cote
// enfant soit identique, quels que soient ses espaces, guillemets et
// barres obliques inverses.
std::wstring QuoteWindowsArgument (const std::wstring& argument);
#endif

} // namespace process
} // namespace cgmesh
